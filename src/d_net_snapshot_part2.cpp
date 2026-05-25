// This file is split from d_net.cpp

	case DEM_ADDSLOTDEFAULT:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u)
			&& HCDEAppendLegacyWeaponIndex(output, outputCapacity, outputCursor, data, dataSize, inputCursor);
		break;

	case DEM_SETPITCHLIMIT:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 2u);
		break;

	default:
		ok = false;
		break;
	}

	return ok && inputCursor == dataSize;
}

// Build a native HLIVE_SERVER_SNAPSHOT body directly from authoritative gameplay
// state. The output buffer holds the full snapshot payload (header + quitters +
// records + world deltas + actor deltas + invasion snapshot). The header bytes are
// fixed up at the end once `bodyBytes` is known. Returns false (with outputSize=0)
// on any encoding failure, leaving the caller to invoke HCDEAbortLiveGameplaySend
// instead of falling back to a legacy NCMD encoder.
static bool HCDEBuildNativeServerSnapshotPayload(int client, uint8_t controlFlags, uint8_t routingByte,
	uint32_t sequenceAck, uint32_t consistencyAck, uint32_t baseSequence, uint32_t baseConsistency,
	uint8_t commandTics, uint8_t consistencyTics, uint8_t stabilityBuffer, const int* playerNums, uint8_t playerCount,
	const int* quitNums, uint8_t quitterCount, const usercmd_t (*commands)[MAXSENDTICS],
	const uint8_t* const (*eventStreams)[MAXSENDTICS], const size_t (*eventSizes)[MAXSENDTICS],
	uint8_t* output, size_t outputCapacity, size_t& outputSize, const char*& failureReason)
{
	outputSize = 0u;
	failureReason = nullptr;
	auto fail = [&](const char* reason) -> bool
	{
		failureReason = reason;
		return false;
	};
	if (playerCount > MAXPLAYERS
		|| commandTics > MAXSENDTICS
		|| consistencyTics > MAXSENDTICS
		|| (playerCount > 0u && playerNums == nullptr)
		|| (quitterCount > 0u && quitNums == nullptr)
		|| (commandTics > 0u && (commands == nullptr || eventStreams == nullptr || eventSizes == nullptr)))
	{
		return fail("server-snapshot-invalid-build-input");
	}

	const size_t quitterBytes = (controlFlags & NCMD_QUITTERS) != 0u ? size_t(quitterCount) + 1u : 0u;
	if (((controlFlags & NCMD_QUITTERS) == 0u && quitterCount != 0u)
		|| ((controlFlags & NCMD_QUITTERS) != 0u && quitterCount == 0u)
		|| HCDEServerSnapshotHeaderSize + quitterBytes > outputCapacity)
	{
		return fail("server-snapshot-invalid-quitter-header");
	}
	// Validate quitter slots before we spend any work serialising the body so a bad
	// caller list is rejected up front instead of mid-build (which would otherwise
	// leave partial data in `output` even though we never advertise it via outputSize).
	for (uint8_t i = 0u; i < quitterCount; ++i)
	{
		if (quitNums[i] < 0 || quitNums[i] >= MAXPLAYERS)
			return fail("server-snapshot-invalid-quitter-slot");
	}

	size_t bodyCursor = HCDEServerSnapshotHeaderSize + quitterBytes;
	uint64_t playersSeen = 0u;
	uint64_t worldDeltaPlayersSeen = 0u;
	uint8_t worldDeltaPlayers[MAXPLAYERS] = {};
	size_t worldDeltaPlayerCount = 0u;
	auto addWorldDeltaPlayer = [&](uint8_t playerNum) -> bool
	{
		if (playerNum >= MAXPLAYERS || playerNum >= 64u)
			return false;
		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if ((worldDeltaPlayersSeen & playerMask) != 0u)
			return true;
		if (worldDeltaPlayerCount >= MAXPLAYERS)
			return false;
		worldDeltaPlayersSeen |= playerMask;
		worldDeltaPlayers[worldDeltaPlayerCount++] = playerNum;
		return true;
	};

	if (!HCDEAppendBytes(output, outputCapacity, bodyCursor, HCDEServerSnapshotRecordsMagic, sizeof(HCDEServerSnapshotRecordsMagic))
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, HCDEServerSnapshotRecordsProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, playerCount))
	{
		return fail("server-snapshot-record-header-overflow");
	}

	for (uint8_t p = 0u; p < playerCount; ++p)
	{
		const int playerNumInt = playerNums[p];
		if (playerNumInt < 0 || playerNumInt >= MAXPLAYERS || playerNumInt >= 64)
			return fail("server-snapshot-invalid-player-slot");
		const uint8_t playerNum = uint8_t(playerNumInt);
		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if ((playersSeen & playerMask) != 0u)
			return fail("server-snapshot-duplicate-player-slot");
		playersSeen |= playerMask;
		if (!addWorldDeltaPlayer(playerNum))
			return fail("server-snapshot-world-player-list-full");

		auto& clientState = ClientStates[playerNum];
		if (!HCDEAppendByte(output, outputCapacity, bodyCursor, playerNum)
			|| !HCDEAppendBE16(output, outputCapacity, bodyCursor, uint16_t(clientState.AverageLatency))
			|| !HCDEAppendByte(output, outputCapacity, bodyCursor, consistencyTics)
			|| !HCDEAppendByte(output, outputCapacity, bodyCursor, commandTics))
		{
			return fail("server-snapshot-player-record-overflow");
		}

		for (uint8_t r = 0u; r < consistencyTics; ++r)
		{
			const int tic = int(baseConsistency) + int(r);
			if (!HCDEAppendByte(output, outputCapacity, bodyCursor, r)
				|| !HCDEAppendBE16(output, outputCapacity, bodyCursor, uint16_t(clientState.LocalConsistency[tic % BACKUPTICS])))
			{
				return fail("server-snapshot-consistency-record-overflow");
			}
		}

		for (uint8_t t = 0u; t < commandTics; ++t)
		{
			if (!HCDEAppendByte(output, outputCapacity, bodyCursor, t)
				|| !HCDEAppendEventRecords(output, outputCapacity, bodyCursor, eventStreams[p][t], eventSizes[p][t], /*clientInput=*/false)
				|| !HCDEAppendUserCmdFields(output, outputCapacity, bodyCursor, commands[p][t]))
			{
				return fail("server-snapshot-command-record-overflow");
			}
		}
	}

	for (uint8_t playerNum = 0u; playerNum < MAXPLAYERS; ++playerNum)
	{
		if (!playeringame[playerNum] || I_IsServerReservedSlot(playerNum) || players[playerNum].mo == nullptr)
			continue;
		if (!addWorldDeltaPlayer(playerNum))
			return fail("server-snapshot-world-player-list-full");
	}

	const size_t playerSnapshotEnd = bodyCursor;
	if (!HCDEAppendServerWorldDeltas(client, output, outputCapacity, bodyCursor, worldDeltaPlayers, worldDeltaPlayerCount))
		return fail("server-snapshot-world-delta-build");
	if (*net_echo_debug != 0)
	{
		if (!HCDEAppendPresentationEcho(client, output, outputCapacity, bodyCursor, worldDeltaPlayers, worldDeltaPlayerCount))
			return fail("server-snapshot-presentation-echo-build");
	}
	if (!Net_IsInvasionModeEnabled()
		&& !HCDEAppendSharedActorDeltasV2(client, output,
			HCDELiveLaneBudgetEnd(client, HLANE_ACTOR_DELTA, bodyCursor, outputCapacity), bodyCursor))
	{
		return fail("server-snapshot-actor-delta-build");
	}
	if (Net_IsInvasionModeEnabled()
		&& !HCDEAppendInvasionSnapshot(client, output, outputCapacity, bodyCursor))
	{
		return fail("server-snapshot-invasion-build");
	}

	const size_t bodyBytes = bodyCursor - HCDEServerSnapshotHeaderSize - quitterBytes;
	if (bodyBytes > UINT16_MAX)
		return fail("server-snapshot-body-too-large");

	memcpy(&output[HCDEServerSnapshotMagicOffset], HCDEServerSnapshotMagic, sizeof(HCDEServerSnapshotMagic));
	output[HCDEServerSnapshotVersionOffset] = HCDEServerSnapshotProtocolVersion;
	output[HCDEServerSnapshotFlagsOffset] = controlFlags;
	output[HCDEServerSnapshotRoutingOffset] = routingByte;
	output[HCDEServerSnapshotPlayerCountOffset] = playerCount;
	HCDELiveWriteBE32(&output[HCDEServerSnapshotSequenceAckOffset], sequenceAck);
	HCDELiveWriteBE32(&output[HCDEServerSnapshotConsistencyAckOffset], consistencyAck);
	HCDELiveWriteBE16(&output[HCDEServerSnapshotQuitterBytesOffset], uint16_t(quitterBytes));
	HCDELiveWriteBE32(&output[HCDEServerSnapshotBaseSequenceOffset], baseSequence);
	HCDELiveWriteBE32(&output[HCDEServerSnapshotBaseConsistencyOffset], baseConsistency);
	output[HCDEServerSnapshotCommandTicsOffset] = commandTics;
	output[HCDEServerSnapshotConsistencyTicsOffset] = consistencyTics;
	output[HCDEServerSnapshotStabilityOffset] = stabilityBuffer;
	HCDELiveWriteBE16(&output[HCDEServerSnapshotBodyBytesOffset], uint16_t(bodyBytes));
	if (quitterBytes > 0u)
	{
		// Quitter inputs were validated up-front above; just emit them.
		output[HCDEServerSnapshotHeaderSize] = quitterCount;
		for (uint8_t i = 0u; i < quitterCount; ++i)
			output[HCDEServerSnapshotHeaderSize + 1u + i] = uint8_t(quitNums[i]);
	}

	outputSize = bodyCursor;
	++HCDELiveProfile.ServerSnapshotPacketsBuilt;
	++HCDELiveProfile.ServerSnapshotNativeBuilt;
	HCDELiveProfile.ServerSnapshotBytesBuilt += outputSize;
	HCDERecordLiveLaneTx(HLANE_PLAYER_SNAPSHOT, client, playerSnapshotEnd);
	DebugTrace::Markf("net.snapshot", "HCDE server snapshot native build client=%d players=%u tics=%u consistencies=%u quitters=%u records=%zu deltas=%zu",
		client, unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), unsigned(quitterCount), bodyBytes, worldDeltaPlayerCount);
	return true;
}

