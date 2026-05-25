// This file is split from d_net.cpp

	cursor += 2u;
	command.forwardmove = int16_t(HCDELiveReadBE16(&data[cursor]));
	cursor += 2u;
	command.sidemove = int16_t(HCDELiveReadBE16(&data[cursor]));
	cursor += 2u;
	command.upmove = int16_t(HCDELiveReadBE16(&data[cursor]));
	cursor += 2u;
	return true;
}

static bool HCDEAppendServerWorldDeltas(int client, uint8_t* output, size_t outputCapacity, size_t& cursor, const uint8_t* playerNums, size_t playerCount)
{
	if (playerCount > MAXPLAYERS || playerCount > UINT8_MAX)
		return false;

	const size_t startCursor = cursor;
	if (!HCDEAppendBytes(output, outputCapacity, cursor, HCDEServerWorldDeltaMagic, sizeof(HCDEServerWorldDeltaMagic))
		|| !HCDEAppendByte(output, outputCapacity, cursor, HCDEServerWorldDeltaProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max<int>(gametic, 0)))
		|| !HCDEAppendByte(output, outputCapacity, cursor, uint8_t(playerCount)))
	{
		return false;
	}

	for (size_t i = 0u; i < playerCount; ++i)
	{
		const uint8_t playerNum = playerNums[i];
		if (playerNum >= MAXPLAYERS)
			return false;

		const player_t& player = players[playerNum];
		const AActor* mo = player.mo;
		uint8_t flags = 0u;
		int health = player.health;
		DVector3 pos = {};
		DVector3 vel = {};
		uint32_t yaw = 0u;
		uint32_t pitch = 0u;
		if (mo != nullptr)
		{
			flags |= HCDEServerWorldDeltaPoseHasActor;
			if (player.playerstate == PST_LIVE)
				flags |= HCDEServerWorldDeltaPoseLive;
			if (player.onground)
				flags |= HCDEServerWorldDeltaPoseOnGround;
			health = mo->health;
			pos = mo->Pos();
			vel = mo->Vel;
			yaw = mo->Angles.Yaw.BAMs();
			pitch = mo->Angles.Pitch.BAMs();
		}

		if (!HCDEAppendByte(output, outputCapacity, cursor, playerNum)
			|| !HCDEAppendByte(output, outputCapacity, cursor, flags)
			|| !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(health, INT16_MIN, INT16_MAX)))
			|| !HCDEAppendFloat(output, outputCapacity, cursor, pos.X)
			|| !HCDEAppendFloat(output, outputCapacity, cursor, pos.Y)
			|| !HCDEAppendFloat(output, outputCapacity, cursor, pos.Z)
			|| !HCDEAppendFloat(output, outputCapacity, cursor, vel.X)
			|| !HCDEAppendFloat(output, outputCapacity, cursor, vel.Y)
			|| !HCDEAppendFloat(output, outputCapacity, cursor, vel.Z)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, yaw)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, pitch))
		{
			return false;
		}
	}
	++HCDELiveProfile.WorldDeltaPacketsBuilt;
	HCDELiveProfile.WorldDeltaRecordsBuilt += playerCount;
	HCDELiveProfile.WorldDeltaBytesBuilt += cursor - startCursor;
	HCDERecordLiveLaneTx(HLANE_PLAYER_SNAPSHOT, client, cursor - startCursor);
	HCDERecordPlayerSnapshotPressure(client, cursor - startCursor, playerCount);
	return true;
}

static const char* Net_InvasionActionStateName(uint8_t state)
{
	switch (state)
	{
	case HCDEInvasionActorActionNone:    return "none";
	case HCDEInvasionActorActionSpawn:   return "spawn";
	case HCDEInvasionActorActionSee:     return "see";
	case HCDEInvasionActorActionMelee:   return "melee";
	case HCDEInvasionActorActionMissile: return "missile";
	case HCDEInvasionActorActionPain:    return "pain";
	default:                             return "?";
	}
}

// HCDE: emit a "what was around me when I got hit?" snapshot to the trace
// stream every time the snapshot reports the local pawn's health dropped.
// We use this to diagnose reports like "monster I never saw clipped me for
// damage" - the dump lists every nearby monster the local playsim is aware
// of (whether it came from invasion mirror replication or from the local
// playsim simulating the original Doom monster) within
// `cl_debug_monster_proximity` units of the local pawn.
//
// Each invasion-mirror line includes class, position, distance, current
// action state, MF_CORPSE / MF_SOLID / MF_SHOOTABLE, and whether the
// visual-only mirror sanitizer ever armed for that actor. Each non-mirror
// line uses the same columns but omits the mirror-specific fields.
//
// In co-op (sv_gametype != 4) the invasion-mirror table is empty; the
// non-mirror walk is what surfaces "phantom damage" caused by simulation
// divergence between server and client - if the dump lists no live monsters
// within radius but the server confirmed damage, the local playsim is out
// of sync with the authority.
//
// Disabled when the cvar is 0; rate-limited to once per second otherwise so
// a stream of damage tics does not flood the log.
static void Net_DebugDumpMonstersAroundLocalPlayer(int newHealth, int previousHealth, const char* trigger)
{
	const int radiusUnits = clamp<int>(*cl_debug_monster_proximity, 0, 4096);
	if (radiusUnits <= 0)
		return;
	if (consoleplayer < 0 || consoleplayer >= MAXPLAYERS)
		return;
	AActor* localMo = players[consoleplayer].mo;
	if (localMo == nullptr)
		return;
	if (!HCDELiveReportIntervalElapsed(LastHCDEMonsterProximityDumpMS, 1000u))
		return;

	const DVector3 localPos = localMo->Pos();
	const double radius = double(radiusUnits);
	const double radiusSq = radius * radius;
	unsigned mirrorReported = 0u;
	unsigned actorReported = 0u;
	unsigned totalLiveActors = 0u;

	DebugTrace::Markf("net.desync",
		"monster proximity dump trigger=%s pos=(%.1f,%.1f,%.1f) health=%d->%d radius=%d mirrors=%u gametic=%d clienttic=%d",
		trigger != nullptr ? trigger : "?",
		localPos.X, localPos.Y, localPos.Z,
		previousHealth, newHealth, radiusUnits,
		unsigned(InvasionReplicatedActors.Size()), gametic, ClientTic);

	// Pass 1: invasion-mirror table. In invasion mode this is the only
	// authoritative-shadow source; in non-invasion modes it is empty and
	// we fall through to the playsim-actor walk below.
	for (auto& ref : InvasionReplicatedActors)
	{
		AActor* actor = ref.Actor;
		if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
			continue;

		const DVector3 actorPos = actor->Pos();
		const DVector3 delta = actorPos - localPos;
		const double distSq = delta.LengthSquared();
		if (distSq > radiusSq)
			continue;

		const double dist = sqrt(distSq);
		const char* className = actor->GetClass() != nullptr
			? actor->GetClass()->TypeName.GetChars() : "<unknown>";
		DebugTrace::Markf("net.desync",
			"  mirror id=%u class=%s pos=(%.1f,%.1f,%.1f) dist=%.1f health=%d projectile=%d action=%s flags=0x%x flags5=0x%x corpse=%d solid=%d shootable=%d visual-armed=%d",
			unsigned(ref.Id),
			className,
			actorPos.X, actorPos.Y, actorPos.Z,
			dist, actor->health,
			ref.IsProjectile ? 1 : 0,
			Net_InvasionActionStateName(ref.VisualActionState),
			actor->flags.GetValue(),
			actor->flags5.GetValue(),
			(actor->flags & MF_CORPSE) != 0 ? 1 : 0,
			(actor->flags & MF_SOLID) != 0 ? 1 : 0,
			(actor->flags & MF_SHOOTABLE) != 0 ? 1 : 0,
			ref.MirrorVisualArmed ? 1 : 0);
		++mirrorReported;
	}

	// Pass 2: every live monster/projectile actor the local playsim currently has.
	// This captures monsters that came from the local simulation rather
	// than from invasion mirror replication (the only path that exists in
	// co-op vs Doom monsters), so a mismatch between this list and what
	// the server thinks is alive points directly at simulation divergence.
	if (primaryLevel != nullptr)
	{
		auto iterator = primaryLevel->GetThinkerIterator<AActor>();
		while (AActor* actor = iterator.Next())
		{
			if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
				continue;
			// Skip the local pawn itself and other player pawns to keep the
			// dump focused on hostile/neutral threats.
			if (actor->player != nullptr)
				continue;
			// Only surface things that can plausibly hurt the player. Many Doom
			// decorations are shootable, so MF_SHOOTABLE alone produces pages of
			// lamps/torches/corpses; real threats are monsters or live missiles.
			const bool plausibleThreat = (actor->flags3 & MF3_ISMONSTER) != 0
				|| (actor->flags & MF_MISSILE) != 0;
			if (!plausibleThreat)
				continue;
			if ((actor->flags & MF_CORPSE) != 0)
				continue;
			if (actor->health <= 0)
				continue;

			++totalLiveActors;
			const DVector3 actorPos = actor->Pos();
			const DVector3 delta = actorPos - localPos;
			const double distSq = delta.LengthSquared();
			if (distSq > radiusSq)
				continue;

			const double dist = sqrt(distSq);
			const char* className = actor->GetClass() != nullptr
				? actor->GetClass()->TypeName.GetChars() : "<unknown>";
			const char* targetClassName = (actor->target != nullptr && actor->target->GetClass() != nullptr)
				? actor->target->GetClass()->TypeName.GetChars() : "<none>";
			// State label: identify which of the well-known animation
			// chains the monster is currently on by comparing actor->state
			// against the class's first-state pointers. Only Spawn, See,
			// Melee, and Missile are direct AActor fields; everything else
			// (Pain/Death/Raise) collapses to "Other" in this dump and the
			// flags column above already exposes MF_CORPSE for dead actors.
			const char* stateLabel = "<none>";
			if (actor->state != nullptr)
			{
				if (actor->state == actor->SpawnState)              stateLabel = "Spawn";
				else if (actor->state == actor->SeeState)           stateLabel = "See";
				else if (actor->state == actor->MeleeState)         stateLabel = "Melee";
				else if (actor->state == actor->MissileState)       stateLabel = "Missile";
				else                                                stateLabel = "Other";
			}
			DebugTrace::Markf("net.desync",
				"  actor class=%s pos=(%.1f,%.1f,%.1f) dist=%.1f health=%d flags=0x%x flags5=0x%x stat=%d state=%s target=%s",
				className,
				actorPos.X, actorPos.Y, actorPos.Z,
				dist, actor->health,
				actor->flags.GetValue(),
				actor->flags5.GetValue(),
				actor->GetStatNum(),
				stateLabel,
				targetClassName);
			++actorReported;
		}
	}

	if (mirrorReported == 0u && actorReported == 0u)
	{
		// No nearby live entity at all - this is the strongest possible
		// signal that the damage came from an authority-only source the
		// local playsim never simulated. Either an actor was destroyed on
		// our side before the snapshot caught up, or the local simulation
		// has diverged from the server's authoritative simulation and is
		// missing a monster the server still has alive nearby.
		DebugTrace::Markf("net.desync",
			"  (no live monsters within radius; total-mirrors=%u total-live-actors=%u)",
			unsigned(InvasionReplicatedActors.Size()), totalLiveActors);
	}
}

static void HCDEApplyLocalHealthFields(player_t& player, int serverHealth, bool onGround)
{
	AActor* mo = player.mo;
	if (mo == nullptr)
		return;

	const int previousHealth = max<int>(player.health, mo->health);
	mo->health = serverHealth;
	player.health = serverHealth;
	player.onground = onGround;
	if (serverHealth < previousHealth)
	{
		player.damagecount = clamp<int>(player.damagecount + previousHealth - serverHealth, 0, 100);
		// Optional diagnostic: when the user opts in via cl_debug_monster_proximity,
		// log every replicated invasion monster currently within range of the
		// local pawn so we can see whether the damage tic came from a mirror
		// that was already close (expected), a mirror in the wrong state
		// (e.g. still in spawn frames so it never drew its attack sprite),
		// or no mirror at all (a true authority-only damage source).
		Net_DebugDumpMonstersAroundLocalPlayer(serverHealth, previousHealth, "damage");
	}
	else if (serverHealth > previousHealth && player.damagecount > 0)
	{
		// Authoritative repair just raised our health above where the local
		// pawn was sitting - that is a real heal event (medkit, respawn, or a
		// prediction roll-back that wiped a bogus predicted hit). Clear the
		// stale red damage tint so it cannot persist with HUD health that has
		// already gone back up. We deliberately do NOT clear when serverHealth
		// equals previousHealth: that is the steady-state path where the
		// damagecount from a real recent hit should fade naturally inside
		// P_PlayerThink, and clobbering it would also incorrectly wipe the
		// tint after a megasphere player takes damage from 200 down to 150
		// and the server simply confirms the new (still-above-100) value.
		player.damagecount = 0;
	}
}

static void HCDEApplyLocalPoseRepair(player_t& player, const DVector3& serverPos, const DVector3& serverVel,
	uint32_t yawBam, uint32_t pitchBam, bool onGround, bool clearPrediction)
{
	AActor* mo = player.mo;
	if (mo == nullptr)
		return;

	// oldPos is only used to preserve viewz offset (i.e. crouch/zoom Z bias)
	// across the snap. We deliberately do NOT seed mo->Prev with it before
	// the ClearInterpolation() call below: ClearInterpolation unconditionally
	// resets Prev = Pos() and PrevPortalGroup = Sector->PortalGroup, so any
	// "interpolate from oldPos to newPos" attempt here would be silently
	// clobbered. The current behavior IS a snap (no smear across the map),
	// and that is what teleport / hard-drift reconciliation requires.
	const DVector3 oldPos = mo->Pos();
	mo->SetOrigin(serverPos, false);
	mo->Vel = serverVel;
	mo->SetAngle(DAngle::fromBam(yawBam), 0);
	mo->SetPitch(DAngle::fromBam(pitchBam), 0);
	player.onground = onGround;
	if (player.viewheight > 0.0)
		player.viewz = serverPos.Z + player.viewheight;
	else
		player.viewz = serverPos.Z + (player.viewz - oldPos.Z);
	mo->renderflags |= RF_NOINTERPOLATEVIEW;
	mo->ClearInterpolation();
	if (clearPrediction)
		P_ClearPredictionData();
}

static bool HCDELocalPlayerNeedsPoseRepair(const player_t& player, int serverHealth, double driftSq)
{
	if (player.mo == nullptr)
		return false;

	if (driftSq > HCDEServerReconcileHardDistance * HCDEServerReconcileHardDistance)
		return true;

	const int previousHealth = max<int>(player.health, player.mo->health);
	if (serverHealth >= previousHealth)
		return false;

	const int damage = previousHealth - serverHealth;
	if (driftSq <= HCDEServerReconcilePoseDamageDistance * HCDEServerReconcilePoseDamageDistance)
		return false;

	return damage >= HCDEServerReconcilePoseMinDamage;
}

