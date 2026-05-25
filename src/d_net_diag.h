#pragma once

#include <cstdint>

constexpr uint8_t HCDEPresentationEchoMagic[4] = { 'E', 'C', 'H', 'O' };
constexpr uint8_t HCDEPresentationEchoProtocolVersion = 1u;

// Forward declarations
struct player_t;

// Ping/Latency metrics helper
void Net_LogPingSample(int clientNum, int leadTics, int leadMs, int rttMs, int ticDup, int extraTics, int delta);

// Presentation echo helper
void Net_CompareEchoToLocal(int clientNum, uint32_t serverTic, int playerNum,
	uint32_t readyWeapName, uint32_t pendingWeapName,
	uint32_t pspriteStateName, int16_t pspriteTics,
	uint16_t weaponState, uint8_t playerState, int16_t viewHeight);

bool HCDEAppendPresentationEcho(int client, uint8_t* output, size_t outputCapacity, size_t& cursor, const uint8_t* playerNums, size_t playerCount);
bool HCDEReadPresentationEcho(int clientNum, const uint8_t* body, size_t bodyBytes, size_t& cursor);

struct usercmd_t;
void Net_ApplySelfTestInputs(usercmd_t* cmd, int clientTic);
