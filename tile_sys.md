# Tile-Based Multi-Threaded Rasterizer Implementation Plan

## Overview
Implement tile-based rendering with multi-threading to improve performance through better cache locality and parallel processing. Development will proceed in phases to validate correctness before adding complexity.

---

## Target Configuration
- **Resolution:** 1280×720
- **Tile Size:** 32×32 pixels (start here, test 16×16 later)
- **Total Tiles:** 920 tiles (40 wide × 23 tall)
- **Thread Count:** 4 threads
- **Tiles per Thread:** ~230 tiles each

---

## Architecture

### Rendering Flow
```
1. Divide screen into 32×32 pixel tiles (920 total)
2. Each render thread:
   - Grabs next available tile from work queue
   - Renders tile to LOCAL framebuffer + depth buffer
   - Writes completed tile to main framebuffer
   - Repeats until all tiles done
3. Frame complete when all 920 tiles rendered
```

### Per-Tile Resources
- **Local Framebuffer:** 32×32×4 bytes = 4KB (color data)
- **Local Depth Buffer:** 32×32×4 bytes = 4KB (depth data)
- **Total per tile:** 8KB (fits in L1 cache)

---

## Development Phases

### Phase 1: Single-Threaded Tiling (Validation)
**Goal:** Prove the tile system works correctly

**Implementation:**
- Divide screen into 32×32 tiles
- Loop through tiles sequentially (top-left → bottom-right)
- For each tile:
  - Test ALL triangles (no optimization yet)
  - Render only pixels within tile bounds
  - Write tile to main framebuffer
- Compare output to non-tiled version (should be identical)

**Success Criteria:**
- Frame renders correctly
- Visual output matches original non-tiled renderer
- No artifacts at tile boundaries

**Key Code Structure:**
```c
void ay_tile_draw_indexed(...)
{
    const uint32_t tileSize = 32;
    const uint32_t tilesX = (fbWidth + tileSize - 1) / tileSize;
    const uint32_t tilesY = (fbHeight + tileSize - 1) / tileSize;
    
    for(uint32_t ty = 0; ty < tilesY; ty++) {
        for(uint32_t tx = 0; tx < tilesX; tx++) {
            uint32_t minX = tx * tileSize;
            uint32_t minY = ty * tileSize;
            uint32_t maxX = min(minX + tileSize, fbWidth);
            uint32_t maxY = min(minY + tileSize, fbHeight);
            
            render_tile(minX, minY, maxX, maxY, ...);
        }
    }
}
```

---

### Phase 2: Triangle Binning (Optimization)
**Goal:** Only test triangles that actually overlap each tile

**Implementation:**
- Pre-process: For each triangle, determine which tiles it overlaps
- Build per-tile lists of relevant triangles
- During rendering: Only test triangles in that tile's list

**Benefits:**
- Skip triangles that don't overlap tile (massive savings)
- Better cache locality (sequential triangle access per tile)

**Success Criteria:**
- Same visual output as Phase 1
- Measure speedup from reduced triangle tests
- Profile to confirm cache improvements

**Algorithm:**
```c
// Pre-process (once per frame)
for each triangle:
    compute triangle bounding box
    for each tile that overlaps bounding box:
        add triangle to tile's list

// Render (per tile)
for each tile:
    for each triangle in tile's list:
        rasterize triangle (clipped to tile bounds)
```

---

### Phase 3: Multi-Threading (Parallelism)
**Goal:** Render tiles in parallel across multiple CPU cores

**Architecture:**
- Work queue containing all 920 tiles
- 4 render threads that grab tiles from queue
- Each thread renders to LOCAL tile buffers (no contention)
- Threads write completed tiles to main framebuffer (with lock)

**Synchronization Strategy:**
```
Option A (Simpler - Start Here):
- Render threads write directly to main FB with mutex
- Single lock for framebuffer writes

Option B (If Option A bottlenecks):
- Dedicated assembler thread
- Render threads push to completed tile queue
- Assembler pulls from queue and writes to main FB
```

**Success Criteria:**
- Same visual output as Phase 2
- Measure speedup (target: 2-3x on 4 cores)
- No race conditions or artifacts
- Proper synchronization at frame boundaries



---

## Performance Expectations

### Phase 1 (Single-threaded tiling):
- **Baseline:** 25ms
- **Expected:** 22-25ms (possible 10% improvement from cache locality)
- **Goal:** Validate correctness

