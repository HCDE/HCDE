// This file is split from d_net.cpp


// These have to be here since they have game-specific data. Only the data
// from the frontend should be put in these, all backend handling should be
// done in the core files.

size_t Net_SetEngineInfo(uint8_t*& stream)
{
	stream[0] = VER_MAJOR % 256;
	stream[1] = VER_MINOR % 256;
	stream[2] = VER_REVISION % 256;

	// Send over any loaded files to ensure their checksum is correct.
	size_t numWads = 0u;
	size_t bufferIndex = 7u;
	for (size_t i = 0u; i < fileSystem.GetNumWads(); ++i)
	{
		if (fileSystem.IsOptionalResource(i))
			continue;

		++numWads;
		const FString crc = fileSystem.GetResourceHash(i);
		memcpy(&stream[bufferIndex], crc.GetChars(), crc.Len() + 1u);
		bufferIndex += crc.Len() + 1u;
	}

	stream[3] = (numWads >> 24);
	stream[4] = (numWads >> 16);
	stream[5] = (numWads >> 8);
	stream[6] = numWads;

	return bufferIndex;
}

static bool ReadPacketString(const uint8_t* stream, size_t packetLength, size_t& offset, FString& value)
{
	if (offset >= packetLength)
		return false;

	const size_t start = offset;
	while (offset < packetLength && stream[offset] != 0u)
		++offset;

	if (offset >= packetLength)
		return false;

	value = FString(reinterpret_cast<const char*>(&stream[start]), offset - start);
	++offset;
	return true;
}

FVerificationError Net_VerifyEngine(uint8_t*& stream, size_t& offset, size_t packetLength)
{
	FVerificationError error = {};

	TArray<FString> crcs = {};
	TArray<FString> names = {};
	for (size_t i = 0u; i < fileSystem.GetNumWads(); ++i)
	{
		if (!fileSystem.IsOptionalResource(i))
		{
			crcs.Push(fileSystem.GetResourceHash(i));
			names.Push(fileSystem.GetResourceFileName(i));
		}
	}

	if (packetLength < 7u)
	{
		error.Error = FVerificationError::VE_ENGINE;
		error.Major = VER_MAJOR % 256;
		error.Minor = VER_MINOR % 256;
		error.Revision = VER_REVISION % 256;
		error.NetMajor = 0;
		error.NetMinor = 0;
		error.NetRevision = 0;
		return error;
	}

	const size_t numWads = (stream[3] << 24) | (stream[4] << 16) | (stream[5] << 8) | stream[6];
	if (numWads < crcs.Size())
		error.Error = FVerificationError::VE_FILE_MISSING;
	else if (numWads > crcs.Size())
		error.Error = FVerificationError::VE_FILE_UNKNOWN;

	TArray<size_t> unverified = {};
	for (size_t i = 0u; i < crcs.Size(); ++i)
		unverified.Push(i);

	offset = 7u;
	for (size_t i = 0u; i < numWads; ++i)
	{
		FString netCrc = {};
		if (!ReadPacketString(stream, packetLength, offset, netCrc))
		{
			error.Error = FVerificationError::VE_ENGINE;
			error.Major = VER_MAJOR % 256;
			error.Minor = VER_MINOR % 256;
			error.Revision = VER_REVISION % 256;
			error.NetMajor = stream[0];
			error.NetMinor = stream[1];
			error.NetRevision = stream[2];
			return error;
		}

		if (error.Error == FVerificationError::VE_FILE_UNKNOWN)
		{
			if (crcs.Find(netCrc) >= crcs.Size())
				error.UnknownFiles.Push(netCrc);
		}
		else if (crcs[i] != netCrc)
		{
			const size_t c = crcs.Find(netCrc);
			if (c >= crcs.Size())
			{
				error.Error = FVerificationError::VE_FILE_UNKNOWN;
				error.UnknownFiles.Push(netCrc);
			}
			else
			{
				if (error.Error == FVerificationError::VE_NONE)
					error.Error = FVerificationError::VE_FILE_ORDER;
				unverified.Delete(unverified.Find(c));
			}
		}
		else
		{
			unverified.Delete(unverified.Find(i));
		}
	}

	if (error.Error == FVerificationError::VE_FILE_MISSING)
	{
		for (auto i : unverified)
		{
			FixPathSeperator(names[i]);
			auto ar = names[i].Split('/', FString::TOK_SKIPEMPTY);
			error.MissingFiles.Push(ar.Last());
		}
	}
	else if (error.Error == FVerificationError::VE_FILE_ORDER)
	{
		error.ExpectedOrder = crcs;
		// Remove the core and iwad files.
		error.ExpectedOrder.Delete(0);
		error.ExpectedOrder.Delete(0);
	}

	// Intentionally do this last to avoid messing with the above loop.
	if (stream[0] != (VER_MAJOR % 256) || stream[1] != (VER_MINOR % 256) || stream[2] != (VER_REVISION % 256))
	{
		error.Error = FVerificationError::VE_ENGINE;
		error.Major = VER_MAJOR % 256;
		error.Minor = VER_MINOR % 256;
		error.Revision = VER_REVISION % 256;
		error.NetMajor = stream[0];
		error.NetMinor = stream[1];
		error.NetRevision = stream[2];
	}

	return error;
}

void Net_SetupUserInfo()
{
	D_SetupUserInfo();
}

const char* Net_GetClientName(int client, unsigned int charLimit)
{
	if (client < 0 || client >= MAXPLAYERS)
	{
		return "unknown";
	}
	return players[client].userinfo.GetName(charLimit);
}

void Net_GetKickableClientList(TArray<int>& clients, TArray<FString>& labels)
{
	clients.Clear();
	labels.Clear();

	if (!netgame)
	{
		return;
	}

	for (auto client : NetworkClients)
	{
		if (client == consoleplayer || client < 0 || client >= MAXPLAYERS || I_IsHCDEServiceAuthoritySlot(client))
		{
			continue;
		}

		FString label;
		label.Format("%s [%d]", Net_GetClientName(client, 24u), client);
		clients.Push(client);
		labels.Push(label);
	}
}

void Net_SetUserInfo(int client, TArrayView<uint8_t>& stream)
{
	auto str = D_GetUserInfoStrings(client, true);
	WriteFString(str, stream);
}

void Net_ReadUserInfo(int client, TArrayView<uint8_t>& stream)
{
	D_ReadUserInfoStrings(client, stream, false);
}

void Net_SetMapLoadInfo(TArrayView<uint8_t>& stream)
{
	WriteFString(startmap, stream);
	WriteInt32(rngseed, stream);

	auto load = Args->CheckValue(FArg_loadgame);
	if (load != nullptr)
	{
		WriteInt8(1, stream);
		WriteString(load, stream);
	}
	else
	{
		WriteInt8(0, stream);
	}
}

void Net_ReadMapLoadInfo(TArrayView<uint8_t>& stream)
{
	startmap = ReadStringConst(stream);
	rngseed = ReadInt32(stream);

	if (ReadInt8(stream))
	{
		auto load = ReadString(stream);
		// Don't override the existing argument in case they need to use
		// a custom savefile name.
		if (!Args->CheckParm(FArg_loadgame))
		{
			Args->AppendArg(FArg_loadgame);
			Args->AppendRawArg(load);
		}
	}

	// Reset this immediately so any further RNG calls the engine has to make will be synced.
	FRandom::StaticClearRandom();
}

void Net_SetServerInfo(TArrayView<uint8_t>& stream)
{
	C_WriteCVars(stream, CVAR_SERVERINFO, true);
}

void Net_ReadServerInfo(TArrayView<uint8_t>& stream)
{
	C_ReadCVars(stream);
}

void Net_SetGameInfo(TArrayView<uint8_t>& stream)
{
	Net_SetMapLoadInfo(stream);
	Net_SetServerInfo(stream);
}

void Net_ReadGameInfo(TArrayView<uint8_t>& stream)
{
	Net_ReadMapLoadInfo(stream);
	Net_ReadServerInfo(stream);
}

// Connects players to each other if needed.
bool D_CheckNetGame()
{
	if (!I_InitNetwork())
		return false;

	if (Args->CheckParm(FArg_extratic))
		net_extratic = true;

	HCDEInitializeAuthoritySession();
	for (auto client : NetworkClients)
	{
		if (I_IsServerReservedSlot(client))
		{
			// The dedicated authority is a transport slot, not a pawn. Keeping it
			// in playeringame lets it consume/block coop starts on maps with a
			// single player start, which can trap real clients on respawn.
			playeringame[client] = false;
			continue;
		}
		playeringame[client] = true;
	}

	if (MaxClients > 1u)
	{
		const int visibleClients = I_GetVisibleMaxClients();
		if (I_IsDedicatedServerMode())
		{
			Printf("Dedicated server for %d player%s\n", visibleClients, visibleClients == 1 ? "" : "s");
		}
		else
		{
			const int visiblePlayer = I_ToVisibleClientSlot(consoleplayer) + 1;
			Printf("Player %d of %d\n", visiblePlayer, visibleClients);
		}
	}

	return true;
}

