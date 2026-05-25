// This file is split from d_net.cpp


	uint8_t gameplayPayload[MAX_MSGLEN];
	size_t gameplayPayloadSize = 0u;
	const char* buildFailureReason = nullptr;
	if (!HCDEBuildNativeServerSnapshotPayload(client, controlFlags, routingByte,
		sequenceAck, consistencyAck, baseSequence, baseConsistency, commandTics, consistencyTics,
		stabilityBuffer, playerNums, playerCount, quitNums, quitterCount, commands, eventStreams, eventSizes,
		gameplayPayload, sizeof(gameplayPayload), gameplayPayloadSize, buildFailureReason))
	{
		HCDEAbortLiveGameplaySend(client, type, buildFailureReason != nullptr ? buildFailureReason : "server-snapshot-native-build", 0u);
		return true;
	}
	if (gameplayPayloadSize > MAX_MSGLEN - HCDELiveHeaderSize - HCDEGameplayHeaderSize)
	{
		HCDEAbortLiveGameplaySend(client, type, "server-snapshot-native-too-large", 0u, gameplayPayloadSize);
		return true;
	}

	auto& peer = HCDELivePeers[client];
	const size_t payloadOffset = BeginHCDELivePacket(client, type);
	WriteHCDEGameplayEnvelope(&NetBuffer[payloadOffset], HGP_SERVER_SNAPSHOT);
	memcpy(&NetBuffer[payloadOffset + HCDEGameplayHeaderSize], gameplayPayload, gameplayPayloadSize);
	++peer.SnapshotSent;
	if (peer.SnapshotSent == 1u)
		Printf("NetSession:: HCDE live server snapshots active with client %d\n", client);
	DebugTrace::Markf("net", "HCDE live server snapshot native send client=%d seq=%u ack=%u payload=%zu sent=%u",
		client, peer.TxSequence, peer.RxSequence, gameplayPayloadSize, peer.SnapshotSent);
	++HCDELiveProfile.LivePacketsWrapped;
	HCDELiveProfile.LivePayloadBytesWrapped += gameplayPayloadSize;
	HCDELiveProfile.LiveBytesWrapped += payloadOffset + HCDEGameplayHeaderSize + gameplayPayloadSize;
	ClearHCDELiveReplayPressure(client, "server-snapshot-native-send");
	HSendPacket(client, payloadOffset + HCDEGameplayHeaderSize + gameplayPayloadSize);
	return true;
}

CVAR(Bool, vid_dontdowait, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, vid_lowerinbackground, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR(Bool, net_ticbalance, true, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR(Bool, net_extratic, false, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR(Bool, net_limitsaves, true, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR(Bool, net_repeatableactioncooldown, true, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR(Bool, net_limitconversations, false, CVAR_SERVERINFO | CVAR_NOSAVE)
CUSTOM_CVAR(Int, net_disablepause, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
	else if (self > 2)
		self = 2;
}
CUSTOM_CVAR(Int, net_cutscenereadytype, RT_VOTE, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < RT_VOTE)
		self = RT_VOTE;
	else if (self > RT_HOST_ONLY)
		self = RT_HOST_ONLY;
}
CUSTOM_CVAR(Float, net_cutscenereadypercent, 0.5f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.0f)
		self = 0.0f;
	else if (self > 1.0f)
		self = 1.0f;
}
CVAR(Float, net_cutscenecountdown, 30.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
CUSTOM_CVAR(Float, sv_invasioncountdowntime, 30.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	// Compatibility/server-ops cvar:
	// keep this accepted now so launchers/admin scripts can tune invasion/horde
	// countdown timing even while that mode is still being expanded.
	if (self < 0.0f)
		self = 0.0f;
}
CUSTOM_CVAR(Float, sv_invasionspawntime, 8.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.0f)
		self = 0.0f;
}
CUSTOM_CVAR(Float, sv_invasioncleanuptime, 4.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.0f)
		self = 0.0f;
}
CUSTOM_CVAR(Float, sv_invasionintermissiontime, 6.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.0f)
		self = 0.0f;
}
CUSTOM_CVAR(Float, sv_invasionresulttime, 8.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.0f)
		self = 0.0f;
}
CUSTOM_CVAR(Bool, sv_invasionexitonvictory, true, CVAR_SERVERINFO | CVAR_NOSAVE)
{
}
CUSTOM_CVAR(Int, sv_invasionwaves, 8, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 1)
		self = 1;
	else if (self > HCDEInvasionMaxWavesLimit)
		self = HCDEInvasionMaxWavesLimit;
}
CUSTOM_CVAR_NAMED(Int, wavelimit_compat, wavelimit, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
	else if (self > HCDEInvasionMaxWavesLimit)
		self = HCDEInvasionMaxWavesLimit;
}
CUSTOM_CVAR_NAMED(Int, duellimit_compat, duellimit, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
	else if (self > 255)
		self = 255;
}
CUSTOM_CVAR(Bool, sv_usemapsettingswavelimit, true, CVAR_SERVERINFO | CVAR_NOSAVE)
{
}
CUSTOM_CVAR(Int, sv_invasionbasebudget, 24, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 1)
		self = 1;
}
CUSTOM_CVAR(Int, sv_invasionbudgetstep, 8, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
}
CUSTOM_CVAR(Int, sv_invasionperplayer, 6, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
}
CUSTOM_CVAR(Float, sv_invasionspawninterval, 0.35f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.05f)
		self = 0.05f;
}
CUSTOM_CVAR(Int, sv_invasionspawnburst, 3, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 1)
		self = 1;
}
CUSTOM_CVAR(Int, sv_invasionmaxactive, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
}
CUSTOM_CVAR(Int, sv_invasionbosswaveevery, 5, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
}
CUSTOM_CVAR(Int, sv_invasionbossbonus, 20, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
}
CUSTOM_CVAR(Bool, sv_invasionspotusemaptags, false, CVAR_SERVERINFO | CVAR_NOSAVE)
{
}
CUSTOM_CVAR(Bool, sv_invasionspotfallback, true, CVAR_SERVERINFO | CVAR_NOSAVE)
{
}
CUSTOM_CVAR(Bool, sv_invasionsimlod, true, CVAR_SERVERINFO | CVAR_NOSAVE)
{
}
CUSTOM_CVAR(Float, sv_invasionsimlodfullrange, 2048.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	const float clamped = clamp<float>(self, 256.0f, 32768.0f);
	if (self != clamped)
		self = clamped;
}
CUSTOM_CVAR(Float, sv_invasionsimlodreducedrange, 4096.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	const float clamped = clamp<float>(self, float(sv_invasionsimlodfullrange), 65536.0f);
	if (self != clamped)
		self = clamped;
}
CUSTOM_CVAR(Int, sv_invasionsimlodreducedinterval, 5, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	const int clamped = clamp<int>(self, 1, TICRATE * 5);
	if (self != clamped)
		self = clamped;
}
CUSTOM_CVAR(Int, sv_invasionsimloddormantinterval, TICRATE * 3, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	const int clamped = clamp<int>(self, TICRATE / 2, TICRATE * 30);
	if (self != clamped)
		self = clamped;
}

CVAR(Bool, cl_noboldchat, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, cl_nochatsound, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Int, cl_showchat, CHAT_GLOBAL, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < CHAT_DISABLED)
		self = CHAT_DISABLED;
	else if (self > CHAT_GLOBAL)
		self = CHAT_GLOBAL;
}

