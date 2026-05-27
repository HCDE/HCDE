// HCDE world-state checksum module.
//
// The server hashes a small set of categorized game-state samples each time it
// builds a snapshot for a remote client (gated by `net_checksum`). The chunk is
// appended right before the snapshot trailer; clients recompute their own
// hashes at apply time and compare. Mismatches feed `Net_ChecksumReportMismatch`
// which surfaces a `net.desync` warning, triggers a black-box flush, and (if a
// cooldown allows) writes a diagnostic bundle.
//
// Both ends MUST iterate the same actor / sector / mover list in the same
// order; whenever a category gains new fields, bump the hash version implicit
// in the server snapshot envelope or flip the relevant CHK_* slot to a fresh
// value so old peers degrade silently rather than spam mismatches.

#include "d_net_checksum.h"
#include "d_net_diagnostics.h"
#include "d_net_blackbox.h"
#include "d_net.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_level.h"
#include "g_levellocals.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "common/engine/debugtrace.h"
#include "m_crc32.h"
#include "common/engine/i_net.h"
#include "dthinker.h"

#include <climits>
#include <cmath>
#include <cstring>

CUSTOM_CVAR(Int, net_checksum, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
}

// Legacy throttle knob. Net_ChecksumTick() now recomputes every gametic so the
// per-tic ring buffer stays dense enough for async snapshot comparison; this
// CVAR is retained only so older configs don't break on load.
CUSTOM_CVAR(Int, net_checksum_interval, 4, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	else if (self > 35)
		self = 35;
}

CUSTOM_CVAR(Int, net_checksum_categories, 0x3F, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 0x3F)
		self = 0x3F;
}