static void HCDEQueuePredictedLocalHealthRepair(uint32_t serverTic, int serverHealth, bool onGround,
	const DVector3* serverPos = nullptr, const DVector3* serverVel = nullptr,
	uint32_t yawBam = 0u, uint32_t pitchBam = 0u, bool applyPose = false)
{
	if (PendingLocalHealthRepair.Valid && serverTic < PendingLocalHealthRepair.ServerTic)
		return;

	PendingLocalHealthRepair.Valid = true;
	PendingLocalHealthRepair.ServerTic = serverTic;
	PendingLocalHealthRepair.Health = serverHealth;
	PendingLocalHealthRepair.OnGround = onGround;
	PendingLocalHealthRepair.ApplyPose = applyPose;
	if (applyPose && serverPos != nullptr && serverVel != nullptr)
	{
		PendingLocalHealthRepair.Pos = *serverPos;
		PendingLocalHealthRepair.Vel = *serverVel;
		PendingLocalHealthRepair.Yaw = yawBam;
		PendingLocalHealthRepair.Pitch = pitchBam;
	}
	else
	{
		PendingLocalHealthRepair.ApplyPose = false;
	}

	if (consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
	{
		player_t& player = players[consoleplayer];
		if (applyPose && serverPos != nullptr && serverVel != nullptr && player.mo != nullptr)
		{
			const double driftSq = (player.mo->Pos() - *serverPos).LengthSquared();
			HCDEApplyLocalPoseRepair(player, *serverPos, *serverVel, yawBam, pitchBam, onGround,
				driftSq > HCDEServerReconcileHardDistance * HCDEServerReconcileHardDistance);
		}
		HCDEApplyLocalHealthFields(player, serverHealth, onGround);
	}
}

static void HCDEApplyPendingLocalHealthRepair()
{
	if (!PendingLocalHealthRepair.Valid)
		return;

	if (consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
	{
		player_t& player = players[consoleplayer];
		if (PendingLocalHealthRepair.ApplyPose)
		{
			const double driftSq = player.mo != nullptr
				? (player.mo->Pos() - PendingLocalHealthRepair.Pos).LengthSquared()
				: 0.0;
			HCDEApplyLocalPoseRepair(player,
				PendingLocalHealthRepair.Pos,
				PendingLocalHealthRepair.Vel,
				PendingLocalHealthRepair.Yaw,
				PendingLocalHealthRepair.Pitch,
				PendingLocalHealthRepair.OnGround,
				driftSq > HCDEServerReconcileHardDistance * HCDEServerReconcileHardDistance);
		}
		HCDEApplyLocalHealthFields(player,
			PendingLocalHealthRepair.Health,
			PendingLocalHealthRepair.OnGround);
		DebugTrace::Markf("net", "HCDE pending local health repair applied tic=%u health=%d pose=%d",
			PendingLocalHealthRepair.ServerTic,
			PendingLocalHealthRepair.Health,
			PendingLocalHealthRepair.ApplyPose ? 1 : 0);
	}

	PendingLocalHealthRepair.Valid = false;
}

static bool HCDEValidateServerWorldDeltas(int clientNum, const uint8_t* body, size_t bodyBytes, size_t& bodyCursor, uint8_t playerCount, uint64_t snapshotPlayers)
{
	if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < HCDEServerWorldDeltaHeaderSize)
		return false;
	if (memcmp(&body[bodyCursor + HCDEServerWorldDeltaMagicOffset], HCDEServerWorldDeltaMagic, sizeof(HCDEServerWorldDeltaMagic)) != 0)
		return false;

	size_t cursor = bodyCursor + HCDEServerWorldDeltaHeaderSize;
	const uint8_t version = body[bodyCursor + HCDEServerWorldDeltaVersionOffset];
	const uint8_t flags = body[bodyCursor + HCDEServerWorldDeltaFlagsOffset];
	const uint8_t deltaCount = body[bodyCursor + HCDEServerWorldDeltaCountOffset];
	if ((version != 1u && version != HCDEServerWorldDeltaProtocolVersion) || flags != 0u || playerCount > MAXPLAYERS || deltaCount > MAXPLAYERS)
		return false;
	const size_t deltaRecordSize = version >= 2u ? HCDEServerWorldDeltaRecordV2Size : HCDEServerWorldDeltaRecordV1Size;

	uint64_t deltaPlayers = 0u;
	uint32_t serverTic = 0u;
	size_t ticCursor = bodyCursor + HCDEServerWorldDeltaTicOffset;
	if (!HCDEReadBE32Field(body, bodyBytes, ticCursor, serverTic))
		return false;

	const bool canMutatePlaysim = HCDEWorldDeltasCanMutatePlaysim();
	for (uint8_t i = 0u; i < deltaCount; ++i)
	{
		if (cursor > bodyBytes || bodyBytes - cursor < deltaRecordSize)
			return false;

		uint8_t playerNum = 0u;
		uint8_t poseFlags = 0u;
		uint16_t healthBits = 0u;
		uint32_t yaw = 0u;
		uint32_t pitch = 0u;
		double values[6] = {};
		if (!HCDEReadByteField(body, bodyBytes, cursor, playerNum)
			|| !HCDEReadByteField(body, bodyBytes, cursor, poseFlags)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, healthBits))
		{
			return false;
		}
		for (double& value : values)
		{
			if (version >= 2u
				? !HCDEReadFloatField(body, bodyBytes, cursor, value)
				: !HCDEReadDoubleField(body, bodyBytes, cursor, value))
			{
				return false;
			}
		}
		if (!HCDEReadBE32Field(body, bodyBytes, cursor, yaw)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, pitch))
		{
			return false;
		}

		if (playerNum >= MAXPLAYERS || playerNum >= 64u || (poseFlags & ~(HCDEServerWorldDeltaPoseHasActor | HCDEServerWorldDeltaPoseLive | HCDEServerWorldDeltaPoseOnGround)) != 0u)
			return false;
		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if ((deltaPlayers & playerMask) != 0u)
			return false;
		deltaPlayers |= playerMask;

		if ((poseFlags & HCDEServerWorldDeltaPoseHasActor) == 0u)
			continue;

		player_t& player = players[playerNum];
		AActor* mo = player.mo;
		if (mo == nullptr)
			continue;

		auto& peer = HCDELivePeers[clientNum];
		++peer.WorldDeltaReceived;
		const DVector3 serverPos = { values[0], values[1], values[2] };
		const DVector3 serverVel = { values[3], values[4], values[5] };
		DVector3 delta = mo->Pos() - serverPos;
		if (mo->Level != nullptr && mo->Sector != nullptr)
		{
			sector_t* serverSector = mo->Level->PointInSector(serverPos);
			if (serverSector != nullptr)
				delta += mo->Level->Displacements.getOffset(mo->Sector->PortalGroup, serverSector->PortalGroup);
		}
		const double drift = delta.LengthSquared();
		const int serverHealth = int(int16_t(healthBits));
		if (playerNum == consoleplayer)
		{
			const bool serverReportsOnGround = (poseFlags & HCDEServerWorldDeltaPoseOnGround) != 0u;
			const bool serverReportsLive = (poseFlags & HCDEServerWorldDeltaPoseLive) != 0u && serverHealth > 0;
			const bool serverReportsDead = serverHealth <= 0;
			const bool localNeedsRespawnRepair = serverReportsLive
				&& (player.playerstate != PST_LIVE
					|| mo->health <= 0
					|| player.health <= 0
					|| (mo->flags & MF_CORPSE) != 0);
			const bool localNeedsDeathRepair = serverReportsDead
				&& player.playerstate == PST_LIVE
				&& (mo->health > 0 || player.health > 0)
				&& (mo->flags & MF_CORPSE) == 0;
			// HCDE: Teleporters, line portals, ACS Warp(), and other "instant"
			// server-side relocations move the pawn hundreds of units without
			// touching health, onground, or playerstate. If we only check the
			// health/onground deltas below we'd see "everything matches" and
			// `continue`, leaving the local pawn stranded at the pre-teleport
			// position while the authoritative pawn is already at the
			// destination. Every input the client sends from that point would
			// be against the wrong sector / wrong line-side, which is exactly
			// the desync users report after walking through a teleporter. The
			// drift threshold is shared with HCDELocalPlayerNeedsPoseRepair so
			// the "did the server move us against our will" decision stays in
			// one place; the soft-drift damage path keeps its tighter rule
			// (HCDEServerReconcilePoseDamageDistance) because that one is
			// designed to bias against snapping during ordinary movement.
			const bool localNeedsPoseRepair = drift > HCDEServerReconcileHardDistance * HCDEServerReconcileHardDistance;
			const bool serverHealthMatchesLocal = mo->health == serverHealth && player.health == serverHealth;
			const bool serverOnGroundMatchesLocal = player.onground == serverReportsOnGround;
			const bool needsLocalStateRepair = localNeedsRespawnRepair
				|| localNeedsDeathRepair
				|| localNeedsPoseRepair
				|| !serverHealthMatchesLocal
				|| !serverOnGroundMatchesLocal;
			if (!needsLocalStateRepair)
				continue;
			// "Drift-only" = the pose-drift threshold is the SOLE reason we're
			// in here (no respawn, no death, no health/onground delta). This
			// is the teleport / line-portal / ACS-Warp signature and it gets
			// its own trace so we can tell it apart from the much more common
			// health/onground-driven reconciliation in the post-mortem log.
			// Kept structurally aligned with `needsLocalStateRepair` above so
			// adding a future trigger to the OR-chain forces us to revisit
			// this label too instead of silently mis-tagging the new path.
			const bool driftOnlyRepair = localNeedsPoseRepair
				&& !localNeedsRespawnRepair
				&& !localNeedsDeathRepair
				&& serverHealthMatchesLocal
				&& serverOnGroundMatchesLocal;
			if (driftOnlyRepair)
			{
				DebugTrace::Markf("net",
					"HCDE client teleport reconcile player=%u drift=%.2f local=(%.1f,%.1f,%.1f) server=(%.1f,%.1f,%.1f) tic=%u",
					unsigned(playerNum), sqrt(drift),
					mo->X(), mo->Y(), mo->Z(),
					serverPos.X, serverPos.Y, serverPos.Z,
					unsigned(serverTic));
			}

			++peer.BaselineLocalDrift;
			if (!canMutatePlaysim)
			{
				DebugTrace::Markf("net", "HCDE client local state repair deferred from=%d player=%u drift=%.2f health=%d local-health=%d",
					clientNum, unsigned(playerNum), sqrt(drift), serverHealth, player.health);
				continue;
			}

			if (NetworkEntityManager::IsPredicting()
				&& !localNeedsRespawnRepair
				&& !localNeedsDeathRepair)
			{
				const bool applyPose = HCDELocalPlayerNeedsPoseRepair(player, serverHealth, drift);
				HCDEQueuePredictedLocalHealthRepair(serverTic, serverHealth, serverReportsOnGround,
					applyPose ? &serverPos : nullptr,
					applyPose ? &serverVel : nullptr,
					yaw, pitch, applyPose);
				++HCDELiveProfile.PredictionLocalHealthRepairs;
				++peer.Reconciliations;
				DebugTrace::Markf("net", "HCDE client local health repair queued from=%d player=%u drift=%.2f health=%d pose=%d reconciliations=%u",
					clientNum, unsigned(playerNum), sqrt(drift), serverHealth, applyPose ? 1 : 0, peer.Reconciliations);
				continue;
			}

			if (NetworkEntityManager::IsPredicting())
			{
				P_UnPredictClient();
				mo = player.mo;
				if (mo == nullptr)
					continue;
				PendingLocalHealthRepair.Valid = false;
			}

			if (localNeedsRespawnRepair)
			{
				// A death/respawn handoff is the one time the local client must
				// accept a full server-authored pawn state. Otherwise the client
				// can keep predicting from a stale corpse while the server has
				// already put the player back in the live round.
				//
				// Capture death-state evidence BEFORE we clear MF_CORPSE so we
				// can decide whether the weapon psprite needs to be rebuilt.
				// If we're only repairing PST_ENTER/PST_REBORN -> PST_LIVE (no
				// corpse flag, no zero-health) then the local pawn was simply
				// finishing its initial spawn handoff: its PSprites are fresh
				// from P_SpawnPlayer and we MUST NOT tear them down again.
				const bool wasCorpse = (mo->flags & MF_CORPSE) != 0;
				const bool wasDead = mo->health <= 0 || player.health <= 0;
				const bool weaponWasLowered = wasCorpse || wasDead;
				const double defaultViewHeight = player.DefaultViewHeight();
				const double viewZOffset = defaultViewHeight > 0.0 ? defaultViewHeight : player.viewz - mo->Z();
				const AActor* defaults = mo->GetDefault();
				mo->flags &= ~MF_CORPSE;
				if (defaults != nullptr)
					mo->flags |= defaults->flags & (MF_SOLID | MF_SHOOTABLE);
				mo->SetOrigin(serverPos, false);
				mo->Vel = serverVel;
				mo->SetAngle(DAngle::fromBam(yaw), 0);
				mo->SetPitch(DAngle::fromBam(pitch), 0);
				mo->health = serverHealth;
				player.health = serverHealth;
				SET_PLAYER_STATE(&player, playerNum, PST_LIVE, "HCDE_ValidateServerWorldDeltas_respawn_repair");
				player.camera = mo;
				player.damagecount = 0;
				player.bonuscount = 0;
				player.poisoncount = 0;
				player.fixedcolormap = NOFIXEDCOLORMAP;
				player.fixedlightlevel = -1;
				player.extralight = 0;
				player.BlendR = player.BlendG = player.BlendB = player.BlendA = 0.f;
				player.attacker = nullptr;
				player.viewheight = viewZOffset;
				player.onground = serverReportsOnGround;
				player.viewz = serverPos.Z + viewZOffset;
				// Respawn is always a hard snap to the spawn point - never
				// interpolate from the corpse position. ClearInterpolation
				// resets Prev = Pos and PrevPortalGroup = Sector->PortalGroup,
				// which is exactly what we want here (no smear from death
				// location to spawn). RF_NOINTERPOLATEVIEW kills the view
				// lerp on top of that.
				mo->renderflags |= RF_NOINTERPOLATEVIEW;
				mo->ClearInterpolation();
				// Mirror the server's PlayerReborn() handoff: wipe stale PSprites
				// (which may still be locked in the weapon's Lower/Death state
				// from a prior hard-death repair or a death tic the client
				// predicted locally) and bring the ReadyWeapon back up so the
				// gun doesn't stay glued to the bottom of the screen after
				// respawning. Only do this when there's actual evidence the
				// weapon was lowered (corpse flag or zero-health); during a
				// PST_ENTER/PST_REBORN -> PST_LIVE handoff the PSprites are
				// already valid from P_SpawnPlayer and rebuilding them races
				// the initial weapon-up animation.
				if (weaponWasLowered)
				{
					// player.ReadyWeapon may still be the weapon the user was
					// holding before they died. The server already ran
					// PlayerReborn() (which destructs+reconstructs player_t
					// and calls GiveDefaultInventory()), but our client-side
					// respawn repair reuses the existing pawn in place and
					// never reconstructs the local player struct - so the
					// ReadyWeapon pointer here can be one of:
					//   (a) a weapon AActor that has since been destroyed,
					//   (b) a weapon that is no longer in mo->Inventory after
					//       the server-side reborn shuffled inventory items,
					//   (c) genuinely the same weapon we had pre-death and
					//       still own.
					// In cases (a)/(b) P_SetupPsprites(false) silently brings
					// up a dangling pointer (or nothing) and the player wakes
					// up unable to fire - not even their fists - which is
					// exactly what the "respawned with no weapon" reports
					// describe. Validate that the pointer still lives in our
					// inventory before trusting it; otherwise null it out and
					// let PickNewWeapon scan the live inventory chain for the
					// best replacement, which mirrors what the engine does
					// when an ACS script TakeInventory's a weapon mid-game.
					AActor* readyWeap = player.ReadyWeapon;
					bool readyValid = readyWeap != nullptr
						&& (readyWeap->ObjectFlags & OF_EuthanizeMe) == 0;
					if (readyValid)
					{
						AActor* invItem = mo->Inventory;
						for (; invItem != nullptr; invItem = invItem->Inventory)
						{
							if (invItem == readyWeap)
								break;
						}
						readyValid = invItem != nullptr;
					}

					// Whichever branch we take below, the local player_t still
					// carries the WeaponState/refire/attackdown/usedown flags
					// from the tic we died on. A_WeaponReady normally clears
					// those naturally once the gun reaches its Ready frames,
					// but they can keep the *first* post-respawn fire press
					// from registering: WF_DISABLESWITCH locks the user out
					// of weapon switching, a non-zero refire makes the next
					// trigger pull look like a continuation of the dead pawn's
					// attack chain, and a stuck attackdown=true forces the
					// player to release+repress fire before A_WeaponReady will
					// arm a new shot. Resetting them here gives us the same
					// fresh-pawn state PlayerReborn() guarantees on the server.
					// We snapshot the pre-reset values first so the trace
					// below can show what the player_t actually looked like
					// at the moment we respawned, rather than the always-zero
					// reset values.
					const uint16_t preResetWeaponState = player.WeaponState;
					const short preResetRefire = player.refire;
					const bool preResetAttackdown = player.attackdown;
					const bool preResetUsedown = player.usedown;
					player.WeaponState = 0u;
					player.refire = 0;
					player.attackdown = false;
					player.usedown = false;

					if (readyValid)
					{
						P_SetupPsprites(&player, false);
					}
					else
					{
						// Tear down stale PSprites first - P_SetupPsprites does
						// this internally but we are taking the manual path so
						// we have to mirror that behavior - then ask the player
						// class for the best weapon still on the pawn. The
						// ZScript PickNewWeapon() sets PendingWeapon to its
						// pick and, because ReadyWeapon is null, calls
						// BringUpWeapon() itself, so we land with a valid
						// PSprite stack and a usable gun (or fists, if that's
						// all the inventory has left).
						player.ReadyWeapon = nullptr;
						player.PendingWeapon = (AActor*)WP_NOCHANGE;
						player.DestroyPSprites();
						IFVIRTUALPTRNAME(mo, NAME_PlayerPawn, PickNewWeapon)
						{
							CallVM<AActor*>(func, mo, (AActor*)nullptr);
						}
					}
					// Trace BOTH branches: a silent success leaves us blind to
					// "I respawned and the gun is up but it won't fire" reports
					// like the chainsaw screenshot. Logging the path taken plus
					// the pre-reset WeaponState/refire/attackdown/usedown values
					// (the *interesting* ones - the post-reset values are zero
					// by construction) and the resulting ReadyWeapon/PendingWeapon
					// stack lets us tell apart "we picked the wrong gun" from "we
					// picked the right gun but the psprite state machine is
					// stuck" the next time a user files one of these.
					const char* refreshPath = readyValid ? "p_setup-psprites" :
						(readyWeap == nullptr ? "fallback:null" :
							((readyWeap->ObjectFlags & OF_EuthanizeMe) != 0 ? "fallback:destroyed"
								: "fallback:not-in-inventory"));
					int inventoryCount = 0;
					for (AActor* invItem = mo->Inventory; invItem != nullptr && inventoryCount < 256; invItem = invItem->Inventory)
						++inventoryCount;
					DebugTrace::Markf("net",
						"HCDE client respawn weapon refresh player=%u path=%s pre-readyweap=%p new-readyweap=%p new-pendingweap=%p inv-count=%d pre-weaponstate=0x%04x pre-refire=%d pre-attackdown=%d pre-usedown=%d",
						unsigned(playerNum), refreshPath,
						(void*)readyWeap, (void*)player.ReadyWeapon, (void*)player.PendingWeapon,
						inventoryCount, unsigned(preResetWeaponState), int(preResetRefire),
						preResetAttackdown ? 1 : 0, preResetUsedown ? 1 : 0);
				}
				P_ClearPredictionData();
				PendingLocalHealthRepair.Valid = false;
				++peer.HardReconciliations;
				++peer.Reconciliations;
				++HCDELiveProfile.PredictionHardRespawnRepairs;
				DebugTrace::Markf("net", "HCDE client respawn repair from=%d player=%u drift=%.2f health=%d reconciliations=%u hard=%u",
					clientNum, unsigned(playerNum), sqrt(drift), serverHealth, peer.Reconciliations, peer.HardReconciliations);
				continue;
			}

			if (localNeedsDeathRepair)
			{
				const int previousHealth = max<int>(player.health, mo->health);
				HCDEApplyLocalPoseRepair(player, serverPos, serverVel, yaw, pitch, serverReportsOnGround, true);
				mo = player.mo;
				if (mo == nullptr)
					continue;
				// Capture the predicted mo->health value *before* we overwrite
				// it with the server-clamped value. Some third-party PlayerPawn
				// Die() overrides use the raw health magnitude to decide gib
				// vs normal death (e.g. `if (health < -GetSpawnHealth()) gib`)
				// and the previous code clobbered that signal whenever the
				// server snapshot only reported `serverHealth = 0` for a kill
				// the client had locally predicted at, say, -120. This trace
				// lets us see in real captures whether the pre-write health is
				// ever stronger (more negative) than serverHealth - if so, we
				// know the gib branch is being lost in the wild and we need
				// to preserve the predicted magnitude here. Until we see it
				// happen we deliberately keep the original ordering to avoid
				// regressing the normal-death path.
				const int preWriteMoHealth = mo->health;
				const int preWritePlayerHealth = player.health;
				mo->health = min<int>(serverHealth, 0);
				player.health = mo->health;
				player.onground = serverReportsOnGround;
				if (previousHealth > serverHealth)
					player.damagecount = clamp<int>(player.damagecount + previousHealth - serverHealth, 0, 100);
				if (preWriteMoHealth < serverHealth)
				{
					// Predicted state was stronger overkill than what the
					// server now reports - mo->CallDie() will see the weaker
					// authoritative value, not the predicted gib magnitude.
					DebugTrace::Warningf("net",
						"HCDE client death repair lost overkill predicted-mo-health=%d predicted-player-health=%d server-health=%d player=%u tic=%u",
						preWriteMoHealth, preWritePlayerHealth, serverHealth, unsigned(playerNum), unsigned(serverTic));
				}
				mo->CallDie(nullptr, nullptr, DMG_FORCED, NAME_None);
				P_ClearPredictionData();
				PendingLocalHealthRepair.Valid = false;
				++peer.HardReconciliations;
				++peer.Reconciliations;
				++HCDELiveProfile.PredictionHardDeathRepairs;
				DebugTrace::Markf("net", "HCDE client death repair from=%d player=%u drift=%.2f health=%d reconciliations=%u hard=%u",
					clientNum, unsigned(playerNum), sqrt(drift), serverHealth, peer.Reconciliations, peer.HardReconciliations);
				// Optional diagnostic for "what just killed me?" investigations -
				// we dump the local invasion-mirror neighborhood here for the
				// same reasons the regular damage path does (see comment in
				// HCDEApplyLocalHealthFields). The rate-limit inside the dump
				// helper keeps a fast damage->death->respawn cluster from
				// emitting two dumps in the same second.
				Net_DebugDumpMonstersAroundLocalPlayer(serverHealth, previousHealth, "death");
				continue;
			}

			// Local clients own prediction for their own pawn. Do not apply
			// server-authored position during normal movement; doing so can pin
			// movement if the dedicated input route lags. When the server reports
			// damage with meaningful drift, snap to the authoritative pose so melee
			// and other hitscan/contact deaths line up with what the player sees.
			const bool applyPose = HCDELocalPlayerNeedsPoseRepair(player, serverHealth, drift);
			if (applyPose)
			{
				HCDEApplyLocalPoseRepair(player, serverPos, serverVel, yaw, pitch, serverReportsOnGround,
					drift > HCDEServerReconcileHardDistance * HCDEServerReconcileHardDistance);
			}
			HCDEApplyLocalHealthFields(player, serverHealth, serverReportsOnGround);
			PendingLocalHealthRepair.Valid = false;
			++peer.Reconciliations;
			++HCDELiveProfile.PredictionLocalStateRepairs;
			DebugTrace::Markf("net", "HCDE client local state repair from=%d player=%u drift=%.2f health=%d pose=%d reconciliations=%u",
				clientNum, unsigned(playerNum), sqrt(drift), serverHealth, applyPose ? 1 : 0, peer.Reconciliations);
			continue;
		}

		const bool needsPoseRepair = drift > HCDEServerBaselineRepairDistance * HCDEServerBaselineRepairDistance;
		const bool needsStateRepair = mo->health != serverHealth || player.health != serverHealth || player.onground != ((poseFlags & HCDEServerWorldDeltaPoseOnGround) != 0u);
		if (needsPoseRepair || needsStateRepair)
		{
			if (!canMutatePlaysim)
			{
				DebugTrace::Markf("net", "HCDE baseline repair deferred client=%d player=%u drift=%.2f health=%d local-health=%d",
					clientNum, unsigned(playerNum), sqrt(drift), serverHealth, player.health);
				continue;
			}

			mo->SetOrigin(serverPos, false);
			mo->Vel = serverVel;
			mo->SetAngle(DAngle::fromBam(yaw), 0);
			mo->SetPitch(DAngle::fromBam(pitch), 0);
			mo->health = serverHealth;
			player.health = serverHealth;
			player.onground = (poseFlags & HCDEServerWorldDeltaPoseOnGround) != 0u;
			mo->ClearInterpolation();
			++peer.BaselineRepairs;
			++HCDELiveProfile.RemotePlayerBaselineRepairs;
			DebugTrace::Markf("net", "HCDE baseline repair client=%d player=%u drift=%.2f health=%d repairs=%u",
				clientNum, unsigned(playerNum), sqrt(drift), serverHealth, peer.BaselineRepairs);
		}
	}

	if ((deltaPlayers & snapshotPlayers) != snapshotPlayers)
	{
		++HCDELiveProfile.PlayerSnapshotMissingRecords;
		return false;
	}

	bodyCursor = cursor;
	++HCDELiveProfile.WorldDeltaPacketsReceived;
	HCDELiveProfile.WorldDeltaRecordsReceived += deltaCount;
	HCDELiveProfile.WorldDeltaBytesReceived += size_t(deltaCount) * deltaRecordSize + HCDEServerWorldDeltaHeaderSize;
	HCDERecordLiveLaneRx(HLANE_PLAYER_SNAPSHOT, clientNum, size_t(deltaCount) * deltaRecordSize + HCDEServerWorldDeltaHeaderSize);
	DebugTrace::Markf("net", "HCDE server world delta recv tic=%u players=%u bytes=%zu",
		serverTic, unsigned(deltaCount), size_t(deltaCount) * deltaRecordSize + HCDEServerWorldDeltaHeaderSize);
	return true;
}

static void HCDEPushRecentAuthorityEvent(const FHCDEAuthorityEvent& event)
{
	HCDERecentAuthorityEvents.Push(event);
	while (HCDERecentAuthorityEvents.Size() > HCDEAuthorityEventHistoryLimit)
	{
		HCDERecentAuthorityEvents.Delete(0);
	}
}

static void Net_RecordInvasionSpawnEvent(AActor* spawned)
{
	if (!I_IsLocalHCDEServiceAuthority() || spawned == nullptr)
		return;

	const char* className = spawned->GetClass()->TypeName.GetChars();
	if (className == nullptr || className[0] == '\0')
		return;

	FHCDEAuthorityEvent event;
	event.EventType = HCDEAuthorityEventSpawn;
	event.Source = HREP_SOURCE_INVASION;
	event.Category = Net_ClassifyHCDEReplicatedActor(spawned, Net_IsInvasionReplicatedProjectile(spawned));
	event.ActorFlags = HCDEActorDeltaFlagLive;
	event.ClassId = Net_GetHCDEReplicatedActorClassId(spawned->GetClass());
	event.EventSeq = InvasionNextAuthorityEventSeq++;
	if (InvasionNextAuthorityEventSeq == 0u)
		InvasionNextAuthorityEventSeq = 1u;
	event.Id = InvasionNextSpawnEventId++;
	if (InvasionNextSpawnEventId == 0u)
		InvasionNextSpawnEventId = 1u;
	event.Tic = gametic;
	event.Wave = InvasionWaveDirector.Wave;
	event.ClassName = className;
	event.Pos = spawned->Pos();
	event.Yaw = spawned->Angles.Yaw;
	event.Health = spawned->health;
	HCDEPushRecentAuthorityEvent(event);
	Net_RegisterInvasionReplicatedActor(event.Id, spawned);

	DebugTrace::Markf("invasion", "replicate spawn id=%u wave=%d class=%s pos=(%.1f,%.1f,%.1f) health=%d",
		unsigned(event.Id),
		event.Wave,
		event.ClassName.GetChars(),
		event.Pos.X,
		event.Pos.Y,
		event.Pos.Z,
		event.Health);
}

static void Net_RecordInvasionDespawnEvent(const FInvasionReplicatedActorRef& ref, AActor* actor, int serverHealth)
{
	if (!I_IsLocalHCDEServiceAuthority()
		|| ref.Id == 0u
		|| !Net_IsInvasionModeEnabled())
	{
		return;
	}

	FHCDEAuthorityEvent event;
	event.EventType = HCDEAuthorityEventDespawn;
	event.Source = HREP_SOURCE_INVASION;
	event.Category = ref.IsProjectile ? HREP_ACTOR_PROJECTILE : HREP_ACTOR_MONSTER;
	event.ActorFlags = 0u;
	event.ClassId = actor != nullptr ? Net_GetHCDEReplicatedActorClassId(actor->GetClass()) : 0u;
	event.EventSeq = InvasionNextAuthorityEventSeq++;
	if (InvasionNextAuthorityEventSeq == 0u)
		InvasionNextAuthorityEventSeq = 1u;
	event.Id = ref.Id;
	event.Tic = gametic;
	event.Wave = InvasionWaveDirector.Wave;
	if (actor != nullptr && actor->GetClass() != nullptr)
		event.ClassName = actor->GetClass()->TypeName.GetChars();
	event.Pos = actor != nullptr ? actor->Pos() : ref.VisualTargetPos;
	event.Yaw = actor != nullptr ? actor->Angles.Yaw : ref.VisualTargetYaw;
	event.Health = serverHealth;
	HCDEPushRecentAuthorityEvent(event);

	DebugTrace::Markf("invasion", "replicate despawn id=%u seq=%u wave=%d class=%s pos=(%.1f,%.1f,%.1f) health=%d projectile=%d",
		unsigned(event.Id),
		unsigned(event.EventSeq),
		event.Wave,
		event.ClassName.IsNotEmpty() ? event.ClassName.GetChars() : "<unknown>",
		event.Pos.X,
		event.Pos.Y,
		event.Pos.Z,
		event.Health,
		ref.IsProjectile ? 1 : 0);
}

static bool Net_ShouldRecordInvasionDamageEvent(const FInvasionReplicatedActorRef& ref, int serverHealth)
{
	if (ref.LastAuthorityHealthEventTic <= 0)
		return true;

	const int ticsSinceLastHealthFact = gametic - ref.LastAuthorityHealthEventTic;
	const int healthDeltaSinceLastFact = serverHealth >= ref.LastAuthorityEventHealth
		? serverHealth - ref.LastAuthorityEventHealth
		: ref.LastAuthorityEventHealth - serverHealth;
	return ticsSinceLastHealthFact >= max<int>(HCDEAuthorityDamageMinIntervalTics, 1)
		|| healthDeltaSinceLastFact >= HCDEAuthorityDamageImmediateDelta;
}

static void Net_RecordInvasionDamageEvent(FInvasionReplicatedActorRef& ref, AActor* actor, int previousHealth, int serverHealth)
{
	if (!I_IsLocalHCDEServiceAuthority()
		|| ref.Id == 0u
		|| actor == nullptr
		|| ref.IsProjectile
		|| serverHealth <= 0
		|| previousHealth == serverHealth
		|| !Net_IsInvasionModeEnabled())
	{
		return;
	}
	if (!Net_ShouldRecordInvasionDamageEvent(ref, serverHealth))
		return;

	FHCDEAuthorityEvent event;
	event.EventType = HCDEAuthorityEventDamage;
	event.Source = HREP_SOURCE_INVASION;
	event.Category = HREP_ACTOR_MONSTER;
	event.ActorFlags = HCDEActorDeltaFlagLive;
	event.ClassId = actor->GetClass() != nullptr ? Net_GetHCDEReplicatedActorClassId(actor->GetClass()) : 0u;
	event.EventSeq = InvasionNextAuthorityEventSeq++;
	if (InvasionNextAuthorityEventSeq == 0u)
		InvasionNextAuthorityEventSeq = 1u;
	event.Id = ref.Id;
	event.Tic = gametic;
	event.Wave = InvasionWaveDirector.Wave;
	if (actor->GetClass() != nullptr)
		event.ClassName = actor->GetClass()->TypeName.GetChars();
	event.Pos = actor->Pos();
	event.Yaw = actor->Angles.Yaw;
	event.Health = serverHealth;
	HCDEPushRecentAuthorityEvent(event);

	if (serverHealth < previousHealth)
	{
		ref.ServerForcedActionState = HCDEInvasionActorActionPain;
		ref.ServerForcedActionTic = gametic;
	}
	ref.LastAuthorityEventHealth = serverHealth;
	ref.LastAuthorityHealthEventTic = gametic;

	DebugTrace::Markf("invasion", "replicate damage id=%u seq=%u wave=%d class=%s health=%d previous=%d pos=(%.1f,%.1f,%.1f)",
		unsigned(event.Id),
		unsigned(event.EventSeq),
		event.Wave,
		event.ClassName.IsNotEmpty() ? event.ClassName.GetChars() : "<unknown>",
		event.Health,
		previousHealth,
		event.Pos.X,
		event.Pos.Y,
		event.Pos.Z);
}

static bool Net_HasPendingInvasionSpawnEvent(uint32_t id)
{
	for (const auto& pending : InvasionPendingSpawnEvents)
	{
		if (pending.Id == id)
			return true;
	}
	return false;
}

static void Net_QueueInvasionSpawnEvent(const FHCDEAuthorityEvent& event)
{
	if (event.Id == 0u
		|| event.Id <= InvasionLastAppliedSpawnEventId
		|| Net_HasPendingInvasionSpawnEvent(event.Id))
	{
		return;
	}

	InvasionPendingSpawnEvents.Push(event);
	while (InvasionPendingSpawnEvents.Size() > HCDEInvasionSpawnEventHistoryLimit)
	{
		InvasionPendingSpawnEvents.Delete(0);
	}

	DebugTrace::Markf("invasion", "mirror spawn queued id=%u wave=%d class=%s reason=prediction-active",
		unsigned(event.Id), event.Wave, event.ClassName.GetChars());
}

static bool Net_HasPendingInvasionMirrorSpawn(uint32_t id)
{
	for (const auto& pending : InvasionPendingMirrorSpawns)
	{
		if (pending.Id == id)
			return true;
	}
	return false;
}

static void Net_QueueInvasionMirrorSpawn(uint32_t id, int wave, const FString& className,
	const DVector3& pos, const DVector3& vel, DAngle yaw, DAngle pitch, int health,
	bool markApplied)
{
	if (id == 0u
		|| className.IsEmpty()
		|| Net_HasPendingInvasionMirrorSpawn(id))
	{
		return;
	}
	if (auto existing = Net_FindInvasionReplicatedActor(id); existing != nullptr && existing->Actor != nullptr)
		return;

	FInvasionPendingMirrorSpawn pending;
	pending.Id = id;
	pending.Wave = wave;
	pending.ClassName = className;
	pending.Pos = pos;
	pending.Vel = vel;
	pending.Yaw = yaw;
	pending.Pitch = pitch;
	pending.Health = health;
	pending.MarkApplied = markApplied;
	pending.QueuedTic = gametic;
	InvasionPendingMirrorSpawns.Push(pending);
	while (InvasionPendingMirrorSpawns.Size() > HCDEInvasionSpawnEventHistoryLimit)
	{
		InvasionPendingMirrorSpawns.Delete(0);
	}

	DebugTrace::Markf("invasion", "mirror delta spawn queued id=%u wave=%d class=%s mark=%d reason=prediction-active",
		unsigned(id), wave, className.GetChars(), markApplied ? 1 : 0);
}

static void Net_SetInvasionMirrorVisualOnly(uint32_t id, AActor* actor)
{
	if (I_IsLocalHCDEServiceAuthority()
		|| actor == nullptr
		|| (actor->ObjectFlags & OF_EuthanizeMe) != 0)
	{
		return;
	}

	auto* ref = Net_FindInvasionReplicatedActor(id);
	if (ref != nullptr && ref->MirrorVisualArmed)
		return;

	const bool wasThinking = actor->GetStatNum() >= STAT_FIRST_THINKING;
	const bool projectileMirror = Net_IsInvasionReplicatedProjectile(actor);

	// Monster and projectile mirrors share the same "visual-only" sanitization
	// now that mirrors never block the local player. The only mirror-specific
	// extra is RF_NOSPRITESHADOW on projectile mirrors so their interpolated
	// position does not cast a separate shadow from the authoritative actor.
	const bool needsWorldRelink = (actor->flags & MF_NOBLOCKMAP) == 0
		|| (actor->flags & (MF_SOLID | MF_SHOOTABLE)) != 0;
	if (needsWorldRelink)
	{
		FLinkContext ctx;
		actor->UnlinkFromWorld(&ctx);
		actor->flags |= MF_NOBLOCKMAP;
		actor->flags &= ~(MF_SOLID | MF_SHOOTABLE);
		actor->LinkToWorld(&ctx);
	}
	else
	{
		actor->flags &= ~(MF_SOLID | MF_SHOOTABLE);
	}
	if (projectileMirror)
		actor->renderflags |= RF_NOSPRITESHADOW;

	actor->flags |= MF_NOCLIP;
	actor->flags4 |= MF4_STANDSTILL;
	actor->flags5 |= MF5_NOINTERACTION | MF5_NOINFIGHTING;
	actor->flags7 &= ~MF7_INCHASE;
	actor->target = nullptr;
	actor->lastenemy = nullptr;
	actor->goal = nullptr;

	if (!projectileMirror)
		actor->Vel = DVector3(0, 0, 0);

	if (!projectileMirror && actor->state == actor->SpawnState && actor->SeeState != nullptr)
		actor->SetState(actor->SeeState, true);

	if (wasThinking)
		actor->ChangeStatNum(STAT_INFO);

	if (wasThinking || needsWorldRelink)
	{
		// Mirrors are always visual-only blockmap-detached here, so we no
		// longer log a "blockmap" column - decode flags / MF_NOBLOCKMAP from
		// the flags field instead if collision is ever in question.
		DebugTrace::Markf("invasion", "mirror client replica armed id=%u class=%s stat=%d projectile=%d flags=0x%x flags5=0x%x pos=(%.1f,%.1f,%.1f)",
			unsigned(id),
			actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
			actor->GetStatNum(),
			projectileMirror ? 1 : 0,
			actor->flags.GetValue(),
			actor->flags5.GetValue(),
			actor->X(),
			actor->Y(),
			actor->Z());
	}

	if (ref != nullptr)
		ref->MirrorVisualArmed = true;
}

