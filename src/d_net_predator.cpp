// HCDE Predator Economy game-mode scaffold -- see d_net_predator.h.
//
// Phase 1: scaffold and diagnostics only. No snapshot replication, no
// buy command opcode, no role-based gameplay. This file gives the
// eventual implementation a stable home so subsequent changes are about
// behaviour rather than file layout.

#include "d_net_predator.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "common/engine/i_net.h"
#include "d_net.h"
#include "d_player.h"
#include "d_protocol.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_random.h"
#include "printf.h"
#include "templates.h"
#include "vm.h"
#include "actor.h"

#include <cstring>

namespace
{
FHCDEPredatorRoundDirector PredatorDirector;

// Named RNG -- mirrors how `pr_invasion` is set up. Selection of the
// predator role and any future round-roll logic must run through this
// instance so saves/demos serialize the RNG state deterministically.
FRandom pr_predator("Predator");

PClass* HCDEPredatorPawnClass()
{
	static PClass* cls = nullptr;
	static bool lookedUp = false;
	if (!lookedUp)
	{
		cls = PClass::FindClass("HCDEPredatorPawn");
		lookedUp = true;
	}
	return cls;
}

bool HCDEPredatorPawnSupportsMirror(AActor* mo)
{
	PClass* cls = HCDEPredatorPawnClass();
	return mo != nullptr && cls != nullptr && mo->GetClass()->IsDescendantOf(cls);
}

void HCDEPredatorMirrorPawnState(int playernum)
{
	if (playernum < 0 || playernum >= MAXPLAYERS)
		return;

	AActor* mo = players[playernum].mo;
	if (!HCDEPredatorPawnSupportsMirror(mo))
		return;

	const bool isPredator = PredatorDirector.PredatorPlayerNum == playernum;
	const int currency = HCDEPredatorGetCurrency(playernum);

	{
		IFVIRTUALPTRNAME(mo, "HCDEPredatorPawn", HCDE_SetPredatorRole)
		{
			VMValue params[2] = { mo, isPredator };
			VMCall(func, params, 2, nullptr, 0);
		}
	}

	{
		IFVIRTUALPTRNAME(mo, "HCDEPredatorPawn", HCDE_SetPredatorCurrency)
		{
			VMValue params[2] = { mo, currency };
			VMCall(func, params, 2, nullptr, 0);
		}
	}

	{
		IFVIRTUALPTRNAME(mo, "HCDEPredatorPawn", HCDE_SetPredatorRoundState)
		{
			VMValue params[2] = { mo, int(PredatorDirector.State) };
			VMCall(func, params, 2, nullptr, 0);
		}
	}

	const FHCDEPredatorBuyResult& result = PredatorDirector.LastBuyResult;
	if (result.AuthoritySequence != 0 && result.PlayerNum == playernum)
	{
		{
			IFVIRTUALPTRNAME(mo, "HCDEPredatorPawn", HCDE_SetPredatorBuyResult)
			{
				VMValue params[7] =
				{
					mo,
					int(result.Item),
					int(result.Code),
					result.Cost,
					result.BalanceAfter,
					result.ClientRequestId,
					result.AuthoritySequence
				};
				VMCall(func, params, 7, nullptr, 0);
			}
		}
	}
}

void HCDEPredatorMirrorAllPawns()
{
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			HCDEPredatorMirrorPawnState(i);
		}
	}
}
}

// CVAR surface. All default-off / conservative values; the audit explicitly
// requires that mode activation is server-driven.
CVAR(Bool, sv_predator_enable, false, CVAR_ARCHIVE | CVAR_SERVERINFO)
CVAR(Bool, sv_predator_allow_cheats, false, CVAR_ARCHIVE | CVAR_SERVERINFO)
CUSTOM_CVAR(Int, sv_predator_round_seconds, 180, CVAR_ARCHIVE | CVAR_SERVERINFO)
{
	if (self < 30) self = 30;
	if (self > 1800) self = 1800;
}
CUSTOM_CVAR(Int, sv_predator_buy_seconds, 20, CVAR_ARCHIVE | CVAR_SERVERINFO)
{
	if (self < 5) self = 5;
	if (self > 120) self = 120;
}
CUSTOM_CVAR(Int, sv_predator_starting_currency, 800, CVAR_ARCHIVE | CVAR_SERVERINFO)
{
	if (self < 0) self = 0;
	if (self > 100000) self = 100000;
}

