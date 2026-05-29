// HCDE Eternity Spatial Audio backend facade.
//
// Roadmap board item: #3 ("Eternity's Spatial Audio Engine").
//
// Per the design in `docs/HCDE_FEATURE_IMPORTS.md` and
// `docs/HCDE_ROADMAP_INTEGRATION_PLAN.md`:
//
//   - The Eternity audio engine is added as a *sibling backend* to the
//     existing OpenAL / etc. backends under `common/audio/sound/`.
//   - No new abstraction layer is introduced; the existing `SoundSystem`
//     (or equivalent) interfaces are reused.
//   - Backend selection is hidden behind the `snd_backend` CVAR (or the
//     equivalent existing selector). Default remains unchanged.
//   - All audio state remains local to the client; no replication or
//     savegame impact.
//
// This file is the Phase 1 facade. It compiles, links, registers as a real
// SoundRenderer, and records the same source/listener/spatial parameters the
// eventual Eternity mixer will consume. It is intentionally silent until the
// actual Eternity mixer sources (or a vendored library) are integrated.
//
// When wiring the real backend:
//
//   1. Replace the stub `EternitySoundSystem` (or equivalent) with a
//      concrete implementation that forwards to the Eternity mixer.
//   2. Register the backend with whatever factory / selector the engine
//      uses (behind `snd_backend`).
//   3. Ensure the backend honours the existing `S_Sound` call sites and
//      the `SoundSystem` interface contract (source handles, listener
//      positions, attenuation, etc.).
//   4. Run the existing audio smoke tests in `tests/` to verify no
//      behavioural divergence on identical inputs.
//
// See `docs/HCDE_FEATURE_IMPORTS.md` § "Eternity Spatial Audio (facade
// boundary)" for the boundary contract (triggers only, no replicated
// audio state, no demo-determinism dependency on backend).

#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_sound.h"
#include "printf.h"
#include "s_soundinternal.h"  // for shared internal sound types

EXTERN_CVAR(String, snd_backend)

namespace
{

bool gEternityRegistrationAttempted = false;
bool gEternityRendererConstructed = false;
int gEternityDiagnosticTriggers = 0;

struct FEternitySpatialSubmit
{
	int Count2D = 0;
	int Count3D = 0;
	int ListenerUpdates = 0;
	int ParamUpdates3D = 0;
	float LastVolume = 0.0f;
	float LastPitch = 0.0f;
	float LastDistanceScale = 0.0f;
	float LastPosX = 0.0f;
	float LastPosY = 0.0f;
	float LastPosZ = 0.0f;
	float LastVelX = 0.0f;
	float LastVelY = 0.0f;
	float LastVelZ = 0.0f;
	int LastChannel = -1;
	int LastChannelFlags = 0;
	int LastPriority = 0;
	bool LastAreaSound = false;
};

class EternitySoundRenderer final : public SoundRenderer
{
public:
	EternitySoundRenderer()
	{
		gEternityRendererConstructed = true;
	}

	bool IsNull() override
	{
		return false;
	}

	void SetSfxVolume(float volume) override
	{
		SfxVolume = volume;
	}

	void SetMusicVolume(float volume) override
	{
		MusicVolume = volume;
	}

	void UpdateMusicParams() override
	{
	}

	SoundHandle LoadSound(uint8_t*, int, int, int) override
	{
		++LoadedSounds;
		SoundHandle retval = { nullptr };
		return retval;
	}

	SoundHandle LoadSoundRaw(uint8_t*, int, int, int, int, int, int) override
	{
		++LoadedSounds;
		SoundHandle retval = { nullptr };
		return retval;
	}

	void UnloadSound(SoundHandle) override
	{
	}

	unsigned int GetMSLength(SoundHandle) override
	{
		return 250;
	}

	unsigned int GetSampleLength(SoundHandle) override
	{
		return 0;
	}

	float GetOutputRate() override
	{
		return 44100.0f;
	}

	SoundStream* CreateStream(SoundStreamCallback, int, SampleType, ChannelConfig, int, void*) override
	{
		++StreamRequests;
		return nullptr;
	}

	FISoundChannel* StartSound(SoundHandle, float vol, float pitch, int chanflags, FISoundChannel*, float) override
	{
		++Spatial.Count2D;
		Spatial.LastVolume = vol;
		Spatial.LastPitch = pitch;
		Spatial.LastChannelFlags = chanflags;
		return nullptr;
	}

	FISoundChannel* StartSound3D(SoundHandle, SoundListener*, float vol, FRolloffInfo*, float distscale,
		float pitch, int priority, const FVector3& pos, const FVector3& vel, int channum,
		int chanflags, FISoundChannel*, float) override
	{
		++Spatial.Count3D;
		Spatial.LastVolume = vol;
		Spatial.LastPitch = pitch;
		Spatial.LastDistanceScale = distscale;
		Spatial.LastPosX = float(pos.X);
		Spatial.LastPosY = float(pos.Y);
		Spatial.LastPosZ = float(pos.Z);
		Spatial.LastVelX = float(vel.X);
		Spatial.LastVelY = float(vel.Y);
		Spatial.LastVelZ = float(vel.Z);
		Spatial.LastChannel = channum;
		Spatial.LastChannelFlags = chanflags;
		Spatial.LastPriority = priority;
		return nullptr;
	}

	void StopChannel(FISoundChannel*) override
	{
	}

	void ChannelVolume(FISoundChannel*, float volume) override
	{
		Spatial.LastVolume = volume;
	}

	void ChannelPitch(FISoundChannel*, float pitch) override
	{
		Spatial.LastPitch = pitch;
	}

	void MarkStartTime(FISoundChannel*, float) override
	{
	}

	unsigned int GetPosition(FISoundChannel*) override
	{
		return 0;
	}