// Build a native HLIVE_CLIENT_COMMANDS body for the local non-authority player from
// LocalCmds + NetEvents data passed in by NetUpdate. The header bytes are fixed up
// at the end once `bodyBytes` is known. Returns false (with outputSize=0) on any
// encoding failure, leaving the caller to invoke HCDEAbortLiveGameplaySend instead
// of attempting any legacy NCMD fallback.
static bool HCDEBuildNativeClientInputPayload(int client, uint8_t controlFlags, uint8_t routingByte,
	uint32_t sequenceAck, uint32_t consistencyAck, uint32_t baseSequence, uint32_t baseConsistency,
	uint8_t commandTics, uint8_t consistencyTics, uint8_t stabilityBuffer, int playerNum, int sequenceOffset,
	const usercmd_t* commands, const uint8_t* const* eventStreams, const size_t* eventSizes,
	uint8_t* output, size_t outputCapacity, size_t& outputSize, const char*& failureReason)
{
	outputSize = 0u;
	failureReason = nullptr;
	auto fail = [&](const char* reason) -> bool
	{
		failureReason = reason;
		return false;
	};
	if (playerNum < 0 || playerNum >= MAXPLAYERS
		|| commandTics > MAXSENDTICS
		|| consistencyTics > MAXSENDTICS
		|| (commandTics > 0u && (commands == nullptr || eventStreams == nullptr || eventSizes == nullptr))
		|| HCDEClientInputHeaderSize > outputCapacity)
	{
		return fail("client-input-invalid-build-input");
	}

	size_t bodyCursor = HCDEClientInputHeaderSize;
	if (!HCDEAppendBytes(output, outputCapacity, bodyCursor, HCDEClientInputRecordsMagic, sizeof(HCDEClientInputRecordsMagic))
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, HCDEClientInputRecordsProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, 1u)
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, uint8_t(playerNum))
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, consistencyTics)
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, commandTics))
	{
		return fail("client-input-record-header-overflow");
	}

	auto& clientState = ClientStates[playerNum];
	for (uint8_t r = 0u; r < consistencyTics; ++r)
	{
		const int tic = int(baseConsistency) + int(r);
		if (!HCDEAppendByte(output, outputCapacity, bodyCursor, r)
			|| !HCDEAppendBE16(output, outputCapacity, bodyCursor, uint16_t(clientState.LocalConsistency[tic % BACKUPTICS])))
		{
			return fail("client-input-consistency-record-overflow");
		}
	}

	for (uint8_t t = 0u; t < commandTics; ++t)
	{
		if (!HCDEAppendByte(output, outputCapacity, bodyCursor, t)
			|| !HCDEAppendEventRecords(output, outputCapacity, bodyCursor, eventStreams[t], eventSizes[t], /*clientInput=*/true)
			|| !HCDEAppendUserCmdFields(output, outputCapacity, bodyCursor, commands[t]))
		{
			return fail("client-input-command-record-overflow");
		}
	}

	const size_t bodyBytes = bodyCursor - HCDEClientInputHeaderSize;
	if (bodyBytes > UINT16_MAX)
		return fail("client-input-body-too-large");

	memcpy(&output[HCDEClientInputMagicOffset], HCDEClientInputMagic, sizeof(HCDEClientInputMagic));
	output[HCDEClientInputVersionOffset] = HCDEClientInputProtocolVersion;
	output[HCDEClientInputFlagsOffset] = controlFlags;
	output[HCDEClientInputRoutingOffset] = routingByte;
	output[HCDEClientInputPlayerCountOffset] = 1u;
	HCDELiveWriteBE32(&output[HCDEClientInputSequenceAckOffset], sequenceAck);
	HCDELiveWriteBE32(&output[HCDEClientInputConsistencyAckOffset], consistencyAck);
	HCDELiveWriteBE32(&output[HCDEClientInputBaseSequenceOffset], baseSequence + uint32_t(sequenceOffset));
	HCDELiveWriteBE32(&output[HCDEClientInputBaseConsistencyOffset], baseConsistency);
	output[HCDEClientInputCommandTicsOffset] = commandTics;
	output[HCDEClientInputConsistencyTicsOffset] = consistencyTics;
	output[HCDEClientInputStabilityOffset] = stabilityBuffer;
	HCDELiveWriteBE16(&output[HCDEClientInputBodyBytesOffset], uint16_t(bodyBytes));
	outputSize = HCDEClientInputHeaderSize + bodyBytes;
	++HCDELiveProfile.ClientInputPacketsBuilt;
	++HCDELiveProfile.ClientInputNativeBuilt;
	HCDELiveProfile.ClientInputBytesBuilt += outputSize;
	HCDERecordLiveLaneTx(HLANE_COMMAND, client, outputSize);
	DebugTrace::Markf("net", "HCDE client input native build client=%d player=%d tics=%u consistencies=%u records=%zu",
		client, playerNum, unsigned(commandTics), unsigned(consistencyTics), bodyBytes);
	return true;
}

static bool Net_IsLateJoinSyncPending(int client);
static void Net_ClearLateJoinSyncPending(int client, const char* reason);
static void Net_ResetAuthorityWaitWatchdog(const char* reason, bool trace);
static void Net_EnsureRuntimeClientSlot(int client, int sourceClient);
static void DisconnectClient(int clientNum);

// Translate `eventCount` canonical HCDE tic events starting at `body[bodyCursor]`
// into the legacy NCMD/DEM_* layout expected by Net_DoCommand and store the result
// into `output[outputCursor..]`. Advances `bodyCursor` past each event regardless
// of whether the translation succeeds; the caller treats a false return as a
// malformed packet. The `clientInput` flag selects the receive-side allow-list:
// client-input bodies are gated by HCDEIsAllowedClientInputEventType, while server
// snapshot bodies use the broader HCDEIsAllowedTicEventType.
static bool HCDEAppendNativeCommandEventsToLegacyBuffer(const uint8_t* body, size_t bodyBytes, size_t& bodyCursor,
	size_t eventCount, bool clientInput, uint8_t* output, size_t outputCapacity, size_t& outputCursor)
{
	for (size_t e = 0u; e < eventCount; ++e)
	{
		if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u)
			return false;

		const uint8_t eventType = body[bodyCursor++];
		const size_t eventPayloadBytes = HCDELiveReadBE16(&body[bodyCursor]);
		bodyCursor += 2u;
		const bool allowed = clientInput
			? HCDEIsAllowedClientInputEventType(eventType)
			: HCDEIsAllowedTicEventType(eventType);
		if (!allowed
			|| bodyCursor > bodyBytes
			|| eventPayloadBytes > bodyBytes - bodyCursor)
		{
			return false;
		}

		if (!HCDEAppendByte(output, outputCapacity, outputCursor, eventType)
			|| !HCDEAppendLegacyEventPayload(eventType, &body[bodyCursor], eventPayloadBytes, output, outputCapacity, outputCursor))
		{
			return false;
		}
		bodyCursor += eventPayloadBytes;
	}
	return true;
}

