// This file is split from d_net.cpp

// =============================================================================
// HCDE PREDICTION-LOSS DEBUGGER IMPLEMENTATION
// =============================================================================
//
// Called once per `TryRunTics` invocation on the client side after the tic gate
// metrics (`availableTics`, `lowestSequence`) have been computed. The function:
//
//   1) Updates per-window extrema (min available, max ack-lag, max stale-visual,
//      max backlog) cheaply every frame.
//   2) Once per `net_predict_debug_interval` tics it dumps a CSV row capturing
//      gate health, peer counters, mirror divergence, repair deltas, local
//      pose/velocity, viewheight and PSprite Y so we can correlate "gun looks
//      wrong" / "monster shooting at nothing" with the actual netcode state
//      (and reset the extrema for the next window).
//   3) If level >= 2, it cross-checks the window against soft-warn thresholds
//      and appends a tagged human-readable line to hcde_prediction_softwarn.log
//      (rate-limited). These fire well below the existing fault threshold.
//   4) If level >= 3, it also dumps the full DebugTrace ring to a unique trace
//      file whenever a soft-warn fires (max once per 5s) so we can post-mortem
//      sub-fault drift the same way we post-mortem real faults.
//
// Designed to be cheap when off (single int compare) and bounded when on (one
// fopen/fprintf per sample interval, plus one fopen on soft-warn).
static void Net_PredictionDebugTick(int totalTics, int availableTics, int lowestSequence)
{
	const int level = *net_predict_debug;
	if (level <= 0)
		return;
	if (!netgame || gamestate != GS_LEVEL || demoplayback)
		return;
	// The authority does not predict against itself; nothing useful to log.
	if (Net_IsLocalInvasionAuthority())
		return;

	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	const FClientNetState* authorityState = (authoritySlot >= 0 && authoritySlot < MAXPLAYERS)
		? &ClientStates[authoritySlot] : nullptr;
	const FHCDELivePeerState* authorityPeer = (authoritySlot >= 0 && authoritySlot < MAXPLAYERS)
		? &HCDELivePeers[authoritySlot] : nullptr;

	static uint64_t lastPingLogMS = 0;
	const uint64_t pingLogNowMS = I_msTime();
	if (pingLogNowMS - lastPingLogMS >= 1000)
	{
		lastPingLogMS = pingLogNowMS;
		for (auto client : NetworkClients)
		{
			int leadTics = 0;
			if (client == consoleplayer)
				leadTics = max<int>((ClientTic - gametic) / TicDup, 0);
			else
				leadTics = max<int>(ClientStates[client].CurrentSequence - (gametic / TicDup), 0);

			int leadMs = leadTics * TicDup * 1000 / TICRATE;
			int rttMs = ClientStates[client].AverageLatency;
			int extraTics = net_extratic ? 1 : 0;
			int delta = ClientTic - gametic;

			Net_LogPingSample(client, leadTics, leadMs, rttMs, TicDup, extraTics, delta);
		}
	}

	const int commandBacklog = ClientTic - gametic;
	const int newestTic = authorityState != nullptr ? authorityState->CurrentSequence : -1;
	const int currentAck = authorityState != nullptr ? authorityState->SequenceAck : -1;
	const int seqAckLag = (newestTic >= 0 && currentAck >= 0) ? max(newestTic - currentAck, 0) : 0;

	// Update per-window extrema (cheap; do every frame).
	if (availableTics < HCDEPredictionDebugMinAvailable)
		HCDEPredictionDebugMinAvailable = availableTics;
	if (seqAckLag > HCDEPredictionDebugMaxAckLag)
		HCDEPredictionDebugMaxAckLag = seqAckLag;
	if (commandBacklog > HCDEPredictionDebugMaxBacklog)
		HCDEPredictionDebugMaxBacklog = commandBacklog;

	// Walk replicated actors once to gather mirror-health metrics.
	unsigned trackedMirrors = unsigned(InvasionReplicatedActors.Size());
	unsigned blockingMirrors = 0u;
	unsigned projectileMirrors = 0u;
	unsigned staleAttackingMirrors = 0u;
	int maxVisualStaleThisFrame = 0;
	for (const auto& ref : InvasionReplicatedActors)
	{
		AActor* actor = ref.Actor.Get();
		if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
			continue;
		if (Net_IsInvasionActorCorpseLike(actor))
			continue;
		if (ref.IsProjectile)
			++projectileMirrors;
		else
			++blockingMirrors;
		const bool attacking = ref.VisualActionState == HCDEInvasionActorActionMelee
			|| ref.VisualActionState == HCDEInvasionActorActionMissile
			|| ref.VisualActionState == HCDEInvasionActorActionPain;
		if (attacking && ref.VisualTargetTic > 0)
		{
			const int age = gametic - ref.VisualTargetTic;
			if (age > maxVisualStaleThisFrame)
				maxVisualStaleThisFrame = age;
			if (age > TICRATE / 2) // 0.5s of no fresh target = noteworthy
				++staleAttackingMirrors;
		}
	}
	if (maxVisualStaleThisFrame > HCDEPredictionDebugMaxStaleVisual)
		HCDEPredictionDebugMaxStaleVisual = maxVisualStaleThisFrame;

	const int serverActive = Net_GetInvasionActiveMonsterCount();
	const int mirrorDelta = abs(int(blockingMirrors) - serverActive);

	static uint64_t mismatchStartMS = 0;
	if (serverActive != (int)blockingMirrors)
	{
		if (mismatchStartMS == 0)
		{
			mismatchStartMS = I_msTime();
		}
		else if (I_msTime() - mismatchStartMS >= 1000)
		{
			static uint64_t lastDesyncLogMS = 0;
			if (I_msTime() - lastDesyncLogMS >= 1000)
			{
				lastDesyncLogMS = I_msTime();
				DebugTrace::Warningf("net.desync", "[MIRROR PARITY DESYNC] mismatch for >1s: server active monsters=%d, client blocking mirrors=%d, gametic=%d",
					serverActive, (int)blockingMirrors, gametic);
			}
		}
	}
	else
	{
		mismatchStartMS = 0;
	}

	// Only emit a sample row once per configured interval.
	const int interval = max<int>(*net_predict_debug_interval, 1);
	if (gametic - HCDEPredictionDebugLastSampleTic < interval)
		return;

	// Compute window deltas for the snapshot row.
	const uint64_t lifetimeLocalRepairs = HCDELiveProfile.PredictionLocalHealthRepairs
		+ HCDELiveProfile.PredictionLocalStateRepairs;
	const uint64_t lifetimeHardRepairs = HCDELiveProfile.PredictionHardRespawnRepairs
		+ HCDELiveProfile.PredictionHardDeathRepairs;
	const uint64_t lifetimeFaultReports = HCDELiveProfile.PredictionFaultReports;
	const uint64_t windowPassiveClient = HCDEPredictionDebugLifetime.PassiveClientResends
		- HCDEPredictionDebugWindowPassiveClient;
	const uint64_t windowPassiveAuth = HCDEPredictionDebugLifetime.PassiveAuthorityResends
		- HCDEPredictionDebugWindowPassiveAuth;
	const uint64_t windowLocalRepairs = lifetimeLocalRepairs - HCDEPredictionDebugWindowLocalRepairs;
	const uint64_t windowHardRepairs = lifetimeHardRepairs - HCDEPredictionDebugWindowHardRepairs;
	const uint64_t windowFaultReports = lifetimeFaultReports - HCDEPredictionDebugWindowFaultReports;

	const player_t* localPlayer = (consoleplayer >= 0 && consoleplayer < MAXPLAYERS) ? &players[consoleplayer] : nullptr;
	const AActor* localActor = localPlayer != nullptr ? localPlayer->mo : nullptr;
	const int latestCommandTic = max<int>(ClientTic - 1, 0);
	const usercmd_t& latestCmd = LocalCmds[latestCommandTic % LOCALCMDTICS];
	const bool wantsAttack = (latestCmd.buttons & BT_ATTACK) != 0;
	const bool wantsMove = latestCmd.forwardmove != 0 || latestCmd.sidemove != 0 || latestCmd.upmove != 0;
	const bool compatPredicting = localPlayer != nullptr && (localPlayer->cheats & CF_PREDICTING) != 0;

	// PSprite Y is the "gun floating" indicator the user joked about.
	double pspriteX = 0.0;
	double pspriteY = 0.0;
	bool pspriteAlive = false;
	if (localPlayer != nullptr)
	{
		// FindPSprite is non-const on player_t; cast away const purely for read.
		player_t* readablePlayer = const_cast<player_t*>(localPlayer);
		DPSprite* sp = readablePlayer->FindPSprite(PSP_WEAPON);
		if (sp != nullptr)
		{
			pspriteX = sp->x;
			pspriteY = sp->y;
			pspriteAlive = (sp->GetState() != nullptr);
		}
	}

	const uint64_t nowMS = I_msTime();
	const FString csvPath = FStringf("%s/hcde_predict_metrics.csv", M_GetAppDataPath(true).GetChars());

	// Write header once per process if file is empty/new.
	if (!HCDEPredictionDebugCsvHeaderProbed)
	{
		HCDEPredictionDebugCsvHeaderProbed = true;
		bool needsHeader = true;
		if (FILE* probe = fopen(csvPath.GetChars(), "rb"))
		{
			fseek(probe, 0, SEEK_END);
			needsHeader = ftell(probe) <= 0;
			fclose(probe);
		}
		if (needsHeader)
		{
			if (FILE* hdr = fopen(csvPath.GetChars(), "w"))
			{
				fprintf(hdr,
					"ms_time,gametic,clienttic,totaltics,availabletics,backlog,lowestseq,"
					"authority,seq,ack,acklag,snap_rx,ctrl_rx,any_rx,snap_count,input_sent,"
					"win_passive_client,win_passive_auth,win_local_repairs,win_hard_repairs,win_faults,"
					"min_available_w,max_acklag_w,max_stale_visual_w,max_backlog_w,"
					"invasion,wave,server_active,tracked_mirrors,blocking_mirrors,projectile_mirrors,"
					"stale_attacking,mirror_delta,last_world_tics,lag_state,"
					"predicting,compat_predicting,attack,move,fwd,side,up,buttons,"
					"pos_x,pos_y,pos_z,vel_x,vel_y,vel_z,bob,health,"
					"viewheight,view_z_off,psprite_x,psprite_y,psprite_alive,room\n");
				fclose(hdr);
			}
		}
	}

	if (FILE* csv = fopen(csvPath.GetChars(), "a"))
	{
		fprintf(csv,
			"%llu,%d,%d,%d,%d,%d,%d,"
			"%d,%d,%d,%d,%u,%u,%u,%u,%u,"
			"%llu,%llu,%llu,%llu,%llu,"
			"%d,%d,%d,%d,"
			"%s,%d,%d,%u,%u,%u,"
			"%u,%d,%d,%s,"
			"%d,%d,%d,%d,%d,%d,%d,0x%08x,"
			"%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.4f,%d,"
			"%.2f,%.2f,%.2f,%.2f,%d,%u\n",
			static_cast<unsigned long long>(nowMS),
			gametic, ClientTic, totalTics, availableTics, commandBacklog, lowestSequence,
			authoritySlot, newestTic, currentAck, seqAckLag,
			authorityPeer != nullptr ? authorityPeer->RxServerSnapshotSequence : 0u,
			authorityPeer != nullptr ? authorityPeer->RxControlSequence : 0u,
			authorityPeer != nullptr ? authorityPeer->RxSequence : 0u,
			authorityPeer != nullptr ? authorityPeer->SnapshotReceived : 0u,
			authorityPeer != nullptr ? authorityPeer->ClientCommandSent : 0u,
			static_cast<unsigned long long>(windowPassiveClient),
			static_cast<unsigned long long>(windowPassiveAuth),
			static_cast<unsigned long long>(windowLocalRepairs),
			static_cast<unsigned long long>(windowHardRepairs),
			static_cast<unsigned long long>(windowFaultReports),
			HCDEPredictionDebugMinAvailable == INT_MAX ? availableTics : HCDEPredictionDebugMinAvailable,
			HCDEPredictionDebugMaxAckLag,
			HCDEPredictionDebugMaxStaleVisual,
			HCDEPredictionDebugMaxBacklog,
			Net_InvasionStateName(InvasionState), InvasionWaveDirector.Wave, serverActive,
			trackedMirrors, blockingMirrors, projectileMirrors,
			staleAttackingMirrors, mirrorDelta,
			EnterTic - LastGameUpdate, Net_LagStateName(LagState),
			NetworkEntityManager::IsPredicting() ? 1 : 0, compatPredicting ? 1 : 0,
			wantsAttack ? 1 : 0, wantsMove ? 1 : 0,
			latestCmd.forwardmove, latestCmd.sidemove, latestCmd.upmove, latestCmd.buttons,
			localActor != nullptr ? localActor->X() : 0.0,
			localActor != nullptr ? localActor->Y() : 0.0,
			localActor != nullptr ? localActor->Z() : 0.0,
			localActor != nullptr ? localActor->Vel.X : 0.0,
			localActor != nullptr ? localActor->Vel.Y : 0.0,
			localActor != nullptr ? localActor->Vel.Z : 0.0,
			localPlayer != nullptr ? double(localPlayer->bob) : 0.0,
			localActor != nullptr ? localActor->health : 0,
			localPlayer != nullptr ? double(localPlayer->viewheight) : 0.0,
			localPlayer != nullptr && localActor != nullptr ? double(localPlayer->viewz - localActor->Z()) : 0.0,
			pspriteX, pspriteY, pspriteAlive ? 1 : 0,
			unsigned(CurrentRoomID));
		fclose(csv);
	}

	// Soft-warning + optional trace dump.
	if (level >= 2 && EnterTic >= HCDEPredictionPauseGraceUntil)
	{
		const int ackThreshold = *net_predict_softwarn_ack_lag;
		const int mirrorThreshold = *net_predict_softwarn_mirror_delta;
		const int passiveThreshold = *net_predict_softwarn_passive_storm;
		const bool warnAckLag = HCDEPredictionDebugMaxAckLag >= ackThreshold;
		const bool warnMirror = mirrorDelta >= mirrorThreshold;
		const bool warnAvailable = HCDEPredictionDebugMinAvailable != INT_MAX
			&& HCDEPredictionDebugMinAvailable <= 1
			&& HCDEPredictionDebugMaxBacklog > TicDup;
		const bool warnStaleAttack = staleAttackingMirrors > 0;
		const bool warnPassiveStorm = windowPassiveClient >= unsigned(passiveThreshold);
		const bool warnHardChurn = windowHardRepairs >= 2u;
		const bool warnAny = warnAckLag || warnMirror || warnAvailable
			|| warnStaleAttack || warnPassiveStorm || warnHardChurn;

		if (warnAny && HCDELiveReportIntervalElapsed(HCDEPredictionDebugLastSoftWarnMS, 250u))
		{
			const FString warnPath = FStringf("%s/hcde_prediction_softwarn.log",
				M_GetAppDataPath(true).GetChars());
			if (FILE* warn = fopen(warnPath.GetChars(), "a"))
			{
				fprintf(warn,
					"%llu HCDE predict softwarn triggers=[%s%s%s%s%s%s] ack=%d/%d (lag=%d max=%d) "
					"avail=%d (min=%d) backlog=%d (max=%d) mirror=%d (blocking=%u server=%d) "
					"stale-attack=%u max-stale=%d passive-client=%llu passive-auth=%llu "
					"local-repairs=%llu hard-repairs=%llu fault-reports=%llu invasion=%s wave=%d "
					"snap-count=%u input-sent=%u viewheight=%.2f view-z-off=%.2f psprite-y=%.2f "
					"pos=(%.1f,%.1f,%.1f) vel=(%.2f,%.2f,%.2f) health=%d room=%u\n",
					static_cast<unsigned long long>(nowMS),
					warnAckLag ? "ack-lag " : "",
					warnAvailable ? "available-low " : "",
					warnMirror ? "mirror-drift " : "",
					warnStaleAttack ? "stale-attack " : "",
					warnPassiveStorm ? "passive-storm " : "",
					warnHardChurn ? "hard-churn " : "",
					currentAck, newestTic, seqAckLag, HCDEPredictionDebugMaxAckLag,
					availableTics,
					HCDEPredictionDebugMinAvailable == INT_MAX ? availableTics : HCDEPredictionDebugMinAvailable,
					commandBacklog, HCDEPredictionDebugMaxBacklog,
					mirrorDelta, blockingMirrors, serverActive,
					staleAttackingMirrors, HCDEPredictionDebugMaxStaleVisual,
					static_cast<unsigned long long>(windowPassiveClient),
					static_cast<unsigned long long>(windowPassiveAuth),
					static_cast<unsigned long long>(windowLocalRepairs),
					static_cast<unsigned long long>(windowHardRepairs),
					static_cast<unsigned long long>(windowFaultReports),
					Net_InvasionStateName(InvasionState), InvasionWaveDirector.Wave,
					authorityPeer != nullptr ? authorityPeer->SnapshotReceived : 0u,
					authorityPeer != nullptr ? authorityPeer->ClientCommandSent : 0u,
					localPlayer != nullptr ? double(localPlayer->viewheight) : 0.0,
					localPlayer != nullptr && localActor != nullptr ? double(localPlayer->viewz - localActor->Z()) : 0.0,
					pspriteY,
					localActor != nullptr ? localActor->X() : 0.0,
					localActor != nullptr ? localActor->Y() : 0.0,
					localActor != nullptr ? localActor->Z() : 0.0,
					localActor != nullptr ? localActor->Vel.X : 0.0,
					localActor != nullptr ? localActor->Vel.Y : 0.0,
					localActor != nullptr ? localActor->Vel.Z : 0.0,
					localActor != nullptr ? localActor->health : 0,
					unsigned(CurrentRoomID));
				fclose(warn);
			}

			DebugTrace::Warningf("net",
				"HCDE predict softwarn ack=%d/%d (lag=%d) avail=%d backlog=%d mirror-delta=%d stale-attack=%u passive=%llu invasion=%s wave=%d triggers=%s%s%s%s%s%s",
				currentAck, newestTic, seqAckLag, availableTics, commandBacklog, mirrorDelta,
				staleAttackingMirrors,
				static_cast<unsigned long long>(windowPassiveClient),
				Net_InvasionStateName(InvasionState), InvasionWaveDirector.Wave,
				warnAckLag ? "ack-lag " : "",
				warnAvailable ? "available-low " : "",
				warnMirror ? "mirror-drift " : "",
				warnStaleAttack ? "stale-attack " : "",
				warnPassiveStorm ? "passive-storm " : "",
				warnHardChurn ? "hard-churn " : "");

			if (*hcde_hud_debug)
			{
				Printf(PRINT_HIGH,
					"HCDE predict softwarn ack=%d/%d (lag=%d) avail=%d backlog=%d mirror-delta=%d stale-attack=%u passive=%llu\n",
					currentAck, newestTic, seqAckLag, availableTics, commandBacklog, mirrorDelta,
					staleAttackingMirrors,
					static_cast<unsigned long long>(windowPassiveClient));
			}

			if (level >= 3
				&& HCDELiveReportIntervalElapsed(HCDEPredictionDebugLastTraceDumpMS, 5000u))
			{
				const FString tracePath = FStringf("%s/hcde_prediction_softwarn_trace_%llu.txt",
					M_GetAppDataPath(true).GetChars(), static_cast<unsigned long long>(nowMS));
				DebugTrace::SaveToFile(tracePath.GetChars(), "net", DebugTrace::Severity::Debug);
			}
		}
	}

	// Roll the window forward.
	HCDEPredictionDebugWindowPassiveClient = HCDEPredictionDebugLifetime.PassiveClientResends;
	HCDEPredictionDebugWindowPassiveAuth = HCDEPredictionDebugLifetime.PassiveAuthorityResends;
	HCDEPredictionDebugWindowLocalRepairs = lifetimeLocalRepairs;
	HCDEPredictionDebugWindowHardRepairs = lifetimeHardRepairs;
	HCDEPredictionDebugWindowFaultReports = lifetimeFaultReports;
	HCDEPredictionDebugMinAvailable = availableTics;
	HCDEPredictionDebugMaxAckLag = seqAckLag;
	HCDEPredictionDebugMaxStaleVisual = maxVisualStaleThisFrame;
	HCDEPredictionDebugMaxBacklog = commandBacklog;
	HCDEPredictionDebugLastSampleTic = gametic;
}