//
// D_QuitNetGame
// Called before quitting to leave a net game
// without hanging the other players
//
void D_QuitNetGame(const char* reason)
{
	if (!netgame || !usergame || consoleplayer == -1 || demoplayback || NetworkClients.Size() == 1)
		return;

	const char* quitReason = reason != nullptr && reason[0] != '\0' ? reason : "unspecified";
	Printf(PRINT_HIGH, "NetGame:: Local player %d '%s' leaving net game reason=%s at gametic=%d clienttic=%d room=%u map=%s gamestate=%d gameaction=%d levelstart=%d clients=%u\n",
		consoleplayer, players[consoleplayer].userinfo.GetName(), quitReason, gametic, ClientTic, unsigned(CurrentRoomID),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		int(gamestate), int(gameaction), int(LevelStartStatus), unsigned(NetworkClients.Size()));
	DebugTrace::Warningf("net", "local quit player=%d name=%s reason=%s gametic=%d clienttic=%d room=%u map=%s gamestate=%d gameaction=%d levelstart=%d clients=%u",
		consoleplayer, players[consoleplayer].userinfo.GetName(), quitReason, gametic, ClientTic, unsigned(CurrentRoomID),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		int(gamestate), int(gameaction), int(LevelStartStatus), unsigned(NetworkClients.Size()));

	// Send a bunch of packets for stability.
	NetBuffer[0] = NCMD_EXIT;
	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	if (I_IsLocalHCDEServiceAuthority())
	{
		// This currently doesn't really do anything, but it's being split off into its
		// own branch should proper host migration be added in the future (i.e. sending over stored event
		// data rather than just dropping it entirely).
		NetBuffer[1] = HCDESelectNextServiceAuthoritySlot(authoritySlot);
		for (int i = 0; i < 4; ++i)
		{
			for (auto client : NetworkClients)
			{
				if (!I_IsHCDEServiceAuthoritySlot(client))
					HSendPacket(client, 2);
			}

			I_WaitVBL(1);
		}
	}
	else
	{
		for (int i = 0; i < 4; ++i)
		{
			// Only the service authority should know about this information.
			HSendPacket(authoritySlot, 1);
			I_WaitVBL(1);
		}
	}
}

static double HCDEProfileAverage(uint64_t total, uint64_t count)
{
	return count > 0u ? double(total) / double(count) : 0.0;
}

static void HCDEPrintPregameProfile();

static void HCDEPrintLiveLaneSummary()
{
	Printf(PRINT_HIGH, "  lanes:\n");
	for (uint8_t lane = 0u; lane < HLANE_COUNT; ++lane)
	{
		const auto& stats = HCDELiveProfile.Lanes[lane];
		Printf(PRINT_HIGH,
			"    %s protected=%d budget=%zu tx=%llu/%llu rx=%llu/%llu deferred=%llu clamps=%llu\n",
			HCDELiveLaneName(lane),
			HCDELiveLaneProtected(lane) ? 1 : 0,
			HCDELiveLaneDefaultBudgetBytes(lane),
			static_cast<unsigned long long>(stats.TxPackets),
			static_cast<unsigned long long>(stats.TxBytes),
			static_cast<unsigned long long>(stats.RxPackets),
			static_cast<unsigned long long>(stats.RxBytes),
			static_cast<unsigned long long>(stats.Deferred),
			static_cast<unsigned long long>(stats.BudgetClamps));
	}
}

