// HCDE movement diagnostics implementation. See d_net_movement_diag.h.

#include "d_net_movement_diag.h"

#include "doomstat.h"
#include "d_player.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "common/engine/debugtrace.h"
#include "i_specialpaths.h"
#include "i_time.h"
#include "zstring.h"

#include <stdio.h>
#include <math.h>
#include <cmath>
#include <stdint.h>

EXTERN_CVAR(Int, net_predict_debug)
EXTERN_CVAR(Int, net_event_debug)
EXTERN_CVAR(Int, cl_net_prediction_lead)

// 0 = disabled (in-memory metrics still tracked, no CSV output)
// 1 = jitter+drift CSV samples written every reconcile
// 2 = +per-frame trace markers
// 3 = full
//
// Defaults to 0: at level 1 we fopen-append the CSV once per reconcile
// (~30 Hz on a client). On Windows + antivirus that I/O cost compounds and
// after ~10 seconds of play it shows up as periodic rubber banding because
// each append blocks the snapshot RX path. Diagnostics are now opt-in;
// adaptive prediction lead and net_movement_dump still work at level 0.
CUSTOM_CVAR(Int, net_movement_debug, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	if (self > 3)
		self = 3;
}

// Adaptive lead control. This does not rewrite cl_net_prediction_lead; it
// derives a runtime lead from measured snapshot jitter and applies a tighter
// command-generation ceiling so client lead cannot balloon during stalls.
CUSTOM_CVAR(Bool, net_adaptive_lead, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
}

CUSTOM_CVAR(Int, net_adaptive_lead_min, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	if (self > 8)
		self = 8;
}

CUSTOM_CVAR(Int, net_adaptive_lead_max, 6, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	if (self > 16)
		self = 16;
}

CUSTOM_CVAR(Int, net_adaptive_lead_guard, 3, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	if (self > 16)
		self = 16;
}