static void Net_SeedInvasionMirrorVisualTarget(FInvasionReplicatedActorRef& ref, AActor* actor)
{
	if (actor == nullptr)
		return;

	if (Net_IsInvasionReplicatedProjectile(actor))
		ref.IsProjectile = true;
	ref.HasVisualTarget = true;
	ref.VisualTargetPos = actor->Pos();
	ref.VisualTargetVel = actor->Vel;
	ref.VisualTargetYaw = actor->Angles.Yaw;
	ref.VisualTargetPitch = actor->Angles.Pitch;
	ref.VisualTargetHealth = actor->health;
	ref.VisualTargetTic = gametic;
}

static void Net_SetInvasionMirrorVisualTarget(FInvasionReplicatedActorRef& ref, const DVector3& pos,
	const DVector3& vel, DAngle yaw, DAngle pitch, int health)
{
	const int previousTic = ref.VisualTargetTic;
	const int jump = (previousTic > 0) ? (gametic - previousTic) : 0;
	if (jump > 2)
	{
		AActor* actor = ref.Actor.Get();
		const char* clsName = (actor != nullptr) ? actor->GetClass()->TypeName.GetChars() : "Unknown";
		DebugTrace::Warningf("net.desync", "[MIRROR JUMP] VisualTargetTic jumped by %d tics (prev=%d, current=%d) for id=%u (%s)",
			jump, previousTic, gametic, unsigned(ref.Id), clsName);
	}

	ref.HasVisualTarget = true;
	ref.VisualTargetPos = pos;
	ref.VisualTargetVel = vel;
	ref.VisualTargetYaw = yaw;
	ref.VisualTargetPitch = pitch;
	ref.VisualTargetHealth = health;
	ref.VisualTargetTic = gametic;
}

static double Net_GetInvasionMirrorVisualStepCap(const AActor* actor)
{
	double baseSpeed = HCDEInvasionMirrorVisualFallbackStepPerTic;
	if (actor != nullptr && actor->Speed > 0.0 && actor->Speed < 128.0)
		baseSpeed = actor->Speed;
	return clamp<double>(baseSpeed * HCDEInvasionMirrorVisualSpeedMultiplier, 2.0, HCDEInvasionMirrorVisualMaxStepPerTic);
}