static void HCDEPrintLiveProfile()
{
	const double avgWorldMS = HCDEProfileAverage(HCDELiveProfile.WorldTicMicros, HCDELiveProfile.WorldTics) / 1000.0;
	const double maxWorldMS = double(HCDELiveProfile.WorldTicMaxMicros) / 1000.0;
	Printf(PRINT_HIGH,
		"HCDE net profile: room=%u gametic=%d clienttic=%d clients=%u authority=%d invasion-active=%d tracked=%u\n",
		unsigned(CurrentRoomID), gametic, ClientTic, unsigned(NetworkClients.Size()),
		I_IsLocalHCDEServiceAuthority() ? 1 : 0,
		Net_GetInvasionActiveMonsterCount(), unsigned(InvasionReplicatedActors.Size()));
	Printf(PRINT_HIGH,
		"  live: wrapped=%llu bytes=%llu payload=%llu encode-fail=%llu cap-reject=%llu legacy-gameplay-reject=%llu replay-req=%llu replay-suppress=%llu replay-escalate=%llu control-tx=%llu/%llu control-rx=%llu/%llu caps-tx=%llu caps-rx=%llu legacy-rx=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.LivePacketsWrapped),
		static_cast<unsigned long long>(HCDELiveProfile.LiveBytesWrapped),
		static_cast<unsigned long long>(HCDELiveProfile.LivePayloadBytesWrapped),
		static_cast<unsigned long long>(HCDELiveProfile.LiveNativeEncodeFailures),
		static_cast<unsigned long long>(HCDELiveProfile.LiveNativeCapabilityRejects),
		static_cast<unsigned long long>(HCDELiveProfile.LiveLegacyGameplayRejected),
		static_cast<unsigned long long>(HCDELiveProfile.LiveReplayRequests),
		static_cast<unsigned long long>(HCDELiveProfile.LiveReplaySuppressions),
		static_cast<unsigned long long>(HCDELiveProfile.LiveReplayEscalations),
		static_cast<unsigned long long>(HCDELiveProfile.ControlPacketsSent),
		static_cast<unsigned long long>(HCDELiveProfile.ControlBytesSent),
		static_cast<unsigned long long>(HCDELiveProfile.ControlPacketsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.ControlBytesReceived),
		static_cast<unsigned long long>(HCDELiveProfile.CapabilityControlsSent),
		static_cast<unsigned long long>(HCDELiveProfile.CapabilityControlsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.LegacyControlsReceived));
	Printf(PRINT_HIGH,
		"  client-input: built=%llu native-built=%llu bytes=%llu recv=%llu bytes=%llu native-apply=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.ClientInputPacketsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ClientInputNativeBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ClientInputBytesBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ClientInputPacketsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.ClientInputBytesReceived),
		static_cast<unsigned long long>(HCDELiveProfile.ClientInputNativeApplied));
	Printf(PRINT_HIGH,
		"  snapshots: built=%llu native-built=%llu bytes=%llu recv=%llu bytes=%llu native-apply=%llu world-delta-tx=%llu records=%llu bytes=%llu world-delta-rx=%llu records=%llu bytes=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.ServerSnapshotPacketsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ServerSnapshotNativeBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ServerSnapshotBytesBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ServerSnapshotPacketsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.ServerSnapshotBytesReceived),
		static_cast<unsigned long long>(HCDELiveProfile.ServerSnapshotNativeApplied),
		static_cast<unsigned long long>(HCDELiveProfile.WorldDeltaPacketsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.WorldDeltaRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.WorldDeltaBytesBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.WorldDeltaPacketsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.WorldDeltaRecordsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.WorldDeltaBytesReceived));
	Printf(PRINT_HIGH,
		"  competitive-player-lane: max-bytes=%llu max-records=%llu budget-pressure=%llu missing-records=%llu local-health=%llu local-state=%llu respawn-hard=%llu death-hard=%llu predict-faults=%llu attack-faults=%llu move-faults=%llu remote-repairs=%llu shared-player-suppressed=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMaxBytes),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMaxRecords),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotBudgetPressure),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMissingRecords),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionLocalHealthRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionLocalStateRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionHardRespawnRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionHardDeathRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionFaultReports),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionFaultAttackReports),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionFaultMoveReports),
		static_cast<unsigned long long>(HCDELiveProfile.RemotePlayerBaselineRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorPlayerRecordsSuppressed));
	Printf(PRINT_HIGH,
		"  authority-events: packets-tx=%llu bytes=%llu records=%llu deferred=%llu catchup-records=%llu catchup-complete=%llu packets-rx=%llu bytes=%llu recv=%llu applied=%llu missing=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventPacketsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventBytesBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsDeferred),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventCatchupRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventCatchupWindowsCompleted),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventPacketsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventBytesReceived),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsApplied),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsMissing));
	Printf(PRINT_HIGH,
		"    authority-breakdown tx spawn=%llu damage=%llu despawn=%llu pickup-spawn=%llu pickup-retire=%llu superseded=%llu rx spawn=%llu damage=%llu despawn=%llu pickup-spawn=%llu pickup-retire=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventSpawnRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventDamageRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventDespawnRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventPickupSpawnRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventPickupRetireRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsSuperseded),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventSpawnRecordsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventDamageRecordsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventDespawnRecordsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventPickupSpawnRecordsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventPickupRetireRecordsReceived));
	Printf(PRINT_HIGH,
		"  invasion: snapshots-tx=%llu bytes=%llu rx=%llu bytes=%llu legacy-spawns=retired legacy-actor-deltas=retired\n",
		static_cast<unsigned long long>(HCDELiveProfile.InvasionSnapshotPacketsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.InvasionSnapshotBytesBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.InvasionSnapshotPacketsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.InvasionSnapshotBytesReceived));
	Printf(PRINT_HIGH,
		"  actor-index: id-hit=%llu id-miss=%llu ptr-hit=%llu ptr-miss=%llu rebuilds=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.InvasionActorIdLookupHits),
		static_cast<unsigned long long>(HCDELiveProfile.InvasionActorIdLookupMisses),
		static_cast<unsigned long long>(HCDELiveProfile.InvasionActorPtrLookupHits),
		static_cast<unsigned long long>(HCDELiveProfile.InvasionActorPtrLookupMisses),
		static_cast<unsigned long long>(HCDELiveProfile.InvasionActorIndexRebuilds));
	Printf(PRINT_HIGH,
		"  shared-actors: tracked=%u classes=%u reg=%llu upd=%llu retired=%llu compacted=%llu id-hit=%llu id-miss=%llu ptr-hit=%llu ptr-miss=%llu class-reg=%llu\n",
		unsigned(HCDEReplicatedActors.Size()),
		unsigned(HCDEReplicatedActorClasses.Size()),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorRegistered),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorUpdated),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorRetired),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorCompacted),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorIdLookupHits),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorIdLookupMisses),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorPtrLookupHits),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorPtrLookupMisses),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorClassRegistered));
	Printf(PRINT_HIGH,
		"  mode-migration: scans=%llu considered=%llu touched=%llu invasion=%llu coop=%llu dm=%llu last=%u/%u/%u/%u/%u\n",
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationScans),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationActorsConsidered),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationActorsRegistered),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationInvasionActive),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationCoopActive),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationDMActive),
		HCDEModeMigrationLastConsidered,
		HCDEModeMigrationLastRegistered,
		HCDEModeMigrationLastInvasion,
		HCDEModeMigrationLastCoop,
		HCDEModeMigrationLastDM);
	Printf(PRINT_HIGH,
		"  actor-queues: builds=%llu candidates=%llu priority=%llu deferred=%llu max-depth=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.ActorQueueBuilds),
		static_cast<unsigned long long>(HCDELiveProfile.ActorQueueCandidates),
		static_cast<unsigned long long>(HCDELiveProfile.ActorQueuePriorityCandidates),
		static_cast<unsigned long long>(HCDELiveProfile.ActorQueueDeferredCandidates),
		static_cast<unsigned long long>(HCDELiveProfile.ActorQueueMaxDepth));
	Printf(PRINT_HIGH,
		"  actor-interest: critical=%llu high=%llu medium=%llu low=%llu dormant=%llu skipped=%llu keepalive=%llu protected=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestCritical),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestHigh),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestMedium),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestLow),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestDormant),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestSkipped),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestKeepAlive),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestProtected));
	Printf(PRINT_HIGH,
		"  projectile-policy: eval=%llu critical=%llu high=%llu medium=%llu low=%llu dormant=%llu skipped=%llu keepalive=%llu protected=%llu inbound=%llu player-owned=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyEvaluated),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyCritical),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyHigh),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyMedium),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyLow),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyDormant),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicySkipped),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyKeepAlive),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyProtected),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyInbound),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyPlayerOwned));
	Printf(PRINT_HIGH,
		"  actor-delta-v2: packets-tx=%llu bytes=%llu records=%llu full=%llu partial=%llu unchanged=%llu budget-defer=%llu rx=%llu recv=%llu applied=%llu missing=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2PacketsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2BytesBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2RecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2FullRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2PartialRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2SkippedUnchanged),
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2DeferredBudget),
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2PacketsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2RecordsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2RecordsApplied),
		static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2RecordsMissing));
	Printf(PRINT_HIGH,
		"  baseline-repair: active=%d windows=%llu resets=%llu authority-catchup-records=%llu authority-catchup-complete=%llu\n",
		HCDECountActiveActorBaselineRepairs(),
		static_cast<unsigned long long>(HCDELiveProfile.ActorBaselineRepairWindows),
		static_cast<unsigned long long>(HCDELiveProfile.ActorBaselineRepairResets),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventCatchupRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventCatchupWindowsCompleted));
	Printf(PRINT_HIGH,
		"  sim-lod: passes=%llu full=%llu reduced=%llu dormant=%llu suspended=%llu restored=%llu think=%llu skipped=%llu wake-health=%llu wake-distance=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.SimLODPasses),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODFull),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODReduced),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODDormant),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODSuspended),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODRestored),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODThinkAllowed),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODThinkSkipped),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODWakeHealth),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODWakeDistance));
		Printf(PRINT_HIGH,
			"  world-tic: count=%llu avg=%.3fms max=%.3fms\n",
			static_cast<unsigned long long>(HCDELiveProfile.WorldTics),
			avgWorldMS, maxWorldMS);
		const double avgMirrorMS = HCDEProfileAverage(HCDELiveProfile.MirrorVisualMicros, HCDELiveProfile.MirrorVisualPasses) / 1000.0;
		const double maxMirrorMS = double(HCDELiveProfile.MirrorVisualMaxMicros) / 1000.0;
		Printf(PRINT_HIGH,
			"  mirror-visual: passes=%llu updated=%llu skipped=%llu avg=%.3fms max=%.3fms pending-spawns=%u pending-events=%u tracked=%u\n",
			static_cast<unsigned long long>(HCDELiveProfile.MirrorVisualPasses),
			static_cast<unsigned long long>(HCDELiveProfile.MirrorVisualActorsUpdated),
			static_cast<unsigned long long>(HCDELiveProfile.MirrorVisualActorsSkipped),
			avgMirrorMS,
			maxMirrorMS,
			unsigned(InvasionPendingMirrorSpawns.Size()),
			unsigned(InvasionPendingSpawnEvents.Size()),
			unsigned(InvasionReplicatedActors.Size()));
		HCDEPrintLiveLaneSummary();
		HCDEPrintPregameProfile();
	}

	static void HCDEPrintPregameProfile()
	{
		const auto& pregame = I_GetHCDEPregameServiceProfile();
		Printf(PRINT_HIGH,
			"  pregame packets: received=%llu too-short=%llu missing-payload=%llu bad-crc=%llu compressed-malformed=%llu decompress-fail=%llu oversized=%llu\n",
			static_cast<unsigned long long>(pregame.PacketReceived),
			static_cast<unsigned long long>(pregame.PacketTooShort),
			static_cast<unsigned long long>(pregame.PacketMissingPayload),
			static_cast<unsigned long long>(pregame.PacketBadCrc),
			static_cast<unsigned long long>(pregame.PacketCompressedMalformed),
			static_cast<unsigned long long>(pregame.PacketCompressedDecompressFailure),
			static_cast<unsigned long long>(pregame.PacketOversized));
	Printf(PRINT_HIGH,
		"  pregame services: too-short=%llu token-mismatch=%llu ack-out-of-range=%llu seq-zero=%llu seq-replay=%llu queue-reused=%llu queue-full-add=%llu queue-full-commit=%llu malformed=%llu sent=%llu retransmit=%llu acked=%llu timeout-drops=%llu unsupported=%llu\n",
		static_cast<unsigned long long>(pregame.ServicePacketsTooShort),
		static_cast<unsigned long long>(pregame.ServiceTokenMismatch),
		static_cast<unsigned long long>(pregame.ServiceAckOutOfRange),
			static_cast<unsigned long long>(pregame.ServiceSeqZero),
			static_cast<unsigned long long>(pregame.ServiceSeqReplayOrDuplicate),
			static_cast<unsigned long long>(pregame.ServiceQueueReused),
			static_cast<unsigned long long>(pregame.ServiceQueueFullAdd),
			static_cast<unsigned long long>(pregame.ServiceQueueFullCommit),
			static_cast<unsigned long long>(pregame.ServiceQueueMalformed),
			static_cast<unsigned long long>(pregame.ServiceQueueSent),
			static_cast<unsigned long long>(pregame.ServiceQueueRetransmit),
		static_cast<unsigned long long>(pregame.ServiceQueueAcked),
		static_cast<unsigned long long>(pregame.ServiceTimeoutDrops),
		static_cast<unsigned long long>(pregame.ServiceUnsupported));
	Printf(PRINT_HIGH,
		"  pregame quarantine: active=%d strikes=%llu quarantine-activations=%llu quarantine-drops=%llu\n",
		I_CountHCDEPregameServiceQuarantines(),
		static_cast<unsigned long long>(pregame.ServiceMalformedStrikes),
		static_cast<unsigned long long>(pregame.ServiceMalformedQuarantineActivations),
		static_cast<unsigned long long>(pregame.ServiceMalformedQuarantineDrops));
	}

	static void HCDEPrintLiveStressReport()
	{
	Net_CompactHCDEReplicatedActors();

	// Fold the detailed live counters into a short pressure snapshot for soak logs.
	int activeShared = 0;
	int retiredShared = 0;
	int sourceCounts[HREP_SOURCE_DM + 1] = {};
	int categoryCounts[HREP_ACTOR_VISUAL + 1] = {};
	for (const auto& ref : HCDEReplicatedActors)
	{
		if (ref.Active)
			++activeShared;
		if (ref.Retired)
			++retiredShared;
		if (ref.Source <= HREP_SOURCE_DM)
			++sourceCounts[ref.Source];
		if (ref.Category <= HREP_ACTOR_VISUAL)
			++categoryCounts[ref.Category];
	}

	const uint64_t deltaPackets = HCDELiveProfile.ActorDeltaV2PacketsBuilt;
	const uint64_t deltaBytes = HCDELiveProfile.ActorDeltaV2BytesBuilt;
	const uint64_t deltaRecords = HCDELiveProfile.ActorDeltaV2RecordsBuilt;
	const uint64_t deferredRecords = HCDELiveProfile.ActorDeltaV2DeferredBudget;
	const double avgDeltaBytes = HCDEProfileAverage(deltaBytes, deltaPackets);
	const double avgDeltaRecords = HCDEProfileAverage(deltaRecords, deltaPackets);
	const double deferredPerSent = deltaRecords > 0u ? (double(deferredRecords) * 100.0) / double(deltaRecords) : 0.0;
	const double avgWorldMS = HCDEProfileAverage(HCDELiveProfile.WorldTicMicros, HCDELiveProfile.WorldTics) / 1000.0;
	const double maxWorldMS = double(HCDELiveProfile.WorldTicMaxMicros) / 1000.0;
	const auto& pregame = I_GetHCDEPregameServiceProfile();
	const uint64_t pregameServiceQueueTx = pregame.ServiceQueueSent + pregame.ServiceQueueRetransmit;
	const uint64_t pregameServiceDrops = pregame.ServiceTimeoutDrops + pregame.ServiceAckOutOfRange;
	const uint64_t pregamePacketErrs = pregame.PacketTooShort + pregame.PacketMissingPayload + pregame.PacketBadCrc
		+ pregame.PacketCompressedMalformed + pregame.PacketCompressedDecompressFailure + pregame.PacketOversized;

	DebugTrace::Infof("net",
		"stress report mode=%s clients=%u world_avg_ms=%.3f world_max_ms=%.3f shared_active=%d invasion_active=%d player_snapshot_max=%llu player_snapshot_pressure=%llu local_repairs=%llu hard_repairs=%llu authority_records=%llu authority_deferred=%llu catchup_records=%llu baseline_repairs=%d delta_packets=%llu delta_records=%llu deferred=%llu queue_max=%llu projectile_eval=%llu projectile_skipped=%llu projectile_protected=%llu lod=%u/%u/%u pregame_packet_rx=%llu pregame_service_tx=%llu pregame_service_drops=%llu pregame_packet_errors=%llu",
		Net_IsInvasionModeEnabled() ? "invasion" : (deathmatch ? "dm" : "coop"),
		unsigned(NetworkClients.Size()),
		avgWorldMS, maxWorldMS,
		activeShared,
		Net_GetInvasionActiveMonsterCount(),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMaxBytes),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotBudgetPressure),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionLocalHealthRepairs + HCDELiveProfile.PredictionLocalStateRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionHardRespawnRepairs + HCDELiveProfile.PredictionHardDeathRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsDeferred),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventCatchupRecordsBuilt),
		HCDECountActiveActorBaselineRepairs(),
		static_cast<unsigned long long>(deltaPackets),
		static_cast<unsigned long long>(deltaRecords),
		static_cast<unsigned long long>(deferredRecords),
		static_cast<unsigned long long>(HCDELiveProfile.ActorQueueMaxDepth),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyEvaluated),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicySkipped),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyProtected),
		InvasionSimulationLODCurrent[HSIMLOD_FULL],
		InvasionSimulationLODCurrent[HSIMLOD_REDUCED],
		InvasionSimulationLODCurrent[HSIMLOD_DORMANT],
		static_cast<unsigned long long>(pregame.PacketReceived),
		static_cast<unsigned long long>(pregameServiceQueueTx),
		static_cast<unsigned long long>(pregameServiceDrops),
		static_cast<unsigned long long>(pregamePacketErrs));
	Printf(PRINT_HIGH,
		"  pregame quarantine: active=%d strikes=%llu quarantine-activations=%llu quarantine-drops=%llu\n",
		I_CountHCDEPregameServiceQuarantines(),
		static_cast<unsigned long long>(pregame.ServiceMalformedStrikes),
		static_cast<unsigned long long>(pregame.ServiceMalformedQuarantineActivations),
		static_cast<unsigned long long>(pregame.ServiceMalformedQuarantineDrops));

	Printf(PRINT_HIGH,
		"HCDE net stress report: room=%u gametic=%d mode=%s clients=%u authority=%d\n",
		unsigned(CurrentRoomID), gametic,
		Net_IsInvasionModeEnabled() ? "invasion" : (deathmatch ? "dm" : "coop"),
		unsigned(NetworkClients.Size()), I_IsLocalHCDEServiceAuthority() ? 1 : 0);
	Printf(PRINT_HIGH,
		"  world pressure: tics=%llu avg=%.3fms max=%.3fms invasion-active=%d wave=%u state=%s\n",
		static_cast<unsigned long long>(HCDELiveProfile.WorldTics),
		avgWorldMS, maxWorldMS,
		Net_GetInvasionActiveMonsterCount(),
		unsigned(Net_GetInvasionWave()),
		Net_InvasionStateName(InvasionState));
	Printf(PRINT_HIGH,
		"  actor pressure: shared-active=%d retired=%d classes=%u invasion-tracked=%u queue-max=%llu queue-deferred=%llu\n",
		activeShared, retiredShared,
		unsigned(HCDEReplicatedActorClasses.Size()),
		unsigned(InvasionReplicatedActors.Size()),
		static_cast<unsigned long long>(HCDELiveProfile.ActorQueueMaxDepth),
		static_cast<unsigned long long>(HCDELiveProfile.ActorQueueDeferredCandidates));
	Printf(PRINT_HIGH,
		"  delta pressure: packets=%llu bytes=%llu records=%llu avg-bytes=%.1f avg-records=%.1f budget-deferred=%llu deferred-per-sent=%.1f%%\n",
		static_cast<unsigned long long>(deltaPackets),
		static_cast<unsigned long long>(deltaBytes),
		static_cast<unsigned long long>(deltaRecords),
		avgDeltaBytes, avgDeltaRecords,
		static_cast<unsigned long long>(deferredRecords),
		deferredPerSent);
	Printf(PRINT_HIGH,
		"  authority pressure: packets=%llu records=%llu deferred=%llu catchup-records=%llu catchup-complete=%llu recv=%llu applied=%llu missing=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventPacketsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsDeferred),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventCatchupRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventCatchupWindowsCompleted),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsReceived),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsApplied),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsMissing));
	Printf(PRINT_HIGH,
		"    authority facts tx spawn=%llu damage=%llu despawn=%llu pickup-spawn=%llu pickup-retire=%llu superseded=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventSpawnRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventDamageRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventDespawnRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventPickupSpawnRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventPickupRetireRecordsBuilt),
		static_cast<unsigned long long>(HCDELiveProfile.AuthorityEventRecordsSuperseded));
	Printf(PRINT_HIGH,
		"  competitive lane: player-max=%llu records=%llu budget-pressure=%llu missing=%llu local-repairs=%llu/%llu hard=%llu/%llu remote-repairs=%llu shared-player-suppressed=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMaxBytes),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMaxRecords),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotBudgetPressure),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMissingRecords),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionLocalHealthRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionLocalStateRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionHardRespawnRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionHardDeathRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.RemotePlayerBaselineRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorPlayerRecordsSuppressed));
		Printf(PRINT_HIGH,
			"  baseline repair: active=%d windows=%llu resets=%llu\n",
			HCDECountActiveActorBaselineRepairs(),
			static_cast<unsigned long long>(HCDELiveProfile.ActorBaselineRepairWindows),
			static_cast<unsigned long long>(HCDELiveProfile.ActorBaselineRepairResets));
		HCDEPrintPregameProfile();
		Printf(PRINT_HIGH,
			"  relevance: critical=%llu high=%llu medium=%llu low=%llu dormant=%llu skipped=%llu keepalive=%llu protected=%llu\n",
			static_cast<unsigned long long>(HCDELiveProfile.ActorInterestCritical),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestHigh),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestMedium),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestLow),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestDormant),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestSkipped),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestKeepAlive),
		static_cast<unsigned long long>(HCDELiveProfile.ActorInterestProtected));
	Printf(PRINT_HIGH,
		"  projectile policy: eval=%llu critical/high/medium/low/dormant=%llu/%llu/%llu/%llu/%llu skipped=%llu keepalive=%llu protected=%llu inbound=%llu player-owned=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyEvaluated),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyCritical),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyHigh),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyMedium),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyLow),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyDormant),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicySkipped),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyKeepAlive),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyProtected),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyInbound),
		static_cast<unsigned long long>(HCDELiveProfile.ProjectilePolicyPlayerOwned));
	Printf(PRINT_HIGH,
		"  sim-lod: enabled=%d current=%u/%u/%u suspended=%u think=%u skipped=%u total-skipped=%llu wake=%llu/%llu\n",
		sv_invasionsimlod ? 1 : 0,
		InvasionSimulationLODCurrent[HSIMLOD_FULL],
		InvasionSimulationLODCurrent[HSIMLOD_REDUCED],
		InvasionSimulationLODCurrent[HSIMLOD_DORMANT],
		InvasionSimulationLODSuspendedCurrent,
		InvasionSimulationLODAllowedCurrent,
		InvasionSimulationLODSkippedCurrent,
		static_cast<unsigned long long>(HCDELiveProfile.SimLODThinkSkipped),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODWakeHealth),
		static_cast<unsigned long long>(HCDELiveProfile.SimLODWakeDistance));
	Printf(PRINT_HIGH,
		"  migration: scans=%llu considered=%llu last-considered=%u last-touched=%u "
		"source-shared=%d source-invasion=%d source-coop=%d source-dm=%d "
		"category-monster=%d category-projectile=%d category-pickup=%d category-map=%d "
		"category-script=%d category-visual=%d category-unknown=%d "
		"players-suppressed=%llu script-suppressed=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationScans),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationActorsConsidered),
		HCDEModeMigrationLastConsidered,
		HCDEModeMigrationLastRegistered,
		sourceCounts[HREP_SOURCE_SHARED],
		sourceCounts[HREP_SOURCE_INVASION],
		sourceCounts[HREP_SOURCE_COOP],
		sourceCounts[HREP_SOURCE_DM],
		categoryCounts[HREP_ACTOR_MONSTER],
		categoryCounts[HREP_ACTOR_PROJECTILE],
		categoryCounts[HREP_ACTOR_PICKUP],
		categoryCounts[HREP_ACTOR_MAP],
		categoryCounts[HREP_ACTOR_SCRIPT],
		categoryCounts[HREP_ACTOR_VISUAL],
		categoryCounts[HREP_ACTOR_UNKNOWN],
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationPlayerActorsSuppressed),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationScriptActorsSuppressed));
	HCDEPrintLiveLaneSummary();

	for (auto pNum : NetworkClients)
	{
		const auto& peer = HCDELivePeers[pNum];
		Printf(PRINT_HIGH,
			"  peer[%d]: negotiated=0x%llx queue=%u priority=%u deferred=%u top=%d skipped=%u keepalive=%u repair-until=%d auth-next=%u tx-seq=%u rx-seq=%u\n",
			pNum,
			static_cast<unsigned long long>(peer.NegotiatedCapabilities),
			peer.ActorQueueDepth,
			peer.ActorQueuePriorityDepth,
			peer.ActorQueueDeferredDepth,
			peer.ActorQueueTopScore,
			peer.ActorInterestSkipped,
			peer.ActorInterestKeepAlive,
			HCDEActorBaselineRepairUntilTic[pNum],
			unsigned(HCDEAuthorityEventReplayNextId[pNum]),
			peer.TxSequence,
			peer.RxSequence);
	}
}

