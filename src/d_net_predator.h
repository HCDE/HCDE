#pragma once

// HCDE Predator Economy game-mode scaffold.
//
// Roadmap board item: #12 ("Predator Economy Game Mode"). This is Phase 1
// scaffold-only -- mode CVARs, a state enum, a tick stub, and a status
// CCMD. Snapshot replication, buy commands, role-based behaviour, and the
// ZScript predator pawn are explicit follow-ups, in that order.
//
// Hard server-authority rules from `docs/HCDE_PREDATOR_AUDIT.md`:
//
//   - Currency lives on the server. Clients only see replicated fields.
//   - Buy actions flow as commands client -> server, validated by the
//     server, then broadcast as authority events.
//   - Predator role is assigned by the server using the named RNG
//     `pr_predator`. Clients cannot self-promote.
//   - Round timer is server-side only; clients see a replicated countdown.
//   - Cheat opcodes are scoped/filtered when this mode is active.

#include "doomdef.h"

#include <cstdint>

constexpr uint8_t HCDEPredatorSnapshotV1Version = 1u;
constexpr uint64_t HCDELiveCapPredatorSnapshotV1 = 1ull << 5;

enum class EHCDEPredatorState : uint8_t
{
	Disabled,    // Mode is not active on this server.
	Waiting,     // Pre-round; waiting for enough players.
	Buy,         // Short buy phase before the hunt.
	Hunt,        // Active round.
	End,         // Round ended; scoring + currency awards.
};

enum class EHCDEPredatorBuyItem : uint8_t
{
	None,
	Armor,
	Ammo,
	Medkit,
};

enum class EHCDEPredatorBuyResultCode : uint8_t
{
	Granted,
	RejectedModeDisabled,
	RejectedNotAuthority,
	RejectedBadPlayer,
	RejectedWrongPhase,
	RejectedBadItem,
	RejectedInsufficientFunds,
};

const char* HCDEPredatorStateName(EHCDEPredatorState state);
const char* HCDEPredatorBuyItemName(EHCDEPredatorBuyItem item);
const char* HCDEPredatorBuyResultName(EHCDEPredatorBuyResultCode result);

struct FHCDEPredatorBuyRequest
{
	uint8_t Version = 1u;
	int32_t PlayerNum = -1;
	EHCDEPredatorBuyItem Item = EHCDEPredatorBuyItem::None;
	int32_t ClientRequestId = 0;
};

struct FHCDEPredatorBuyResult
{
	uint8_t Version = 1u;
	int32_t PlayerNum = -1;
	EHCDEPredatorBuyItem Item = EHCDEPredatorBuyItem::None;
	EHCDEPredatorBuyResultCode Code = EHCDEPredatorBuyResultCode::RejectedBadItem;
	int32_t Cost = 0;
	int32_t BalanceAfter = 0;
	int32_t ClientRequestId = 0;
	int32_t AuthoritySequence = 0;
};


// Phase 1 director. Today this only steps a deterministic state-machine
// cycle on a fake timer for diagnostics; no playsim or snapshot behaviour
// hangs off it yet.
struct FHCDEPredatorRoundDirector
{
	EHCDEPredatorState State = EHCDEPredatorState::Disabled;
	int    RoundIndex = 0;
	int    TicksInState = 0;
	int    TicksRemaining = 0;          // Server-authored countdown for client UI.

	// Server-side bookkeeping. Clients only receive these through snapshots;
	// local client code must not mutate them.
	int    PredatorPlayerNum = -1;     // -1 means none assigned yet.
	int    StartingCurrency = 0;
	int    LastReportedTick = 0;
	int    BuyResultSequence = 0;
	FHCDEPredatorBuyResult LastBuyResult;
	int    PlayerCurrency[MAXPLAYERS] = {};
	uint8_t PlayerCurrencyValid[MAXPLAYERS] = {};
};

// Phase 2 data contract. This is intentionally round-state only; per-player
// currency and buy result events are separate follow-ups so V1 can land
// without changing scoring or inventory behavior.
struct FHCDEPredatorSnapshotV1
{
	uint8_t Version = HCDEPredatorSnapshotV1Version;
	EHCDEPredatorState State = EHCDEPredatorState::Disabled;
	int32_t RoundIndex = 0;
	int32_t TicksInState = 0;
	int32_t TicksRemaining = 0;
	int32_t PredatorPlayerNum = -1;
	int32_t StartingCurrency = 0;
	int32_t PlayerCurrency[MAXPLAYERS] = {};
	uint8_t PlayerCurrencyValid[MAXPLAYERS] = {};
};

// Returns the singleton server-side director. On clients this is also
// safe to call but the director simply stays Disabled because the server
// owns the state.
FHCDEPredatorRoundDirector& HCDEPredatorRoundDirector();

// Per-tic step. The eventual implementation will:
//   1. Move between Waiting/Buy/Hunt/End on configured durations.
//   2. Use `pr_predator` (named FRandom) to pick the predator role.
//   3. Apply currency awards on round transitions.
//   4. Emit authority events for clients.
// Today it just bumps `TicksInState` so the status CCMD can show motion.
void HCDEPredatorTick();

// True iff the mode is active and the local instance is the authority.
// Future buy/role/round code gates on this so a client never mutates state.
bool HCDEPredatorShouldDriveAuthority();

// Build/apply helpers for the future snapshot stream integration. Building is
// server-authority only; applying only updates the local mirror fields.
bool HCDEPredatorBuildSnapshotV1(FHCDEPredatorSnapshotV1& out);
void HCDEPredatorApplySnapshotV1(const FHCDEPredatorSnapshotV1& snapshot);

int HCDEPredatorGetCurrency(int playernum);
bool HCDEPredatorServerSetCurrency(int playernum, int amount);
bool HCDEPredatorServerAddCurrency(int playernum, int delta);
bool HCDEPredatorServerSubmitBuyRequest(const FHCDEPredatorBuyRequest& request, FHCDEPredatorBuyResult& result);
int HCDEPredatorServerSelectPredatorRole();
