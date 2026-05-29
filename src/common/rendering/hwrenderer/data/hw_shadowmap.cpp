/*
** hw_shadowmap.cpp
**
** 1D dynamic shadow maps (API independent part)
**
**---------------------------------------------------------------------------
**
** Copyright 2017 Magnus Norddahl
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

#include "hw_shadowmap.h"
#include "hw_cvars.h"
#include "hw_dynlightdata.h"
#include "buffers.h"
#include "shaderuniforms.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "hwrenderer/postprocessing/hw_postprocess_cvars.h"
#include "c_dispatch.h"
#include "printf.h"

/*
	The 1D shadow maps are stored in a 1024x1024 texture as float depth values (R32F).

	Each line in the texture is assigned to a single light. For example, to grab depth values for light 20
	the fragment shader (main.fp) needs to sample from row 20. That is, the V texture coordinate needs
	to be 20.5/1024.

	The texel row for each light is split into four parts. One for each direction, like a cube texture,
	but then only in 2D where this reduces itself to a square. When main.fp samples from the shadow map
	it first decides in which direction the fragment is (relative to the light), like cubemap sampling does
	for 3D, but once again just for the 2D case.

	Texels 0-255 is Y positive, 256-511 is X positive, 512-767 is Y negative and 768-1023 is X negative.

	Generating the shadow map itself is done by FShadowMap::Update(). The shadow map texture's FBO is
	bound and then a screen quad is drawn to make a fragment shader cover all texels. For each fragment
	it shoots a ray and collects the distance to what it hit.

	The shadowmap.fp shader knows which light and texel it is processing by mapping gl_FragCoord.y back
	to the light index, and it knows which direction to ray trace by looking at gl_FragCoord.x. For
	example, if gl_FragCoord.y is 20.5, then it knows its processing light 20, and if gl_FragCoord.x is
	127.5, then it knows we are shooting straight ahead for the Y positive direction.

	Ray testing is done by uploading two GPU storage buffers - one holding AABB tree nodes, and one with
	the line segments at the leaf nodes of the tree. The fragment shader then performs a test same way
	as on the CPU, except everything uses indexes as pointers are not allowed in GLSL.
*/

cycle_t IShadowMap::UpdateCycles;
int IShadowMap::LightsProcessed;
int IShadowMap::LightsEligible;
int IShadowMap::LightsShadowmapped;
int IShadowMap::LightsBudgetedOut;
int IShadowMap::LightRowsUpdated;
int IShadowMap::LightPriorityEnabled;
int IShadowMap::BudgetHardCap;
int IShadowMap::BudgetRuntimeCap;
int IShadowMap::BudgetAdaptiveEnabled;