CCMD(net_profile)
{
	HCDEPrintLiveProfile();
}

CCMD(net_profile_reset)
{
	HCDELiveProfile.Clear();
	I_ResetHCDEPregameServiceProfile();
	Printf(PRINT_HIGH, "HCDE net profile counters reset.\n");
}

// On-demand snapshot for the prediction-loss debugger. Forces a full DebugTrace
// dump plus a one-line marker into hcde_prediction_softwarn.log regardless of
// the current `net_predict_debug` level. Use when the user observes a subtle
// drift (gun floating, mirror twitching, monster shooting at nothing) but the
// existing fault logger has not fired.
//
// Callable both from the `net_predict_dump` console command and from the
// diagnostic moment-capture path (Net_DiagRunMarkLocally) so a `net_mark`
// invocation always pulls a fresh prediction dump alongside the trace anchor.
void Net_DiagRunPredictDump()
{
	const uint64_t nowMS = I_msTime();
	const player_t* localPlayer = (consoleplayer >= 0 && consoleplayer < MAXPLAYERS) ? &players[consoleplayer] : nullptr;
	const AActor* localActor = localPlayer != nullptr ? localPlayer->mo : nullptr;
	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	const FClientNetState* authorityState = (authoritySlot >= 0 && authoritySlot < MAXPLAYERS)
		? &ClientStates[authoritySlot] : nullptr;
	const FHCDELivePeerState* authorityPeer = (authoritySlot >= 0 && authoritySlot < MAXPLAYERS)
		? &HCDELivePeers[authoritySlot] : nullptr;
	const int commandBacklog = ClientTic - gametic;
	const int newestTic = authorityState != nullptr ? authorityState->CurrentSequence : -1;
	const int currentAck = authorityState != nullptr ? authorityState->SequenceAck : -1;
	const int seqAckLag = (newestTic >= 0 && currentAck >= 0) ? max(newestTic - currentAck, 0) : 0;

	unsigned blockingMirrors = 0u;
	unsigned staleAttackingMirrors = 0u;
	int maxStaleVisual = 0;
	for (const auto& ref : InvasionReplicatedActors)
	{
		AActor* actor = ref.Actor.Get();
		if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
			continue;
		if (Net_IsInvasionActorCorpseLike(actor))
			continue;
		if (!ref.IsProjectile)
			++blockingMirrors;
		const bool attacking = ref.VisualActionState == HCDEInvasionActorActionMelee
			|| ref.VisualActionState == HCDEInvasionActorActionMissile
			|| ref.VisualActionState == HCDEInvasionActorActionPain;
		if (attacking && ref.VisualTargetTic > 0)
		{
			const int age = gametic - ref.VisualTargetTic;
			if (age > maxStaleVisual)
				maxStaleVisual = age;
			if (age > TICRATE / 2)
				++staleAttackingMirrors;
		}
	}
	const int serverActive = Net_GetInvasionActiveMonsterCount();
	const int mirrorDelta = abs(int(blockingMirrors) - serverActive);

	double pspriteY = 0.0;
	if (localPlayer != nullptr)
	{
		player_t* readable = const_cast<player_t*>(localPlayer);
		if (DPSprite* sp = readable->FindPSprite(PSP_WEAPON))
			pspriteY = sp->y;
	}

	const FString warnPath = FStringf("%s/hcde_prediction_softwarn.log",
		M_GetAppDataPath(true).GetChars());
	if (FILE* warn = fopen(warnPath.GetChars(), "a"))
	{
		fprintf(warn,
			"%llu HCDE predict softwarn triggers=[manual-dump] ack=%d/%d (lag=%d) "
			"avail=- (lifetime) backlog=%d mirror=%d (blocking=%u server=%d) "
			"stale-attack=%u max-stale=%d passive-client=%llu passive-auth=%llu "
			"local-repairs=%llu hard-repairs=%llu fault-reports=%llu invasion=%s wave=%d "
			"snap-count=%u input-sent=%u viewheight=%.2f view-z-off=%.2f psprite-y=%.2f "
			"pos=(%.1f,%.1f,%.1f) vel=(%.2f,%.2f,%.2f) health=%d room=%u\n",
			static_cast<unsigned long long>(nowMS),
			currentAck, newestTic, seqAckLag, commandBacklog,
			mirrorDelta, blockingMirrors, serverActive,
			staleAttackingMirrors, maxStaleVisual,
			static_cast<unsigned long long>(HCDEPredictionDebugLifetime.PassiveClientResends),
			static_cast<unsigned long long>(HCDEPredictionDebugLifetime.PassiveAuthorityResends),
			static_cast<unsigned long long>(HCDELiveProfile.PredictionLocalHealthRepairs + HCDELiveProfile.PredictionLocalStateRepairs),
			static_cast<unsigned long long>(HCDELiveProfile.PredictionHardRespawnRepairs + HCDELiveProfile.PredictionHardDeathRepairs),
			static_cast<unsigned long long>(HCDELiveProfile.PredictionFaultReports),
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

	const FString tracePath = FStringf("%s/hcde_prediction_dump_%llu.txt",
		M_GetAppDataPath(true).GetChars(), static_cast<unsigned long long>(nowMS));
	DebugTrace::SaveToFile(tracePath.GetChars(), "net", DebugTrace::Severity::Debug);

	Printf(PRINT_HIGH,
		"HCDE prediction dump written: %s\n"
		"  ack=%d/%d (lag=%d) backlog=%d mirror-delta=%d stale-attack=%u passive-client=%llu fault-reports=%llu\n",
		tracePath.GetChars(),
		currentAck, newestTic, seqAckLag, commandBacklog, mirrorDelta,
		staleAttackingMirrors,
		static_cast<unsigned long long>(HCDEPredictionDebugLifetime.PassiveClientResends),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionFaultReports));
}

// Console wrapper around Net_DiagRunPredictDump so the user can trigger a
// dump from the menu/console. The implementation lives in the shared helper
// because the moment-capture path needs to invoke the same logic without
// re-entering the CCMD dispatcher.
CCMD(net_predict_dump)
{
	Net_DiagRunPredictDump();
}

void Net_GetLagHUDMetrics(FHCDELagHUDMetrics& out)
{
	out = HCDELagHUDLast;
}

bool Net_ShouldDrawLagHUD()
{
	return (*hcde_lag_hud || *hcde_hud_debug) && gamestate == GS_LEVEL && !demoplayback;
}

CCMD(hcde_lag_hud)
{
	if (argv.argc() <= 1)
	{
		Printf(PRINT_HIGH, "hcde_lag_hud is %s. Use `hcde_lag_hud 1` for overlay or `stat hcde_lag` for stat panel.\n",
			*hcde_lag_hud ? "on" : "off");
		return;
	}
	hcde_lag_hud = !!atoi(argv[1]);
}

CCMD(hcde_lag_stat)
{
	FStat::ToggleStat("hcde_lag");
	Printf(PRINT_HIGH, "stat hcde_lag toggled. Use `stat hcde_lag 0` to disable.\n");
}

ADD_STAT(hcde_lag)
{
	const FHCDELagHUDMetrics& m = HCDELagHUDLast;
	FString out;
	out.AppendFormat("HCDE lag monitor (gametic=%d clienttic=%d)\n", m.Gametic, m.ClientTic);
	out.AppendFormat("lag=%s backlog=%d avail=%d run=%d total=%d stale=%d%s\n",
		m.LagState, m.CommandBacklog, m.AvailableTics, m.RunTics, m.TotalTics, m.SimStaleTics,
		m.TicGateStalled ? " STALL" : "");
	out.AppendFormat("invasion=%s wave=%d%s\n",
		m.InvasionState, m.InvasionWave,
		(m.InvasionState != nullptr && strcmp(m.InvasionState, "disabled") == 0) ? " (idle: not started yet)" : "");
	out.AppendFormat("mirrors tracked=%d pending-spawn=%d pending-event=%d steps=%d\n",
		m.TrackedMirrors, m.PendingSpawns, m.PendingEvents, m.WorldSteps);
	out.AppendFormat("mirror ms last=%.2f avg=%.2f max=%.2f | world ms avg=%.2f max=%.2f\n",
		m.LastMirrorMS, m.AvgMirrorMS, m.MaxMirrorMS, m.AvgWorldMS, m.MaxWorldMS);
	if (m.TicGateStalled)
		out.AppendFormat("tic gate stalled: waiting for authority snapshots\n");
	if (m.CommandBacklog > TICRATE)
		out.AppendFormat("high command backlog: client outran server sim\n");
	if (m.PendingSpawns > 16 || m.PendingEvents > 16)
		out.AppendFormat("spawn backlog: wave catch-up still draining\n");
	return out;
}

CCMD(net_unlagged)
{
	const auto& commandLane = HCDELiveProfile.Lanes[HLANE_COMMAND];
	const auto& playerLane = HCDELiveProfile.Lanes[HLANE_PLAYER_SNAPSHOT];
	const auto& actorLane = HCDELiveProfile.Lanes[HLANE_ACTOR_DELTA];
	const double avgCommandTx = HCDEProfileAverage(commandLane.TxBytes, commandLane.TxPackets);
	const double avgPlayerTx = HCDEProfileAverage(playerLane.TxBytes, playerLane.TxPackets);
	const double avgActorTx = HCDEProfileAverage(actorLane.TxBytes, actorLane.TxPackets);
	const int commandLead = max<int>((ClientTic - gametic) / max<int>(TicDup, 1), 0);
	uint32_t good = 0u;
	uint32_t degraded = 0u;
	uint32_t critical = 0u;

	Printf(PRINT_HIGH,
		"HCDE unlagged/high-ping audit: mode=%s netgame=%d dedicated-slot=%d ticdup=%u lag=%s stability=%d command-lead=%d player-lane-protected=%d shared-player-suppressed=%llu\n",
		Net_IsInvasionModeEnabled() ? "invasion" : (deathmatch ? "dm" : "coop"),
		netgame ? 1 : 0,
		I_UsesDedicatedServerSlot() ? 1 : 0,
		unsigned(TicDup),
		Net_LagStateName(LagState),
		StabilityBuffer,
		commandLead,
		HCDELiveLaneProtected(HLANE_PLAYER_SNAPSHOT) ? 1 : 0,
		static_cast<unsigned long long>(HCDELiveProfile.SharedActorPlayerRecordsSuppressed));
	Printf(PRINT_HIGH,
		"  aggregate: command-tx=%llu avg=%.1f rx=%llu player-tx=%llu avg=%.1f rx=%llu actor-tx=%llu avg=%.1f rx=%llu actor-defer=%llu\n",
		static_cast<unsigned long long>(commandLane.TxPackets), avgCommandTx,
		static_cast<unsigned long long>(commandLane.RxPackets),
		static_cast<unsigned long long>(playerLane.TxPackets), avgPlayerTx,
		static_cast<unsigned long long>(playerLane.RxPackets),
		static_cast<unsigned long long>(actorLane.TxPackets), avgActorTx,
		static_cast<unsigned long long>(actorLane.RxPackets),
		static_cast<unsigned long long>(actorLane.Deferred));
	Printf(PRINT_HIGH,
		"  aggregate pressure: player-budget=%zu missing=%llu hard-repair=%llu actor-lane-budget-clamps=%llu actor-lane-deferred=%llu\n",
		HCDELiveLaneDefaultBudgetBytes(HLANE_PLAYER_SNAPSHOT),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMissingRecords),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionHardRespawnRepairs + HCDELiveProfile.PredictionHardDeathRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.Lanes[HLANE_ACTOR_DELTA].BudgetClamps),
		static_cast<unsigned long long>(HCDELiveProfile.Lanes[HLANE_ACTOR_DELTA].Deferred));
	Printf(PRINT_HIGH,
		"  player snapshots: max-bytes=%llu max-records=%llu budget=%zu pressure=%llu missing=%llu local-health=%llu local-state=%llu hard-respawn=%llu hard-death=%llu remote-repairs=%llu\n",
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMaxBytes),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMaxRecords),
		HCDELiveLaneDefaultBudgetBytes(HLANE_PLAYER_SNAPSHOT),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotBudgetPressure),
		static_cast<unsigned long long>(HCDELiveProfile.PlayerSnapshotMissingRecords),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionLocalHealthRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionLocalStateRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionHardRespawnRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.PredictionHardDeathRepairs),
		static_cast<unsigned long long>(HCDELiveProfile.RemotePlayerBaselineRepairs));

	for (auto pNum : NetworkClients)
	{
		const auto& state = ClientStates[pNum];
		const auto& peer = HCDELivePeers[pNum];
		const auto& peerCommandLane = peer.Lanes[HLANE_COMMAND];
		const auto& peerPlayerLane = peer.Lanes[HLANE_PLAYER_SNAPSHOT];
		const auto& peerActorLane = peer.Lanes[HLANE_ACTOR_DELTA];
		const uint32_t healthScore = HCDEAssessUnlaggedPeerHealth(int(state.AverageLatency), commandLead, peer, peerPlayerLane, peerActorLane);
		const char* healthLabel = HCDEUnlaggedHealthLabel(healthScore);
		if (strcmp(healthLabel, "good") == 0)
			++good;
		else if (strcmp(healthLabel, "degraded") == 0)
			++degraded;
		else
			++critical;
		Printf(PRINT_HIGH,
			"  client=%d [%s:%u] avg-lat=%ums seq=%d ack=%d flags=0x%x stability=%u cmd=%u/%u snap=%u/%u world=%u reconcile=%u hard=%u baseline=%u drift=%u player-max=%u/%u pressure=%u actor-defer=%llu cmd-budget=%llu snap-budget=%llu actor-budget=%llu\n",
			pNum,
			healthLabel,
			healthScore,
			unsigned(state.AverageLatency),
			state.CurrentSequence,
			state.SequenceAck,
			unsigned(state.Flags),
			unsigned(state.StabilityBuffer),
			peer.ClientCommandSent,
			peer.ClientCommandReceived,
			peer.SnapshotSent,
			peer.SnapshotReceived,
			peer.WorldDeltaReceived,
			peer.Reconciliations,
			peer.HardReconciliations,
			peer.BaselineRepairs,
			peer.BaselineLocalDrift,
			peer.PlayerSnapshotMaxBytes,
			peer.PlayerSnapshotMaxRecords,
			peer.PlayerSnapshotBudgetPressure,
			static_cast<unsigned long long>(peerActorLane.Deferred),
			static_cast<unsigned long long>(peerCommandLane.BudgetClamps),
			static_cast<unsigned long long>(peerPlayerLane.BudgetClamps),
			static_cast<unsigned long long>(peerActorLane.BudgetClamps));
	}
	Printf(PRINT_HIGH,
		"  peer health summary: good=%u degraded=%u critical=%u\n",
		good, degraded, critical);
}