namespace
{

constexpr int kPeerCount = 64;
constexpr int kFrameRingSize = 64;
constexpr int kInputBucketCount = 8;	// track up to 8 distinct button bits

struct FJitterState
{
	uint64_t LastRxMS = 0u;
	uint64_t Samples = 0u;
	double IntervalAvgMS = 0.0;
	double IntervalM2 = 0.0;	// Welford running variance accumulator
	double IntervalMaxMS = 0.0;
	double IntervalMinMS = 0.0;
	uint64_t LongStretches = 0u;	// inter-arrival > 100ms (3.5+ tics)
};

struct FFrameRing
{
	uint64_t LastFrameMS = 0u;
	int WriteIdx = 0;
	int Count = 0;
	double Samples[kFrameRingSize] = {};
	double WindowMaxMS = 0.0;
	uint64_t LongFrameCount = 0u;	// frames over 33ms (i.e. <30fps)
};

struct FDriftSummary
{
	uint64_t Samples = 0u;
	double DriftMaxUnits = 0.0;
	double DriftAvgUnits = 0.0;	// running mean
	double DriftM2 = 0.0;
	double VelMaxUnits = 0.0;
	uint64_t HardRepairs = 0u;
	uint64_t BaselineRepairs = 0u;
	uint64_t Skips = 0u;
};

struct FInputBucket
{
	uint32_t ButtonBit = 0u;
	int LastIssuedTic = -1;
	uint64_t Samples = 0u;
	double LatencyAvgTics = 0.0;
	int LatencyMaxTics = 0;
	int LatencyMinTics = INT32_MAX;
};

FJitterState gJitter[kPeerCount];
FFrameRing gFrameRing;
FDriftSummary gDrift;
FInputBucket gInputs[kInputBucketCount];

bool gCsvHeaderProbed = false;

const char* RepairTierName(int tier)
{
	switch (tier)
	{
	case 0: return "skip";
	case 1: return "baseline";
	case 2: return "hard";
	case 3: return "respawn";
	case 4: return "death";
	default: return "?";
	}
}

FInputBucket* FindOrAllocBucket(uint32_t buttonBit)
{
	for (auto& bucket : gInputs)
	{
		if (bucket.ButtonBit == buttonBit)
			return &bucket;
	}
	for (auto& bucket : gInputs)
	{
		if (bucket.ButtonBit == 0u)
		{
			bucket.ButtonBit = buttonBit;
			bucket.LatencyMinTics = INT32_MAX;
			return &bucket;
		}
	}
	return nullptr;
}

double FrameRingAverageMS()
{
	if (gFrameRing.Count <= 0)
		return 0.0;
	double sum = 0.0;
	for (int i = 0; i < gFrameRing.Count; ++i)
		sum += gFrameRing.Samples[i];
	return sum / double(gFrameRing.Count);
}

double FrameRingMaxMS()
{
	if (gFrameRing.Count <= 0)
		return 0.0;
	double maxV = gFrameRing.Samples[0];
	for (int i = 1; i < gFrameRing.Count; ++i)
		maxV = gFrameRing.Samples[i] > maxV ? gFrameRing.Samples[i] : maxV;
	return maxV;
}

void EnsureCsvHeader(const char* path)
{
	if (gCsvHeaderProbed)
		return;
	// Open in append mode. "ab" creates the file when missing but, crucially,
	// never truncates an existing CSV - unlike the previous fopen(path, "w")
	// path, which wiped accumulated history whenever the read probe failed for
	// a file that actually existed (AV lock, transient permission error, etc.).
	FILE* csv = fopen(path, "ab");
	if (csv == nullptr)
		return; // Could not open at all; leave the flag clear so we retry later.
	gCsvHeaderProbed = true;
	// Seek to the end so ftell reports the real size; a zero here means the
	// file is brand new / empty and still needs its column header. Append-mode
	// writes always land at EOF regardless of the current seek position.
	fseek(csv, 0, SEEK_END);
	if (ftell(csv) <= 0)
	{
		fprintf(csv,
			"ms_time,gametic,client,server_tic,event,drift_units,vel_delta,"
			"server_health,local_health,repair,jitter_avg_ms,jitter_max_ms,"
			"jitter_min_ms,jitter_long_stretches,frame_avg_ms,frame_max_ms\n");
	}
	fclose(csv);
}

// Pick the peer with the most snapshot RX samples as a stand-in for the
// authority. In single-player or pre-handshake we return null and adaptive
// lead falls back to the configured CVar.
const FJitterState* AuthorityJitter()
{
	const FJitterState* best = nullptr;
	for (int i = 0; i < kPeerCount; ++i)
	{
		if (gJitter[i].Samples == 0u)
			continue;
		if (best == nullptr || gJitter[i].Samples > best->Samples)
			best = &gJitter[i];
	}
	return best;
}

double JitterStdevMS(const FJitterState* jit)
{
	if (jit == nullptr || jit->Samples <= 1u)
		return 0.0;
	return sqrt(jit->IntervalM2 / double(jit->Samples - 1u));
}

int ComputeAdaptiveDesiredLead(int configuredLead, const FJitterState* jit)
{
	const int configured = clamp<int>(configuredLead, 0, 8);
	if (!*net_adaptive_lead || jit == nullptr || jit->Samples < 4u)
		return configured;

	const double idealSnapshotMS = 1000.0 / double(TICRATE);
	const double stdevMS = JitterStdevMS(jit);
	const double excessIntervalMS = jit->IntervalAvgMS > idealSnapshotMS
		? jit->IntervalAvgMS - idealSnapshotMS
		: 0.0;
	const double safetyMS = excessIntervalMS + stdevMS * 2.0;
	const int jitterLead = int(ceil(safetyMS / idealSnapshotMS));
	const int minLead = min<int>(*net_adaptive_lead_min, *net_adaptive_lead_max);
	const int maxLead = max<int>(*net_adaptive_lead_min, *net_adaptive_lead_max);
	return clamp<int>(max<int>(configured, jitterLead), minLead, maxLead);
}

void WriteCsvSample(const char* event, uint32_t serverTic, double driftUnits,
	double velDeltaUnits, int serverHealth, int localHealth, int repairTier)
{
	if (*net_movement_debug <= 0)
		return;

	const FString path = FStringf("%s/hcde_movement_diag.csv",
		M_GetAppDataPath(true).GetChars());
	EnsureCsvHeader(path.GetChars());

	const FJitterState* jit = AuthorityJitter();
	double jitterAvg = jit != nullptr ? jit->IntervalAvgMS : 0.0;
	double jitterMax = jit != nullptr ? jit->IntervalMaxMS : 0.0;
	double jitterMin = jit != nullptr ? jit->IntervalMinMS : 0.0;
	uint64_t jitterLong = jit != nullptr ? jit->LongStretches : 0u;

	if (FILE* csv = fopen(path.GetChars(), "a"))
	{
		fprintf(csv,
			"%llu,%d,%d,%u,%s,%.3f,%.3f,%d,%d,%s,%.2f,%.2f,%.2f,%llu,%.2f,%.2f\n",
			static_cast<unsigned long long>(I_msTime()),
			gametic, consoleplayer, serverTic, event,
			driftUnits, velDeltaUnits,
			serverHealth, localHealth, RepairTierName(repairTier),
			jitterAvg, jitterMax, jitterMin,
			static_cast<unsigned long long>(jitterLong),
			FrameRingAverageMS(), FrameRingMaxMS());
		fclose(csv);
	}
}

}	// namespace

