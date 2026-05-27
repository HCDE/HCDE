// HCDE net "black box" packet recorder.
//
// Implements a bounded ring buffer of recently-seen packet payloads plus
// per-tic anchors so a forensic dump can be saved on demand or when the engine
// detects a bad event (prediction fault, checksum mismatch, manual mark). The
// buffer is sized by `net_blackbox_size_mb` (clamped to [4, 256] MiB) and
// gated by `net_blackbox_record`; both CVARs are `CVAR_ARCHIVE` so they
// persist across restarts.
//
// Concurrency: The recording API may be called from the UDP receive path as
// well as the main playsim thread (HCDE service work currently runs on the
// main thread but the lock keeps us correct should that ever change). The
// flush helper takes the lock when iterating the ring so save/load cannot
// observe a partially-written entry.

#include "d_net_blackbox.h"
#include "d_net_diagnostics.h"
#include "doomstat.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "common/engine/debugtrace.h"
#include "i_specialpaths.h"
#include "i_time.h"
#include "cmdlib.h"
#include "d_net.h"

#include <atomic>
#include <mutex>
#include <vector>
#include <cstring>
#include <algorithm>

// Master toggle - 0 disables both recording and auto-save while leaving any
// already-buffered entries in memory, 1 keeps the recorder warm.
CUSTOM_CVAR(Int, net_blackbox_record, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
}

// Soft byte budget for the ring (in MiB). Eviction is lazy - reducing this
// CVAR at runtime starts trimming the oldest entries on the next add.
CUSTOM_CVAR(Int, net_blackbox_size_mb, 32, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 4)
		self = 4;
	else if (self > 256)
		self = 256;
}

namespace
{
constexpr uint32_t BlackboxMagic = 0x58484248u; // 'HBBX' little-endian on disk.
constexpr uint32_t BlackboxVersion = 1u;
constexpr uint64_t AutoSaveCooldownMS = 5000u;

enum EBlackboxEntryType : uint8_t
{
	BBOX_PACKET = 1,
	BBOX_TIC_INDEX = 2,
};

// Disk layout for a black box entry header. Pack to 1 so the on-disk size is
// stable across MSVC/Clang and matches the Python replay tool's struct format
// (`<IBBBBIIiiiQI` = 40 bytes). Adding a new field here MUST also update
// tests/netcode_step12/replay_blackbox.py and bump BlackboxVersion.
#pragma pack(push, 1)
struct FBlackboxEntryHeader
{
	uint32_t Magic = BlackboxMagic;
	uint8_t Type = 0u;
	uint8_t Direction = 0u;
	uint8_t Lane = 0u;
	uint8_t Room = 0u;
	uint32_t Seq = 0u;
	uint32_t Ack = 0u;
	int32_t PeerSlot = -1;
	int32_t Gametic = 0;
	int32_t ClientTic = 0;
	uint64_t TimestampMS = 0u;
	uint32_t PayloadSize = 0u;
};
#pragma pack(pop)
static_assert(sizeof(FBlackboxEntryHeader) == 40u,
	"FBlackboxEntryHeader must be 40 bytes on disk to match replay_blackbox.py");

struct FBlackboxEntry
{
	FBlackboxEntryHeader Header = {};
	std::vector<uint8_t> Payload;
};

std::mutex BlackboxMutex;
std::vector<FBlackboxEntry> BlackboxRing;
size_t BlackboxRingStart = 0u;
size_t BlackboxRingCount = 0u;
size_t BlackboxRingCapacity = 4096u;
size_t BlackboxBytesUsed = 0u;
// Last auto-save wall-clock time; mutated under BlackboxMutex via the lazy
// save path so concurrent fault triggers can't all stamp the same instant.
uint64_t LastAutoSaveMS = 0u;
// Atomic so the fast-path "already initialized?" check on the recording side
// does not race with Net_BlackboxInit() / Shutdown().
std::atomic<bool> BlackboxInitialized{false};

size_t BlackboxBudgetBytes()
{
	return size_t(*net_blackbox_size_mb) * 1024u * 1024u;
}

// Drop the oldest entry. Caller must hold BlackboxMutex.
void BlackboxEvictOldest()
{
	if (BlackboxRingCount == 0u)
		return;
	const size_t index = BlackboxRingStart % BlackboxRingCapacity;
	FBlackboxEntry& entry = BlackboxRing[index];
	BlackboxBytesUsed -= sizeof(FBlackboxEntryHeader) + entry.Payload.size();
	entry.Payload.clear();
	BlackboxRingStart = (BlackboxRingStart + 1u) % BlackboxRingCapacity;
	--BlackboxRingCount;
}

// Append an entry to the ring buffer, evicting older entries to honor the
// configured byte budget AND the slot-count capacity. Caller must hold
// BlackboxMutex. The entry is consumed via std::move regardless of which path
// stores it so payload buffers are not duplicated.
void BlackboxStoreEntry(FBlackboxEntry&& entry)
{
	const size_t entryBytes = sizeof(FBlackboxEntryHeader) + entry.Payload.size();
	// Evict oldest until either the budget fits or the ring is empty. A single
	// entry larger than the entire budget will still be stored (this iteration
	// exits with count == 0) and overwritten by the next add.
	while (BlackboxBytesUsed + entryBytes > BlackboxBudgetBytes() && BlackboxRingCount > 0u)
		BlackboxEvictOldest();

	// Also enforce the slot-count cap; budget eviction already covers the byte
	// limit but the vector is hard-bounded to BlackboxRingCapacity slots.
	while (BlackboxRingCount >= BlackboxRingCapacity && BlackboxRingCount > 0u)
		BlackboxEvictOldest();

	if (BlackboxRing.size() < BlackboxRingCapacity)
	{
		BlackboxRing.push_back(std::move(entry));
	}
	else
	{
		const size_t index = (BlackboxRingStart + BlackboxRingCount) % BlackboxRingCapacity;
		BlackboxRing[index] = std::move(entry);
	}
	++BlackboxRingCount;
	BlackboxBytesUsed += entryBytes;
}
}