CCMD(net_capabilities)
{
	const uint64_t localCapabilities = HCDELiveLocalCapabilities();
	Printf(PRINT_HIGH, "HCDE live capabilities: local=0x%llx names=",
		static_cast<unsigned long long>(localCapabilities));
	HCDEPrintLiveCapabilityNames(localCapabilities, 0u);
	for (auto pNum : NetworkClients)
	{
		const auto& peer = HCDELivePeers[pNum];
		Printf(PRINT_HIGH,
			"  client=%d remote=0x%llx negotiated=0x%llx controls=%u caps=%u unsupported=0x%llx legacy=%u names=",
			pNum,
			static_cast<unsigned long long>(peer.RemoteCapabilities),
			static_cast<unsigned long long>(peer.NegotiatedCapabilities),
			peer.ControlReceived,
			peer.CapabilityControlReceived,
			static_cast<unsigned long long>(peer.UnsupportedCapabilities),
			peer.LegacyControlReceived);
		HCDEPrintLiveCapabilityNames(peer.NegotiatedCapabilities, peer.UnsupportedCapabilities);
	}
}

CCMD(net_actorstats)
{
	Net_CompactHCDEReplicatedActors();
	int categoryCounts[HREP_ACTOR_VISUAL + 1] = {};
	int active = 0;
	int retired = 0;
	int sourceCounts[HREP_SOURCE_DM + 1] = {};
	for (const auto& ref : HCDEReplicatedActors)
	{
		if (ref.Active)
			++active;
		if (ref.Retired)
			++retired;
		if (ref.Source <= HREP_SOURCE_DM)
			++sourceCounts[ref.Source];
		if (ref.Category <= HREP_ACTOR_VISUAL)
			++categoryCounts[ref.Category];
	}

	Printf(PRINT_HIGH,
		"HCDE replicated actors: tracked=%u active=%d retired=%d classes=%u id-index=%u ptr-index=%u\n",
		unsigned(HCDEReplicatedActors.Size()), active, retired,
		unsigned(HCDEReplicatedActorClasses.Size()),
		unsigned(HCDEReplicatedActorIdIndex.CountUsed()),
		unsigned(HCDEReplicatedActorPtrIndex.CountUsed()));
	for (int source = HREP_SOURCE_SHARED; source <= HREP_SOURCE_DM; ++source)
	{
		Printf(PRINT_HIGH, "  source[%s]=%d\n",
			HCDEReplicatedActorSourceName(uint8_t(source)), sourceCounts[source]);
	}
	for (int category = HREP_ACTOR_UNKNOWN; category <= HREP_ACTOR_VISUAL; ++category)
	{
		Printf(PRINT_HIGH, "  %s=%d\n",
			HCDEReplicatedActorCategoryName(uint8_t(category)), categoryCounts[category]);
	}

	const unsigned int classLimit = min<unsigned int>(unsigned(HCDEReplicatedActorClasses.Size()), 16u);
	for (unsigned int i = 0u; i < classLimit; ++i)
	{
		const PClassActor* actorClass = Net_GetHCDEReplicatedActorClass(uint16_t(i + 1u));
		Printf(PRINT_HIGH, "  class[%u]=%s\n", i + 1u,
			actorClass != nullptr ? actorClass->TypeName.GetChars() : "<missing>");
	}
	if (HCDEReplicatedActorClasses.Size() > classLimit)
		Printf(PRINT_HIGH, "  ... %u more classes\n", unsigned(HCDEReplicatedActorClasses.Size() - classLimit));
}