namespace
{
constexpr uint8_t ChecksumMagic[4] = { 'H', 'C', 'K', 'S' };
constexpr uint8_t ChecksumVersion = 1u;

enum EChecksumCategory : uint8_t
{
	CHK_PLAYERS = 0,
	CHK_SECTORS = 1,
	CHK_MOVERS = 2,
	CHK_ACTORS = 3,
	CHK_RNG = 4,
	CHK_LINESPEC = 5,
	CHK_COUNT = 6,
};

uint32_t CategoryHashes[CHK_COUNT] = {};
// Highest gametic that triggered Net_ChecksumComputeAll(); used both by the
// per-tic compute throttle and to deduplicate recomputes when the server
// builds multiple snapshots within the same world tic.
int LastComputedTic = INT_MIN;
uint32_t LineSpecRollingHash = 0u;
// Per-tic snapshot ring buffer. Without this, the comparison in
// Net_ChecksumReadAndCompare hashed the *current* world state and matched it
// against the server's hash from a possibly-different gametic - in async
// netcode the client may be behind the authority by several tics, so its
// hash naturally differs from the authority's hash for the snapshot's tic
// even when the worlds are perfectly synchronized. That false-positive lit
// CHECKSUM_MISMATCH every snapshot, which in turn paid for `CopyFileToLatest`
// and bundle writes; the result was a feedback loop where the diagnostic
// system itself produced enough I/O to make the desync worse.
//
// The buffer is keyed by gametic and stores the post-compute snapshot of
// CategoryHashes[]. ReadAndCompare() looks up the bucket whose Tic matches
// the server's compute tic and only reports a mismatch when both peers
// actually produced a hash for the same logical world tic. If we have no
// matching bucket (server raced ahead, or we're so far behind that the entry
// was already overwritten) we silently skip the comparison rather than treat
// "we don't know" as "they disagree".
struct TicHashBucket
{
	int Tic = INT_MIN;
	uint32_t Hashes[CHK_COUNT] = {};
};
constexpr size_t TicHashHistoryDepth = 64u;
TicHashBucket TicHashHistory[TicHashHistoryDepth] = {};
size_t TicHashHistoryCursor = 0u;
// Cooldown stamps to keep `Net_ChecksumReportMismatch` from spamming bundles
// when a single tic of drift fires across every category in succession.
uint64_t LastMismatchReportMS = 0u;
uint64_t LastMismatchBundleMS = 0u;
constexpr uint64_t MismatchReportCooldownMS = 250u;
// The bundle cooldown was 5s, which sounds conservative until you realize a
// genuinely diverging session produces a fresh mismatch on every snapshot
// tic. With a 5s cooldown that meant one Net_DiagWriteBundle call every 5s,
// and each bundle is several megabytes of trace + blackbox + metric data
// being flushed synchronously to disk. The resulting I/O stalls fed straight
// back into prediction lead, which fed back into more drift, which fed back
// into more "mismatches". 60s is still well inside a typical play session
// so a real desync still produces the forensic dump the user can ship to
// us, but the inter-bundle gap is large enough that the dump itself stops
// being part of the symptom.
constexpr uint64_t MismatchBundleCooldownMS = 60000u;

uint32_t MixU32(uint32_t hash, uint32_t value)
{
	return Bcrc32(&value, sizeof(value), hash);
}

// Hash a double by quantizing to 1/256 unit precision and treating the result
// as an int32. NaN and infinities are coerced to 0 so they can't poison the
// hash differently on each peer (msvc/clang/llvm's int conversion of NaN is
// UB and varies by toolchain).
uint32_t MixDouble(uint32_t hash, double value)
{
	if (!std::isfinite(value))
		return MixU32(hash, 0u);
	const double clamped = std::max(-2147483648.0, std::min(2147483392.0, value * 256.0));
	const int32_t scaled = int32_t(clamped);
	return MixU32(hash, uint32_t(scaled));
}

// Player checksum intentionally OMITS position/velocity. The whole point of
// HCDE's prediction system is that the local client runs the player pawn one
// or more tics ahead of the authority - so any byte-exact compare of mo->X()
// against the snapshot's authoritative value is guaranteed to disagree at
// MixDouble's 1/256-unit precision the moment the player so much as twitches
// the stick. That used to fire CHECKSUM_MISMATCH category=players literally
// every snapshot tic, which in turn fired the Net_DiagWriteBundle path on its
// 5s cooldown and dumped multi-megabyte forensic bundles back to back. The
// resulting disk I/O stalls were a primary contributor to the "ping spike +
// invisible monster attacks" feedback loop the user was fighting.
//
// Real position drift is already handled by HCDELocalPlayerNeedsPoseRepair /
// HCDELocalDriftExceedsPredictionAllowance - that path computes drift on the
// authoritative coordinates and applies snaps when they exceed the lead-aware
// allowance. Including position here only generated noise that made the
// reconcile traces harder to read AND caused expensive bundle writes. Health,
// playerstate, and onground stay in - those should NOT diverge under
// prediction, so a mismatch in them is genuine and worth a forensic dump.
void ComputePlayerHash()
{
	uint32_t hash = 0u;
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (!playeringame[i])
			continue;
		const player_t& player = players[i];
		const AActor* mo = player.mo;
		if (mo == nullptr)
			continue;
		hash = MixU32(hash, uint32_t(i));
		hash = MixU32(hash, uint32_t(mo->health));
		hash = MixU32(hash, uint32_t(player.playerstate));
		hash = MixU32(hash, player.onground ? 1u : 0u);
	}
	CategoryHashes[CHK_PLAYERS] = hash;
}

void ComputeSectorHash()
{
	uint32_t hash = 0u;
	if (primaryLevel == nullptr)
	{
		CategoryHashes[CHK_SECTORS] = 0u;
		return;
	}
	for (int i = 0; i < primaryLevel->sectors.Size(); ++i)
	{
		const sector_t* sec = &primaryLevel->sectors[i];
		hash = MixU32(hash, uint32_t(i));
		hash = MixDouble(hash, sec->floorplane.fD());
		hash = MixDouble(hash, sec->ceilingplane.fD());
		hash = MixU32(hash, uint32_t(sec->lightlevel));
		hash = MixU32(hash, uint32_t(sec->special));
	}
	CategoryHashes[CHK_SECTORS] = hash;
}

