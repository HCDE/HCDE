/*
** hw_entrypoint.cpp
**
** manages the rendering of the player's view
**
**---------------------------------------------------------------------------
**
** Copyright 2004-2016 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

#include <algorithm>
#include <vector>

#include "gi.h"
#include "a_dynlight.h"
#include "m_png.h"
#include "doomstat.h"
#include "r_data/r_interpolate.h"
#include "r_utility.h"
#include "d_player.h"
#include "i_time.h"
#include "swrenderer/r_swscene.h"
#include "swrenderer/r_renderer.h"
#include "hw_dynlightdata.h"
#include "hw_clock.h"
#include "flatvertices.h"
#include "v_palette.h"
#include "d_main.h"
#include "g_cvars.h"
#include "v_draw.h"

#include "hw_lightbuffer.h"
#include "hw_bonebuffer.h"
#include "hw_cvars.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "hwrenderer/scene/hw_fakeflat.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hw_vrmodes.h"

EXTERN_CVAR(Bool, cl_capfps)
EXTERN_CVAR(Bool, hcde_shadow_autofallback)
EXTERN_CVAR(Bool, hcde_shadow_autobudget)
EXTERN_CVAR(Int, hcde_shadowprofile)
EXTERN_CVAR(Float, hcde_shadow_autobudget_targetms)
EXTERN_CVAR(Int, hcde_shadow_autobudget_minlights)
EXTERN_CVAR(Int, hcde_shadow_autobudget_step)
extern bool NoInterpolateView;

static SWSceneDrawer *swdrawer;

void CleanSWDrawer()
{
	if (swdrawer) delete swdrawer;
	swdrawer = nullptr;
}

#include "g_levellocals.h"
#include "a_dynlight.h"

namespace
{
	struct FShadowLightCandidate
	{
		FDynamicLight* Light = nullptr;
		double Score = 0.;
	};

	double ShadowLightPriorityScore(FDynamicLight* light, const DVector3& viewPos, int viewPortalGroup)
	{
		// Lower scores win. Radius is squared with distance so larger nearby
		// lights keep useful shadows when the per-frame row budget is capped.
		const double radius = light->GetRadius();
		const double radiusScale = radius > 1.0 ? radius * radius : 1.0;
		const double distanceSq = (light->PosRelative(viewPortalGroup) - viewPos).LengthSquared();
		return distanceSq / radiusScale;
	}

	int ShadowMapQualityCapForTextureLimit(int maxTextureSize)
	{
		if (maxTextureSize >= 8192) return 8192;
		if (maxTextureSize >= 4096) return 4096;
		if (maxTextureSize >= 2048) return 2048;
		if (maxTextureSize >= 1024) return 1024;
		if (maxTextureSize >= 512) return 512;
		if (maxTextureSize >= 256) return 256;
		return 128;
	}

	int ShadowMapBudgetCapForQuality(int quality, int backend)
	{
		int cap = 64;
		if (quality >= 4096) cap = 1024;
		else if (quality >= 2048) cap = 768;
		else if (quality >= 1024) cap = 512;
		else if (quality >= 512) cap = 256;
		else if (quality >= 256) cap = 128;

		// GLES targets are usually tighter on fill/bandwidth; keep a conservative
		// cap unless users explicitly disable auto-fallback.
		if (backend == BACKEND_OPENGLES)
			cap = std::min(cap, 256);

		return cap;
	}

	struct FShadowProfileOverrideState
	{
		const level_info_t* ActiveMap = nullptr;
		int PreviousProfile = -1;
	};

	struct FShadowAutoBudgetState
	{
		bool Active = false;
		int RuntimeCap = -1;
		int LastHardCap = -1;
	};

	int gShadowRuntimeBudgetCap = 1024;

	void ApplyMapShadowProfileOverride(const level_info_t* levelInfo)
	{
		const int requestedProfile = levelInfo != nullptr ? levelInfo->HcdeShadowProfileOverride : -1;
		static FShadowProfileOverrideState overrideState;

		if (requestedProfile < 0)
		{
			if (overrideState.ActiveMap != nullptr)
			{
				if (overrideState.PreviousProfile >= 0 && hcde_shadowprofile != overrideState.PreviousProfile)
				{
					hcde_shadowprofile = overrideState.PreviousProfile;
				}
				overrideState.ActiveMap = nullptr;
				overrideState.PreviousProfile = -1;
			}
			return;
		}

		if (overrideState.ActiveMap == nullptr)
		{
			overrideState.PreviousProfile = hcde_shadowprofile;
		}
		overrideState.ActiveMap = levelInfo;
		if (hcde_shadowprofile != requestedProfile)
		{
			hcde_shadowprofile = requestedProfile;
		}
	}

	void ApplyShadowCapabilityFallbacks(FLevelLocals* level)
	{
		const level_info_t* levelInfo = level != nullptr ? level->info : nullptr;
		ApplyMapShadowProfileOverride(levelInfo);

		IShadowMap::BudgetHardCap = 0;
		IShadowMap::BudgetRuntimeCap = 0;
		IShadowMap::BudgetAdaptiveEnabled = 0;
		gShadowRuntimeBudgetCap = 0;

		if (screen == nullptr)
			return;

		// If SSBO shadowmap support is unavailable, RenderViewpoint's existing
		// guard will skip shadowmap updates safely. Keep user CVARs untouched.
		if (!screen->allowSSBO() || (screen->hwcaps & RFL_SHADER_STORAGE_BUFFER) == 0)
			return;

		int qualityCap = 8192;
		if (hcde_shadow_autofallback)
		{
			int textureLimit = screen->MaxShadowMapTextureSize();
			if (textureLimit <= 0)
				textureLimit = 4096;

			qualityCap = ShadowMapQualityCapForTextureLimit(textureLimit);
			if (screen->Backend() == BACKEND_OPENGLES && qualityCap > 1024)
				qualityCap = 1024;
		}

		if (levelInfo != nullptr && levelInfo->HcdeShadowQualityCap > 0)
		{
			qualityCap = std::min(qualityCap, levelInfo->HcdeShadowQualityCap);
		}

		if (gl_shadowmap_quality > qualityCap)
			gl_shadowmap_quality = qualityCap;

		int budgetCap = 1024;
		if (hcde_shadow_autofallback)
		{
			budgetCap = ShadowMapBudgetCapForQuality(gl_shadowmap_quality, screen->Backend());
		}

		if (levelInfo != nullptr && levelInfo->HcdeShadowMaxLightsCap >= 0)
		{
			budgetCap = std::min(budgetCap, levelInfo->HcdeShadowMaxLightsCap);
		}

		if (budgetCap < 0) budgetCap = 0;
		else if (budgetCap > 1024) budgetCap = 1024;

		if (gl_shadowmap_maxlights > budgetCap)
			gl_shadowmap_maxlights = budgetCap;

		int runtimeBudgetCap = budgetCap;
		static FShadowAutoBudgetState autoBudgetState;

		// HCDE Shadow Auto-Budgeting: Adapt the number of shadowmapped lights
		// based on the time it took to upload the shadowmap and AABB tree in the
		// previous frame. This prevents performance tanking in light-heavy areas.
		if (hcde_shadow_autobudget && gl_light_shadowmap)
		{
			int userBudget = gl_shadowmap_maxlights;
			if (userBudget < 0) userBudget = 0;
			else if (userBudget > 1024) userBudget = 1024;

			// The ceiling is the minimum of the user's manual cap and the device/quality hard cap.
			int ceiling = std::min(userBudget, budgetCap);
			int minBudget = hcde_shadow_autobudget_minlights;
			if (minBudget < 0) minBudget = 0;
			else if (minBudget > 1024) minBudget = 1024;
			if (minBudget > ceiling) minBudget = ceiling;

			if (!autoBudgetState.Active || autoBudgetState.LastHardCap != budgetCap || autoBudgetState.RuntimeCap < 0)
			{
				// On first activation (or hard-cap changes), start from the current
				// allowed ceiling and then adapt from there.
				autoBudgetState.RuntimeCap = ceiling;
			}
			autoBudgetState.Active = true;
			autoBudgetState.LastHardCap = budgetCap;

			int step = hcde_shadow_autobudget_step;
			if (step < 1) step = 1;
			else if (step > 256) step = 256;

			// Measure previous frame's shadowmap processing time.
			const double lastUploadMs = IShadowMap::UpdateCycles.TimeMS();
			const double targetMs = std::max(0.25, double(hcde_shadow_autobudget_targetms));

			// Use a small deadband around the target to avoid per-frame oscillation.
			const double highWater = targetMs * 1.20;
			const double lowWater = targetMs * 0.70;

			if (lastUploadMs > highWater && autoBudgetState.RuntimeCap > minBudget)
			{
				// Performance is below target; reduce light count.
				autoBudgetState.RuntimeCap = std::max(minBudget, autoBudgetState.RuntimeCap - step);
			}
			else if (lastUploadMs < lowWater && autoBudgetState.RuntimeCap < ceiling)
			{
				// Performance is above target; we can afford more shadows.
				autoBudgetState.RuntimeCap = std::min(ceiling, autoBudgetState.RuntimeCap + step);
			}

			if (autoBudgetState.RuntimeCap < minBudget) autoBudgetState.RuntimeCap = minBudget;
			else if (autoBudgetState.RuntimeCap > ceiling) autoBudgetState.RuntimeCap = ceiling;

			runtimeBudgetCap = autoBudgetState.RuntimeCap;
		}
		else
		{
			autoBudgetState.Active = false;
			autoBudgetState.RuntimeCap = -1;
			autoBudgetState.LastHardCap = -1;

			// Shadowmap passes are disabled; report zero active runtime rows while
			// keeping the hard cap visible for diagnostics.
			if (!gl_light_shadowmap)
			{
				runtimeBudgetCap = 0;
			}
		}

		gShadowRuntimeBudgetCap = runtimeBudgetCap;
		IShadowMap::BudgetHardCap = budgetCap;
		IShadowMap::BudgetRuntimeCap = runtimeBudgetCap;
		IShadowMap::BudgetAdaptiveEnabled = (hcde_shadow_autobudget && gl_light_shadowmap) ? 1 : 0;
	}
}

void CollectLights(FLevelLocals* Level, const DVector3& viewPos, int viewPortalGroup)
{
	IShadowMap* sm = &screen->mShadowMap;
	int lightindex = 0;
	// Respect both the manual gl_shadowmap_maxlights and the dynamic runtime budget.
	int shadowLightLimit = std::min(int(gl_shadowmap_maxlights), gShadowRuntimeBudgetCap);

	if (shadowLightLimit < 0)
	{
		shadowLightLimit = 0;
	}
	else if (shadowLightLimit > 1024)
	{
		shadowLightLimit = 1024;
	}

	std::vector<FShadowLightCandidate> candidates;
	for (auto light = Level->lights; light; light = light->next)
	{
		IShadowMap::LightsProcessed++;
		light->mShadowmapIndex = 1024; // Default to 'no shadowmap' index

		if (light->shadowmapped && light->IsActive())
		{
			// Candidate lights are scored based on proximity and screen coverage.
			candidates.push_back({ light, ShadowLightPriorityScore(light, viewPos, viewPortalGroup) });
		}
	}

	IShadowMap::LightsEligible = int(candidates.size());
	IShadowMap::LightPriorityEnabled = gl_shadowmap_prioritize ? 1 : 0;

	// Sort lights so the most important ones are assigned indices first.
	if (gl_shadowmap_prioritize)
	{
		std::stable_sort(candidates.begin(), candidates.end(), [](const FShadowLightCandidate& left, const FShadowLightCandidate& right)
		{
			return left.Score < right.Score;
		});
	}

	for (const auto& candidate : candidates)
	{
		if (lightindex >= shadowLightLimit)
		{
			break;
		}

		auto light = candidate.Light;
		IShadowMap::LightsShadowmapped++;
		light->mShadowmapIndex = lightindex;
		// Upload light parameters to the shadowmap list.
		sm->SetLight(lightindex, (float)light->X(), (float)light->Y(), (float)light->Z(), light->GetRadius());
		lightindex++;
	}

	IShadowMap::LightsBudgetedOut = int(candidates.size()) - lightindex;
	if (IShadowMap::LightsBudgetedOut < 0)
	{
		IShadowMap::LightsBudgetedOut = 0;
	}

	// Update the number of active rows to process in the shadowmap shader.
	sm->SetActiveLightRows(lightindex);

	// Clear remaining light slots.
	for (; lightindex < 1024; lightindex++)
	{
		sm->SetLight(lightindex, 0, 0, 0, 0);
	}
}


//-----------------------------------------------------------------------------
//
// Renders one viewpoint in a scene
//
//-----------------------------------------------------------------------------

sector_t* RenderViewpoint(FRenderViewpoint& mainvp, AActor* camera, IntRect* bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen)
{
	auto& RenderState = *screen->RenderState();

	R_SetupFrame(mainvp, r_viewwindow, camera);

	if (mainview && toscreen)
	{
		ApplyShadowCapabilityFallbacks(camera != nullptr ? camera->Level : nullptr);
	}

	if (mainview && toscreen && !(camera->Level->flags3 & LEVEL3_NOSHADOWMAP) && camera->Level->HasDynamicLights && gl_light_shadowmap && screen->allowSSBO() && (screen->hwcaps & RFL_SHADER_STORAGE_BUFFER))
	{
		screen->SetAABBTree(camera->Level->aabbTree);
		const DVector3 viewPos = mainvp.Pos;
		const int viewPortalGroup = mainvp.sector != nullptr ? mainvp.sector->PortalGroup : 0;
		screen->mShadowMap.SetCollectLights([=] {
			CollectLights(camera->Level, viewPos, viewPortalGroup);
		});
		screen->UpdateShadowMap();
	}
	else
	{
		// null all references to the level if we do not need a shadowmap. This will shortcut all internal calculations without further checks.
		screen->SetAABBTree(nullptr);
		screen->mShadowMap.SetCollectLights(nullptr);
	}

	screen->SetLevelMesh(camera->Level->levelMesh);

	// Update the attenuation flag of all light defaults for each viewpoint.
	// This function will only do something if the setting differs.
	FLightDefaults::SetAttenuationForLevel(!!(camera->Level->flags3 & LEVEL3_ATTENUATE));

	// Render (potentially) multiple views for stereo 3d
	// Fixme. The view offsetting should be done with a static table and not require setup of the entire render state for the mode.
	auto vrmode = VRMode::GetVRMode(mainview && toscreen);
	const int eyeCount = vrmode->mEyeCount;
	screen->FirstEye();
	for (int eye_ix = 0; eye_ix < eyeCount; ++eye_ix)
	{
		const auto& eye = vrmode->mEyes[eye_ix];
		screen->SetViewportRects(bounds);

		if (mainview) // Bind the scene frame buffer and turn on draw buffers used by ssao
		{
			bool useSSAO = (gl_ssao != 0);
			screen->SetSceneRenderTarget(useSSAO);
			RenderState.SetPassType(useSSAO ? GBUFFER_PASS : NORMAL_PASS);
			RenderState.EnableDrawBuffers(RenderState.GetPassDrawBufferCount(), true);
		}

		auto di = HWDrawInfo::StartDrawInfo(mainvp.ViewLevel, nullptr, mainvp, nullptr);
		auto& vp = di->Viewpoint;

		di->Set3DViewport(RenderState);
		di->SetViewArea();
		auto cm = di->SetFullbrightFlags(mainview ? vp.camera->player : nullptr);
		float flash = 1.f;

		// Only used by the GLES2 renderer
		RenderState.SetSpecialColormap(cm, flash);

		di->Viewpoint.FieldOfView = DAngle::fromDeg(fov);	// Set the real FOV for the current scene (it's not necessarily the same as the global setting in r_viewpoint)

		// Stereo mode specific perspective projection
		float inv_iso_dist = 1.0f;
		bool iso_ortho = (camera->ViewPos != NULL) && (camera->ViewPos->Flags & VPSF_ORTHOGRAPHIC);
		if (iso_ortho && (camera->ViewPos->Offset.Length() > 0)) inv_iso_dist = 1.0/camera->ViewPos->Offset.Length();
		di->VPUniforms.mProjectionMatrix = eye.GetProjection(fov, ratio, fovratio * inv_iso_dist, iso_ortho);
		di->ProjectionMatrix2 = eye.GetProjection(fov, ratio, fovratio, false); // Regular ol' perspective projection matrix

		// Stereo mode specific viewpoint adjustment
		vp.Pos += eye.GetViewShift(vp.HWAngles.Yaw.Degrees());
		di->SetupView(RenderState, vp.Pos.X, vp.Pos.Y, vp.Pos.Z, false, false);

		di->ProcessScene(toscreen);

		if (mainview)
		{
			PostProcess.Clock();
			if (toscreen) di->EndDrawScene(mainvp.sector, RenderState); // do not call this for camera textures.

			if (RenderState.GetPassType() == GBUFFER_PASS) // Turn off ssao draw buffers
			{
				RenderState.SetPassType(NORMAL_PASS);
				RenderState.EnableDrawBuffers(1);
			}

			screen->PostProcessScene(false, cm, flash, [&]() { di->DrawEndScene2D(mainvp.sector, RenderState); });
			PostProcess.Unclock();
		}
		// Reset colormap so 2D drawing isn't affected
		RenderState.SetSpecialColormap(CM_DEFAULT, 1);

		di->EndDrawInfo();
		if (eyeCount - eye_ix > 1)
			screen->NextEye(eyeCount);
	}

	return mainvp.sector;
}

void DoWriteSavePic(FileWriter* file, ESSType ssformat, uint8_t* scr, int width, int height, sector_t* viewsector, bool upsidedown)
{
	PalEntry palette[256];
	PalEntry modulateColor;
	auto blend = V_CalcBlend(viewsector, &modulateColor);
	int pixelsize = 1;
	// Apply the screen blend, because the renderer does not provide this.
	if (ssformat == SS_RGB)
	{
		int numbytes = width * height * 3;
		pixelsize = 3;
		if (modulateColor != 0xffffffff)
		{
			float r = modulateColor.r / 255.f;
			float g = modulateColor.g / 255.f;
			float b = modulateColor.b / 255.f;
			for (int i = 0; i < numbytes; i += 3)
			{
				scr[i] = uint8_t(scr[i] * r);
				scr[i + 1] = uint8_t(scr[i + 1] * g);
				scr[i + 2] = uint8_t(scr[i + 2] * b);
			}
		}
		float iblendfac = 1.f - blend.W;
		blend.X *= blend.W;
		blend.Y *= blend.W;
		blend.Z *= blend.W;
		for (int i = 0; i < numbytes; i += 3)
		{
			scr[i] = uint8_t(scr[i] * iblendfac + blend.X);
			scr[i + 1] = uint8_t(scr[i + 1] * iblendfac + blend.Y);
			scr[i + 2] = uint8_t(scr[i + 2] * iblendfac + blend.Z);
		}
	}
	else
	{
		// Apply the screen blend to the palette. The colormap related parts get skipped here because these are already part of the image.
		DoBlending(GPalette.BaseColors, palette, 256, uint8_t(blend.X), uint8_t(blend.Y), uint8_t(blend.Z), uint8_t(blend.W * 255));
	}

	int pitch = width * pixelsize;
	if (upsidedown)
	{
		scr += ((height - 1) * width * pixelsize);
		pitch *= -1;
	}

	M_CreatePNG(file, scr, ssformat == SS_PAL ? palette : nullptr, ssformat, width, height, pitch, vid_gamma);
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void WriteSavePic(player_t* player, FileWriter* file, int width, int height)
{
	if (!V_IsHardwareRenderer())
	{
		SWRenderer->WriteSavePic(player, file, width, height);
	}
	else
	{
		IntRect bounds;
		bounds.left = 0;
		bounds.top = 0;
		bounds.width = width;
		bounds.height = height;
		auto& RenderState = *screen->RenderState();

		// we must be sure the GPU finished reading from the buffer before we fill it with new data.
		screen->WaitForCommands(false);

		// Switch to render buffers dimensioned for the savepic
		screen->SetSaveBuffers(true);
		screen->ImageTransitionScene(true);

		hw_postprocess.SetTonemapMode(level.info ? level.info->tonemap : ETonemapMode::None);
		hw_ClearFakeFlat();
		screen->mVertexData->Reset();
		RenderState.SetVertexBuffer(screen->mVertexData);
		screen->mLights->Clear();
		screen->mBones->Clear();
		screen->mViewpoints->Clear();

		// This shouldn't overwrite the global viewpoint even for a short time.
		FRenderViewpoint savevp;
		sector_t* viewsector = RenderViewpoint(savevp, players[consoleplayer].camera, &bounds, r_viewpoint.FieldOfView.Degrees(), 1.6f, 1.6f, true, false);
		RenderState.EnableStencil(false);
		RenderState.SetNoSoftLightLevel();

		TArray<uint8_t> scr(width * height * 3, true);
		screen->CopyScreenToBuffer(width, height, scr.Data());

		DoWriteSavePic(file, SS_RGB, scr.Data(), width, height, viewsector, screen->FlipSavePic());

		// Switch back the screen render buffers
		screen->SetViewportRects(nullptr);
		screen->SetSaveBuffers(false);
	}
}

//===========================================================================
//
// Renders the main view
//
//===========================================================================

static void CheckTimer(FRenderState &state, uint64_t ShaderStartTime)
{
	// if firstFrame is not yet initialized, initialize it to current time
	// if we're going to overflow a float (after ~4.6 hours, or 24 bits), re-init to regain precision
	if ((state.firstFrame == 0) || (screen->FrameTime - state.firstFrame >= 1 << 24) || ShaderStartTime >= state.firstFrame)
		state.firstFrame = screen->FrameTime - 1;
}


sector_t* RenderView(player_t* player)
{
	auto RenderState = screen->RenderState();
	RenderState->SetVertexBuffer(screen->mVertexData);
	screen->mVertexData->Reset();
	hw_postprocess.SetTonemapMode(level.info ? level.info->tonemap : ETonemapMode::None);

	if (level.flags3 & LEVEL3_NOAMBIENTOCCLUSION)
	{
		hw_postprocess.SetNoAmbientOcclusion();
	}

	sector_t* retsec;
	if (!V_IsHardwareRenderer())
	{
		screen->SetActiveRenderTarget();	// only relevant for Vulkan

		if (!swdrawer) swdrawer = new SWSceneDrawer;
		retsec = swdrawer->RenderView(player);
	}
	else
	{
		hw_ClearFakeFlat();

		iter_dlightf = iter_dlight = draw_dlight = draw_dlightf = 0;

		checkBenchActive();

		// reset statistics counters
		ResetProfilingData();

		// Get this before everything else
		if (cl_capfps || r_NoInterpolate) r_viewpoint.TicFrac = 1.;
		else r_viewpoint.TicFrac = I_GetTimeFrac();

		screen->mLights->Clear();
		screen->mBones->Clear();
		screen->mViewpoints->Clear();

		// NoInterpolateView should have no bearing on camera textures, but needs to be preserved for the main view below.
		bool saved_niv = NoInterpolateView;
		NoInterpolateView = false;

		// Shader start time does not need to be handled per level. Just use the one from the camera to render from.
		if (player->camera)
			CheckTimer(*RenderState, player->camera->Level->ShaderStartTime);

		// Draw all canvases that changed
		for (FCanvas* canvas : AllCanvases)
		{
			if (canvas->Tex && canvas->Tex->CheckNeedsUpdate())
			{
				screen->RenderTextureView(canvas->Tex, [=](IntRect& bounds)
					{
						screen->SetViewportRects(&bounds);
						Draw2D(&canvas->Drawer, *screen->RenderState(), 0, 0, canvas->Tex->GetWidth(), canvas->Tex->GetHeight());
						canvas->Drawer.Clear();
					});
				canvas->Tex->SetUpdated(true);
			}
		}

		// prepare all camera textures that have been used in the last frame.
		// This must be done for all levels, not just the primary one!
		for (auto Level : AllLevels())
		{
			Level->canvasTextureInfo.UpdateAll([&](AActor* camera, FCanvasTexture* camtex, double fov)
				{
					screen->RenderTextureView(camtex, [=](IntRect& bounds)
						{
							FRenderViewpoint texvp;
							float ratio = camtex->aspectRatio / Level->info->pixelstretch;
							RenderViewpoint(texvp, camera, &bounds, fov, ratio, ratio, false, false);
						});
				});
		}
		NoInterpolateView = saved_niv;

		// now render the main view
		float fovratio;
		float ratio = r_viewwindow.WidescreenRatio;
		if (r_viewwindow.WidescreenRatio >= 1.3f)
		{
			fovratio = 1.333333f;
		}
		else
		{
			fovratio = ratio;
		}

		screen->ImageTransitionScene(true); // Only relevant for Vulkan.

		retsec = RenderViewpoint(r_viewpoint, player->camera, NULL, r_viewpoint.FieldOfView.Degrees(), ratio, fovratio, true, true);
	}
	All.Unclock();
	return retsec;
}
