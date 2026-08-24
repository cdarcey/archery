/*
   ay_rasterize.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "ay_rasterize.h"
#include "ay_threading.h"
#define AY_RASTERIZE_PROFILE_ENABLED
#include "ay_rasterize_profile.h"

#ifdef AY_RASTERIZE_PROFILE_ENABLED
ayDrawIndProfiler g_raster_profiler = {0};
#endif

// TODO: do we test for most optimal number or make this cconfigurable
#define THREAD_COUNT 8


#include "stb_image_write.h"
#include "stb_image.h"

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
    uint32_t uTileSize;          // 32x32
    uint32_t uTilesX;            // 40 for 1280 width
    uint32_t uTilesY;            // 23 for 720 height
    uint32_t uTotalTiles;        // 920
    uint32_t uFrameBufferWidth;  // 1280
    uint32_t uFrameBufferHeight; // 720
} ayTileRenderer;

typedef struct _ayTileBins
{
    uint32_t* uTriangleIndices;  // flat array of all triangle indices
    uint32_t* uCounts;           // triangles per tile
    uint32_t  uCapacity;         // max triangles per tile
    uint32_t  uTotalTiles;       // 920
} ayTileBins;

typedef struct _ayTileWorkerData
{
    ayGraphicsData*    ptData;
    ayAtomicCounter*   ptNextTileIndex;
    ayCriticalSection* ptFramebufferLock;
} ayTileWorkerData;

typedef struct _ayGraphicsData
{
    ayFrameBufferData* ptFrameBufferData;
    const void*        pVerticies;
    ayPipeline*        ptPipeline;
    uint32_t*          puIndexBufferData;
    ayDescriptor       tDescriptors[16];

    uint32_t           uScreenWidth;
    uint32_t           uScreenHeight;

    // tile rendering infrastructure (NULL if bTileRendering == false)
    bool                    bTileRendering;
    ayTileRenderer*         ptTileRenderer;
    ayTileBins*             ptTileBins;
    ayTransformedVertex*    pTransformedVertexCache;
    size_t                  szTransformedVertexCapacity; // in vertex count, not bytes

    // internal tile bounds (set by ay_render_tile_local for per-tile rendering)
    uint32_t           uTileMinX;
    uint32_t           uTileMinY;
    uint32_t           uTileMaxX;
    uint32_t           uTileMaxY;

    // upscaling (uOutputWidth/uOutputHeight both 0 if bUpscaling == false)
    bool            bUpscaling;
    ayUpscaleInfo   tUpscaleInfo;
    ayFrameBufferData* ptOutputFrameBuffer;
} ayGraphicsData;

//-----------------------------------------------------------------------------
// [SECTION] internal forward decleration
//-----------------------------------------------------------------------------

void        ay_draw_indexed_backend(ayGraphicsData* ptData, uint32_t uFirstIndex, uint32_t uIndexCount);
static void ay_set_pixel(ayFrameBufferData* ptData, ayVec2 input, ayVec4 tColor);
static void ay_upscale_frame_buffer(ayFrameBufferData* ptInputFrameBuffer, ayFrameBufferData* ptOutputFrameBuffer);

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline float
ay_edge_function(ayVec3 one, ayVec3 two, ayVec3 three)
{
    return(two.x - one.x) * (three.y - one.y) - (two.y - one.y) * (three.x - one.x);
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
ay_ndc_to_screen(ayVec3* ayVert, uint32_t uWidth, uint32_t uHeight) 
{
    ayVert->x = uWidth *  (0.5f + 0.5f * ayVert->x);
    ayVert->y = uHeight * (0.5f + 0.5f * ayVert->y);
}

static inline void 
ay_blend_varying_component(float* dest, const float* v0, const float* v1, const float* v2, float wA, float wB, float wC) 
{
    *dest = v0[0] * wA + v1[0] * wB + v2[0] * wC;
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
        *ptData->ptTileRenderer = ay_init_tile_renderer(ptCreateGraphicsInfo->uScreenWidth, ptCreateGraphicsInfo->uScreenHeight, ptCreateGraphicsInfo->tTileSettings.uTileSize);
        
        ptData->ptTileBins = malloc(sizeof(ayTileBins));
        memset(ptData->ptTileBins, 0, sizeof(ayTileBins));
        
        ptData->ptTileBins->uCapacity = 100;
        ptData->ptTileBins->uTotalTiles = ptData->ptTileRenderer->uTotalTiles;
        
        ptData->ptTileBins->uCounts = malloc(sizeof(uint32_t) * ptData->ptTileBins->uTotalTiles);
        ptData->ptTileBins->uTriangleIndices = malloc(sizeof(uint32_t) * ptData->ptTileBins->uTotalTiles * ptData->ptTileBins->uCapacity);
        
        // transformed vertex cache starts NULL, grows on first draw
        ptData->pTransformedVertexCache = NULL;
        ptData->szTransformedVertexCapacity = 0;
    }

    ptData->bUpscaling = ptCreateGraphicsInfo->bUpscale;

    if(ptCreateGraphicsInfo->bUpscale)
    {
        ptData->tUpscaleInfo = ptCreateGraphicsInfo->tUpscaleSettings;

        // no depth needed, upscale pass only writes color
        ptData->ptOutputFrameBuffer = ay_initialize_frame_buffer(
            ptCreateGraphicsInfo->tUpscaleSettings.uOutputWidth,
            ptCreateGraphicsInfo->tUpscaleSettings.uOutputHeight,
            false
        );
    }

    return ptData;
}

void
ay_destroy_graphics(ayGraphicsData** ppData)
{
    if(!ppData || !*ppData) return;
    
    ayGraphicsData* ptData = *ppData;
    
    if(ptData->bTileRendering)
    {
        free(ptData->pTransformedVertexCache);
        
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
    ayWindow* tNewWindow = malloc(sizeof(ayWindow));
    if(!tNewWindow) return NULL;

    glfwInit();
    tNewWindow->pWindow = glfwCreateWindow(uWidth, uHeight, pcTitle, NULL, NULL);
    glfwMakeContextCurrent(tNewWindow->pWindow);

    // set up OpenGL for 2D rendering
    glViewport(0, 0, uWidth, uHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, uWidth, uHeight, 0, -1, 1); // Y-down coordinates
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // create OpenGL texture for framebuffer
    glGenTextures(1, &tNewWindow->uframebufferTexture);
    glBindTexture(GL_TEXTURE_2D, tNewWindow->uframebufferTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, uWidth, uHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // store values
    tNewWindow->uWidth  = uWidth;
    tNewWindow->uHeight = uHeight;

    return tNewWindow;
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
    ayFrameBufferData* ptSourceBuffer = ptData->ptFrameBufferData;

    if(ptData->bUpscaling)
    {
        PROFILE_START(UpscalePass);
        ay_upscale_frame_buffer(ptData->ptFrameBufferData, ptData->ptOutputFrameBuffer);
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
    // get tile bounds from renderer
    uint32_t uMinX, uMinY, uMaxX, uMaxY;
    ay_get_tile_bounds(tDataCopy.ptTileRenderer, uTileIndex, &uMinX, &uMinY, &uMaxX, &uMaxY);

    // create tile local frame buffer for ptDatatCopy to point to 
    ayFrameBufferData tTileData = {0};
    tTileData.bDepthEnabled = tDataCopy.ptFrameBufferData->bDepthEnabled;
    tTileData.auData        = auLocalFB;
    tTileData.pfDepthBuffer = afLocalDB;
    tTileData.uHeight       = uMaxY - uMinY;
    tTileData.uWidth        = uMaxX - uMinX;

    // create bins index buffer
    uint32_t uBinStart = uTileIndex * tDataCopy.ptTileBins->uCapacity;
    uint32_t uTriangleCount = tDataCopy.ptTileBins->uCounts[uTileIndex];

    // TODO: these buffers probably shouldnt be hard coded 
    // tied to triangle bin capacity so both need to match 
    uint32_t auTileIndexBuffer[300] = {0}; 

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

    ay_draw_indexed_backend(&tDataCopy, 0, uTriangleCount * 3);   
}

void 
ay_add_tile_to_frame(ayFrameBufferData* tMainFB, uint8_t* uLocalFB, uint32_t uMinX, uint32_t uMinY, uint32_t uMaxX, uint32_t uMaxY)
{
    // grab tile width (partial tiles possible when frame buffer dim is not divisible by tile size)
    uint32_t uTileWidth = uMaxX - uMinX;
    uint32_t uTileHeight = uMaxY - uMinY;
    
    // copy row by row
    for(uint32_t i = 0; i < uTileHeight; i++) 
    {
        uint32_t uSrcOffset = i * uTileWidth * 4;  // local FB is tileWidth wide
        uint32_t uDstOffset = ((uMinY + i) * tMainFB->uWidth + uMinX) * 4;
        
        memcpy(&tMainFB->auData[uDstOffset], &uLocalFB[uSrcOffset], uTileWidth * 4);
    }
}

void
ay_bin_triangles(ayGraphicsData* ptData, uint32_t uIndexCount, uint32_t uFirstIndex)
{
    // clear bins for reuse
    memset(ptData->ptTileBins->uCounts, 0, sizeof(uint32_t) * ptData->ptTileBins->uTotalTiles);

    // check/grow transformed vertex cache will eventually match largest mesh per frame
    // may need to offer some sort of reset function in the future but that can be 
    // addressed after testing more of the new draw systems
    size_t uNeededCapacity = ptData->ptPipeline->tLayout.uVertexCount;
    if(uNeededCapacity > ptData->szTransformedVertexCapacity)
    {
        ptData->pTransformedVertexCache = realloc(ptData->pTransformedVertexCache, uNeededCapacity * sizeof(ayTransformedVertex));
        // TODO: do we need check for success here? 
        ptData->szTransformedVertexCapacity = uNeededCapacity;
    }
    
    ayTransformedVertex* ptTransformedVerts = ptData->pTransformedVertexCache;

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

        // create bounding box of tiles not pixel bounding boxes like used in rasterizer
        uint32_t uStartTileX = uTriMinX / ptData->ptTileRenderer->uTileSize;
        uint32_t uStartTileY = uTriMinY / ptData->ptTileRenderer->uTileSize;
        uint32_t uStopTileX = uTriMaxX / ptData->ptTileRenderer->uTileSize;
        uint32_t uStopTileY = uTriMaxY / ptData->ptTileRenderer->uTileSize;
        uStopTileX = ay_min(ptData->ptTileRenderer->uTilesX - 1, uStopTileX);
        uStopTileY = ay_min(ptData->ptTileRenderer->uTilesY - 1, uStopTileY);

        // bin triangle to overlapping tiles
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
            }
        }
    }
}

void
ay_draw(ayGraphicsData* ptData, uint32_t uFirstVertex, uint32_t uVertexCount)
{
    bool bTiledRendering = ptData->bTileRendering;

    // set winding sign (CW need negative, CCW needs positive barycentric coords)
    float fWindingSign = (ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_CLOCKWISE) ? 1.0f : -1.0f;

    // calculate frame buffer size and cache pointers
    const uint32_t fbWidth = ptData->ptFrameBufferData->uWidth;
    const uint32_t fbHeight = ptData->ptFrameBufferData->uHeight;
    float* pfDepthBuffer = ptData->ptFrameBufferData->pfDepthBuffer;

    ayTransformedVertex* transformedVerts = NULL;
    if(bTiledRendering) 
    {
        transformedVerts = ptData->pTransformedVertexCache;
    }

    for(uint32_t i = 0; i < uVertexCount; i += 3)
    {
        const uint32_t uIndex0 = uFirstVertex + i;
        const uint32_t uIndex1 = uFirstVertex + i + 1;
        const uint32_t uIndex2 = uFirstVertex + i + 2;

        ayVec3 tVertex0;
        ayVec3 tVertex1;
        ayVec3 tVertex2;
        ayVaryingData tVaryingData0 = {0};
        ayVaryingData tVaryingData1 = {0};
        ayVaryingData tVaryingData2 = {0};

        if(bTiledRendering) // tiled path - read from cache
        {
            tVertex0 = transformedVerts[uIndex0].tScreenPos;
            tVertex1 = transformedVerts[uIndex1].tScreenPos;
            tVertex2 = transformedVerts[uIndex2].tScreenPos;
            
            tVaryingData0 = transformedVerts[uIndex0].tVaryings;
            tVaryingData1 = transformedVerts[uIndex1].tVaryings;
            tVaryingData2 = transformedVerts[uIndex2].tVaryings;
        }
        else // non-tiled path - run vertex shader
        {
            const char* pcVtxBuffer = (char*)ptData->pVerticies;
            
            tVertex0 = ay_run_vertex_shader(ptData, uIndex0, pcVtxBuffer, &tVaryingData0);
            tVertex1 = ay_run_vertex_shader(ptData, uIndex1, pcVtxBuffer, &tVaryingData1);
            tVertex2 = ay_run_vertex_shader(ptData, uIndex2, pcVtxBuffer, &tVaryingData2);

            // frame buffer space transformation
            ay_ndc_to_screen(&tVertex0, fbWidth, fbHeight);
            ay_ndc_to_screen(&tVertex1, fbWidth, fbHeight);
            ay_ndc_to_screen(&tVertex2, fbWidth, fbHeight);
        }

        // edge function for entire triangle 
        float ABC = (float)ay_edge_function(tVertex0, tVertex1, tVertex2);

        // cull backfaces based on winding
        if(ABC > 0 && ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_CLOCKWISE) continue;
        if(ABC < 0 && ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_COUNTER_CLOCKWISE) continue;
        if(ABC == 0) continue;  // degenerate triangle

        // Bounding box with clamping
        uint32_t uTriMinX = ay_min3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x);
        uint32_t uTriMinY = ay_min3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y);
        uint32_t uTriMaxX = ay_max3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x);
        uint32_t uTriMaxY = ay_max3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y);
        
        // expand triangle bounds by 1 pix to ensure we dont miss data
        if(uTriMinX > 0) uTriMinX -= 1;
        if(uTriMinY > 0) uTriMinY -= 1;
        uTriMaxX += 1;
        uTriMaxY += 1;

        // clamp to screen bounds
        uTriMinX = ay_max(0, uTriMinX);
        uTriMinY = ay_max(0, uTriMinY);
        uTriMaxX = ay_min(ptData->uScreenWidth, uTriMaxX);
        uTriMaxY = ay_min(ptData->uScreenHeight, uTriMaxY);

        // if tiled rendering, clamp to tile bounds
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

        // initialize at start of row
        float ABP = (ABa * minX + ABb * minY + ABc) * fWindingSign;
        float BCP = (BCa * minX + BCb * minY + BCc) * fWindingSign;
        float CAP = (CAa * minX + CAb * minY + CAc) * fWindingSign;

        const float invABC = 1.0f / ABC;

        // varying system
        int varyDataOffset = 0;
        ayVaryingData blendedVaryingData = {0};

        for(uint32_t j = 0; j < 16; j++)
        {
            blendedVaryingData._auOffset[j] = tVaryingData0._auOffset[j];
            blendedVaryingData.atTypes[j] = tVaryingData0.atTypes[j];
        }

        int iVaryingCount = 0;
        for(int varyIndex = 0; varyIndex < 16; varyIndex++)
        {
            if(tVaryingData0.atTypes[varyIndex] == AY_VARYING_TYPE_NONE)
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

        if(ptData->ptFrameBufferData->bDepthEnabled)
        {
            for(uint32_t y = minY; y < maxY; y++)
            {
                uint32_t uRowDepthIndex = uDepthIndex;
                float rowABP = ABP;
                float rowBCP = BCP;
                float rowCAP = CAP;

                for(uint32_t x = minX; x < maxX; x++)
                {
                    if(rowABP >= 0 && rowBCP >= 0 && rowCAP >= 0)
                    {
                        // for tiled or not tiled drawing
                        uint32_t uLocalX = x - ptData->uTileMinX;
                        uint32_t uLocalY = y - ptData->uTileMinY;
                        uint32_t uBufferDepthIndex = bTiledRendering ? (uLocalY * fbWidth + uLocalX) : uRowDepthIndex;

                        varyDataOffset = 0;

                        const float weightA = rowBCP * invABC;
                        const float weightB = rowCAP * invABC;
                        const float weightC = rowABP * invABC;

                        ayPixelShaderBuiltIns tBuiltIns = {
                            .tUV = {x, y}
                        };

                        for(int varyIndex = 0; varyIndex < iVaryingCount; varyIndex++)
                        {
                            int componentCount = iCompCount[varyIndex];
                            if(componentCount > 0)
                            {
                                const float* v0 = (const float*)&tVaryingData0.acVaryingData[varyDataOffset];
                                const float* v1 = (const float*)&tVaryingData1.acVaryingData[varyDataOffset];
                                const float* v2 = (const float*)&tVaryingData2.acVaryingData[varyDataOffset];
                                float* dest = (float*)&blendedVaryingData.acVaryingData[varyDataOffset];

                                for(int c = 0; c < componentCount; c++)
                                {
                                    dest[c] = v0[c] * weightA + v1[c] * weightB + v2[c] * weightC;
                                }
                                varyDataOffset += componentCount * sizeof(float);
                            }
                        }

                        float fPixelDepth = tVertex0.z * weightA + tVertex1.z * weightB + tVertex2.z * weightC;

                        if(fPixelDepth > pfDepthBuffer[uBufferDepthIndex]) 
                        {
                            ayVec4 tFinalColor = ptData->ptPipeline->tPixelShader(tBuiltIns, ptData->tDescriptors, &blendedVaryingData);
                            float alphaScale = tFinalColor.a / 255.0f;
                            tFinalColor.r *= alphaScale;
                            tFinalColor.g *= alphaScale;
                            tFinalColor.b *= alphaScale;
                            
                            ayVec2 tWritePos = bTiledRendering ? (ayVec2){uLocalX, uLocalY} : (ayVec2){x, y};
                            ay_set_pixel(ptData->ptFrameBufferData, tWritePos, tFinalColor);

                            pfDepthBuffer[uBufferDepthIndex] = fPixelDepth;
                        }
                    }
                    // incrementally update edge functions for next pixel in row
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
            for(uint32_t y = minY; y < maxY; y++)
            {
                float rowABP = ABP;
                float rowBCP = BCP;
                float rowCAP = CAP;

                for(uint32_t x = minX; x < maxX; x++)
                {
                    if(rowABP >= 0 && rowBCP >= 0 && rowCAP >= 0)
                    {
                        uint32_t uLocalX = x - ptData->uTileMinX;
                        uint32_t uLocalY = y - ptData->uTileMinY;
                        
                        varyDataOffset = 0;

                        const float weightA = rowBCP * invABC;
                        const float weightB = rowCAP * invABC;
                        const float weightC = rowABP * invABC;

                        ayPixelShaderBuiltIns tBuiltIns = {
                            .tUV = {x, y}
                        };

                        for(int varyIndex = 0; varyIndex < iVaryingCount; varyIndex++)
                        {
                            int componentCount = iCompCount[varyIndex];
                            if(componentCount > 0)
                            {
                                const float* v0 = (const float*)&tVaryingData0.acVaryingData[varyDataOffset];
                                const float* v1 = (const float*)&tVaryingData1.acVaryingData[varyDataOffset];
                                const float* v2 = (const float*)&tVaryingData2.acVaryingData[varyDataOffset];
                                float* dest = (float*)&blendedVaryingData.acVaryingData[varyDataOffset];

                                for(int c = 0; c < componentCount; c++)
                                {
                                    dest[c] = v0[c] * weightA + v1[c] * weightB + v2[c] * weightC;
                                }
                                varyDataOffset += componentCount * sizeof(float);
                            }
                        }

                        ayVec4 tFinalColor = ptData->ptPipeline->tPixelShader(tBuiltIns, ptData->tDescriptors, &blendedVaryingData);
                        float alphaScale = tFinalColor.a / 255.0f;
                        tFinalColor.r *= alphaScale;
                        tFinalColor.g *= alphaScale;
                        tFinalColor.b *= alphaScale;
                        
                        ayVec2 tWritePos = bTiledRendering ? (ayVec2){uLocalX, uLocalY} : (ayVec2){x, y};
                        ay_set_pixel(ptData->ptFrameBufferData, tWritePos, tFinalColor);
                    }
                    // incrementally update edge functions for next pixel in row
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
    }
}

void 
ay_draw_indexed_backend(ayGraphicsData* ptData, uint32_t uFirstIndex, uint32_t uIndexCount)
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

        // edge function for entire triangle 
        float ABC = (float)ay_edge_function(tVertex0, tVertex1, tVertex2);

        // cull backfaces based on winding
        if(ABC > 0 && ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_CLOCKWISE) continue;
        if(ABC < 0 && ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_COUNTER_CLOCKWISE) continue;
        if(ABC == 0) continue;  // degenerate triangle

        // get triangle bounding box
        uint32_t uTriMinX = ay_min3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x);
        uint32_t uTriMinY = ay_min3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y);
        uint32_t uTriMaxX = ay_max3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x);
        uint32_t uTriMaxY = ay_max3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y);
        
        // expand triangle bounds by 1 pix to ensure we dont miss data
        if(uTriMinX > 0) uTriMinX -= 1;
        if(uTriMinY > 0) uTriMinY -= 1;
        uTriMaxX += 1;
        uTriMaxY += 1;

        // clamp to screen bounds
        uTriMinX = ay_max(0, uTriMinX);
        uTriMinY = ay_max(0, uTriMinY);
        uTriMaxX = ay_min(ptData->uScreenWidth, uTriMaxX);
        uTriMaxY = ay_min(ptData->uScreenHeight, uTriMaxY);

        // if tiled rendering, clamp to tile bounds
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

        // initialize at start of row
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

                        // calculate weights
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

                        // depth checking and setting pixel/depth buffer
                        PROFILE_START(DepthTest);
                        float fPixelDepth = tVertex0.z * weightA + tVertex1.z * weightB + tVertex2.z * weightC;
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
                    // incrementally update edge functions for next pixel in row
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
                    // incrementally update edge functions for next pixel in row
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

void* 
tile_worker_thread(void* pData) 
{
    ayTileWorkerData* ptTileData = (ayTileWorkerData*)pData;

    while(true)
    {
        uint32_t uTileInd = ay_atomic_fetch_add(ptTileData->ptNextTileIndex, 1);
        
        if(uTileInd >= ptTileData->ptData->ptTileRenderer->uTotalTiles)
            break;

        // tile local buffers
        // TODO: make configureable and remove hard coded values
        uint8_t auLocalFB[32 * 32 * 4];
        float   afLocalDB[32 * 32];
        memset(auLocalFB, 255, sizeof(auLocalFB));
        memset(afLocalDB, 0, sizeof(afLocalDB));

        // render tile
        ay_render_tile_local(*ptTileData->ptData, auLocalFB, afLocalDB, uTileInd);
    
        // using critical sections since tiles could try to copy at the same time and frame buffer is shared
        ay_enter_critical_section(ptTileData->ptFramebufferLock); 

        uint32_t uMinX, uMinY, uMaxX, uMaxY;
        ay_get_tile_bounds(ptTileData->ptData->ptTileRenderer, uTileInd, &uMinX, &uMinY, &uMaxX, &uMaxY);
        ay_add_tile_to_frame(ptTileData->ptData->ptFrameBufferData, auLocalFB, uMinX, uMinY, uMaxX, uMaxY);
        
        ay_leave_critical_section(ptTileData->ptFramebufferLock); 
    }
    
    return NULL;
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
        // frame buffer then join all threads and clean up
        // TODO: address if we want to give threads a global lifespan 
        ay_bin_triangles(ptData, uIndexCount, uFirstIndex);

        ayAtomicCounter* tTileCounter;
        if(ay_create_atomic_counter(0, &tTileCounter) != AY_ATOMICS_RESULT_SUCCESS) 
            return;

        ayCriticalSection* tCriticalSection;
        if(ay_create_critical_section(&tCriticalSection) != AY_THREAD_RESULT_SUCCESS)
        {
            ay_destroy_atomic_counter(&tTileCounter);
            return;
        }

        ayTileWorkerData tTileData = {
            .ptData            = ptData,
            .ptNextTileIndex   = tTileCounter,
            .ptFramebufferLock = tCriticalSection
        };

        ayThread* tThreads[THREAD_COUNT];
        for(int i = 0; i < THREAD_COUNT; i++)
        {
            ay_create_thread(tile_worker_thread, &tTileData, &tThreads[i]);
        }

        // wait for all threads to complete
        for(int i = 0; i < THREAD_COUNT; i++)
        {
            ay_join_thread(tThreads[i]);
            ay_destroy_thread(&tThreads[i]);
        }

        ay_destroy_atomic_counter(&tTileCounter);
        ay_destroy_critical_section(&tCriticalSection);
    }
    else
    {
        ay_draw_indexed_backend(ptData, uFirstIndex, uIndexCount);
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
    memset(ptData, 0, sizeof(ayFrameBufferData));

    ptData->uWidth = uWidth;
    ptData->uHeight = uHeight;
    ptData->auData = malloc(sizeof(char) * 4 * uWidth * uHeight);
    memset(ptData->auData, 0, sizeof(char) * 4 * uWidth * uHeight);

    if(bDepthEnabled)
    {
        // 0 is our clear value for depth buffer
        ptData->bDepthEnabled = bDepthEnabled;
        ptData->pfDepthBuffer = malloc(sizeof(float) * uHeight * uWidth);
        memset(ptData->pfDepthBuffer, 0, ptData->uWidth * ptData->uHeight * sizeof(float));
    }

    return ptData;
};

void
ay_output_frame_buffer(ayFrameBufferData* ptData)
{
    stbi_write_png("output.png", ptData->uWidth, ptData->uHeight, 4, ptData->auData, sizeof(char) * 4 * ptData->uWidth);
};

void
ay_clear_frame_buffer(ayFrameBufferData* ptData)
{
    memset(ptData->auData, 255, sizeof(char) * (ptData->uHeight * 4) * (ptData->uWidth));
        
    if(ptData->bDepthEnabled && ptData->pfDepthBuffer)
    {
        // depth buffer clear value is 0
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

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

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
ay_upscale_frame_buffer(ayFrameBufferData* ptInputFrameBuffer, ayFrameBufferData* ptOutputFrameBuffer)
{
    ayTexture tLowResTexture = {
        .pucData = ptInputFrameBuffer->auData,
        .iWidth  = ptInputFrameBuffer->uWidth,
        .iHeight = ptInputFrameBuffer->uHeight
    };

    for(uint32_t uY = 0; uY < ptOutputFrameBuffer->uHeight; uY++)
    {
        float fV = (float)uY / (float)(ptOutputFrameBuffer->uHeight - 1);
        int iRowOffset = ptOutputFrameBuffer->uWidth * 4 * uY;

        for(uint32_t uX = 0; uX < ptOutputFrameBuffer->uWidth; uX++)
        {
            float fU = (float)uX / (float)(ptOutputFrameBuffer->uWidth - 1);

            ayVec4 tColor = ay_sample_texture(tLowResTexture, (ayVec2){fU, fV}, 4);

            int iPixelStart = iRowOffset + uX * 4;
            ptOutputFrameBuffer->auData[iPixelStart + 0] = (unsigned char)tColor.r;
            ptOutputFrameBuffer->auData[iPixelStart + 1] = (unsigned char)tColor.g;
            ptOutputFrameBuffer->auData[iPixelStart + 2] = (unsigned char)tColor.b;
            ptOutputFrameBuffer->auData[iPixelStart + 3] = (unsigned char)tColor.a;
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"