void ComputeMoverHash()
{
	uint32_t hash = 0u;
	if (primaryLevel == nullptr)
	{
		CategoryHashes[CHK_MOVERS] = 0u;
		return;
	}
	for (int i = 0; i < primaryLevel->sectors.Size(); ++i)
	{
		sector_t* sec = &primaryLevel->sectors[i];
		if (!sec->PlaneMoving(sector_t::floor) && !sec->PlaneMoving(sector_t::ceiling))
			continue;
		hash = MixU32(hash, uint32_t(i));
		hash = MixDouble(hash, sec->floorplane.fD());
		hash = MixDouble(hash, sec->ceilingplane.fD());
		hash = MixU32(hash, sec->floordata != nullptr ? 1u : 0u);
		hash = MixU32(hash, sec->ceilingdata != nullptr ? 1u : 0u);
	}
	CategoryHashes[CHK_MOVERS] = hash;
}

// Hash a class name into a process-stable 32-bit value. We CANNOT use
// FName::GetIndex() here: those indices are assigned in registration order
// inside each individual process, so the same FName ("DoomImp") may have a
// different index in the client vs the server even when both processes
// loaded the exact same WAD. Hashing the printable name string instead
// guarantees the same input maps to the same output across processes, which is what
// the cross-peer comparison in Net_ChecksumReadAndCompare actually needs.
static uint32_t HashClassName(const PClass* cls)
{
	if (cls == nullptr)
		return 0u;
	const char* name = cls->TypeName.GetChars();
	if (name == nullptr || *name == '\0')
		return 0u;
	return CalcCRC32(reinterpret_cast<const uint8_t*>(name), unsigned(strlen(name)));
}

// Actor checksum uses a *commutative* accumulator (per-actor mini-hash plus
// a running sum) instead of feeding the global hash with each actor in
// iteration order. The reason is that TThinkerIterator visits actors in the
// order they were registered with the level's stat lists, and that order
// CAN diverge between client and server even when both peers agree on the
// final population - the most common cases:
//   * Client-side prediction fires a rocket / spawns a puff one or more
//     tics before the authority runs the matching usercmd, so the spawn
//     event is observed at different gametics on each peer.
//   * Replicated invasion mirrors are inserted on the client when their
//     spawn snapshot arrives, which can lag a tic or two behind the
//     server's local spawn.
// With a sequential CRC32 chain those reorderings produce different
// 32-bit values for byte-identical actor populations, which fired
// `CHECKSUM_MISMATCH category=actors` repeatedly even though the worlds
// were genuinely synchronized. Summing per-actor hashes is order-agnostic
// while still flipping the final value if any actor's class/health
// actually differs - that's the property the cross-peer compare needs.
void ComputeActorHash()
{
	uint32_t accum = 0u;
	unsigned count = 0u;
	if (primaryLevel != nullptr)
	{
		TThinkerIterator<AActor> it(primaryLevel);
		AActor* actor = nullptr;
		while ((actor = it.Next()) != nullptr)
		{
			if ((actor->ObjectFlags & OF_EuthanizeMe) != 0)
				continue;
			++count;
			uint32_t entry = MixU32(0u, HashClassName(actor->GetClass()));
			entry = MixU32(entry, uint32_t(actor->health));
			accum += entry;
		}
	}
	uint32_t hash = MixU32(0u, count);
	hash = MixU32(hash, accum);
	CategoryHashes[CHK_ACTORS] = hash;
}

void ComputeRngHash()
{
	uint32_t hash = 0u;
	hash = MixU32(hash, uint32_t(rngseed));
	hash = MixU32(hash, uint32_t(gametic));
	CategoryHashes[CHK_RNG] = hash;
}

void ComputeLineSpecHash()
{
	CategoryHashes[CHK_LINESPEC] = LineSpecRollingHash;
}

// Drop the contents of CategoryHashes[] into the next slot of the per-tic
// ring buffer so client-side ReadAndCompare can later look us up by tic.
// Net_ChecksumComputeAll() zeroes any disabled category slot before calling
// here, so the bucket always reflects the active mask rather than stale
// values left over from an earlier compute when the user toggled categories.
void StoreTicHashSnapshot()
{
	TicHashBucket& bucket = TicHashHistory[TicHashHistoryCursor];
	bucket.Tic = gametic;
	for (uint8_t i = 0u; i < CHK_COUNT; ++i)
		bucket.Hashes[i] = CategoryHashes[i];
	TicHashHistoryCursor = (TicHashHistoryCursor + 1u) % TicHashHistoryDepth;
}

