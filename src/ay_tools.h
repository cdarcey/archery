#ifndef AY_TOOLS_H
#define AY_TOOLS_H

#include <string.h> // strncpy
#include <stdio.h>  // printf
#include <stdlib.h> // malloc
#include "glfw3.h"

typedef struct _ayProfileInfo
{
    char     pcName[64];
    double   dStartTime;
    double   dEndTime;
    double   dTotalTime;
    double   dDuration;
    int      iCallCount;
    double*  uCircularBuffer;
    uint32_t uBufferSize;
} ayProfileInfo;

typedef enum _ayProfilerTypes
{
    PROFILE_FRAME_TIME,
    PROFILE_RASTERIZE,
    PROFILE_VERTEX_SHADER,
    PROFILE_PIXEL_SHADER,
    PROFILE_CLEAR,
    PROFILE_SET_PIXEL,
    PROFILE_VARYING,
    PROFILE_EDGE_FUNCTION,
    PROFILE_COUNT
} ayProfilerTypes;

typedef enum _ayProfilerMeasurement
{
    AY_PROFILE_MILLISECONDS,
    AY_PROFILE_MICROSECONDS,
    AY_PROFILE_NANOSECONDS,
    AY_PROFILE_SECONDS
} ayProfilerMeasurement;

#ifdef AY_PROFILER_IMPLEMENTATION
ayProfileInfo* g_profilers[PROFILE_COUNT] = {0}; 
#endif

static inline ayProfileInfo*
ay_init_new_profiler(uint32_t uBufferSize)
{
    ayProfileInfo* tNewProfileInfo = malloc(sizeof(ayProfileInfo));
    if(!tNewProfileInfo) return NULL;
    memset(tNewProfileInfo, 0, sizeof(ayProfileInfo));

    tNewProfileInfo->uCircularBuffer = malloc(uBufferSize * sizeof(double));
    tNewProfileInfo->uBufferSize = uBufferSize; // store in case we need again later

    return tNewProfileInfo;
}

static inline void
ay_free_profiler(ayProfileInfo* tProfiler)
{
    if(!tProfiler) return;
    free(tProfiler->uCircularBuffer);
    free(tProfiler);
}

static inline void 
ay_start_profile(ayProfileInfo* tInfo, const char* pcName)
{
    tInfo->dStartTime = glfwGetTime();
    strncpy(tInfo->pcName, pcName, sizeof(tInfo->pcName) - 1);
    tInfo->pcName[sizeof(tInfo->pcName) - 1] = '\0';  // null terminate
    tInfo->iCallCount++;
    tInfo->dTotalTime = 0.0;
}

static inline void 
ay_end_profile(ayProfileInfo* tProfiler)
{
    tProfiler->dEndTime = glfwGetTime();
    tProfiler->dDuration = (tProfiler->dEndTime - tProfiler->dStartTime) * 1000;
    tProfiler->dTotalTime += tProfiler->dDuration;
    if(tProfiler->iCallCount  >= tProfiler->uBufferSize + 1) // call count is 1 on first call and buffer is 0 index
    {
        tProfiler->iCallCount = 0;
    }
    tProfiler->uCircularBuffer[tProfiler->iCallCount] = tProfiler->dDuration;
}

static inline void
ay_print_profiler_results(ayProfileInfo* tProfiler, ayProfilerMeasurement tTimeOutput)
{
    if(!tProfiler) return;

    double dTotalTime = 0.0;
    for(uint32_t i = 0; i < tProfiler->uBufferSize; i++)
    {
        dTotalTime += tProfiler->uCircularBuffer[i];
    }
    double dAverageTime = dTotalTime / tProfiler->uBufferSize;

    const char* unit;
    double scaledTime = dAverageTime;

    switch(tTimeOutput) 
    {
        case AY_PROFILE_SECONDS:      scaledTime  = 1000.0;    unit = " sec";      break;
        case AY_PROFILE_MILLISECONDS: unit        = " mill sec"; break;
        case AY_PROFILE_MICROSECONDS: scaledTime *= 1000.0;    unit = " micro sec"; break;
        case AY_PROFILE_NANOSECONDS:  scaledTime *= 1000000.0; unit = " nano sec";  break;
    }
    
    printf("%s: avg = %.2f%s\n", tProfiler->pcName, scaledTime, unit);
}


#endif