// Apply the shared NCMD-style header bits carried by native HCDE gameplay packets:
// the retransmit flag/resend identifier, room-id staleness check, sequence ack,
// late-join sync gating (authority side), and the authority watchdog (non-
// authority side). Mirrors the inline logic in the legacy NetUpdate receive loop;
// `staleRoom` is set true when the routing byte does not match `CurrentRoomID`,
// in which case the caller must skip applying the rest of the payload.
static void HCDEApplyNativeGameplayHeader(int clientNum, uint8_t controlFlags, uint8_t routingByte, uint32_t sequenceAck, uint32_t consistencyAck, bool& staleRoom)
{
	staleRoom = false;
	auto& clientState = ClientStates[clientNum];
	if ((controlFlags & NCMD_RETRANSMIT) != 0u)
	{
		clientState.ResendID = routingByte;
		clientState.Flags |= CF_RETRANSMIT;
	}
	if (routingByte != CurrentRoomID)
	{
		staleRoom = true;
		DebugTrace::Markf("net", "ignored stale-room native packet client=%d packet-room=%u current-room=%u gametic=%d clienttic=%d",
			clientNum, unsigned(routingByte), unsigned(CurrentRoomID), gametic, ClientTic);
		return;
	}

	clientState.Flags |= CF_UPDATED;
	clientState.SequenceAck = int(sequenceAck);
	if (I_IsLocalHCDEServiceAuthority() && Net_IsLateJoinSyncPending(clientNum))
	{
		const int targetSeq = LateJoinSyncTargetSequence[clientNum];
		const int targetCon = LateJoinSyncTargetConsistency[clientNum];
		const bool seqReady = targetSeq < 0 || clientState.SequenceAck >= targetSeq;
		const bool conReady = targetCon < 0 || int(consistencyAck) >= targetCon;
		constexpr int LateJoinPromotionTimeoutTics = MAXSENDTICS * 12;
		const bool timedOut = LateJoinSyncStartTic[clientNum] >= 0
			&& EnterTic - LateJoinSyncStartTic[clientNum] >= LateJoinPromotionTimeoutTics;

		if (!seqReady)
		{
			const int resendFrom = max<int>(clientState.SequenceAck + 1, 0);
			if (clientState.ResendSequenceFrom < 0 || clientState.ResendSequenceFrom > resendFrom)
				clientState.ResendSequenceFrom = resendFrom;
			clientState.Flags |= CF_RETRANSMIT_SEQ;
		}
		if (!conReady)
		{
			const int resendFrom = max<int>(int(consistencyAck) + 1, 0);
			if (clientState.ResendConsistencyFrom < 0 || clientState.ResendConsistencyFrom > resendFrom)
				clientState.ResendConsistencyFrom = resendFrom;
			clientState.Flags |= CF_RETRANSMIT_CON;
		}

		if (seqReady && conReady)
		{
			players[clientNum].waiting = false;
			Net_ClearLateJoinSyncPending(clientNum, "acks-caught-up-native");
		}
		else if (timedOut)
		{
			DebugTrace::Warningf("net", "late-join native sync promotion timed out client=%d seq=%d/%d con=%d/%d room=%u",
				clientNum, clientState.SequenceAck, targetSeq, int(consistencyAck), targetCon, unsigned(CurrentRoomID));
			players[clientNum].waiting = false;
			Net_ClearLateJoinSyncPending(clientNum, "promotion-timeout-native");
		}
	}
	if (!I_IsLocalHCDEServiceAuthority() && I_IsHCDEServiceAuthoritySlot(clientNum))
	{
		const bool wasWaiting = players[clientNum].waiting;
		Net_ResetAuthorityWaitWatchdog("authority-native-packet", wasWaiting);
		if (wasWaiting)
		{
			DebugTrace::Markf("net", "authority wait cleared by native current-room packet client=%d seq=%d ack=%d room=%u",
				clientNum, clientState.CurrentSequence, clientState.SequenceAck, unsigned(CurrentRoomID));
		}
	}
}

// Decode an HLIVE_CLIENT_COMMANDS payload directly into ClientStates without ever
// repacking through the classic NCMD NetBuffer layout. Two passes: the first
// validates structural invariants (record uniqueness, bounded offsets, well-formed
// events/user commands) so the second pass can mutate state without ever rolling
// back on a malformed tail.
static bool HCDETryApplyNativeClientInputPayload(int clientNum, const uint8_t* payload, size_t inputPayloadSize,
	const uint8_t* body, size_t bodyBytes, uint32_t remoteGameTic, const char*& failureReason)
{
	failureReason = nullptr;
	auto fail = [&](const char* reason) -> bool
	{
		failureReason = reason;
		return false;
	};
	const uint8_t controlFlags = payload[HCDEClientInputFlagsOffset];
	const uint8_t routingByte = payload[HCDEClientInputRoutingOffset];
	const uint8_t playerCount = payload[HCDEClientInputPlayerCountOffset];
	const uint32_t sequenceAck = HCDELiveReadBE32(&payload[HCDEClientInputSequenceAckOffset]);
	const uint32_t consistencyAck = HCDELiveReadBE32(&payload[HCDEClientInputConsistencyAckOffset]);
	const uint32_t baseSequence = HCDELiveReadBE32(&payload[HCDEClientInputBaseSequenceOffset]);
	const uint32_t baseConsistency = HCDELiveReadBE32(&payload[HCDEClientInputBaseConsistencyOffset]);
	const uint8_t commandTics = payload[HCDEClientInputCommandTicsOffset];
	const uint8_t consistencyTics = payload[HCDEClientInputConsistencyTicsOffset];
	const uint8_t stabilityBuffer = payload[HCDEClientInputStabilityOffset];

	// Reused across every (player, tic) pair to avoid 14 KB of stack zero-init on every
	// inner iteration; eventCursor tracks the live byte count for each event payload.
	uint8_t eventScratch[MAX_MSGLEN];

	size_t bodyCursor = HCDEClientInputRecordsHeaderSize;
	uint64_t playersSeen = 0u;
	for (uint8_t p = 0u; p < playerCount; ++p)
	{
		if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u)
			return fail("client-input-record-truncated");

		const uint8_t playerNum = body[bodyCursor++];
		const uint8_t consistencyCount = body[bodyCursor++];
		const uint8_t commandCount = body[bodyCursor++];
		if (playerNum >= MAXPLAYERS || playerNum >= 64u || consistencyCount != consistencyTics || commandCount != commandTics)
			return fail("client-input-invalid-player-record");
		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if (playersSeen & playerMask)
			return fail("client-input-duplicate-player-record");
		playersSeen |= playerMask;
		if (!HCDEInputRecordAuthorized(clientNum, playerNum, playerCount))
			return fail("client-input-unauthorized-player-record");

		uint64_t consistencyOffsetsSeen = 0u;
		for (uint8_t r = 0u; r < consistencyCount; ++r)
		{
			if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u || body[bodyCursor] >= consistencyTics)
				return fail("client-input-consistency-truncated");
			const uint64_t consistencyMask = uint64_t(1u) << body[bodyCursor];
			if (consistencyOffsetsSeen & consistencyMask)
				return fail("client-input-duplicate-consistency-offset");
			consistencyOffsetsSeen |= consistencyMask;
			bodyCursor += 3u;
		}

		uint64_t commandOffsetsSeen = 0u;
		for (uint8_t t = 0u; t < commandCount; ++t)
		{
			if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u + HCDEExplicitUserCmdBytes)
				return fail("client-input-command-truncated");

			const uint8_t commandOffset = body[bodyCursor++];
			const size_t eventCount = HCDELiveReadBE16(&body[bodyCursor]);
			bodyCursor += 2u;
			if (commandOffset >= commandTics)
				return fail("client-input-command-offset-out-of-range");
			const uint64_t commandMask = uint64_t(1u) << commandOffset;
			if (commandOffsetsSeen & commandMask)
				return fail("client-input-duplicate-command-offset");
			commandOffsetsSeen |= commandMask;

			size_t eventCursor = 0u;
			if (!HCDEAppendNativeCommandEventsToLegacyBuffer(body, bodyBytes, bodyCursor,
				eventCount, true, eventScratch, sizeof(eventScratch), eventCursor))
			{
				return fail("client-input-event-records-invalid");
			}

			usercmd_t command = {};
			if (!HCDEReadUserCmdFields(body, bodyBytes, bodyCursor, command))
				return fail("client-input-usercmd-invalid");
		}
	}
	if (bodyCursor != bodyBytes)
		return fail("client-input-body-trailing-bytes");

	bool staleRoom = false;
	HCDEApplyNativeGameplayHeader(clientNum, controlFlags, routingByte, sequenceAck, consistencyAck, staleRoom);
	if (staleRoom)
		return fail("client-input-stale-room");
	if (I_IsLocalHCDEServiceAuthority())
		ClientStates[clientNum].StabilityBuffer = stabilityBuffer;

	bodyCursor = HCDEClientInputRecordsHeaderSize;
	for (uint8_t p = 0u; p < playerCount; ++p)
	{
		const uint8_t playerNum = body[bodyCursor++];
		const uint8_t consistencyCount = body[bodyCursor++];
		const uint8_t commandCount = body[bodyCursor++];
		auto& pState = ClientStates[playerNum];

		if (!I_IsLocalHCDEServiceAuthority()
			|| I_IsHCDEServiceAuthoritySlot(playerNum) || !I_IsHCDEServiceAuthoritySlot(clientNum))
		{
			pState.ConsistencyAck = int(consistencyAck);
		}

		int16_t consistencies[MAXSENDTICS] = {};
		bool consistencyPresent[MAXSENDTICS] = {};
		for (uint8_t r = 0u; r < consistencyCount; ++r)
		{
			const uint8_t offset = body[bodyCursor++];
			consistencies[offset] = int16_t(HCDELiveReadBE16(&body[bodyCursor]));
			consistencyPresent[offset] = true;
			bodyCursor += 2u;
		}
		for (uint8_t i = 0u; i < consistencyTics; ++i)
		{
			const int cTic = int(baseConsistency) + int(i);
			if (cTic <= pState.CurrentNetConsistency)
				continue;
			if (cTic > pState.CurrentNetConsistency + 1 || !consistencyPresent[i] || consistencies[i] == 0)
			{
				ClientStates[clientNum].Flags |= CF_MISSING_CON;
				break;
			}
			pState.NetConsistency[cTic % BACKUPTICS] = consistencies[i];
			pState.CurrentNetConsistency = cTic;
		}

		usercmd_t commands[MAXSENDTICS] = {};
		bool commandPresent[MAXSENDTICS] = {};
		FDynamicBuffer commandEvents[MAXSENDTICS] = {};
		for (uint8_t t = 0u; t < commandCount; ++t)
		{
			const uint8_t commandOffset = body[bodyCursor++];
			const size_t eventCount = HCDELiveReadBE16(&body[bodyCursor]);
			bodyCursor += 2u;
			size_t eventCursor = 0u;
			if (!HCDEAppendNativeCommandEventsToLegacyBuffer(body, bodyBytes, bodyCursor,
				eventCount, true, eventScratch, sizeof(eventScratch), eventCursor))
			{
				// Validation pass should have rejected this already; bail without mutating
				// pState further so the caller can request replay.
				return fail("client-input-apply-event-records-invalid");
			}
			commandEvents[commandOffset].SetData(eventScratch, int(eventCursor));
			commandPresent[commandOffset] = HCDEReadUserCmdFields(body, bodyBytes, bodyCursor, commands[commandOffset]);
		}
		for (uint8_t i = 0u; i < commandTics; ++i)
		{
			const int seq = int(baseSequence) + int(i);
			if (seq <= pState.CurrentSequence)
				continue;
			if (seq > pState.CurrentSequence + 1 || !commandPresent[i])
			{
				ClientStates[clientNum].Flags |= CF_MISSING_SEQ;
				break;
			}

			auto& curTic = pState.Tics[seq % BACKUPTICS];
			curTic.Data.SetData(nullptr, 0u);
			int eventLen = 0;
			uint8_t* eventData = commandEvents[i].GetData(&eventLen);
			if (eventLen > 0)
				curTic.Data.SetData(eventData, eventLen);
			curTic.Command = commands[i];
			if (!I_IsLocalHCDEServiceAuthority()
				|| I_IsHCDEServiceAuthoritySlot(playerNum) || !I_IsHCDEServiceAuthoritySlot(clientNum))
			{
				pState.CurrentSequence = seq;
			}
			if (!I_IsLocalHCDEServiceAuthority() && !I_IsHCDEServiceAuthoritySlot(playerNum))
				pState.SequenceAck = seq;
		}
	}

	// Successful apply clears any malformed-packet pressure that earlier rejects raised.
	ClientStates[clientNum].MalformedPacketStrikes = 0u;
	ClientStates[clientNum].MalformedWindowStartMS = 0u;
	ClearHCDELiveReplayPressure(clientNum, "client-input-native-apply");
	DebugTrace::Markf("net", "HCDE client input native-apply client=%d room=%u tic=%u players=%u tics=%u consistencies=%u records=%zu",
		clientNum, unsigned(CurrentRoomID), remoteGameTic, unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), bodyBytes);
	++HCDELiveProfile.ClientInputNativeApplied;
	HCDELiveProfile.ClientInputPacketsReceived++;
	HCDELiveProfile.ClientInputBytesReceived += inputPayloadSize;
	HCDERecordLiveLaneRx(HLANE_COMMAND, clientNum, inputPayloadSize);
	return true;
}