// Find the ring-buffer entry whose stored tic matches `lookupTic`. Returns
// nullptr when the requested tic isn't in the buffer (either we haven't run
// it yet, or it has already aged out beyond TicHashHistoryDepth tics ago).
const TicHashBucket* FindTicHashBucket(int lookupTic)
{
	for (size_t i = 0u; i < TicHashHistoryDepth; ++i)
	{
		const TicHashBucket& bucket = TicHashHistory[i];
		if (bucket.Tic == lookupTic)
			return &bucket;
	}
	return nullptr;
}

// Compute every enabled category hash and remember the gametic the snapshot
// was taken on. Callers should prefer Net_ChecksumComputeIfStale() to avoid
// recomputing the same set of hashes more than once per world tic.
void Net_ChecksumComputeAll()
{
	const int categoryMask = *net_checksum_categories;
	// Zero disabled slots first so a toggled-off category cannot leave a stale
	// hash in CategoryHashes[] (and therefore in the ring buffer) from the
	// previous compute pass.
	for (uint8_t i = 0u; i < CHK_COUNT; ++i)
	{
		if ((categoryMask & (1 << i)) == 0)
			CategoryHashes[i] = 0u;
	}
	if ((categoryMask & (1 << CHK_PLAYERS)) != 0)
		ComputePlayerHash();
	if ((categoryMask & (1 << CHK_SECTORS)) != 0)
		ComputeSectorHash();
	if ((categoryMask & (1 << CHK_MOVERS)) != 0)
		ComputeMoverHash();
	if ((categoryMask & (1 << CHK_ACTORS)) != 0)
		ComputeActorHash();
	if ((categoryMask & (1 << CHK_RNG)) != 0)
		ComputeRngHash();
	if ((categoryMask & (1 << CHK_LINESPEC)) != 0)
		ComputeLineSpecHash();
	LastComputedTic = gametic;
	StoreTicHashSnapshot();
}

// Skip recompute when the cached hashes still describe the current gametic.
// The server may build several snapshot packets per tic (one per remote peer)
// and the client may receive several snapshots before any new world tic runs;
// in both cases the hash inputs cannot have changed.
void Net_ChecksumComputeIfStale()
{
	if (LastComputedTic == gametic)
		return;
	Net_ChecksumComputeAll();
}
}

// Reset all cached hash state. Called from Net_DiagSessionBegin() on process
// start and from the level transition hook so a fresh map does not inherit
// stale tic/hash anchors that would suppress legitimate recomputes.
void Net_ChecksumInit()
{
	memset(CategoryHashes, 0, sizeof(CategoryHashes));
	LastComputedTic = INT_MIN;
	LineSpecRollingHash = 0u;
	LastMismatchReportMS = 0u;
	LastMismatchBundleMS = 0u;
	// Wipe the per-tic ring buffer so a stale bucket from the previous map
	// (whose `gametic` happens to collide with a fresh tic on the new map)
	// can't masquerade as our authoritative hash for that tic.
	for (size_t i = 0u; i < TicHashHistoryDepth; ++i)
	{
		TicHashHistory[i].Tic = INT_MIN;
		memset(TicHashHistory[i].Hashes, 0, sizeof(TicHashHistory[i].Hashes));
	}
	TicHashHistoryCursor = 0u;
}

void Net_ChecksumResetForNewLevel()
{
	Net_ChecksumInit();
}

void Net_ChecksumNoteLineSpec(int lineIndex, int special, bool success)
{
	if (!*net_checksum)
		return;
	LineSpecRollingHash = MixU32(LineSpecRollingHash, uint32_t(lineIndex));
	LineSpecRollingHash = MixU32(LineSpecRollingHash, uint32_t(special));
	LineSpecRollingHash = MixU32(LineSpecRollingHash, success ? 1u : 0u);
}