const char* HCDEPredatorStateName(EHCDEPredatorState state)
{
	switch (state)
	{
	case EHCDEPredatorState::Disabled: return "disabled";
	case EHCDEPredatorState::Waiting:  return "waiting";
	case EHCDEPredatorState::Buy:      return "buy";
	case EHCDEPredatorState::Hunt:     return "hunt";
	case EHCDEPredatorState::End:      return "end";
	default: return "unknown";
	}
}

const char* HCDEPredatorBuyItemName(EHCDEPredatorBuyItem item)
{
	switch (item)
	{
	case EHCDEPredatorBuyItem::None:   return "none";
	case EHCDEPredatorBuyItem::Armor:  return "armor";
	case EHCDEPredatorBuyItem::Ammo:   return "ammo";
	case EHCDEPredatorBuyItem::Medkit: return "medkit";
	default: return "unknown";
	}
}

const char* HCDEPredatorBuyResultName(EHCDEPredatorBuyResultCode result)
{
	switch (result)
	{
	case EHCDEPredatorBuyResultCode::Granted:                   return "granted";
	case EHCDEPredatorBuyResultCode::RejectedModeDisabled:      return "rejected-mode-disabled";
	case EHCDEPredatorBuyResultCode::RejectedNotAuthority:      return "rejected-not-authority";
	case EHCDEPredatorBuyResultCode::RejectedBadPlayer:         return "rejected-bad-player";
	case EHCDEPredatorBuyResultCode::RejectedWrongPhase:        return "rejected-wrong-phase";
	case EHCDEPredatorBuyResultCode::RejectedBadItem:           return "rejected-bad-item";
	case EHCDEPredatorBuyResultCode::RejectedInsufficientFunds: return "rejected-insufficient-funds";
	default: return "unknown";
	}
}

FHCDEPredatorRoundDirector& HCDEPredatorRoundDirector()
{
	return PredatorDirector;
}

static bool HCDEPredatorValidPlayer(int playernum)
{
	return playernum >= 0 && playernum < MAXPLAYERS;
}

bool HCDEPredatorShouldDriveAuthority()
{
	if (!*sv_predator_enable)
		return false;
	if (!I_IsLocalHCDEServiceAuthority())
		return false;
	return true;
}

static void HCDEPredatorResetCurrencyToStarting()
{
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		PredatorDirector.PlayerCurrencyValid[i] = playeringame[i] ? 1u : 0u;
		PredatorDirector.PlayerCurrency[i] = playeringame[i] ? max<int>(*sv_predator_starting_currency, 0) : 0;
	}
}

static int HCDEPredatorStateDurationTics(EHCDEPredatorState state)
{
	switch (state)
	{
	case EHCDEPredatorState::Waiting: return TICRATE * 5;
	case EHCDEPredatorState::Buy:     return max<int>(*sv_predator_buy_seconds, 1) * TICRATE;
	case EHCDEPredatorState::Hunt:    return max<int>(*sv_predator_round_seconds, 1) * TICRATE;
	case EHCDEPredatorState::End:     return TICRATE * 5;
	default: return 0;
	}
}

static void HCDEPredatorRefreshCountdown()
{
	const int duration = HCDEPredatorStateDurationTics(PredatorDirector.State);
	PredatorDirector.TicksRemaining = duration > 0 ? max<int>(duration - PredatorDirector.TicksInState, 0) : 0;
}