// Validate the HCDE client-input envelope produced by HSendNativeClientInputPacket
// and apply it natively. Returns true only when the payload was fully consumed and
// the per-player tic data was committed to ClientStates; the caller treats any
// false return as a malformed packet worth a replay request.
static bool UnwrapHCDELiveClientInputPayload(int clientNum, size_t payloadSize, const char*& failureReason)
{
	failureReason = nullptr;
	auto& peer = HCDELivePeers[clientNum];
	const uint8_t* payload = nullptr;
	size_t inputPayloadSize = 0u;
	uint32_t remoteGameTic = 0u;
	if (!UnwrapHCDEGameplayEnvelope(clientNum, payloadSize, "client input", HGP_CLIENT_INPUTS, payload, inputPayloadSize, remoteGameTic))
	{
		failureReason = "client-input-envelope-invalid";
		return false;
	}

	if (inputPayloadSize < HCDEClientInputHeaderSize
		|| memcmp(&payload[HCDEClientInputMagicOffset], HCDEClientInputMagic, sizeof(HCDEClientInputMagic)) != 0)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "malformed HCDE client input from client=%d size=%zu", clientNum, inputPayloadSize);
		failureReason = "client-input-magic-invalid";
		return false;
	}

	// Only the fields used for header validation and diagnostic logging are
	// extracted here; everything else (sequences, routing, stability) is consumed
	// inside HCDETryApplyNativeClientInputPayload from the same payload buffer.
	const uint8_t inputVersion = payload[HCDEClientInputVersionOffset];
	const uint8_t controlFlags = payload[HCDEClientInputFlagsOffset];
	const uint8_t playerCount = payload[HCDEClientInputPlayerCountOffset];
	const uint8_t commandTics = payload[HCDEClientInputCommandTicsOffset];
	const uint8_t consistencyTics = payload[HCDEClientInputConsistencyTicsOffset];
	const size_t bodyBytes = HCDELiveReadBE16(&payload[HCDEClientInputBodyBytesOffset]);
	const uint8_t disallowedControlFlags = NCMD_EXIT | NCMD_SETUP | NCMD_LEVELREADY | NCMD_QUITTERS | NCMD_LATENCY | NCMD_LATENCYACK | NCMD_COMPRESSED;
	auto rejectClientInput = [&](const char* reason)
	{
		failureReason = reason;
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE client input from client=%d reason=%s version=%u flags=%u players=%u tics=%u consistencies=%u body=%zu size=%zu",
			clientNum, reason != nullptr ? reason : "unknown", unsigned(inputVersion), unsigned(controlFlags & disallowedControlFlags),
			unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), bodyBytes, inputPayloadSize);
		return false;
	};

	if (inputVersion != HCDEClientInputProtocolVersion)
		return rejectClientInput("client-input-version-mismatch");
	if (controlFlags & disallowedControlFlags)
		return rejectClientInput("client-input-disallowed-control-flags");
	if (playerCount > MAXPLAYERS)
		return rejectClientInput("client-input-player-count-overflow");
	if (commandTics > MAXSENDTICS)
		return rejectClientInput("client-input-command-tics-overflow");
	if (consistencyTics > MAXSENDTICS)
		return rejectClientInput("client-input-consistency-tics-overflow");
	if (playerCount == 0u && (commandTics != 0u || consistencyTics != 0u))
		return rejectClientInput("client-input-empty-player-records-with-tics");
	if (HCDEClientInputHeaderSize + bodyBytes != inputPayloadSize)
		return rejectClientInput("client-input-body-length-mismatch");

	const uint8_t* body = &payload[HCDEClientInputHeaderSize];
	if (bodyBytes < HCDEClientInputRecordsHeaderSize
		|| memcmp(&body[HCDEClientInputRecordsMagicOffset], HCDEClientInputRecordsMagic, sizeof(HCDEClientInputRecordsMagic)) != 0)
		return rejectClientInput("missing-records");

	const uint8_t recordsVersion = body[HCDEClientInputRecordsVersionOffset];
	const uint8_t recordPlayerCount = body[HCDEClientInputRecordsPlayerCountOffset];
	if (recordsVersion != HCDEClientInputRecordsProtocolVersion)
		return rejectClientInput("client-input-record-version-mismatch");
	if (recordPlayerCount != playerCount)
		return rejectClientInput("client-input-record-player-count-mismatch");

	if (HCDETryApplyNativeClientInputPayload(clientNum, payload, inputPayloadSize, body, bodyBytes, remoteGameTic, failureReason))
		return true;

	return rejectClientInput(failureReason != nullptr ? failureReason : "client-input-native-apply-failed");
}

