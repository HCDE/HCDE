#pragma once

#include <cstdint>
#include <cstddef>

// HCDE net black box packet recorder.
//
// A bounded, mutex-guarded ring buffer of recent network packets and per-tic
// anchors. Used both as a "what just happened?" forensic tool when a checksum
// mismatch / prediction fault fires, and as the on-demand capture target for
// `net_blackbox_save` / `net_diag_bundle`.

// Allocate ring storage and reset cooldowns. Idempotent.
void Net_BlackboxInit();
// Release ring storage. After this returns the next record call will lazily
// re-init via the atomic guard.
void Net_BlackboxShutdown();
// Record a single packet. `direction` is 0 for outbound, 1 for inbound.
// `payload`/`payloadSize` describe the packet bytes as they sat on the wire.
void Net_BlackboxRecordPacket(int direction, int peerSlot, uint8_t lane,
	uint32_t seq, uint32_t ack, uint8_t room, const uint8_t* payload, size_t payloadSize);
// Stamp a tic boundary into the ring so the offline tool can correlate
// adjacent packets to the gametic/clienttic they straddled.
void Net_BlackboxRecordTicIndex();
// Save the ring with a labeled trigger. Honors a 5-second cooldown so a
// burst of triggers does not produce a wall of redundant dumps.
void Net_BlackboxAutoSave(const char* trigger);
// Save unconditionally. Returns false only when fopen fails.
bool Net_BlackboxSave(const char* label, class FString& outPath);
// Drop everything currently buffered. Does not touch any on-disk files.
void Net_BlackboxClear();