// Per-tic compute + store. Runs every world tic from G_Ticker so the client's
// per-tic ring buffer is dense enough that ReadAndCompare can always look up
// the server's compute tic. Previously this was throttled by
// `net_checksum_interval` (default 4 tics), but in async netcode a 4-tic
// granularity is too sparse: the server stamps its hash with whatever gametic
// it computed at, which is usually every tic, so a client that only computes
// every fourth tic would miss three out of four comparison opportunities and
// the rest of the time would compare against a tic the server never stamped.
// The interval CVAR is kept for legacy users but the per-tic gate below keeps
// compute cost bounded by ZDoom's single-compute-per-gametic guard inside
// Net_ChecksumComputeIfStale().
void Net_ChecksumTick()
{
	if (!*net_checksum || gamestate != GS_LEVEL)
		return;
	if (LastComputedTic == gametic)
		return;
	Net_ChecksumComputeIfStale();
}

bool Net_ChecksumAppendBytes(uint8_t* output, size_t outputCapacity, size_t& cursor, const void* data, size_t size)
{
	if (cursor + size > outputCapacity)
		return false;
	memcpy(output + cursor, data, size);
	cursor += size;
	return true;
}

// Server-side: append the latest hashes to the snapshot body. The chunk is
// optional and self-describing - clients without checksum support skip it via
// the magic mismatch path in Net_ChecksumReadAndCompare. Only the local
// authority should ever emit this chunk; on a non-authority client the snapshot
// builder is producing data for its own legacy paths and must not lie about
// being the source of truth.
void Net_ChecksumApplyServerChunk(uint8_t* output, size_t outputCapacity, size_t& cursor)
{
	if (!*net_checksum || !I_IsLocalHCDEServiceAuthority())
		return;
	Net_ChecksumComputeIfStale();
	if (!Net_ChecksumAppendBytes(output, outputCapacity, cursor, ChecksumMagic, sizeof(ChecksumMagic)))
		return;
	const uint8_t version = ChecksumVersion;
	const uint8_t count = CHK_COUNT;
	if (!Net_ChecksumAppendBytes(output, outputCapacity, cursor, &version, 1)
		|| !Net_ChecksumAppendBytes(output, outputCapacity, cursor, &count, 1))
		return;
	const uint32_t tic = uint32_t(gametic);
	if (!Net_ChecksumAppendBytes(output, outputCapacity, cursor, &tic, sizeof(tic)))
		return;
	for (uint8_t i = 0u; i < CHK_COUNT; ++i)
	{
		if (!Net_ChecksumAppendBytes(output, outputCapacity, cursor, &CategoryHashes[i], sizeof(CategoryHashes[i])))
			return;
	}
}