// Decode an HLIVE_SERVER_SNAPSHOT payload directly into ClientStates and the world
// delta/actor delta/invasion appliers without ever materialising a classic NCMD
// NetBuffer. As with the client-input variant, a validation pass guards the apply
// pass so a malformed tail never leaves partially-mutated state behind.
static bool HCDETryApplyNativeServerSnapshotPayload(int clientNum, const uint8_t* payload, size_t snapshotPayloadSize,
	const uint8_t* body, size_t bodyBytes, size_t quitterBytes, uint32_t remoteGameTic, const char*& failureReason)
{
	failureReason = nullptr;
	auto fail = [&](const char* reason) -> bool
	{
		failureReason = reason;
		return false;
	};
	const uint8_t controlFlags = payload[HCDEServerSnapshotFlagsOffset];
	const uint8_t routingByte = payload[HCDEServerSnapshotRoutingOffset];
	const uint8_t playerCount = payload[HCDEServerSnapshotPlayerCountOffset];
	const uint32_t sequenceAck = HCDELiveReadBE32(&payload[HCDEServerSnapshotSequenceAckOffset]);
	const uint32_t consistencyAck = HCDELiveReadBE32(&payload[HCDEServerSnapshotConsistencyAckOffset]);
	const uint32_t baseSequence = HCDELiveReadBE32(&payload[HCDEServerSnapshotBaseSequenceOffset]);
	const uint32_t baseConsistency = HCDELiveReadBE32(&payload[HCDEServerSnapshotBaseConsistencyOffset]);
	const uint8_t commandTics = payload[HCDEServerSnapshotCommandTicsOffset];
	const uint8_t consistencyTics = payload[HCDEServerSnapshotConsistencyTicsOffset];
	const uint8_t stabilityBuffer = payload[HCDEServerSnapshotStabilityOffset];

	auto& livePeer = HCDELivePeers[clientNum];
	if (!I_IsLocalHCDEServiceAuthority() && remoteGameTic <= livePeer.LastAppliedSnapshotGameTic)
	{
		bool staleRoom = false;
		HCDEApplyNativeGameplayHeader(clientNum, controlFlags, routingByte, sequenceAck, consistencyAck, staleRoom);
		if (staleRoom)
			return fail("server-snapshot-stale-room");
		ClearHCDELiveReplayPressure(clientNum, remoteGameTic < livePeer.LastAppliedSnapshotGameTic
			? "server-snapshot-stale-tic" : "server-snapshot-idempotent");
		++livePeer.DuplicateCount;
		return true;
	}

	// Reused across every (player, tic) pair to avoid 14 KB of stack zero-init on every
	// inner iteration; eventCursor tracks the live byte count for each event payload.
	uint8_t eventScratch[MAX_MSGLEN];

	size_t bodyCursor = HCDEServerSnapshotRecordsHeaderSize;
	uint64_t playersSeen = 0u;
	for (uint8_t p = 0u; p < playerCount; ++p)
	{
		if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 5u)
			return fail("server-snapshot-player-record-truncated");
		const uint8_t playerNum = body[bodyCursor++];
		bodyCursor += 2u; // latency (validated during apply pass)
		const uint8_t consistencyCount = body[bodyCursor++];
		const uint8_t commandCount = body[bodyCursor++];
		if (playerNum >= MAXPLAYERS || playerNum >= 64u || consistencyCount != consistencyTics || commandCount != commandTics)
			return fail("server-snapshot-invalid-player-record");
		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if (playersSeen & playerMask)
			return fail("server-snapshot-duplicate-player-record");
		playersSeen |= playerMask;

		uint64_t consistencyOffsetsSeen = 0u;
		for (uint8_t r = 0u; r < consistencyCount; ++r)
		{
			if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u || body[bodyCursor] >= consistencyTics)
				return fail("server-snapshot-consistency-truncated");
			const uint64_t consistencyMask = uint64_t(1u) << body[bodyCursor];
			if (consistencyOffsetsSeen & consistencyMask)
				return fail("server-snapshot-duplicate-consistency-offset");
			consistencyOffsetsSeen |= consistencyMask;
			bodyCursor += 3u;
		}

		uint64_t commandOffsetsSeen = 0u;
		for (uint8_t t = 0u; t < commandCount; ++t)
		{
			if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u + HCDEExplicitUserCmdBytes)
				return fail("server-snapshot-command-truncated");
			const uint8_t commandOffset = body[bodyCursor++];
			const size_t eventCount = HCDELiveReadBE16(&body[bodyCursor]);
			bodyCursor += 2u;
			if (commandOffset >= commandTics)
				return fail("server-snapshot-command-offset-out-of-range");
			const uint64_t commandMask = uint64_t(1u) << commandOffset;
			if (commandOffsetsSeen & commandMask)
				return fail("server-snapshot-duplicate-command-offset");
			commandOffsetsSeen |= commandMask;

			size_t eventCursor = 0u;
			if (!HCDEAppendNativeCommandEventsToLegacyBuffer(body, bodyBytes, bodyCursor,
				eventCount, false, eventScratch, sizeof(eventScratch), eventCursor))
			{
				return fail("server-snapshot-event-records-invalid");
			}

			usercmd_t command = {};
			if (!HCDEReadUserCmdFields(body, bodyBytes, bodyCursor, command))
				return fail("server-snapshot-usercmd-invalid");
		}
	}
	const size_t playerSnapshotBodyEnd = bodyCursor;

	bool staleRoom = false;
	HCDEApplyNativeGameplayHeader(clientNum, controlFlags, routingByte, sequenceAck, consistencyAck, staleRoom);
	if (staleRoom)
		return fail("server-snapshot-stale-room");

	if ((controlFlags & NCMD_QUITTERS) != 0u && quitterBytes > 0u)
	{
		const uint8_t* quitters = &payload[HCDEServerSnapshotHeaderSize];
		const int numPlayers = quitters[0];
		for (int i = 0; i < numPlayers && size_t(i + 1) < quitterBytes; ++i)
		{
			const int quitter = quitters[i + 1];
			Printf(PRINT_HIGH, "NetGame:: Authority reported client %d quit at gametic=%d clienttic=%d room=%u\n",
				quitter, gametic, ClientTic, unsigned(CurrentRoomID));
			DebugTrace::Warningf("net", "authority quit broadcast client=%d from=%d gametic=%d clienttic=%d room=%u native=1",
				quitter, clientNum, gametic, ClientTic, unsigned(CurrentRoomID));
			DisconnectClient(quitter);
		}
	}

	if (I_IsHCDEServiceAuthoritySlot(clientNum))
		CommandsAhead = stabilityBuffer;
	else if (I_IsLocalHCDEServiceAuthority())
		ClientStates[clientNum].StabilityBuffer = stabilityBuffer;

	bodyCursor = HCDEServerSnapshotRecordsHeaderSize;
	for (uint8_t p = 0u; p < playerCount; ++p)
	{
		const uint8_t playerNum = body[bodyCursor++];
		const uint16_t latency = HCDELiveReadBE16(&body[bodyCursor]);
		bodyCursor += 2u;
		const uint8_t consistencyCount = body[bodyCursor++];
		const uint8_t commandCount = body[bodyCursor++];
		if (I_IsHCDEServiceAuthoritySlot(clientNum))
			Net_EnsureRuntimeClientSlot(playerNum, clientNum);
		auto& pState = ClientStates[playerNum];
		if (I_IsHCDEServiceAuthoritySlot(clientNum) && !I_IsLocalHCDEServiceAuthority())
			pState.AverageLatency = latency;

		if (!I_IsLocalHCDEServiceAuthority()
			|| I_IsHCDEServiceAuthoritySlot(playerNum) || !I_IsHCDEServiceAuthoritySlot(clientNum))
		{
			pState.ConsistencyAck = int(consistencyAck);
		}

		int16_t consistencies[MAXSENDTICS] = {};
		bool consistencyPresent[MAXSENDTICS] = {};
		for (uint8_t r = 0u; r < consistencyCount; ++r)
		{
			const uint8_t offset = body[bodyCursor++];
			consistencies[offset] = int16_t(HCDELiveReadBE16(&body[bodyCursor]));
			consistencyPresent[offset] = true;
			bodyCursor += 2u;
		}
		for (uint8_t i = 0u; i < consistencyTics; ++i)
		{
			const int cTic = int(baseConsistency) + int(i);
			if (cTic <= pState.CurrentNetConsistency)
				continue;
			if (cTic > pState.CurrentNetConsistency + 1 || !consistencyPresent[i] || consistencies[i] == 0)
			{
				ClientStates[clientNum].Flags |= CF_MISSING_CON;
				break;
			}
			pState.NetConsistency[cTic % BACKUPTICS] = consistencies[i];
			pState.CurrentNetConsistency = cTic;
		}

		usercmd_t commands[MAXSENDTICS] = {};
		bool commandPresent[MAXSENDTICS] = {};
		FDynamicBuffer commandEvents[MAXSENDTICS] = {};
		for (uint8_t t = 0u; t < commandCount; ++t)
		{
			const uint8_t commandOffset = body[bodyCursor++];
			const size_t eventCount = HCDELiveReadBE16(&body[bodyCursor]);
			bodyCursor += 2u;
			size_t eventCursor = 0u;
			if (!HCDEAppendNativeCommandEventsToLegacyBuffer(body, bodyBytes, bodyCursor,
				eventCount, false, eventScratch, sizeof(eventScratch), eventCursor))
			{
				// Validation should have caught this; bail without mutating pState further.
				return fail("server-snapshot-apply-event-records-invalid");
			}
			commandEvents[commandOffset].SetData(eventScratch, int(eventCursor));
			commandPresent[commandOffset] = HCDEReadUserCmdFields(body, bodyBytes, bodyCursor, commands[commandOffset]);
		}
		for (uint8_t i = 0u; i < commandTics; ++i)
		{
			const int seq = int(baseSequence) + int(i);
			if (seq <= pState.CurrentSequence)
				continue;
			if (seq > pState.CurrentSequence + 1 || !commandPresent[i])
			{
				ClientStates[clientNum].Flags |= CF_MISSING_SEQ;
				break;
			}
			auto& curTic = pState.Tics[seq % BACKUPTICS];
			curTic.Data.SetData(nullptr, 0u);
			int eventLen = 0;
			uint8_t* eventData = commandEvents[i].GetData(&eventLen);
			if (eventLen > 0)
				curTic.Data.SetData(eventData, eventLen);
			curTic.Command = commands[i];
			if (!I_IsLocalHCDEServiceAuthority()
				|| I_IsHCDEServiceAuthoritySlot(playerNum) || !I_IsHCDEServiceAuthoritySlot(clientNum))
			{
				pState.CurrentSequence = seq;
			}
			if (!I_IsLocalHCDEServiceAuthority() && !I_IsHCDEServiceAuthoritySlot(playerNum))
				pState.SequenceAck = seq;
		}
	}

	bodyCursor = playerSnapshotBodyEnd;
	if (!HCDEValidateServerWorldDeltas(clientNum, body, bodyBytes, bodyCursor, playerCount, playersSeen))
		return fail("server-snapshot-world-delta-invalid");
	const bool expectActorDelta = HCDELivePeerHasCapability(clientNum, HCDELiveCapActorDeltaV2)
		&& HCDELivePeerHasCapability(clientNum, HCDELiveCapActorRegistryV1);
	const bool hasInvasionSnapshot = bodyCursor < bodyBytes
		&& bodyBytes - bodyCursor >= HCDEInvasionSnapshotHeaderV1Size
		&& memcmp(&body[bodyCursor + HCDEInvasionSnapshotMagicOffset], HCDEInvasionSnapshotMagic, sizeof(HCDEInvasionSnapshotMagic)) == 0u;
	const bool expectInvasionSnapshot = hasInvasionSnapshot
		&& HCDELivePeerHasCapability(clientNum, HCDELiveCapInvasionSnapshotV2);
	if (bodyCursor < bodyBytes
		&& expectActorDelta
		&& bodyBytes - bodyCursor >= HCDEActorDeltasHeaderSize
		&& memcmp(&body[bodyCursor + HCDEActorDeltasMagicOffset], HCDEActorDeltasMagic, sizeof(HCDEActorDeltasMagic)) == 0)
	{
		if (!HCDEApplyActorDeltasV2(clientNum, body, bodyBytes, bodyCursor))
			return fail("server-snapshot-actor-delta-invalid");
	}
	if (expectInvasionSnapshot
		&& !HCDEApplyInvasionSnapshot(clientNum, body, bodyBytes, bodyCursor))
	{
		return fail("server-snapshot-invasion-invalid");
	}
	else if (hasInvasionSnapshot && !HCDELivePeerHasCapability(clientNum, HCDELiveCapInvasionSnapshotV2))
	{
		return fail("server-snapshot-invasion-capability-missing");
	}
	if (bodyCursor < bodyBytes
		&& bodyBytes - bodyCursor >= sizeof(HCDEPresentationEchoMagic)
		&& memcmp(&body[bodyCursor], HCDEPresentationEchoMagic, sizeof(HCDEPresentationEchoMagic)) == 0)
	{
		if (!HCDEReadPresentationEcho(clientNum, body, bodyBytes, bodyCursor))
			return fail("server-snapshot-presentation-echo-invalid");
	}
	if (bodyCursor != bodyBytes)
		return fail("server-snapshot-body-trailing-bytes");

	ClientStates[clientNum].MalformedPacketStrikes = 0u;
	ClientStates[clientNum].MalformedWindowStartMS = 0u;
	if (!I_IsLocalHCDEServiceAuthority())
		livePeer.LastAppliedSnapshotGameTic = max(livePeer.LastAppliedSnapshotGameTic, remoteGameTic);
	ClearHCDELiveReplayPressure(clientNum, "server-snapshot-native-apply");
	DebugTrace::Markf("net.snapshot", "HCDE server snapshot native-apply client=%d room=%u tic=%u players=%u tics=%u consistencies=%u quitters=%zu records=%zu",
		clientNum, unsigned(CurrentRoomID), remoteGameTic, unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), quitterBytes, bodyBytes);
	++HCDELiveProfile.ServerSnapshotNativeApplied;
	++HCDELiveProfile.ServerSnapshotPacketsReceived;
	HCDELiveProfile.ServerSnapshotBytesReceived += snapshotPayloadSize;
	HCDERecordLiveLaneRx(HLANE_PLAYER_SNAPSHOT, clientNum, HCDEServerSnapshotHeaderSize + quitterBytes + playerSnapshotBodyEnd);
	return true;
}