void HCDEPredatorTick()
{
	if (!HCDEPredatorShouldDriveAuthority())
	{
		PredatorDirector.State = EHCDEPredatorState::Disabled;
		PredatorDirector.TicksInState = 0;
		PredatorDirector.TicksRemaining = 0;
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			PredatorDirector.PlayerCurrency[i] = 0;
			PredatorDirector.PlayerCurrencyValid[i] = 0u;
		}
		return;
	}

	// Phase 1 stub: never produce real authority events. Just walk a
	// deterministic state cycle so `predator_status` shows non-zero
	// values when the mode is enabled. Phase 2 replaces this body with
	// real buy/hunt/end transitions and snapshot writes.
	if (PredatorDirector.State == EHCDEPredatorState::Disabled)
	{
		PredatorDirector.State = EHCDEPredatorState::Waiting;
		PredatorDirector.TicksInState = 0;
		PredatorDirector.RoundIndex = 0;
		PredatorDirector.StartingCurrency = max<int>(*sv_predator_starting_currency, 0);
		HCDEPredatorResetCurrencyToStarting();
	}

	PredatorDirector.TicksInState++;

	// Touch the named RNG once per round transition so its state evolves
	// alongside the rest of the playsim. Real selection logic lives in
	// Phase 4.
	const int buyTics = max<int>(*sv_predator_buy_seconds, 1) * TICRATE;
	const int huntTics = max<int>(*sv_predator_round_seconds, 1) * TICRATE;
	const int endTics = 5 * TICRATE;

	switch (PredatorDirector.State)
	{
	case EHCDEPredatorState::Waiting:
		if (PredatorDirector.TicksInState >= TICRATE * 5)
		{
			PredatorDirector.State = EHCDEPredatorState::Buy;
			PredatorDirector.TicksInState = 0;
			PredatorDirector.PredatorPlayerNum = -1;
		}
		break;
	case EHCDEPredatorState::Buy:
		if (PredatorDirector.TicksInState >= buyTics)
		{
			PredatorDirector.State = EHCDEPredatorState::Hunt;
			PredatorDirector.TicksInState = 0;
			HCDEPredatorServerSelectPredatorRole();
		}
		break;
	case EHCDEPredatorState::Hunt:
		if (PredatorDirector.TicksInState >= huntTics)
		{
			PredatorDirector.State = EHCDEPredatorState::End;
			PredatorDirector.TicksInState = 0;
		}
		break;
	case EHCDEPredatorState::End:
		if (PredatorDirector.TicksInState >= endTics)
		{
			PredatorDirector.State = EHCDEPredatorState::Waiting;
			PredatorDirector.TicksInState = 0;
			PredatorDirector.RoundIndex++;
		}
		break;
	default:
		PredatorDirector.State = EHCDEPredatorState::Waiting;
		PredatorDirector.TicksInState = 0;
		break;
	}

	HCDEPredatorRefreshCountdown();
	HCDEPredatorMirrorAllPawns();
}

bool HCDEPredatorBuildSnapshotV1(FHCDEPredatorSnapshotV1& out)
{
	if (!HCDEPredatorShouldDriveAuthority())
		return false;

	HCDEPredatorRefreshCountdown();
	out.Version = HCDEPredatorSnapshotV1Version;
	out.State = PredatorDirector.State;
	out.RoundIndex = PredatorDirector.RoundIndex;
	out.TicksInState = PredatorDirector.TicksInState;
	out.TicksRemaining = PredatorDirector.TicksRemaining;
	out.PredatorPlayerNum = PredatorDirector.PredatorPlayerNum;
	out.StartingCurrency = PredatorDirector.StartingCurrency;
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		out.PlayerCurrency[i] = PredatorDirector.PlayerCurrency[i];
		out.PlayerCurrencyValid[i] = PredatorDirector.PlayerCurrencyValid[i];
	}
	return true;
}

void HCDEPredatorApplySnapshotV1(const FHCDEPredatorSnapshotV1& snapshot)
{
	if (snapshot.Version != HCDEPredatorSnapshotV1Version)
		return;
	if (HCDEPredatorShouldDriveAuthority())
		return;

	PredatorDirector.State = snapshot.State;
	PredatorDirector.RoundIndex = snapshot.RoundIndex;
	PredatorDirector.TicksInState = snapshot.TicksInState;
	PredatorDirector.TicksRemaining = snapshot.TicksRemaining;
	PredatorDirector.PredatorPlayerNum = snapshot.PredatorPlayerNum;
	PredatorDirector.StartingCurrency = snapshot.StartingCurrency;
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		PredatorDirector.PlayerCurrency[i] = snapshot.PlayerCurrency[i];
		PredatorDirector.PlayerCurrencyValid[i] = snapshot.PlayerCurrencyValid[i];
	}
	HCDEPredatorMirrorAllPawns();
}

int HCDEPredatorGetCurrency(int playernum)
{
	if (!HCDEPredatorValidPlayer(playernum) || !PredatorDirector.PlayerCurrencyValid[playernum])
		return 0;

	return PredatorDirector.PlayerCurrency[playernum];
}

bool HCDEPredatorServerSetCurrency(int playernum, int amount)
{
	if (!HCDEPredatorShouldDriveAuthority() || !HCDEPredatorValidPlayer(playernum))
		return false;
	if (!playeringame[playernum])
		return false;

	PredatorDirector.PlayerCurrency[playernum] = clamp(amount, 0, 1000000);
	PredatorDirector.PlayerCurrencyValid[playernum] = 1u;
	return true;
}

bool HCDEPredatorServerAddCurrency(int playernum, int delta)
{
	if (!HCDEPredatorShouldDriveAuthority() || !HCDEPredatorValidPlayer(playernum))
		return false;

	const int current = HCDEPredatorGetCurrency(playernum);
	return HCDEPredatorServerSetCurrency(playernum, current + delta);
}