CVAR(Bool, gl_light_shadowmap, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, gl_shadowmap_prioritize, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, hcde_shadow_autofallback, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, hcde_shadow_autobudget, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// HCDE roadmap #17: k8vavoom-style rendering preset scaffold.
//
// These CVARs are intentionally low-blast and do not invent any new rendering
// features by themselves. They provide a stable switchboard that composes
// existing HCDE shadowmap/postprocess settings into a lighting-heavy profile,
// without touching playsim, snapshots, saves, demos, or network state.
//
// Composition is presentation-only: when the profile is enabled it raises the
// floor on a curated set of existing CVARs. Disabling the profile does not
// restore previous values - users who want to revert can reset individual
// CVARs (or use `r_k8vavoom_status` to see what is currently active). This
// keeps the profile a one-shot apply rather than a state machine that has to
// be perfectly symmetric with the rest of the engine.
EXTERN_CVAR(Int, gl_shadowmap_quality)

namespace
{
	struct K8vavoomPresetSnapshot
	{
		bool   applied = false;
		bool   shadowmap = false;
		bool   prioritize = false;
		int    quality = 0;
		bool   bloom = false;
		int    tonemap = 0;
		int    ssao = 0;
		bool   shadow_boost = false;
		bool   raylight_probe = false;
	};

	K8vavoomPresetSnapshot g_K8vavoomPresetState;

	void HCDE_K8vavoomApplyLightingProfile(int profile, bool shadowBoost, bool raylightProbe)
	{
		K8vavoomPresetSnapshot &s = g_K8vavoomPresetState;
		s = K8vavoomPresetSnapshot{};
		s.applied = profile > 0;

		if (profile <= 0)
			return;

		// Baseline preset: enable shadowmaps and prioritization, raise quality
		// floor, and enable a conservative postprocess bundle.
		gl_light_shadowmap = true;
		gl_shadowmap_prioritize = true;

		const int qualityFloor = shadowBoost ? 1024 : 512;
		if (gl_shadowmap_quality < qualityFloor)
			gl_shadowmap_quality = qualityFloor;

		gl_bloom = true;
		if (gl_tonemap == 0)
			gl_tonemap = 1; // Linear -> generic tonemap operator
		if (gl_ssao == 0)
			gl_ssao = 1; // Low-quality SSAO floor

		s.shadowmap     = gl_light_shadowmap;
		s.prioritize    = gl_shadowmap_prioritize;
		s.quality       = gl_shadowmap_quality;
		s.bloom         = gl_bloom;
		s.tonemap       = gl_tonemap;
		s.ssao          = gl_ssao;
		s.shadow_boost  = shadowBoost;
		s.raylight_probe = raylightProbe;

		Printf(PRINT_HIGH,
			"hcde_k8vavoom_lighting_profile=1 applied: shadowmap=%s prioritize=%s quality=%d bloom=%s tonemap=%d ssao=%d boost=%s raylight=%s\n",
			s.shadowmap ? "on" : "off",
			s.prioritize ? "on" : "off",
			s.quality,
			s.bloom ? "on" : "off",
			s.tonemap,
			s.ssao,
			s.shadow_boost ? "on" : "off",
			s.raylight_probe ? "on" : "off");
	}
}

CVAR(Bool, hcde_k8vavoom_shadow_boost, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, hcde_k8vavoom_raylight_probe, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Int, hcde_k8vavoom_lighting_profile, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;

	HCDE_K8vavoomApplyLightingProfile(self, hcde_k8vavoom_shadow_boost, hcde_k8vavoom_raylight_probe);
}

CCMD(r_k8vavoom_status)
{
	const K8vavoomPresetSnapshot &s = g_K8vavoomPresetState;
	Printf(PRINT_HIGH, "k8vavoom lighting profile: %s\n",
		(hcde_k8vavoom_lighting_profile > 0) ? "ENABLED" : "disabled");
	Printf(PRINT_HIGH, "  composed snapshot (last apply): applied=%s\n", s.applied ? "yes" : "no");
	Printf(PRINT_HIGH, "    gl_light_shadowmap        = %s (live=%s)\n", s.shadowmap ? "on" : "off",
		gl_light_shadowmap ? "on" : "off");
	Printf(PRINT_HIGH, "    gl_shadowmap_prioritize   = %s (live=%s)\n", s.prioritize ? "on" : "off",
		gl_shadowmap_prioritize ? "on" : "off");
	Printf(PRINT_HIGH, "    gl_shadowmap_quality      = %d (live=%d)\n", s.quality, *gl_shadowmap_quality);
	Printf(PRINT_HIGH, "    gl_bloom                  = %s (live=%s)\n", s.bloom ? "on" : "off",
		gl_bloom ? "on" : "off");
	Printf(PRINT_HIGH, "    gl_tonemap                = %d (live=%d)\n", s.tonemap, *gl_tonemap);
	Printf(PRINT_HIGH, "    gl_ssao                   = %d (live=%d)\n", s.ssao, *gl_ssao);
	Printf(PRINT_HIGH, "  sub-flags: shadow_boost=%s raylight_probe=%s\n",
		hcde_k8vavoom_shadow_boost ? "on" : "off",
		hcde_k8vavoom_raylight_probe ? "on" : "off");
}

CCMD(r_k8vavoom_reset)
{
	hcde_k8vavoom_lighting_profile = 0;
	hcde_k8vavoom_shadow_boost = false;
	hcde_k8vavoom_raylight_probe = false;

	gl_light_shadowmap = false;
	gl_shadowmap_prioritize = true;
	gl_shadowmap_quality = 512;
	gl_bloom = false;
	gl_tonemap = 0;
	gl_ssao = 0;

	g_K8vavoomPresetState = K8vavoomPresetSnapshot{};
	Printf(PRINT_HIGH, "k8vavoom lighting preset reset to HCDE defaults.\n");
	Printf(PRINT_HIGH, "  reset: profile=0 shadowmap=off prioritize=on quality=512 bloom=off tonemap=0 ssao=0\n");
}

CUSTOM_CVAR(Float, hcde_shadow_autobudget_targetms, 1.20f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0.25f)
		self = 0.25f;
	else if (self > 10.0f)
		self = 10.0f;
}

