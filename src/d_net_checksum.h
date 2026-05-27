#pragma once

#include <cstdint>
#include <cstddef>

// HCDE world-state checksum API.
//
// The server computes and emits CRC32 fingerprints of select playsim state
// (players, sectors, movers, actors, RNG, line specials) inside each snapshot
// it sends; clients recompute the same set and compare. Any divergence is
// reported through Net_ChecksumReportMismatch which throttles trace lines and
// forensic bundle writes.

// Initialize state at process start; clears all cached hashes and cooldowns.
void Net_ChecksumInit();
// Reset state on a level change; identical to Net_ChecksumInit() but exists as
// a separate symbol so the call sites read clearly.
void Net_ChecksumResetForNewLevel();
// Per-gametic recompute hook called from G_Ticker. Keeps the client's per-tic
// hash ring buffer dense so Net_ChecksumReadAndCompare can look up the server's
// compute tic. The server-side snapshot path always recomputes via
// Net_ChecksumComputeIfStale() when building payloads.
void Net_ChecksumTick();
// Roll a line-special activation into the running line-spec hash.
void Net_ChecksumNoteLineSpec(int lineIndex, int special, bool success);
// Server-side: append the latest hashes to a snapshot body. No-op when the
// caller is not the local authority or `net_checksum` is disabled.
void Net_ChecksumApplyServerChunk(uint8_t* output, size_t outputCapacity, size_t& cursor);
// Client-side: parse and compare a chunk produced by the call above. Returns
// false only on a structurally malformed chunk; an absent chunk returns true.
bool Net_ChecksumReadAndCompare(const uint8_t* body, size_t bodyBytes, size_t& cursor, uint32_t serverTic);
// Surface a category-level mismatch; subject to internal cooldowns.
void Net_ChecksumReportMismatch(const char* category, uint32_t tic, uint32_t serverHash, uint32_t localHash);