static void Net_TickInvasionMirrorVisualActors(unsigned& updated, unsigned& skipped)
{
	updated = 0u;
	skipped = 0u;
	if (Net_IsLocalInvasionAuthority() || InvasionReplicatedActors.Size() == 0)
		return;

	AActor* camera = nullptr;
	if (consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
	{
		camera = players[consoleplayer].camera;
		if (camera == nullptr)
			camera = players[consoleplayer].mo;
	}
	const DVector3 cameraPos = camera != nullptr ? camera->Pos() : DVector3(0, 0, 0);
	const double farUpdateDistanceSq = 2048.0 * 2048.0;

	for (auto& ref : InvasionReplicatedActors)
	{
		AActor* actor = ref.Actor;
		if (actor == nullptr
			|| (actor->ObjectFlags & OF_EuthanizeMe) != 0
			|| Net_IsInvasionActorCorpseLike(actor))
		{
			continue;
		}

		const bool farFromCamera = camera != nullptr
			&& !ref.IsProjectile
			&& (actor->Pos() - cameraPos).LengthSquared() > farUpdateDistanceSq;
		if (farFromCamera)
		{
			++skipped;
			continue;
		}

		if (ref.HasVisualTarget)
		{
			actor->health = ref.VisualTargetHealth;
			actor->Angles.Yaw = ref.VisualTargetYaw;
			actor->Angles.Pitch = ref.VisualTargetPitch;

			const DVector3 oldPos = actor->Pos();
			const DVector3 delta = ref.VisualTargetPos - oldPos;
			const double distSq = delta.LengthSquared();
			const double snapDistanceSq = HCDEInvasionMirrorVisualSnapDistance * HCDEInvasionMirrorVisualSnapDistance;
			const bool combatVisual = ref.VisualActionState == HCDEInvasionActorActionMelee
				|| ref.VisualActionState == HCDEInvasionActorActionMissile;
			if (distSq > snapDistanceSq || combatVisual)
			{
				actor->SetOrigin(ref.VisualTargetPos, false);
				actor->Prev = ref.VisualTargetPos;
				actor->PrevPortalGroup = actor->Sector != nullptr ? actor->Sector->PortalGroup : actor->PrevPortalGroup;
				actor->ClearInterpolation();
			}
			else if (ref.IsProjectile)
			{
				const DVector3 oldRenderPos = actor->Pos();
				const int oldPortalGroup = actor->Sector != nullptr ? actor->Sector->PortalGroup : actor->PrevPortalGroup;
				// Projectile mirrors are server-pose driven. Some mods use zero-tic
				// seeker states (for example A_SeekerMissile(9999,9999)) that can turn
				// a missile immediately after spawn. Extrapolating locally from Speed
				// and the last yaw can make those projectiles visually miss walls or
				// appear to move too fast. Trust the latest replicated pose instead.
				actor->SetOrigin(ref.VisualTargetPos, false);
				actor->Prev = oldRenderPos;
				actor->PrevPortalGroup = oldPortalGroup;
				actor->Vel = ref.VisualTargetVel;
			}
			else if (distSq > 0.01)
			{
				const DVector3 oldRenderPos = actor->Pos();
				const int oldPortalGroup = actor->Sector != nullptr ? actor->Sector->PortalGroup : actor->PrevPortalGroup;
				const double dist = sqrt(distSq);
				const double step = min(dist, Net_GetInvasionMirrorVisualStepCap(actor));
				const DVector3 nextPos = oldRenderPos + delta * (step / dist);
				actor->SetOrigin(nextPos, false);
				actor->Prev = oldRenderPos;
				actor->PrevPortalGroup = oldPortalGroup;
				actor->Vel = DVector3(0, 0, 0);
			}
			else
			{
				actor->Vel = DVector3(0, 0, 0);
			}
		}

		// Projectile mirrors are pose-driven above; ticking their state machine every
		// frame just burns CPU and can fight the extrapolated origin.
		if (ref.IsProjectile)
		{
			++updated;
			continue;
		}

		if (actor->state == nullptr
			|| actor->tics == -1
			|| (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		{
			++updated;
			continue;
		}

		// If the mirror is in an attack/pain animation but the server has
		// stopped refreshing its visual target, do not advance the state
		// machine. Otherwise a missed actor-delta or despawn event leaves
		// the mirror endlessly firing at nothing while the server has
		// already moved on. Freezing the current frame is much less
		// distracting than a looped attack pose; the next server update
		// will reset the actor cleanly.
		const bool attackingMirror = ref.VisualActionState == HCDEInvasionActorActionMelee
			|| ref.VisualActionState == HCDEInvasionActorActionMissile
			|| ref.VisualActionState == HCDEInvasionActorActionPain;
		const int visualTargetAgeTics = gametic - ref.VisualTargetTic;
		const bool visualTargetStale = ref.VisualTargetTic > 0
			&& visualTargetAgeTics > TICRATE; // 1s without an update
		if (attackingMirror && visualTargetStale)
		{
			++updated;
			continue;
		}

		if (actor->tics > 0)
			--actor->tics;
		if (actor->tics <= 0)
			actor->SetState(actor->state->GetNextState(), true);
		++updated;
	}

}

static bool Net_SpawnInvasionMirrorActor(uint32_t id, int wave, const FString& className,
	const DVector3& pos, const DVector3& vel, DAngle yaw, DAngle pitch, int health, const char* source, bool markApplied,
	uint8_t authorityCategoryHint = HREP_ACTOR_UNKNOWN)
{
	if (Net_IsLocalInvasionAuthority())
		return true;
	if (id == 0u || className.IsEmpty())
		return false;
	if (primaryLevel == nullptr || gamestate != GS_LEVEL || NetworkEntityManager::IsPredicting())
		return false;

	if (auto existing = Net_FindInvasionReplicatedActor(id); existing != nullptr && existing->Actor != nullptr)
	{
		AActor* eact = existing->Actor.Get();
		if (authorityCategoryHint == HREP_ACTOR_PROJECTILE)
			existing->IsProjectile = true;
		else if (eact != nullptr && Net_ClassDefaultsSuggestProjectile(eact->GetClass()))
			existing->IsProjectile = true;
		if (eact != nullptr && Net_IsInvasionReplicatedProjectile(eact))
			existing->IsProjectile = true;

		DVector3 useVel = existing->IsProjectile ? vel : DVector3(0, 0, 0);
		if (existing->IsProjectile && useVel.LengthSquared() < 1.0 && eact != nullptr)
		{
			useVel = eact->Vel;
			if (useVel.LengthSquared() < 1.0 && eact->Speed > 0.0)
			{
				useVel.X = eact->Speed * yaw.Cos();
				useVel.Y = eact->Speed * yaw.Sin();
				useVel.Z = eact->Speed * pitch.Sin();
			}
		}

		Net_SetInvasionMirrorVisualTarget(*existing, pos, useVel, yaw, pitch, health);
		if (markApplied && id > InvasionLastAppliedSpawnEventId)
			InvasionLastAppliedSpawnEventId = id;
		return true;
	}

	PClassActor* cls = PClass::FindActor(className.GetChars());
	if (cls == nullptr)
	{
		if (markApplied && id > InvasionLastAppliedSpawnEventId)
			InvasionLastAppliedSpawnEventId = id;
		DebugTrace::Markf("invasion", "mirror spawn skipped id=%u wave=%d class=%s source=%s reason=missing-class",
			unsigned(id), wave, className.GetChars(), source != nullptr ? source : "unknown");
		return true;
	}

	AActor* actor = Spawn(primaryLevel, cls, pos, ALLOW_REPLACE);
	if (actor == nullptr)
	{
		if (markApplied && id > InvasionLastAppliedSpawnEventId)
			InvasionLastAppliedSpawnEventId = id;
		DebugTrace::Markf("invasion", "mirror spawn skipped id=%u wave=%d class=%s source=%s reason=spawn-null",
			unsigned(id), wave, className.GetChars(), source != nullptr ? source : "unknown");
		return true;
	}

	actor->Angles.Yaw = yaw;
	actor->Angles.Pitch = pitch;
	if (health > 0)
		actor->health = health;
	Net_ApplyInvasionMonsterSkillTuning(actor);
	actor->ClearInterpolation();
	const DVector3 spawnVel = actor->Vel;
	Net_SetInvasionMirrorVisualOnly(id, actor);
	Net_RegisterInvasionReplicatedActor(id, actor);
	if (auto ref = Net_FindInvasionReplicatedActor(id); ref != nullptr)
	{
		if (authorityCategoryHint == HREP_ACTOR_PROJECTILE
			|| Net_ClassDefaultsSuggestProjectile(cls))
		{
			ref->IsProjectile = true;
		}
		if (Net_IsInvasionReplicatedProjectile(actor))
			ref->IsProjectile = true;

		if (!ref->IsProjectile)
		{
			InvasionWaveDirector.ActiveMonsters.Push(MakeObjPtr<AActor*>(actor));
			Net_SpawnInvasionTeleportFog(actor);
		}
		DVector3 visualVel = ref->IsProjectile ? vel : DVector3(0, 0, 0);
		if (ref->IsProjectile && visualVel.LengthSquared() < 1.0)
			visualVel = spawnVel;
		if (ref->IsProjectile && visualVel.LengthSquared() < 1.0 && actor->Speed > 0.0)
		{
			// Client-side projectile mirrors spawn without the server's launch
			// velocity. Seed motion from the replicated facing until the first
			// actor delta arrives so imp fireballs are visible in flight.
			visualVel.X = actor->Speed * yaw.Cos();
			visualVel.Y = actor->Speed * yaw.Sin();
			visualVel.Z = actor->Speed * pitch.Sin();
		}
		Net_SetInvasionMirrorVisualTarget(*ref, pos, visualVel, yaw, pitch, actor->health);
	}
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
			ConsistencyGraceUntilTic[i] = max<int>(ConsistencyGraceUntilTic[i], gametic + TICRATE);
	}
	if (markApplied && id > InvasionLastAppliedSpawnEventId)
		InvasionLastAppliedSpawnEventId = id;

	DebugTrace::Markf("invasion.mirror", "mirror spawned id=%u wave=%d class=%s source=%s pos=(%.1f,%.1f,%.1f)",
		unsigned(id),
		wave,
		className.GetChars(),
		source != nullptr ? source : "unknown",
		pos.X,
		pos.Y,
		pos.Z);
	return true;
}

static void Net_DrainPendingInvasionMirrorSpawns()
{
	if (Net_IsLocalInvasionAuthority()
		|| InvasionPendingMirrorSpawns.Size() == 0
		|| NetworkEntityManager::IsPredicting())
	{
		return;
	}

	// Cap how many mirror spawns may be materialized per drain call so a large
	// pending burst (typical at the start of a wave, where the authority emits
	// dozens of monsters in a single tic) doesn't construct them all in one
	// frame. Each Spawn() runs P_SpawnMobj + thing initialization, so 50+ in a
	// tick is the dominant source of the "invasion lag" the player sees. The
	// remainder is retained and applied across subsequent frames. Scale the cap
	// with backlog so wave starts drain faster instead of leaving a long tail of
	// invisible monsters that still cost network + visual work every frame.
	const unsigned int pendingBacklog = unsigned(InvasionPendingMirrorSpawns.Size());
	const unsigned int MaxMirrorSpawnsPerCall = clamp<unsigned int>(
		max<unsigned int>(8u, pendingBacklog / 3u), 8u, 24u);
	unsigned int applied = 0u;

	TArray<FInvasionPendingMirrorSpawn> retained;
	for (const auto& pending : InvasionPendingMirrorSpawns)
	{
		if (auto existing = Net_FindInvasionReplicatedActor(pending.Id); existing != nullptr && existing->Actor != nullptr)
			continue;

		if (pending.Wave < InvasionWaveDirector.Wave || InvasionState == INVS_DISABLED)
			continue;

		if (gametic - pending.QueuedTic > TICRATE * 2)
			continue;

		if (pending.Wave != InvasionWaveDirector.Wave
			|| !Net_IsInvasionRoundActiveState(InvasionState)
			|| primaryLevel == nullptr
			|| gamestate != GS_LEVEL)
		{
			retained.Push(pending);
			continue;
		}

		if (applied >= MaxMirrorSpawnsPerCall)
		{
			// Defer the rest of the backlog to the next drain so we keep the
			// per-frame actor construction cost bounded.
			retained.Push(pending);
			continue;
		}

		if (!Net_SpawnInvasionMirrorActor(pending.Id, pending.Wave, pending.ClassName,
			pending.Pos, pending.Vel, pending.Yaw, pending.Pitch, pending.Health,
			"delta-repair-queued", pending.MarkApplied, HREP_ACTOR_UNKNOWN))
		{
			retained.Push(pending);
			continue;
		}

		++applied;
		if (auto ref = Net_FindInvasionReplicatedActor(pending.Id); ref != nullptr && ref->Actor != nullptr)
		{
			if (Net_ClassDefaultsSuggestProjectile(ref->Actor->GetClass())
				|| Net_IsInvasionReplicatedProjectile(ref->Actor.Get()))
			{
				ref->IsProjectile = true;
			}
			ref->Actor->Vel = pending.Vel;
			Net_SetInvasionMirrorVisualTarget(*ref, pending.Pos, pending.Vel, pending.Yaw, pending.Pitch, pending.Health);
		}
	}

	InvasionPendingMirrorSpawns.Swap(retained);
}

static bool Net_ApplyInvasionSpawnEvent(const FHCDEAuthorityEvent& event)
{
	if (Net_IsLocalInvasionAuthority())
		return true;

	if (event.Id <= InvasionLastAppliedSpawnEventId)
		return true;

	if (event.Wave != InvasionWaveDirector.Wave
		|| !Net_IsInvasionRoundActiveState(InvasionState)
		|| primaryLevel == nullptr
		|| gamestate != GS_LEVEL)
	{
		// Keep unapplied events replayable across the join/load handoff. Spawn
		// history is included in later snapshots, so the client can mirror them
		// once the level and invasion state are ready.
		return true;
	}

	if (NetworkEntityManager::IsPredicting())
	{
		// Spawn events are authoritative server state. If they are applied inside
		// the client prediction window, the rollback cleaner can classify them as
		// predicted scratch actors and delete them before they render.
		Net_QueueInvasionSpawnEvent(event);
		return true;
	}

	return Net_SpawnInvasionMirrorActor(event.Id, event.Wave, event.ClassName, event.Pos, event.Vel,
		event.Yaw, nullAngle, event.Health, "spawn-event", true, event.Category);
}

static void Net_DrainPendingInvasionSpawnEvents()
{
	if (Net_IsLocalInvasionAuthority()
		|| InvasionPendingSpawnEvents.Size() == 0
		|| NetworkEntityManager::IsPredicting())
	{
		return;
	}

	// Spread monster spawns across frames, but drain projectile spawn events
	// more aggressively so imp fireballs and other missiles appear promptly
	// instead of waiting behind wave-start monster bursts.
	const unsigned int pendingEvents = unsigned(InvasionPendingSpawnEvents.Size());
	const unsigned int MaxMonsterSpawnEventsPerCall = clamp<unsigned int>(
		max<unsigned int>(4u, pendingEvents / 4u), 4u, 16u);
	constexpr unsigned int MaxProjectileSpawnEventsPerCall = 24u;
	unsigned int monsterApplied = 0u;
	unsigned int projectileApplied = 0u;

	auto tryApplyEvent = [&](const FHCDEAuthorityEvent& event, bool projectileEvent) -> bool
	{
		if (event.Id <= InvasionLastAppliedSpawnEventId)
			return true;

		if (event.Wave < InvasionWaveDirector.Wave || InvasionState == INVS_DISABLED)
			return true;

		if (event.Wave != InvasionWaveDirector.Wave
			|| !Net_IsInvasionRoundActiveState(InvasionState)
			|| primaryLevel == nullptr
			|| gamestate != GS_LEVEL)
		{
			return false;
		}

		const unsigned int cap = projectileEvent ? MaxProjectileSpawnEventsPerCall : MaxMonsterSpawnEventsPerCall;
		const unsigned int applied = projectileEvent ? projectileApplied : monsterApplied;
		if (applied >= cap)
			return false;

		const uint32_t beforeId = InvasionLastAppliedSpawnEventId;
		Net_ApplyInvasionSpawnEvent(event);
		if (InvasionLastAppliedSpawnEventId > beforeId)
		{
			if (projectileEvent)
				++projectileApplied;
			else
				++monsterApplied;
			return true;
		}

		return event.Id > InvasionLastAppliedSpawnEventId ? false : true;
	};

	TArray<FHCDEAuthorityEvent> retained;
	const uint32_t previousAppliedSpawnEventId = InvasionLastAppliedSpawnEventId;
	for (const auto& event : InvasionPendingSpawnEvents)
	{
		if (event.Category == HREP_ACTOR_PROJECTILE)
		{
			if (!tryApplyEvent(event, true))
				retained.Push(event);
		}
	}
	for (const auto& event : InvasionPendingSpawnEvents)
	{
		if (event.Category == HREP_ACTOR_PROJECTILE)
			continue;

		if (!tryApplyEvent(event, false))
			retained.Push(event);
	}

	InvasionPendingSpawnEvents.Swap(retained);
	if (InvasionLastAppliedSpawnEventId != previousAppliedSpawnEventId)
	{
		DebugTrace::Markf("invasion", "mirror drained spawn events through %u active=%d wave=%d pending=%u",
			unsigned(InvasionLastAppliedSpawnEventId),
			Net_GetInvasionActiveMonsterCount(),
			Net_GetInvasionWave(),
			unsigned(InvasionPendingSpawnEvents.Size()));
		if (*hcde_hud_debug)
		{
			Printf(PRINT_HIGH,
				"HCDE invasion mirror drained spawn events through %u active=%d wave=%d pending=%u\n",
				unsigned(InvasionLastAppliedSpawnEventId),
				Net_GetInvasionActiveMonsterCount(),
				Net_GetInvasionWave(),
				unsigned(InvasionPendingSpawnEvents.Size()));
		}
	}
}

static void Net_LogInvasionMirrorVisualDiagnostic()
{
	if (Net_IsLocalInvasionAuthority()
		|| InvasionState == INVS_DISABLED
		|| gamestate != GS_LEVEL
		|| gametic < InvasionMirrorNextVisualDiagnosticTic)
	{
		return;
	}

	InvasionMirrorNextVisualDiagnosticTic = gametic + TICRATE * 2;

	AActor* camera = nullptr;
	if (consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
	{
		camera = players[consoleplayer].camera;
		if (camera == nullptr)
			camera = players[consoleplayer].mo;
	}

	const DVector3 cameraPos = camera != nullptr ? camera->Pos() : DVector3(0, 0, 0);
	int live = 0;
	int drawable = 0;
	int hidden = 0;
	int dormant = 0;
	int visualOnly = 0;
	int euthanized = 0;
	double nearestDistSq = -1.0;
	uint32_t nearestId = 0u;
	AActor* nearest = nullptr;

	for (auto& ref : InvasionReplicatedActors)
	{
		AActor* actor = ref.Actor;
		if (actor == nullptr)
			continue;

		if ((actor->ObjectFlags & OF_EuthanizeMe) != 0)
		{
			++euthanized;
			continue;
		}

		if (actor->health > 0)
			++live;
		if ((actor->flags2 & MF2_DORMANT) != 0)
			++dormant;
		if (actor->GetStatNum() < STAT_FIRST_THINKING)
			++visualOnly;

		const bool actorDrawable = (actor->renderflags & RF_INVISIBLE) == 0
			&& actor->RenderStyle.IsVisible(actor->Alpha);
		if (actorDrawable)
			++drawable;
		else
			++hidden;

		if (camera == nullptr)
			continue;

		const double distSq = (actor->Pos() - cameraPos).LengthSquared();
		if (nearest == nullptr || nearestDistSq < 0.0 || distSq < nearestDistSq)
		{
			nearestDistSq = distSq;
			nearestId = ref.Id;
			nearest = actor;
		}
	}

	if (nearest != nullptr)
	{
		const double dist = sqrt(nearestDistSq);
		DebugTrace::Debugf("invasion",
			"mirror visual state=%s wave=%d tracked=%u live=%d drawable=%d hidden=%d dormant=%d visualonly=%d euth=%d camera=(%.1f, %.1f, %.1f) nearest=%u:%s pos=(%.1f, %.1f, %.1f) dist=%.1f health=%d speed=%.2f stepcap=%.2f stat=%d flags=0x%x flags2=0x%x rflags=0x%x style=0x%x alpha=%.2f",
			Net_InvasionStateName(InvasionState),
			InvasionWaveDirector.Wave,
			unsigned(InvasionReplicatedActors.Size()),
			live,
			drawable,
			hidden,
			dormant,
			visualOnly,
			euthanized,
			cameraPos.X,
			cameraPos.Y,
			cameraPos.Z,
			unsigned(nearestId),
			nearest->GetClass()->TypeName.GetChars(),
			nearest->X(),
			nearest->Y(),
			nearest->Z(),
			dist,
			nearest->health,
			nearest->Speed,
			Net_GetInvasionMirrorVisualStepCap(nearest),
			nearest->GetStatNum(),
			nearest->flags.GetValue(),
			nearest->flags2.GetValue(),
			nearest->renderflags.GetValue(),
			unsigned(nearest->RenderStyle.AsDWORD),
			nearest->Alpha);
		if (*hcde_hud_debug)
		{
			Printf(PRINT_HIGH,
				"HCDE invasion mirror visual state=%s wave=%d tracked=%u live=%d drawable=%d hidden=%d dormant=%d visualonly=%d euth=%d camera=(%.1f, %.1f, %.1f) nearest=%u:%s pos=(%.1f, %.1f, %.1f) dist=%.1f health=%d speed=%.2f stepcap=%.2f stat=%d flags=0x%x flags2=0x%x rflags=0x%x style=0x%x alpha=%.2f\n",
				Net_InvasionStateName(InvasionState),
				InvasionWaveDirector.Wave,
				unsigned(InvasionReplicatedActors.Size()),
				live,
				drawable,
				hidden,
				dormant,
				visualOnly,
				euthanized,
				cameraPos.X,
				cameraPos.Y,
				cameraPos.Z,
				unsigned(nearestId),
				nearest->GetClass()->TypeName.GetChars(),
				nearest->X(),
				nearest->Y(),
				nearest->Z(),
				dist,
				nearest->health,
				nearest->Speed,
				Net_GetInvasionMirrorVisualStepCap(nearest),
				nearest->GetStatNum(),
				nearest->flags.GetValue(),
				nearest->flags2.GetValue(),
				nearest->renderflags.GetValue(),
				unsigned(nearest->RenderStyle.AsDWORD),
				nearest->Alpha);
		}
	}
	else
	{
		DebugTrace::Debugf("invasion",
			"mirror visual state=%s wave=%d tracked=%u live=%d drawable=%d hidden=%d dormant=%d visualonly=%d euth=%d camera=%s",
			Net_InvasionStateName(InvasionState),
			InvasionWaveDirector.Wave,
			unsigned(InvasionReplicatedActors.Size()),
			live,
			drawable,
			hidden,
			dormant,
			visualOnly,
			euthanized,
			camera != nullptr ? "ready" : "missing");
		if (*hcde_hud_debug)
		{
			Printf(PRINT_HIGH,
				"HCDE invasion mirror visual state=%s wave=%d tracked=%u live=%d drawable=%d hidden=%d dormant=%d visualonly=%d euth=%d camera=%s\n",
				Net_InvasionStateName(InvasionState),
				InvasionWaveDirector.Wave,
				unsigned(InvasionReplicatedActors.Size()),
				live,
				drawable,
				hidden,
				dormant,
				visualOnly,
				euthanized,
				camera != nullptr ? "ready" : "missing");
		}
	}
}

static bool Net_IsInvasionActorCorpseLike(const AActor* actor)
{
	return actor != nullptr
		&& (actor->health <= 0 || (actor->flags & MF_CORPSE) != 0);
}

static bool Net_ClassDefaultsSuggestProjectile(PClassActor* cls)
{
	if (cls == nullptr)
		return false;
	const AActor* def = GetDefaultByType(cls);
	return def != nullptr
		&& ((def->flags & MF_MISSILE) != 0 || (def->BounceFlags & BOUNCE_MBF) != 0);
}

static bool Net_IsInvasionReplicatedProjectile(const AActor* actor)
{
	if (actor == nullptr)
		return false;
	if ((actor->flags & MF_MISSILE) != 0 || (actor->BounceFlags & BOUNCE_MBF) != 0)
		return true;
	return Net_ClassDefaultsSuggestProjectile(actor->GetClass());
}

static void Net_PrepareInvasionMirrorCorpsePhysics(AActor* actor, bool snapToFloor)
{
	if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		return;

	if (actor->GetStatNum() < STAT_FIRST_THINKING)
		actor->ChangeStatNum(STAT_DEFAULT);
	actor->flags &= ~MF_NOCLIP;
	actor->flags4 &= ~MF4_STANDSTILL;
	if (actor->Sector != nullptr)
	{
		P_FindFloorCeiling(actor);
		if (snapToFloor && (actor->flags & MF_NOGRAVITY) == 0 && fabs(actor->Z() - actor->floorz) > 0.5)
		{
			actor->SetZ(actor->floorz, false);
			actor->Prev.Z = actor->Z();
		}
	}
	actor->Vel = DVector3(0, 0, 0);
	actor->ClearInterpolation();
}

static bool Net_InvasionStateSequenceContains(const AActor* actor, FState* start, FState* state)
{
	if (actor == nullptr || start == nullptr || state == nullptr)
		return false;

	FState* current = start;
	for (int steps = 0; current != nullptr && steps < 32; ++steps)
	{
		if (current == state)
			return true;

		FState* next = current->GetNextState();
		if (next == nullptr
			|| next == start
			|| next == actor->SpawnState
			|| next == actor->SeeState)
		{
			return false;
		}
		current = next;
	}
	return false;
}

static uint8_t Net_GetInvasionActorActionState(const AActor* actor)
{
	if (actor == nullptr || actor->state == nullptr)
		return HCDEInvasionActorActionNone;
	if (Net_IsInvasionActorCorpseLike(actor))
		return HCDEInvasionActorActionNone;
	if (auto ref = Net_FindInvasionReplicatedActorByActor(actor); ref != nullptr)
	{
		if ((ref->ServerForcedActionState == HCDEInvasionActorActionMelee
				|| ref->ServerForcedActionState == HCDEInvasionActorActionMissile
				|| ref->ServerForcedActionState == HCDEInvasionActorActionPain)
			&& gametic - ref->ServerForcedActionTic <= HCDEInvasionActorActionHoldTics)
		{
			return ref->ServerForcedActionState;
		}
	}
	if (Net_InvasionStateSequenceContains(actor, actor->MeleeState, actor->state))
		return HCDEInvasionActorActionMelee;
	if (Net_InvasionStateSequenceContains(actor, actor->MissileState, actor->state))
		return HCDEInvasionActorActionMissile;
	if (Net_InvasionStateSequenceContains(actor, actor->FindState(NAME_Pain), actor->state))
		return HCDEInvasionActorActionPain;
	if (actor->SeeState != nullptr)
		return HCDEInvasionActorActionSee;
	if (actor->SpawnState != nullptr)
		return HCDEInvasionActorActionSpawn;
	return HCDEInvasionActorActionNone;
}

static FState* Net_GetInvasionMirrorActionState(AActor* actor, uint8_t actionState)
{
	if (actor == nullptr)
		return nullptr;

	switch (actionState)
	{
	case HCDEInvasionActorActionSpawn:
		return actor->SpawnState;
	case HCDEInvasionActorActionSee:
		return actor->SeeState != nullptr ? actor->SeeState : actor->SpawnState;
	case HCDEInvasionActorActionMelee:
		return actor->MeleeState != nullptr ? actor->MeleeState : actor->SeeState;
	case HCDEInvasionActorActionMissile:
		return actor->MissileState != nullptr ? actor->MissileState : actor->SeeState;
	case HCDEInvasionActorActionPain:
		if (FState* pain = actor->FindState(NAME_Pain); pain != nullptr)
			return pain;
		return actor->SeeState;
	default:
		return nullptr;
	}
}

static bool Net_IsInvasionActorActionPriority(uint8_t actionState)
{
	return actionState == HCDEInvasionActorActionMelee
		|| actionState == HCDEInvasionActorActionMissile
		|| actionState == HCDEInvasionActorActionPain;
}

static void HCDEInsertActorPriorityCandidate(TArray<FHCDEActorPriorityCandidate>& queue, const FHCDEActorPriorityCandidate& candidate)
{
	size_t pos = queue.Size();
	queue.Push(candidate);
	while (pos > 0u)
	{
		const auto& previous = queue[pos - 1u];
		if (previous.Score > candidate.Score
			|| (previous.Score == candidate.Score && previous.ActorIndex <= candidate.ActorIndex))
		{
			break;
		}
		queue[pos] = previous;
		--pos;
	}
	queue[pos] = candidate;
}

static bool HCDEIsValidLiveClient(int clientNum)
{
	return clientNum >= 0 && clientNum < MAXPLAYERS;
}

static FHCDEProjectilePolicyResult HCDEEvaluateProjectilePolicy(AActor* projectile, AActor* viewer, bool hasBaseline, int lastRelevantTic)
{
	FHCDEProjectilePolicyResult policy;
	policy.IsProjectile = projectile != nullptr
		&& (Net_IsInvasionReplicatedProjectile(projectile) || (projectile->flags & MF_MISSILE) != 0);
	if (!policy.IsProjectile)
		return policy;

	policy.HasBaseline = hasBaseline;
	const bool live = (projectile->ObjectFlags & OF_EuthanizeMe) == 0;
	policy.PlayerOwned = projectile->target != nullptr && projectile->target->player != nullptr;
	if (viewer != nullptr)
	{
		const DVector3 toViewer = viewer->Pos() - projectile->Pos();
		policy.DistanceSquared = toViewer.LengthSquared();
		policy.TargetingViewer = projectile->target == viewer || projectile->tracer == viewer;
		const double closingSpeed = projectile->Vel | toViewer;
		policy.InboundToViewer = closingSpeed > 0.0
			&& projectile->Vel.LengthSquared() > 16.0 * 16.0
			&& policy.DistanceSquared <= 4096.0 * 4096.0;
	}
	else
	{
		// Without a viewer (join/respawn edge cases), keep non-owned projectiles conservative to avoid
		// over-broadcasting priority when no spatial signal is available.
		policy.Tier = policy.PlayerOwned ? HINTEREST_MEDIUM : HINTEREST_LOW;
	}

	const bool hasDistance = policy.DistanceSquared >= 0.0;
	if (policy.TargetingViewer
		|| (hasDistance && policy.DistanceSquared <= 768.0 * 768.0)
		|| (policy.InboundToViewer && hasDistance && policy.DistanceSquared <= 2048.0 * 2048.0))
	{
		policy.Tier = HINTEREST_CRITICAL;
		policy.Protected = true;
	}
	else if ((hasDistance && policy.DistanceSquared <= 2048.0 * 2048.0)
		|| (hasDistance && policy.PlayerOwned && policy.DistanceSquared <= 3072.0 * 3072.0)
		|| policy.InboundToViewer)
	{
		policy.Tier = HINTEREST_HIGH;
	}
	else if (hasDistance && policy.DistanceSquared <= 4096.0 * 4096.0)
	{
		policy.Tier = HINTEREST_MEDIUM;
	}
	else if (hasDistance && policy.DistanceSquared <= 8192.0 * 8192.0)
	{
		policy.Tier = HINTEREST_LOW;
	}
	else if (!hasDistance)
	{
		// No viewer position available (e.g., during join/respawn), keep a conservative baseline.
		policy.Tier = policy.PlayerOwned ? HINTEREST_MEDIUM : HINTEREST_LOW;
	}
	else
	{
		policy.Tier = HINTEREST_DORMANT;
	}

	switch (EHCDEActorInterestTier(policy.Tier))
	{
	case HINTEREST_CRITICAL:
		policy.KeepAliveTics = 1;
		policy.ScoreBonus = 8000;
		break;
	case HINTEREST_HIGH:
		policy.KeepAliveTics = max<int>(TICRATE / 6, 1);
		policy.ScoreBonus = 5500;
		break;
	case HINTEREST_MEDIUM:
		policy.KeepAliveTics = max<int>(TICRATE / 3, 1);
		policy.ScoreBonus = 3000;
		break;
	case HINTEREST_LOW:
		policy.KeepAliveTics = TICRATE;
		policy.ScoreBonus = 900;
		break;
	case HINTEREST_DORMANT:
	default:
		policy.KeepAliveTics = TICRATE * 3;
		policy.ScoreBonus = 0;
		break;
	}

	const int silentTics = hasBaseline ? max<int>(gametic - lastRelevantTic, 0) : INT_MAX;
	policy.KeepAlive = hasBaseline && silentTics >= policy.KeepAliveTics;
	policy.Relevant = live
		&& (policy.Protected
			|| policy.KeepAlive
			|| policy.Tier == HINTEREST_CRITICAL
			|| policy.Tier == HINTEREST_HIGH
			|| policy.Tier == HINTEREST_MEDIUM
			|| (!hasBaseline && policy.Tier == HINTEREST_LOW));
	return policy;
}

static void HCDERecordProjectilePolicyResult(int clientNum, const FHCDEProjectilePolicyResult& policy)
{
	if (!policy.IsProjectile)
		return;

	++HCDELiveProfile.ProjectilePolicyEvaluated;
	if (policy.Tier < HINTEREST_COUNT)
	{
		if (clientNum >= 0 && clientNum < MAXPLAYERS)
			++HCDELivePeers[clientNum].ProjectilePolicyTiers[policy.Tier];
		switch (EHCDEActorInterestTier(policy.Tier))
		{
		case HINTEREST_CRITICAL:
			++HCDELiveProfile.ProjectilePolicyCritical;
			break;
		case HINTEREST_HIGH:
			++HCDELiveProfile.ProjectilePolicyHigh;
			break;
		case HINTEREST_MEDIUM:
			++HCDELiveProfile.ProjectilePolicyMedium;
			break;
		case HINTEREST_LOW:
			++HCDELiveProfile.ProjectilePolicyLow;
			break;
		case HINTEREST_DORMANT:
		default:
			++HCDELiveProfile.ProjectilePolicyDormant;
			break;
		}
	}
	if (!policy.Relevant)
	{
		++HCDELiveProfile.ProjectilePolicySkipped;
		if (clientNum >= 0 && clientNum < MAXPLAYERS)
			++HCDELivePeers[clientNum].ProjectilePolicySkipped;
	}
	if (policy.KeepAlive)
	{
		++HCDELiveProfile.ProjectilePolicyKeepAlive;
		if (clientNum >= 0 && clientNum < MAXPLAYERS)
			++HCDELivePeers[clientNum].ProjectilePolicyKeepAlive;
	}
	if (policy.Protected)
	{
		++HCDELiveProfile.ProjectilePolicyProtected;
		if (clientNum >= 0 && clientNum < MAXPLAYERS)
			++HCDELivePeers[clientNum].ProjectilePolicyProtected;
	}
	if (policy.InboundToViewer)
		++HCDELiveProfile.ProjectilePolicyInbound;
	if (policy.PlayerOwned)
		++HCDELiveProfile.ProjectilePolicyPlayerOwned;
}

static bool HCDEShouldSendSharedActorDelta(const FHCDEReplicatedActorRef& ref)
{
	if (!ref.Active || ref.Retired || ref.Actor == nullptr)
		return false;
	if ((ref.Actor->ObjectFlags & OF_EuthanizeMe) != 0)
		return false;
	if (ref.Category == HREP_ACTOR_PLAYER)
	{
		++HCDELiveProfile.SharedActorPlayerRecordsSuppressed;
		return false;
	}
	if (ref.Category == HREP_ACTOR_UNKNOWN || ref.Category > HREP_ACTOR_VISUAL)
		return false;

	return ref.Source == HREP_SOURCE_SHARED
		|| ref.Source == HREP_SOURCE_COOP
		|| ref.Source == HREP_SOURCE_DM;
}

static uint8_t HCDEGetSharedActorActionState(AActor* actor, uint8_t category)
{
	if (actor == nullptr || category != HREP_ACTOR_MONSTER)
		return HCDEInvasionActorActionNone;
	return Net_GetInvasionActorActionState(actor);
}

static FHCDEActorInterestResult HCDEComputeInvasionActorInterest(int clientNum, size_t actorIndex)
{
	FHCDEActorInterestResult interest;
	if (clientNum < 0 || clientNum >= MAXPLAYERS || actorIndex >= InvasionReplicatedActors.Size())
		return interest;

	AActor* actor = InvasionReplicatedActors[actorIndex].Actor;
	if (actor == nullptr)
		return interest;

	const auto& ref = InvasionReplicatedActors[actorIndex];
	const uint8_t actionState = Net_GetInvasionActorActionState(actor);
	const bool actionPriority = Net_IsInvasionActorActionPriority(actionState);
	const bool deadOrForced = ref.ForceDeathDelta || Net_IsInvasionActorCorpseLike(actor);
	const bool liveProjectile = ref.IsProjectile && Net_IsInvasionReplicatedProjectile(actor) && !ref.ForceDeathDelta;
	int lastRelevantTic = 0;
	const auto* sharedRef = Net_FindHCDEReplicatedActor(ref.Id);
	if (sharedRef != nullptr)
	{
		const auto& sent = sharedRef->ClientState[clientNum];
		interest.HasBaseline = sent.BaselineValid;
		lastRelevantTic = sent.LastSentTic;
	}
	if (HCDEActorBaselineRepairActive(clientNum))
	{
		interest.HasBaseline = false;
		lastRelevantTic = 0;
	}

	AActor* viewer = players[clientNum].mo;
	bool targetsViewer = false;
	if (viewer != nullptr)
	{
		interest.DistanceSquared = actor->Distance3DSquared(viewer);
		targetsViewer = actor->target == viewer || actor->tracer == viewer;
	}

	const FHCDEProjectilePolicyResult projectilePolicy = liveProjectile
		? HCDEEvaluateProjectilePolicy(actor, viewer, interest.HasBaseline, lastRelevantTic)
		: FHCDEProjectilePolicyResult();
	if (liveProjectile)
		HCDERecordProjectilePolicyResult(clientNum, projectilePolicy);
	interest.Protected = !interest.HasBaseline || deadOrForced || actionPriority || targetsViewer;
	if (liveProjectile)
		interest.Protected = deadOrForced || projectilePolicy.Protected;

	if (liveProjectile && !interest.HasBaseline)
		interest.Score += 50000;

	if (liveProjectile)
		interest.Tier = projectilePolicy.Tier;
	else if (interest.Protected)
		interest.Tier = HINTEREST_CRITICAL;
	else if (interest.DistanceSquared >= 0.0 && interest.DistanceSquared <= 1024.0 * 1024.0)
		interest.Tier = HINTEREST_HIGH;
	else if (interest.DistanceSquared >= 0.0 && interest.DistanceSquared <= 2048.0 * 2048.0)
		interest.Tier = HINTEREST_MEDIUM;
	else if (interest.DistanceSquared >= 0.0 && interest.DistanceSquared <= 4096.0 * 4096.0)
		interest.Tier = HINTEREST_LOW;
	else
		interest.Tier = HINTEREST_DORMANT;

	if (liveProjectile)
	{
		interest.KeepAliveTics = projectilePolicy.KeepAliveTics;
	}
	else
	{
		switch (EHCDEActorInterestTier(interest.Tier))
		{
		case HINTEREST_CRITICAL:
			interest.KeepAliveTics = 1;
			break;
		case HINTEREST_HIGH:
			interest.KeepAliveTics = max<int>(TICRATE / 5, 1);
			break;
		case HINTEREST_MEDIUM:
			interest.KeepAliveTics = max<int>(TICRATE / 2, 1);
			break;
		case HINTEREST_LOW:
			interest.KeepAliveTics = TICRATE * 2;
			break;
		case HINTEREST_DORMANT:
		default:
			interest.KeepAliveTics = TICRATE * 5;
			break;
		}
	}

	interest.LastRelevantTic = lastRelevantTic;
	const int silentTics = interest.HasBaseline ? max<int>(gametic - lastRelevantTic, 0) : INT_MAX;
	interest.KeepAlive = liveProjectile ? projectilePolicy.KeepAlive : (interest.HasBaseline && silentTics >= interest.KeepAliveTics);
	interest.Relevant = liveProjectile
		? (deadOrForced || projectilePolicy.Relevant)
		: (interest.Protected
			|| !interest.HasBaseline
			|| interest.KeepAlive
			|| interest.Tier == HINTEREST_HIGH
			|| interest.Tier == HINTEREST_MEDIUM);
	interest.Priority = liveProjectile
		? (deadOrForced || projectilePolicy.Protected || projectilePolicy.KeepAlive || projectilePolicy.Tier <= HINTEREST_HIGH)
		: (interest.Protected || interest.KeepAlive);

	if (!interest.Relevant)
		return interest;

	if (!interest.HasBaseline)
		interest.Score += 12000;
	if (deadOrForced)
		interest.Score += 11000;
	if (actionPriority)
		interest.Score += 9000;
	if (liveProjectile)
		interest.Score += projectilePolicy.ScoreBonus;
	if (targetsViewer)
		interest.Score += 4500;
	if (interest.KeepAlive)
		interest.Score += 3500;

	switch (EHCDEActorInterestTier(interest.Tier))
	{
	case HINTEREST_CRITICAL:
		interest.Score += 5000;
		break;
	case HINTEREST_HIGH:
		interest.Score += 3000;
		break;
	case HINTEREST_MEDIUM:
		interest.Score += 1500;
		break;
	case HINTEREST_LOW:
		interest.Score += 500;
		break;
	default:
		break;
	}

	if (interest.HasBaseline && lastRelevantTic > 0)
		interest.Score += clamp<int>(gametic - lastRelevantTic, 0, TICRATE * 5) * 18;
	else
		interest.Score += TICRATE * 4 * 18;

	return interest;
}

static int HCDEComputeInvasionActorPriorityScore(int clientNum, size_t actorIndex, size_t activeRefs, size_t sendCursor, bool& priority, bool& keepAlive, uint8_t& interestTier)
{
	priority = false;
	keepAlive = false;
	interestTier = HINTEREST_DORMANT;
	const FHCDEActorInterestResult interest = HCDEComputeInvasionActorInterest(clientNum, actorIndex);
	if (!interest.Relevant)
		return INT_MIN;

	priority = interest.Priority;
	keepAlive = interest.KeepAlive;
	interestTier = interest.Tier;
	int score = interest.Score;

	if (activeRefs > 0u)
	{
		const size_t wrappedOffset = (actorIndex + activeRefs - (sendCursor % activeRefs)) % activeRefs;
		score += int(min<size_t>(activeRefs - wrappedOffset, 512u));
	}

	return score;
}

static TArray<FHCDEActorPriorityCandidate>& HCDEBuildInvasionActorPriorityQueue(int clientNum, int activeRefs, size_t sendCursor)
{
	auto& queue = HCDEActorPriorityQueues[clientNum];
	queue.Clear();
	uint32_t priorityDepth = 0u;
	uint32_t keepAliveDepth = 0u;
	uint32_t skippedDepth = 0u;
	uint32_t interestTiers[HINTEREST_COUNT] = {};
	if (clientNum < 0 || clientNum >= MAXPLAYERS || activeRefs <= 0)
		return queue;

	HCDELivePeers[clientNum].ProjectilePolicySkipped = 0u;
	HCDELivePeers[clientNum].ProjectilePolicyKeepAlive = 0u;
	HCDELivePeers[clientNum].ProjectilePolicyProtected = 0u;
	for (uint8_t interest = 0u; interest < HINTEREST_COUNT; ++interest)
		HCDELivePeers[clientNum].ProjectilePolicyTiers[interest] = 0u;

	for (size_t actorIndex = 0u; actorIndex < size_t(activeRefs); ++actorIndex)
	{
		bool priority = false;
		bool keepAlive = false;
		uint8_t interestTier = HINTEREST_DORMANT;
		const int score = HCDEComputeInvasionActorPriorityScore(clientNum, actorIndex, size_t(activeRefs), sendCursor, priority, keepAlive, interestTier);
		if (interestTier < HINTEREST_COUNT)
			++interestTiers[interestTier];
		if (score == INT_MIN)
		{
			++skippedDepth;
			continue;
		}

		FHCDEActorPriorityCandidate candidate;
		candidate.ActorIndex = actorIndex;
		candidate.Score = score;
		candidate.Priority = priority;
		candidate.KeepAlive = keepAlive;
		candidate.InterestTier = interestTier;
		HCDEInsertActorPriorityCandidate(queue, candidate);
		if (priority)
			++priorityDepth;
		if (keepAlive)
			++keepAliveDepth;
	}

	auto& peer = HCDELivePeers[clientNum];
	peer.ActorQueueDepth = uint32_t(queue.Size());
	peer.ActorQueuePriorityDepth = priorityDepth;
	peer.ActorQueueDeferredDepth = 0u;
	peer.ActorQueueTopScore = queue.Size() > 0u ? queue[0].Score : 0;
	peer.ActorInterestSkipped = skippedDepth;
	peer.ActorInterestKeepAlive = keepAliveDepth;
	for (uint8_t interest = 0u; interest < HINTEREST_COUNT; ++interest)
		peer.ActorInterestTiers[interest] = interestTiers[interest];
	++HCDELiveProfile.ActorQueueBuilds;
	HCDELiveProfile.ActorQueueCandidates += queue.Size();
	HCDELiveProfile.ActorQueuePriorityCandidates += priorityDepth;
	HCDELiveProfile.ActorQueueMaxDepth = max<uint64_t>(HCDELiveProfile.ActorQueueMaxDepth, queue.Size());
	HCDELiveProfile.ActorInterestCritical += interestTiers[HINTEREST_CRITICAL];
	HCDELiveProfile.ActorInterestHigh += interestTiers[HINTEREST_HIGH];
	HCDELiveProfile.ActorInterestMedium += interestTiers[HINTEREST_MEDIUM];
	HCDELiveProfile.ActorInterestLow += interestTiers[HINTEREST_LOW];
	HCDELiveProfile.ActorInterestDormant += interestTiers[HINTEREST_DORMANT];
	HCDELiveProfile.ActorInterestSkipped += skippedDepth;
	HCDELiveProfile.ActorInterestKeepAlive += keepAliveDepth;
	HCDELiveProfile.ActorInterestProtected += priorityDepth;
	return queue;
}

static void Net_ApplyInvasionMirrorActionState(FInvasionReplicatedActorRef& ref, AActor* actor, uint8_t actionState)
{
	if (actor == nullptr
		|| actionState == HCDEInvasionActorActionNone
		|| actionState > HCDEInvasionActorActionMax
		|| Net_IsInvasionActorCorpseLike(actor))
	{
		return;
	}

	FState* targetState = Net_GetInvasionMirrorActionState(actor, actionState);
	if (targetState == nullptr)
		return;

	// Repeated priority deltas for the same attack should not restart the
	// local animation while it is already inside that action sequence. Some
	// monsters, such as the Cacodemon, put the fullbright firing frame a few
	// states after the missile state's first frame, so naively re-entering
	// targetState every delta would skip those frames every tic. We detect
	// "still playing the right animation" via Net_InvasionStateSequenceContains
	// (walks targetState's frame chain to see if actor->state is part of it).
	const bool alreadyInActionSequence = Net_InvasionStateSequenceContains(actor, targetState, actor->state);
	if (ref.VisualActionState != actionState || actor->state == nullptr || !alreadyInActionSequence)
	{
		actor->SetState(targetState, true);
		ref.VisualActionState = actionState;
		ref.VisualActionTic = gametic;
	}
}

static void Net_DetachInvasionMirrorCorpse(FInvasionReplicatedActorRef& ref)
{
	AActor* actor = ref.Actor;
	if (actor != nullptr && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
	{
		// A retired mirror corpse is no longer server-driven, so stop any last
		// replicated velocity from making the death frame slide around.
		Net_PrepareInvasionMirrorCorpsePhysics(actor, true);
		DebugTrace::Markf("invasion", "mirror corpse detached id=%u class=%s health=%d pos=(%.1f,%.1f,%.1f)",
			unsigned(ref.Id),
			actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
			actor->health,
			actor->X(),
			actor->Y(),
			actor->Z());
	}
	Net_SetInvasionReplicatedActorPtr(ref, nullptr);
	ref.DeathDeltaSent = true;
}

static void Net_RetireInvasionMirrorProjectile(FInvasionReplicatedActorRef& ref)
{
	AActor* actor = ref.Actor;
	if (actor != nullptr && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
	{
		if (actor->GetStatNum() < STAT_FIRST_THINKING)
			actor->ChangeStatNum(STAT_DEFAULT);
		actor->flags |= MF_NOBLOCKMAP | MF_NOCLIP;
		actor->flags &= ~(MF_SOLID | MF_SHOOTABLE);
		actor->flags5 |= MF5_NOINTERACTION;

		DebugTrace::Markf("invasion", "mirror projectile retired id=%u class=%s pos=(%.1f,%.1f,%.1f) flags=0x%x bounce=0x%x",
			unsigned(ref.Id),
			actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
			actor->X(),
			actor->Y(),
			actor->Z(),
			actor->flags.GetValue(),
			actor->BounceFlags.GetValue());

		if (Net_IsInvasionReplicatedProjectile(actor))
		{
			P_ExplodeMissile(actor, nullptr, nullptr);
		}
		else
		{
			actor->ClearCounters();
			actor->Destroy();
		}
	}
	Net_SetInvasionReplicatedActorPtr(ref, nullptr);
	ref.DeathDeltaSent = true;
}

static void Net_PurgeStaleInvasionMirrorActorsOnClient()
{
	if (Net_IsLocalInvasionAuthority() || InvasionReplicatedActors.Size() == 0)
		return;

	size_t writeIdx = 0u;
	unsigned purged = 0u;
	for (size_t i = 0u; i < InvasionReplicatedActors.Size(); ++i)
	{
		auto& ref = InvasionReplicatedActors[i];
		AActor* actor = ref.Actor;
		if (ref.Id == 0u
			|| actor == nullptr
			|| (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		{
			if (actor != nullptr)
			{
				actor->ClearCounters();
				actor->Destroy();
			}
			Net_SetInvasionReplicatedActorPtr(ref, nullptr);
			++purged;
			continue;
		}

		if (ref.IsProjectile)
		{
			const bool projectileExpired = ref.SpawnTic > 0
				&& gametic - ref.SpawnTic > HCDEInvasionProjectileMirrorMaxAgeTics;
			if (!Net_IsInvasionReplicatedProjectile(actor) || projectileExpired || ref.ForceDeathDelta)
			{
				Net_RetireInvasionMirrorProjectile(ref);
				++purged;
				continue;
			}
		}
		else if (Net_IsInvasionActorCorpseLike(actor) && ref.DeathDeltaSent)
		{
			Net_DetachInvasionMirrorCorpse(ref);
			++purged;
			continue;
		}

		if (writeIdx != i)
			InvasionReplicatedActors[writeIdx] = ref;
		++writeIdx;
	}

	if (writeIdx < InvasionReplicatedActors.Size())
	{
		InvasionReplicatedActors.Resize(unsigned(writeIdx));
		Net_RebuildInvasionReplicatedActorIndexes();
	}

	if (purged > 0 && *hcde_hud_debug)
	{
		Printf(PRINT_HIGH, "HCDE invasion mirror purge removed=%u tracked=%u pending-spawns=%u pending-events=%u\n",
			purged,
			unsigned(InvasionReplicatedActors.Size()),
			unsigned(InvasionPendingMirrorSpawns.Size()),
			unsigned(InvasionPendingSpawnEvents.Size()));
	}
}

static void Net_RetireInvasionMirrorActor(FInvasionReplicatedActorRef& ref, int serverHealth)
{
	AActor* actor = ref.Actor;
	if (actor == nullptr)
	{
		ref.DeathDeltaSent = true;
		return;
	}

	if (ref.IsProjectile)
	{
		Net_RetireInvasionMirrorProjectile(ref);
		return;
	}

	const bool alreadyCorpse = Net_IsInvasionActorCorpseLike(actor);
	if (!alreadyCorpse && serverHealth <= 0 && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
	{
		actor->health = min<int>(actor->health, serverHealth);
		Net_PrepareInvasionMirrorCorpsePhysics(actor, false);
		if ((actor->flags & MF_CORPSE) == 0)
			actor->CallDie(nullptr, nullptr);
	}

	if (Net_IsInvasionActorCorpseLike(actor) && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
	{
		Net_DetachInvasionMirrorCorpse(ref);
		return;
	}

	DebugTrace::Markf("invasion", "mirror stale actor destroyed id=%u class=%s health=%d server-health=%d pos=(%.1f,%.1f,%.1f)",
		unsigned(ref.Id),
		actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
		actor->health,
		serverHealth,
		actor->X(),
		actor->Y(),
		actor->Z());
	actor->ClearCounters();
	actor->Destroy();
	Net_SetInvasionReplicatedActorPtr(ref, nullptr);
	ref.DeathDeltaSent = true;
}

static bool Net_ApplyInvasionDespawnEvent(uint32_t actorId, int serverHealth)
{
	if (Net_IsLocalInvasionAuthority())
		return true;
	if (actorId == 0u)
		return false;
	if (!Net_IsInvasionRoundActiveState(InvasionState)
		|| primaryLevel == nullptr
		|| gamestate != GS_LEVEL
		|| NetworkEntityManager::IsPredicting())
	{
		return true;
	}

	auto* ref = Net_FindInvasionReplicatedActor(actorId);
	if (ref == nullptr || ref->Actor == nullptr)
		return true;

	Net_RetireInvasionMirrorActor(*ref, serverHealth);
	return true;
}

static bool Net_ApplyInvasionDamageEvent(uint32_t actorId, int serverHealth)
{
	if (Net_IsLocalInvasionAuthority())
		return true;
	if (actorId == 0u)
		return false;
	if (!Net_IsInvasionRoundActiveState(InvasionState)
		|| primaryLevel == nullptr
		|| gamestate != GS_LEVEL
		|| NetworkEntityManager::IsPredicting())
	{
		return true;
	}

	auto* ref = Net_FindInvasionReplicatedActor(actorId);
	if (ref == nullptr || ref->Actor == nullptr)
		return true;

	AActor* actor = ref->Actor.Get();
	if (serverHealth <= 0 || Net_IsInvasionActorCorpseLike(actor))
	{
		Net_RetireInvasionMirrorActor(*ref, serverHealth);
		return true;
	}

	if (actor->health != serverHealth)
	{
		const bool tookDamage = serverHealth < actor->health;
		actor->health = serverHealth;
		ref->VisualTargetHealth = serverHealth;
		ref->VisualTargetTic = gametic;
		if (!ref->IsProjectile && tookDamage)
			Net_ApplyInvasionMirrorActionState(*ref, actor, HCDEInvasionActorActionPain);
	}

	return true;
}

static int Net_CompactInvasionReplicatedActors()
{
	size_t writeIdx = 0u;
	for (size_t i = 0u; i < InvasionReplicatedActors.Size(); ++i)
	{
		AActor* actor = InvasionReplicatedActors[i].Actor;
		if (InvasionReplicatedActors[i].Id == 0u
			|| actor == nullptr
			|| (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		{
			if (actor != nullptr && !InvasionReplicatedActors[i].DeathDeltaSent)
				Net_RecordInvasionDespawnEvent(InvasionReplicatedActors[i], actor, actor->health);
			continue;
		}

		if (!InvasionReplicatedActors[i].IsProjectile)
		{
			const int previousHealth = InvasionReplicatedActors[i].LastAuthorityHealth;
			const int currentHealth = actor->health;
			if (currentHealth != previousHealth)
				Net_RecordInvasionDamageEvent(InvasionReplicatedActors[i], actor, previousHealth, currentHealth);
			InvasionReplicatedActors[i].LastAuthorityHealth = currentHealth;
		}

		if (InvasionReplicatedActors[i].IsProjectile)
		{
			const bool projectileExpired = InvasionReplicatedActors[i].SpawnTic > 0
				&& gametic - InvasionReplicatedActors[i].SpawnTic > HCDEInvasionProjectileMirrorMaxAgeTics;
			if (!Net_IsInvasionReplicatedProjectile(actor) || projectileExpired || InvasionReplicatedActors[i].ForceDeathDelta)
			{
				if (InvasionReplicatedActors[i].DeathDeltaSent)
					continue;

				// Send one final non-live packet so clients can play a local
				// projectile impact instead of leaving a stale missile sprite.
				Net_RecordInvasionDespawnEvent(InvasionReplicatedActors[i], actor, actor->health);
				InvasionReplicatedActors[i].DeathDeltaSent = true;
				if (projectileExpired)
					InvasionReplicatedActors[i].ForceDeathDelta = true;
			}
			else
			{
				InvasionReplicatedActors[i].DeathDeltaSent = false;
			}
		}
		else if (Net_IsInvasionActorCorpseLike(actor))
		{
			if (InvasionReplicatedActors[i].DeathDeltaSent)
				continue;

			// Keep a newly dead monster for one more packet so clients can
			// retire the mirror actor into a local corpse instead of deleting it.
			Net_RecordInvasionDespawnEvent(InvasionReplicatedActors[i], actor, actor->health);
			InvasionReplicatedActors[i].DeathDeltaSent = true;
		}
		else
		{
			InvasionReplicatedActors[i].DeathDeltaSent = false;
		}

		if (writeIdx != i)
			InvasionReplicatedActors[writeIdx] = InvasionReplicatedActors[i];
		++writeIdx;
	}

	if (writeIdx < InvasionReplicatedActors.Size())
		InvasionReplicatedActors.Resize(unsigned(writeIdx));
	Net_RebuildInvasionReplicatedActorIndexes();
	Net_CompactHCDEReplicatedActors();
	return int(writeIdx);
}

static FInvasionReplicatedActorRef* Net_FindInvasionReplicatedActor(uint32_t id)
{
	size_t index = 0u;
	if (Net_GetInvasionReplicatedActorIndex(id, index))
	{
		++HCDELiveProfile.InvasionActorIdLookupHits;
		return &InvasionReplicatedActors[index];
	}
	++HCDELiveProfile.InvasionActorIdLookupMisses;
	return nullptr;
}

static FInvasionReplicatedActorRef* Net_FindInvasionReplicatedActorByActor(const AActor* actor)
{
	size_t index = 0u;
	if (Net_GetInvasionReplicatedActorIndexByActor(actor, index))
	{
		++HCDELiveProfile.InvasionActorPtrLookupHits;
		return &InvasionReplicatedActors[index];
	}
	++HCDELiveProfile.InvasionActorPtrLookupMisses;
	return nullptr;
}

static void Net_ForceInvasionActorAction(const AActor* actor, uint8_t actionState)
{
	if (actor == nullptr
		|| (actionState != HCDEInvasionActorActionMelee
			&& actionState != HCDEInvasionActorActionMissile
			&& actionState != HCDEInvasionActorActionPain))
	{
		return;
	}

	if (auto ref = Net_FindInvasionReplicatedActorByActor(actor); ref != nullptr)
	{
		ref->ServerForcedActionState = actionState;
		ref->ServerForcedActionTic = gametic;
	}
}

bool Net_IsInvasionClientMirrorActor(const AActor* actor)
{
	if (actor == nullptr || I_IsLocalHCDEServiceAuthority())
		return false;

	return Net_FindInvasionReplicatedActorByActor(actor) != nullptr;
}

bool Net_IsInvasionClientMirrorBlockingActor(const AActor* actor)
{
	// Client mirrors are render/proxy state only. They are intentionally not
	// movement blockers because their visual interpolation can differ from the
	// server's authoritative actor position by enough to poison prediction.
	return false;
}

void Net_RecordInvasionActorAttack(AActor* attacker, AActor* target)
{
	if (!I_IsLocalHCDEServiceAuthority()
		|| !Net_IsInvasionModeEnabled()
		|| gamestate != GS_LEVEL
		|| attacker == nullptr
		|| target == nullptr
		|| attacker->health <= 0
		|| (attacker->flags3 & MF3_ISMONSTER) == 0
		|| Net_FindInvasionReplicatedActorByActor(attacker) == nullptr)
	{
		return;
	}

	uint8_t actionState = Net_GetInvasionActorActionState(attacker);
	if (actionState != HCDEInvasionActorActionMelee && actionState != HCDEInvasionActorActionMissile)
	{
		const double meleeRange = max<double>(attacker->meleerange, 0.0)
			+ attacker->radius
			+ target->radius
			+ 32.0;
		const bool likelyMelee = attacker->MeleeState != nullptr
			&& attacker->Distance3D(target) <= meleeRange;
		if (likelyMelee)
			actionState = HCDEInvasionActorActionMelee;
		else if (attacker->MissileState != nullptr)
			actionState = HCDEInvasionActorActionMissile;
		else if (attacker->MeleeState != nullptr)
			actionState = HCDEInvasionActorActionMelee;
	}

	Net_ForceInvasionActorAction(attacker, actionState);
}

static void Net_RegisterInvasionReplicatedActor(uint32_t id, AActor* actor)
{
	if (id == 0u || actor == nullptr)
		return;

	if (auto existing = Net_FindInvasionReplicatedActor(id); existing != nullptr)
	{
		const bool actorChanged = existing->Actor.Get() != actor;
		Net_SetInvasionReplicatedActorPtr(*existing, actor);
		existing->DeathDeltaSent = false;
		existing->ForceDeathDelta = false;
		existing->SimulationLastHealth = actor->health;
		if (actorChanged)
		{
			existing->LastAuthorityHealth = actor->health;
			existing->LastAuthorityEventHealth = actor->health;
			existing->LastAuthorityHealthEventTic = gametic;
		}
		if (Net_IsInvasionReplicatedProjectile(actor))
			existing->IsProjectile = true;
		Net_SeedInvasionMirrorVisualTarget(*existing, actor);
		return;
	}

	FInvasionReplicatedActorRef ref;
	ref.Id = id;
	ref.Actor = MakeObjPtr<AActor*>(actor);
	ref.IsProjectile = Net_IsInvasionReplicatedProjectile(actor);
	ref.SpawnTic = gametic;
	ref.SimulationLastHealth = actor->health;
	ref.LastAuthorityHealth = actor->health;
	ref.LastAuthorityEventHealth = actor->health;
	ref.LastAuthorityHealthEventTic = gametic;
	Net_SeedInvasionMirrorVisualTarget(ref, actor);
	InvasionReplicatedActors.Push(ref);
	DebugTrace::Infof("playsim.actor", "register invasion mirror id=%u class=%s projectile=%d solid=%d shootable=%d gametic=%d",
		unsigned(id), actor->GetClass()->TypeName.GetChars(), ref.IsProjectile ? 1 : 0,
		(actor->flags & MF_SOLID) != 0 ? 1 : 0,
		(actor->flags & MF_SHOOTABLE) != 0 ? 1 : 0,
		gametic);
	Net_IndexInvasionReplicatedActor(InvasionReplicatedActors.Size() - 1u);
	Net_RegisterHCDEReplicatedActor(id, actor,
		Net_ClassifyHCDEReplicatedActor(actor, ref.IsProjectile), HREP_SOURCE_INVASION);
}

static bool Net_InvasionDeltaVectorChanged(const DVector3& a, const DVector3& b, double epsilon)
{
	return fabs(a.X - b.X) > epsilon
		|| fabs(a.Y - b.Y) > epsilon
		|| fabs(a.Z - b.Z) > epsilon;
}

static void Net_ResetHCDEReplicatedActorBaseline(int clientNum)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return;

	for (auto& ref : HCDEReplicatedActors)
		ref.ClientState[clientNum] = {};
}

static const char* HCDEReplicatedActorSourceName(uint8_t source)
{
	switch (EHCDEReplicatedActorSource(source))
	{
	case HREP_SOURCE_INVASION:
		return "invasion";
	case HREP_SOURCE_COOP:
		return "coop";
	case HREP_SOURCE_DM:
		return "dm";
	default:
		return "shared";
	}
}

static const char* HCDEReplicatedActorCategoryName(uint8_t category)
{
	switch (EHCDEReplicatedActorCategory(category))
	{
	case HREP_ACTOR_PLAYER:
		return "player";
	case HREP_ACTOR_MONSTER:
		return "monster";
	case HREP_ACTOR_PROJECTILE:
		return "projectile";
	case HREP_ACTOR_PICKUP:
		return "pickup";
	case HREP_ACTOR_MAP:
		return "map";
	case HREP_ACTOR_SCRIPT:
		return "script";
	case HREP_ACTOR_VISUAL:
		return "visual";
	default:
		return "unknown";
	}
}

static bool Net_IsHCDEReplicatedScriptActor(const AActor* actor)
{
	if (actor == nullptr)
		return false;

	const int statNum = actor->GetStatNum();
	if (statNum == STAT_INVENTORY
		|| statNum == STAT_LIGHT
		|| statNum == STAT_LIGHTTRANSFER
		|| statNum == STAT_EARTHQUAKE
		|| statNum == STAT_MAPMARKER
		|| statNum == STAT_SCRIPTS
		|| statNum == STAT_DLIGHT
		|| statNum == STAT_SECTOREFFECT
		|| statNum == STAT_ACTORMOVER
		|| statNum == STAT_DECALTHINKER)
	{
		return false;
	}

	return statNum == STAT_DEFAULT
		|| (statNum >= STAT_USER && statNum <= STAT_USER_MAX)
		|| statNum == STAT_VISUALTHINKER;
}

static uint8_t Net_ClassifyHCDEReplicatedActor(const AActor* actor, bool invasionProjectile)
{
	if (actor == nullptr)
		return HREP_ACTOR_UNKNOWN;
	if (actor->player != nullptr)
		return HREP_ACTOR_PLAYER;
	if (invasionProjectile || (actor->flags & MF_MISSILE) != 0)
		return HREP_ACTOR_PROJECTILE;
	if ((actor->flags3 & MF3_ISMONSTER) != 0)
		return HREP_ACTOR_MONSTER;
	if ((actor->flags & MF_SPECIAL) != 0)
		return HREP_ACTOR_PICKUP;
	if ((actor->flags & (MF_SHOOTABLE | MF_SOLID)) != 0)
		return HREP_ACTOR_MAP;
	if (Net_IsHCDEReplicatedScriptActor(actor))
		return HREP_ACTOR_SCRIPT;
	return HREP_ACTOR_UNKNOWN;
}

static bool Net_ShouldMigrateHCDEModeActor(const AActor* actor, bool dmMode, uint8_t& category)
{
	category = HREP_ACTOR_UNKNOWN;
	if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		return false;
	if (actor->IsClientSide())
		return false;

	const bool projectile = Net_IsInvasionReplicatedProjectile(actor) || (actor->flags & MF_MISSILE) != 0;
	category = Net_ClassifyHCDEReplicatedActor(actor, projectile);
	if (category == HREP_ACTOR_UNKNOWN || category == HREP_ACTOR_VISUAL)
		return false;

	if (projectile)
		return true;
	if (category == HREP_ACTOR_SCRIPT)
	{
		++HCDELiveProfile.ModeMigrationScriptActorsSuppressed;
		return false;
	}
	if (dmMode)
		return category == HREP_ACTOR_PLAYER
			|| category == HREP_ACTOR_PICKUP
			|| category == HREP_ACTOR_MAP;
	if (actor->player != nullptr)
	{
		++HCDELiveProfile.ModeMigrationPlayerActorsSuppressed;
		return false;
	}
	return category == HREP_ACTOR_MONSTER
		|| category == HREP_ACTOR_PICKUP
		|| category == HREP_ACTOR_MAP;
}

static uint32_t Net_AllocateHCDEModeActorId()
{
	if (HCDEModeNextActorId < 0x80000000u)
		HCDEModeNextActorId = 0x80000000u;
	const uint32_t id = HCDEModeNextActorId++;
	if (HCDEModeNextActorId == 0u)
		HCDEModeNextActorId = 0x80000000u;
	return id;
}

static uint16_t Net_GetHCDEReplicatedActorClassId(const PClassActor* actorClass)
{
	if (actorClass == nullptr)
		return 0u;

	const unsigned int* stored = HCDEReplicatedActorClassIndex.CheckKey(actorClass);
	if (stored != nullptr)
		return uint16_t(*stored + 1u);

	if (HCDEReplicatedActorClasses.Size() >= UINT16_MAX)
		return 0u;

	const unsigned int index = HCDEReplicatedActorClasses.Push(actorClass);
	HCDEReplicatedActorClassIndex.Insert(actorClass, index);
	++HCDELiveProfile.SharedActorClassRegistered;
	return uint16_t(index + 1u);
}

static const PClassActor* Net_GetHCDEReplicatedActorClass(uint16_t classId)
{
	if (classId == 0u)
		return nullptr;
	const size_t index = size_t(classId - 1u);
	return index < HCDEReplicatedActorClasses.Size() ? HCDEReplicatedActorClasses[index] : nullptr;
}

static void Net_ClearHCDEReplicatedActorIndexes()
{
	HCDEReplicatedActorIdIndex.Clear();
	HCDEReplicatedActorPtrIndex.Clear();
}

static void Net_IndexHCDEReplicatedActor(size_t index)
{
	if (index >= HCDEReplicatedActors.Size())
		return;

	const auto& ref = HCDEReplicatedActors[index];
	if (ref.Id != 0u)
		HCDEReplicatedActorIdIndex.Insert(ref.Id, unsigned(index));
	const AActor* actor = ref.Actor.Get();
	if (actor != nullptr)
		HCDEReplicatedActorPtrIndex.Insert(actor, unsigned(index));
}

static void Net_RebuildHCDEReplicatedActorIndexes()
{
	Net_ClearHCDEReplicatedActorIndexes();
	for (size_t i = 0u; i < HCDEReplicatedActors.Size(); ++i)
	{
		Net_IndexHCDEReplicatedActor(i);
	}
}

static bool Net_GetHCDEReplicatedActorIndex(uint32_t id, size_t& index)
{
	if (id == 0u)
		return false;

	const unsigned int* stored = HCDEReplicatedActorIdIndex.CheckKey(id);
	if (stored == nullptr)
		return false;

	const size_t candidate = size_t(*stored);
	if (candidate >= HCDEReplicatedActors.Size() || HCDEReplicatedActors[candidate].Id != id)
	{
		HCDEReplicatedActorIdIndex.Remove(id);
		return false;
	}

	index = candidate;
	return true;
}

static bool Net_GetHCDEReplicatedActorIndexByActor(const AActor* actor, size_t& index)
{
	if (actor == nullptr)
		return false;

	const unsigned int* stored = HCDEReplicatedActorPtrIndex.CheckKey(actor);
	if (stored == nullptr)
		return false;

	const size_t candidate = size_t(*stored);
	if (candidate >= HCDEReplicatedActors.Size() || HCDEReplicatedActors[candidate].Actor != actor)
	{
		HCDEReplicatedActorPtrIndex.Remove(actor);
		return false;
	}

	index = candidate;
	return true;
}

static FHCDEReplicatedActorRef* Net_FindHCDEReplicatedActor(uint32_t id)
{
	size_t index = 0u;
	if (Net_GetHCDEReplicatedActorIndex(id, index))
	{
		++HCDELiveProfile.SharedActorIdLookupHits;
		return &HCDEReplicatedActors[index];
	}
	++HCDELiveProfile.SharedActorIdLookupMisses;
	return nullptr;
}

static FHCDEReplicatedActorRef* Net_FindHCDEReplicatedActorByActor(const AActor* actor)
{
	size_t index = 0u;
	if (Net_GetHCDEReplicatedActorIndexByActor(actor, index))
	{
		++HCDELiveProfile.SharedActorPtrLookupHits;
		return &HCDEReplicatedActors[index];
	}
	++HCDELiveProfile.SharedActorPtrLookupMisses;
	return nullptr;
}

static void Net_SetHCDEReplicatedActorPtr(FHCDEReplicatedActorRef& ref, AActor* actor)
{
	const AActor* oldActor = ref.Actor.Get();
	if (oldActor != nullptr)
		HCDEReplicatedActorPtrIndex.Remove(oldActor);
	ref.Actor = MakeObjPtr<AActor*>(actor);
	if (actor != nullptr)
	{
		size_t index = 0u;
		if (Net_GetHCDEReplicatedActorIndex(ref.Id, index))
			HCDEReplicatedActorPtrIndex.Insert(static_cast<const AActor*>(actor), unsigned(index));
	}
}

static bool Net_IsHCDEAuthorityPickupSource(uint8_t source)
{
	return source == HREP_SOURCE_COOP || source == HREP_SOURCE_DM || source == HREP_SOURCE_SHARED;
}

static bool Net_ShouldRecordHCDEPickupSpawnEvent(uint32_t id, const AActor* actor, uint8_t category, uint8_t source)
{
	return I_IsLocalHCDEServiceAuthority()
		&& actor != nullptr
		&& id != 0u
		&& category == HREP_ACTOR_PICKUP
		&& Net_IsHCDEAuthorityPickupSource(source)
		&& !Net_IsInvasionModeEnabled();
}

static void Net_RecordHCDEPickupSpawnEvent(uint32_t id, AActor* actor, uint8_t category, uint8_t source, uint16_t classId)
{
	if (!Net_ShouldRecordHCDEPickupSpawnEvent(id, actor, category, source)
		|| actor->GetClass() == nullptr)
	{
		return;
	}

	FHCDEAuthorityEvent event;
	event.EventType = HCDEAuthorityEventSpawn;
	event.Source = source;
	event.Category = category;
	event.ActorFlags = HCDEActorDeltaFlagLive;
	event.ClassId = classId != 0u ? classId : Net_GetHCDEReplicatedActorClassId(actor->GetClass());
	event.EventSeq = InvasionNextAuthorityEventSeq++;
	if (InvasionNextAuthorityEventSeq == 0u)
		InvasionNextAuthorityEventSeq = 1u;
	event.Id = id;
	event.Tic = gametic;
	event.Wave = 0;
	event.ClassName = actor->GetClass()->TypeName.GetChars();
	event.Pos = actor->Pos();
	event.Yaw = actor->Angles.Yaw;
	event.Health = actor->health;
	HCDEPushRecentAuthorityEvent(event);

	DebugTrace::Markf("net", "HCDE authority pickup spawn id=%u seq=%u source=%s class=%s pos=(%.1f,%.1f,%.1f)",
		unsigned(event.Id),
		unsigned(event.EventSeq),
		HCDEReplicatedActorSourceName(event.Source),
		event.ClassName.GetChars(),
		event.Pos.X,
		event.Pos.Y,
		event.Pos.Z);
}

static void Net_RegisterHCDEReplicatedActor(uint32_t id, AActor* actor, uint8_t category, uint8_t source)
{
	if (id == 0u || actor == nullptr)
		return;

	if (auto byActor = Net_FindHCDEReplicatedActorByActor(actor); byActor != nullptr && byActor->Id != id)
		Net_RetireHCDEReplicatedActor(byActor->Id);

	const uint16_t classId = Net_GetHCDEReplicatedActorClassId(actor->GetClass());
	if (auto existing = Net_FindHCDEReplicatedActor(id); existing != nullptr)
	{
		const bool wasMissingOrRetired = existing->Actor == nullptr || existing->Retired;
		Net_SetHCDEReplicatedActorPtr(*existing, actor);
		existing->ClassId = classId;
		existing->Category = category;
		existing->Source = source;
		existing->Active = true;
		existing->Retired = false;
		existing->RetireTic = 0;
		existing->LastTouchedTic = gametic;
		if (wasMissingOrRetired)
			Net_RecordHCDEPickupSpawnEvent(id, actor, category, source, classId);
		++HCDELiveProfile.SharedActorUpdated;
		return;
	}

	FHCDEReplicatedActorRef ref;
	ref.Id = id;
	ref.Actor = MakeObjPtr<AActor*>(actor);
	ref.ClassId = classId;
	ref.Category = category;
	ref.Source = source;
	ref.Active = true;
	ref.SpawnTic = gametic;
	ref.LastTouchedTic = gametic;
	HCDEReplicatedActors.Push(ref);
	Net_IndexHCDEReplicatedActor(HCDEReplicatedActors.Size() - 1u);
	Net_RecordHCDEPickupSpawnEvent(id, actor, category, source, classId);
	++HCDELiveProfile.SharedActorRegistered;
}

static FHCDEReplicatedActorRef* Net_RegisterHCDEReplicatedActorBaseline(uint32_t id, uint16_t classId, uint8_t category, uint8_t source)
{
	if (id == 0u || classId == 0u || category == HREP_ACTOR_UNKNOWN || category > HREP_ACTOR_VISUAL)
		return Net_FindHCDEReplicatedActor(id);

	if (auto existing = Net_FindHCDEReplicatedActor(id); existing != nullptr)
	{
		existing->ClassId = classId;
		existing->Category = category;
		existing->Source = source;
		existing->Active = true;
		existing->Retired = false;
		existing->RetireTic = 0;
		existing->LastTouchedTic = gametic;
		++HCDELiveProfile.SharedActorUpdated;
		return existing;
	}

	FHCDEReplicatedActorRef ref;
	ref.Id = id;
	ref.ClassId = classId;
	ref.Category = category;
	ref.Source = source;
	ref.Active = true;
	ref.SpawnTic = gametic;
	ref.LastTouchedTic = gametic;
	HCDEReplicatedActors.Push(ref);
	Net_IndexHCDEReplicatedActor(HCDEReplicatedActors.Size() - 1u);
	++HCDELiveProfile.SharedActorRegistered;
	return &HCDEReplicatedActors[HCDEReplicatedActors.Size() - 1u];
}

static void Net_RetireHCDEReplicatedActor(uint32_t id)
{
	if (auto ref = Net_FindHCDEReplicatedActor(id); ref != nullptr)
	{
		Net_SetHCDEReplicatedActorPtr(*ref, nullptr);
		ref->Active = false;
		ref->Retired = true;
		ref->RetireTic = gametic;
		ref->LastTouchedTic = gametic;
		++HCDELiveProfile.SharedActorRetired;
	}
}

static bool Net_ShouldRecordHCDEPickupRetireEvent(const FHCDEReplicatedActorRef& ref, const AActor* actor)
{
	return I_IsLocalHCDEServiceAuthority()
		&& actor != nullptr
		&& ref.Id != 0u
		&& !ref.Retired
		&& ref.Category == HREP_ACTOR_PICKUP
		&& Net_IsHCDEAuthorityPickupSource(ref.Source)
		&& !Net_IsInvasionModeEnabled();
}

static void Net_RecordHCDEPickupRetireEvent(const FHCDEReplicatedActorRef& ref, AActor* actor)
{
	if (!Net_ShouldRecordHCDEPickupRetireEvent(ref, actor))
		return;

	FHCDEAuthorityEvent event;
	event.EventType = HCDEAuthorityEventDespawn;
	event.Source = ref.Source;
	event.Category = ref.Category;
	event.ActorFlags = 0u;
	event.ClassId = ref.ClassId != 0u ? ref.ClassId : Net_GetHCDEReplicatedActorClassId(actor->GetClass());
	event.EventSeq = InvasionNextAuthorityEventSeq++;
	if (InvasionNextAuthorityEventSeq == 0u)
		InvasionNextAuthorityEventSeq = 1u;
	event.Id = ref.Id;
	event.Tic = gametic;
	event.Wave = 0;
	if (actor->GetClass() != nullptr)
		event.ClassName = actor->GetClass()->TypeName.GetChars();
	event.Pos = actor->Pos();
	event.Yaw = actor->Angles.Yaw;
	event.Health = actor->health;
	HCDEPushRecentAuthorityEvent(event);

	DebugTrace::Markf("net", "HCDE authority pickup retire id=%u seq=%u source=%s class=%s pos=(%.1f,%.1f,%.1f)",
		unsigned(event.Id),
		unsigned(event.EventSeq),
		HCDEReplicatedActorSourceName(event.Source),
		event.ClassName.IsNotEmpty() ? event.ClassName.GetChars() : "<unknown>",
		event.Pos.X,
		event.Pos.Y,
		event.Pos.Z);
}

static int Net_CompactHCDEReplicatedActors()
{
	size_t writeIdx = 0u;
	int removed = 0;
	for (size_t i = 0u; i < HCDEReplicatedActors.Size(); ++i)
	{
		AActor* actor = HCDEReplicatedActors[i].Actor;
		const bool staleActor = actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0;
		if (staleActor && Net_ShouldRecordHCDEPickupRetireEvent(HCDEReplicatedActors[i], actor))
		{
			Net_RecordHCDEPickupRetireEvent(HCDEReplicatedActors[i], actor);
			Net_SetHCDEReplicatedActorPtr(HCDEReplicatedActors[i], nullptr);
			HCDEReplicatedActors[i].Active = false;
			HCDEReplicatedActors[i].Retired = true;
			HCDEReplicatedActors[i].RetireTic = gametic;
			HCDEReplicatedActors[i].LastTouchedTic = gametic;
			++HCDELiveProfile.SharedActorRetired;
		}
		const bool liveRemoteBaseline = actor == nullptr
			&& HCDEReplicatedActors[i].Active
			&& !HCDEReplicatedActors[i].Retired
			&& (HCDEReplicatedActors[i].Source == HREP_SOURCE_SHARED
				|| HCDEReplicatedActors[i].Source == HREP_SOURCE_COOP
				|| HCDEReplicatedActors[i].Source == HREP_SOURCE_DM)
			&& HCDEReplicatedActors[i].LastTouchedTic > 0
			&& gametic - HCDEReplicatedActors[i].LastTouchedTic <= TICRATE * 10;
		const bool retireExpired = HCDEReplicatedActors[i].Retired
			&& HCDEReplicatedActors[i].RetireTic > 0
			&& gametic - HCDEReplicatedActors[i].RetireTic > TICRATE * 2;
		if (HCDEReplicatedActors[i].Id == 0u || retireExpired || (staleActor && !HCDEReplicatedActors[i].Retired && !liveRemoteBaseline))
		{
			++removed;
			continue;
		}

		if (staleActor && !liveRemoteBaseline)
			Net_SetHCDEReplicatedActorPtr(HCDEReplicatedActors[i], nullptr);
		if (writeIdx != i)
			HCDEReplicatedActors[writeIdx] = HCDEReplicatedActors[i];
		++writeIdx;
	}

	if (writeIdx < HCDEReplicatedActors.Size())
		HCDEReplicatedActors.Resize(unsigned(writeIdx));
	Net_RebuildHCDEReplicatedActorIndexes();
	HCDELiveProfile.SharedActorCompacted += uint64_t(max<int>(removed, 0));
	return removed;
}

static void Net_ClearHCDEReplicatedActors()
{
	HCDEReplicatedActors.Clear();
	Net_ClearHCDEReplicatedActorIndexes();
	HCDEReplicatedActorClasses.Clear();
	HCDEReplicatedActorClassIndex.Clear();
	HCDEModeNextActorId = 0x80000000u;
	HCDEModeMigrationNextScanTic = 0;
	for (int client = 0; client < MAXPLAYERS; ++client)
		HCDEActorDeltaV2SendCursor[client] = 0u;
}

static uint32_t HCDEFirstRecentAuthorityEventId()
{
	for (const auto& event : HCDERecentAuthorityEvents)
	{
		if (event.EventSeq != 0u)
			return event.EventSeq;
	}
	return 0u;
}

static void HCDEClearActorBaselineRepair(int clientNum, const char* reason)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return;

	if (HCDEActorBaselineRepairUntilTic[clientNum] > 0 || HCDEAuthorityEventReplayNextId[clientNum] != 0u)
	{
		DebugTrace::Markf("net", "HCDE baseline repair clear client=%d room=%u gametic=%d reason=%s",
			clientNum, unsigned(CurrentRoomID), gametic, reason != nullptr ? reason : "unknown");
	}
	HCDEActorBaselineRepairUntilTic[clientNum] = 0;
	HCDEAuthorityEventReplayNextId[clientNum] = 0u;
}

static void HCDEBeginActorBaselineRepair(int clientNum, const char* reason)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return;

	Net_ResetHCDEReplicatedActorBaseline(clientNum);
	HCDEInvasionActorDeltaV2SendCursor[clientNum] = 0u;
	HCDEActorDeltaV2SendCursor[clientNum] = 0u;
	HCDEActorBaselineRepairUntilTic[clientNum] = max<int>(HCDEActorBaselineRepairUntilTic[clientNum],
		gametic + HCDEActorBaselineRepairWindowTics);
	HCDEAuthorityEventReplayNextId[clientNum] = HCDEFirstRecentAuthorityEventId();
	++HCDELiveProfile.ActorBaselineRepairWindows;
	++HCDELiveProfile.ActorBaselineRepairResets;
	++HCDELivePeers[clientNum].ActorBaselineRepairWindows;
	++HCDELivePeers[clientNum].ActorBaselineRepairResets;
	DebugTrace::Markf("net", "HCDE baseline repair begin client=%d room=%u gametic=%d until=%d authority-replay-next=%u reason=%s",
		clientNum, unsigned(CurrentRoomID), gametic, HCDEActorBaselineRepairUntilTic[clientNum],
		unsigned(HCDEAuthorityEventReplayNextId[clientNum]), reason != nullptr ? reason : "unknown");
}

static void Net_MigrateHCDEModeActor(AActor* actor, uint8_t category, uint8_t source, uint32_t& registered)
{
	if (actor == nullptr)
		return;

	if (auto existing = Net_FindHCDEReplicatedActorByActor(actor); existing != nullptr)
	{
		Net_RegisterHCDEReplicatedActor(existing->Id, actor, category, source);
		++registered;
		return;
	}

	Net_RegisterHCDEReplicatedActor(Net_AllocateHCDEModeActorId(), actor, category, source);
	++registered;
}

static void Net_TickHCDEModeActorMigration()
{
	HCDEModeMigrationLastConsidered = 0u;
	HCDEModeMigrationLastRegistered = 0u;
	HCDEModeMigrationLastInvasion = 0u;
	HCDEModeMigrationLastCoop = 0u;
	HCDEModeMigrationLastDM = 0u;
	if (!I_IsLocalHCDEServiceAuthority() || gamestate != GS_LEVEL || primaryLevel == nullptr)
		return;

	if (gametic < HCDEModeMigrationNextScanTic)
		return;
	HCDEModeMigrationNextScanTic = gametic + TICRATE;
	++HCDELiveProfile.ModeMigrationScans;

	if (Net_IsInvasionModeEnabled())
	{
		for (auto& ref : InvasionReplicatedActors)
		{
			AActor* actor = ref.Actor.Get();
			if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
				continue;
			++HCDEModeMigrationLastConsidered;
			Net_RegisterHCDEReplicatedActor(ref.Id, actor,
				Net_ClassifyHCDEReplicatedActor(actor, ref.IsProjectile), HREP_SOURCE_INVASION);
			++HCDEModeMigrationLastRegistered;
			++HCDEModeMigrationLastInvasion;
		}
	}
	else if (netgame || multiplayer)
	{
		const bool dmMode = deathmatch != 0;
		auto iterator = primaryLevel->GetThinkerIterator<AActor>();
		while (AActor* actor = iterator.Next())
		{
			++HCDEModeMigrationLastConsidered;
			uint8_t category = HREP_ACTOR_UNKNOWN;
			if (!Net_ShouldMigrateHCDEModeActor(actor, dmMode, category))
				continue;
			Net_MigrateHCDEModeActor(actor, category, dmMode ? HREP_SOURCE_DM : HREP_SOURCE_COOP, HCDEModeMigrationLastRegistered);
			if (dmMode)
				++HCDEModeMigrationLastDM;
			else
				++HCDEModeMigrationLastCoop;
		}
	}

	HCDELiveProfile.ModeMigrationActorsConsidered += HCDEModeMigrationLastConsidered;
	HCDELiveProfile.ModeMigrationActorsRegistered += HCDEModeMigrationLastRegistered;
	HCDELiveProfile.ModeMigrationInvasionActive += HCDEModeMigrationLastInvasion;
	HCDELiveProfile.ModeMigrationCoopActive += HCDEModeMigrationLastCoop;
	HCDELiveProfile.ModeMigrationDMActive += HCDEModeMigrationLastDM;
	Net_CompactHCDEReplicatedActors();
}

static bool HCDEAuthorityEventSuperseded(size_t index, size_t eventCount)
{
	if (index >= eventCount)
		return false;

	const auto& event = HCDERecentAuthorityEvents[index];
	if (event.Id == 0u)
		return false;

	const bool damageEvent = event.EventType == HCDEAuthorityEventDamage;
	const bool pickupEvent = event.Category == HREP_ACTOR_PICKUP
		&& (event.EventType == HCDEAuthorityEventSpawn || event.EventType == HCDEAuthorityEventDespawn)
		&& Net_IsHCDEAuthorityPickupSource(event.Source);
	if (!damageEvent && !pickupEvent)
		return false;

	for (size_t next = index + 1u; next < eventCount; ++next)
	{
		const auto& later = HCDERecentAuthorityEvents[next];
		if (later.Id != event.Id)
			continue;
		if (damageEvent
			&& (later.EventType == HCDEAuthorityEventDamage
				|| later.EventType == HCDEAuthorityEventDespawn))
		{
			return true;
		}
		if (pickupEvent
			&& later.Category == HREP_ACTOR_PICKUP
			&& Net_IsHCDEAuthorityPickupSource(later.Source)
			&& (later.EventType == HCDEAuthorityEventSpawn
				|| later.EventType == HCDEAuthorityEventDespawn))
		{
			return true;
		}
	}
	return false;
}

static void HCDEProfileRecordAuthorityEventBuilt(uint8_t eventType, uint8_t source, uint8_t category)
{
	const bool pickupEvent = category == HREP_ACTOR_PICKUP && Net_IsHCDEAuthorityPickupSource(source);
	if (eventType == HCDEAuthorityEventSpawn && pickupEvent)
		++HCDELiveProfile.AuthorityEventPickupSpawnRecordsBuilt;
	else if (eventType == HCDEAuthorityEventDespawn && pickupEvent)
		++HCDELiveProfile.AuthorityEventPickupRetireRecordsBuilt;
	else if (eventType == HCDEAuthorityEventSpawn)
		++HCDELiveProfile.AuthorityEventSpawnRecordsBuilt;
	else if (eventType == HCDEAuthorityEventDamage)
		++HCDELiveProfile.AuthorityEventDamageRecordsBuilt;
	else if (eventType == HCDEAuthorityEventDespawn)
		++HCDELiveProfile.AuthorityEventDespawnRecordsBuilt;
}

static void HCDEProfileRecordAuthorityEventReceived(uint8_t eventType, uint8_t source, uint8_t category)
{
	const bool pickupEvent = category == HREP_ACTOR_PICKUP && Net_IsHCDEAuthorityPickupSource(source);
	if (eventType == HCDEAuthorityEventSpawn && pickupEvent)
		++HCDELiveProfile.AuthorityEventPickupSpawnRecordsReceived;
	else if (eventType == HCDEAuthorityEventDespawn && pickupEvent)
		++HCDELiveProfile.AuthorityEventPickupRetireRecordsReceived;
	else if (eventType == HCDEAuthorityEventSpawn)
		++HCDELiveProfile.AuthorityEventSpawnRecordsReceived;
	else if (eventType == HCDEAuthorityEventDamage)
		++HCDELiveProfile.AuthorityEventDamageRecordsReceived;
	else if (eventType == HCDEAuthorityEventDespawn)
		++HCDELiveProfile.AuthorityEventDespawnRecordsReceived;
}

static bool HCDEAppendAuthorityEvents(int clientNum, uint8_t* output, size_t outputCapacity, size_t& cursor)
{
	if (!HCDEIsValidLiveClient(clientNum))
		return false;
	if (!HCDELivePeerHasCapability(clientNum, HCDELiveCapAuthorityEventsV1))
		return true;

	const size_t eventCount = HCDERecentAuthorityEvents.Size();
	if (eventCount == 0u)
		return true;

	const size_t startCursor = cursor;
	if (cursor > outputCapacity || outputCapacity - cursor < HCDEAuthorityEventsHeaderSize)
	{
		++HCDELiveProfile.AuthorityEventRecordsDeferred;
		HCDERecordLiveLaneDeferred(HLANE_AUTHORITY, clientNum);
		return true;
	}

	if (!HCDEAppendBytes(output, outputCapacity, cursor, HCDEAuthorityEventsMagic, sizeof(HCDEAuthorityEventsMagic))
		|| !HCDEAppendByte(output, outputCapacity, cursor, HCDEAuthorityEventsProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u))
	{
		cursor = startCursor;
		return false;
	}

	const size_t countOffset = cursor;
	if (!HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u))
	{
		cursor = startCursor;
		return false;
	}

	auto nextEventIdAfter = [&](size_t index) -> uint32_t
	{
		for (size_t next = index + 1u; next < eventCount; ++next)
		{
			if (HCDERecentAuthorityEvents[next].EventSeq != 0u)
				return HCDERecentAuthorityEvents[next].EventSeq;
		}
		return 0u;
	};

	uint8_t count = 0u;
	const bool catchupActive = HCDEAuthorityEventReplayNextId[clientNum] != 0u;
	size_t start = 0u;
	if (catchupActive)
	{
		start = eventCount;
		for (size_t i = 0u; i < eventCount; ++i)
		{
			if (HCDERecentAuthorityEvents[i].EventSeq >= HCDEAuthorityEventReplayNextId[clientNum])
			{
				start = i;
				break;
			}
		}
		if (start == eventCount)
			HCDEAuthorityEventReplayNextId[clientNum] = 0u;
	}
	else
	{
		const size_t replayLimit = min<size_t>(HCDEAuthorityEventReplayLimit, HCDEAuthorityEventPacketLimit);
		start = eventCount > replayLimit ? eventCount - replayLimit : 0u;
	}

	uint32_t nextCatchupId = catchupActive ? HCDEAuthorityEventReplayNextId[clientNum] : 0u;
	for (size_t i = start; i < eventCount && count < UINT8_MAX && count < HCDEAuthorityEventPacketLimit; ++i)
	{
		const auto& event = HCDERecentAuthorityEvents[i];
		const char* className = event.ClassName.GetChars();
		const size_t classNameLen = className != nullptr ? strlen(className) : 0u;
		const bool spawnEvent = event.EventType == HCDEAuthorityEventSpawn;
		const bool despawnEvent = event.EventType == HCDEAuthorityEventDespawn;
		const bool damageEvent = event.EventType == HCDEAuthorityEventDamage;
		const bool supersededEvent = HCDEAuthorityEventSuperseded(i, eventCount);
		if (event.Id == 0u
			|| event.EventSeq == 0u
			|| (!spawnEvent && !despawnEvent && !damageEvent)
			|| (spawnEvent && classNameLen == 0u)
			|| supersededEvent
			|| classNameLen > UINT8_MAX)
		{
			if (supersededEvent)
				++HCDELiveProfile.AuthorityEventRecordsSuperseded;
			if (catchupActive)
				nextCatchupId = nextEventIdAfter(i);
			continue;
		}

		const FHCDEReplicatedActorRef* sharedRef = Net_FindHCDEReplicatedActor(event.Id);
		const uint16_t classId = sharedRef != nullptr ? sharedRef->ClassId : event.ClassId;
		const uint8_t category = sharedRef != nullptr
			? sharedRef->Category
			: (event.Category <= HREP_ACTOR_VISUAL ? event.Category : HREP_ACTOR_MONSTER);
		const uint8_t source = sharedRef != nullptr
			? sharedRef->Source
			: (event.Source <= HREP_SOURCE_DM ? event.Source : HREP_SOURCE_INVASION);
		const uint8_t actorFlags = despawnEvent ? 0u : (sharedRef != nullptr ? sharedRef->Flags : event.ActorFlags);
		constexpr size_t fixedRecordBytes = 1u + 1u + 1u + 1u + 4u + 4u + 2u + 2u + 2u + 1u + 6u * sizeof(double) + 4u + 4u;
		const size_t recordBytes = fixedRecordBytes + classNameLen;
		const size_t actorDeltaReserve = !HCDELivePeerHasCapability(clientNum, HCDELiveCapLaneBudgetsV1) && InvasionReplicatedActors.Size() > 0
			? HCDEAuthorityEventActorDeltaReserveBytes
			: 0u;
		if (cursor > outputCapacity
			|| outputCapacity - cursor < recordBytes
			|| outputCapacity - cursor - recordBytes < actorDeltaReserve)
		{
			++HCDELiveProfile.AuthorityEventRecordsDeferred;
			HCDERecordLiveLaneDeferred(HLANE_AUTHORITY, clientNum);
			if (catchupActive && count > 0u)
				nextCatchupId = event.EventSeq;
			break;
		}

		const size_t recordStart = cursor;
		if (!HCDEAppendByte(output, outputCapacity, cursor, event.EventType)
			|| !HCDEAppendByte(output, outputCapacity, cursor, source)
			|| !HCDEAppendByte(output, outputCapacity, cursor, category)
			|| !HCDEAppendByte(output, outputCapacity, cursor, actorFlags)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, event.Id)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max<int>(event.Tic, 0)))
			|| !HCDEAppendBE16(output, outputCapacity, cursor, classId)
			|| !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(event.Health, INT16_MIN, INT16_MAX)))
			|| !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(event.Wave, 0, UINT16_MAX)))
			|| !HCDEAppendByte(output, outputCapacity, cursor, uint8_t(classNameLen))
			|| !HCDEAppendBytes(output, outputCapacity, cursor, reinterpret_cast<const uint8_t*>(className), classNameLen)
			|| !HCDEAppendDouble(output, outputCapacity, cursor, event.Pos.X)
			|| !HCDEAppendDouble(output, outputCapacity, cursor, event.Pos.Y)
			|| !HCDEAppendDouble(output, outputCapacity, cursor, event.Pos.Z)
			|| !HCDEAppendDouble(output, outputCapacity, cursor, event.Vel.X)
			|| !HCDEAppendDouble(output, outputCapacity, cursor, event.Vel.Y)
			|| !HCDEAppendDouble(output, outputCapacity, cursor, event.Vel.Z)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, event.Yaw.BAMs())
			|| !HCDEAppendBE32(output, outputCapacity, cursor, 0u))
		{
			cursor = recordStart;
			break;
		}

		++count;
		HCDEProfileRecordAuthorityEventBuilt(event.EventType, source, category);
		if (catchupActive)
			nextCatchupId = nextEventIdAfter(i);
	}

	if (catchupActive && count > 0u)
	{
		HCDEAuthorityEventReplayNextId[clientNum] = nextCatchupId;
		HCDELiveProfile.AuthorityEventCatchupRecordsBuilt += count;
		HCDELivePeers[clientNum].AuthorityEventCatchupRecords += count;
		if (nextCatchupId == 0u)
		{
			++HCDELiveProfile.AuthorityEventCatchupWindowsCompleted;
			DebugTrace::Markf("net", "HCDE authority catchup complete client=%d count=%u history=%zu",
				clientNum, unsigned(count), eventCount);
		}
	}

	if (count == 0u)
	{
		cursor = startCursor;
		return true;
	}

	output[countOffset] = count;
	++HCDELiveProfile.AuthorityEventPacketsBuilt;
	HCDELiveProfile.AuthorityEventRecordsBuilt += count;
	HCDELiveProfile.AuthorityEventBytesBuilt += cursor - startCursor;
	HCDERecordLiveLaneTx(HLANE_AUTHORITY, clientNum, cursor - startCursor);
	DebugTrace::Markf("net", "HCDE authority events send client=%d count=%u history=%zu catchup=%d next=%u bytes=%zu",
		clientNum, unsigned(count), eventCount, catchupActive ? 1 : 0,
		unsigned(HCDEAuthorityEventReplayNextId[clientNum]), cursor - startCursor);
	return true;
}