// Validate the HCDE server-snapshot envelope produced by HSendNativeServerSnapshotPacket
// and apply it natively. Returns true only when the payload was fully consumed and the
// player/quitter/world/actor/invasion sections were committed; any false return is
// treated by the caller as a malformed packet worth a replay request.
static bool UnwrapHCDELiveServerSnapshotPayload(int clientNum, size_t payloadSize, const char*& failureReason)
{
	failureReason = nullptr;
	auto& peer = HCDELivePeers[clientNum];
	const uint8_t* payload = nullptr;
	size_t snapshotPayloadSize = 0u;
	uint32_t remoteGameTic = 0u;
	if (!UnwrapHCDEGameplayEnvelope(clientNum, payloadSize, "server snapshot", HGP_SERVER_SNAPSHOT, payload, snapshotPayloadSize, remoteGameTic))
	{
		failureReason = "server-snapshot-envelope-invalid";
		return false;
	}

	if (snapshotPayloadSize < HCDEServerSnapshotHeaderSize
		|| memcmp(&payload[HCDEServerSnapshotMagicOffset], HCDEServerSnapshotMagic, sizeof(HCDEServerSnapshotMagic)) != 0)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "malformed HCDE server snapshot from client=%d size=%zu", clientNum, snapshotPayloadSize);
		failureReason = "server-snapshot-magic-invalid";
		return false;
	}

	// Only the fields used for header validation and diagnostic logging are
	// extracted here; everything else (sequences, routing, stability, base
	// sequence/consistency) is consumed inside HCDETryApplyNativeServerSnapshotPayload
	// from the same payload buffer.
	const uint8_t snapshotVersion = payload[HCDEServerSnapshotVersionOffset];
	const uint8_t controlFlags = payload[HCDEServerSnapshotFlagsOffset];
	const uint8_t playerCount = payload[HCDEServerSnapshotPlayerCountOffset];
	const size_t quitterBytes = HCDELiveReadBE16(&payload[HCDEServerSnapshotQuitterBytesOffset]);
	const uint8_t commandTics = payload[HCDEServerSnapshotCommandTicsOffset];
	const uint8_t consistencyTics = payload[HCDEServerSnapshotConsistencyTicsOffset];
	const size_t bodyBytes = HCDELiveReadBE16(&payload[HCDEServerSnapshotBodyBytesOffset]);
	const uint8_t disallowedControlFlags = NCMD_EXIT | NCMD_SETUP | NCMD_LEVELREADY | NCMD_LATENCY | NCMD_LATENCYACK | NCMD_COMPRESSED;
	auto rejectServerSnapshot = [&](const char* reason)
	{
		failureReason = reason;
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE server snapshot from client=%d reason=%s version=%u flags=%u players=%u tics=%u consistencies=%u quitters=%zu body=%zu size=%zu",
			clientNum, reason != nullptr ? reason : "unknown", unsigned(snapshotVersion), unsigned(controlFlags & disallowedControlFlags),
			unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), quitterBytes, bodyBytes, snapshotPayloadSize);
		if (!I_IsLocalHCDEServiceAuthority()
			&& HCDELiveReportIntervalElapsed(LastHCDELiveSnapshotRejectReportMS, 2000u))
		{
			Printf(PRINT_HIGH,
				"HCDE net snapshot rejected: reason=%s from=%d snapshot-seq=%u any-seq=%u ack=%u players=%u tics=%u body=%zu size=%zu gametic=%d clienttic=%d unsupported=%u\n",
				reason != nullptr ? reason : "unknown", clientNum,
				peer.RxServerSnapshotSequence, peer.RxSequence, peer.PeerAck,
				unsigned(playerCount), unsigned(commandTics), bodyBytes, snapshotPayloadSize,
				gametic, ClientTic, peer.UnsupportedReceived);
		}
		return false;
	};

	const bool payloadLengthsFit = snapshotPayloadSize >= HCDEServerSnapshotHeaderSize
		&& quitterBytes <= snapshotPayloadSize - HCDEServerSnapshotHeaderSize
		&& bodyBytes == snapshotPayloadSize - HCDEServerSnapshotHeaderSize - quitterBytes;
	const bool quitterLengthMatches = quitterBytes == 0u
		|| (HCDEServerSnapshotHeaderSize < snapshotPayloadSize && size_t(payload[HCDEServerSnapshotHeaderSize]) + 1u == quitterBytes);

	if (snapshotVersion != HCDEServerSnapshotProtocolVersion)
		return rejectServerSnapshot("server-snapshot-version-mismatch");
	if (controlFlags & disallowedControlFlags)
		return rejectServerSnapshot("server-snapshot-disallowed-control-flags");
	if (playerCount > MAXPLAYERS)
		return rejectServerSnapshot("server-snapshot-player-count-overflow");
	if (commandTics > MAXSENDTICS)
		return rejectServerSnapshot("server-snapshot-command-tics-overflow");
	if (consistencyTics > MAXSENDTICS)
		return rejectServerSnapshot("server-snapshot-consistency-tics-overflow");
	if ((controlFlags & NCMD_QUITTERS) == 0u && quitterBytes != 0u)
		return rejectServerSnapshot("server-snapshot-quitter-bytes-without-flag");
	if ((controlFlags & NCMD_QUITTERS) != 0u && quitterBytes == 0u)
		return rejectServerSnapshot("server-snapshot-quitter-flag-without-bytes");
	if (playerCount == 0u && (commandTics != 0u || consistencyTics != 0u))
		return rejectServerSnapshot("server-snapshot-empty-player-records-with-tics");
	if (!payloadLengthsFit)
		return rejectServerSnapshot("server-snapshot-body-length-mismatch");
	if (!quitterLengthMatches)
		return rejectServerSnapshot("server-snapshot-quitter-length-mismatch");

	const uint8_t* body = &payload[HCDEServerSnapshotHeaderSize + quitterBytes];
	if (bodyBytes < HCDEServerSnapshotRecordsHeaderSize
		|| memcmp(&body[HCDEServerSnapshotRecordsMagicOffset], HCDEServerSnapshotRecordsMagic, sizeof(HCDEServerSnapshotRecordsMagic)) != 0)
		return rejectServerSnapshot("missing-records");

	const uint8_t recordsVersion = body[HCDEServerSnapshotRecordsVersionOffset];
	const uint8_t recordPlayerCount = body[HCDEServerSnapshotRecordsPlayerCountOffset];
	if (recordsVersion != HCDEServerSnapshotRecordsProtocolVersion)
		return rejectServerSnapshot("server-snapshot-record-version-mismatch");
	if (recordPlayerCount != playerCount)
		return rejectServerSnapshot("server-snapshot-record-player-count-mismatch");

	if (HCDETryApplyNativeServerSnapshotPayload(clientNum, payload, snapshotPayloadSize, body, bodyBytes, quitterBytes, remoteGameTic, failureReason))
		return true;

	return rejectServerSnapshot(failureReason != nullptr ? failureReason : "server-snapshot-native-apply-failed");
}