CUSTOM_CVAR(Int, cl_debugprediction, 0, CVAR_CHEAT)
{
	if (self < 0)
		self = 0;
	else if (self > BACKUPTICS - 1)
		self = BACKUPTICS - 1;
}
CVAR(Bool, net_desyncdebug, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Used to write out all network events that occured leading up to the next tick.
static struct NetEventData
{
	struct FStream {
		uint8_t* Stream;
		size_t Used = 0;

		FStream()
		{
			Grow(256);
		}

		~FStream()
		{
			if (Stream != nullptr)
				M_Free(Stream);
		}

		void Grow(size_t size)
		{
			Stream = (uint8_t*)M_Realloc(Stream, size);
		}
	} Streams[BACKUPTICS];

private:
	size_t CurrentSize = 0;
	size_t MaxSize = 256;
	int CurrentClientTic = 0;

	// Make more room for special Command.
	void GetMoreBytes(size_t newSize)
	{
		MaxSize = max<size_t>(MaxSize * 2, newSize + 30);

		DPrintf(DMSG_NOTIFY, "Expanding special size to %zu\n", MaxSize);

		for (auto& stream : Streams)
			stream.Grow(MaxSize);

		CurrentStream = Streams[CurrentClientTic % BACKUPTICS].Stream + CurrentSize;
	}

	void AddBytes(size_t bytes)
	{
		if (CurrentSize + bytes >= MaxSize)
			GetMoreBytes(CurrentSize + bytes);

		CurrentSize += bytes;
	}

public:
	uint8_t* CurrentStream = nullptr;

	// Boot up does some faux network events so we need to wait until after
	// everything is initialized to actually set up the network stream.
	void InitializeEventData()
	{
		CurrentStream = Streams[0].Stream;
		CurrentSize = 0;
	}

	void ResetStream()
	{
		CurrentClientTic = ClientTic / TicDup;
		CurrentStream = Streams[CurrentClientTic % BACKUPTICS].Stream;
		CurrentSize = 0;
	}

	void NewClientTic()
	{
		const int tic = ClientTic / TicDup;
		if (CurrentClientTic == tic)
			return;

		Streams[CurrentClientTic % BACKUPTICS].Used = CurrentSize;

		CurrentClientTic = tic;
		CurrentStream = Streams[tic % BACKUPTICS].Stream;
		CurrentSize = 0;
	}

	NetEventData& operator<<(uint8_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(1);
			UncheckedWriteInt8(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int16_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(2);
			UncheckedWriteInt16(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int32_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(4);
			UncheckedWriteInt32(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int64_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(8);
			UncheckedWriteInt64(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(float it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(4);
			UncheckedWriteFloat(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(double it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(8);
			UncheckedWriteDouble(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(const char *it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(strlen(it) + 1);
			UncheckedWriteString(it, &CurrentStream);
		}
		return *this;
	}
} NetEvents;

static const char* Net_InvasionSpawnSourceName(EInvasionSpawnSource source)
{
	switch (source)
	{
	case INVSPAWN_CLASSIC: return "classic";
	case INVSPAWN_MAPSPOT: return "mapspot";
	case INVSPAWN_DEATHMATCH: return "deathmatch";
	case INVSPAWN_PLAYERSTART: return "playerstart";
	default: return "none";
	}
}

static PClassActor* Net_GetInvasionSpotBaseClass()
{
	static bool resolved = false;
	static PClassActor* cls = nullptr;
	if (!resolved)
	{
		cls = PClass::FindActor("CustomMonsterInvasionSpot");
		if (cls == nullptr)
			cls = PClass::FindActor("BaseMonsterInvasionSpot");
		resolved = true;
	}
	return cls;
}

void Net_BeginInvasionSpawnRegistration(FLevelLocals* level)
{
	InvasionRegisteredSpawnSpotLevel = level;
	InvasionRegisteredSpawnSpots.Clear();
}

bool Net_RegisterInvasionSpawnSpotFromMapThing(FLevelLocals* level, const FMapThing* mapThing, PClassActor* spotClass)
{
	if (level == nullptr || mapThing == nullptr || spotClass == nullptr)
		return false;

	const PClassActor* invasionSpotBase = Net_GetInvasionSpotBaseClass();
	if (invasionSpotBase == nullptr || !spotClass->IsDescendantOf(invasionSpotBase))
		return false;

	if (InvasionRegisteredSpawnSpotLevel != level)
	{
		Net_BeginInvasionSpawnRegistration(level);
	}

	FInvasionSpawnSpotRecord record;
	record.Pos = mapThing->pos;
	// Map thing Z is a floor-relative offset, matching FLevelLocals::SpawnMapThing.
	if (sector_t* sector = level->PointInSector(record.Pos); sector != nullptr)
	{
		record.Pos.Z = sector->floorplane.ZatPoint(record.Pos) + mapThing->pos.Z;
	}
	record.Yaw = DAngle::fromDeg(mapThing->angle);
	record.Tag = mapThing->thingid;
	record.StartSpawnNumber = max<int>(mapThing->args[0], 1);
	record.SpawnDelayTics = max(Net_InvasionSecondsToTics(float(mapThing->args[1])), 0);
	record.RoundSpawnDelayTics = max(Net_InvasionSecondsToTics(float(mapThing->args[2])), 0);
	record.FirstWave = max<int>(mapThing->args[3], 1);
	record.MaxSpawnNumber = max<int>(mapThing->args[4], 0);
	record.Source = INVSPAWN_CLASSIC;
	record.SpotClass = spotClass;
	record.PlannedSpawnCount = 0;
	record.SpawnedCount = 0;
	record.NextSpawnGameTic = 0;
	InvasionRegisteredSpawnSpots.Push(record);
	return true;
}

static PClassActor* Net_GetMapSpotClass()
{
	static bool resolved = false;
	static PClassActor* cls = nullptr;
	if (!resolved)
	{
		cls = PClass::FindActor("MapSpot");
		resolved = true;
	}
	return cls;
}

static bool Net_IsNoMonstersActive()
{
	if ((dmflags & DF_NO_MONSTERS) != 0)
		return true;
	return primaryLevel != nullptr && (primaryLevel->flags2 & LEVEL2_NOMONSTERS) != 0;
}

static bool Net_IsValidInvasionMonsterClass(const PClassActor* cls)
{
	if (cls == nullptr)
		return false;

	const AActor* defaults = GetDefaultByType(cls);
	if (defaults == nullptr)
		return false;

	return (defaults->flags3 & MF3_ISMONSTER) != 0;
}

static PClassActor* Net_ResolveInvasionMonsterClass(const char* className)
{
	if (className == nullptr || *className == '\0')
		return nullptr;

	PClassActor* cls = PClass::FindActor(className);
	if (cls == nullptr)
		return nullptr;

	cls = cls->GetReplacement(primaryLevel);
	return Net_IsValidInvasionMonsterClass(cls) ? cls : nullptr;
}

static PClassActor* Net_SelectInvasionMonsterFromPool(const char* const* pool, size_t count, int wave)
{
	if (pool == nullptr || count == 0)
		return nullptr;

	TArray<PClassActor*> resolved;
	for (size_t i = 0; i < count; ++i)
	{
		PClassActor* cls = Net_ResolveInvasionMonsterClass(pool[i]);
		if (cls == nullptr)
			continue;

		bool waveMet = true;
		for (const auto& profile : InvasionMonsterProfiles)
		{
			if (stricmp(profile.ClassName, pool[i]) == 0)
			{
				if (wave < profile.MinWave)
					waveMet = false;
				break;
			}
		}

		if (waveMet)
			resolved.Push(cls);
	}

	if (resolved.Size() == 0)
		return nullptr;

	return resolved[pr_invasion() % resolved.Size()];
}

static PClassActor* Net_SelectInvasionMonsterFromDropList(const PClassActor* spotClass)
{
	if (spotClass == nullptr)
		return nullptr;

	const AActor* defaults = GetDefaultByType(spotClass);
	if (defaults == nullptr)
		return nullptr;

	const FDropItem* drop = defaults->GetDropItems();
	if (drop == nullptr)
		return nullptr;

	int weightTotal = 0;
	for (auto di = drop; di != nullptr; di = di->Next)
	{
		if (di->Name == NAME_None)
			continue;

		PClassActor* candidate = PClass::FindActor(di->Name);
		if (!Net_IsValidInvasionMonsterClass(candidate))
			continue;

		const int amount = max(di->Amount, 1);
		weightTotal += amount;
	}

	if (weightTotal <= 0)
		return nullptr;

	int pick = pr_invasion() % weightTotal;
	for (auto di = drop; di != nullptr; di = di->Next)
	{
		if (di->Name == NAME_None)
			continue;

		PClassActor* candidate = PClass::FindActor(di->Name);
		if (!Net_IsValidInvasionMonsterClass(candidate))
			continue;

		const int amount = max(di->Amount, 1);
		pick -= amount;
		if (pick >= 0)
			continue;

		const int probability = clamp<int>(di->Probability, 0, 255);
		if (pr_invasion() > probability)
			return nullptr;

		candidate = candidate->GetReplacement(primaryLevel);
		return Net_IsValidInvasionMonsterClass(candidate) ? candidate : nullptr;
	}

	return nullptr;
}

static PClassActor* Net_SelectInvasionMonsterForSpot(const FInvasionSpawnSpotRecord& spot)
{
	static const char* const weakPool[] = {
		"ZombieMan", "ShotgunGuy", "ChaingunGuy", "DoomImp", "Demon", "Spectre", "LostSoul"
	};
	static const char* const strongPool[] = {
		"Cacodemon", "HellKnight", "BaronOfHell", "Revenant", "Fatso", "Arachnotron", "PainElemental"
	};
	static const char* const bossPool[] = {
		"Archvile", "Cyberdemon", "SpiderMastermind", "BaronOfHell", "Revenant"
	};
	static const char* const anyPool[] = {
		"ZombieMan", "ShotgunGuy", "ChaingunGuy", "DoomImp", "Demon", "Spectre", "LostSoul",
		"Cacodemon", "HellKnight", "BaronOfHell", "Revenant", "Fatso", "Arachnotron",
		"PainElemental", "Archvile", "WolfensteinSS", "Cyberdemon", "SpiderMastermind"
	};

	if (Net_IsNoMonstersActive())
		return nullptr;

	if (spot.SpotClass != nullptr)
	{
		// First preference: let modded/map-authored invasion spots provide a drop list.
		if (PClassActor* fromDropList = Net_SelectInvasionMonsterFromDropList(spot.SpotClass); fromDropList != nullptr)
			return fromDropList;

		auto checkSpot = [&](const char* className) {
			auto cls = PClass::FindActor(className);
			return cls != nullptr && spot.SpotClass->IsDescendantOf(cls);
		};

		if (checkSpot("ImpSpot")) return Net_ResolveInvasionMonsterClass("DoomImp");
		if (checkSpot("DemonSpot")) return Net_ResolveInvasionMonsterClass("Demon");
		if (checkSpot("SpectreSpot")) return Net_ResolveInvasionMonsterClass("Spectre");
		if (checkSpot("ZombieManSpot")) return Net_ResolveInvasionMonsterClass("ZombieMan");
		if (checkSpot("ShotgunGuySpot")) return Net_ResolveInvasionMonsterClass("ShotgunGuy");
		if (checkSpot("ChaingunGuySpot")) return Net_ResolveInvasionMonsterClass("ChaingunGuy");
		if (checkSpot("CacodemonSpot")) return Net_ResolveInvasionMonsterClass("Cacodemon");
		if (checkSpot("RevenantSpot")) return Net_ResolveInvasionMonsterClass("Revenant");
		if (checkSpot("FatsoSpot")) return Net_ResolveInvasionMonsterClass("Fatso");
		if (checkSpot("ArachnotronSpot")) return Net_ResolveInvasionMonsterClass("Arachnotron");
		if (checkSpot("HellKnightSpot")) return Net_ResolveInvasionMonsterClass("HellKnight");
		if (checkSpot("BaronOfHellSpot")) return Net_ResolveInvasionMonsterClass("BaronOfHell");
		if (checkSpot("LostSoulSpot")) return Net_ResolveInvasionMonsterClass("LostSoul");
		if (checkSpot("PainElementalSpot")) return Net_ResolveInvasionMonsterClass("PainElemental");
		if (checkSpot("CyberdemonSpot")) return Net_ResolveInvasionMonsterClass("Cyberdemon");
		if (checkSpot("SpiderMastermindSpot")) return Net_ResolveInvasionMonsterClass("SpiderMastermind");
		if (checkSpot("ArchvileSpot")) return Net_ResolveInvasionMonsterClass("Archvile");
		if (checkSpot("WolfensteinSSSpot")) return Net_ResolveInvasionMonsterClass("WolfensteinSS");

		if (checkSpot("WeakDoomMonsterSpot"))
			return Net_SelectInvasionMonsterFromPool(weakPool, countof(weakPool), InvasionWaveDirector.Wave);
		if (checkSpot("PowerfulDoomMonsterSpot"))
			return Net_SelectInvasionMonsterFromPool(strongPool, countof(strongPool), InvasionWaveDirector.Wave);
		if (checkSpot("VeryPowerfulDoomMonsterSpot"))
			return Net_SelectInvasionMonsterFromPool(bossPool, countof(bossPool), InvasionWaveDirector.Wave);
		if (checkSpot("AnyDoomMonsterSpot"))
			return Net_SelectInvasionMonsterFromPool(anyPool, countof(anyPool), InvasionWaveDirector.Wave);
	}

	if (InvasionWaveDirector.Wave >= 8)
		return Net_SelectInvasionMonsterFromPool(bossPool, countof(bossPool), InvasionWaveDirector.Wave);
	if (InvasionWaveDirector.Wave >= 4)
		return Net_SelectInvasionMonsterFromPool(strongPool, countof(strongPool), InvasionWaveDirector.Wave);
	return Net_SelectInvasionMonsterFromPool(weakPool, countof(weakPool), InvasionWaveDirector.Wave);
}

static DVector3 Net_GetInvasionSpawnCandidate(const DVector3& basePos, double xOffset, double yOffset)
{
	DVector3 candidate = basePos + DVector3(xOffset, yOffset, 0.0);
	if (primaryLevel == nullptr)
		return candidate;

	double zOffset = 0.0;
	if (sector_t* baseSector = primaryLevel->PointInSector(basePos); baseSector != nullptr)
	{
		zOffset = basePos.Z - baseSector->floorplane.ZatPoint(basePos);
	}
	if (sector_t* candidateSector = primaryLevel->PointInSector(candidate); candidateSector != nullptr)
	{
		candidate.Z = candidateSector->floorplane.ZatPoint(candidate) + zOffset;
	}
	return candidate;
}

static AActor* Net_SelectInvasionCombatTarget(AActor* actor)
{
	AActor* best = nullptr;
	double bestDistSq = 0.0;
	for (int player = 0; player < MAXPLAYERS; ++player)
	{
		if (!playeringame[player] || I_IsServerReservedSlot(player))
			continue;

		player_t& playerState = players[player];
		AActor* playerMo = playerState.mo;
		if (playerMo == nullptr || playerState.playerstate != PST_LIVE)
			continue;

		if (playerMo->health <= 0 && playerState.health <= 0)
			continue;
		if ((playerMo->flags & MF_SHOOTABLE) == 0)
			continue;
		if ((playerState.cheats & CF_NOTARGET) != 0)
			continue;
		if (actor != nullptr && actor->IsFriend(playerMo))
			continue;

		const double distSq = actor != nullptr ? (playerMo->Pos() - actor->Pos()).LengthSquared() : 0.0;
		if (best == nullptr || distSq < bestDistSq)
		{
			best = playerMo;
			bestDistSq = distSq;
		}
	}

	return best;
}

static void Net_AwakenInvasionMonster(AActor* actor)
{
	if (actor == nullptr || actor->health <= 0 || (actor->flags3 & MF3_ISMONSTER) == 0)
		return;

	AActor* target = Net_SelectInvasionCombatTarget(actor);
	if (target == nullptr)
		return;

	actor->target = target;
	actor->LastHeard = target;
	actor->lastenemy = nullptr;
	actor->reactiontime = max<int>(actor->reactiontime, Net_GetInvasionSkillWakeDelayTics());
	actor->threshold = max<int>(actor->DefThreshold, 10);
	actor->flags4 |= MF4_INCOMBAT;
	actor->Angles.Yaw = actor->AngleTo(target);
	if (actor->SeeState != nullptr)
		actor->SetState(actor->SeeState, true);

	DebugTrace::Markf("invasion", "monster awakened class=%s target=%d dist=%.1f threshold=%d reaction=%d skill=%d",
		actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
		target->player != nullptr ? int(target->player - players) : -1,
		(target->Pos() - actor->Pos()).Length(),
		actor->threshold,
		actor->reactiontime,
		clamp<int>(gameskill, 0, 4));
}

static void Net_SpawnInvasionTeleportFog(AActor* actor)
{
	if (actor == nullptr || primaryLevel == nullptr || gamestate != GS_LEVEL)
		return;

	P_SpawnTeleportFog(actor, actor->Pos(), false, true);
	if (Net_InvasionDebugEnabled(2))
	{
		DebugTrace::Markf("invasion", "spawn fog class=%s pos=(%.1f,%.1f,%.1f) mirror=%d",
			actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
			actor->X(), actor->Y(), actor->Z(),
			Net_IsInvasionClientMirrorActor(actor) ? 1 : 0);
	}
}

// Spawns a monster at the specified spot. Handles telefrag protection and
// collision validation, and returns a compact reason for diagnostics when it
// cannot spawn yet.
static bool Net_SpawnInvasionMonster(FInvasionSpawnSpotRecord& spot, const char** failureReason = nullptr, PClassActor* forcedMonsterClass = nullptr, PClassActor** resolvedMonsterClass = nullptr)
{
	auto fail = [failureReason](const char* reason) {
		if (failureReason != nullptr)
			*failureReason = reason;
		return false;
	};
	if (failureReason != nullptr)
		*failureReason = nullptr;

	if (primaryLevel == nullptr || gamestate != GS_LEVEL)
		return fail("no-level");

	PClassActor* monsterClass = forcedMonsterClass != nullptr ? forcedMonsterClass : Net_SelectInvasionMonsterForSpot(spot);
	if (resolvedMonsterClass != nullptr)
		*resolvedMonsterClass = monsterClass;
	if (monsterClass == nullptr)
		return fail("no-monster-class");

	const AActor* monsterDefaults = GetDefaultByType(monsterClass);
	auto trySpawnAt = [&](const DVector3& pos, const char** reason) -> AActor*
	{
		// Telefrag protection: prevent spawning if a live player is currently inside the spot's footprint.
		// This uses a conservative radius-sum check to avoid 'telefrag-jiggle' on high-latency clients.
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (!playeringame[i] || players[i].mo == nullptr || players[i].mo->health <= 0)
				continue;

			const AActor* playerMo = players[i].mo;
			const double dist = (playerMo->Pos() - pos).Length();
			if (dist < playerMo->radius + monsterDefaults->radius + 8.0)
			{
				if (reason != nullptr)
					*reason = "player-blocking-spot";
				return nullptr;
			}
		}

		AActor* spawned = Spawn(primaryLevel, monsterClass, pos, ALLOW_REPLACE);
		if (spawned == nullptr)
		{
			if (reason != nullptr)
				*reason = "spawn-returned-null";
			return nullptr;
		}

		// Perform a final location validation. Briefly allow actor overlap to mimic
		// standard Doom spawner behavior.
		const ActorFlags2 oldFlags2 = spawned->flags2;
		spawned->flags2 |= MF2_PASSMOBJ;
		if (!P_TestMobjLocation(spawned))
		{
			spawned->flags2 = oldFlags2;
			spawned->ClearCounters();
			spawned->Destroy();
			if (reason != nullptr)
				*reason = "blocked-location";
			return nullptr;
		}

		spawned->flags2 = oldFlags2;
		spawned->Angles.Yaw = spot.Yaw;
		return spawned;
	};

	const char* lastReason = nullptr;
	AActor* spawned = trySpawnAt(spot.Pos, &lastReason);
	if (spawned == nullptr && lastReason != nullptr && stricmp(lastReason, "blocked-location") == 0)
	{
		static const double radii[] = { 16.0, 32.0, 48.0, 64.0, 96.0 };
		for (double radius : radii)
		{
			for (int step = 0; step < 8 && spawned == nullptr; ++step)
			{
				const DAngle angle = DAngle::fromDeg(step * 45.0);
				const DVector3 candidate = Net_GetInvasionSpawnCandidate(spot.Pos, angle.Cos() * radius, angle.Sin() * radius);
				spawned = trySpawnAt(candidate, &lastReason);
			}
			if (spawned != nullptr)
				break;
		}
	}

	if (spawned == nullptr)
		return fail(lastReason != nullptr ? lastReason : "blocked-location");

	// Track the monster for wave-cleared calculations.
	Net_ApplyInvasionMonsterSkillTuning(spawned);
	Net_SpawnInvasionTeleportFog(spawned);
	Net_AwakenInvasionMonster(spawned);
	InvasionWaveDirector.ActiveMonsters.Push(MakeObjPtr<AActor*>(spawned));
	Net_RecordInvasionSpawnEvent(spawned);
	return true;
}

static bool Net_IsInvasionSpotActiveForCurrentTag(const FInvasionSpawnSpotRecord& spot)
{
	if (InvasionSpawnDirectory.ActiveTag > 0 && spot.Tag > 0 && spot.Tag != InvasionSpawnDirectory.ActiveTag)
		return false;
	return true;
}

static bool Net_TryRelocateBlockedClassicInvasionSpawn(FInvasionSpawnSpotRecord& blockedSpot, PClassActor* monsterClass, const char** failureReason)
{
	if (blockedSpot.Source != INVSPAWN_CLASSIC || monsterClass == nullptr)
		return false;

	const char* lastReason = nullptr;
	for (int pass = 0; pass < 2; ++pass)
	{
		for (auto& candidate : InvasionSpawnDirectory.Spots)
		{
			if (&candidate == &blockedSpot
				|| candidate.Source != INVSPAWN_CLASSIC
				|| !Net_IsInvasionSpotActiveForCurrentTag(candidate))
			{
				continue;
			}

			const bool sameSpotClass = candidate.SpotClass == blockedSpot.SpotClass;
			if ((pass == 0 && !sameSpotClass) || (pass == 1 && sameSpotClass))
				continue;

			if (Net_SpawnInvasionMonster(candidate, &lastReason, monsterClass))
			{
				Printf(PRINT_HIGH,
					"Invasion relocated blocked classic spawn: class=%s from=%s tid=%d to=%s tid=%d\n",
					monsterClass->TypeName.GetChars(),
					blockedSpot.SpotClass != nullptr ? blockedSpot.SpotClass->TypeName.GetChars() : "<fallback>",
					blockedSpot.Tag,
					candidate.SpotClass != nullptr ? candidate.SpotClass->TypeName.GetChars() : "<fallback>",
					candidate.Tag);
				return true;
			}
		}
	}

	if (failureReason != nullptr && lastReason != nullptr)
		*failureReason = lastReason;
	return false;
}

// Scans the active monster list and removes dead or invalid actors.
// Updates the global WaveCleared progress counter.
static void Net_UpdateInvasionWaveClearProgress()
{
	int aliveMonsters = 0;
	size_t writeIdx = 0;
	for (size_t i = 0; i < InvasionWaveDirector.ActiveMonsters.Size(); ++i)
	{
		AActor* actor = InvasionWaveDirector.ActiveMonsters[i];
		if (actor == nullptr || actor->health <= 0 || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		{
			continue;
		}

		// Perform O(n) list compaction.
		if (writeIdx != i)
		{
			InvasionWaveDirector.ActiveMonsters[writeIdx] = InvasionWaveDirector.ActiveMonsters[i];
		}
		++writeIdx;
		++aliveMonsters;
	}

	if (writeIdx < InvasionWaveDirector.ActiveMonsters.Size())
	{
		InvasionWaveDirector.ActiveMonsters.Resize(unsigned(writeIdx));
	}

	InvasionWaveDirector.WaveCleared = max(InvasionWaveDirector.WaveSpawned - aliveMonsters, 0);
}

static bool Net_IsInvasionSimulationLODAllowedActor(const FInvasionReplicatedActorRef& ref, const AActor* actor)
{
	if (actor == nullptr
		|| ref.IsProjectile
		|| Net_IsInvasionReplicatedProjectile(actor)
		|| Net_IsInvasionActorCorpseLike(actor)
		|| (actor->flags3 & MF3_ISMONSTER) == 0
		|| (actor->ObjectFlags & OF_EuthanizeMe) != 0)
	{
		return false;
	}

	if ((InvasionWaveDirector.WaveFlags & INV_WAVEF_BOSS) != 0u)
		return false;
	if (gametic - ref.SpawnTic < TICRATE * 2)
		return false;
	if (Net_IsInvasionActorActionPriority(Net_GetInvasionActorActionState(actor)))
		return false;
	if (actor->target != nullptr || actor->tracer != nullptr || actor->lastenemy != nullptr)
		return false;

	return true;
}

static double Net_InvasionNearestParticipantDistanceSquared(const AActor* actor, bool& hasParticipant)
{
	hasParticipant = false;
	double best = 0.0;
	if (actor == nullptr)
		return best;

	for (int player = 0; player < MAXPLAYERS; ++player)
	{
		if (!playeringame[player] || players[player].mo == nullptr || players[player].mo->health <= 0)
			continue;

		const double distSq = (actor->Pos() - players[player].mo->Pos()).LengthSquared();
		if (!hasParticipant || distSq < best)
			best = distSq;
		hasParticipant = true;
	}
	return best;
}

static void Net_RestoreInvasionSimulationLODActor(FInvasionReplicatedActorRef& ref, AActor* actor, const char* reason)
{
	if (actor == nullptr || !ref.SimulationSuspended)
		return;

	const int restoreStat = ref.SimulationOriginalStatNum >= STAT_FIRST_THINKING
		? ref.SimulationOriginalStatNum
		: STAT_DEFAULT;
	if (actor->GetStatNum() < STAT_FIRST_THINKING)
		actor->ChangeStatNum(restoreStat);
	ref.SimulationSuspended = false;
	++HCDELiveProfile.SimLODRestored;
	DebugTrace::Markf("invasion", "sim-lod restore id=%u class=%s stat=%d reason=%s",
		unsigned(ref.Id),
		actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
		actor->GetStatNum(),
		reason != nullptr ? reason : "unknown");
}

static void Net_SuspendInvasionSimulationLODActor(FInvasionReplicatedActorRef& ref, AActor* actor, uint8_t lod)
{
	if (actor == nullptr || actor->GetStatNum() < STAT_FIRST_THINKING)
		return;

	if (!ref.SimulationSuspended)
	{
		ref.SimulationOriginalStatNum = actor->GetStatNum();
		++HCDELiveProfile.SimLODSuspended;
		DebugTrace::Markf("invasion", "sim-lod suspend id=%u class=%s lod=%s stat=%d next=%d",
			unsigned(ref.Id),
			actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
			HCDESimulationLODName(lod),
			actor->GetStatNum(),
			ref.SimulationNextThinkTic);
	}
	actor->ChangeStatNum(STAT_INFO);
	ref.SimulationSuspended = true;
}

static void Net_RestoreAllInvasionSimulationLODActors(const char* reason)
{
	for (auto& ref : InvasionReplicatedActors)
	{
		Net_RestoreInvasionSimulationLODActor(ref, ref.Actor.Get(), reason);
		ref.SimulationLOD = HSIMLOD_FULL;
		ref.SimulationNextThinkTic = 0;
	}
}

static void Net_PrepareInvasionSimulationLOD()
{
	memset(InvasionSimulationLODCurrent, 0, sizeof(InvasionSimulationLODCurrent));
	InvasionSimulationLODSuspendedCurrent = 0u;
	InvasionSimulationLODAllowedCurrent = 0u;
	InvasionSimulationLODSkippedCurrent = 0u;
	InvasionSimulationLODWakeHealthCurrent = 0u;
	InvasionSimulationLODWakeDistanceCurrent = 0u;

	if (!sv_invasionsimlod
		|| !Net_IsInvasionModeEnabled()
		|| !Net_IsLocalInvasionAuthority()
		|| gamestate != GS_LEVEL
		|| !Net_IsInvasionRoundActiveState(InvasionState))
	{
		Net_RestoreAllInvasionSimulationLODActors("disabled");
		return;
	}

	const double fullRangeSq = double(sv_invasionsimlodfullrange) * double(sv_invasionsimlodfullrange);
	const double reducedRangeSq = double(sv_invasionsimlodreducedrange) * double(sv_invasionsimlodreducedrange);
	++HCDELiveProfile.SimLODPasses;
	for (auto& ref : InvasionReplicatedActors)
	{
		AActor* actor = ref.Actor.Get();
		if (actor == nullptr || actor->health <= 0 || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		{
			Net_RestoreInvasionSimulationLODActor(ref, actor, "invalid");
			continue;
		}

		bool hasParticipant = false;
		const double distSq = Net_InvasionNearestParticipantDistanceSquared(actor, hasParticipant);
		const bool healthWake = ref.SimulationLastHealth != 0 && actor->health != ref.SimulationLastHealth;
		const bool distanceWake = hasParticipant && distSq <= fullRangeSq;
		uint8_t lod = HSIMLOD_FULL;
		int interval = 1;
		if (!Net_IsInvasionSimulationLODAllowedActor(ref, actor) || !hasParticipant || healthWake || distanceWake)
		{
			lod = HSIMLOD_FULL;
		}
		else if (distSq <= reducedRangeSq)
		{
			lod = HSIMLOD_REDUCED;
			interval = int(sv_invasionsimlodreducedinterval);
		}
		else
		{
			lod = HSIMLOD_DORMANT;
			interval = int(sv_invasionsimloddormantinterval);
		}

		ref.SimulationLOD = lod;
		ref.SimulationLastDecisionTic = gametic;
		if (lod < HSIMLOD_COUNT)
			++InvasionSimulationLODCurrent[lod];
		if (healthWake)
		{
			++InvasionSimulationLODWakeHealthCurrent;
			++HCDELiveProfile.SimLODWakeHealth;
		}
		if (distanceWake && ref.SimulationSuspended)
		{
			++InvasionSimulationLODWakeDistanceCurrent;
			++HCDELiveProfile.SimLODWakeDistance;
		}

		if (lod == HSIMLOD_FULL)
		{
			ref.SimulationNextThinkTic = gametic + 1;
			ref.SimulationAllowedTics++;
			++InvasionSimulationLODAllowedCurrent;
			++HCDELiveProfile.SimLODThinkAllowed;
			Net_RestoreInvasionSimulationLODActor(ref, actor, healthWake ? "health" : distanceWake ? "distance" : "full");
		}
		else if (gametic >= ref.SimulationNextThinkTic)
		{
			ref.SimulationNextThinkTic = gametic + max<int>(interval, 1);
			ref.SimulationAllowedTics++;
			++InvasionSimulationLODAllowedCurrent;
			++HCDELiveProfile.SimLODThinkAllowed;
			Net_RestoreInvasionSimulationLODActor(ref, actor, HCDESimulationLODName(lod));
		}
		else
		{
			ref.SimulationSkippedTics++;
			++InvasionSimulationLODSkippedCurrent;
			++HCDELiveProfile.SimLODThinkSkipped;
			Net_SuspendInvasionSimulationLODActor(ref, actor, lod);
		}
		if (ref.SimulationSuspended)
			++InvasionSimulationLODSuspendedCurrent;
		ref.SimulationLastHealth = actor->health;
	}

	HCDELiveProfile.SimLODFull += InvasionSimulationLODCurrent[HSIMLOD_FULL];
	HCDELiveProfile.SimLODReduced += InvasionSimulationLODCurrent[HSIMLOD_REDUCED];
	HCDELiveProfile.SimLODDormant += InvasionSimulationLODCurrent[HSIMLOD_DORMANT];
}

static int Net_DestroyTrackedInvasionMonsters(const char* reason)
{
	int removed = 0;
	Net_RestoreAllInvasionSimulationLODActors(reason != nullptr ? reason : "destroy");
	if (gamestate == GS_LEVEL)
	{
		for (size_t i = 0; i < InvasionWaveDirector.ActiveMonsters.Size(); ++i)
		{
			AActor* actor = InvasionWaveDirector.ActiveMonsters[i];
			if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
				continue;

			actor->ClearCounters();
			actor->Destroy();
			++removed;
		}
	}

	InvasionWaveDirector.ActiveMonsters.Clear();
	InvasionReplicatedActors.Clear();
	Net_ClearInvasionReplicatedActorIndexes();
	Net_ClearHCDEReplicatedActors();
	InvasionPendingMirrorSpawns.Clear();
	for (int client = 0; client < MAXPLAYERS; ++client)
	{
		HCDEInvasionActorDeltaV2SendCursor[client] = 0u;
		HCDEActorDeltaV2SendCursor[client] = 0u;
		HCDEActorPriorityQueues[client].Clear();
	}
	if (removed > 0)
	{
		DebugTrace::Markf("invasion", "cleanup removed=%d reason=%s wave=%d map=%s",
			removed,
			reason != nullptr ? reason : "(none)",
			InvasionWaveDirector.Wave,
			primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
	}
	return removed;
}

static double Net_GetInvasionMonsterDifficultyModifier()
{
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 1)
		return 1.25;
	if (skillIndex == 2)
		return 1.5;
	return 1.6225;
}

static double Net_GetInvasionSkillBudgetScale()
{
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 0)
		return 0.50;
	if (skillIndex == 1)
		return 0.70;
	if (skillIndex == 2)
		return 0.85;
	return 1.0;
}

static int Net_ApplyInvasionSkillBudgetScale(int budget)
{
	if (budget <= 1)
		return max<int>(budget, 1);

	const double scaled = double(budget) * Net_GetInvasionSkillBudgetScale();
	return clamp<int>(int(floor(scaled + 0.5)), 1, 65535);
}

static double Net_GetInvasionSkillSpawnIntervalScale()
{
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 0)
		return 1.75;
	if (skillIndex == 1)
		return 1.35;
	if (skillIndex == 2)
		return 1.15;
	return 1.0;
}

static int Net_GetInvasionSkillWakeDelayTics()
{
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 0)
		return TICRATE;
	if (skillIndex == 1)
		return TICRATE / 2;
	return 0;
}

static double Net_GetInvasionSkillMonsterSpeedScale()
{
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 0)
		return 0.65;
	if (skillIndex == 1)
		return 0.75;
	if (skillIndex == 2)
		return 0.85;
	return 1.0;
}

static void Net_ApplyInvasionMonsterSkillTuning(AActor* actor)
{
	if (actor == nullptr || (actor->flags3 & MF3_ISMONSTER) == 0)
		return;

	const double speedScale = Net_GetInvasionSkillMonsterSpeedScale();
	if (speedScale < 0.999)
	{
		actor->Speed *= speedScale;
	}
}

static int Net_GetInvasionActiveMonsterCap()
{
	if (sv_invasionmaxactive > 0)
		return clamp<int>(sv_invasionmaxactive, 1, 1024);

	const int players = max(Net_CountInvasionParticipants(), 1);
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 0)
		return 8 + max(players - 1, 0) * 4;
	if (skillIndex == 1)
		return 11 + max(players - 1, 0) * 5;
	if (skillIndex == 2)
		return 16 + max(players - 1, 0) * 6;
	return 24 + max(players - 1, 0) * 8;
}

static int Net_ComputeInvasionSpotWaveCount(const FInvasionSpawnSpotRecord& spot, int wave)
{
	const int firstWave = max(spot.FirstWave, 1);
	if (wave < firstWave)
		return 0;

	const int startAmount = max(spot.StartSpawnNumber, 1);
	const int waveAge = max(wave - firstWave, 0);
	const double modifier = Net_GetInvasionMonsterDifficultyModifier();
	const double scaled = pow(modifier, double(waveAge)) * double(startAmount);
	int count = max<int>(int(floor(scaled + 0.5)), 0);
	if (spot.MaxSpawnNumber > 0)
		count = min(count, spot.MaxSpawnNumber);
	return count;
}

static int Net_SelectInvasionSpotTag(const TArray<FInvasionSpawnSpotRecord>& spots, int wave)
{
	if (!sv_invasionspotusemaptags)
		return 0;

	TArray<int> tags;
	for (const auto& spot : spots)
	{
		if (spot.Tag <= 0 || spot.Source != INVSPAWN_CLASSIC)
			continue;

		bool exists = false;
		for (const int tag : tags)
		{
			if (tag == spot.Tag)
			{
				exists = true;
				break;
			}
		}
		if (!exists)
			tags.Push(spot.Tag);
	}

	if (tags.Size() == 0)
		return 0;

	for (const int tag : tags)
	{
		if (tag == wave)
			return tag;
	}

	for (unsigned i = 0; i + 1 < tags.Size(); ++i)
	{
		for (unsigned j = i + 1; j < tags.Size(); ++j)
		{
			if (tags[j] < tags[i])
			{
				const int swapValue = tags[i];
				tags[i] = tags[j];
				tags[j] = swapValue;
			}
		}
	}
	const int index = clamp<int>(wave - 1, 0, int(tags.Size()) - 1);
	return tags[index];
}

static void Net_BuildFallbackInvasionSpots(FLevelLocals* level)
{
	InvasionSpawnDirectory.UsingFallback = true;
	InvasionSpawnDirectory.FallbackSource = INVSPAWN_NONE;

	if (sv_invasionspotfallback && level != nullptr)
	{
		if (auto mapSpotClass = Net_GetMapSpotClass(); mapSpotClass != nullptr)
		{
			auto it = level->GetThinkerIterator<AActor>();
			AActor* actor = nullptr;
			while ((actor = it.Next()) != nullptr)
			{
				if (!actor->IsA(mapSpotClass))
					continue;

				FInvasionSpawnSpotRecord record;
				record.Pos = actor->Pos();
				record.Yaw = actor->Angles.Yaw;
				record.Tag = actor->tid;
				record.Source = INVSPAWN_MAPSPOT;
				record.PlannedSpawnCount = 0;
				record.NextSpawnGameTic = gametic;
				InvasionSpawnDirectory.Spots.Push(record);
			}
		}
	}

	if (InvasionSpawnDirectory.Spots.Size() > 0)
	{
		InvasionSpawnDirectory.FallbackSource = INVSPAWN_MAPSPOT;
		return;
	}

	if (level != nullptr)
	{
		for (const auto& start : level->deathmatchstarts)
		{
			FInvasionSpawnSpotRecord record;
			record.Pos = start.pos;
			record.Yaw = DAngle::fromDeg(start.angle);
			record.Source = INVSPAWN_DEATHMATCH;
			record.PlannedSpawnCount = 0;
			record.NextSpawnGameTic = gametic;
			InvasionSpawnDirectory.Spots.Push(record);
		}
	}

	if (InvasionSpawnDirectory.Spots.Size() > 0)
	{
		InvasionSpawnDirectory.FallbackSource = INVSPAWN_DEATHMATCH;
		return;
	}

	if (level != nullptr)
	{
		for (const auto& start : level->AllPlayerStarts)
		{
			FInvasionSpawnSpotRecord record;
			record.Pos = start.pos;
			record.Yaw = DAngle::fromDeg(start.angle);
			record.Source = INVSPAWN_PLAYERSTART;
			record.PlannedSpawnCount = 0;
			record.NextSpawnGameTic = gametic;
			InvasionSpawnDirectory.Spots.Push(record);
		}
	}

	if (InvasionSpawnDirectory.Spots.Size() > 0)
		InvasionSpawnDirectory.FallbackSource = INVSPAWN_PLAYERSTART;
}

static void Net_RebuildInvasionSpawnSpots(int wave)
{
	if (primaryLevel == nullptr || gamestate != GS_LEVEL)
	{
		InvasionSpawnDirectory.Reset();
		return;
	}

	if (InvasionSpawnDirectory.Spots.Size() == 0)
	{
		if (InvasionRegisteredSpawnSpotLevel == primaryLevel && InvasionRegisteredSpawnSpots.Size() > 0)
		{
			for (const auto& spot : InvasionRegisteredSpawnSpots)
			{
				InvasionSpawnDirectory.Spots.Push(spot);
			}
			DebugTrace::Markf("invasion", "registered-spots copied=%u map=%s",
				unsigned(InvasionRegisteredSpawnSpots.Size()),
				primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
		}

		const PClassActor* invasionSpotBase = Net_GetInvasionSpotBaseClass();
		if (InvasionSpawnDirectory.Spots.Size() == 0 && invasionSpotBase != nullptr)
		{
			auto it = primaryLevel->GetThinkerIterator<AActor>();
			AActor* actor = nullptr;
			while ((actor = it.Next()) != nullptr)
			{
				if (!actor->IsA(invasionSpotBase))
					continue;

				FInvasionSpawnSpotRecord record;
				record.Pos = actor->Pos();
				record.Yaw = actor->Angles.Yaw;
				record.Tag = actor->tid;
				record.StartSpawnNumber = max<int>(actor->args[0], 1);
				record.SpawnDelayTics = max(Net_InvasionSecondsToTics(float(actor->args[1])), 0);
				record.RoundSpawnDelayTics = max(Net_InvasionSecondsToTics(float(actor->args[2])), 0);
				record.FirstWave = max<int>(actor->args[3], 1);
				record.MaxSpawnNumber = max<int>(actor->args[4], 0);
				record.Source = INVSPAWN_CLASSIC;
				record.SpotClass = actor->GetClass();
				record.PlannedSpawnCount = 0;
				record.SpawnedCount = 0;
				record.NextSpawnGameTic = 0;
				InvasionSpawnDirectory.Spots.Push(record);
			}
		}

		if (InvasionSpawnDirectory.Spots.Size() == 0)
		{
			Net_BuildFallbackInvasionSpots(primaryLevel);
		}
		InvasionSpawnDirectory.TotalSpotCount = int(InvasionSpawnDirectory.Spots.Size());
	}

	InvasionSpawnDirectory.ActiveTag = Net_SelectInvasionSpotTag(InvasionSpawnDirectory.Spots, wave);
	InvasionSpawnDirectory.TotalSpawnBudget = 0;
	InvasionSpawnDirectory.ActiveSpotCount = 0;
	InvasionSpawnDirectory.SpawnedThisWave = 0;

	for (auto& spot : InvasionSpawnDirectory.Spots)
	{
		spot.SpawnedCount = 0;
		spot.NextSpawnGameTic = gametic + spot.RoundSpawnDelayTics;

		if (InvasionSpawnDirectory.ActiveTag > 0 && spot.Tag > 0 && spot.Tag != InvasionSpawnDirectory.ActiveTag)
		{
			spot.PlannedSpawnCount = 0;
			continue;
		}

		spot.PlannedSpawnCount = Net_ComputeInvasionSpotWaveCount(spot, wave);
		if (spot.PlannedSpawnCount > 0)
		{
			InvasionSpawnDirectory.TotalSpawnBudget += spot.PlannedSpawnCount;
			++InvasionSpawnDirectory.ActiveSpotCount;
		}
	}
}

static void Net_PrepareInvasionMirrorFromSnapshot(EInvasionState previousState, int previousWave)
{
	if (I_IsLocalHCDEServiceAuthority())
		return;

	// Clients mirror only compact invasion UI/status fields from the server.
	// Monster spawning remains server-authored through explicit spawn events;
	// rebuilding local spot plans here made join clients run a second invasion.
	const bool waveChanged = previousWave != InvasionWaveDirector.Wave;
	const bool leftActiveRound = Net_IsInvasionRoundActiveState(previousState)
		&& !Net_IsInvasionRoundActiveState(InvasionState);
	if (waveChanged || leftActiveRound || InvasionState == INVS_DISABLED)
	{
		Net_DestroyTrackedInvasionMonsters("mirror-snapshot-reset");
		InvasionPendingSpawnEvents.Clear();
		InvasionPendingMirrorSpawns.Clear();
		InvasionLastAppliedSpawnEventId = 0u;
		InvasionMirrorNextVisualDiagnosticTic = 0;
		InvasionMirrorNextPurgeTic = 0;
	}
	else if (InvasionReplicatedActiveMonsterCount == 0
		&& InvasionState != INVS_SPAWNING
		&& InvasionState != INVS_DISABLED
		&& InvasionState != INVS_COUNTDOWN)
	{
		// Server reports zero active monsters during a non-spawning phase
		// (cleanup/intermission/victory/failure) but the client still has live
		// mirrors. A dropped despawn or death event will otherwise leave them
		// frozen mid-attack until the next wave clears them. Retire any live,
		// non-projectile mirror as if it died so the death animation plays and
		// the actor stops shooting at nothing.
		unsigned reconciled = 0u;
		for (auto& ref : InvasionReplicatedActors)
		{
			AActor* actor = ref.Actor;
			if (actor == nullptr
				|| (actor->ObjectFlags & OF_EuthanizeMe) != 0
				|| ref.IsProjectile
				|| Net_IsInvasionActorCorpseLike(actor))
			{
				continue;
			}
			Net_RetireInvasionMirrorActor(ref, 0);
			++reconciled;
		}
		if (reconciled > 0)
		{
			DebugTrace::Warningf("net",
				"HCDE invasion mirror reconcile state=%s wave=%d retired=%u tracked=%u reason=server-active=0",
				Net_InvasionStateName(InvasionState), InvasionWaveDirector.Wave,
				reconciled, unsigned(InvasionReplicatedActors.Size()));
		}
	}

	InvasionSpawnDirectory.Spots.Clear();
	InvasionWaveDirector.SpawnIntervalTics = 0;
	InvasionWaveDirector.SpawnIntervalCountdown = 0;
}

static int Net_GetInvasionSpotRetryDelayTics(const FInvasionSpawnSpotRecord& spot)
{
	// Keep a short floor so blocked spots (for example near live players) do not
	// spin every tic and starve the rest of the spawn set.
	return max(spot.SpawnDelayTics, TICRATE / 2);
}

static int Net_ComputeInvasionSpotSpawnWindowFloorTics()
{
	int window = 0;
	for (const auto& spot : InvasionSpawnDirectory.Spots)
	{
		if (spot.PlannedSpawnCount <= 0)
			continue;

		// Native Skulltag/Zandronum invasion spots own their spawn pacing. The
		// generic spawn-time cvar must not fail a wave before those spot delays
		// have had a chance to release every planned spawn.
		const int planned = max(spot.PlannedSpawnCount, 1);
		const int roundDelay = max(spot.RoundSpawnDelayTics, 0);
		const int retryDelay = Net_GetInvasionSpotRetryDelayTics(spot);
		window = max(window, roundDelay + retryDelay * planned + TICRATE);
	}
	return window;
}

static bool Net_IsInvasionSpotSpawnReady(const FInvasionSpawnSpotRecord& spot)
{
	if (gametic < spot.NextSpawnGameTic)
		return false;

	if (spot.Source == INVSPAWN_CLASSIC)
	{
		if (spot.PlannedSpawnCount <= 0 || spot.SpawnedCount >= spot.PlannedSpawnCount)
			return false;
	}

	return true;
}

static bool Net_HasPendingClassicInvasionSpotSpawns()
{
	for (const auto& spot : InvasionSpawnDirectory.Spots)
	{
		if (spot.Source == INVSPAWN_CLASSIC
			&& spot.PlannedSpawnCount > 0
			&& spot.SpawnedCount < spot.PlannedSpawnCount)
		{
			return true;
		}
	}
	return false;
}

static void Net_LogInvasionSpawnDiagnostic(const char* event, const FInvasionSpawnSpotRecord* spot, const char* reason, int readySpots, int nextReadyDelayTics)
{
	static int lastWave = -1;
	static int consoleCount = 0;
	static int lastConsoleTic = 0;

	if (lastWave != InvasionWaveDirector.Wave)
	{
		lastWave = InvasionWaveDirector.Wave;
		consoleCount = 0;
		lastConsoleTic = 0;
	}

	const char* spotClassName = "<none>";
	int tid = 0;
	DVector3 pos = DVector3(0, 0, 0);
	int planned = 0;
	int spawned = 0;
	int nextReady = 0;
	if (spot != nullptr)
	{
		spotClassName = spot->SpotClass != nullptr ? spot->SpotClass->TypeName.GetChars() : "<fallback>";
		tid = spot->Tag;
		pos = spot->Pos;
		planned = spot->PlannedSpawnCount;
		spawned = spot->SpawnedCount;
		nextReady = max(spot->NextSpawnGameTic - gametic, 0);
	}

	DebugTrace::Markf("invasion", "spawn %s reason=%s wave=%d/%d state=%s budget=%d spawned=%d ready=%d next-ready=%d spots=%d active=%d tag=%d spot=%s tid=%d planned=%d spot-spawned=%d spot-next=%d pos=(%.1f,%.1f,%.1f)",
		event != nullptr ? event : "diagnostic",
		reason != nullptr ? reason : "unknown",
		InvasionWaveDirector.Wave,
		InvasionWaveDirector.MaxWaves,
		Net_InvasionStateName(InvasionState),
		InvasionWaveDirector.WaveBudget,
		InvasionWaveDirector.WaveSpawned,
		readySpots,
		nextReadyDelayTics,
		InvasionSpawnDirectory.TotalSpotCount,
		InvasionSpawnDirectory.ActiveSpotCount,
		InvasionSpawnDirectory.ActiveTag,
		spotClassName,
		tid,
		planned,
		spawned,
		nextReady,
		pos.X,
		pos.Y,
		pos.Z);

	if (consoleCount >= 8 && gametic - lastConsoleTic < TICRATE * 5)
		return;

	++consoleCount;
	lastConsoleTic = gametic;
	Printf(PRINT_HIGH,
		"Invasion spawn %s: reason=%s wave=%d/%d budget=%d spawned=%d ready=%d next-ready=%d spots=%d active=%d tag=%d spot=%s tid=%d planned=%d spot-spawned=%d\n",
		event != nullptr ? event : "diagnostic",
		reason != nullptr ? reason : "unknown",
		InvasionWaveDirector.Wave,
		InvasionWaveDirector.MaxWaves,
		InvasionWaveDirector.WaveBudget,
		InvasionWaveDirector.WaveSpawned,
		readySpots,
		nextReadyDelayTics,
		InvasionSpawnDirectory.TotalSpotCount,
		InvasionSpawnDirectory.ActiveSpotCount,
		InvasionSpawnDirectory.ActiveTag,
		spotClassName,
		tid,
		planned,
		spawned);
}

static bool Net_TryConsumeInvasionSpawnSlot()
{
	if (InvasionSpawnDirectory.Spots.Size() == 0)
	{
		Net_LogInvasionSpawnDiagnostic("deferred", nullptr, "no-spots", 0, -1);
		return false;
	}

	TArray<int> available;
	int nextReadyDelayTics = -1;
	for (int i = 0; i < int(InvasionSpawnDirectory.Spots.Size()); ++i)
	{
		const auto& spot = InvasionSpawnDirectory.Spots[i];
		if (Net_IsInvasionSpotSpawnReady(spot))
		{
			available.Push(i);
			continue;
		}

		if (spot.Source == INVSPAWN_CLASSIC
			&& spot.PlannedSpawnCount > 0
			&& spot.SpawnedCount < spot.PlannedSpawnCount)
		{
			const int delay = max(spot.NextSpawnGameTic - gametic, 0);
			if (nextReadyDelayTics < 0 || delay < nextReadyDelayTics)
				nextReadyDelayTics = delay;
		}
	}

	if (available.Size() == 0)
	{
		Net_LogInvasionSpawnDiagnostic("deferred", nullptr,
			nextReadyDelayTics >= 0 ? "waiting-for-spot-delay" : "no-ready-spots",
			0, nextReadyDelayTics);
		return false;
	}

	// Try every currently-ready spot at most once per call. This prevents one
	// blocked random pick from stalling the whole burst when other spots are
	// available.
	while (available.Size() > 0)
	{
		const int pickIdx = pr_invasion() % available.Size();
		const int pick = available[pickIdx];
		available[pickIdx] = available[available.Size() - 1];
		available.Pop();

		auto& selected = InvasionSpawnDirectory.Spots[pick];
		const char* failureReason = nullptr;
		PClassActor* attemptedMonsterClass = nullptr;
		bool spawned = Net_SpawnInvasionMonster(selected, &failureReason, nullptr, &attemptedMonsterClass);
		if (!spawned
			&& selected.Source == INVSPAWN_CLASSIC
			&& failureReason != nullptr
			&& stricmp(failureReason, "blocked-location") == 0)
		{
			// Classic invasion spots can be embedded in tight map geometry. When
			// one planned slot is permanently blocked, consume that slot by moving
			// the same monster class to another active invasion spot instead of
			// leaving the wave stuck just below its budget.
			spawned = Net_TryRelocateBlockedClassicInvasionSpawn(selected, attemptedMonsterClass, &failureReason);
		}

		if (selected.Source == INVSPAWN_CLASSIC)
		{
			if (spawned)
				++selected.SpawnedCount;

			const int spawnDelay = Net_GetInvasionSpotRetryDelayTics(selected);
			if (spawnDelay > 0)
				selected.NextSpawnGameTic = gametic + spawnDelay;
		}
		else if (!spawned)
		{
			const int retryDelay = Net_GetInvasionSpotRetryDelayTics(selected);
			if (retryDelay > 0)
				selected.NextSpawnGameTic = gametic + retryDelay;
		}

		if (!spawned)
		{
			Net_LogInvasionSpawnDiagnostic("failed", &selected, failureReason, int(available.Size()) + 1, -1);
			continue;
		}

		++InvasionSpawnDirectory.SpawnedThisWave;
		++InvasionWaveDirector.WaveSpawned;
		DebugTrace::Markf("invasion", "spawned monster wave=%d/%d spawned=%d/%d spot=%s tid=%d pos=(%.1f,%.1f,%.1f)",
			InvasionWaveDirector.Wave,
			InvasionWaveDirector.MaxWaves,
			InvasionWaveDirector.WaveSpawned,
			InvasionWaveDirector.WaveBudget,
			selected.SpotClass != nullptr ? selected.SpotClass->TypeName.GetChars() : "<fallback>",
			selected.Tag,
			selected.Pos.X,
			selected.Pos.Y,
			selected.Pos.Z);
		return true;
	}

	Net_LogInvasionSpawnDiagnostic("failed", nullptr, "all-ready-spots-blocked", 0, -1);
	return false;
}

static void Net_ResetInvasionState(const char* reason)
{
	Net_DestroyTrackedInvasionMonsters(reason != nullptr ? reason : "reset");
	InvasionWaveDirector.Reset();
	InvasionSpawnDirectory.Reset();
	HCDERecentAuthorityEvents.Clear();
	InvasionPendingSpawnEvents.Clear();
	InvasionPendingMirrorSpawns.Clear();
	InvasionReplicatedActors.Clear();
	Net_ClearInvasionReplicatedActorIndexes();
	Net_ClearHCDEReplicatedActors();
	for (int client = 0; client < MAXPLAYERS; ++client)
	{
		HCDEInvasionActorDeltaV2SendCursor[client] = 0u;
		HCDEActorDeltaV2SendCursor[client] = 0u;
		HCDEActorPriorityQueues[client].Clear();
	}
	InvasionNextAuthorityEventSeq = 1u;
	InvasionNextSpawnEventId = 1u;
	InvasionLastAppliedSpawnEventId = 0u;
	InvasionMirrorNextVisualDiagnosticTic = 0;
	InvasionMirrorNextPurgeTic = 0;
	Net_ResetInvasionAnnouncements();
	InvasionReplicatedActiveMonsterCount = 0;
	InvasionWipeGraceTics = 0;
	InvasionNoParticipantTics = 0;
	if (Net_IsInvasionModeEnabled())
		Net_SetInvasionState(INVS_WAITING, 0, reason != nullptr ? reason : "reset");
	else
		Net_SetInvasionState(INVS_DISABLED, 0, reason != nullptr ? reason : "reset");
}

static int Net_CountInvasionParticipants()
{
	int count = 0;
	for (int player = 0; player < MAXPLAYERS; ++player)
	{
		if (playeringame[player] && !I_IsServerReservedSlot(player))
			++count;
	}

	return count;
}

static int Net_CountInvasionAliveParticipants()
{
	int count = 0;
	for (int player = 0; player < MAXPLAYERS; ++player)
	{
		if (!playeringame[player] || I_IsServerReservedSlot(player))
			continue;

		// Dedicated HCDE authority can receive pawn health before the cached
		// player health field is refreshed, so a live pawn with positive health
		// is enough to keep the wave from being treated as a wipe.
		if (players[player].playerstate == PST_LIVE
			&& ((players[player].mo != nullptr && players[player].mo->health > 0)
				|| players[player].health > 0))
		{
			++count;
		}
	}

	return count;
}

static int Net_ComputeInvasionWaveBudget(int wave, bool bossWave)
{
	const int players = max(Net_CountInvasionParticipants(), 1);
	int budget = sv_invasionbasebudget;
	budget += max(wave - 1, 0) * sv_invasionbudgetstep;
	budget += max(players - 1, 0) * sv_invasionperplayer;
	if (bossWave)
		budget += sv_invasionbossbonus;
	return clamp<int>(budget, 1, 65535);
}

static bool Net_IsInvasionBossWave(int wave)
{
	return sv_invasionbosswaveevery > 0 && wave > 0 && (wave % sv_invasionbosswaveevery) == 0;
}

static bool HCDE_CanOverrideCmpgninfLimits()
{
	return !HCDE_ServerMode_IsDedicatedServer() && !netgame;
}

static int Net_GetEffectiveInvasionCountdownTics()
{
	int countdownTics = max(Net_InvasionSecondsToTics(sv_invasioncountdowntime), 0);
	if (countdownTics > 0 && countdownTics < TICRATE * 10)
	{
		if (Net_InvasionDebugEnabled(2))
		{
			Printf(PRINT_HIGH, "HCDE invasion countdown %.2fs is below the stable multiplayer floor; using 10.00s for this wave.\n",
				double(sv_invasioncountdowntime));
			DebugTrace::Warningf("invasion", "countdown clamped requested=%.2f effective=10.00 gametic=%d map=%s",
				double(sv_invasioncountdowntime), gametic,
				primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
		}
		countdownTics = TICRATE * 10;
	}
	return countdownTics;
}

static int Net_ResolveInvasionMaxWaves()
{
	int resolved = clamp<int>(sv_invasionwaves, 1, HCDEInvasionMaxWavesLimit);
	const bool hasMapLimit = primaryLevel != nullptr && primaryLevel->info != nullptr && primaryLevel->info->InvasionWaveLimit > 0;
	const bool useMapMetadata = sv_usemapsettingswavelimit && hasMapLimit;
	const int legacyLimit = clamp<int>(wavelimit_compat, 0, HCDEInvasionMaxWavesLimit);

	if (legacyLimit > 0)
	{
		return legacyLimit;
	}

	// Classic Skulltag/Zandronum online compatibility:
	// if enabled, prefer per-map metadata (CMPGNINF/MAPINFO wavelimit) when present.
	// Server admins can still set explicit `wavelimit` above to override bad
	// or too-small metadata in multiplayer.
	if (useMapMetadata)
	{
		resolved = clamp<int>(primaryLevel->info->InvasionWaveLimit, 1, HCDEInvasionMaxWavesLimit);
	}

	return resolved;
}

int Net_GetCompatDuelLimit()
{
	const bool canOverrideCmpgninfLimits = HCDE_CanOverrideCmpgninfLimits();
	const bool hasMapLimit = primaryLevel != nullptr && primaryLevel->info != nullptr && primaryLevel->info->DuelLimit > 0;

	// Skulltag/CMPGNINF compatibility: in online sessions map metadata
	// settings (duellimit) win unless an offline override mode is active.
	if (hasMapLimit && !canOverrideCmpgninfLimits)
	{
		return clamp<int>(primaryLevel->info->DuelLimit, 1, 255);
	}

	return clamp<int>(duellimit_compat, 0, 255);
}

static void Net_StartInvasionWave(const char* reason);

static void Net_StartInvasionCountdownForNextWave(const char* reason)
{
	InvasionWaveDirector.MaxWaves = Net_ResolveInvasionMaxWaves();
	if (InvasionWaveDirector.Wave >= InvasionWaveDirector.MaxWaves)
	{
		Net_SetInvasionState(INVS_VICTORY, Net_InvasionSecondsToTics(sv_invasionresulttime),
			reason != nullptr ? reason : "waves-complete");
		return;
	}

	// Skulltag invasion ACS expects GetInvasionWave() to already report the
	// pending wave while GetInvasionState() is IS_COUNTDOWN.
	Net_DestroyTrackedInvasionMonsters("wave-countdown");
	++InvasionWaveDirector.Wave;
	InvasionWaveDirector.WaveFlags = Net_IsInvasionBossWave(InvasionWaveDirector.Wave) ? INV_WAVEF_BOSS : 0u;
	InvasionWaveDirector.WaveBudget = 0;
	InvasionWaveDirector.WaveSpawned = 0;
	InvasionWaveDirector.WaveCleared = 0;
	InvasionWaveDirector.ActiveMonsters.Clear();
	InvasionSpawnDirectory.Reset();
	CutsceneCountdown = 0;

	const int countdownTics = Net_GetEffectiveInvasionCountdownTics();
	if (countdownTics > 0)
	{
		Net_SetInvasionState(INVS_COUNTDOWN, countdownTics, reason != nullptr ? reason : "wave-countdown");
	}
	else
	{
		Net_StartInvasionWave(reason != nullptr ? reason : "wave-countdown-complete");
	}
}

static void Net_StartInvasionWave(const char* reason)
{
	InvasionWaveDirector.MaxWaves = Net_ResolveInvasionMaxWaves();
	const bool usingPendingCountdownWave = InvasionState == INVS_COUNTDOWN
		&& InvasionWaveDirector.Wave > 0
		&& InvasionWaveDirector.WaveSpawned == 0
		&& InvasionWaveDirector.ActiveMonsters.Size() == 0;
	if (!usingPendingCountdownWave && InvasionWaveDirector.Wave >= InvasionWaveDirector.MaxWaves)
	{
		Net_SetInvasionState(INVS_VICTORY, Net_InvasionSecondsToTics(sv_invasionresulttime),
			reason != nullptr ? reason : "waves-complete");
		return;
	}

	// Defensive cleanup for forced transitions (console/API) so a new wave never
	// inherits live actors from an older wave.
	Net_DestroyTrackedInvasionMonsters("wave-start");

	if (!usingPendingCountdownWave)
		++InvasionWaveDirector.Wave;

	if ((dmflags & DF_FAST_MONSTERS) != 0)
	{
		dmflags = int(dmflags) & ~DF_FAST_MONSTERS;
		Printf(PRINT_HIGH, "HCDE invasion disabled fast monsters for wave %d\n", InvasionWaveDirector.Wave);
	}

	const bool bossWave = Net_IsInvasionBossWave(InvasionWaveDirector.Wave);
	InvasionWaveDirector.WaveFlags = bossWave ? INV_WAVEF_BOSS : 0u;
	InvasionWaveDirector.WaveBudget = Net_ComputeInvasionWaveBudget(InvasionWaveDirector.Wave, bossWave);
	InvasionWaveDirector.WaveSpawned = 0;
	InvasionWaveDirector.WaveCleared = 0;
	InvasionWaveDirector.ActiveMonsters.Clear();
	int unscaledWaveBudget = InvasionWaveDirector.WaveBudget;
	const int baseSpawnIntervalTics = max(Net_InvasionSecondsToTics(sv_invasionspawninterval), 1);
	InvasionWaveDirector.SpawnIntervalTics = max<int>(
		int(ceil(double(baseSpawnIntervalTics) * Net_GetInvasionSkillSpawnIntervalScale())),
		1);
	InvasionWaveDirector.SpawnIntervalCountdown = InvasionWaveDirector.SpawnIntervalTics;

	Net_RebuildInvasionSpawnSpots(InvasionWaveDirector.Wave);
	if (InvasionSpawnDirectory.TotalSpawnBudget > 0)
	{
		// Invasion spot records own wave budgets when a map provides native invasion
		// markers. The Stage 3 budget cvars remain as fallback for non-invasion maps.
		InvasionWaveDirector.WaveBudget = InvasionSpawnDirectory.TotalSpawnBudget;
		unscaledWaveBudget = InvasionWaveDirector.WaveBudget;
	}
	InvasionWaveDirector.WaveBudget = Net_ApplyInvasionSkillBudgetScale(InvasionWaveDirector.WaveBudget);

	int spawnWindow = Net_InvasionSecondsToTics(sv_invasionspawntime);
	if (spawnWindow <= 0)
	{
		const int burst = max<int>(sv_invasionspawnburst, 1);
		const int chunks = (InvasionWaveDirector.WaveBudget + burst - 1) / burst;
		spawnWindow = max(chunks * InvasionWaveDirector.SpawnIntervalTics, 1);
	}
	const int spotWindow = Net_ComputeInvasionSpotSpawnWindowFloorTics();
	if (spotWindow > spawnWindow)
	{
		Printf(PRINT_HIGH, "HCDE invasion spawn window extended: cvar=%d spot-floor=%d wave=%d/%d budget=%d spots=%d\n",
			spawnWindow, spotWindow, InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves,
			InvasionWaveDirector.WaveBudget, InvasionSpawnDirectory.ActiveSpotCount);
		spawnWindow = spotWindow;
	}

	DebugTrace::Markf("invasion", "wave=%d/%d budget=%d rawbudget=%d skill=%d spots=%d active=%d tag=%d fallback=%d source=%s interval=%d fastmonsters=%d map=%s",
		InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves, InvasionWaveDirector.WaveBudget,
		unscaledWaveBudget, clamp<int>(gameskill, 0, 4),
		InvasionSpawnDirectory.TotalSpotCount, InvasionSpawnDirectory.ActiveSpotCount, InvasionSpawnDirectory.ActiveTag,
		InvasionSpawnDirectory.UsingFallback ? 1 : 0, Net_InvasionSpawnSourceName(InvasionSpawnDirectory.FallbackSource),
		InvasionWaveDirector.SpawnIntervalTics,
		(dmflags & DF_FAST_MONSTERS) != 0 ? 1 : 0,
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
	Printf(PRINT_HIGH, "Invasion wave %d/%d starting: budget=%d rawbudget=%d skill=%d spots=%d active=%d cap=%d speedscale=%.2f tag=%d fallback=%d source=%s interval=%d fastmonsters=%d map=%s\n",
		InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves, InvasionWaveDirector.WaveBudget,
		unscaledWaveBudget, clamp<int>(gameskill, 0, 4),
		InvasionSpawnDirectory.TotalSpotCount, InvasionSpawnDirectory.ActiveSpotCount,
		Net_GetInvasionActiveMonsterCap(), Net_GetInvasionSkillMonsterSpeedScale(),
		InvasionSpawnDirectory.ActiveTag,
		InvasionSpawnDirectory.UsingFallback ? 1 : 0, Net_InvasionSpawnSourceName(InvasionSpawnDirectory.FallbackSource),
		InvasionWaveDirector.SpawnIntervalTics,
		(dmflags & DF_FAST_MONSTERS) != 0 ? 1 : 0,
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");

	Net_SetInvasionState(INVS_SPAWNING, spawnWindow, reason != nullptr ? reason : "wave-start");
}

static void Net_MarkInvasionVictory(const char* reason)
{
	Net_SetInvasionState(INVS_VICTORY, Net_InvasionSecondsToTics(sv_invasionresulttime), reason);
}

static void Net_MarkInvasionFailure(const char* reason)
{
	Net_SetInvasionState(INVS_FAILURE, Net_InvasionSecondsToTics(sv_invasionresulttime), reason);
}

// Transitions the invasion mode to the next wave or victory after a wave is cleared.
static void Net_AdvanceInvasionAfterWaveClear(const char* reason)
{
	if (InvasionWaveDirector.Wave >= InvasionWaveDirector.MaxWaves)
	{
		Net_MarkInvasionVictory(reason != nullptr ? reason : "all-waves-cleared");
	}
	else
	{
		Net_SetInvasionState(INVS_INTERMISSION, Net_InvasionSecondsToTics(sv_invasionintermissiontime),
			reason != nullptr ? reason : "wave-cleared");
	}
}

// Main authoritative tick for the invasion state machine.
// Only runs on the network authority (server/host).
static void Net_TickInvasionState()
{
	// Mode is disabled globally via sv_gametype.
	if (!Net_IsInvasionModeEnabled())
	{
		if (InvasionState != INVS_DISABLED)
		{
			// Clean up and notify clients if the mode is toggled off during a level.
			Net_DestroyTrackedInvasionMonsters("mode-disabled");
			InvasionWaveDirector.Reset();
			InvasionSpawnDirectory.Reset();
			InvasionNoParticipantTics = 0;
			Net_SetInvasionState(INVS_DISABLED, 0, "mode-disabled");
		}
		return;
	}

	// Mode is enabled but has not been initialized for this level yet.
	if (InvasionState == INVS_DISABLED)
	{
		InvasionWaveDirector.Reset();
		InvasionSpawnDirectory.Reset();
		InvasionNoParticipantTics = 0;
		Net_SetInvasionState(INVS_WAITING, 0, "mode-enabled");
	}

	// Once the map is live and at least one participant exists, move out of the
	// idle waiting state automatically. This keeps native invasion maps from
	// sitting forever on "waiting" unless some external cutscene flow is used.
	if (InvasionState == INVS_WAITING)
	{
		if (Net_CountInvasionParticipants() > 0)
		{
			const int countdownTics = Net_GetEffectiveInvasionCountdownTics();
			if (countdownTics > 0)
			{
				Net_StartInvasionCountdownForNextWave("waiting-autostart");
			}
			else
			{
				Net_StartInvasionWave("waiting-autostart");
			}
		}
		return;
	}

	// Wait for countdown to finish (e.g. from Net_StartCutscene).
	if (InvasionState == INVS_COUNTDOWN)
	{
		// Support both flows:
		// - cutscene-ready countdowns continue to mirror the shared cutscene timer
		// - native invasion auto-starts count down locally when no cutscene timer exists
		if (CutsceneCountdown > 0)
		{
			if (!netgame)
				--CutsceneCountdown;
			InvasionStateTics = max(CutsceneCountdown, 0);
			if (CutsceneCountdown <= 0)
				Net_StartInvasionWave("countdown-complete");
			return;
		}

		if (InvasionStateTics > 0)
			--InvasionStateTics;

		if (InvasionStateTics <= 0)
			Net_StartInvasionWave("countdown-complete");

		return;
	}

	// Dedicated servers can temporarily have no active participants between
	// probe joins and real players. Keep the current invasion wave metadata
	// stable for a short reconnect window before forcing a safe reset.
	if (I_IsDedicatedServerMode()
		&& Net_IsInvasionRoundActiveState(InvasionState)
		&& Net_CountInvasionParticipants() <= 0)
	{
		InvasionWipeGraceTics = 0;
		if (InvasionNoParticipantTics <= 0)
		{
			InvasionNoParticipantTics = TICRATE * 30;
		}
		else if (--InvasionNoParticipantTics <= 0)
		{
			Net_ResetInvasionState("no-participants-timeout");
			return;
		}
	}
	else
	{
		InvasionNoParticipantTics = 0;
	}

	if (InvasionStateTics > 0)
		--InvasionStateTics;

	Net_UpdateInvasionWaveClearProgress();

	// Global player-wipe and participant-check logic.
	if (Net_IsInvasionRoundActiveState(InvasionState))
	{
		const int participants = Net_CountInvasionParticipants();
		if (participants <= 0)
		{
			// No active participants remain; reset any active wave state.
			Net_ResetInvasionState("no-participants");
			return;
		}

		const int aliveParticipants = Net_CountInvasionAliveParticipants();
		if (aliveParticipants <= 0)
		{
			// If every participant is dead at the same time, treat this as a wipe.
			// Keep a short grace period so same-tic state transitions do not
			// immediately trigger failure.
			if (InvasionWipeGraceTics <= 0)
			{
				InvasionWipeGraceTics = TICRATE;
			}
			else if (--InvasionWipeGraceTics <= 0)
			{
				Net_MarkInvasionFailure("all-players-dead");
				InvasionWipeGraceTics = 0;
			}
		}
		else
		{
			InvasionWipeGraceTics = 0;
		}
	}

	switch (InvasionState)
	{
	case INVS_SPAWNING:
		// Initialize the wave director if this is the first spawn tick.
		if (InvasionWaveDirector.Wave <= 0)
			Net_StartInvasionWave("spawn-wave-bootstrap");

		if (InvasionWaveDirector.WaveSpawned < InvasionWaveDirector.WaveBudget)
		{
			// Process spawn burst based on interval.
			if (--InvasionWaveDirector.SpawnIntervalCountdown <= 0)
			{
				InvasionWaveDirector.SpawnIntervalCountdown = InvasionWaveDirector.SpawnIntervalTics;
				const int activeCap = Net_GetInvasionActiveMonsterCap();
				const int activeMonsters = Net_GetInvasionActiveMonsterCount();
				if (activeMonsters >= activeCap)
				{
					DebugTrace::Markf("invasion", "spawn throttled active=%d cap=%d wave=%d/%d spawned=%d/%d",
						activeMonsters, activeCap, InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves,
						InvasionWaveDirector.WaveSpawned, InvasionWaveDirector.WaveBudget);
				}
				else
				{
					const int burst = min<int>(max<int>(sv_invasionspawnburst, 1), activeCap - activeMonsters);
					int consumed = 0;
					while (consumed < burst && InvasionWaveDirector.WaveSpawned < InvasionWaveDirector.WaveBudget)
					{
						if (!Net_TryConsumeInvasionSpawnSlot())
							break;
						++consumed;
					}
				}
			}
		}
		Net_UpdateInvasionWaveClearProgress();

		// If the wave budget is fully spawned and already fully cleared, advance
		// immediately without waiting for cleanup timeout.
		if (InvasionWaveDirector.WaveSpawned >= InvasionWaveDirector.WaveBudget
			&& InvasionWaveDirector.WaveCleared >= InvasionWaveDirector.WaveBudget)
		{
			Net_AdvanceInvasionAfterWaveClear("wave-cleared-during-spawn");
			break;
		}

		// Move to cleanup phase once spawning is done or the spawn window expires.
		if (InvasionWaveDirector.WaveSpawned >= InvasionWaveDirector.WaveBudget || InvasionStateTics <= 0)
		{
			if (InvasionWaveDirector.WaveSpawned < InvasionWaveDirector.WaveBudget
				&& Net_HasPendingClassicInvasionSpotSpawns())
			{
				// Native invasion spots may intentionally delay late spawns. Do
				// not end the spawn phase while a planned spot still has budget.
				InvasionStateTics = TICRATE;
			}
			else
			{
				Net_SetInvasionState(INVS_CLEANUP, Net_InvasionSecondsToTics(sv_invasioncleanuptime),
					InvasionWaveDirector.WaveSpawned >= InvasionWaveDirector.WaveBudget ? "wave-spawned" : "spawn-timeout");
			}
		}
		break;

	case INVS_CLEANUP:
		// Transition to next wave or victory once all monsters are cleared.
		//
		// Use WaveSpawned rather than WaveBudget here. Native invasion spots can
		// intentionally miss or delay some budget entries before the spawn window
		// ends; once cleanup has started, only monsters that actually spawned can
		// still be killed. Waiting for the original budget makes the wave appear
		// stuck after players clear every visible monster.
		if (InvasionWaveDirector.WaveCleared >= InvasionWaveDirector.WaveSpawned)
		{
			Net_AdvanceInvasionAfterWaveClear("cleanup-complete");
		}
		// Skulltag/Zandronum invasion waves do not fail just because spawned
		// monsters lived longer than a short cleanup timer. Keep the wave open
		// until players clear the remaining monsters or everyone wipes.
		else if (InvasionStateTics <= 0)
		{
			InvasionStateTics = TICRATE;
		}
		break;

	case INVS_INTERMISSION:
		// Brief pause between waves.
		if (InvasionStateTics <= 0)
			Net_StartInvasionCountdownForNextWave("intermission-complete");
		break;

	case INVS_VICTORY:
	case INVS_FAILURE:
		// Victory behaves like a finished map: keep the result visible for the
		// configured duration, then advance to the map's normal exit target.
		if (InvasionStateTics <= 0)
		{
			if (InvasionState == INVS_VICTORY
				&& sv_invasionexitonvictory
				&& primaryLevel != nullptr
				&& gameaction == ga_nothing)
			{
				DebugTrace::Markf("invasion", "victory exit map=%s wave=%d/%d gametic=%d",
					primaryLevel->MapName.GetChars(),
					InvasionWaveDirector.Wave,
					InvasionWaveDirector.MaxWaves,
					gametic);
				if (Net_InvasionDebugEnabled(1))
					Printf(PRINT_HIGH, "HCDE invasion victory complete: exiting %s\n", primaryLevel->MapName.GetChars());
				primaryLevel->ExitLevel(0, false);
				break;
			}
			Net_ResetInvasionState("result-finished");
		}
		break;

	default:
		break;
	}
}

static void Net_TickInvasionMirrorVisualFrame()
{
	if (Net_IsLocalInvasionAuthority() || InvasionState == INVS_DISABLED)
		return;

	if (InvasionMirrorVisualTickBudget > 0)
	{
		// Catch-up can run several gameplay tics in one frame. Mirror actors are
		// visual-only server replicas, so move them once per frame and let the
		// latest authoritative target correct them instead of multiplying speed.
		// Spawn drains are also gated here to prevent a burst of actor creation
		// when processing multiple tics in a single frame, which would otherwise
		// cause visible lag spikes during network catch-up.
		--InvasionMirrorVisualTickBudget;
		if (gametic >= InvasionMirrorNextPurgeTic)
		{
			InvasionMirrorNextPurgeTic = gametic + TICRATE * 2;
			Net_PurgeStaleInvasionMirrorActorsOnClient();
		}

		const uint64_t visualStartUS = HCDEProfileNowUS();
		Net_DrainPendingInvasionSpawnEvents();
		Net_DrainPendingInvasionMirrorSpawns();
		unsigned updated = 0u;
		unsigned skipped = 0u;
		Net_TickInvasionMirrorVisualActors(updated, skipped);
		LastMirrorVisualPassUS = HCDEProfileNowUS() - visualStartUS;
		HCDEProfileRecordMirrorVisualPass(LastMirrorVisualPassUS, updated, skipped);
		Net_LogInvasionMirrorVisualDiagnostic();
