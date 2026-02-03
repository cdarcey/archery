#ifndef AY_RASTERIZE_PROFILE_H
#define AY_RASTERIZE_PROFILE_H


#include <glfw3.h>

#define DRAW_IND_SAMPLES 120

typedef struct _ayDrawIndProfiler
{
    double   dDrawCall[DRAW_IND_SAMPLES];
    double   dFrameOverHead[DRAW_IND_SAMPLES];
    double   dVertexShader[DRAW_IND_SAMPLES];
    double   dTriangleSetup[DRAW_IND_SAMPLES];
    double   dPixelLoop[DRAW_IND_SAMPLES];
    double   dVaryingSystem[DRAW_IND_SAMPLES];
    double   dDepthTest[DRAW_IND_SAMPLES];
    double   dFragmentShader[DRAW_IND_SAMPLES];
    double   dLoopOverhead[DRAW_IND_SAMPLES];
    uint32_t uCurrentFrame;
} ayDrawIndProfiler;

#ifdef AY_RASTERIZE_PROFILE_ENABLED
    extern ayDrawIndProfiler g_raster_profiler;
    #define PROFILE_START(name) double profile_##name = glfwGetTime()
    #define PROFILE_END(name) g_raster_profiler.d##name[g_raster_profiler.uCurrentFrame] += (glfwGetTime() - profile_##name) * 1000
#else
    #define PROFILE_START(name)
    #define PROFILE_END(name)
#endif

#endif