void HCDEMovementOnSnapshotRx(int clientNum, uint64_t nowMS)
{
	if (clientNum < 0 || clientNum >= kPeerCount)
		return;
	FJitterState& s = gJitter[clientNum];
	if (s.LastRxMS == 0u || nowMS < s.LastRxMS)
	{
		s.LastRxMS = nowMS;
		return;
	}
	const double interval = double(nowMS - s.LastRxMS);
	s.LastRxMS = nowMS;
	++s.Samples;
	const double delta = interval - s.IntervalAvgMS;
	s.IntervalAvgMS += delta / double(s.Samples);
	const double delta2 = interval - s.IntervalAvgMS;
	s.IntervalM2 += delta * delta2;
	if (interval > s.IntervalMaxMS)
		s.IntervalMaxMS = interval;
	// `Samples == 1` means this is the first observed interval, so seed min
	// with it. Using `IntervalMinMS == 0.0` as a sentinel was wrong because a
	// genuinely sub-millisecond burst would lock min at 0 forever.
	if (s.Samples == 1u || interval < s.IntervalMinMS)
		s.IntervalMinMS = interval;
	if (interval > 100.0)
		++s.LongStretches;

	if (*net_movement_debug >= 2 && interval > 100.0)
	{
		DebugTrace::Markf("net.move",
			"snapshot stretch client=%d interval=%.1fms avg=%.1fms max=%.1fms",
			clientNum, interval, s.IntervalAvgMS, s.IntervalMaxMS);
	}
}

void HCDEMovementResetJitter()
{
	for (auto& s : gJitter)
		s = FJitterState{};
	for (auto& bucket : gInputs)
		bucket = FInputBucket{};
	gFrameRing = FFrameRing{};
	gDrift = FDriftSummary{};
}

void HCDEMovementOnReconcile(uint32_t serverTic, double driftUnits, double velDeltaUnits,
	int serverHealth, int localHealth, int repairTier)
{
	// Guard the Welford accumulators against NaN/Inf. A single non-finite
	// sample would permanently poison DriftAvgUnits / DriftM2 / DriftMaxUnits
	// (NaN compares false against everything, so DriftMaxUnits could also latch
	// a bogus value), corrupting every later stdev/summary readout. Coerce a
	// bad sample to zero drift so the counters stay meaningful.
	if (!std::isfinite(driftUnits))
		driftUnits = 0.0;
	if (!std::isfinite(velDeltaUnits))
		velDeltaUnits = 0.0;
	++gDrift.Samples;
	const double delta = driftUnits - gDrift.DriftAvgUnits;
	gDrift.DriftAvgUnits += delta / double(gDrift.Samples);
	const double delta2 = driftUnits - gDrift.DriftAvgUnits;
	gDrift.DriftM2 += delta * delta2;
	if (driftUnits > gDrift.DriftMaxUnits)
		gDrift.DriftMaxUnits = driftUnits;
	if (velDeltaUnits > gDrift.VelMaxUnits)
		gDrift.VelMaxUnits = velDeltaUnits;
	switch (repairTier)
	{
	case 1: ++gDrift.BaselineRepairs; break;
	case 2: ++gDrift.HardRepairs; break;
	case 0: ++gDrift.Skips; break;
	default: break;
	}

	const char* event = "reconcile";
	if (repairTier == 2) event = "hard";
	else if (repairTier == 1) event = "baseline";
	else if (repairTier == 3) event = "respawn";
	else if (repairTier == 4) event = "death";
	else if (repairTier == 0) event = "skip";

	WriteCsvSample(event, serverTic, driftUnits, velDeltaUnits,
		serverHealth, localHealth, repairTier);
}

void HCDEMovementOnFrame(uint64_t nowMS)
{
	if (gFrameRing.LastFrameMS == 0u || nowMS < gFrameRing.LastFrameMS)
	{
		gFrameRing.LastFrameMS = nowMS;
		return;
	}
	double dt = double(nowMS - gFrameRing.LastFrameMS);
	gFrameRing.LastFrameMS = nowMS;
	// Clamp pathological gaps (alt-tab, debugger pause, OS hibernate). Without
	// the cap a multi-second outage permanently poisons WindowMaxMS / averages.
	// 1000ms is well past the worst legitimate frame this engine produces.
	if (dt > 1000.0)
		dt = 1000.0;
	gFrameRing.Samples[gFrameRing.WriteIdx] = dt;
	gFrameRing.WriteIdx = (gFrameRing.WriteIdx + 1) % kFrameRingSize;
	if (gFrameRing.Count < kFrameRingSize)
		++gFrameRing.Count;
	if (dt > gFrameRing.WindowMaxMS)
		gFrameRing.WindowMaxMS = dt;
	if (dt > 33.0)
		++gFrameRing.LongFrameCount;
}

