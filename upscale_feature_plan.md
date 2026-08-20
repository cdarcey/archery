# Upscale Feature Plan

## Goal
Render at a lower internal resolution to cut pixel shader cost, then upscale to
the real output/window resolution as a final blit pass. Optional, mirrors the
existing tile-rendering on/off pattern.

## New struct

```c
typedef struct _ayUpscaleInfo
{
    uint32_t uOutputWidth;
    uint32_t uOutputHeight;
} ayUpscaleInfo;
```

## API changes

```c
ayGraphicsData* ay_initialize_graphics(uint32_t uRenderWidth, uint32_t uRenderHeight,
                                        bool bTileRender, ayUpscaleInfo* ptUpscaleInfo);

void ay_present_frame(ayGraphicsData* ptData, ayWindow* ptWindow);
```

- `uRenderWidth` / `uRenderHeight` become the low-res rasterization target.
  Drives everything internal: tile bounds, triangle bounding boxes,
  `ay_ndc_to_screen`, etc. Unchanged logic, just smaller numbers.
- `ptUpscaleInfo == NULL` disables upscaling entirely (default/current
  behavior). Non-NULL enables it and specifies the real output resolution.
- `ay_present_frame` drops the `ayFrameBufferData*` arg since `ptData`
  already owns `ptFrameBufferData` via `ay_bind_frame_buffer`.

## ayGraphicsData additions

```c
bool                bUpscaling;
ayUpscaleInfo*      ptUpscaleInfo;
ayFrameBufferData*  ptOutputFrameBuffer; // allocated at output res, owned internally
```

## Setup flow (`ay_initialize_graphics`)

1. Store `uRenderWidth`/`uRenderHeight` as `uScreenWidth`/`uScreenHeight`
   (unchanged field, now means render res, not window res).
2. If `ptUpscaleInfo` is non-NULL:
   - `bUpscaling = true`
   - copy/store `ayUpscaleInfo`
   - allocate `ptOutputFrameBuffer` via `ay_initialize_frame_buffer` at
     `uOutputWidth` x `uOutputHeight`
3. If NULL: `bUpscaling = false`, `ptOutputFrameBuffer = NULL`.

## Per-frame flow (unchanged in `main.c` except present call)

1. `ay_clear_frame_buffer(ptFrameBuffer)` — clears the render-res buffer, same as today.
2. `ay_bind_frame_buffer(ptData, ptFrameBuffer)` — unchanged.
3. Draw calls — fully unaware upscaling exists. Tiling, if enabled, operates
   on render-res bounds as it does today.
4. `ay_present_frame(ptData, ptWindow)`:
   - if `bUpscaling`: run upscale pass, `ptData->ptFrameBufferData` (render-res)
     → `ptData->ptOutputFrameBuffer` (output-res)
   - GL upload/blit uses `ptOutputFrameBuffer` if upscaling, else
     `ptFrameBufferData` directly (current behavior)

## New internal function

```c
static void ay_upscale_frame_buffer(ayFrameBufferData* ptLowRes, ayFrameBufferData* ptOutput);
```

- Iterates every pixel of `ptOutput` (output-res).
- For each output pixel, compute a UV in [0,1] based on output pixel position,
  sample `ptLowRes` (wrapped as an `ayTexture`) with `ay_sample_texture_bilinear`.
- Write directly into `ptOutput->auData`, bypassing `ay_set_pixel`'s bounds
  checks (loop is already bounded by design).
- No aspect ratio validation — render res and output res can be any values;
  mismatched ARs just stretch. Matches existing "user handles their data"
  philosophy.

## Aspect ratio stance
Decided: no validation, no letterboxing. If render res and output res have
different aspect ratios, the image stretches. User's responsibility, same as
Vulkan not stopping you from doing something dumb.

## Cleanup (`ay_destroy_graphics`)
Free `ptOutputFrameBuffer` (`auData`, depth buffer if any, struct itself) when
`bUpscaling` is true, alongside existing tile-rendering cleanup.

## Open items to settle during implementation
- Where exactly `ay_upscale_frame_buffer` lives (internal API section, near
  `ay_set_pixel`).
- Precomputing per-column/per-row UV tables for the upscale loop
  (perf optimization already discussed for the sampler itself).
- Whether tiling could eventually apply to the upscale pass too (not needed
  at current scale, revisit if upscale becomes a bottleneck).