static int HCDEPredatorBuyCost(EHCDEPredatorBuyItem item)
{
	switch (item)
	{
	case EHCDEPredatorBuyItem::Armor:  return 300;
	case EHCDEPredatorBuyItem::Ammo:   return 150;
	case EHCDEPredatorBuyItem::Medkit: return 100;
	default: return 0;
	}
}

static void HCDEPredatorPublishBuyResult(FHCDEPredatorBuyResult& result)
{
	result.AuthoritySequence = ++PredatorDirector.BuyResultSequence;
	PredatorDirector.LastBuyResult = result;
	HCDEPredatorMirrorPawnState(result.PlayerNum);
}

bool HCDEPredatorServerSubmitBuyRequest(const FHCDEPredatorBuyRequest& request, FHCDEPredatorBuyResult& result)
{
	result = {};
	result.PlayerNum = request.PlayerNum;
	result.Item = request.Item;
	result.ClientRequestId = request.ClientRequestId;

	if (!*sv_predator_enable)
	{
		result.Code = EHCDEPredatorBuyResultCode::RejectedModeDisabled;
		HCDEPredatorPublishBuyResult(result);
		return false;
	}
	if (!HCDEPredatorShouldDriveAuthority())
	{
		result.Code = EHCDEPredatorBuyResultCode::RejectedNotAuthority;
		HCDEPredatorPublishBuyResult(result);
		return false;
	}
	if (!HCDEPredatorValidPlayer(request.PlayerNum) || !playeringame[request.PlayerNum])
	{
		result.Code = EHCDEPredatorBuyResultCode::RejectedBadPlayer;
		HCDEPredatorPublishBuyResult(result);
		return false;
	}
	if (PredatorDirector.State != EHCDEPredatorState::Buy)
	{
		result.Code = EHCDEPredatorBuyResultCode::RejectedWrongPhase;
		result.BalanceAfter = HCDEPredatorGetCurrency(request.PlayerNum);
		HCDEPredatorPublishBuyResult(result);
		return false;
	}

	const int cost = HCDEPredatorBuyCost(request.Item);
	result.Cost = cost;
	if (cost <= 0)
	{
		result.Code = EHCDEPredatorBuyResultCode::RejectedBadItem;
		result.BalanceAfter = HCDEPredatorGetCurrency(request.PlayerNum);
		HCDEPredatorPublishBuyResult(result);
		return false;
	}

	const int balance = HCDEPredatorGetCurrency(request.PlayerNum);
	if (balance < cost)
	{
		result.Code = EHCDEPredatorBuyResultCode::RejectedInsufficientFunds;
		result.BalanceAfter = balance;
		HCDEPredatorPublishBuyResult(result);
		return false;
	}

	HCDEPredatorServerSetCurrency(request.PlayerNum, balance - cost);
	result.Code = EHCDEPredatorBuyResultCode::Granted;
	result.BalanceAfter = HCDEPredatorGetCurrency(request.PlayerNum);
	HCDEPredatorPublishBuyResult(result);
	return true;
}

int HCDEPredatorServerSelectPredatorRole()
{
	if (!HCDEPredatorShouldDriveAuthority())
		return -1;

	int candidates[MAXPLAYERS] = {};
	int count = 0;
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			candidates[count++] = i;
		}
	}

	if (count <= 0)
	{
		PredatorDirector.PredatorPlayerNum = -1;
		return -1;
	}

	const int selected = candidates[pr_predator() % count];
	PredatorDirector.PredatorPlayerNum = selected;
	return selected;
}

bool HCDEPredatorShouldRejectCheatOpcode(uint8_t opcode)
{
	if (!*sv_predator_enable || *sv_predator_allow_cheats)
		return false;

	switch (opcode)
	{
	case DEM_GENERICCHEAT:
	case DEM_GIVECHEAT:
	case DEM_TAKECHEAT:
	case DEM_SETINV:
	case DEM_WARPCHEAT:
		++PredatorDirector.CheatRejects;
		return true;
	default:
		return false;
	}
}