void Net_BlackboxInit()
{
	std::lock_guard<std::mutex> lock(BlackboxMutex);
	BlackboxRing.clear();
	BlackboxRingStart = 0u;
	BlackboxRingCount = 0u;
	BlackboxBytesUsed = 0u;
	LastAutoSaveMS = 0u;
	BlackboxInitialized.store(true, std::memory_order_release);
}

void Net_BlackboxShutdown()
{
	std::lock_guard<std::mutex> lock(BlackboxMutex);
	BlackboxRing.clear();
	BlackboxRingStart = 0u;
	BlackboxRingCount = 0u;
	BlackboxBytesUsed = 0u;
	BlackboxInitialized.store(false, std::memory_order_release);
}

// Lazy initialization helper: a single TOCTOU on BlackboxInitialized is fine
// because the actual init takes BlackboxMutex and re-checks the flag, so the
// worst case is two callers entering Net_BlackboxInit() back-to-back; the
// second one runs against an already-cleared ring and is effectively a no-op.
static void Net_BlackboxEnsureInitialized()
{
	if (!BlackboxInitialized.load(std::memory_order_acquire))
		Net_BlackboxInit();
}

void Net_BlackboxRecordPacket(int direction, int peerSlot, uint8_t lane,
	uint32_t seq, uint32_t ack, uint8_t room, const uint8_t* payload, size_t payloadSize)
{
	if (!*net_blackbox_record || payloadSize == 0u || payload == nullptr)
		return;
	Net_BlackboxEnsureInitialized();

	FBlackboxEntry entry = {};
	entry.Header.Type = BBOX_PACKET;
	entry.Header.Direction = uint8_t(direction & 0xff);
	entry.Header.Lane = lane;
	entry.Header.Room = room;
	entry.Header.Seq = seq;
	entry.Header.Ack = ack;
	entry.Header.PeerSlot = peerSlot;
	entry.Header.Gametic = gametic;
	entry.Header.ClientTic = ClientTic;
	entry.Header.TimestampMS = I_msTime();
	entry.Header.PayloadSize = uint32_t(payloadSize);
	entry.Payload.assign(payload, payload + payloadSize);

	std::lock_guard<std::mutex> lock(BlackboxMutex);
	BlackboxStoreEntry(std::move(entry));
}

void Net_BlackboxRecordTicIndex()
{
	if (!*net_blackbox_record)
		return;
	Net_BlackboxEnsureInitialized();

	FBlackboxEntry entry = {};
	entry.Header.Type = BBOX_TIC_INDEX;
	entry.Header.Gametic = gametic;
	entry.Header.ClientTic = ClientTic;
	entry.Header.TimestampMS = I_msTime();

	std::lock_guard<std::mutex> lock(BlackboxMutex);
	BlackboxStoreEntry(std::move(entry));
}

