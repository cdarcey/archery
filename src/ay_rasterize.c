/*
   ay_rasterize.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] macros
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ay_rasterize.h"
#include "ay_threading.h"
#define AY_RASTERIZE_PROFILE_ENABLED
#include "ay_rasterize_profile.h"

#include "stb_image_write.h"
#include "stb_image.h"

#ifdef AY_RASTERIZE_PROFILE_ENABLED
ayDrawIndProfiler g_raster_profiler = {0};
#endif

//-----------------------------------------------------------------------------
// [SECTION] macros
//-----------------------------------------------------------------------------

#define THREAD_COUNT 8
#define AY_MAX_TILE_SIZE 32
#define AY_TILE_TRIANGLE_CAPACITY 100
#define AY_MAX_TILE_TRIANGLE_CAPACITY 2048

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _ayTransformedVertex
{
    ayVec3        tScreenPos;
    ayVaryingData tVaryings;
} ayTransformedVertex;

typedef struct _ayTileRenderer
{
    uint32_t uTileSize;
    uint32_t uTilesX;
    uint32_t uTilesY;
    uint32_t uTotalTiles;
    uint32_t uFrameBufferWidth; 
    uint32_t uFrameBufferHeight; 
} ayTileRenderer;

typedef struct _ayTileBins
{
    uint32_t* uTriangleIndices;  // flat array of all triangle indices
    uint32_t* uCounts;           // triangles per tile
    uint32_t  uCapacity;         // max triangles per tile
    uint32_t  uTotalTiles;
} ayTileBins;

typedef struct _ayTileWorkerData
{
    ayGraphicsData*    ptData;
    ayAtomicCounter*   ptNextTileIndex;
    ayCriticalSection* ptFramebufferLock;
} ayTileWorkerData;

typedef struct _ayUpscaleWorkerData
{
    ayFrameBufferData* ptInputFrameBuffer;
    ayFrameBufferData* ptOutputFrameBuffer;
    ayUpscaleFilter    tFilter;
} ayUpscaleWorkerData;

typedef void (*ayJobFunction)(void* pJobData, uint32_t uWorkerIndex);

typedef struct _ayThreadPool
{
    ayThread*            atThreads[THREAD_COUNT];
    ayCriticalSection*   ptLock;
    ayConditionVariable* ptWorkReady;
    ayConditionVariable* ptWorkComplete;

    ayJobFunction        ptCurrentJob;
    void*                pCurrentJobData;
    uint32_t             uGeneration;      // bumped each dispatch so workers know new work arrived
    uint32_t             uWorkersPending;  // counts down to 0 as workers finish the current dispatch
    bool                 bShutdown;
} ayThreadPool;

typedef struct _ayThreadPoolWorkerArgs
{
    ayThreadPool* ptPool;
    uint32_t      uWorkerIndex;
} ayThreadPoolWorkerArgs;

typedef struct _ayGraphicsData
{
    ayFrameBufferData* ptFrameBufferData;
    const void*        pVerticies;
    ayPipeline*        ptPipeline;
    uint32_t*          puIndexBufferData;
    ayDescriptor       tDescriptors[16];
    uint32_t           uScreenWidth;
    uint32_t           uScreenHeight;

    // tile rendering (NULL if bTileRendering == false)
    bool                 bTileRendering;
    ayTileRenderer*      ptTileRenderer;
    ayTileBins*          ptTileBins;
    ayTransformedVertex* pTransformedVertexCache;
    size_t               szTransformedVertexCapacity; // in vertex count, not bytes
    uint32_t*            puSequentialIndexCache; // for faking an index buffer on non indexed draw calls
    size_t               szSequentialIndexCapacity; // in index count, not bytes
    uint32_t             uTileMinX;
    uint32_t             uTileMinY;
    uint32_t             uTileMaxX;
    uint32_t             uTileMaxY;

    // upscaling (uOutputWidth/uOutputHeight both 0 if bUpscaling == false)
    bool            bUpscaling;
    ayUpscaleInfo   tUpscaleInfo;
    ayFrameBufferData* ptOutputFrameBuffer;

    // multi threading
    ayThreadPool* ptThreadPool;

    // fires once if a tile's triangle bin overflows, so silent drops don't
    // go unnoticed but a dense mesh doesn't spam the console every frame
    bool bTriangleCapacityWarned;
} ayGraphicsData;

//-----------------------------------------------------------------------------
// [SECTION] internal forward decleration
//-----------------------------------------------------------------------------

static void*         thread_pool_worker(void* pData);
static ayThreadPool* ay_create_thread_pool(void);
static void          ay_destroy_thread_pool(ayThreadPool** ppPool);
static void          ay_thread_pool_dispatch(ayThreadPool* ptPool, ayJobFunction ptJob, void* pJobData);

static void ay_draw_indexed_backend(ayGraphicsData* ptData, float* pfMainDepthBuffer, uint32_t uFirstIndex, uint32_t uIndexCount);
static void ay_set_pixel(ayFrameBufferData* ptData, ayVec2 input, ayVec4 tColor);
static void tile_worker_job(void* pJobData, uint32_t uWorkerIndex);
static void upscale_worker_job(void* pJobData, uint32_t uWorkerIndex);
static void ay_upscale_frame_buffer(ayGraphicsData* ptData, ayFrameBufferData* ptInputFrameBuffer, ayFrameBufferData* ptOutputFrameBuffer, ayUpscaleFilter tFilter);

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline float
ay_edge_function(ayVec3 a, ayVec3 b, ayVec3 c)
{
    return(b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
};

static inline int 
ay_min(int a, int b) 
{
    return (a < b) ? a : b;
}

static inline int 
ay_min3(int a, int b, int c) 
{
    int temp = (a < b) ? a : b;
    return (temp < c) ? temp : c; 
}

static inline int 
ay_max(int a, int b) 
{
    return (a > b) ? a : b;
}

static inline int 
ay_max3(int a, int b, int c) 
{
    int temp = (a > b) ? a : b;
    return (temp > c) ? temp : c;
}

static inline int
ay_clamp_int(int iValue, int iMin, int iMax)
{
    if(iValue < iMin) return iMin;
    if(iValue > iMax) return iMax;
    return iValue;
}

static float
ay_cubic_hermite(float fA, float fB, float fC, float fD, float fT)
{
    // catmull-rom style hermite interpolation through 4 points, weight fT in [0,1] between b and c
    float fA0 = -fA * 0.5f + fB * 1.5f - fC * 1.5f + fD * 0.5f;
    float fA1 = fA - fB * 2.5f + fC * 2.0f - fD * 0.5f;
    float fA2 = -fA * 0.5f + fC * 0.5f;
    float fA3 = fB;

    return fA0 * fT * fT * fT + fA1 * fT * fT + fA2 * fT + fA3;
}

static inline void 
ay_compute_edge_coeffs(ayVec3 v0, ayVec3 v1, float* a, float* b, float* c) 
{
    *a = v0.y - v1.y;
    *b = v1.x - v0.x;
    *c = v0.x * v1.y - v1.x * v0.y;
}

static inline ayVec3 
ay_run_vertex_shader(ayGraphicsData* ptData, uint32_t uIdx, const char* pcVtxBuffer, ayVaryingData* pVarying) 
{
    ayVertexShaderBuiltIns builtIns = {.uVertexID = uIdx, .tLayout = ptData->ptPipeline->tLayout};
    return ptData->ptPipeline->tVertexShader(builtIns, &pcVtxBuffer[uIdx * ptData->ptPipeline->tLayout.szVertexStride], ptData->tDescriptors, pVarying);
}

static inline void 
ay_ndc_to_screen(ayVec3* ptVertex, uint32_t uWidth, uint32_t uHeight) 
{
    ptVertex->x = uWidth  * (0.5f + 0.5f * ptVertex->x);
    ptVertex->y = uHeight * (0.5f + 0.5f * ptVertex->y);
}

static inline void 
ay_blend_varying_component(float* pfDestination, const float* v0, const float* v1, const float* v2, float wA, float wB, float wC) 
{
    *pfDestination = v0[0] * wA + v1[0] * wB + v2[0] * wC;
}

ayTileRenderer 
ay_init_tile_renderer(uint32_t uFrameBufferWidth, uint32_t uFrameBufferHeight, uint32_t uTileSize)
{
    ayTileRenderer tRenderer;

    tRenderer.uFrameBufferWidth = uFrameBufferWidth;
    tRenderer.uFrameBufferHeight = uFrameBufferHeight;
    tRenderer.uTileSize = uTileSize == 0 ? 32 : uTileSize;
    tRenderer.uTilesX = (uFrameBufferWidth + tRenderer.uTileSize - 1) / tRenderer.uTileSize;
    tRenderer.uTilesY = (uFrameBufferHeight + tRenderer.uTileSize - 1) / tRenderer.uTileSize;
    tRenderer.uTotalTiles = tRenderer.uTilesX * tRenderer.uTilesY;
    return tRenderer;
}

void 
ay_get_tile_bounds(ayTileRenderer* ptRenderer, uint32_t uTileIndex, uint32_t* puMinX, uint32_t* puMinY, uint32_t* puMaxX, uint32_t* puMaxY)
{

    uint32_t uX = uTileIndex % ptRenderer->uTilesX;
    uint32_t uY = uTileIndex / ptRenderer->uTilesX;
    
    *puMinX = uX * ptRenderer->uTileSize;
    *puMinY = uY * ptRenderer->uTileSize;
    *puMaxX = ay_min(*puMinX + ptRenderer->uTileSize, ptRenderer->uFrameBufferWidth);
    *puMaxY = ay_min(*puMinY + ptRenderer->uTileSize, ptRenderer->uFrameBufferHeight);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

ayGraphicsData*
ay_initialize_graphics(ayCreateGraphicsInfo* ptCreateGraphicsInfo)
{
    ayGraphicsData* ptData = malloc(sizeof(ayGraphicsData));
    if(!ptData) return NULL;
    memset(ptData, 0, sizeof(ayGraphicsData));

    ptData->uScreenWidth = ptCreateGraphicsInfo->uScreenWidth;
    ptData->uScreenHeight = ptCreateGraphicsInfo->uScreenHeight;
    ptData->bTileRendering = ptCreateGraphicsInfo->bTileRendering;

    if(ptCreateGraphicsInfo->bTileRendering)
    {
        // if tile rendering we allocate everything here and will free in graphics destroy function
        ptData->ptTileRenderer = malloc(sizeof(ayTileRenderer));
        memset(ptData->ptTileRenderer, 0, sizeof(ayTileRenderer));
        *ptData->ptTileRenderer = ay_init_tile_renderer(ptCreateGraphicsInfo->uScreenWidth, ptCreateGraphicsInfo->uScreenHeight, ptCreateGraphicsInfo->tTileSettings.uTileSize);
        
        ptData->ptTileBins = malloc(sizeof(ayTileBins));
        memset(ptData->ptTileBins, 0, sizeof(ayTileBins));
        
        uint32_t uRequestedCapacity = ptCreateGraphicsInfo->tTileSettings.uTriangleCapacity;
        ptData->ptTileBins->uCapacity = uRequestedCapacity == 0
            ? AY_TILE_TRIANGLE_CAPACITY
            : ay_min(uRequestedCapacity, AY_MAX_TILE_TRIANGLE_CAPACITY);
        ptData->ptTileBins->uTotalTiles = ptData->ptTileRenderer->uTotalTiles;
        
        ptData->ptTileBins->uCounts = malloc(sizeof(uint32_t) * ptData->ptTileBins->uTotalTiles);
        memset(ptData->ptTileBins->uCounts, 0, (sizeof(uint32_t) * ptData->ptTileBins->uTotalTiles));
        ptData->ptTileBins->uTriangleIndices = malloc(sizeof(uint32_t) * ptData->ptTileBins->uTotalTiles * ptData->ptTileBins->uCapacity);
        memset(ptData->ptTileBins->uTriangleIndices, 0, (sizeof(uint32_t) * ptData->ptTileBins->uTotalTiles * ptData->ptTileBins->uCapacity));
        
        // transformed vertex cache starts NULL, grows on first draw
        ptData->pTransformedVertexCache = NULL;
        ptData->szTransformedVertexCapacity = 0;
    }

    ptData->bUpscaling = ptCreateGraphicsInfo->bUpscale;

    if(ptCreateGraphicsInfo->bUpscale)
    {
        ptData->tUpscaleInfo = ptCreateGraphicsInfo->tUpscaleSettings;

        // no depth needed, upscale pass only writes color
        ptData->ptOutputFrameBuffer = ay_initialize_frame_buffer(ptCreateGraphicsInfo->tUpscaleSettings.uOutputWidth, ptCreateGraphicsInfo->tUpscaleSettings.uOutputHeight, false);
    }

    // persistent pool is shared by both tile rendering and upscaling, only
    // created if at least one of them actually needs threaded work
    if(ptCreateGraphicsInfo->bTileRendering || ptCreateGraphicsInfo->bUpscale)
    {
        ptData->ptThreadPool = ay_create_thread_pool();
    }

    return ptData;
}

void
ay_destroy_graphics(ayGraphicsData** ppData)
{
    if(!ppData || !*ppData) return;
    
    ayGraphicsData* ptData = *ppData;
    
    // shut down and join all pool threads first, before freeing anything they could touch
    if(ptData->ptThreadPool)
    {
        ay_destroy_thread_pool(&ptData->ptThreadPool);
    }
    
    if(ptData->bTileRendering)
    {
        free(ptData->pTransformedVertexCache);
        free(ptData->puSequentialIndexCache);
        
        if(ptData->ptTileBins)
        {
            free(ptData->ptTileBins->uTriangleIndices);
            free(ptData->ptTileBins->uCounts);
            free(ptData->ptTileBins);
        }
        
        free(ptData->ptTileRenderer);
    }
    
    if(ptData->bUpscaling && ptData->ptOutputFrameBuffer)
    {
        free(ptData->ptOutputFrameBuffer->auData);
        free(ptData->ptOutputFrameBuffer->pfDepthBuffer);
        free(ptData->ptOutputFrameBuffer);
    }
    
    free(ptData);
    *ppData = NULL;
}

ayWindow* 
ay_create_window(uint32_t uWidth, uint32_t uHeight, const char* pcTitle)
{
    ayWindow* ptNewWindow = malloc(sizeof(ayWindow));
    if(!ptNewWindow) return NULL;
    memset(ptNewWindow, 0, (sizeof(ayWindow)));

    glfwInit();
    ptNewWindow->pWindow = glfwCreateWindow(uWidth, uHeight, pcTitle, NULL, NULL);
    glfwMakeContextCurrent(ptNewWindow->pWindow);

    // set up OpenGL for 2D rendering
    glViewport(0, 0, uWidth, uHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, uWidth, uHeight, 0, -1, 1); // Y-down coordinates
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // create OpenGL texture for framebuffer
    glGenTextures(1, &ptNewWindow->uframebufferTexture);
    glBindTexture(GL_TEXTURE_2D, ptNewWindow->uframebufferTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, uWidth, uHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // store values
    ptNewWindow->uWidth  = uWidth;
    ptNewWindow->uHeight = uHeight;

    return ptNewWindow;
}

void
ay_destroy_window(ayWindow* ptWindow)
{
    if(!ptWindow) return;
    
    glDeleteTextures(1, &ptWindow->uframebufferTexture);
    glfwDestroyWindow(ptWindow->pWindow);
    glfwTerminate();
    free(ptWindow);
}

bool 
ay_window_should_close(ayWindow* ptWindow)
{
    return glfwWindowShouldClose(ptWindow->pWindow);
}

void 
ay_present_frame(ayGraphicsData* ptData, ayWindow* ptWindow)
{
    if(!ptData || !ptWindow) return;
    ayFrameBufferData* ptSourceBuffer = ptData->ptFrameBufferData;

    if(ptData->bUpscaling)
    {
        PROFILE_START(UpscalePass);
        ay_upscale_frame_buffer(ptData, ptData->ptFrameBufferData, ptData->ptOutputFrameBuffer, ptData->tUpscaleInfo.tFilter);
        PROFILE_END(UpscalePass);
        ptSourceBuffer = ptData->ptOutputFrameBuffer;
    }

    // upload framebuffer pixels to gl texture
    glBindTexture(GL_TEXTURE_2D, ptWindow->uframebufferTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ptSourceBuffer->uWidth, ptSourceBuffer->uHeight, 
                    GL_RGBA, GL_UNSIGNED_BYTE, ptSourceBuffer->auData);
    
    // draw fullscreen quad with texture
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, ptWindow->uframebufferTexture);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(0, 0);
        glTexCoord2f(1, 0); glVertex2f(ptWindow->uWidth, 0);
        glTexCoord2f(1, 1); glVertex2f(ptWindow->uWidth, ptWindow->uHeight);
        glTexCoord2f(0, 1); glVertex2f(0, ptWindow->uHeight);
    glEnd();
    
    glfwSwapBuffers(ptWindow->pWindow);
}

void
ay_bind_frame_buffer(ayGraphicsData* ptData, ayFrameBufferData* ptFrameBuffer)
{
    ptData->ptFrameBufferData = ptFrameBuffer;
}

void
ay_bind_pipeline(ayGraphicsData* ptData, ayPipeline* ptPipeline)
{
    ptData->ptPipeline = ptPipeline;
}

void
ay_bind_index_buffer(ayGraphicsData* ptData, uint32_t* puIndexBuffer)
{
    ptData->puIndexBufferData = puIndexBuffer;
};

void
ay_bind_vertex_buffer(ayGraphicsData* ptData, const void* pVertexBuffer)
{
    ptData->pVerticies = pVertexBuffer;
};

void 
ay_bind_descriptor(ayGraphicsData* ptData, uint32_t uBinding, ayDescriptorType eType, const void* pData)
{
    ptData->tDescriptors[uBinding].eType = eType;
    ptData->tDescriptors[uBinding].pData = pData;
}

void
ay_render_tile_local(ayGraphicsData tDataCopy, uint8_t* auLocalFB, float* afLocalDB, uint32_t uTileIndex)
{
    uint32_t uMinX = 0; 
    uint32_t uMinY = 0;
    uint32_t uMaxX = 0;
    uint32_t uMaxY = 0;

    ay_get_tile_bounds(tDataCopy.ptTileRenderer, uTileIndex, &uMinX, &uMinY, &uMaxX, &uMaxY);

    // create tile local frame buffer for ptDatatCopy to point to & create bins index buffer
    ayFrameBufferData tTileData = {0};
    tTileData.bDepthEnabled = tDataCopy.ptFrameBufferData->bDepthEnabled;
    tTileData.auData        = auLocalFB;
    tTileData.pfDepthBuffer = afLocalDB;
    tTileData.uHeight       = uMaxY - uMinY;
    tTileData.uWidth        = uMaxX - uMinX;

    uint32_t uBinStart = uTileIndex * tDataCopy.ptTileBins->uCapacity;
    uint32_t uTriangleCount = tDataCopy.ptTileBins->uCounts[uTileIndex];

    // grab the real frame's depth buffer before ptFrameBufferData gets
    // pointed at the local tile struct below, so the backend can still
    // check candidate pixels against everything drawn earlier this frame
    float* pfMainDepthBuffer = tDataCopy.ptFrameBufferData->pfDepthBuffer;

    uint32_t auTileIndexBuffer[AY_MAX_TILE_TRIANGLE_CAPACITY * 3] = {0};

    for(uint32_t i = 0; i < uTriangleCount; i++) 
    {
        uint32_t uTriIdx = tDataCopy.ptTileBins->uTriangleIndices[uBinStart + i];
        
        auTileIndexBuffer[i * 3]     = tDataCopy.puIndexBufferData[uTriIdx * 3];
        auTileIndexBuffer[i * 3 + 1] = tDataCopy.puIndexBufferData[uTriIdx * 3 + 1];
        auTileIndexBuffer[i * 3 + 2] = tDataCopy.puIndexBufferData[uTriIdx * 3 + 2];
    }
    tDataCopy.puIndexBufferData = auTileIndexBuffer;

    // pass tile constraints to ptDataCopy
    tDataCopy.ptFrameBufferData = &tTileData;
    tDataCopy.uTileMinX = uMinX;
    tDataCopy.uTileMinY = uMinY;
    tDataCopy.uTileMaxX = uMaxX;
    tDataCopy.uTileMaxY = uMaxY;

    ay_draw_indexed_backend(&tDataCopy, pfMainDepthBuffer, 0, uTriangleCount * 3);   
}

void 
ay_add_tile_to_frame(ayFrameBufferData* tMainFB, uint8_t* uLocalFB, float* pfLocalDB, uint32_t uMinX, uint32_t uMinY, uint32_t uMaxX, uint32_t uMaxY)
{
    // grab tile width (partial tiles possible when frame buffer dim is not divisible by tile size)
    uint32_t uTileWidth = uMaxX - uMinX;
    uint32_t uTileHeight = uMaxY - uMinY;

    for(uint32_t y = 0; y < uTileHeight; y++)
    {
        for(uint32_t x = 0; x < uTileWidth; x++)
        {
            uint32_t uLocalPixelIndex = y * uTileWidth + x;
            uint32_t uDstPixelIndex = (uMinY + y) * tMainFB->uWidth + (uMinX + x);

            if(tMainFB->bDepthEnabled)
            {
                float fLocalDepth = pfLocalDB[uLocalPixelIndex];

                // 0 is the tile's clear/background depth (nothing rasterized
                // here on this draw call), never overwrite with background
                if(fLocalDepth == 0.0f)
                    continue;

                // reverse-z: higher value is closer. only accept this pixel
                // if it's actually closer than whatever another draw already
                // placed here earlier in the same frame
                if(fLocalDepth <= tMainFB->pfDepthBuffer[uDstPixelIndex])
                    continue;

                tMainFB->pfDepthBuffer[uDstPixelIndex] = fLocalDepth;
            }

            uint32_t uLocalByteOffset = uLocalPixelIndex * 4;
            uint32_t uDstByteOffset = uDstPixelIndex * 4;
            tMainFB->auData[uDstByteOffset + 0] = uLocalFB[uLocalByteOffset + 0];
            tMainFB->auData[uDstByteOffset + 1] = uLocalFB[uLocalByteOffset + 1];
            tMainFB->auData[uDstByteOffset + 2] = uLocalFB[uLocalByteOffset + 2];
            tMainFB->auData[uDstByteOffset + 3] = uLocalFB[uLocalByteOffset + 3];
        }
    }
}

bool
ay_bin_triangles(ayGraphicsData* ptData, uint32_t uIndexCount, uint32_t uFirstIndex)
{
    memset(ptData->ptTileBins->uCounts, 0, sizeof(uint32_t) * ptData->ptTileBins->uTotalTiles);

    // check/grow transformed vertex cache will eventually match largest mesh per frame
    // may need to offer some sort of reset function in the future but that can be 
    // addressed after testing more of the new draw systems
    size_t uNeededCapacity = ptData->ptPipeline->tLayout.uVertexCount;
    if(uNeededCapacity > ptData->szTransformedVertexCapacity)
    {
        // realloc into a temp pointer first: on failure the original allocation is
        // still valid and must not be overwritten/leaked
        void* pNewCache = realloc(ptData->pTransformedVertexCache, uNeededCapacity * sizeof(ayTransformedVertex));
        if(!pNewCache)
            return false;

        ptData->pTransformedVertexCache = pNewCache;
        ptData->szTransformedVertexCapacity = uNeededCapacity;
    }
    
    ayTransformedVertex* ptTransformedVerts = ptData->pTransformedVertexCache;

    uint32_t uDroppedTriangles = 0;

    // process each triangle
    for(uint32_t i = 0; i < uIndexCount; i += 3)
    {
        const uint32_t uIndex0 = ptData->puIndexBufferData[uFirstIndex + i];
        const uint32_t uIndex1 = ptData->puIndexBufferData[uFirstIndex + i + 1];
        const uint32_t uIndex2 = ptData->puIndexBufferData[uFirstIndex + i + 2];

        ayVaryingData tVaryingData0 = {0};
        ayVaryingData tVaryingData1 = {0};
        ayVaryingData tVaryingData2 = {0};

        const char* pcVtxBuffer = (char*)ptData->pVerticies;
        ayVec3 tVertex0 = ay_run_vertex_shader(ptData, uIndex0, pcVtxBuffer, &tVaryingData0);
        ayVec3 tVertex1 = ay_run_vertex_shader(ptData, uIndex1, pcVtxBuffer, &tVaryingData1);
        ayVec3 tVertex2 = ay_run_vertex_shader(ptData, uIndex2, pcVtxBuffer, &tVaryingData2);

        ay_ndc_to_screen(&tVertex0, ptData->uScreenWidth, ptData->uScreenHeight);
        ay_ndc_to_screen(&tVertex1, ptData->uScreenWidth, ptData->uScreenHeight);
        ay_ndc_to_screen(&tVertex2, ptData->uScreenWidth, ptData->uScreenHeight);

        // store transformed vertices (position + varyings)
        ptTransformedVerts[uIndex0].tScreenPos = tVertex0;
        ptTransformedVerts[uIndex0].tVaryings = tVaryingData0;
        
        ptTransformedVerts[uIndex1].tScreenPos = tVertex1;
        ptTransformedVerts[uIndex1].tVaryings = tVaryingData1;
        
        ptTransformedVerts[uIndex2].tScreenPos = tVertex2;
        ptTransformedVerts[uIndex2].tVaryings = tVaryingData2;

        // get triangle bounding box
        uint32_t uTriMinX = ay_min3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x);
        uint32_t uTriMinY = ay_min3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y);
        uint32_t uTriMaxX = ay_max3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x);
        uint32_t uTriMaxY = ay_max3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y);

        // create bounding box of TILES not pixel bounding boxes like used in rasterizer
        uint32_t uStartTileX = uTriMinX / ptData->ptTileRenderer->uTileSize;
        uint32_t uStartTileY = uTriMinY / ptData->ptTileRenderer->uTileSize;
        uint32_t uStopTileX = uTriMaxX / ptData->ptTileRenderer->uTileSize;
        uint32_t uStopTileY = uTriMaxY / ptData->ptTileRenderer->uTileSize;
        uStopTileX = ay_min(ptData->ptTileRenderer->uTilesX - 1, uStopTileX);
        uStopTileY = ay_min(ptData->ptTileRenderer->uTilesY - 1, uStopTileY);

        for(uint32_t uY = uStartTileY; uY <= uStopTileY; uY++)
        {
            for(uint32_t uX = uStartTileX; uX <= uStopTileX; uX++)
            {
                uint32_t uTileIndex = uY * ptData->ptTileRenderer->uTilesX + uX;
                uint32_t uBinStart = uTileIndex * ptData->ptTileBins->uCapacity;
                uint32_t uCount = ptData->ptTileBins->uCounts[uTileIndex];
                
                if(uCount < ptData->ptTileBins->uCapacity) 
                {
                    ptData->ptTileBins->uTriangleIndices[uBinStart + uCount] = i / 3;
                    ptData->ptTileBins->uCounts[uTileIndex]++;
                }
                else
                {
                    uDroppedTriangles++;
                }
            }
        }
    }

    if(uDroppedTriangles > 0 && !ptData->bTriangleCapacityWarned)
    {
        printf("ay_rasterize warning: %u triangle bin overflow(s) this draw call, "
               "per-tile capacity of %u exceeded. these triangles were silently dropped. "
               "raise ayTileRenderInfo.uTriangleCapacity to fix (this warning only prints once).\n",
               uDroppedTriangles, ptData->ptTileBins->uCapacity);
        ptData->bTriangleCapacityWarned = true;
    }

    return true;
}

void
ay_draw(ayGraphicsData* ptData, uint32_t uFirstVertex, uint32_t uVertexCount)
{
    // ay_draw has no real index buffer to bin/tile against, so synthesize an
    // identity one (uFirstVertex, uFirstVertex+1, ...) and delegate straight
    // to ay_draw_indexed, which already has correct tiled/non-tiled dispatch.
    // this avoids a third copy of the rasterization loop and gives ay_draw
    // full tiling support for free.
    if(uVertexCount > ptData->szSequentialIndexCapacity)
    {
        void* pNewCache = realloc(ptData->puSequentialIndexCache, uVertexCount * sizeof(uint32_t));
        if(!pNewCache)
            return;

        ptData->puSequentialIndexCache = pNewCache;
        ptData->szSequentialIndexCapacity = uVertexCount;
    }

    for(uint32_t i = 0; i < uVertexCount; i++)
        ptData->puSequentialIndexCache[i] = uFirstVertex + i;

    // stash and restore in case the caller had a real index buffer bound
    // (e.g. mixing ay_draw and ay_draw_indexed calls on the same ptData)
    uint32_t* puPreviousIndexBuffer = ptData->puIndexBufferData;
    ptData->puIndexBufferData = ptData->puSequentialIndexCache;

    ay_draw_indexed(ptData, 0, uVertexCount);

    ptData->puIndexBufferData = puPreviousIndexBuffer;
}

static void 
ay_draw_indexed_backend(ayGraphicsData* ptData, float* pfMainDepthBuffer, uint32_t uFirstIndex, uint32_t uIndexCount)
{
    bool bTiledRendering = ptData->bTileRendering;

    // set winding sign (CW need negative, CCW needs positive barycentric coords)
    float fWindingSign = (ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_CLOCKWISE) ? -1.0f : 1.0f;

    // calculate frame buffer size cache pointer to depth buffer
    const uint32_t fbWidth = ptData->ptFrameBufferData->uWidth;
    const uint32_t fbHeight = ptData->ptFrameBufferData->uHeight;
    float* pfDepthBuffer = ptData->ptFrameBufferData->pfDepthBuffer;
    
    ayTransformedVertex* transformedVerts = NULL;
    if(bTiledRendering) 
    {
        transformedVerts = ptData->pTransformedVertexCache;
    }
    
    for(uint32_t i = 0; i < uIndexCount; i += 3)
    {
        const uint32_t uIndex0 = ptData->puIndexBufferData[uFirstIndex + i];
        const uint32_t uIndex1 = ptData->puIndexBufferData[uFirstIndex + i + 1];
        const uint32_t uIndex2 = ptData->puIndexBufferData[uFirstIndex + i + 2];

        PROFILE_START(TriangleSetup);
        ayVec3 tVertex0; 
        ayVec3 tVertex1; 
        ayVec3 tVertex2;
        ayVaryingData tVaryingData0 = {0};
        ayVaryingData tVaryingData1 = {0};
        ayVaryingData tVaryingData2 = {0};
        
        if(bTiledRendering) // tiled path
        {
            tVertex0 = ptData->pTransformedVertexCache[uIndex0].tScreenPos;
            tVertex1 = ptData->pTransformedVertexCache[uIndex1].tScreenPos;
            tVertex2 = ptData->pTransformedVertexCache[uIndex2].tScreenPos;
            
            tVaryingData0 = ptData->pTransformedVertexCache[uIndex0].tVaryings;
            tVaryingData1 = ptData->pTransformedVertexCache[uIndex1].tVaryings;
            tVaryingData2 = ptData->pTransformedVertexCache[uIndex2].tVaryings;
        }
        else // non-tiled path
        {
            PROFILE_START(VertexShader);
            const char* pcVtxBuffer = (char*)ptData->pVerticies;
            tVertex0 = ay_run_vertex_shader(ptData, uIndex0, pcVtxBuffer, &tVaryingData0);
            tVertex1 = ay_run_vertex_shader(ptData, uIndex1, pcVtxBuffer, &tVaryingData1);
            tVertex2 = ay_run_vertex_shader(ptData, uIndex2, pcVtxBuffer, &tVaryingData2);
            PROFILE_END(VertexShader);

            ay_ndc_to_screen(&tVertex0, ptData->uScreenWidth, ptData->uScreenHeight);
            ay_ndc_to_screen(&tVertex1, ptData->uScreenWidth, ptData->uScreenHeight);
            ay_ndc_to_screen(&tVertex2, ptData->uScreenWidth, ptData->uScreenHeight);
        }

        float ABC = (float)ay_edge_function(tVertex0, tVertex1, tVertex2);

        // cull backfaces based on winding
        if(ABC > 0 && ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_CLOCKWISE) continue;
        if(ABC < 0 && ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_COUNTER_CLOCKWISE) continue;
        if(ABC == 0) continue;  // degenerate triangle

        uint32_t uTriMinX = ay_min3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x);
        uint32_t uTriMinY = ay_min3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y);
        uint32_t uTriMaxX = ay_max3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x);
        uint32_t uTriMaxY = ay_max3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y);
        
        if(uTriMinX > 0) uTriMinX -= 1;
        if(uTriMinY > 0) uTriMinY -= 1;
        uTriMaxX += 1;
        uTriMaxY += 1;

        uTriMinX = ay_max(0, uTriMinX);
        uTriMinY = ay_max(0, uTriMinY);
        uTriMaxX = ay_min(ptData->uScreenWidth, uTriMaxX);
        uTriMaxY = ay_min(ptData->uScreenHeight, uTriMaxY);

        if(bTiledRendering) 
        {
            uTriMinX = ay_max(ptData->uTileMinX, uTriMinX);
            uTriMinY = ay_max(ptData->uTileMinY, uTriMinY);
            uTriMaxX = ay_min(ptData->uTileMaxX, uTriMaxX);
            uTriMaxY = ay_min(ptData->uTileMaxY, uTriMaxY);
            
            // early out if triangle doesn't overlap tile at all
            if(uTriMinX >= uTriMaxX || uTriMinY >= uTriMaxY) continue;
        }

        const uint32_t minX = uTriMinX;
        const uint32_t minY = uTriMinY;
        const uint32_t maxX = uTriMaxX;
        const uint32_t maxY = uTriMaxY;

        // precompute edge function coefficients
        float ABa, ABb, ABc, BCa, BCb, BCc, CAa, CAb, CAc;
        ay_compute_edge_coeffs(tVertex0, tVertex1, &ABa, &ABb, &ABc);
        ay_compute_edge_coeffs(tVertex1, tVertex2, &BCa, &BCb, &BCc);
        ay_compute_edge_coeffs(tVertex2, tVertex0, &CAa, &CAb, &CAc);

        // apply winding sign to coefficients
        ABa *= fWindingSign;
        BCa *= fWindingSign;
        CAa *= fWindingSign;

        ABb *= fWindingSign;
        BCb *= fWindingSign;
        CAb *= fWindingSign;

        float ABP = (ABa * minX + ABb * minY + ABc) * fWindingSign;
        float BCP = (BCa * minX + BCb * minY + BCc) * fWindingSign;
        float CAP = (CAa * minX + CAb * minY + CAc) * fWindingSign;

        // precompute inverse triangle area (1/area so we can multiply instead of divide (faster))
        const float invABC = 1.0f / ABC;
        PROFILE_END(TriangleSetup);

        PROFILE_START(VaryingSystem);
        // varying system
        int iVaryDataOffset = 0;
        ayVaryingData blendedVaryingData = {0};

        int iVaryingCount = 0;
        for(uint32_t j = 0; j < 16; j++)
        {
            blendedVaryingData._auOffset[j] = tVaryingData0._auOffset[j];
            blendedVaryingData.atTypes[j] = tVaryingData0.atTypes[j];
            if(tVaryingData0.atTypes[j] == AY_VARYING_TYPE_NONE)
                break;
            iVaryingCount++;
        }

        int iCompCount[16] = {0};
        for(uint32_t k = 0; k < iVaryingCount; k++)
        {
            ayVaryingType type = tVaryingData0.atTypes[k];

            if    (type == AY_VARYING_TYPE_FLOAT) iCompCount[k] = 1;
            else if(type == AY_VARYING_TYPE_VEC2) iCompCount[k] = 2;
            else if(type == AY_VARYING_TYPE_VEC3) iCompCount[k] = 3;
            else if(type == AY_VARYING_TYPE_VEC4) iCompCount[k] = 4;
        }
        PROFILE_END(VaryingSystem);

        // depth index for tiled or non tiled
        uint32_t uDepthIndex;
        if(bTiledRendering) 
        {
            uDepthIndex = 0;  // start at beginning of tile buffer
        } 
        else 
        {
            uDepthIndex = minY * fbWidth + minX;
        }

        // pixel loop
        PROFILE_START(PixelLoop);
        if(ptData->ptFrameBufferData->bDepthEnabled)
        {
            for(uint32_t y = minY; y < maxY; y++)
            {
                uint32_t uRowDepthIndex = uDepthIndex; // copy depth index for rows 
                float rowABP = ABP;
                float rowBCP = BCP;
                float rowCAP = CAP;

                for(uint32_t x = minX; x < maxX; x++)
                {
                    if(rowABP>= 0 && rowBCP >= 0 && rowCAP >= 0)
                    {
                        // for tiled or not tiled drawing
                        uint32_t uLocalX = x - ptData->uTileMinX;
                        uint32_t uLocalY = y - ptData->uTileMinY;
                        uint32_t uBufferDepthIndex = bTiledRendering ? (uLocalY * fbWidth + uLocalX) : uRowDepthIndex;

                        // reset offset for every new pixel
                        iVaryDataOffset = 0; 

                        const float weightA = rowBCP * invABC;
                        const float weightB = rowCAP * invABC;
                        const float weightC = rowABP * invABC;

                        float fPixelDepth = tVertex0.z * weightA + tVertex1.z * weightB + tVertex2.z * weightC;

                        // TODO: this early-Z check assumes a losing fragment can be safely
                        // discarded, because the winner fully replaces it. That's only true for
                        // opaque geometry. If alpha blending is added, transparent fragments need
                        // to be excluded from this reject path entirely a losing depth test
                        // doesn't mean "invisible," it means "blends behind what's already here."
                        // Transparent draws will likely need their own pass: back-to-front order,
                        // depth test on, depth write off, and no early rejection at all.
                        if(bTiledRendering && pfMainDepthBuffer)
                        {
                            uint32_t uMainScreenIndex = y * ptData->uScreenWidth + x;
                            bool bOccludedByEarlierDraw = fPixelDepth <= pfMainDepthBuffer[uMainScreenIndex];

                            if(bOccludedByEarlierDraw)
                            {
                                rowABP += ABa;
                                rowBCP += BCa;
                                rowCAP += CAa;
                                uRowDepthIndex += 1;
                                continue;
                            }
                        }

                        ayPixelShaderBuiltIns tBuiltIns = {
                            .tUV = {x, y}
                        };
                    
                        for(int iVaryIndex = 0; iVaryIndex < iVaryingCount; iVaryIndex++)
                        {
                            int iCurrentCompCount = iCompCount[iVaryIndex];
                            if(iCurrentCompCount > 0)
                            {
                                const float* v0 = (const float*)&tVaryingData0.acVaryingData[iVaryDataOffset];
                                const float* v1 = (const float*)&tVaryingData1.acVaryingData[iVaryDataOffset];
                                const float* v2 = (const float*)&tVaryingData2.acVaryingData[iVaryDataOffset];
                                float* dest = (float*)&blendedVaryingData.acVaryingData[iVaryDataOffset];

                                for(int c = 0; c < iCurrentCompCount; c++)
                                {
                                    dest[c] = v0[c] * weightA + v1[c] * weightB + v2[c] * weightC;
                                }
                                iVaryDataOffset += iCurrentCompCount * sizeof(float);
                            }
                        }

                        PROFILE_START(DepthTest);
                        float fCurrentDepth = pfDepthBuffer[uBufferDepthIndex];
                        PROFILE_END(DepthTest);

                        if(fPixelDepth > fCurrentDepth) 
                        {
                            PROFILE_START(FragmentShader);
                            ayVec4 tFinalColor = ptData->ptPipeline->tPixelShader(tBuiltIns, ptData->tDescriptors, &blendedVaryingData);
                            float alphaScale = tFinalColor.a / 255.0f;
                            tFinalColor.r *= alphaScale;
                            tFinalColor.g *= alphaScale;
                            tFinalColor.b *= alphaScale;
                            PROFILE_END(FragmentShader);
                            
                            ay_set_pixel(ptData->ptFrameBufferData, (ayVec2){uLocalX, uLocalY}, tFinalColor);
                            pfDepthBuffer[uBufferDepthIndex] = fPixelDepth;
                        }
                    }
                    // incrementally update edge functions for next pixel
                    rowABP += ABa;
                    rowBCP += BCa;
                    rowCAP += CAa;
                    uRowDepthIndex += 1; 
                }
                // incrementally update edge functions for next row
                ABP += ABb;
                BCP += BCb;
                CAP += CAb;
                uDepthIndex += fbWidth; 
            }
        } 
        else
        {
            for(uint32_t y = minY; y <= maxY; y++)
            {
                float rowABP = ABP;
                float rowBCP = BCP;
                float rowCAP = CAP;

                for(uint32_t x = minX; x <= maxX; x++)
                {
                    if(rowABP >= 0 && rowBCP >= 0 && rowCAP >= 0)
                    {
                        uint32_t uLocalX = x - ptData->uTileMinX;
                        uint32_t uLocalY = y - ptData->uTileMinY;
                        iVaryDataOffset = 0; // reset offset for every new pixel

                        const float weightA = rowBCP * invABC;
                        const float weightB = rowCAP * invABC;
                        const float weightC = rowABP * invABC;

                        ayPixelShaderBuiltIns tBuiltIns = {
                            .tUV = {x, y}
                        };
                    
                        for(int iVaryIndex = 0; iVaryIndex < iVaryingCount; iVaryIndex++)
                        {
                            int iCurrentCompCount = iCompCount[iVaryIndex];
                            if(iCurrentCompCount > 0)
                            {
                                const float* v0 = (const float*)&tVaryingData0.acVaryingData[iVaryDataOffset];
                                const float* v1 = (const float*)&tVaryingData1.acVaryingData[iVaryDataOffset];
                                const float* v2 = (const float*)&tVaryingData2.acVaryingData[iVaryDataOffset];
                                float* dest = (float*)&blendedVaryingData.acVaryingData[iVaryDataOffset];

                                for(int c = 0; c < iCurrentCompCount; c++)
                                {
                                    dest[c] = v0[c] * weightA + v1[c] * weightB + v2[c] * weightC;
                                }
                                iVaryDataOffset += iCurrentCompCount * sizeof(float);
                            }
                        }

                        PROFILE_START(FragmentShader);
                        ayVec4 tFinalColor = ptData->ptPipeline->tPixelShader(tBuiltIns, ptData->tDescriptors, &blendedVaryingData);
                        float alphaScale = tFinalColor.a / 255.0f;
                        tFinalColor.r *= alphaScale;
                        tFinalColor.g *= alphaScale;
                        tFinalColor.b *= alphaScale;
                        PROFILE_END(FragmentShader);

                        ayVec2 tWritePos = bTiledRendering ? (ayVec2){uLocalX, uLocalY} : (ayVec2){x, y};
                        ay_set_pixel(ptData->ptFrameBufferData, tWritePos, tFinalColor);
                    }
                    // incrementally update edge functions for next pixel
                    rowABP += ABa;
                    rowBCP += BCa;
                    rowCAP += CAa;
                }
                // incrementally update edge functions for next row
                ABP += ABb;
                BCP += BCb;
                CAP += CAb;
            }
        } 
        PROFILE_END(PixelLoop);
    }
}

void 
ay_draw_indexed(ayGraphicsData* ptData, uint32_t uFirstIndex, uint32_t uIndexCount)
{
    // kept api same for user side and just check if tiled or not to call correct version on the backend 
    if(ptData->bTileRendering)
    {
        // bin triangles to stop from running checks on triangles 
        // not in a specific tile, we are using atomic counter for
        // tracking the tile we are currently on and the critical 
        // section prevents race conditions when copying tile to 
        // frame buffer
        if(!ay_bin_triangles(ptData, uIndexCount, uFirstIndex))
            return;

        ayAtomicCounter* ptTileCounter;
        if(ay_create_atomic_counter(0, &ptTileCounter) != AY_ATOMICS_RESULT_SUCCESS) 
            return;

        ayCriticalSection* ptCriticalSection;
        if(ay_create_critical_section(&ptCriticalSection) != AY_THREAD_RESULT_SUCCESS)
        {
            ay_destroy_atomic_counter(&ptTileCounter);
            return;
        }

        ayTileWorkerData tTileData = {
            .ptData            = ptData,
            .ptNextTileIndex   = ptTileCounter,
            .ptFramebufferLock = ptCriticalSection
        };

        // dispatched to the persistent pool created at graphics init, threads
        // are reused rather than spawned/joined on every draw call
        ay_thread_pool_dispatch(ptData->ptThreadPool, tile_worker_job, &tTileData);

        ay_destroy_atomic_counter(&ptTileCounter);
        ay_destroy_critical_section(&ptCriticalSection);
    }
    else
    {
        ay_draw_indexed_backend(ptData, NULL, uFirstIndex, uIndexCount);
    }
}

const void*
ay_get_vertex_attrib(const void* pcVertexDataIn, ayVertexLayout tLayout, uint32_t tAttribLocation)
{
    const void* ptResult = pcVertexDataIn;
    if(tLayout.tAttribType[tAttribLocation] == AY_VERTEX_ATTRIBUTE_TYPE_VEC2)
    {
        uint32_t uOffset = (uint32_t)tLayout.szAttribOffset[tAttribLocation];
        ptResult = (char*)ptResult + uOffset;
    }
    else if(tLayout.tAttribType[tAttribLocation] == AY_VERTEX_ATTRIBUTE_TYPE_VEC3)
    {
        uint32_t uOffset = (uint32_t)tLayout.szAttribOffset[tAttribLocation];
        ptResult = (char*)ptResult + uOffset;
    }
    else if(tLayout.tAttribType[tAttribLocation] == AY_VERTEX_ATTRIBUTE_TYPE_VEC4)
    {
        uint32_t uOffset = (uint32_t)tLayout.szAttribOffset[tAttribLocation];
        ptResult = (char*)ptResult + uOffset;
    }
    else if(tLayout.tAttribType[tAttribLocation] == AY_VERTEX_ATTRIBUTE_TYPE_FLOAT)
    {
        uint32_t uOffset = (uint32_t)tLayout.szAttribOffset[tAttribLocation];
        ptResult = (char*)ptResult + uOffset;
    }
    return (void*)ptResult;
};

ayFrameBufferData*
ay_initialize_frame_buffer(uint32_t uWidth, uint32_t uHeight, bool bDepthEnabled)
{
    ayFrameBufferData* ptData = malloc(sizeof(ayFrameBufferData));
    if(!ptData) return NULL;
    memset(ptData, 0, sizeof(ayFrameBufferData));

    ptData->uWidth = uWidth;
    ptData->uHeight = uHeight;
    ptData->auData = malloc(sizeof(char) * 4 * uWidth * uHeight);
    memset(ptData->auData, 0, sizeof(char) * 4 * uWidth * uHeight);

    if(bDepthEnabled)
    {
        // 0 is our clear value for depth buffer (faster)
        ptData->bDepthEnabled = bDepthEnabled;
        ptData->pfDepthBuffer = malloc(sizeof(float) * uHeight * uWidth);
        memset(ptData->pfDepthBuffer, 0, ptData->uWidth * ptData->uHeight * sizeof(float));
    }

    return ptData;
};

void
ay_output_frame_buffer(ayFrameBufferData* ptData)
{
    // leaving in for debug purposes
    stbi_write_png("output.png", ptData->uWidth, ptData->uHeight, 4, ptData->auData, sizeof(char) * 4 * ptData->uWidth);
};

void
ay_clear_frame_buffer(ayFrameBufferData* ptData)
{
    memset(ptData->auData, 255, sizeof(char) * (ptData->uHeight * 4) * (ptData->uWidth));
        
    if(ptData->bDepthEnabled && ptData->pfDepthBuffer)
    {
        // depth buffer clear value is 0 (faster)
        memset(ptData->pfDepthBuffer, 0, ptData->uWidth * ptData->uHeight * sizeof(float));
    }
}

void*
ay_set_varying(ayVaryingType tType, ayVaryingData* ptVaryingDataOut)
{
    void* ptResult = (void*)&ptVaryingDataOut->acVaryingData[ptVaryingDataOut->_uCurrentOffset];
    ptVaryingDataOut->_auOffset[ptVaryingDataOut->_uCurrentVarying] = ptVaryingDataOut->_uCurrentOffset;

    if(tType == AY_VARYING_TYPE_FLOAT)     ptVaryingDataOut->_uCurrentOffset += sizeof(float);
    else if(tType == AY_VARYING_TYPE_VEC2) ptVaryingDataOut->_uCurrentOffset += sizeof(float) * 2;
    else if(tType == AY_VARYING_TYPE_VEC3) ptVaryingDataOut->_uCurrentOffset += sizeof(float) * 3;
    else if(tType == AY_VARYING_TYPE_VEC4) ptVaryingDataOut->_uCurrentOffset += sizeof(float) * 4;

    ptVaryingDataOut->atTypes[ptVaryingDataOut->_uCurrentVarying] = tType;
    ptVaryingDataOut->_uCurrentVarying++;

    return ptResult;
}

const void*
ay_get_varying(uint32_t uVaryingIndex, const ayVaryingData* ptVaryingDataOut)
{
    uint32_t uOffset = ptVaryingDataOut->_auOffset[uVaryingIndex];
    return (void*)&ptVaryingDataOut->acVaryingData[uOffset];
}

unsigned char*
ay_load_png(const char* pcFileName, int* iWidthOut, int* iHeightOut)
{
    int iComponentsInFile = 0;
    return stbi_load(pcFileName, iWidthOut, iHeightOut, &iComponentsInFile, 4);
}

ayVec4
ay_sample_texture(ayTexture tTexture, ayVec2 tUV, uint32_t uComponents)
{
    // convert UV to pixel coords & clamp to texture bounds 
    int iPixelX = (int)((tUV.x) * (tTexture.iWidth - 1));
    int iPixelY = (int)((tUV.y) * (tTexture.iHeight - 1));
    iPixelX = iPixelX < 0 ? 0 : (iPixelX >= tTexture.iWidth ? tTexture.iWidth - 1 : iPixelX);
    iPixelY = iPixelY < 0 ? 0 : (iPixelY >= tTexture.iHeight ? tTexture.iHeight - 1 : iPixelY);
    
    // compute offset
    int iPixelStart = (iPixelY * tTexture.iWidth + iPixelX) * uComponents;

    return (ayVec4){
        (float)tTexture.pucData[iPixelStart], 
        (float)tTexture.pucData[iPixelStart + 1], 
        (float)tTexture.pucData[iPixelStart + 2],
        (float)tTexture.pucData[iPixelStart + 3]};
}

ayVec4
ay_sample_texture_bilinear(ayTexture tTexture, ayVec2 tUV, uint32_t uComponents)
{
    // bilinearly samples a texture at uv, 
    // returning interpolated rgb and 
    // passthrough alpha from the top-left texel
    
    float fPixelX = tUV.x * (tTexture.iWidth - 1);
    float fPixelY = tUV.y * (tTexture.iHeight - 1);

    int iX0 = (int)fPixelX;
    int iY0 = (int)fPixelY;
    int iX1 = iX0 + 1;
    int iY1 = iY0 + 1;

    iX1 = iX1 >= tTexture.iWidth ? tTexture.iWidth - 1 : iX1;
    iY1 = iY1 >= tTexture.iHeight ? tTexture.iHeight - 1 : iY1;

    float fFracX = fPixelX - iX0;
    float fFracY = fPixelY - iY0;

    float fWX0 = 1.0f - fFracX;
    float fWY0 = 1.0f - fFracY;

    int iRow0 = iY0 * tTexture.iWidth;
    int iRow1 = iY1 * tTexture.iWidth;

    int iOffsetTL = (iRow0 + iX0) * uComponents;
    int iOffsetTR = (iRow0 + iX1) * uComponents;
    int iOffsetBL = (iRow1 + iX0) * uComponents;
    int iOffsetBR = (iRow1 + iX1) * uComponents;

    float fAlphaPassthrough = (float)tTexture.pucData[iOffsetTL + 3];

    ayVec3 tTopLeft = {
        (float)tTexture.pucData[iOffsetTL],
        (float)tTexture.pucData[iOffsetTL + 1],
        (float)tTexture.pucData[iOffsetTL + 2]
    };

    ayVec3 tTopRight = {
        (float)tTexture.pucData[iOffsetTR],
        (float)tTexture.pucData[iOffsetTR + 1],
        (float)tTexture.pucData[iOffsetTR + 2]
    };

    ayVec3 tBottomLeft = {
        (float)tTexture.pucData[iOffsetBL],
        (float)tTexture.pucData[iOffsetBL + 1],
        (float)tTexture.pucData[iOffsetBL + 2]
    };

    ayVec3 tBottomRight = {
        (float)tTexture.pucData[iOffsetBR],
        (float)tTexture.pucData[iOffsetBR + 1],
        (float)tTexture.pucData[iOffsetBR + 2]
    };

    ayVec3 tTop = {
        tTopLeft.r * fWX0 + tTopRight.r * fFracX,
        tTopLeft.g * fWX0 + tTopRight.g * fFracX,
        tTopLeft.b * fWX0 + tTopRight.b * fFracX
    };

    ayVec3 tBottom = {
        tBottomLeft.r * fWX0 + tBottomRight.r * fFracX,
        tBottomLeft.g * fWX0 + tBottomRight.g * fFracX,
        tBottomLeft.b * fWX0 + tBottomRight.b * fFracX
    };

    ayVec4 tResult = {
        tTop.r * fWY0 + tBottom.r * fFracY,
        tTop.g * fWY0 + tBottom.g * fFracY,
        tTop.b * fWY0 + tBottom.b * fFracY,
        fAlphaPassthrough
    };

    return tResult;
}

ayVec4
ay_sample_texture_bicubic(ayTexture tTexture, ayVec2 tUV, uint32_t uComponents)
{
    // 16-tap catmull-rom bicubic sample, alpha passthrough from nearest texel

    float fPixelX = tUV.x * (tTexture.iWidth - 1);
    float fPixelY = tUV.y * (tTexture.iHeight - 1);

    int iX1 = (int)fPixelX;
    int iY1 = (int)fPixelY;

    float fFracX = fPixelX - iX1;
    float fFracY = fPixelY - iY1;

    float afRowResults[4][3];

    for(int iRow = -1; iRow <= 2; iRow++)
    {
        int iSampleY = ay_clamp_int(iY1 + iRow, 0, tTexture.iHeight - 1);

        float afSamples[4][3];
        for(int iCol = -1; iCol <= 2; iCol++)
        {
            int iSampleX = ay_clamp_int(iX1 + iCol, 0, tTexture.iWidth - 1);
            int iOffset = (iSampleY * tTexture.iWidth + iSampleX) * uComponents;

            afSamples[iCol + 1][0] = (float)tTexture.pucData[iOffset];
            afSamples[iCol + 1][1] = (float)tTexture.pucData[iOffset + 1];
            afSamples[iCol + 1][2] = (float)tTexture.pucData[iOffset + 2];
        }

        afRowResults[iRow + 1][0] = ay_cubic_hermite(afSamples[0][0], afSamples[1][0], afSamples[2][0], afSamples[3][0], fFracX);
        afRowResults[iRow + 1][1] = ay_cubic_hermite(afSamples[0][1], afSamples[1][1], afSamples[2][1], afSamples[3][1], fFracX);
        afRowResults[iRow + 1][2] = ay_cubic_hermite(afSamples[0][2], afSamples[1][2], afSamples[2][2], afSamples[3][2], fFracX);
    }

    int iAlphaSampleY = ay_clamp_int(iY1, 0, tTexture.iHeight - 1);
    int iAlphaSampleX = ay_clamp_int(iX1, 0, tTexture.iWidth - 1);
    int iAlphaOffset = (iAlphaSampleY * tTexture.iWidth + iAlphaSampleX) * uComponents;
    float fAlphaPassthrough = (float)tTexture.pucData[iAlphaOffset + 3];

    ayVec4 tResult = {
        ay_cubic_hermite(afRowResults[0][0], afRowResults[1][0], afRowResults[2][0], afRowResults[3][0], fFracY),
        ay_cubic_hermite(afRowResults[0][1], afRowResults[1][1], afRowResults[2][1], afRowResults[3][1], fFracY),
        ay_cubic_hermite(afRowResults[0][2], afRowResults[1][2], afRowResults[2][2], afRowResults[3][2], fFracY),
        fAlphaPassthrough
    };

    // clamp since catmull-rom can overshoot/ring past 0-255
    tResult.r = tResult.r < 0.0f ? 0.0f : (tResult.r > 255.0f ? 255.0f : tResult.r);
    tResult.g = tResult.g < 0.0f ? 0.0f : (tResult.g > 255.0f ? 255.0f : tResult.g);
    tResult.b = tResult.b < 0.0f ? 0.0f : (tResult.b > 255.0f ? 255.0f : tResult.b);

    return tResult;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

void*
thread_pool_worker(void* pData)
{
    ayThreadPoolWorkerArgs* ptArgs = (ayThreadPoolWorkerArgs*)pData;
    ayThreadPool* ptPool = ptArgs->ptPool;
    uint32_t uLastSeenGeneration = 0;

    ay_enter_critical_section(ptPool->ptLock);
    while(true)
    {
        // sleep until the dispatcher bumps the generation (new work) or asks us to shut down
        while(ptPool->uGeneration == uLastSeenGeneration && !ptPool->bShutdown)
            ay_condition_wait(ptPool->ptWorkReady, ptPool->ptLock);

        if(ptPool->bShutdown)
            break;

        uLastSeenGeneration = ptPool->uGeneration;
        ayJobFunction ptJob = ptPool->ptCurrentJob;
        void* pJobData = ptPool->pCurrentJobData;

        ay_leave_critical_section(ptPool->ptLock);
        ptJob(pJobData, ptArgs->uWorkerIndex);
        ay_enter_critical_section(ptPool->ptLock);

        ptPool->uWorkersPending--;
        if(ptPool->uWorkersPending == 0)
            ay_condition_signal(ptPool->ptWorkComplete);
    }
    ay_leave_critical_section(ptPool->ptLock);

    free(ptArgs);
    return NULL;
}

static ayThreadPool*
ay_create_thread_pool(void)
{
    ayThreadPool* ptPool = malloc(sizeof(ayThreadPool));
    memset(ptPool, 0, sizeof(ayThreadPool));

    ay_create_critical_section(&ptPool->ptLock);
    ay_create_condition(&ptPool->ptWorkReady);
    ay_create_condition(&ptPool->ptWorkComplete);

    for(int i = 0; i < THREAD_COUNT; i++)
    {
        ayThreadPoolWorkerArgs* ptArgs = malloc(sizeof(ayThreadPoolWorkerArgs));
        ptArgs->ptPool = ptPool;
        ptArgs->uWorkerIndex = i;
        ay_create_thread(thread_pool_worker, ptArgs, &ptPool->atThreads[i]);
    }

    return ptPool;
}

static void
ay_destroy_thread_pool(ayThreadPool** ppPool)
{
    if(!ppPool || !*ppPool) return;
    ayThreadPool* ptPool = *ppPool;

    ay_enter_critical_section(ptPool->ptLock);
    ptPool->bShutdown = true;
    ay_condition_broadcast(ptPool->ptWorkReady);
    ay_leave_critical_section(ptPool->ptLock);

    // ay_destroy_thread joins internally, so this blocks until every
    // worker has woken up, seen bShutdown, and returned
    for(int i = 0; i < THREAD_COUNT; i++)
        ay_destroy_thread(&ptPool->atThreads[i]);

    ay_destroy_condition(&ptPool->ptWorkReady);
    ay_destroy_condition(&ptPool->ptWorkComplete);
    ay_destroy_critical_section(&ptPool->ptLock);

    free(ptPool);
    *ppPool = NULL;
}

static void
ay_thread_pool_dispatch(ayThreadPool* ptPool, ayJobFunction ptJob, void* pJobData)
{
    ay_enter_critical_section(ptPool->ptLock);

    ptPool->ptCurrentJob = ptJob;
    ptPool->pCurrentJobData = pJobData;
    ptPool->uWorkersPending = THREAD_COUNT;
    ptPool->uGeneration++;

    ay_condition_broadcast(ptPool->ptWorkReady);

    // block until every worker has finished this dispatch
    while(ptPool->uWorkersPending != 0)
        ay_condition_wait(ptPool->ptWorkComplete, ptPool->ptLock);

    ay_leave_critical_section(ptPool->ptLock);
}

static void
ay_set_pixel(ayFrameBufferData* ptData, ayVec2 input, ayVec4 tColor)
{
    if(input.x < 0)
        return;
    if(input.y < 0)
        return;
    if(input.x >= ptData->uWidth)
        return;
    if(input.y >= ptData->uHeight)
        return;

    int iRowOffset = ptData->uWidth * 4 * (int)input.y;
    int iPixelStart = iRowOffset + (int)input.x * 4;

    ptData->auData[iPixelStart + 0] = (unsigned char)tColor.r;
    ptData->auData[iPixelStart + 1] = (unsigned char)tColor.g;
    ptData->auData[iPixelStart + 2] = (unsigned char)tColor.b;
    ptData->auData[iPixelStart + 3] = (unsigned char)tColor.a;

};

static void
tile_worker_job(void* pJobData, uint32_t uWorkerIndex)
{
    (void)uWorkerIndex; // tile claiming is dynamic via atomic counter, not index-based
    ayTileWorkerData* ptTileData = (ayTileWorkerData*)pJobData;

    while(true)
    {
        uint32_t uTileInd = ay_atomic_fetch_add(ptTileData->ptNextTileIndex, 1);
        
        if(uTileInd >= ptTileData->ptData->ptTileRenderer->uTotalTiles)
            break;

        // skip tiles with no triangles bound for this draw call: rendering and
        // copying an empty tile would overwrite whatever an earlier draw call
        // already placed there with background color, since ay_add_tile_to_frame
        // does a per-pixel copy that still touches every pixel in the tile rect
        if(ptTileData->ptData->ptTileBins->uCounts[uTileInd] == 0)
            continue;

        // tile local buffers, sized off AY_MAX_TILE_SIZE since uTileSize is
        // clamped to it in ay_init_tile_renderer and can never exceed it
        uint8_t auLocalFB[AY_MAX_TILE_SIZE * AY_MAX_TILE_SIZE * 4];
        float   afLocalDB[AY_MAX_TILE_SIZE * AY_MAX_TILE_SIZE];
        memset(auLocalFB, 255, sizeof(auLocalFB));
        memset(afLocalDB, 0, sizeof(afLocalDB));

        ay_render_tile_local(*ptTileData->ptData, auLocalFB, afLocalDB, uTileInd);
    
        // using critical sections since tiles could try to copy at the same time and frame buffer is shared
        ay_enter_critical_section(ptTileData->ptFramebufferLock); 

        uint32_t uMinX = 0;
        uint32_t uMinY = 0;
        uint32_t uMaxX = 0;
        uint32_t uMaxY = 0;
        ay_get_tile_bounds(ptTileData->ptData->ptTileRenderer, uTileInd, &uMinX, &uMinY, &uMaxX, &uMaxY);
        ay_add_tile_to_frame(ptTileData->ptData->ptFrameBufferData, auLocalFB, afLocalDB, uMinX, uMinY, uMaxX, uMaxY);
        
        ay_leave_critical_section(ptTileData->ptFramebufferLock); 
    }
}

static void
upscale_worker_job(void* pJobData, uint32_t uWorkerIndex)
{
    ayUpscaleWorkerData* ptWorkerData = (ayUpscaleWorkerData*)pJobData;

    ayTexture tInputFrameBuffer = {
        .pucData = ptWorkerData->ptInputFrameBuffer->auData,
        .iWidth  = ptWorkerData->ptInputFrameBuffer->uWidth,
        .iHeight = ptWorkerData->ptInputFrameBuffer->uHeight
    };

    uint32_t uOutputHeight = ptWorkerData->ptOutputFrameBuffer->uHeight;
    uint32_t uOutputWidth  = ptWorkerData->ptOutputFrameBuffer->uWidth;
    uint32_t uRowsPerThread = uOutputHeight / THREAD_COUNT;

    uint32_t uStartY = uWorkerIndex * uRowsPerThread;
    // last worker picks up any remainder rows from integer division
    uint32_t uEndY = (uWorkerIndex == THREAD_COUNT - 1) ? uOutputHeight : (uWorkerIndex + 1) * uRowsPerThread;

    for(uint32_t uY = uStartY; uY < uEndY; uY++)
    {
        float fV = (float)uY / (float)(uOutputHeight - 1);
        int iRowOffset = uOutputWidth * 4 * uY;

        for(uint32_t uX = 0; uX < uOutputWidth; uX++)
        {
            float fU = (float)uX / (float)(uOutputWidth - 1);

            ayVec4 tColor;
            switch(ptWorkerData->tFilter)
            {
                case AY_UPSCALE_FILTER_BILINEAR:
                    tColor = ay_sample_texture_bilinear(tInputFrameBuffer, (ayVec2){fU, fV}, 4);
                    break;
                case AY_UPSCALE_FILTER_BICUBIC:
                    tColor = ay_sample_texture_bicubic(tInputFrameBuffer, (ayVec2){fU, fV}, 4);
                    break;
                case AY_UPSCALE_FILTER_NEAREST:
                default:
                    tColor = ay_sample_texture(tInputFrameBuffer, (ayVec2){fU, fV}, 4);
                    break;
            }

            int iPixelStart = iRowOffset + uX * 4;
            ptWorkerData->ptOutputFrameBuffer->auData[iPixelStart + 0] = (unsigned char)tColor.r;
            ptWorkerData->ptOutputFrameBuffer->auData[iPixelStart + 1] = (unsigned char)tColor.g;
            ptWorkerData->ptOutputFrameBuffer->auData[iPixelStart + 2] = (unsigned char)tColor.b;
            ptWorkerData->ptOutputFrameBuffer->auData[iPixelStart + 3] = (unsigned char)tColor.a;
        }
    }
}

static void
ay_upscale_frame_buffer(ayGraphicsData* ptData, ayFrameBufferData* ptInputFrameBuffer, ayFrameBufferData* ptOutputFrameBuffer, ayUpscaleFilter tFilter)
{
    // single shared job struct, each worker computes its own disjoint row
    // band from its pool-assigned index; no lock needed since bands never overlap
    ayUpscaleWorkerData tWorkerData = {
        .ptInputFrameBuffer  = ptInputFrameBuffer,
        .ptOutputFrameBuffer = ptOutputFrameBuffer,
        .tFilter             = tFilter
    };

    ay_thread_pool_dispatch(ptData->ptThreadPool, upscale_worker_job, &tWorkerData);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"