static AActor* Net_FindLocalHCDEPickupForAuthorityEvent(uint32_t actorId, uint8_t category,
	const FString& className, const DVector3& pos)
{
	if (category != HREP_ACTOR_PICKUP || primaryLevel == nullptr)
		return nullptr;

	if (auto* ref = Net_FindHCDEReplicatedActor(actorId); ref != nullptr && ref->Actor != nullptr)
		return ref->Actor.Get();

	if (className.IsEmpty())
		return nullptr;

	PClassActor* actorClass = PClass::FindActor(className.GetChars());
	if (actorClass == nullptr)
		return nullptr;

	AActor* bestActor = nullptr;
	double bestDistSq = 16.0 * 16.0;
	auto iterator = primaryLevel->GetThinkerIterator<AActor>(actorClass->TypeName);
	while (AActor* actor = iterator.Next())
	{
		if (actor == nullptr
			|| (actor->ObjectFlags & OF_EuthanizeMe) != 0
			|| (actor->flags & MF_SPECIAL) == 0
			|| !actor->IsA(actorClass))
		{
			continue;
		}

		const DVector3 delta = actor->Pos() - pos;
		const double distSq = delta.LengthSquared();
		if (distSq < bestDistSq)
		{
			bestDistSq = distSq;
			bestActor = actor;
		}
	}
	return bestActor;
}