bool HCDEPredator_HandleNetEvent(int playerNum, const char* name, int arg0, int arg1, int arg2)
{
	(void)arg2;
	if (name == nullptr)
		return false;
	if (std::strcmp(name, HCDEPredatorBuyNetEventName()) != 0)
		return false;

	// Arrival path:
	//   - The DEM_NETEVENT lane delivers this to every client during command
	//     playback. Only the authority instance actually validates and mutates
	//     state. Non-authority clients ignore the event (the result will reach
	//     them through the existing snapshot stream).
	if (!HCDEPredatorShouldDriveAuthority())
		return true;

	FHCDEPredatorBuyRequest request;
	request.PlayerNum = playerNum;
	request.Item = static_cast<EHCDEPredatorBuyItem>(arg0);
	request.ClientRequestId = arg1;

	FHCDEPredatorBuyResult result;
	(void)HCDEPredatorServerSubmitBuyRequest(request, result);
	return true;
}

bool HCDEPredator_SendBuyRequest(EHCDEPredatorBuyItem item, int clientRequestId)
{
	// The DEM_NETEVENT lane already accepts this format: name + argcount(3) +
	// 3x int32 + manual-flag. We mirror that wire layout exactly so
	// `EventManager::SendNetworkEvent` and `HCDEPredator_HandleNetEvent` see
	// the same byte sequence in both directions.
	Net_WriteInt8(DEM_NETEVENT);
	Net_WriteString(HCDEPredatorBuyNetEventName());
	Net_WriteInt8(3);
	Net_WriteInt32(static_cast<int32_t>(item));
	Net_WriteInt32(clientRequestId);
	Net_WriteInt32(0);
	Net_WriteInt8(0); // manual flag (always 0 for engine-issued buys)
	return true;
}

// Diagnostic CCMDs. Phase 3 wiring (DEM_NETEVENT-routed buy command from
// client to server) is a follow-up; for now these CCMDs let an operator
// exercise the server-side path locally (e.g. on a listen server) so the
// validation logic, currency math, and result publication can be tested
// end-to-end before the network handshake lands.
//
// Both CCMDs gate on `HCDEPredatorShouldDriveAuthority()` so a pure client
// instance cannot mutate state.

CCMD(predator_buy)
{
	// Client-side buy command. Anyone can invoke this; the server is the
	// authority. The request rides DEM_NETEVENT and is validated server-side
	// inside HCDEPredator_HandleNetEvent.
	if (argv.argc() < 2)
	{
		Printf(PRINT_HIGH, "usage: predator_buy <armor|ammo|medkit>\n");
		return;
	}
	const FString itemName = argv[1];
	EHCDEPredatorBuyItem item = EHCDEPredatorBuyItem::None;
	if (itemName.CompareNoCase("armor") == 0)        item = EHCDEPredatorBuyItem::Armor;
	else if (itemName.CompareNoCase("ammo") == 0)    item = EHCDEPredatorBuyItem::Ammo;
	else if (itemName.CompareNoCase("medkit") == 0)  item = EHCDEPredatorBuyItem::Medkit;
	else
	{
		Printf(PRINT_HIGH, "predator_buy: unknown item '%s' (use armor/ammo/medkit)\n", itemName.GetChars());
		return;
	}

	const int clientRequestId = ++PredatorDirector.BuyResultSequence;
	HCDEPredator_SendBuyRequest(item, clientRequestId);
	Printf(PRINT_HIGH, "predator_buy: dispatched DEM_NETEVENT '%s' item=%s requestId=%d (waiting for authority result)\n",
		HCDEPredatorBuyNetEventName(), HCDEPredatorBuyItemName(item), clientRequestId);
}

CCMD(predator_buy_test)
{
	if (!HCDEPredatorShouldDriveAuthority())
	{
		Printf(PRINT_HIGH, "predator_buy_test: not authority (or sv_predator_enable=0); ignoring.\n");
		return;
	}
	if (argv.argc() < 3)
	{
		Printf(PRINT_HIGH, "usage: predator_buy_test <playernum> <armor|ammo|medkit>\n");
		return;
	}
	const int playernum = atoi(argv[1]);
	const FString itemName = argv[2];
	EHCDEPredatorBuyItem item = EHCDEPredatorBuyItem::None;
	if (itemName.CompareNoCase("armor") == 0)        item = EHCDEPredatorBuyItem::Armor;
	else if (itemName.CompareNoCase("ammo") == 0)    item = EHCDEPredatorBuyItem::Ammo;
	else if (itemName.CompareNoCase("medkit") == 0)  item = EHCDEPredatorBuyItem::Medkit;
	else
	{
		Printf(PRINT_HIGH, "predator_buy_test: unknown item '%s' (use armor/ammo/medkit)\n", itemName.GetChars());
		return;
	}

	FHCDEPredatorBuyRequest request;
	request.PlayerNum = playernum;
	request.Item = item;
	request.ClientRequestId = ++PredatorDirector.BuyResultSequence; // synthetic id for the test path

	FHCDEPredatorBuyResult result;
	const bool granted = HCDEPredatorServerSubmitBuyRequest(request, result);
	Printf(PRINT_HIGH, "predator_buy_test: player=%d item=%s granted=%d code=%s cost=%d balance=%d\n",
		result.PlayerNum, HCDEPredatorBuyItemName(result.Item), granted ? 1 : 0,
		HCDEPredatorBuyResultName(result.Code), result.Cost, result.BalanceAfter);
}