static bool HandleHCDELivePacket(int clientNum)
{
	auto& peer = HCDELivePeers[clientNum];
	const uint8_t version = NetBuffer[HCDELiveVersionOffset];
	const uint8_t type = NetBuffer[HCDELiveTypeOffset];
	const uint32_t sequence = HCDELiveReadBE32(&NetBuffer[HCDELiveTxSequenceOffset]);
	const uint32_t ack = HCDELiveReadBE32(&NetBuffer[HCDELiveAckOffset]);
	const size_t payloadSize = NetBufferLength - HCDELiveHeaderSize;

	if (!I_ClientUsesHCDEService(clientNum))
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE live %s from unnegotiated client=%d",
			HCDELiveMessageName(type), clientNum);
		return true;
	}

	if (version != HCDELiveProtocolVersion)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE live %s from client=%d with unsupported version=%u",
			HCDELiveMessageName(type), clientNum, unsigned(version));
		return true;
	}

	if (!HCDELiveSequenceIsFresh(clientNum, type, sequence))
		return true;

	switch (EHCDELiveMessage(type))
	{
	case HLIVE_CONTROL:
	{
		if (payloadSize < HCDELiveControlBasePayloadSize)
		{
			DebugTrace::Markf("net", "malformed HCDE live control from client=%d payload=%zu", clientNum, payloadSize);
			return true;
		}

		AcceptHCDELiveSequence(clientNum, type, sequence);
		peer.PeerAck = ack;
		++peer.ControlReceived;
		++HCDELiveProfile.ControlPacketsReceived;
		HCDELiveProfile.ControlBytesReceived += NetBufferLength;
		HCDERecordLiveLaneRx(HLANE_CONTROL, clientNum, NetBufferLength);
		if (peer.ControlReceived == 1u)
			Printf("%s:: HCDE live channel active with client %d\n", I_IsLocalHCDELiveAuthority() ? "NetServer" : "NetSession", clientNum);

		const uint32_t remoteGameTic = HCDELiveReadBE32(&NetBuffer[HCDELiveHeaderSize]);
		const uint8_t remoteConsole = NetBuffer[HCDELiveHeaderSize + 4u];
		const uint8_t remoteMaxClients = NetBuffer[HCDELiveHeaderSize + 5u];
		HCDEApplyLiveControlCapabilities(clientNum, payloadSize);
		DebugTrace::Markf("net", "HCDE live control recv client=%d seq=%u ack=%u gametic=%u console=%u max=%u caps=0x%llx negotiated=0x%llx",
			clientNum, sequence, ack, remoteGameTic, remoteConsole, remoteMaxClients,
			static_cast<unsigned long long>(peer.RemoteCapabilities),
			static_cast<unsigned long long>(peer.NegotiatedCapabilities));
		break;
	}
	case HLIVE_CLIENT_COMMANDS:
	{
		if (!I_ShouldAcceptHCDELiveClientInputFrom(clientNum))
		{
			++peer.UnsupportedReceived;
			DebugTrace::Markf("net", "ignored HCDE live client commands from client=%d route-authority=%d local-authority=%d",
				clientNum, I_IsHCDELiveAuthoritySlot(clientNum), I_IsLocalHCDELiveAuthority());
			return true;
		}
		if (!HCDELivePeerHasRequiredGameplayCapabilities(clientNum))
		{
			HCDERejectLiveGameplayForCapabilities(clientNum, EHCDELiveMessage(type), "recv", true);
			return true;
		}

		const char* decodeFailureReason = nullptr;
		if (!UnwrapHCDELiveClientInputPayload(clientNum, payloadSize, decodeFailureReason))
		{
			RequestHCDELiveReplay(clientNum, decodeFailureReason != nullptr ? decodeFailureReason : "client-input-decode");
			return true;
		}

		AcceptHCDELiveSequence(clientNum, type, sequence);
		peer.PeerAck = ack;
		++peer.ClientCommandReceived;
		if (peer.ClientCommandReceived == 1u)
			Printf("NetServer:: HCDE live client inputs active with client %d\n", clientNum);
		DebugTrace::Markf("net", "HCDE live client-input boundary recv client=%d payload=%zu local-authority=%d",
			clientNum, payloadSize, I_IsLocalHCDELiveAuthority());
		return true;
	}
	case HLIVE_SERVER_SNAPSHOT:
	{
		if (!I_ShouldAcceptHCDELiveServerSnapshotFrom(clientNum))
		{
			++peer.UnsupportedReceived;
			DebugTrace::Markf("net", "ignored HCDE live server snapshot from client=%d route-authority=%d local-authority=%d",
				clientNum, I_IsHCDELiveAuthoritySlot(clientNum), I_IsLocalHCDELiveAuthority());
			return true;
		}
		if (!HCDELivePeerHasRequiredGameplayCapabilities(clientNum))
		{
			HCDERejectLiveGameplayForCapabilities(clientNum, EHCDELiveMessage(type), "recv", true);
			return true;
		}

		const char* decodeFailureReason = nullptr;
		if (!UnwrapHCDELiveServerSnapshotPayload(clientNum, payloadSize, decodeFailureReason))
		{
			RequestHCDELiveReplay(clientNum, decodeFailureReason != nullptr ? decodeFailureReason : "server-snapshot-decode");
			return true;
		}

		AcceptHCDELiveSequence(clientNum, type, sequence);
		peer.PeerAck = ack;
		++peer.SnapshotReceived;
		if (peer.SnapshotReceived == 1u)
			Printf("NetSession:: HCDE live server snapshots active with client %d\n", clientNum);
		DebugTrace::Markf("net", "HCDE live snapshot boundary recv client=%d payload=%zu from-authority=%d",
			clientNum, payloadSize, I_IsHCDELiveAuthoritySlot(clientNum));
		return true;
	}
	default:
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored unsupported HCDE live type=%u from client=%d payload=%zu",
			unsigned(type), clientNum, payloadSize);
		break;
	}
	return true;
}