static bool Net_ApplyHCDEPickupRetireEvent(uint32_t actorId, uint16_t classId, uint8_t category,
	uint8_t source, const FString& className, const DVector3& pos)
{
	if (I_IsLocalHCDEServiceAuthority())
		return true;
	if (actorId == 0u || category != HREP_ACTOR_PICKUP)
		return false;
	if (!Net_IsHCDEAuthorityPickupSource(source))
		return false;
	if (primaryLevel == nullptr || gamestate != GS_LEVEL || NetworkEntityManager::IsPredicting())
		return true;

	FHCDEReplicatedActorRef* ref = Net_FindHCDEReplicatedActor(actorId);
	if (ref == nullptr && classId != 0u)
		ref = Net_RegisterHCDEReplicatedActorBaseline(actorId, classId, category, source);

	AActor* actor = Net_FindLocalHCDEPickupForAuthorityEvent(actorId, category, className, pos);
	if (actor != nullptr)
	{
		if (ref != nullptr)
			Net_SetHCDEReplicatedActorPtr(*ref, actor);
		actor->ClearCounters();
		actor->Destroy();
	}

	if (ref != nullptr)
	{
		Net_SetHCDEReplicatedActorPtr(*ref, nullptr);
		ref->Active = false;
		ref->Retired = true;
		ref->RetireTic = gametic;
		ref->LastTouchedTic = gametic;
		ref->Category = category;
		ref->Source = source;
		if (classId != 0u)
			ref->ClassId = classId;
	}

	DebugTrace::Markf("net", "HCDE authority pickup retire apply id=%u source=%s class=%s found=%d",
		unsigned(actorId),
		HCDEReplicatedActorSourceName(source),
		className.IsNotEmpty() ? className.GetChars() : "<unknown>",
		actor != nullptr ? 1 : 0);
	return true;
}