CCMD(predator_admin_grant)
{
	if (!HCDEPredatorShouldDriveAuthority())
	{
		Printf(PRINT_HIGH, "predator_admin_grant: not authority; ignoring.\n");
		return;
	}
	if (argv.argc() < 3)
	{
		Printf(PRINT_HIGH, "usage: predator_admin_grant <playernum> <delta>\n");
		return;
	}
	const int playernum = atoi(argv[1]);
	const int delta = atoi(argv[2]);
	const bool ok = HCDEPredatorServerAddCurrency(playernum, delta);
	Printf(PRINT_HIGH, "predator_admin_grant: player=%d delta=%d ok=%d balance=%d\n",
		playernum, delta, ok ? 1 : 0, HCDEPredatorGetCurrency(playernum));
}

CCMD(predator_status)
{
	const FHCDEPredatorRoundDirector& dir = HCDEPredatorRoundDirector();
	Printf(PRINT_HIGH, "\n=== HCDE Predator Economy ===\n");
	Printf(PRINT_HIGH, "  sv_predator_enable             = %s\n", *sv_predator_enable ? "on" : "off");
	Printf(PRINT_HIGH, "  sv_predator_allow_cheats       = %s\n", *sv_predator_allow_cheats ? "on" : "off");
	Printf(PRINT_HIGH, "  sv_predator_round_seconds      = %d\n", *sv_predator_round_seconds);
	Printf(PRINT_HIGH, "  sv_predator_buy_seconds        = %d\n", *sv_predator_buy_seconds);
	Printf(PRINT_HIGH, "  sv_predator_starting_currency  = %d\n", *sv_predator_starting_currency);
	Printf(PRINT_HIGH, "  authority-drives               = %s\n",
		HCDEPredatorShouldDriveAuthority() ? "yes" : "no");
	Printf(PRINT_HIGH, "  state                          = %s\n", HCDEPredatorStateName(dir.State));
	Printf(PRINT_HIGH, "  round-index                    = %d\n", dir.RoundIndex);
	Printf(PRINT_HIGH, "  tics-in-state                  = %d\n", dir.TicksInState);
	Printf(PRINT_HIGH, "  tics-remaining                 = %d\n", dir.TicksRemaining);
	Printf(PRINT_HIGH, "  predator-playernum             = %d\n", dir.PredatorPlayerNum);
	Printf(PRINT_HIGH, "  starting-currency              = %d\n", dir.StartingCurrency);
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (dir.PlayerCurrencyValid[i])
		{
			Printf(PRINT_HIGH, "  currency[%d]                    = %d\n", i, dir.PlayerCurrency[i]);
		}
	}
	Printf(PRINT_HIGH, "  last-buy-result-seq             = %d\n", dir.LastBuyResult.AuthoritySequence);
	Printf(PRINT_HIGH, "  last-buy-result                 = p%d %s %s cost=%d balance=%d request=%d\n",
		dir.LastBuyResult.PlayerNum,
		HCDEPredatorBuyItemName(dir.LastBuyResult.Item),
		HCDEPredatorBuyResultName(dir.LastBuyResult.Code),
		dir.LastBuyResult.Cost,
		dir.LastBuyResult.BalanceAfter,
		dir.LastBuyResult.ClientRequestId);
	Printf(PRINT_HIGH, "  cheat-opcode-rejects            = %d\n", dir.CheatRejects);
	Printf(PRINT_HIGH, "  snapshot-v1-capability         = 0x%016llx\n", (unsigned long long)HCDELiveCapPredatorSnapshotV1);
	Printf(PRINT_HIGH, "  phase                          = snapshot contract only; no buy/currency/role gameplay yet.\n");
	Printf(PRINT_HIGH, "  see docs/HCDE_PREDATOR_AUDIT.md for boundaries.\n");
	Printf(PRINT_HIGH, "=============================\n");
}