// Emit one live control packet per peer at HCDELiveControlIntervalMS cadence
// (1 Hz). The control envelope carries the gametic snapshot, console/maxclient
// info, and capability negotiation bits.
static void SendHCDELiveControl()
{
	const uint64_t now = I_msTime();
	if (LastHCDELiveControlMS != 0u && now - LastHCDELiveControlMS < HCDELiveControlIntervalMS)
		return;

	LastHCDELiveControlMS = now;
	for (auto client : NetworkClients)
	{
		if (!I_ShouldSendHCDELiveControlTo(client))
			continue;

		auto& peer = HCDELivePeers[client];
		const size_t payloadOffset = BeginHCDELivePacket(client, HLIVE_CONTROL);
		HCDELiveWriteBE32(&NetBuffer[payloadOffset], uint32_t(max<int>(gametic, 0)));
		NetBuffer[payloadOffset + 4u] = uint8_t(clamp<int>(consoleplayer, 0, UINT8_MAX));
		NetBuffer[payloadOffset + 5u] = uint8_t(clamp<int>(MaxClients, 0, UINT8_MAX));
		HCDEAppendLiveControlCapabilities(NetBuffer, payloadOffset);
		++peer.ControlSent;
		++HCDELiveProfile.ControlPacketsSent;
		++HCDELiveProfile.CapabilityControlsSent;
		HCDELiveProfile.ControlBytesSent += HCDELiveHeaderSize + HCDELiveControlPayloadSize;
		HCDERecordLiveLaneTx(HLANE_CONTROL, client, HCDELiveHeaderSize + HCDELiveControlPayloadSize);
		DebugTrace::Markf("net", "HCDE live control send client=%d seq=%u ack=%u gametic=%d sent=%u caps=0x%llx",
			client, peer.TxSequence, peer.RxSequence, gametic, peer.ControlSent,
			static_cast<unsigned long long>(HCDELiveLocalCapabilities()));
		HSendPacket(client, HCDELiveHeaderSize + HCDELiveControlPayloadSize);
	}
}

// Native gameplay encoding cannot fall back to legacy NCMD on the wire; when a
// build fails for an HCDE peer we instead record the failure, optionally ask the
// peer for a replay (server snapshots only, since clients need them to advance
// game state), and emit a single warning so the diagnostics show why the peer
// stalled. `legacySize` and `payloadSize` are recorded for the trace only.
static void HCDEAbortLiveGameplaySend(int client, EHCDELiveMessage type, const char* reason, size_t legacySize, size_t payloadSize = 0u)
{
	++HCDELiveProfile.LiveNativeEncodeFailures;
	DebugTrace::Warningf("net",
		"HCDE live %s encode failed client=%d reason=%s legacy=%zu payload=%zu seq=%d ack=%d room=%u native-only",
		HCDELiveMessageName(uint8_t(type)), client, reason != nullptr ? reason : "unknown",
		legacySize, payloadSize, ClientStates[client].CurrentSequence, ClientStates[client].SequenceAck,
		unsigned(CurrentRoomID));
	if (type == HLIVE_SERVER_SNAPSHOT)
	{
		Printf(PRINT_HIGH,
			"HCDE live server snapshot for client %d failed (%s); not sending legacy fallback.\n",
			client, reason != nullptr ? reason : "unknown");
		RequestHCDELiveReplay(client, reason);
	}
	else
	{
		DPrintf(DMSG_WARNING, "HCDE live client input for client %d failed (%s): %zu bytes\n",
			client, reason != nullptr ? reason : "unknown", legacySize);
	}
}

// Final guard for the legacy NetBuffer send path. Non-HCDE peers (or anything not
// classified as live gameplay) still send through the classic NCMD packet shaped
// in NetBuffer. For HCDE gameplay we must have already gone through the native
// HSendNative* wrappers; reaching here means the native build was bypassed, so we
// abort with a native-only diagnostic instead of leaking NCMD onto the wire.
static void HSendLiveGameplayPacket(int client, size_t size)
{
	const bool clientCommand = ShouldWrapHCDEClientCommandPacket(client);
	const bool serverSnapshot = ShouldWrapHCDEServerSnapshotPacket(client);
	if (!clientCommand && !serverSnapshot)
	{
		if (HCDEEnforcesNativeGameplayForPeer(client))
		{
			HCDERejectLegacyGameplayPeer(client, "send", I_ClientUsesHCDEService(client) ? "native-route-unavailable" : "non-hcde-peer");
			return;
		}
		HSendPacket(client, size);
		return;
	}
	if (size > MAX_MSGLEN)
	{
		I_FatalError("HCDE live gameplay packet for client %d exceeded NetBuffer: %zu bytes", client, size);
	}

	const EHCDELiveMessage type = clientCommand ? HLIVE_CLIENT_COMMANDS : HLIVE_SERVER_SNAPSHOT;
	if (!HCDELivePeerHasRequiredGameplayCapabilities(client))
	{
		HCDERejectLiveGameplayForCapabilities(client, type, "send", false);
		return;
	}

	HCDEAbortLiveGameplaySend(client, type, "native-send-not-built", size);
}

// Native send wrapper for HLIVE_CLIENT_COMMANDS. Returns:
//   false  -- this peer is not a candidate for native client-input send (the
//             caller should fall back to HSendLiveGameplayPacket which itself
//             gates HCDE peers).
//   true   -- a native send attempt was made for this peer. Success/failure of
//             the encode is hidden from the caller because in both cases the
//             peer must NOT be subjected to a legacy fallback packet.
static bool HSendNativeClientInputPacket(int client, uint8_t controlFlags, uint8_t routingByte,
	uint32_t sequenceAck, uint32_t consistencyAck, uint32_t baseSequence, uint32_t baseConsistency,
	uint8_t commandTics, uint8_t consistencyTics, uint8_t stabilityBuffer, int playerNum, int sequenceOffset,
	const usercmd_t* commands, const uint8_t* const* eventStreams, const size_t* eventSizes)
{
	constexpr EHCDELiveMessage type = HLIVE_CLIENT_COMMANDS;
	if (!ShouldWrapHCDEClientCommandPacket(client))
		return false;
	if (!HCDELivePeerHasRequiredGameplayCapabilities(client))
	{
		HCDERejectLiveGameplayForCapabilities(client, type, "send", false);
		return true;
	}

	uint8_t gameplayPayload[MAX_MSGLEN];
	size_t gameplayPayloadSize = 0u;
	const char* buildFailureReason = nullptr;
	if (!HCDEBuildNativeClientInputPayload(client, controlFlags, routingByte,
		sequenceAck, consistencyAck, baseSequence, baseConsistency, commandTics, consistencyTics,
		stabilityBuffer, playerNum, sequenceOffset, commands, eventStreams, eventSizes,
		gameplayPayload, sizeof(gameplayPayload), gameplayPayloadSize, buildFailureReason))
	{
		HCDEAbortLiveGameplaySend(client, type, buildFailureReason != nullptr ? buildFailureReason : "client-input-native-build", 0u);
		return true;
	}
	if (gameplayPayloadSize > MAX_MSGLEN - HCDELiveHeaderSize - HCDEGameplayHeaderSize)
	{
		HCDEAbortLiveGameplaySend(client, type, "client-input-native-too-large", 0u, gameplayPayloadSize);
		return true;
	}

	auto& peer = HCDELivePeers[client];
	const size_t payloadOffset = BeginHCDELivePacket(client, type);
	WriteHCDEGameplayEnvelope(&NetBuffer[payloadOffset], HGP_CLIENT_INPUTS);
	memcpy(&NetBuffer[payloadOffset + HCDEGameplayHeaderSize], gameplayPayload, gameplayPayloadSize);
	++peer.ClientCommandSent;
	if (peer.ClientCommandSent == 1u)
		Printf("NetSession:: HCDE live client inputs active with client %d\n", client);
	DebugTrace::Markf("net", "HCDE live client commands native send client=%d seq=%u ack=%u payload=%zu sent=%u",
		client, peer.TxSequence, peer.RxSequence, gameplayPayloadSize, peer.ClientCommandSent);
	++HCDELiveProfile.LivePacketsWrapped;
	HCDELiveProfile.LivePayloadBytesWrapped += gameplayPayloadSize;
	HCDELiveProfile.LiveBytesWrapped += payloadOffset + HCDEGameplayHeaderSize + gameplayPayloadSize;
	ClearHCDELiveReplayPressure(client, "client-input-native-send");
	HSendPacket(client, payloadOffset + HCDEGameplayHeaderSize + gameplayPayloadSize);
	return true;
}

// Native send wrapper for HLIVE_SERVER_SNAPSHOT. Returns false if this peer is
// not a native server-snapshot target (caller should fall back to
// HSendLiveGameplayPacket). Otherwise returns true and hides build success/
// failure: native build failure triggers HCDEAbortLiveGameplaySend (which both
// records the failure and requests a replay from the peer) instead of falling
// back to the legacy NCMD encoder.
static bool HSendNativeServerSnapshotPacket(int client, uint8_t controlFlags, uint8_t routingByte,
	uint32_t sequenceAck, uint32_t consistencyAck, uint32_t baseSequence, uint32_t baseConsistency,
	uint8_t commandTics, uint8_t consistencyTics, uint8_t stabilityBuffer, const int* playerNums, uint8_t playerCount,
	const int* quitNums, uint8_t quitterCount, const usercmd_t (*commands)[MAXSENDTICS],
	const uint8_t* const (*eventStreams)[MAXSENDTICS], const size_t (*eventSizes)[MAXSENDTICS])
{
	constexpr EHCDELiveMessage type = HLIVE_SERVER_SNAPSHOT;
	if (!ShouldWrapHCDEServerSnapshotPacket(client))
		return false;
	if (!HCDELivePeerHasRequiredGameplayCapabilities(client))
	{
		HCDERejectLiveGameplayForCapabilities(client, type, "send", false);
		return true;
	}
