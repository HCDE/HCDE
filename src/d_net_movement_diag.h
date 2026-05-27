#pragma once

// HCDE movement diagnostics
//
// Always-on, bounded-cost instrumentation for diagnosing prediction lag,
// jitter, and server/client unpredictability. Tracks four signals the
// existing predict-metrics CSV does not capture:
//
//   1. Snapshot inter-arrival jitter (per peer). Updated whenever the client
//      accepts an authoritative server snapshot.
//   2. Local pawn drift vs authority truth, sampled every world delta apply.
//      Includes pos / vel deltas, predicted-vs-server jitter, repair tier.
//   3. Wall-clock frame time histogram (last 64 frames). Detects rendering
//      hitches that aren't visible in tic-based metrics.
//   4. Input-to-echo latency: how many tics between sending a button press
//      (BT_ATTACK / BT_USE) and seeing the authority confirm via snapshot.
//
// All samples are bounded ring buffers in process memory. CSV output goes to
// `%LOCALAPPDATA%/hcde/hcde_movement_diag.csv`. Use the `net_movement_dump`
// console command to print the rolling summary on demand.

#include <cstdint>

class FString;

// Snapshot RX jitter tracker. Called from the snapshot accept path on the
// client. `nowMS` is the wall-clock receive time. Updates per-peer rolling
// average + variance of inter-arrival.
void HCDEMovementOnSnapshotRx(int clientNum, uint64_t nowMS);

// Reset all per-peer jitter state (called on net restart / room change).
void HCDEMovementResetJitter();

// Drift sample. Called from world-delta apply for the local console player.
//   serverTic    : authority gametic the server reported
//   driftUnits   : sqrt(pos delta squared) in map units
//   velDeltaUnits: |client vel - server vel|
//   repairTier   : 0=skipped, 1=baseline, 2=hard, 3=respawn, 4=death
void HCDEMovementOnReconcile(uint32_t serverTic, double driftUnits, double velDeltaUnits,
	int serverHealth, int localHealth, int repairTier);

// Per-frame entry. Called once per `D_Display` tick or once per `TryRunTics`
// frame -- we want wall-clock frame time, not tic time.
void HCDEMovementOnFrame(uint64_t nowMS);

// Input-button rising-edge tracker. `buttonBit` is the BT_* bit. `nowTic` is
// gametic at the moment we issued the command. Stores it so we can match
// against the authority echo.
void HCDEMovementOnInputButton(uint32_t buttonBit, int nowTic);

// Authority confirmation. Called when the authority snapshot reports the
// local player executed the input (e.g. weapon fired). `nowTic` is gametic
// at the moment we observed the echo.
void HCDEMovementOnInputEcho(uint32_t buttonBit, int nowTic);

// Print rolling summary. Renders to the in-game console.
void HCDEMovementPrintSummary();

// Compose a one-line summary suitable for embedding in trace markers.
FString HCDEMovementOneLineSummary();

// Adaptive prediction lead (Phase 3). The desired lead is the small buffer the
// world tic loop should preserve for smooth local prediction. The command lead
// limit is a harder ceiling for client command generation so lead cannot run
// away when snapshots arrive irregularly.
int HCDEMovementGetAdaptiveDesiredLead(int configuredLead);
int HCDEMovementGetAdaptiveCommandLeadLimit(int configuredLead, int fallbackLimit);
void HCDEMovementGetAdaptiveLeadDebug(int& desiredLead, int& commandLimit,
	double& jitterAvgMS, double& jitterStdevMS, uint64_t& samples);