CCMD(net_migration)
{
	HCDEModeMigrationNextScanTic = 0;
	Net_TickHCDEModeActorMigration();
	int sourceCounts[HREP_SOURCE_DM + 1] = {};
	for (const auto& ref : HCDEReplicatedActors)
	{
		if (ref.Source <= HREP_SOURCE_DM && ref.Active)
			++sourceCounts[ref.Source];
	}
	int categoryCounts[HREP_ACTOR_VISUAL + 1] = {};
	for (const auto& ref : HCDEReplicatedActors)
	{
		if (ref.Category <= HREP_ACTOR_VISUAL)
			++categoryCounts[ref.Category];
	}
	Printf(PRINT_HIGH,
		"HCDE mode migration: mode=%s scans=%llu considered=%llu last-considered=%u last-touched=%u "
		"source-shared=%d source-invasion=%d source-coop=%d source-dm=%d "
		"category-monster=%d category-projectile=%d category-pickup=%d category-map=%d "
		"category-script=%d category-visual=%d category-unknown=%d "
		"tracked=%u players-suppressed=%llu scripts-suppressed=%llu\n",
		Net_IsInvasionModeEnabled() ? "invasion" : (deathmatch ? "dm" : "coop"),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationScans),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationActorsConsidered),
		HCDEModeMigrationLastConsidered,
		HCDEModeMigrationLastRegistered,
		sourceCounts[HREP_SOURCE_SHARED],
		sourceCounts[HREP_SOURCE_INVASION],
		sourceCounts[HREP_SOURCE_COOP],
		sourceCounts[HREP_SOURCE_DM],
		categoryCounts[HREP_ACTOR_MONSTER],
		categoryCounts[HREP_ACTOR_PROJECTILE],
		categoryCounts[HREP_ACTOR_PICKUP],
		categoryCounts[HREP_ACTOR_MAP],
		categoryCounts[HREP_ACTOR_SCRIPT],
		categoryCounts[HREP_ACTOR_VISUAL],
		categoryCounts[HREP_ACTOR_UNKNOWN],
		unsigned(HCDEReplicatedActors.Size()),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationPlayerActorsSuppressed),
		static_cast<unsigned long long>(HCDELiveProfile.ModeMigrationScriptActorsSuppressed));
	for (int source = HREP_SOURCE_SHARED; source <= HREP_SOURCE_DM; ++source)
	{
		int count = 0;
		for (const auto& ref : HCDEReplicatedActors)
		{
			if (ref.Source == source && ref.Active)
				++count;
		}
		Printf(PRINT_HIGH, "  %s=%d\n", HCDEReplicatedActorSourceName(uint8_t(source)), count);
	}
}