void HCDEMovementOnInputButton(uint32_t buttonBit, int nowTic)
{
	FInputBucket* bucket = FindOrAllocBucket(buttonBit);
	if (bucket == nullptr)
		return;
	bucket->LastIssuedTic = nowTic;
}

void HCDEMovementOnInputEcho(uint32_t buttonBit, int nowTic)
{
	FInputBucket* bucket = FindOrAllocBucket(buttonBit);
	if (bucket == nullptr || bucket->LastIssuedTic < 0)
		return;
	const int latency = nowTic - bucket->LastIssuedTic;
	bucket->LastIssuedTic = -1;
	if (latency < 0 || latency > 1000)
		return;
	++bucket->Samples;
	const double delta = double(latency) - bucket->LatencyAvgTics;
	bucket->LatencyAvgTics += delta / double(bucket->Samples);
	if (latency > bucket->LatencyMaxTics)
		bucket->LatencyMaxTics = latency;
	if (latency < bucket->LatencyMinTics)
		bucket->LatencyMinTics = latency;
}

int HCDEMovementGetAdaptiveDesiredLead(int configuredLead)
{
	return ComputeAdaptiveDesiredLead(configuredLead, AuthorityJitter());
}

int HCDEMovementGetAdaptiveCommandLeadLimit(int configuredLead, int fallbackLimit)
{
	// Phase 3 originally returned a tightened single-digit cap here, but in
	// practice the world-tic cap (in d_net.cpp around the desiredLead
	// enforcement) is the mechanism that actually keeps client lead pinned
	// near desiredLead. This function only feeds a hard ceiling on command
	// generation, and tightening it caused command throughput to choke on
	// routine jitter (rubber-banding after ~10s as the lead naturally
	// reached the pinhole). The caller now passes the legacy BACKUPTICS/2
	// ceiling and we just echo that back. The signature is retained so
	// existing call sites keep compiling and a future phase can re-enable
	// adaptive ceilings without another API churn.
	(void)configuredLead;
	return max<int>(fallbackLimit, 1);
}

void HCDEMovementGetAdaptiveLeadDebug(int& desiredLead, int& commandLimit,
	double& jitterAvgMS, double& jitterStdevMS, uint64_t& samples)
{
	const FJitterState* jit = AuthorityJitter();
	desiredLead = HCDEMovementGetAdaptiveDesiredLead(int(*cl_net_prediction_lead));
	commandLimit = HCDEMovementGetAdaptiveCommandLeadLimit(desiredLead, desiredLead + clamp<int>(*net_adaptive_lead_guard, 1, 16));
	jitterAvgMS = jit != nullptr ? jit->IntervalAvgMS : 0.0;
	jitterStdevMS = JitterStdevMS(jit);
	samples = jit != nullptr ? jit->Samples : 0u;
}

FString HCDEMovementOneLineSummary()
{
	const FJitterState* jit = AuthorityJitter();
	double jitterAvg = jit != nullptr ? jit->IntervalAvgMS : 0.0;
	double jitterMax = jit != nullptr ? jit->IntervalMaxMS : 0.0;
	uint64_t jitterLong = jit != nullptr ? jit->LongStretches : 0u;
	FString out;
	out.Format("jitter avg=%.1fms max=%.1fms long=%llu drift avg=%.2f max=%.2f vel-max=%.2f hard=%llu base=%llu frame avg=%.1fms max=%.1fms long=%llu",
		jitterAvg, jitterMax, static_cast<unsigned long long>(jitterLong),
		gDrift.DriftAvgUnits, gDrift.DriftMaxUnits, gDrift.VelMaxUnits,
		static_cast<unsigned long long>(gDrift.HardRepairs),
		static_cast<unsigned long long>(gDrift.BaselineRepairs),
		FrameRingAverageMS(), FrameRingMaxMS(),
		static_cast<unsigned long long>(gFrameRing.LongFrameCount));
	return out;
}