static bool Net_ApplyHCDEPickupSpawnEvent(uint32_t actorId, uint16_t classId, uint8_t category,
	uint8_t source, const FString& className, const DVector3& pos, DAngle yaw, int health)
{
	if (I_IsLocalHCDEServiceAuthority())
		return true;
	if (actorId == 0u || category != HREP_ACTOR_PICKUP)
		return false;
	if (!Net_IsHCDEAuthorityPickupSource(source))
		return false;
	if (primaryLevel == nullptr || gamestate != GS_LEVEL || NetworkEntityManager::IsPredicting())
		return true;

	FHCDEReplicatedActorRef* ref = Net_FindHCDEReplicatedActor(actorId);
	AActor* actor = Net_FindLocalHCDEPickupForAuthorityEvent(actorId, category, className, pos);
	if (actor == nullptr)
	{
		PClassActor* actorClass = className.IsNotEmpty()
			? PClass::FindActor(className.GetChars())
			: const_cast<PClassActor*>(Net_GetHCDEReplicatedActorClass(classId));
		if (actorClass == nullptr)
		{
			if (ref == nullptr && classId != 0u)
				Net_RegisterHCDEReplicatedActorBaseline(actorId, classId, category, source);
			DebugTrace::Markf("net", "HCDE authority pickup spawn skipped id=%u source=%s class=%s reason=missing-class",
				unsigned(actorId),
				HCDEReplicatedActorSourceName(source),
				className.IsNotEmpty() ? className.GetChars() : "<unknown>");
			return true;
		}

		actor = Spawn(primaryLevel, actorClass, pos, ALLOW_REPLACE);
		if (actor != nullptr)
		{
			actor->Angles.Yaw = yaw;
			if (health > 0)
				actor->health = health;
			actor->ClearInterpolation();
		}
	}

	if (actor != nullptr)
	{
		Net_RegisterHCDEReplicatedActor(actorId, actor, category, source);
		ref = Net_FindHCDEReplicatedActor(actorId);
	}
	else if (ref == nullptr && classId != 0u)
	{
		ref = Net_RegisterHCDEReplicatedActorBaseline(actorId, classId, category, source);
	}

	if (ref != nullptr)
	{
		ref->Active = true;
		ref->Retired = false;
		ref->RetireTic = 0;
		ref->LastTouchedTic = gametic;
		ref->Category = category;
		ref->Source = source;
		if (classId != 0u)
			ref->ClassId = classId;
	}

	DebugTrace::Markf("net", "HCDE authority pickup spawn apply id=%u source=%s class=%s found=%d",
		unsigned(actorId),
		HCDEReplicatedActorSourceName(source),
		className.IsNotEmpty() ? className.GetChars() : "<unknown>",
		actor != nullptr ? 1 : 0);
	return true;
}

static bool HCDEApplyAuthorityEvents(int clientNum, const uint8_t* body, size_t bodyBytes, size_t& bodyCursor)
{
	if (!HCDEIsValidLiveClient(clientNum))
		return false;

	if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < HCDEAuthorityEventsHeaderSize)
		return false;
	if (memcmp(&body[bodyCursor + HCDEAuthorityEventsMagicOffset], HCDEAuthorityEventsMagic, sizeof(HCDEAuthorityEventsMagic)) != 0)
		return false;

	const size_t startCursor = bodyCursor;
	const uint8_t version = body[bodyCursor + HCDEAuthorityEventsVersionOffset];
	const uint8_t flags = body[bodyCursor + HCDEAuthorityEventsFlagsOffset];
	const uint8_t count = body[bodyCursor + HCDEAuthorityEventsCountOffset];
	const uint8_t reserved = body[bodyCursor + HCDEAuthorityEventsReservedOffset];
	if (version != HCDEAuthorityEventsProtocolVersion || flags != 0u || reserved != 0u)
		return false;

	size_t cursor = bodyCursor + HCDEAuthorityEventsHeaderSize;
	uint32_t applied = 0u;
	uint32_t missing = 0u;
	for (uint8_t i = 0u; i < count; ++i)
	{
		uint8_t eventType = 0u;
		uint8_t source = HREP_SOURCE_SHARED;
		uint8_t category = HREP_ACTOR_UNKNOWN;
		uint8_t actorFlags = 0u;
		uint32_t actorId = 0u;
		uint32_t eventTic = 0u;
		uint16_t classId = 0u;
		uint16_t healthBits = 0u;
		uint16_t wave = 0u;
		uint8_t classNameLen = 0u;
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
		double vx = 0.0;
		double vy = 0.0;
		double vz = 0.0;
		uint32_t yaw = 0u;
		uint32_t pitch = 0u;
		if (!HCDEReadByteField(body, bodyBytes, cursor, eventType)
			|| !HCDEReadByteField(body, bodyBytes, cursor, source)
			|| !HCDEReadByteField(body, bodyBytes, cursor, category)
			|| !HCDEReadByteField(body, bodyBytes, cursor, actorFlags)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, actorId)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, eventTic)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, classId)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, healthBits)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, wave)
			|| !HCDEReadByteField(body, bodyBytes, cursor, classNameLen)
			|| (eventType == HCDEAuthorityEventSpawn && classNameLen == 0u)
			|| cursor > bodyBytes
			|| classNameLen > bodyBytes - cursor)
		{
			return false;
		}
		if ((eventType != HCDEAuthorityEventSpawn
				&& eventType != HCDEAuthorityEventDespawn
				&& eventType != HCDEAuthorityEventDamage)
			|| source > HREP_SOURCE_DM
			|| category > HREP_ACTOR_VISUAL
			|| (actorFlags & ~HCDEActorDeltaFlagLive) != 0u)
		{
			return false;
		}

		FString className(reinterpret_cast<const char*>(&body[cursor]), classNameLen);
		cursor += classNameLen;
		if (!HCDEReadDoubleField(body, bodyBytes, cursor, x)
			|| !HCDEReadDoubleField(body, bodyBytes, cursor, y)
			|| !HCDEReadDoubleField(body, bodyBytes, cursor, z)
			|| !HCDEReadDoubleField(body, bodyBytes, cursor, vx)
			|| !HCDEReadDoubleField(body, bodyBytes, cursor, vy)
			|| !HCDEReadDoubleField(body, bodyBytes, cursor, vz)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, yaw)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, pitch))
		{
			return false;
		}

		HCDEProfileRecordAuthorityEventReceived(eventType, source, category);
		if (eventType == HCDEAuthorityEventSpawn && source == HREP_SOURCE_INVASION)
		{
			FHCDEAuthorityEvent event;
			event.Id = actorId;
			event.Tic = int(eventTic);
			event.Wave = int(wave);
			event.ClassName = className;
			event.Pos = DVector3(x, y, z);
			event.Vel = DVector3(vx, vy, vz);
			event.Yaw = DAngle::fromBam(yaw);
			event.Health = int(int16_t(healthBits));
			event.Category = category;
			if (Net_ApplyInvasionSpawnEvent(event))
				++applied;
			else
				++missing;
		}
		else if (eventType == HCDEAuthorityEventSpawn && category == HREP_ACTOR_PICKUP)
		{
			if (Net_ApplyHCDEPickupSpawnEvent(actorId, classId, category, source, className,
				DVector3(x, y, z), DAngle::fromBam(yaw), int(int16_t(healthBits))))
			{
				++applied;
			}
			else
			{
				++missing;
			}
		}
		else if (eventType == HCDEAuthorityEventDespawn && source == HREP_SOURCE_INVASION)
		{
			if (Net_ApplyInvasionDespawnEvent(actorId, int(int16_t(healthBits))))
				++applied;
			else
				++missing;
		}
		else if (eventType == HCDEAuthorityEventDespawn && category == HREP_ACTOR_PICKUP)
		{
			if (Net_ApplyHCDEPickupRetireEvent(actorId, classId, category, source, className, DVector3(x, y, z)))
				++applied;
			else
				++missing;
		}
		else if (eventType == HCDEAuthorityEventDamage && source == HREP_SOURCE_INVASION)
		{
			if (Net_ApplyInvasionDamageEvent(actorId, int(int16_t(healthBits))))
				++applied;
			else
				++missing;
		}
		else
		{
			++missing;
		}

		(void)category;
		(void)actorFlags;
		(void)classId;
		(void)pitch;
	}

	bodyCursor = cursor;
	++HCDELiveProfile.AuthorityEventPacketsReceived;
	HCDELiveProfile.AuthorityEventBytesReceived += bodyCursor - startCursor;
	HCDELiveProfile.AuthorityEventRecordsReceived += count;
	HCDELiveProfile.AuthorityEventRecordsApplied += applied;
	HCDELiveProfile.AuthorityEventRecordsMissing += missing;
	HCDERecordLiveLaneRx(HLANE_AUTHORITY, clientNum, bodyCursor - startCursor);
	DebugTrace::Markf("net", "HCDE authority events recv client=%d count=%u applied=%u missing=%u",
		clientNum, unsigned(count), unsigned(applied), unsigned(missing));
	return true;
}

static void Net_ClearInvasionReplicatedActorIndexes()
{
	InvasionReplicatedActorIdIndex.Clear();
	InvasionReplicatedActorPtrIndex.Clear();
}

static void Net_IndexInvasionReplicatedActor(size_t index)
{
	if (index >= InvasionReplicatedActors.Size())
		return;

	const auto& ref = InvasionReplicatedActors[index];
	if (ref.Id != 0u)
		InvasionReplicatedActorIdIndex.Insert(ref.Id, unsigned(index));
	const AActor* actor = ref.Actor.Get();
	if (actor != nullptr)
		InvasionReplicatedActorPtrIndex.Insert(actor, unsigned(index));
}

static void Net_RebuildInvasionReplicatedActorIndexes()
{
	Net_ClearInvasionReplicatedActorIndexes();
	for (size_t i = 0u; i < InvasionReplicatedActors.Size(); ++i)
	{
		Net_IndexInvasionReplicatedActor(i);
	}
	++HCDELiveProfile.InvasionActorIndexRebuilds;
}

static bool Net_GetInvasionReplicatedActorIndex(uint32_t id, size_t& index)
{
	if (id == 0u)
		return false;

	const unsigned int* stored = InvasionReplicatedActorIdIndex.CheckKey(id);
	if (stored == nullptr)
		return false;

	const size_t candidate = size_t(*stored);
	if (candidate >= InvasionReplicatedActors.Size() || InvasionReplicatedActors[candidate].Id != id)
	{
		InvasionReplicatedActorIdIndex.Remove(id);
		return false;
	}

	index = candidate;
	return true;
}

static bool Net_GetInvasionReplicatedActorIndexByActor(const AActor* actor, size_t& index)
{
	if (actor == nullptr)
		return false;

	const unsigned int* stored = InvasionReplicatedActorPtrIndex.CheckKey(actor);
	if (stored == nullptr)
		return false;

	const size_t candidate = size_t(*stored);
	if (candidate >= InvasionReplicatedActors.Size() || InvasionReplicatedActors[candidate].Actor != actor)
	{
		InvasionReplicatedActorPtrIndex.Remove(actor);
		return false;
	}

	index = candidate;
	return true;
}

static void Net_SetInvasionReplicatedActorPtr(FInvasionReplicatedActorRef& ref, AActor* actor)
{
	const AActor* oldActor = ref.Actor.Get();
	if (oldActor != nullptr)
		InvasionReplicatedActorPtrIndex.Remove(oldActor);
	ref.Actor = MakeObjPtr<AActor*>(actor);
	if (actor != nullptr)
	{
		ref.SimulationLastHealth = actor->health;
		size_t index = 0u;
		if (Net_GetInvasionReplicatedActorIndex(ref.Id, index))
			InvasionReplicatedActorPtrIndex.Insert(static_cast<const AActor*>(actor), unsigned(index));
		Net_RegisterHCDEReplicatedActor(ref.Id, actor,
			Net_ClassifyHCDEReplicatedActor(actor, Net_IsInvasionReplicatedProjectile(actor)), HREP_SOURCE_INVASION);
	}
	else
	{
		Net_RetireHCDEReplicatedActor(ref.Id);
	}
}

void Net_RegisterInvasionReplicatedMissile(AActor* missile, const AActor* source)
{
	if (!I_IsLocalHCDEServiceAuthority()
		|| !Net_IsInvasionModeEnabled()
		|| gamestate != GS_LEVEL
		|| primaryLevel == nullptr
		|| missile == nullptr
		|| source == nullptr
		|| (missile->ObjectFlags & OF_EuthanizeMe) != 0
		|| !Net_IsInvasionReplicatedProjectile(missile)
		|| Net_FindInvasionReplicatedActorByActor(missile) != nullptr)
	{
		return;
	}

	auto sourceRef = Net_FindInvasionReplicatedActorByActor(source);
	if (sourceRef == nullptr)
		return;
	sourceRef->ServerForcedActionState = HCDEInvasionActorActionMissile;
	sourceRef->ServerForcedActionTic = gametic;

	const uint32_t projectileId = InvasionNextSpawnEventId++;
	if (InvasionNextSpawnEventId == 0u)
		InvasionNextSpawnEventId = 1u;

	FHCDEAuthorityEvent event;
	event.EventType = HCDEAuthorityEventSpawn;
	event.Source = HREP_SOURCE_INVASION;
	event.Category = HREP_ACTOR_PROJECTILE;
	event.ActorFlags = HCDEActorDeltaFlagLive;
	event.ClassId = Net_GetHCDEReplicatedActorClassId(missile->GetClass());
	event.EventSeq = InvasionNextAuthorityEventSeq++;
	if (InvasionNextAuthorityEventSeq == 0u)
		InvasionNextAuthorityEventSeq = 1u;
	event.Id = projectileId;
	event.Tic = gametic;
	event.Wave = InvasionWaveDirector.Wave;
	event.ClassName = missile->GetClass()->TypeName.GetChars();
	event.Pos = missile->Pos();
	event.Vel = missile->Vel;
	event.Yaw = missile->Angles.Yaw;
	event.Health = missile->health;
	HCDEPushRecentAuthorityEvent(event);
	Net_RegisterInvasionReplicatedActor(projectileId, missile);

	DebugTrace::Markf("invasion", "missile replicated id=%u class=%s source=%s pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f)",
		unsigned(projectileId),
		missile->GetClass() != nullptr ? missile->GetClass()->TypeName.GetChars() : "<unknown>",
		source->GetClass() != nullptr ? source->GetClass()->TypeName.GetChars() : "<unknown>",
		missile->X(),
		missile->Y(),
		missile->Z(),
		missile->Vel.X,
		missile->Vel.Y,
		missile->Vel.Z);
}

static bool HCDEAppendActorDeltasV2(int clientNum, uint8_t* output, size_t outputCapacity, size_t& cursor)
{
	if (!HCDEIsValidLiveClient(clientNum))
		return false;
	if (!HCDELivePeerHasCapability(clientNum, HCDELiveCapActorDeltaV2)
		|| !HCDELivePeerHasCapability(clientNum, HCDELiveCapActorRegistryV1))
		return true;

	if (!I_IsLocalHCDEServiceAuthority() || !Net_IsInvasionModeEnabled())
	{
		// Actor-delta-v2 is only a real visual mirror lane in invasion mode.
		// In co-op/DM it currently creates remote baselines for authority actors
		// without owning the matching client-side actor lifetime, which makes the
		// shared actor table churn and bloats every snapshot. Emit a valid empty
		// block outside invasion until the non-invasion mirror path is complete.
		return HCDEAppendBytes(output, outputCapacity, cursor, HCDEActorDeltasMagic, sizeof(HCDEActorDeltasMagic))
			&& HCDEAppendByte(output, outputCapacity, cursor, HCDEActorDeltasProtocolVersion)
			&& HCDEAppendByte(output, outputCapacity, cursor, HCDEActorDeltasFlagComplete)
			&& HCDEAppendByte(output, outputCapacity, cursor, 0u)
			&& HCDEAppendByte(output, outputCapacity, cursor, 0u);
	}

	const size_t startCursor = cursor;
	const int activeRefs = Net_CompactInvasionReplicatedActors();
	Net_CompactHCDEReplicatedActors();
	size_t& sendCursor = HCDEInvasionActorDeltaV2SendCursor[clientNum];
	if (activeRefs <= 0)
		sendCursor = 0u;
	else if (sendCursor >= size_t(activeRefs))
		sendCursor %= size_t(activeRefs);

	const size_t headerCursor = cursor;
	if (!HCDEAppendBytes(output, outputCapacity, cursor, HCDEActorDeltasMagic, sizeof(HCDEActorDeltasMagic))
		|| !HCDEAppendByte(output, outputCapacity, cursor, HCDEActorDeltasProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u))
	{
		return false;
	}

	uint8_t count = 0u;
	size_t nextSendCursor = sendCursor;
	uint64_t considered = 0u;
	uint64_t fullSent = 0u;
	uint64_t partialSent = 0u;
	uint64_t skippedUnchanged = 0u;
	uint64_t deferredBudget = 0u;
	const bool baselineRepair = HCDEActorBaselineRepairActive(clientNum);
	auto& actorQueue = HCDEBuildInvasionActorPriorityQueue(clientNum, activeRefs, sendCursor);
	for (size_t queueIndex = 0u; queueIndex < actorQueue.Size() && count < UINT8_MAX; ++queueIndex)
	{
		const auto& candidate = actorQueue[queueIndex];
		if (candidate.ActorIndex >= InvasionReplicatedActors.Size())
		{
			++deferredBudget;
			HCDELiveProfile.ActorQueueDeferredCandidates++;
			HCDERecordLiveLaneDeferred(HLANE_ACTOR_DELTA, clientNum);
			nextSendCursor = (candidate.ActorIndex + 1u) % max<size_t>(activeRefs, 1u);
			break;
		}

		auto& invasionRef = InvasionReplicatedActors[candidate.ActorIndex];
		AActor* actor = invasionRef.Actor;
		if (actor == nullptr)
		{
			++deferredBudget;
			HCDELiveProfile.ActorQueueDeferredCandidates++;
			HCDERecordLiveLaneDeferred(HLANE_ACTOR_DELTA, clientNum);
			nextSendCursor = (candidate.ActorIndex + 1u) % max<size_t>(activeRefs, 1u);
			continue;
		}

		auto* sharedRef = Net_FindHCDEReplicatedActor(invasionRef.Id);
		if (sharedRef == nullptr)
		{
			Net_RegisterHCDEReplicatedActor(invasionRef.Id, actor,
				Net_ClassifyHCDEReplicatedActor(actor, invasionRef.IsProjectile), HREP_SOURCE_INVASION);
			sharedRef = Net_FindHCDEReplicatedActor(invasionRef.Id);
		}
		if (sharedRef == nullptr)
			continue;

		++considered;
		auto& sent = sharedRef->ClientState[clientNum];
		const bool projectileLive = invasionRef.IsProjectile && Net_IsInvasionReplicatedProjectile(actor) && !invasionRef.ForceDeathDelta;
		uint8_t actorFlags = 0u;
		if ((actor->health > 0 || projectileLive) && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
			actorFlags |= HCDEActorDeltaFlagLive;
		const uint8_t actionState = Net_GetInvasionActorActionState(actor);
		const int actorHealth = projectileLive && actor->health <= 0 ? 1 : actor->health;
		const DVector3 actorPos = actor->Pos();
		const DVector3 actorVel = actor->Vel;
		const uint32_t actorYaw = actor->Angles.Yaw.BAMs();
		const uint32_t actorPitch = actor->Angles.Pitch.BAMs();
		const bool forceFull = baselineRepair
			|| !sent.BaselineValid
			|| sent.ClassId != sharedRef->ClassId
			|| sent.Category != sharedRef->Category
			|| gametic - sent.LastBaselineTic >= TICRATE
			|| (actorFlags & HCDEActorDeltaFlagLive) == 0u;

		uint16_t fieldMask = 0u;
		if (forceFull || sent.Category != sharedRef->Category)
			fieldMask |= HCDEActorDeltaFieldCategory;
		if (forceFull || sent.Flags != actorFlags)
			fieldMask |= HCDEActorDeltaFieldFlags;
		if (forceFull || sent.ActionState != actionState || (candidate.Priority && Net_IsInvasionActorActionPriority(actionState)))
			fieldMask |= HCDEActorDeltaFieldAction;
		if (forceFull || sent.Health != actorHealth)
			fieldMask |= HCDEActorDeltaFieldHealth;
		if (forceFull || Net_InvasionDeltaVectorChanged(sent.Pos, actorPos, 1.0 / HCDEActorDeltaPosScale))
			fieldMask |= HCDEActorDeltaFieldPos;
		if (forceFull || Net_InvasionDeltaVectorChanged(sent.Vel, actorVel, 1.0 / HCDEActorDeltaVelScale))
			fieldMask |= HCDEActorDeltaFieldVel;
		if (forceFull || HCDECompactAngle(sent.Yaw) != HCDECompactAngle(actorYaw) || HCDECompactAngle(sent.Pitch) != HCDECompactAngle(actorPitch))
			fieldMask |= HCDEActorDeltaFieldAngles;
		if (fieldMask == 0u)
		{
			++skippedUnchanged;
			continue;
		}

		size_t recordBytes = 4u + 2u + 2u;
		if (fieldMask & HCDEActorDeltaFieldCategory)
			recordBytes += 1u;
		if (fieldMask & HCDEActorDeltaFieldFlags)
			recordBytes += 1u;
		if (fieldMask & HCDEActorDeltaFieldAction)
			recordBytes += 1u;
		if (fieldMask & HCDEActorDeltaFieldHealth)
			recordBytes += 2u;
		if (fieldMask & HCDEActorDeltaFieldPos)
			recordBytes += 3u * 4u;
		if (fieldMask & HCDEActorDeltaFieldVel)
			recordBytes += 3u * 2u;
		if (fieldMask & HCDEActorDeltaFieldAngles)
			recordBytes += 4u;
		if (cursor > outputCapacity || outputCapacity - cursor < recordBytes)
		{
			++deferredBudget;
			auto& peer = HCDELivePeers[clientNum];
			peer.ActorQueueDeferredDepth = uint32_t(actorQueue.Size() - queueIndex);
			HCDELiveProfile.ActorQueueDeferredCandidates += actorQueue.Size() - queueIndex;
			HCDERecordLiveLaneDeferred(HLANE_ACTOR_DELTA, clientNum);
			nextSendCursor = candidate.ActorIndex;
			break;
		}

		if (!HCDEAppendBE32(output, outputCapacity, cursor, sharedRef->Id)
			|| !HCDEAppendBE16(output, outputCapacity, cursor, sharedRef->ClassId)
			|| !HCDEAppendBE16(output, outputCapacity, cursor, fieldMask))
		{
			return false;
		}
		if ((fieldMask & HCDEActorDeltaFieldCategory)
			&& !HCDEAppendByte(output, outputCapacity, cursor, sharedRef->Category))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldFlags)
			&& !HCDEAppendByte(output, outputCapacity, cursor, actorFlags))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldAction)
			&& !HCDEAppendByte(output, outputCapacity, cursor, actionState))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldHealth)
			&& !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(actorHealth, INT16_MIN, INT16_MAX))))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldPos)
			&& (!HCDEAppendQuantizedPos(output, outputCapacity, cursor, actorPos.X)
				|| !HCDEAppendQuantizedPos(output, outputCapacity, cursor, actorPos.Y)
				|| !HCDEAppendQuantizedPos(output, outputCapacity, cursor, actorPos.Z)))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldVel)
			&& (!HCDEAppendQuantizedVel(output, outputCapacity, cursor, actorVel.X)
				|| !HCDEAppendQuantizedVel(output, outputCapacity, cursor, actorVel.Y)
				|| !HCDEAppendQuantizedVel(output, outputCapacity, cursor, actorVel.Z)))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldAngles)
			&& (!HCDEAppendBE16(output, outputCapacity, cursor, HCDECompactAngle(actorYaw))
				|| !HCDEAppendBE16(output, outputCapacity, cursor, HCDECompactAngle(actorPitch))))
			return false;

		sent.BaselineValid = true;
		sent.LastSentTic = gametic;
		sent.ClassId = sharedRef->ClassId;
		sent.Category = sharedRef->Category;
		sent.Flags = actorFlags;
		sent.ActionState = actionState;
		sent.Health = actorHealth;
		sent.Pos = actorPos;
		sent.Vel = actorVel;
		sent.Yaw = actorYaw;
		sent.Pitch = actorPitch;
		if (forceFull)
		{
			sent.LastBaselineTic = gametic;
			++fullSent;
		}
		else
		{
			++partialSent;
		}
		++count;
		nextSendCursor = (candidate.ActorIndex + 1u) % size_t(activeRefs);
	}

	sendCursor = activeRefs > 0 ? nextSendCursor : 0u;
	const uint8_t flags = count == activeRefs ? HCDEActorDeltasFlagComplete : 0u;
	output[headerCursor + HCDEActorDeltasFlagsOffset] = flags;
	output[headerCursor + HCDEActorDeltasCountOffset] = count;
	++HCDELiveProfile.ActorDeltaV2PacketsBuilt;
	HCDELiveProfile.ActorDeltaV2BytesBuilt += cursor - startCursor;
	HCDELiveProfile.ActorDeltaV2RecordsBuilt += count;
	HCDELiveProfile.ActorDeltaV2FullRecordsBuilt += fullSent;
	HCDELiveProfile.ActorDeltaV2PartialRecordsBuilt += partialSent;
	HCDELiveProfile.ActorDeltaV2SkippedUnchanged += skippedUnchanged;
	HCDELiveProfile.ActorDeltaV2DeferredBudget += deferredBudget;
	HCDERecordLiveLaneTx(HLANE_ACTOR_DELTA, clientNum, cursor - startCursor);

	DebugTrace::Markf("net", "HCDE actor delta v2 send client=%d count=%u complete=%d active=%d full=%llu partial=%llu skipped=%llu deferred=%llu cursor=%zu bytes-left=%zu",
		clientNum, unsigned(count),
		(flags & HCDEActorDeltasFlagComplete) != 0u ? 1 : 0,
		activeRefs,
		static_cast<unsigned long long>(fullSent),
		static_cast<unsigned long long>(partialSent),
		static_cast<unsigned long long>(skippedUnchanged),
		static_cast<unsigned long long>(deferredBudget),
		sendCursor,
		cursor <= outputCapacity ? outputCapacity - cursor : 0u);
	return true;
}