CCMD(net_lanes)
{
	Printf(PRINT_HIGH, "HCDE live lanes: local role=%s room=%u gametic=%d\n",
		I_IsLocalHCDEServiceAuthority() ? "authority" : "client",
		unsigned(CurrentRoomID), gametic);
	HCDEPrintLiveLaneSummary();
	for (auto pNum : NetworkClients)
	{
		const auto& peer = HCDELivePeers[pNum];
		Printf(PRINT_HIGH, "  client=%d tx-seq=%u rx-seq=%u negotiated=0x%llx queue=%u qprio=%u qdefer=%u top=%d skipped=%u keepalive=%u repair-until=%d auth-next=%u repair-windows=%u repair-resets=%u catchup-records=%u\n",
			pNum, peer.TxSequence, peer.RxSequence,
			static_cast<unsigned long long>(peer.NegotiatedCapabilities),
			peer.ActorQueueDepth,
			peer.ActorQueuePriorityDepth,
			peer.ActorQueueDeferredDepth,
			peer.ActorQueueTopScore,
			peer.ActorInterestSkipped,
			peer.ActorInterestKeepAlive,
			HCDEActorBaselineRepairUntilTic[pNum],
			unsigned(HCDEAuthorityEventReplayNextId[pNum]),
			peer.ActorBaselineRepairWindows,
			peer.ActorBaselineRepairResets,
			peer.AuthorityEventCatchupRecords);
		for (uint8_t lane = 0u; lane < HLANE_COUNT; ++lane)
		{
			const auto& stats = peer.Lanes[lane];
			if (stats.TxPackets == 0u && stats.RxPackets == 0u && stats.Deferred == 0u && stats.BudgetClamps == 0u)
				continue;
			Printf(PRINT_HIGH,
				"    %s budget=%zu active=%d tx=%llu/%llu rx=%llu/%llu deferred=%llu clamps=%llu\n",
				HCDELiveLaneName(lane),
				HCDELiveLaneDefaultBudgetBytes(lane),
				HCDELivePeerHasCapability(pNum, HCDELiveCapLaneBudgetsV1) ? 1 : 0,
				static_cast<unsigned long long>(stats.TxPackets),
				static_cast<unsigned long long>(stats.TxBytes),
				static_cast<unsigned long long>(stats.RxPackets),
				static_cast<unsigned long long>(stats.RxBytes),
				static_cast<unsigned long long>(stats.Deferred),
				static_cast<unsigned long long>(stats.BudgetClamps));
		}
	}
}

CCMD(net_relevance)
{
	Printf(PRINT_HIGH, "HCDE actor relevance: tracked=%u gametic=%d\n",
		unsigned(InvasionReplicatedActors.Size()), gametic);
	for (auto pNum : NetworkClients)
	{
		const auto& peer = HCDELivePeers[pNum];
		Printf(PRINT_HIGH,
			"  client=%d queue=%u priority=%u skipped=%u keepalive=%u top=%d\n",
			pNum,
			peer.ActorQueueDepth,
			peer.ActorQueuePriorityDepth,
			peer.ActorInterestSkipped,
			peer.ActorInterestKeepAlive,
			peer.ActorQueueTopScore);
		for (uint8_t interest = 0u; interest < HINTEREST_COUNT; ++interest)
		{
			Printf(PRINT_HIGH, "    %s=%u\n",
				HCDEActorInterestName(interest),
				peer.ActorInterestTiers[interest]);
		}
		Printf(PRINT_HIGH,
			"    projectile-policy skipped=%u keepalive=%u protected=%u\n",
			peer.ProjectilePolicySkipped,
			peer.ProjectilePolicyKeepAlive,
			peer.ProjectilePolicyProtected);
		for (uint8_t interest = 0u; interest < HINTEREST_COUNT; ++interest)
		{
			Printf(PRINT_HIGH, "      projectile-%s=%u\n",
				HCDEActorInterestName(interest),
				peer.ProjectilePolicyTiers[interest]);
		}
	}
}

