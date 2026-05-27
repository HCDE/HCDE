#pragma once

#include "d_net_blackbox.h"
#include "d_net_checksum.h"

#include <cstdint>
#include <cstddef>

class FString;

// ---------------------------------------------------------------------------
// HCDE net diagnostics expansion (session, marks, bundle, black box, checksum)
// ---------------------------------------------------------------------------

struct FHCDENetDiagProfileSummary
{
	uint64_t PredictionFaultReports = 0u;
	uint64_t PredictionLocalHealthRepairs = 0u;
	uint64_t PredictionLocalStateRepairs = 0u;
	uint64_t PredictionHardRespawnRepairs = 0u;
	uint64_t PredictionHardDeathRepairs = 0u;
	uint64_t RemotePlayerBaselineRepairs = 0u;
	uint64_t ActorDeltaV2RecordsMissing = 0u;
	uint64_t WorldDeltaRecordsReceived = 0u;
	uint64_t ClientInputNativeApplied = 0u;
	uint64_t ServerSnapshotNativeApplied = 0u;
};

void HCDEGetLiveProfileSummary(FHCDENetDiagProfileSummary& out);

void Net_DiagSessionBegin();
void Net_DiagSessionNetReady();
void Net_DiagMapBegin(const char* mapName);
void Net_DiagMapEnd(const char* mapName, const char* reason);
void Net_DiagSessionEnd(const char* reason);
void Net_DiagPeerCorrelate(int clientNum, uint32_t remoteSessionId, uint64_t negotiatedCaps);

void Net_DiagQueueMark(const char* label);
void Net_DiagRunMarkLocally(const char* label);
void Net_DiagApplyPendingMark(int clientNum, const char* label);
bool Net_DiagHasPendingMark();
// Append the queued client mark (if any) to an outgoing packet body.
// Returns false only when the mark would not fit in the output buffer.
// On a successful append the in-memory queue is cleared.
bool Net_DiagTryAppendMarkChunk(uint8_t* output, size_t outputCapacity, size_t& cursor);
// Look for an HCMK chunk at `body[bodyCursor]`. Returns true only when the magic
// matches and the chunk is fully present; sets `outChunkBytes` to the encoded size
// of the chunk so the caller can either skip or consume it. The cursor is left
// untouched - callers decide how to advance.
bool Net_DiagPeekMarkChunk(const uint8_t* body, size_t bodyBytes, size_t bodyCursor, size_t& outChunkBytes);
// Consume an HCMK chunk previously appended by Net_DiagTryAppendMarkChunk and
// run the diagnostic mark on the receiving side. Returns true only when a chunk
// was found AND fully parsed; the cursor is advanced on success.
bool Net_DiagTryConsumeMarkChunk(const uint8_t* body, size_t bodyBytes, size_t& bodyCursor, int clientNum);

void Net_DiagGetProfileSummary(FHCDENetDiagProfileSummary& out);

void Net_DiagTraceInputAuthority(int clientNum, const char* event, const char* detail);
void Net_DiagTraceServerPlayerTruth(int clientNum, uint32_t serverTic, int playerNum,
	double x, double y, double z, double vx, double vy, double vz,
	int health, bool onGround, uint8_t playerState);

void Net_DiagRunPredictDump();
bool Net_DiagWriteBundle(const char* label, FString& outPath);

// Gameplay / world event telemetry (gated by net_event_debug)
void Net_DiagTraceUseEdge(int playerNum, uint32_t buttons, bool rising);
void Net_DiagTraceUseLine(int playerNum, int lineIndex, bool success, bool useFail);
void Net_DiagTraceLineSpec(int playerNum, int lineIndex, int special, int tag, bool success, bool predicted);
void Net_DiagTraceDoorEvent(int tag, int type, int direction, double speed);
void Net_DiagTraceSectorAction(int sectorIndex, int activation, bool didIt);

// Inspect bundle / blackbox from console
void Net_DiagInspectFile(const char* path);