static bool HCDEAppendSharedActorDeltasV2(int clientNum, uint8_t* output, size_t outputCapacity, size_t& cursor)
{
	if (!HCDEIsValidLiveClient(clientNum))
		return false;
	if (!HCDELivePeerHasCapability(clientNum, HCDELiveCapActorDeltaV2)
		|| !HCDELivePeerHasCapability(clientNum, HCDELiveCapActorRegistryV1))
		return true;

	if (!I_IsLocalHCDEServiceAuthority())
		return true;

	HCDELivePeers[clientNum].ProjectilePolicySkipped = 0u;
	HCDELivePeers[clientNum].ProjectilePolicyKeepAlive = 0u;
	HCDELivePeers[clientNum].ProjectilePolicyProtected = 0u;
	for (uint8_t interest = 0u; interest < HINTEREST_COUNT; ++interest)
		HCDELivePeers[clientNum].ProjectilePolicyTiers[interest] = 0u;

	Net_CompactHCDEReplicatedActors();
	const size_t registrySize = HCDEReplicatedActors.Size();
	size_t activeRefs = 0u;
	for (size_t i = 0u; i < registrySize; ++i)
	{
		if (HCDEShouldSendSharedActorDelta(HCDEReplicatedActors[i]))
			++activeRefs;
	}
	if (activeRefs == 0u)
	{
		HCDEActorDeltaV2SendCursor[clientNum] = 0u;
		return true;
	}

	size_t& sendCursor = HCDEActorDeltaV2SendCursor[clientNum];
	if (sendCursor >= registrySize)
		sendCursor %= registrySize;

	const size_t startCursor = cursor;
	if (cursor > outputCapacity || outputCapacity - cursor < HCDEActorDeltasHeaderSize)
	{
		HCDELiveProfile.ActorDeltaV2DeferredBudget += activeRefs;
		HCDERecordLiveLaneDeferred(HLANE_ACTOR_DELTA, clientNum);
		return true;
	}

	const size_t headerCursor = cursor;
	if (!HCDEAppendBytes(output, outputCapacity, cursor, HCDEActorDeltasMagic, sizeof(HCDEActorDeltasMagic))
		|| !HCDEAppendByte(output, outputCapacity, cursor, HCDEActorDeltasProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u))
	{
		return false;
	}

	uint8_t count = 0u;
	size_t nextSendCursor = sendCursor;
	uint64_t fullSent = 0u;
	uint64_t partialSent = 0u;
	uint64_t skippedUnchanged = 0u;
	uint64_t deferredBudget = 0u;
	const bool baselineRepair = HCDEActorBaselineRepairActive(clientNum);
	for (size_t scanned = 0u; scanned < registrySize && count < UINT8_MAX; ++scanned)
	{
		const size_t actorIndex = (sendCursor + scanned) % registrySize;
		auto& sharedRef = HCDEReplicatedActors[actorIndex];
		if (!HCDEShouldSendSharedActorDelta(sharedRef))
			continue;

		AActor* actor = sharedRef.Actor.Get();
		if (actor == nullptr)
			continue;

		auto& sent = sharedRef.ClientState[clientNum];
		const bool projectileLive = sharedRef.Category == HREP_ACTOR_PROJECTILE
			&& ((actor->flags & MF_MISSILE) != 0 || Net_IsInvasionReplicatedProjectile(actor));
		if (projectileLive)
		{
			const FHCDEProjectilePolicyResult projectilePolicy = HCDEEvaluateProjectilePolicy(actor,
				players[clientNum].mo, baselineRepair ? false : sent.BaselineValid, baselineRepair ? 0 : sent.LastSentTic);
			HCDERecordProjectilePolicyResult(clientNum, projectilePolicy);
			if (!projectilePolicy.Relevant)
			{
				nextSendCursor = (actorIndex + 1u) % registrySize;
				continue;
			}
		}
		uint8_t actorFlags = 0u;
		if ((actor->health > 0 || projectileLive) && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
			actorFlags |= HCDEActorDeltaFlagLive;
		const uint8_t actionState = HCDEGetSharedActorActionState(actor, sharedRef.Category);
		const int actorHealth = projectileLive && actor->health <= 0 ? 1 : actor->health;
		const DVector3 actorPos = actor->Pos();
		const DVector3 actorVel = actor->Vel;
		const uint32_t actorYaw = actor->Angles.Yaw.BAMs();
		const uint32_t actorPitch = actor->Angles.Pitch.BAMs();
		const bool forceFull = baselineRepair
			|| !sent.BaselineValid
			|| sent.ClassId != sharedRef.ClassId
			|| sent.Category != sharedRef.Category
			|| gametic - sent.LastBaselineTic >= TICRATE
			|| (actorFlags & HCDEActorDeltaFlagLive) == 0u;

		uint16_t fieldMask = 0u;
		if (forceFull || sent.Category != sharedRef.Category)
			fieldMask |= HCDEActorDeltaFieldCategory;
		if (forceFull || sent.Flags != actorFlags)
			fieldMask |= HCDEActorDeltaFieldFlags;
		if (forceFull || sent.ActionState != actionState)
			fieldMask |= HCDEActorDeltaFieldAction;
		if (forceFull || sent.Health != actorHealth)
			fieldMask |= HCDEActorDeltaFieldHealth;
		if (forceFull || Net_InvasionDeltaVectorChanged(sent.Pos, actorPos, 1.0 / HCDEActorDeltaPosScale))
			fieldMask |= HCDEActorDeltaFieldPos;
		if (forceFull || Net_InvasionDeltaVectorChanged(sent.Vel, actorVel, 1.0 / HCDEActorDeltaVelScale))
			fieldMask |= HCDEActorDeltaFieldVel;
		if (forceFull || HCDECompactAngle(sent.Yaw) != HCDECompactAngle(actorYaw) || HCDECompactAngle(sent.Pitch) != HCDECompactAngle(actorPitch))
			fieldMask |= HCDEActorDeltaFieldAngles;
		if (fieldMask == 0u)
		{
			++skippedUnchanged;
			nextSendCursor = (actorIndex + 1u) % registrySize;
			continue;
		}

		size_t recordBytes = 4u + 2u + 2u;
		if (fieldMask & HCDEActorDeltaFieldCategory)
			recordBytes += 1u;
		if (fieldMask & HCDEActorDeltaFieldFlags)
			recordBytes += 1u;
		if (fieldMask & HCDEActorDeltaFieldAction)
			recordBytes += 1u;
		if (fieldMask & HCDEActorDeltaFieldHealth)
			recordBytes += 2u;
		if (fieldMask & HCDEActorDeltaFieldPos)
			recordBytes += 3u * 4u;
		if (fieldMask & HCDEActorDeltaFieldVel)
			recordBytes += 3u * 2u;
		if (fieldMask & HCDEActorDeltaFieldAngles)
			recordBytes += 4u;
		if (cursor > outputCapacity || outputCapacity - cursor < recordBytes)
		{
			++deferredBudget;
			HCDELivePeers[clientNum].ActorQueueDeferredDepth = uint32_t(activeRefs - min<size_t>(activeRefs, size_t(count)));
			HCDELiveProfile.ActorQueueDeferredCandidates += activeRefs - min<size_t>(activeRefs, size_t(count));
			HCDERecordLiveLaneDeferred(HLANE_ACTOR_DELTA, clientNum);
			nextSendCursor = actorIndex;
			break;
		}

		if (!HCDEAppendBE32(output, outputCapacity, cursor, sharedRef.Id)
			|| !HCDEAppendBE16(output, outputCapacity, cursor, sharedRef.ClassId)
			|| !HCDEAppendBE16(output, outputCapacity, cursor, fieldMask))
		{
			return false;
		}
		if ((fieldMask & HCDEActorDeltaFieldCategory)
			&& !HCDEAppendByte(output, outputCapacity, cursor, sharedRef.Category))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldFlags)
			&& !HCDEAppendByte(output, outputCapacity, cursor, actorFlags))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldAction)
			&& !HCDEAppendByte(output, outputCapacity, cursor, actionState))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldHealth)
			&& !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(actorHealth, INT16_MIN, INT16_MAX))))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldPos)
			&& (!HCDEAppendQuantizedPos(output, outputCapacity, cursor, actorPos.X)
				|| !HCDEAppendQuantizedPos(output, outputCapacity, cursor, actorPos.Y)
				|| !HCDEAppendQuantizedPos(output, outputCapacity, cursor, actorPos.Z)))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldVel)
			&& (!HCDEAppendQuantizedVel(output, outputCapacity, cursor, actorVel.X)
				|| !HCDEAppendQuantizedVel(output, outputCapacity, cursor, actorVel.Y)
				|| !HCDEAppendQuantizedVel(output, outputCapacity, cursor, actorVel.Z)))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldAngles)
			&& (!HCDEAppendBE16(output, outputCapacity, cursor, HCDECompactAngle(actorYaw))
				|| !HCDEAppendBE16(output, outputCapacity, cursor, HCDECompactAngle(actorPitch))))
			return false;

		sent.BaselineValid = true;
		sent.LastSentTic = gametic;
		sent.ClassId = sharedRef.ClassId;
		sent.Category = sharedRef.Category;
		sent.Flags = actorFlags;
		sent.ActionState = actionState;
		sent.Health = actorHealth;
		sent.Pos = actorPos;
		sent.Vel = actorVel;
		sent.Yaw = actorYaw;
		sent.Pitch = actorPitch;
		if (forceFull)
		{
			sent.LastBaselineTic = gametic;
			++fullSent;
		}
		else
		{
			++partialSent;
		}
		++count;
		nextSendCursor = (actorIndex + 1u) % registrySize;
	}

	sendCursor = registrySize > 0u ? nextSendCursor : 0u;
	if (count == 0u)
	{
		cursor = startCursor;
		HCDELiveProfile.ActorDeltaV2SkippedUnchanged += skippedUnchanged;
		HCDELiveProfile.ActorDeltaV2DeferredBudget += deferredBudget;
		return true;
	}

	const uint8_t flags = size_t(count) == activeRefs ? HCDEActorDeltasFlagComplete : 0u;
	output[headerCursor + HCDEActorDeltasFlagsOffset] = flags;
	output[headerCursor + HCDEActorDeltasCountOffset] = count;
	++HCDELiveProfile.ActorDeltaV2PacketsBuilt;
	HCDELiveProfile.ActorDeltaV2BytesBuilt += cursor - startCursor;
	HCDELiveProfile.ActorDeltaV2RecordsBuilt += count;
	HCDELiveProfile.ActorDeltaV2FullRecordsBuilt += fullSent;
	HCDELiveProfile.ActorDeltaV2PartialRecordsBuilt += partialSent;
	HCDELiveProfile.ActorDeltaV2SkippedUnchanged += skippedUnchanged;
	HCDELiveProfile.ActorDeltaV2DeferredBudget += deferredBudget;
	HCDERecordLiveLaneTx(HLANE_ACTOR_DELTA, clientNum, cursor - startCursor);

	DebugTrace::Markf("net", "HCDE shared actor delta v2 send client=%d count=%u complete=%d active=%zu full=%llu partial=%llu skipped=%llu deferred=%llu cursor=%zu bytes-left=%zu",
		clientNum, unsigned(count),
		(flags & HCDEActorDeltasFlagComplete) != 0u ? 1 : 0,
		activeRefs,
		static_cast<unsigned long long>(fullSent),
		static_cast<unsigned long long>(partialSent),
		static_cast<unsigned long long>(skippedUnchanged),
		static_cast<unsigned long long>(deferredBudget),
		sendCursor,
		cursor <= outputCapacity ? outputCapacity - cursor : 0u);
	return true;
}

static bool HCDEApplyActorDeltasV2(int clientNum, const uint8_t* body, size_t bodyBytes, size_t& bodyCursor)
{
	if (!HCDEIsValidLiveClient(clientNum))
		return false;
	if (!HCDELivePeerHasCapability(clientNum, HCDELiveCapActorDeltaV2)
		|| !HCDELivePeerHasCapability(clientNum, HCDELiveCapActorRegistryV1))
		return false;

	if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < HCDEActorDeltasHeaderSize)
		return false;
	if (memcmp(&body[bodyCursor + HCDEActorDeltasMagicOffset], HCDEActorDeltasMagic, sizeof(HCDEActorDeltasMagic)) != 0)
		return false;

	const uint8_t version = body[bodyCursor + HCDEActorDeltasVersionOffset];
	const uint8_t flags = body[bodyCursor + HCDEActorDeltasFlagsOffset];
	const uint8_t count = body[bodyCursor + HCDEActorDeltasCountOffset];
	const uint8_t reserved = body[bodyCursor + HCDEActorDeltasReservedOffset];
	if (version != HCDEActorDeltasProtocolVersion
		|| (flags & ~HCDEActorDeltasFlagComplete) != 0u
		|| reserved != 0u)
	{
		return false;
	}

	const size_t startCursor = bodyCursor;
	size_t cursor = bodyCursor + HCDEActorDeltasHeaderSize;
	const bool invasionActorLane = Net_IsInvasionModeEnabled();
	int applied = 0;
	int missing = 0;
	for (uint8_t i = 0u; i < count; ++i)
	{
		uint32_t id = 0u;
		uint16_t classId = 0u;
		uint16_t fieldMask = 0u;
		uint8_t category = HREP_ACTOR_UNKNOWN;
		uint8_t actorFlags = 0u;
		uint8_t actionState = HCDEInvasionActorActionNone;
		uint16_t healthBits = 0u;
		double values[6] = {};
		uint16_t yawCompact = 0u;
		uint16_t pitchCompact = 0u;
		if (!HCDEReadBE32Field(body, bodyBytes, cursor, id)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, classId)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, fieldMask)
			|| id == 0u
			|| fieldMask == 0u
			|| (fieldMask & ~HCDEActorDeltaFieldAll) != 0u)
		{
			return false;
		}

		if ((fieldMask & HCDEActorDeltaFieldCategory)
			&& !HCDEReadByteField(body, bodyBytes, cursor, category))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldFlags)
			&& !HCDEReadByteField(body, bodyBytes, cursor, actorFlags))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldAction)
			&& !HCDEReadByteField(body, bodyBytes, cursor, actionState))
			return false;
		if ((fieldMask & HCDEActorDeltaFieldHealth)
			&& !HCDEReadBE16Field(body, bodyBytes, cursor, healthBits))
			return false;
		if (fieldMask & HCDEActorDeltaFieldPos)
		{
			for (int valueIndex = 0; valueIndex < 3; ++valueIndex)
			{
				if (!HCDEReadQuantizedPosField(body, bodyBytes, cursor, values[valueIndex]))
					return false;
			}
		}
		if (fieldMask & HCDEActorDeltaFieldVel)
		{
			for (int valueIndex = 3; valueIndex < 6; ++valueIndex)
			{
				if (!HCDEReadQuantizedVelField(body, bodyBytes, cursor, values[valueIndex]))
					return false;
			}
		}
		if ((fieldMask & HCDEActorDeltaFieldAngles)
			&& (!HCDEReadBE16Field(body, bodyBytes, cursor, yawCompact)
				|| !HCDEReadBE16Field(body, bodyBytes, cursor, pitchCompact)))
		{
			return false;
		}
		if (category > HREP_ACTOR_VISUAL
			|| (actorFlags & ~HCDEActorDeltaFlagLive) != 0u
			|| actionState > HCDEInvasionActorActionMax)
		{
			return false;
		}

		auto* sharedRef = Net_FindHCDEReplicatedActor(id);
		auto* invasionRef = invasionActorLane ? Net_FindInvasionReplicatedActor(id) : nullptr;
		if (!invasionActorLane && sharedRef == nullptr)
		{
			// Non-invasion actor deltas are disabled on the send path above, but
			// tolerate older peers by parsing and discarding their records. The
			// old behavior registered baselines with no backing client actor, so
			// a repeated full baseline stream grew HCDEReplicatedActors forever
			// and polluted prediction diagnostics with thousands of missing refs.
			sharedRef = nullptr;
		}
		AActor* actor = invasionRef != nullptr ? invasionRef->Actor.Get()
			: (sharedRef != nullptr ? sharedRef->Actor.Get() : nullptr);
		if (sharedRef == nullptr && actor != nullptr)
		{
			Net_RegisterHCDEReplicatedActor(id, actor,
				Net_ClassifyHCDEReplicatedActor(actor, invasionRef != nullptr && invasionRef->IsProjectile), HREP_SOURCE_INVASION);
			sharedRef = Net_FindHCDEReplicatedActor(id);
		}
		FHCDEReplicatedActorClientState fallbackState;
		FHCDEReplicatedActorClientState* state = sharedRef != nullptr ? &sharedRef->ClientState[clientNum] : &fallbackState;
		const bool hasBaseline = state->BaselineValid;
		if ((fieldMask & HCDEActorDeltaFieldCategory) == 0u)
			category = hasBaseline ? state->Category : (sharedRef != nullptr ? sharedRef->Category : HREP_ACTOR_UNKNOWN);
		if ((fieldMask & HCDEActorDeltaFieldFlags) == 0u)
		{
			if (hasBaseline)
				actorFlags = state->Flags;
			else if (actor != nullptr && (actor->health > 0 || (invasionRef != nullptr && invasionRef->IsProjectile)) && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
				actorFlags = HCDEActorDeltaFlagLive;
		}
		if ((fieldMask & HCDEActorDeltaFieldAction) == 0u)
			actionState = hasBaseline ? state->ActionState : (invasionRef != nullptr ? invasionRef->VisualActionState : HCDEInvasionActorActionNone);
		if ((fieldMask & HCDEActorDeltaFieldHealth) == 0u)
			healthBits = uint16_t(clamp<int>(hasBaseline ? state->Health : (actor != nullptr ? actor->health : 0), INT16_MIN, INT16_MAX));
		if ((fieldMask & HCDEActorDeltaFieldPos) == 0u)
		{
			const DVector3 pos = hasBaseline ? state->Pos
				: (invasionRef != nullptr && invasionRef->HasVisualTarget ? invasionRef->VisualTargetPos
					: (actor != nullptr ? actor->Pos() : DVector3()));
			values[0] = pos.X;
			values[1] = pos.Y;
			values[2] = pos.Z;
		}
		if ((fieldMask & HCDEActorDeltaFieldVel) == 0u)
		{
			const DVector3 vel = hasBaseline ? state->Vel : (actor != nullptr ? actor->Vel : DVector3());
			values[3] = vel.X;
			values[4] = vel.Y;
			values[5] = vel.Z;
		}
		if ((fieldMask & HCDEActorDeltaFieldAngles) == 0u)
		{
			if (hasBaseline)
			{
				yawCompact = HCDECompactAngle(state->Yaw);
				pitchCompact = HCDECompactAngle(state->Pitch);
			}
			else if (invasionRef != nullptr && invasionRef->HasVisualTarget)
			{
				yawCompact = HCDECompactAngle(invasionRef->VisualTargetYaw.BAMs());
				pitchCompact = HCDECompactAngle(invasionRef->VisualTargetPitch.BAMs());
			}
			else if (actor != nullptr)
			{
				yawCompact = HCDECompactAngle(actor->Angles.Yaw.BAMs());
				pitchCompact = HCDECompactAngle(actor->Angles.Pitch.BAMs());
			}
		}

		const int health = int(int16_t(healthBits));
		const DVector3 pos(values[0], values[1], values[2]);
		const DVector3 vel(values[3], values[4], values[5]);
		const DAngle targetYaw = DAngle::fromBam(HCDEExpandCompactAngle(yawCompact));
		const DAngle targetPitch = DAngle::fromBam(HCDEExpandCompactAngle(pitchCompact));
		// Non-invasion HCDA blocks establish shared baselines only. Actor birth,
		// ownership, and destructive gameplay application stay on later authority work.
		if (!invasionActorLane && sharedRef == nullptr)
		{
			++missing;
			continue;
		}

		if (invasionActorLane
			&& (actor == nullptr || invasionRef == nullptr)
			&& (actorFlags & HCDEActorDeltaFlagLive) != 0u
			&& health > 0)
		{
			const PClassActor* actorClass = Net_GetHCDEReplicatedActorClass(classId);
			if (actorClass != nullptr)
			{
				if (NetworkEntityManager::IsPredicting())
				{
					// Mirror spawn events: applying invasion actors inside the
					// prediction window lets rollback delete them before render.
					Net_QueueInvasionMirrorSpawn(id, InvasionWaveDirector.Wave, actorClass->TypeName.GetChars(),
						pos, vel, targetYaw, targetPitch, health, false);
				}
				else if (Net_SpawnInvasionMirrorActor(id, InvasionWaveDirector.Wave, actorClass->TypeName.GetChars(),
					pos, vel, targetYaw, targetPitch, health, "actor-delta-v2", false, category))
				{
					invasionRef = Net_FindInvasionReplicatedActor(id);
					actor = invasionRef != nullptr ? invasionRef->Actor.Get() : nullptr;
					sharedRef = Net_FindHCDEReplicatedActor(id);
					state = sharedRef != nullptr ? &sharedRef->ClientState[clientNum] : state;
				}
			}
		}

		if (sharedRef != nullptr)
		{
			sharedRef->ClassId = classId != 0u ? classId : sharedRef->ClassId;
			sharedRef->Category = category;
			if (invasionActorLane)
				sharedRef->Source = HREP_SOURCE_INVASION;
			sharedRef->LastTouchedTic = gametic;
			state = &sharedRef->ClientState[clientNum];
			state->BaselineValid = true;
			state->LastSentTic = gametic;
			state->ClassId = sharedRef->ClassId;
			state->Category = category;
			state->Flags = actorFlags;
			state->ActionState = actionState;
			state->Health = health;
			state->Pos = pos;
			state->Vel = vel;
			state->Yaw = HCDEExpandCompactAngle(yawCompact);
			state->Pitch = HCDEExpandCompactAngle(pitchCompact);
			if ((fieldMask & (HCDEActorDeltaFieldCategory | HCDEActorDeltaFieldFlags | HCDEActorDeltaFieldHealth | HCDEActorDeltaFieldPos | HCDEActorDeltaFieldAngles)) != 0u)
				state->LastBaselineTic = gametic;
		}

		if (!invasionActorLane)
		{
			++applied;
			continue;
		}

		if (invasionRef == nullptr || actor == nullptr)
		{
			if (Net_HasPendingInvasionMirrorSpawn(id))
			{
				++applied;
				continue;
			}
			++missing;
			continue;
		}
		if ((actorFlags & HCDEActorDeltaFlagLive) == 0u || health <= 0)
		{
			Net_RetireInvasionMirrorActor(*invasionRef, health);
			++applied;
			continue;
		}

		const DVector3 oldPos = actor->Pos();
		const bool firstVisualTarget = !invasionRef->HasVisualTarget;
		if (category == HREP_ACTOR_PROJECTILE)
			invasionRef->IsProjectile = true;
		else if (Net_ClassDefaultsSuggestProjectile(actor->GetClass()))
			invasionRef->IsProjectile = true;
		if (Net_IsInvasionReplicatedProjectile(actor))
			invasionRef->IsProjectile = true;
		Net_SetInvasionMirrorVisualTarget(*invasionRef, pos, vel, targetYaw, targetPitch, health);
		actor->health = health;
		actor->Angles.Yaw = targetYaw;
		actor->Angles.Pitch = targetPitch;
		Net_SetInvasionMirrorVisualOnly(id, actor);
		if (!invasionRef->IsProjectile)
			Net_ApplyInvasionMirrorActionState(*invasionRef, actor, actionState);
		const double distSq = (pos - oldPos).LengthSquared();
		const double snapDistanceSq = HCDEInvasionMirrorVisualSnapDistance * HCDEInvasionMirrorVisualSnapDistance;
		const bool combatAction = actionState == HCDEInvasionActorActionMelee
			|| actionState == HCDEInvasionActorActionMissile;
		if (firstVisualTarget || invasionRef->IsProjectile || distSq > snapDistanceSq || combatAction)
		{
			actor->SetOrigin(pos, false);
			actor->Prev = pos;
			actor->PrevPortalGroup = actor->Sector != nullptr ? actor->Sector->PortalGroup : actor->PrevPortalGroup;
			actor->ClearInterpolation();
		}
		++applied;
	}

	bodyCursor = cursor;
	++HCDELiveProfile.ActorDeltaV2PacketsReceived;
	HCDELiveProfile.ActorDeltaV2RecordsReceived += count;
	HCDELiveProfile.ActorDeltaV2RecordsApplied += applied;
	HCDELiveProfile.ActorDeltaV2RecordsMissing += missing;
	HCDERecordLiveLaneRx(HLANE_ACTOR_DELTA, clientNum, bodyCursor - startCursor);
	DebugTrace::Markf("net", "HCDE actor delta v2 recv client=%d count=%u applied=%d missing=%d tracked=%u",
		clientNum, unsigned(count), applied, missing, unsigned(HCDEReplicatedActors.Size()));
	if (!I_IsLocalHCDEServiceAuthority() && gametic >= HCDEActorDeltaV2ReceiveCompactNextTic[clientNum])
	{
		// Non-authority clients can create baseline-only shared actor refs from
		// incoming deltas (actor == nullptr). Servers compact during their send
		// and migration passes, but a pure receiver has no such cadence. Doom2
		// Remake maps churn enough shared refs that stale client baselines can
		// otherwise climb into the tens of thousands and inflate every lookup.
		HCDEActorDeltaV2ReceiveCompactNextTic[clientNum] = gametic + TICRATE;
		const int removed = Net_CompactHCDEReplicatedActors();
		if (removed > 0)
		{
			DebugTrace::Markf("net", "HCDE actor delta v2 recv compact client=%d removed=%d tracked=%u",
				clientNum, removed, unsigned(HCDEReplicatedActors.Size()));
		}
	}
	return true;
}
