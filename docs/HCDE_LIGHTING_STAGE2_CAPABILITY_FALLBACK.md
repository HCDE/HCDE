# HCDE Lighting Stage 2 - Capability-Based Shadow Fallback

Date: 2026-05-19

Stage 2 adds an automatic capability/fallback layer for dynamic light
shadowmaps so users do not have to hand-tune quality and budgets per machine.

## What Changed

- Added per-backend max shadowmap texture size reporting via framebuffer APIs:
  - OpenGL: `gl.max_texturesize`
  - OpenGLES: `gles.max_texturesize`
  - Vulkan: `maxImageDimension2D`
- Added `hcde_shadow_autofallback` (default `true`) as a global archive CVAR.
- Added runtime fallback logic in the hardware render entrypoint:
  - If shadowmap SSBO capability is unavailable, shadowmap update work is
    skipped through the existing runtime guard (no forced CVAR mutation).
  - If capability is available, `gl_shadowmap_quality` is clamped to what the
    current backend/device can support.
  - `gl_shadowmap_maxlights` is clamped to a conservative budget tier derived
    from the active quality and backend.

## Conservative Budget Tiers

Quality-to-budget cap:

- `4096 -> 1024`
- `2048 -> 768`
- `1024 -> 512`
- `512 -> 256`
- `256 -> 128`
- `128 -> 64`

OpenGLES applies an additional cap of `256` lights for safety.

## Files Updated

- `HCDE/src/common/rendering/v_video.h`
- `HCDE/src/common/rendering/gl/gl_framebuffer.h`
- `HCDE/src/common/rendering/gl/gl_framebuffer.cpp`
- `HCDE/src/common/rendering/gles/gles_framebuffer.h`
- `HCDE/src/common/rendering/gles/gles_framebuffer.cpp`
- `HCDE/src/common/rendering/vulkan/system/vk_renderdevice.h`
- `HCDE/src/common/rendering/vulkan/system/vk_renderdevice.cpp`
- `HCDE/src/common/rendering/hwrenderer/data/hw_shadowmap.cpp`
- `HCDE/src/rendering/hwrenderer/hw_entrypoint.cpp`