// Flush the ring to a `.bin` file plus a small `.json` sidecar describing
// the file (entry count, byte total, label, session id). Returns false only
// when fopen on the binary file fails (e.g. invalid AppData path); the caller
// is responsible for emitting a "save failed" trace line.
bool Net_BlackboxSave(const char* label, FString& outPath)
{
	const uint64_t nowMS = I_msTime();
	const FString path = FStringf("%s/hcde_blackbox_%08X_%llu.bin",
		M_GetAppDataPath(true).GetChars(),
		unsigned(DebugTrace::GetSessionId()),
		static_cast<unsigned long long>(nowMS));

	FILE* out = fopen(path.GetChars(), "wb");
	if (out == nullptr)
		return false;

	std::lock_guard<std::mutex> lock(BlackboxMutex);
	const size_t ringSize = BlackboxRing.size();
	for (size_t i = 0u; i < BlackboxRingCount && ringSize > 0u; ++i)
	{
		const size_t index = (BlackboxRingStart + i) % ringSize;
		const FBlackboxEntry& entry = BlackboxRing[index];
		fwrite(&entry.Header, sizeof(entry.Header), 1, out);
		if (!entry.Payload.empty())
			fwrite(entry.Payload.data(), 1, entry.Payload.size(), out);
	}
	fclose(out);

	const FString manifestPath = FStringf("%s.json", path.GetChars());
	FILE* manifest = fopen(manifestPath.GetChars(), "w");
	if (manifest != nullptr)
	{
		fprintf(manifest,
			"{\"version\":%u,\"session\":\"%08X\",\"label\":\"%s\",\"entries\":%zu,\"bytes\":%zu}\n",
			unsigned(BlackboxVersion),
			unsigned(DebugTrace::GetSessionId()),
			label != nullptr ? label : "save",
			BlackboxRingCount,
			BlackboxBytesUsed);
		fclose(manifest);
	}

	outPath = path;
	DebugTrace::Markf("net.session", "BLACKBOX_SAVE path=%s entries=%zu bytes=%zu label=%s",
		path.GetChars(), BlackboxRingCount, BlackboxBytesUsed, label != nullptr ? label : "save");
	return true;
}

void Net_BlackboxAutoSave(const char* trigger)
{
	if (!*net_blackbox_record)
		return;
	// Test-and-set the cooldown gate under the mutex so concurrent triggers
	// (e.g. simultaneous prediction faults across threads) can't all stamp the
	// same instant and stomp on each other. Net_BlackboxSave acquires the same
	// mutex below, so we release it before re-locking.
	const uint64_t nowMS = I_msTime();
	{
		std::lock_guard<std::mutex> lock(BlackboxMutex);
		if (LastAutoSaveMS != 0u && nowMS - LastAutoSaveMS < AutoSaveCooldownMS)
			return;
		LastAutoSaveMS = nowMS;
	}
	FString path;
	if (Net_BlackboxSave(trigger, path))
	{
		DebugTrace::Markf("net.session", "BLACKBOX_AUTO trigger=%s path=%s",
			trigger != nullptr ? trigger : "auto", path.GetChars());
	}
}

void Net_BlackboxClear()
{
	std::lock_guard<std::mutex> lock(BlackboxMutex);
	BlackboxRing.clear();
	BlackboxRingStart = 0u;
	BlackboxRingCount = 0u;
	BlackboxBytesUsed = 0u;
}

// `net_blackbox_save [label]` - flush the ring buffer to disk now.
CCMD(net_blackbox_save)
{
	const char* label = (argv.argc() > 1) ? argv[1] : "manual";
	FString path;
	if (Net_BlackboxSave(label, path))
		Printf(PRINT_HIGH, "HCDE black box saved: %s\n", path.GetChars());
	else
		Printf(PRINT_HIGH, "HCDE black box save failed.\n");
}

// `net_blackbox_clear` - drop everything currently buffered (does NOT delete
// any saved .bin/.json files, just empties the in-memory ring).
CCMD(net_blackbox_clear)
{
	Net_BlackboxClear();
	Printf(PRINT_HIGH, "HCDE black box cleared.\n");
}

// `net_blackbox_record [0|1]` - inspect or toggle the master CVAR. CCMDs and
// CVARs share the same name space, but the explicit toggle here also prints
// the current size limit for convenience.
CCMD(net_blackbox_record)
{
	if (argv.argc() > 1)
		net_blackbox_record = !!atoi(argv[1]);
	Printf(PRINT_HIGH, "net_blackbox_record=%d size_mb=%d\n", int(*net_blackbox_record), int(*net_blackbox_size_mb));
}