CUSTOM_CVAR(Int, hcde_shadow_autobudget_minlights, 64, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1024)
		self = 1024;
}

CUSTOM_CVAR(Int, hcde_shadow_autobudget_step, 32, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	else if (self > 256)
		self = 256;
}

CUSTOM_CVAR(Int, gl_shadowmap_maxlights, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1024)
		self = 1024;
}

ADD_STAT(shadowmap)
{
	FString out;
	out.Format("upload=%04.2f ms  lights=%d  eligible=%d  shadowmapped=%d  dropped=%d  rows=%d  cap=%d/%d  adaptive=%s  priority=%s",
		IShadowMap::UpdateCycles.TimeMS(), IShadowMap::LightsProcessed, IShadowMap::LightsEligible,
		IShadowMap::LightsShadowmapped, IShadowMap::LightsBudgetedOut, IShadowMap::LightRowsUpdated,
		IShadowMap::BudgetRuntimeCap, IShadowMap::BudgetHardCap, IShadowMap::BudgetAdaptiveEnabled ? "on" : "off",
		IShadowMap::LightPriorityEnabled ? "on" : "off");
	return out;
}

CUSTOM_CVAR(Int, gl_shadowmap_quality, 512, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	switch (self)
	{
	case 2<<6: // 128
	case 2<<7: // 256
	case 2<<8: // 512
	case 2<<9: // 1024
	case 2<<10: // 2048
	case 2<<11: // 4096
	case 2<<12: // 8192
		break;
	default:
		self = 128;
		break;
	}
}

bool IShadowMap::ShadowTest(const DVector3 &lpos, const DVector3 &pos)
{
	if (mAABBTree && gl_light_shadowmap)
		return mAABBTree->RayTest(lpos, pos) >= 1.0f;
	else
		return true;
}

bool IShadowMap::PerformUpdate()
{
	UpdateCycles.Reset();

	LightsProcessed = 0;
	LightsEligible = 0;
	LightsShadowmapped = 0;
	LightsBudgetedOut = 0;
	LightRowsUpdated = 1;
	LightPriorityEnabled = 0;

	// Note: BudgetHardCap, BudgetRuntimeCap, and BudgetAdaptiveEnabled are NOT reset here.
	// They are configured per-frame in ApplyShadowCapabilityFallbacks based on
	// current device capabilities and adaptive feedback.

	// CollectLights will be null if the calling code decides that shadowmaps are not needed.
	if (CollectLights != nullptr)
	{
		UpdateCycles.Clock();
		UploadAABBTree();
		UploadLights();
		return true;
	}
	return false;
}

void IShadowMap::UploadLights()
{
	if (mLights.Size() != 1024 * 4)
	{
		mLights.Resize(1024 * 4);
	}
	CollectLights();

	if (mLightList == nullptr)
		mLightList = screen->CreateDataBuffer(LIGHTLIST_BINDINGPOINT, true, false);

	mLightList->SetData(sizeof(float) * mLights.Size(), &mLights[0], BufferUsageType::Stream);
}


void IShadowMap::UploadAABBTree()
{
	if (mNewTree)
	{
		mNewTree = false;

		if (!mNodesBuffer)
			mNodesBuffer = screen->CreateDataBuffer(LIGHTNODES_BINDINGPOINT, true, false);
		mNodesBuffer->SetData(mAABBTree->NodesSize(), mAABBTree->Nodes(), BufferUsageType::Static);

		if (!mLinesBuffer)
			mLinesBuffer = screen->CreateDataBuffer(LIGHTLINES_BINDINGPOINT, true, false);
		mLinesBuffer->SetData(mAABBTree->LinesSize(), mAABBTree->Lines(), BufferUsageType::Static);
	}
	else if (mAABBTree->Update())
	{
		mNodesBuffer->SetSubData(mAABBTree->DynamicNodesOffset(), mAABBTree->DynamicNodesSize(), mAABBTree->DynamicNodes());
		mLinesBuffer->SetSubData(mAABBTree->DynamicLinesOffset(), mAABBTree->DynamicLinesSize(), mAABBTree->DynamicLines());
	}
}

void IShadowMap::Reset()
{
	delete mLightList; mLightList = nullptr;
	delete mNodesBuffer; mNodesBuffer = nullptr;
	delete mLinesBuffer; mLinesBuffer = nullptr;
}

IShadowMap::~IShadowMap()
{
	Reset();
}
