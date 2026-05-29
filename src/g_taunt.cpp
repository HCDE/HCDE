// HCDE skin taunt mechanic.
//
// Roadmap board item: #21 ("need doom skins taunt sound mechanic").
//
// Wire format (DEM_TAUNT):
//
//   Byte:   variant index (0 = canonical `*taunt`, otherwise unused for now;
//                          server forwards transparently, clients pick the
//                          string variant first and only fall through to the
//                          index if the string is empty).
//   String: variant name, NUL-terminated (empty for canonical `*taunt`).
//
// The opcode is on the cosmetic-only allowlist in `d_net.cpp`. It does not
// touch playsim state, scores, or the prediction window. A receiving client
// just plays `*taunt[-name]` on the originating pawn's CHAN_VOICE; if the
// pawn or the sound is unknown the call is a no-op.
//
// Anti-spam: a small cooldown prevents the CCMD from being mashed into the
// audio mixer (and into the chat lane).

#include "c_dispatch.h"
#include "c_cvars.h"
#include "doomstat.h"
#include "d_player.h"
#include "actor.h"
#include "printf.h"
#include "s_sound.h"
#include "s_soundinternal.h"
#include "i_time.h"
#include "d_net.h"
#include "d_protocol.h"

namespace
{
constexpr uint64_t kTauntCooldownMS = 1500u;	// ~1.5s; same ballpark as chat throttling.
uint64_t gTauntCooldownExpiry = 0u;

FString MakeTauntSoundName(const char* variant)
{
	FString result;
	if (variant != nullptr && variant[0] != '\0')
	{
		result.Format("*taunt-%s", variant);
	}
	else
	{
		result = "*taunt";
	}
	return result;
}
}

// Helper exposed to the demo/network parser in `d_net.cpp` so the receive
// path stays close to the rest of the cosmetic DEM handlers there. Plays the
// taunt sound on `pawn` using the same `*taunt[-variant]` convention as the
// local CCMD; safe to call with a null variant (treated as canonical) or a
// dead/null pawn (no-op).
void HCDETauntPlayOnPawn(AActor* pawn, const char* variant)
{
	if (pawn == nullptr)
		return;

	const FString name = MakeTauntSoundName(variant);
	S_Sound(pawn, CHAN_VOICE, 0, name.GetChars(), 1.f, ATTN_NORM);
}

CCMD(taunt)
{
	if (gamestate != GS_LEVEL)
	{
		Printf("taunt: not in a level.\n");
		return;
	}

	player_t* pp = &players[consoleplayer];
	if (pp->mo == nullptr)
	{
		Printf("taunt: no local pawn.\n");
		return;
	}

	const uint64_t now = I_msTime();
	if (now < gTauntCooldownExpiry)
	{
		const uint64_t remainMS = gTauntCooldownExpiry - now;
		Printf("taunt: cooldown (%llums remaining).\n",
			static_cast<unsigned long long>(remainMS));
		return;
	}

	const char* variant = (argv.argc() >= 2) ? argv[1] : nullptr;

	// Emit the cosmetic DEM_TAUNT opcode on the command stream so other
	// peers can play the same taunt on this player's pawn. The local
	// instance also receives the opcode via the normal command-loop
	// echo, so we do NOT call S_Sound() directly here -- letting the
	// receive path handle it keeps single-player and multiplayer
	// behaviour consistent (and gives the cooldown teeth in both cases).
	Net_WriteInt8(DEM_TAUNT);
	Net_WriteInt8(0u); // variant index (reserved; 0 = canonical/string-driven)
	Net_WriteString(variant != nullptr ? variant : "");

	gTauntCooldownExpiry = now + kTauntCooldownMS;
}