void HCDEMovementPrintSummary()
{
	Printf(PRINT_HIGH, "\n=== HCDE movement diagnostics ===\n");

	int activePeers = 0;
	for (int i = 0; i < kPeerCount; ++i)
	{
		const FJitterState& s = gJitter[i];
		if (s.Samples == 0u)
			continue;
		++activePeers;
		const double variance = s.Samples > 1u ? s.IntervalM2 / double(s.Samples - 1u) : 0.0;
		const double stdev = sqrt(variance);
		Printf(PRINT_HIGH,
			"peer %d snapshot RX: samples=%llu avg=%.2fms stdev=%.2fms min=%.2fms max=%.2fms long-stretches=%llu\n",
			i, static_cast<unsigned long long>(s.Samples),
			s.IntervalAvgMS, stdev, s.IntervalMinMS, s.IntervalMaxMS,
			static_cast<unsigned long long>(s.LongStretches));
	}
	if (activePeers == 0)
		Printf(PRINT_HIGH, "snapshot RX jitter: no samples yet (single-player or not connected)\n");

	if (gDrift.Samples == 0u)
	{
		Printf(PRINT_HIGH, "drift: no reconcile samples yet\n");
	}
	else
	{
		const double variance = gDrift.Samples > 1u ? gDrift.DriftM2 / double(gDrift.Samples - 1u) : 0.0;
		const double stdev = sqrt(variance);
		Printf(PRINT_HIGH,
			"drift: samples=%llu avg=%.3f stdev=%.3f max=%.3f vel-max=%.3f baseline=%llu hard=%llu skip=%llu\n",
			static_cast<unsigned long long>(gDrift.Samples),
			gDrift.DriftAvgUnits, stdev, gDrift.DriftMaxUnits, gDrift.VelMaxUnits,
			static_cast<unsigned long long>(gDrift.BaselineRepairs),
			static_cast<unsigned long long>(gDrift.HardRepairs),
			static_cast<unsigned long long>(gDrift.Skips));
	}

	Printf(PRINT_HIGH,
		"frame time (last %d): avg=%.2fms max=%.2fms window-max=%.2fms long-frames(>33ms)=%llu\n",
		gFrameRing.Count, FrameRingAverageMS(), FrameRingMaxMS(),
		gFrameRing.WindowMaxMS,
		static_cast<unsigned long long>(gFrameRing.LongFrameCount));

	bool anyInput = false;
	for (const auto& bucket : gInputs)
	{
		if (bucket.ButtonBit == 0u || bucket.Samples == 0u)
			continue;
		anyInput = true;
		Printf(PRINT_HIGH,
			"input echo button=0x%08x samples=%llu avg=%.2f tics min=%d max=%d\n",
			bucket.ButtonBit, static_cast<unsigned long long>(bucket.Samples),
			bucket.LatencyAvgTics, bucket.LatencyMinTics, bucket.LatencyMaxTics);
	}
	if (!anyInput)
		Printf(PRINT_HIGH, "input echo: no samples yet\n");

	int adaptiveDesired = 0;
	int adaptiveLimit = 0;
	double adaptiveJitterAvg = 0.0;
	double adaptiveJitterStdev = 0.0;
	uint64_t adaptiveSamples = 0u;
	HCDEMovementGetAdaptiveLeadDebug(adaptiveDesired, adaptiveLimit,
		adaptiveJitterAvg, adaptiveJitterStdev, adaptiveSamples);
	Printf(PRINT_HIGH,
		"adaptive lead: enabled=%d desired=%d command-limit=%d jitter=%.2fms stdev=%.2fms samples=%llu min=%d max=%d guard=%d\n",
		*net_adaptive_lead ? 1 : 0, adaptiveDesired, adaptiveLimit,
		adaptiveJitterAvg, adaptiveJitterStdev,
		static_cast<unsigned long long>(adaptiveSamples),
		int(*net_adaptive_lead_min), int(*net_adaptive_lead_max),
		int(*net_adaptive_lead_guard));

	Printf(PRINT_HIGH,
		"net_movement_debug=%d (0=off, 1=jitter+drift CSV, 2=+frame trace, 3=full)\n",
		int(*net_movement_debug));
	Printf(PRINT_HIGH, "csv: %s/hcde_movement_diag.csv\n",
		M_GetAppDataPath(true).GetChars());
	Printf(PRINT_HIGH, "================================\n");
}

CCMD(net_movement_dump)
{
	HCDEMovementPrintSummary();
}

CCMD(net_movement_reset)
{
	HCDEMovementResetJitter();
	gCsvHeaderProbed = false;
	Printf(PRINT_HIGH, "HCDE movement diagnostics reset.\n");
}