### Phase 2 (Triangle binning):
- **Expected:** 15-20ms (30-40% improvement from skipping irrelevant triangles)
- **Goal:** Prove optimization works

### Phase 3 (Multi-threading, 4 cores):
- **Best case:** 25ms / 4 = 6.25ms (4x speedup)
- **Realistic:** 7-8ms (3-3.5x speedup, accounting for overhead)
- **Components:**
  - Rendering: 25ms / 4 = 6.25ms
  - Synchronization overhead: ~0.5ms
  - Tile assembly/copy: ~1ms
- **Goal:** Achieve 60+ FPS (16.6ms per frame)

---

## Key Design Decisions

### Depth Buffer Strategy:
- **Phase 1-2:** Use full-screen depth buffer (simpler)
- **Phase 3:** Per-tile depth buffers (better cache, no contention)
- **Note:** Don't need to merge depth buffers unless doing post-processing

### Tile Size Testing:
- Start with 32×32 (920 tiles)
- Test 16×16 (3,600 tiles) if needed
- Profile both to find optimal size

### Thread Count:
- Start with 4 threads (match typical quad-core CPU)
- Can make configurable later for different hardware

---

## Testing Strategy

### Visual Validation:
- Take screenshot of non-tiled renderer
- Compare pixel-perfect to tiled renderer at each phase
- Check tile boundaries for artifacts

### Performance Validation:
- Profile each phase independently
- Measure frame time improvement at each step
- Identify bottlenecks before moving to next phase

### Stress Testing:
- Test with complex scenes (many triangles)
- Test with simple scenes (few triangles)
- Verify correct behavior in edge cases

---

## Potential Challenges

### Tile Boundary Issues:
- **Problem:** Artifacts at tile edges
- **Solution:** Ensure bounding box calculations include boundary pixels

### Load Balancing:
- **Problem:** Some tiles have many triangles, others empty
- **Solution:** Dynamic work queue (already planned) handles this naturally

### Synchronization Overhead:
- **Problem:** Lock contention when writing to main framebuffer
- **Solution:** Profile and switch to assembler thread if needed

### Cache Thrashing:
- **Problem:** Tile size too large or too small
- **Solution:** Test 16×16 vs 32×32, profile cache misses

---

## Success Metrics

### Phase 1:
✓ Identical visual output  
✓ No crashes or artifacts  
✓ Clean code structure for Phase 2

### Phase 2:
✓ 30-40% performance improvement  
✓ Reduced triangle tests verified  
✓ Cache improvements measurable

### Phase 3:
✓ 3-3.5x speedup on 4 cores  
✓ Linear scaling with thread count  
✓ Achieve 60+ FPS target

---

## Long-Term Considerations

### Future Optimizations:
- SIMD within tile rendering
- Hierarchical binning (coarse + fine)
- Adaptive tile sizing based on complexity

### Extensibility:
- Keep non-tiled path for debugging
- Make tile size runtime configurable
- Support variable thread counts

---

## Notes

- Keep `ay_draw_indexed()` unchanged as reference implementation
- Build `ay_tile_draw_indexed()` as new entry point
- Maintain ability to switch between tiled and non-tiled for debugging
- Profile extensively at each phase before proceeding
- Document any deviations from plan with rationale

---

## Implementation Checklist

### Phase 1 Tasks:
- [x] Create `ay_tile_draw_indexed()` function
- [x] Implement tile boundary calculation
- [x] Modify pixel loop to clip to tile bounds
- [x] Test with simple scene (sphere)
- [x] Visual comparison with non-tiled
- [x] Profile baseline performance

### Phase 2 Tasks:
- [x] Implement triangle bounding box calculation
- [x] Build tile-to-triangle mapping structure
- [x] Modify renderer to use binned triangles
- [x] Verify no visual regression
- [x] Profile and measure improvement
- [ ] Test with complex scenes

### Phase 3 Tasks:
- [ ] Add win32 thread support -> maybe cross platfrom down the road
- [ ] Implement per-tile local buffers
- [ ] Add synchronization (mutex/critical section)
- [ ] Test thread safety with thread sanitizer
- [ ] Profile multi-threaded performance
- [ ] Tune thread count and tile size
- [ ] Stress test with various workloads