	float GetAudibility(FISoundChannel*) override
	{
		return 0.0f;
	}

	void Sync(bool sync) override
	{
		SyncActive = sync;
	}

	void SetSfxPaused(bool paused, int) override
	{
		SfxPaused = paused;
	}

	void SetInactive(EInactiveState inactive) override
	{
		InactiveState = inactive;
	}

	void UpdateSoundParams3D(SoundListener*, FISoundChannel*, bool areasound, const FVector3& pos, const FVector3& vel) override
	{
		++Spatial.ParamUpdates3D;
		Spatial.LastAreaSound = areasound;
		Spatial.LastPosX = float(pos.X);
		Spatial.LastPosY = float(pos.Y);
		Spatial.LastPosZ = float(pos.Z);
		Spatial.LastVelX = float(vel.X);
		Spatial.LastVelY = float(vel.Y);
		Spatial.LastVelZ = float(vel.Z);
	}

	void UpdateListener(SoundListener*) override
	{
		++Spatial.ListenerUpdates;
	}

	void UpdateSounds() override
	{
		++MixTicks;
	}

	bool IsValid() override
	{
		return true;
	}

	void PrintStatus() override
	{
		Printf("Eternity spatial audio facade active (silent mixer placeholder).\n");
		Printf("  loaded=%d 2d-starts=%d 3d-starts=%d listener-updates=%d param-updates=%d mix-ticks=%d\n",
			LoadedSounds, Spatial.Count2D, Spatial.Count3D, Spatial.ListenerUpdates, Spatial.ParamUpdates3D, MixTicks);
	}

	void PrintDriversList() override
	{
		Printf("Eternity spatial audio facade has no platform drivers yet.\n");
	}

	FString GatherStats() override
	{
		FString out;
		out.Format("Eternity facade: loaded=%d 2d=%d 3d=%d listener=%d params=%d streams=%d mix=%d",
			LoadedSounds, Spatial.Count2D, Spatial.Count3D, Spatial.ListenerUpdates,
			Spatial.ParamUpdates3D, StreamRequests, MixTicks);
		return out;
	}

	FEternitySpatialSubmit Spatial;
	int LoadedSounds = 0;
	int StreamRequests = 0;
	int MixTicks = 0;
	float SfxVolume = 1.0f;
	float MusicVolume = 1.0f;
	bool SyncActive = false;
	bool SfxPaused = false;
	EInactiveState InactiveState = INACTIVE_Active;
};

EternitySoundRenderer* gEternityRenderer = nullptr;

} // namespace

SoundRenderer* I_CreateEternitySoundRenderer()
{
	gEternityRegistrationAttempted = true;
	gEternityRenderer = new EternitySoundRenderer;
	Printf(PRINT_HIGH, "snd_backend=eternity selected: using silent Eternity spatial facade (mixer not vendored yet).\n");
	return gEternityRenderer;
}

SoundRenderer* I_CreateEternitySoundRendererStub()
{
	return I_CreateEternitySoundRenderer();
}

static void HCDEEternityRecordDiagnosticTrigger()
{
	++gEternityDiagnosticTriggers;
	if (gEternityRenderer != nullptr)
	{
		SoundHandle empty = { nullptr };
		gEternityRenderer->StartSound(empty, 1.0f, 1.0f, 0, nullptr, 0.0f);
	}
}

CCMD(snd_eternity_probe)
{
	HCDEEternityRecordDiagnosticTrigger();
	Printf(PRINT_HIGH, "Eternity spatial audio diagnostic trigger submitted.\n");
}

CCMD(snd_eternity_status)
{
	const char* backend = *snd_backend;
	const bool selected = backend != nullptr && stricmp(backend, "eternity") == 0;

	Printf(PRINT_HIGH, "Eternity spatial audio backend: %s\n", selected ? "selected" : "not selected");
	Printf(PRINT_HIGH, "  snd_backend = %s\n", backend != nullptr && backend[0] != '\0' ? backend : "<empty>");
	Printf(PRINT_HIGH, "  registration attempted = %s\n", gEternityRegistrationAttempted ? "yes" : "no");
	Printf(PRINT_HIGH, "  renderer constructed = %s\n", gEternityRendererConstructed ? "yes" : "no");
	Printf(PRINT_HIGH, "  implementation = silent facade (Eternity mixer not vendored yet)\n");
	Printf(PRINT_HIGH, "  diagnostic triggers = %d\n", gEternityDiagnosticTriggers);
	if (gEternityRenderer != nullptr)
	{
		const FEternitySpatialSubmit& spatial = gEternityRenderer->Spatial;
		Printf(PRINT_HIGH, "  loaded sounds = %d\n", gEternityRenderer->LoadedSounds);
		Printf(PRINT_HIGH, "  2D starts = %d, 3D starts = %d\n", spatial.Count2D, spatial.Count3D);
		Printf(PRINT_HIGH, "  listener updates = %d, 3D param updates = %d, mix ticks = %d\n",
			spatial.ListenerUpdates, spatial.ParamUpdates3D, gEternityRenderer->MixTicks);
		Printf(PRINT_HIGH, "  last spatial submit: pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) vol=%.3f pitch=%.3f channel=%d flags=%d priority=%d area=%d\n",
			spatial.LastPosX, spatial.LastPosY, spatial.LastPosZ,
			spatial.LastVelX, spatial.LastVelY, spatial.LastVelZ,
			spatial.LastVolume, spatial.LastPitch,
			spatial.LastChannel, spatial.LastChannelFlags, spatial.LastPriority,
			spatial.LastAreaSound ? 1 : 0);
	}
	Printf(PRINT_HIGH, "  boundary = client-local audio backend; no snapshots, saves, demos, or authority state\n");
}