CCMD(net_simlod)
{
	Printf(PRINT_HIGH,
		"HCDE invasion sim LOD: enabled=%d active=%d full=%u reduced=%u dormant=%u suspended=%u think=%u skipped=%u wake-health=%u wake-distance=%u\n",
		sv_invasionsimlod ? 1 : 0,
		Net_IsInvasionRoundActiveState(InvasionState) ? 1 : 0,
		InvasionSimulationLODCurrent[HSIMLOD_FULL],
		InvasionSimulationLODCurrent[HSIMLOD_REDUCED],
		InvasionSimulationLODCurrent[HSIMLOD_DORMANT],
		InvasionSimulationLODSuspendedCurrent,
		InvasionSimulationLODAllowedCurrent,
		InvasionSimulationLODSkippedCurrent,
		InvasionSimulationLODWakeHealthCurrent,
		InvasionSimulationLODWakeDistanceCurrent);
	Printf(PRINT_HIGH,
		"  ranges: full=%.1f reduced=%.1f intervals: reduced=%d dormant=%d\n",
		double(sv_invasionsimlodfullrange),
		double(sv_invasionsimlodreducedrange),
		int(sv_invasionsimlodreducedinterval),
		int(sv_invasionsimloddormantinterval));

	int printed = 0;
	for (const auto& ref : InvasionReplicatedActors)
	{
		if (!ref.SimulationSuspended && ref.SimulationLOD == HSIMLOD_FULL)
			continue;
		AActor* actor = ref.Actor.Get();
		Printf(PRINT_HIGH,
			"  id=%u class=%s lod=%s suspended=%d next=%d skipped=%llu allowed=%llu health=%d\n",
			unsigned(ref.Id),
			actor != nullptr && actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<missing>",
			HCDESimulationLODName(ref.SimulationLOD),
			ref.SimulationSuspended ? 1 : 0,
			ref.SimulationNextThinkTic,
			static_cast<unsigned long long>(ref.SimulationSkippedTics),
			static_cast<unsigned long long>(ref.SimulationAllowedTics),
			ref.SimulationLastHealth);
		if (++printed >= 16)
		{
			Printf(PRINT_HIGH, "  ... more LOD actors omitted\n");
			break;
		}
	}
}

CCMD(net_stressreport)
{
	HCDEPrintLiveStressReport();
}

// `net_invasion_missing_classes` - report the table of authoritative actor
// classes the local client has been asked to mirror but does not have loaded
// (most often: the dedicated server has a Doom 2 remake / monster pack the
// client is missing). Each row corresponds to a single ZScript class; the
// running totals tell you how many spawn events were silently swallowed.
//
// Symptoms that indicate this command is worth running:
//   * "I'm getting shot but nothing is visibly shooting me" - the missing
//     mirror means the server's monster has full collision and AI but no
//     client-side render proxy was ever built.
//   * Random shots into the air "hit" something - your inputs are forwarded
//     to the authoritative monster which the server is still simulating.
//   * `net.desync` traces with category=actors that don't correlate with
//     real movement drift (the count diverges because mirrors are missing).
CCMD(net_invasion_missing_classes)
{
	if (InvasionMissingClassTable.CountUsed() == 0u)
	{
		Printf(PRINT_HIGH,
			"No missing invasion mirror classes recorded since the last map / wave reset.\n"
			"(Total spawn events swallowed: %u)\n",
			unsigned(InvasionMissingClassTotalSpawns));
		return;
	}
	Printf(PRINT_HIGH,
		"HCDE invasion missing-class table: %u distinct class(es), %u spawn event(s) swallowed.\n",
		unsigned(InvasionMissingClassTable.CountUsed()),
		unsigned(InvasionMissingClassTotalSpawns));
	TMap<FString, FInvasionMissingClassRecord>::Iterator it(InvasionMissingClassTable);
	TMap<FString, FInvasionMissingClassRecord>::Pair* pair = nullptr;
	while (it.NextPair(pair))
	{
		Printf(PRINT_HIGH,
			"  class=%s count=%u first-tic=%d last-tic=%d first-wave=%d\n",
			pair->Key.GetChars(),
			unsigned(pair->Value.Count),
			pair->Value.FirstSeenTic,
			pair->Value.LastSeenTic,
			pair->Value.FirstSeenWave);
	}
	Printf(PRINT_HIGH,
		"If any of these are from a mod (e.g. a monster pack), the client is missing\n"
		"the PK3/WAD that defines the class. Load it on the client side to make those\n"
		"monsters visible. The server-authoritative monster is already shooting at you.\n");
}

ADD_STAT(network)
{
	FString out = {};
	if (!netgame || demoplayback)
	{
		out.AppendFormat("No network stats available.");
		return out;
	}

	out.AppendFormat("Max players: %d\tTic dup: %d",
		MaxClients,
		TicDup);

	if (net_extratic)
		out.AppendFormat("\tExtra tic enabled");

	const bool localAuthority = I_IsLocalHCDEServiceAuthority();
	out.AppendFormat("\nWorld tic: %06d (sequence %06d)", gametic, gametic / TicDup);
	if (!localAuthority)
		out.AppendFormat("\tStart tics delay: %d", LevelStartDebug);

	const int delay = max<int>((ClientTic - gametic) / TicDup, 0);
	const int msDelay = min<int>(delay * TicDup * 1000.0 / TICRATE, 999);
	const int buffer = max<int>(StabilityBuffer, 0);
	const int msBuffer = min<int>(buffer * 1000.0 / TICRATE, 999);
	out.AppendFormat("\nLocal\n\tIs authority: %d\tDelay: %02d (%03dms)\tStability Buffer: %02d (%03dms)",
		localAuthority,
		delay, msDelay,
		buffer, msBuffer);

	if (!localAuthority && consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
		out.AppendFormat("\tAvg latency: %03ums", min<unsigned int>(ClientStates[consoleplayer].AverageLatency, 999u));

	if (LevelStartStatus != LST_READY)
	{
		if (LevelStartStatus == LST_HOST)
			out.AppendFormat("\tWaiting for packets");
		else if (localAuthority)
			out.AppendFormat("\tWaiting for acks");
		else
			out.AppendFormat("\tWaiting for authority");
	}

	int lowestSeq = ClientTic / TicDup;
	for (auto client : NetworkClients)
	{
		if (client == consoleplayer)
			continue;

		auto& state = ClientStates[client];
		if (state.CurrentSequence < lowestSeq)
			lowestSeq = state.CurrentSequence;

		out.AppendFormat("\n%s", players[client].userinfo.GetName(12));
		if (I_IsHCDEServiceAuthoritySlot(client))
			out.AppendFormat("\t(Authority)");

		if ((state.Flags & CF_RETRANSMIT) == CF_RETRANSMIT)
			out.AppendFormat("\t(RT)");
		else if (state.Flags & CF_RETRANSMIT_SEQ)
			out.AppendFormat("\t(RT SEQ)");
		else if (state.Flags & CF_RETRANSMIT_CON)
			out.AppendFormat("\t(RT CON)");

		if ((state.Flags & CF_MISSING) == CF_MISSING)
			out.AppendFormat("\t(MISS)");
		else if (state.Flags & CF_MISSING_SEQ)
			out.AppendFormat("\t(MISS SEQ)");
		else if (state.Flags & CF_MISSING_CON)
			out.AppendFormat("\t(MISS CON)");

		out.AppendFormat("\n");

		out.AppendFormat("\tAck: %06d\tConsistency: %06d", state.SequenceAck, state.ConsistencyAck);
		if (!I_IsHCDEServiceAuthoritySlot(client))
			out.AppendFormat("\tAvg latency: %03ums", min<unsigned int>(state.AverageLatency, 999u));
	}

	if (localAuthority)
		out.AppendFormat("\nAvailable tics: %03d", max<int>(lowestSeq - (gametic / TicDup), 0));
	return out;
}

// Forces playsim processing time to be consistent across frames.
// This improves interpolation for frames in between tics.
//
// With this cvar off the mods with a high playsim processing time will appear
// less smooth as the measured time used for interpolation will vary.

CVAR(Bool, r_ticstability, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

static uint64_t stabilityticduration = 0;
static uint64_t stabilitystarttime = 0;

static void TicStabilityWait()
{
	using namespace std::chrono;
	using namespace std::this_thread;

	if (!r_ticstability)
		return;

	uint64_t start = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	while (true)
	{
		uint64_t cur = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
		if (cur - start > stabilityticduration)
			break;
	}
}

static void TicStabilityBegin()
{
	using namespace std::chrono;
	stabilitystarttime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

static void TicStabilityEnd()
{
	using namespace std::chrono;
	uint64_t stabilityendtime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	stabilityticduration = min(stabilityendtime - stabilitystarttime, (uint64_t)1'000'000);
}

// Don't stabilize tics that are going to have incredibly long pauses in them.
static bool ShouldStabilizeTick()
{
	return gameaction != ga_recordgame && gameaction != ga_newgame && gameaction != ga_newgame2
			&& gameaction != ga_loadgame && gameaction != ga_loadgamehidecon && gameaction != ga_autoloadgame && gameaction != ga_loadgameplaydemo
			&& gameaction != ga_savegame && gameaction != ga_autosave && gameaction != ga_quicksave
			&& gameaction != ga_worlddone && gameaction != ga_completed && gameaction != ga_screenshot && gameaction != ga_fullconsole;
}

// If the connection has been unstable then let the game lag behind for a little bit
// while we wait for it to stabilize, otherwise everything will appear to jitter around.
static void CalculateNetStabilityBuffer(int diff)
{
	if (!netgame || demoplayback)
	{
		StabilityBuffer = 0;
		return;
	}

	if (I_UsesDedicatedServerSlot() && !I_IsLocalHCDEServiceAuthority())
	{
		const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
		const bool lowLatencyAuthority = authoritySlot >= 0
			&& authoritySlot < MAXPLAYERS
			&& HCDELivePeers[authoritySlot].SnapshotReceived > 0u
			&& HCDELivePeers[authoritySlot].RxServerSnapshotSequence > 0u
			&& ClientStates[authoritySlot].AverageLatency <= 16u;
		if (lowLatencyAuthority)
		{
			StabilityBuffer = 0;
			PrevAvailableDiff = max<int>(diff, 0);
