#pragma once

#include "common/engine/debugtrace.h"

inline const char* PlayerStateToString(int state)
{
	switch (state)
	{
	case 0: return "PST_LIVE";
	case 1: return "PST_DEAD";
	case 2: return "PST_REBORN";
	case 3: return "PST_ENTER";
	case 4: return "PST_GONE";
	default: return "PST_UNKNOWN";
	}
}

#define SET_PLAYER_STATE(player_ptr, player_num, new_state, reason_str) \
	do { \
		auto* _p = (player_ptr); \
		if (_p != nullptr) { \
			const int _old = _p->playerstate; \
			_p->playerstate = (new_state); \
			DebugTrace::Markf("playsim.playerstate", "player=%d %s -> %s reason=%s", \
				int(player_num), PlayerStateToString(_old), PlayerStateToString(new_state), (reason_str)); \
		} \
	} while (0)