// Client-side: parse a checksum chunk emitted by Net_ChecksumApplyServerChunk
// and compare against locally computed hashes. Returns true when the chunk was
// either absent (no magic match), missing entirely, or fully consumed; returns
// false only when the chunk is structurally malformed (wrong version/count) so
// the caller can treat it as a malformed snapshot.
bool Net_ChecksumReadAndCompare(const uint8_t* body, size_t bodyBytes, size_t& cursor, uint32_t serverTic)
{
	if (!*net_checksum || body == nullptr)
		return true;
	// The local authority never receives its own snapshots; if for some reason
	// it does (loopback debug aid), don't compare against itself with stale
	// timing - that would create false-positive mismatches.
	if (I_IsLocalHCDEServiceAuthority())
		return true;
	if (cursor + sizeof(ChecksumMagic) + 2u + sizeof(uint32_t) + CHK_COUNT * sizeof(uint32_t) > bodyBytes)
		return true;
	if (memcmp(body + cursor, ChecksumMagic, sizeof(ChecksumMagic)) != 0)
		return true;

	cursor += sizeof(ChecksumMagic);
	const uint8_t version = body[cursor++];
	const uint8_t count = body[cursor++];
	if (version != ChecksumVersion || count != CHK_COUNT)
		return false;

	uint32_t remoteTic = 0u;
	memcpy(&remoteTic, body + cursor, sizeof(remoteTic));
	cursor += sizeof(remoteTic);

	// Look up our own snapshot of CategoryHashes that we computed at the same
	// gametic the server is reporting. Hashing the *current* gametic would
	// produce false positives whenever client and server are at different
	// tics (i.e. always, in async netcode). If the lookup fails - because
	// we haven't run that tic yet, or it has already aged out of the ring -
	// we silently skip the comparison; "we don't know" is not "they
	// disagree".
	const TicHashBucket* localBucket = FindTicHashBucket(int(remoteTic));
	if (localBucket == nullptr)
	{
		// Drain the remote hashes from the cursor so the snapshot continues
		// to parse cleanly, but don't compare. Limit trace volume - this can
		// fire several times per second under heavy lead and the user already
		// sees prediction telemetry covering the same window.
		cursor += CHK_COUNT * sizeof(uint32_t);
		return true;
	}

	static const char* CategoryNames[CHK_COUNT] = {
		"players", "sectors", "movers", "actors", "rng", "linespec"
	};
	for (uint8_t i = 0u; i < CHK_COUNT; ++i)
	{
		uint32_t remoteHash = 0u;
		memcpy(&remoteHash, body + cursor, sizeof(remoteHash));
		cursor += sizeof(remoteHash);
		// Skip categories this peer has disabled. Do NOT treat a zero hash as
		// "disabled" - an empty player list or an unlucky mix can legitimately
		// produce 0x00000000, and skipping on zero would hide real mismatches.
		if ((*net_checksum_categories & (1 << i)) == 0)
			continue;
		if (remoteHash == localBucket->Hashes[i])
			continue;
		Net_ChecksumReportMismatch(CategoryNames[i], serverTic, remoteHash, localBucket->Hashes[i]);
	}
	return true;
}

// Surface a mismatch and (rarely) trigger a forensic flush. Two cooldowns
// throttle the spam:
//  * a short 250ms "report" cooldown debounces consecutive category lines so
//    the trace file isn't flooded when several categories drift in lock-step.
//  * a 60 second "bundle" cooldown (MismatchBundleCooldownMS) prevents
//    Net_DiagWriteBundle's multi-megabyte file I/O from stalling the playsim
//    thread when transient prediction lead causes a stretch of mismatches.
//    Earlier this cooldown was 5s and we observed it actively contributing to
//    the desync feedback loop - see the explanatory comment on
//    MismatchBundleCooldownMS at the top of this file.
void Net_ChecksumReportMismatch(const char* category, uint32_t tic, uint32_t serverHash, uint32_t localHash)
{
	const uint64_t nowMS = uint64_t(I_msTime());
	const bool emitTrace = LastMismatchReportMS == 0u || nowMS - LastMismatchReportMS >= MismatchReportCooldownMS;
	if (emitTrace)
	{
		LastMismatchReportMS = nowMS;
		DebugTrace::Warningf("net.desync",
			"CHECKSUM_MISMATCH category=%s tic=%u server=0x%08x local=0x%08x gametic=%d clienttic=%d",
			category != nullptr ? category : "?",
			unsigned(tic),
			unsigned(serverHash),
			unsigned(localHash),
			gametic,
			ClientTic);
	}

	if (LastMismatchBundleMS != 0u && nowMS - LastMismatchBundleMS < MismatchBundleCooldownMS)
		return;
	LastMismatchBundleMS = nowMS;
	Net_BlackboxAutoSave("checksum_mismatch");
	FString bundlePath;
	Net_DiagWriteBundle("checksum_mismatch", bundlePath);
}

CCMD(net_checksum_dump)
{
	Net_ChecksumComputeAll();
	Printf(PRINT_HIGH, "HCDE checksums gametic=%d\n", gametic);
	const char* names[CHK_COUNT] = { "players", "sectors", "movers", "actors", "rng", "linespec" };
	for (int i = 0; i < CHK_COUNT; ++i)
		Printf(PRINT_HIGH, "  %s=0x%08x\n", names[i], unsigned(CategoryHashes[i]));
}
