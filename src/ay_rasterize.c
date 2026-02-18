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
#include <stdbool.h>


#include "ay_rasterize.h"
#include "ay_threading.h"
#define AY_RASTERIZE_PROFILE_ENABLED
#include "ay_rasterize_profile.h"

#ifdef AY_RASTERIZE_PROFILE_ENABLED
ayDrawIndProfiler g_raster_profiler = {0};
#endif

#define THREAD_COUNT 8


#include "stb_image_write.h"
#include "stb_image.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _ayTileRenderer
{
    uint32_t uTileSize;          // 32x32
    uint32_t uTilesX;            // 40
    uint32_t uTilesY;            // 23
    uint32_t uTotalTiles;        // 920
    uint32_t uFrameBufferWidth;  // 1280
    uint32_t uFrameBufferHeight; // 720
    
} ayTileRenderer;

typedef struct _ayTileBins
{
    uint32_t* uTriangleIndices;  // all triangle indices (flat array)
    uint32_t* uCounts;           // triangles per tile 
    uint32_t  uCapacity;         // max triangles per tile 
    uint32_t  uTotalTiles;       // 920
} ayTileBins;

typedef struct _ayTileWorkerData
{
    ayGraphicsData*    ptData;
    ayTileRenderer     tRenderer;
    ayTileBins*        ptBins;
    ayAtomicCounter*   ptNextTileIndex;
    ayCriticalSection* ptFramebufferLock;
    uint32_t           uFirstIndex;
    uint32_t           uIndexCount;
} ayTileWorkerData;

typedef struct _ayGraphicsData
{
    ayFrameBufferData* ptFrameBufferData;
    const void*        pVerticies;
    ayPipeline*        ptPipeline;
    uint32_t*          puIndexBufferData;
    ayDescriptor       tDescriptors[16]; 

    // store for tile based rendering
    uint32_t           uScreenWidth;
    uint32_t           uScreenHeight;

    // tile rendering info (0 means full screen rendering)
    uint32_t           uTileMinX;
    uint32_t           uTileMinY;
    uint32_t           uTileMaxX;
    uint32_t           uTileMaxY;
} ayGraphicsData;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline float
ay_edge_function(ayVec3 one, ayVec3 two, ayVec3 three)
{
    return(two.x - one.x) * (three.y - one.y) - (two.y - one.y) * (three.x - one.x);
};

// 2-value minimum 
static inline int 
ay_min(int a, int b) 
{
    return (a < b) ? a : b;
}

// 3-value minimum 
static inline int 
ay_min3(int a, int b, int c) 
{
    int temp = (a < b) ? a : b;
    return (temp < c) ? temp : c; 
}

// 2-value maximum 
static inline int 
ay_max(int a, int b) 
{
    return (a > b) ? a : b;
}

// 3-value maximum 
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

static void 
ay_set_pixel(ayFrameBufferData* ptData, ayVec2 input, ayVec4 tColor);

ayTileRenderer 
ay_init_tile_renderer(uint32_t uFrameBufferWidth, uint32_t uFrameBufferHeight)
{
    ayTileRenderer tRenderer;

    tRenderer.uFrameBufferWidth = uFrameBufferWidth;
    tRenderer.uFrameBufferHeight = uFrameBufferHeight;
    tRenderer.uTileSize = 32;
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
    *puMaxX = min(*puMinX + ptRenderer->uTileSize, ptRenderer->uFrameBufferWidth);
    *puMaxY = min(*puMinY + ptRenderer->uTileSize, ptRenderer->uFrameBufferHeight);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

ayGraphicsData*
initialize_graphics(uint32_t uScreenWidth, uint32_t uScreenHeight)
{
    ayGraphicsData* ptData = malloc(sizeof(ayGraphicsData));
    if(!ptData) return NULL;
    memset(ptData, 0, sizeof(ayGraphicsData));

    ptData->uScreenWidth = uScreenWidth;
    ptData->uScreenHeight = uScreenHeight;

    return ptData;
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
ay_present_frame(ayWindow* ptWindow, ayFrameBufferData* ptFrameBuffer)
{
    // upload framebuffer pixels to gl texture
    glBindTexture(GL_TEXTURE_2D, ptWindow->uframebufferTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ptFrameBuffer->uWidth, ptFrameBuffer->uHeight, 
                    GL_RGBA, GL_UNSIGNED_BYTE, ptFrameBuffer->auData);
    
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
ay_render_tile_local(ayGraphicsData tDataCopy, uint8_t* auLocalFB, float* afLocalDB, 
    uint32_t uTileIndex, ayTileBins* tTileBins, uint32_t uMinX, uint32_t uMinY, uint32_t uMaxX, uint32_t uMaxY,
    uint32_t uFirstIndex, uint32_t uIndexCount)
{
    // create tile local frame buffer  for ptDatatCopy to point to 
    ayFrameBufferData tTileData = {0};
    tTileData.bDepthEnabled = tDataCopy.ptFrameBufferData->bDepthEnabled;
    tTileData.auData        = auLocalFB;
    tTileData.pfDepthBuffer = afLocalDB;
    tTileData.uHeight       = uMaxY - uMinY;
    tTileData.uWidth        = uMaxX - uMinX;

    // create bins index buffer
    uint32_t uBinStart = uTileIndex * tTileBins->uCapacity;
    uint32_t uTriangleCount = tTileBins->uCounts[uTileIndex];

    uint32_t auTileIndexBuffer[300] = {0}; // TODO: not efficient and does not handle case if bins capactiy is expanded
                                           // just setting to max size of bin with capacity of 15 triangles * 3 indicies

    for(uint32_t i = 0; i < uTriangleCount; i++) 
    {
        uint32_t uTriIdx = tTileBins->uTriangleIndices[uBinStart + i];
        
        // copy this triangle's 3 vertex indices from actual triangle lists
        // the traingles actual data isnt in the bin but just indicies to 
        // be able to access so that we are not storing extra data that we
        // already have stored
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

    ay_draw_indexed(&tDataCopy, 0, uTriangleCount * 3);   
}

void 
ay_add_tile_to_frame(ayFrameBufferData* tMainFB, uint8_t* uLocalFB, uint32_t uMinX, uint32_t uMinY, uint32_t uMaxX, uint32_t uMaxY)
{
    // grab tile width(partial tiles possible when frame buffer dim is not divisible by tile size)
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

ayTileBins*
ay_bin_triangles(ayGraphicsData* ptData, ayTileRenderer tRenderer, uint32_t uIndexCount, uint32_t uFirstIndex)
{
    // we create all tile bins once at the beggining of the frame
    ayTileBins* tTileBins = malloc(sizeof(ayTileBins));
    if(!tTileBins) return NULL;
    memset(tTileBins, 0, sizeof(ayTileBins));

    tTileBins->uCapacity = 100; // TODO: should i make this configurable 
    tTileBins->uTotalTiles = tRenderer.uTotalTiles;

    tTileBins->uCounts = malloc(sizeof(uint32_t) * tTileBins->uTotalTiles); // 1 tile count for each tile bin
    if(!tTileBins->uCounts) return NULL;
    memset(tTileBins->uCounts, 0, sizeof(uint32_t) * tTileBins->uTotalTiles);

    tTileBins->uTriangleIndices = malloc(sizeof(uint32_t) * tTileBins->uTotalTiles * tTileBins->uCapacity);
    if(!tTileBins->uTriangleIndices) return NULL;
    memset(tTileBins->uTriangleIndices, 0, sizeof(uint32_t) * tTileBins->uTotalTiles * tTileBins->uCapacity);

    uint32_t uTriangleCount = uIndexCount / 3;

    // check every triangle and put in tile bins that have bounding box collisions
    for(uint32_t i = 0; i < uIndexCount; i += 3)
    {
        // get triangle verticies from index buffer 
        const uint32_t uIndex0 = ptData->puIndexBufferData[uFirstIndex + i];
        const uint32_t uIndex1 = ptData->puIndexBufferData[uFirstIndex + i + 1];
        const uint32_t uIndex2 = ptData->puIndexBufferData[uFirstIndex + i + 2];

        // not sure if running the vertex shader twice is the solution here 
        // but with the vertex buffer system is the easiest thing i can think 
        // of at the moment
        // TODO: once tile system is working with multi threading it may be 
        // possible to cache this data and remove from draw call to reduce 
        // code that is rerunning 
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

        // get triangle bounding box
        uint32_t uTriMinX = ay_min3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x);
        uint32_t uTriMinY = ay_min3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y);
        uint32_t uTriMaxX = ay_max3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x);
        uint32_t uTriMaxY = ay_max3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y);

        // create bounding box of tiles to only check the tiles that we have to check and clamp to screen
        uint32_t uStartTileX = uTriMinX / tRenderer.uTileSize;
        uint32_t uStartTileY = uTriMinY / tRenderer.uTileSize;
        uint32_t uStopTileX = uTriMaxX / tRenderer.uTileSize;
        uint32_t uStopTileY = uTriMaxY / tRenderer.uTileSize;
        uStopTileX = ay_min(tRenderer.uTilesX - 1, uStopTileX);
        uStopTileY = ay_min(tRenderer.uTilesY - 1, uStopTileY);

        // add triangle to all tiles, we arent doing expensive triangle intersection tests
        // so we will waste some work by adding tiles that do not need to be checked but we 
        // have early out checks in draw call so the conservative approach should be good
        for(uint32_t uY = uStartTileY; uY <= uStopTileY; uY++)
        {
            for(uint32_t uX = uStartTileX; uX <= uStopTileX; uX++)
            {
                uint32_t uTileIndex = uY * tRenderer.uTilesX + uX;
                
                // add triangle to this tile's bin
                uint32_t uBinStart = uTileIndex * tTileBins->uCapacity;
                uint32_t uCount = tTileBins->uCounts[uTileIndex];
                if(uCount < tTileBins->uCapacity) 
                {
                    tTileBins->uTriangleIndices[uBinStart + uCount] = i / 3; // triangle index 
                    tTileBins->uCounts[uTileIndex]++;
                }
                else
                {
                    // TODO: handle overflow (realloc or warn)
                }
            }
        }
    }
    
    return tTileBins;
}

static void
ay_free_tile_bins(ayTileBins** ppBins)
{
    if(!ppBins || !*ppBins) return;
    
    free((*ppBins)->uTriangleIndices);
    free((*ppBins)->uCounts);
    free(*ppBins);
    *ppBins = NULL;
}

void
ay_draw(ayGraphicsData* ptData, uint32_t uFirstVertex, uint32_t uVertexCount)
{
    // TODO: add indexed draw flow with optimizations
    // set winding sign
    float fWindingSign = (ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_CLOCKWISE) ? 1.0f : -1.0f;

    // calculate frame buffer size
    const uint32_t fbWidth = ptData->ptFrameBufferData->uWidth;
    const uint32_t fbHeight = ptData->ptFrameBufferData->uHeight;

    ayVec2 vertexP = {.x = 0, .y = 0};

    for(uint32_t i = 0; i < uVertexCount; i += 3)
    {
        const uint32_t uIndex0 = uFirstVertex + i;
        const uint32_t uIndex1 = uFirstVertex + i + 1;
        const uint32_t uIndex2 = uFirstVertex + i + 2;

        // type casting void buffer
        const char* pcVtxBuffer = (char*)ptData->pVerticies;

        // defining varying data
        ayVaryingData tVaryingData0 = {0};
        ayVaryingData tVaryingData1 = {0};
        ayVaryingData tVaryingData2 = {0};

        // vertex shader stage
        ayVec3 tVertex0 = ay_run_vertex_shader(ptData, uIndex0, pcVtxBuffer, &tVaryingData0);
        ayVec3 tVertex1 = ay_run_vertex_shader(ptData, uIndex1, pcVtxBuffer, &tVaryingData1);
        ayVec3 tVertex2 = ay_run_vertex_shader(ptData, uIndex2, pcVtxBuffer, &tVaryingData2);

        // frame buffer space transformation
        ay_ndc_to_screen(&tVertex0, fbWidth, fbHeight);
        ay_ndc_to_screen(&tVertex1, fbWidth, fbHeight);
        ay_ndc_to_screen(&tVertex2, fbWidth, fbHeight);

        // edge function for entire triangle 
        float ABC = (float)ay_edge_function(tVertex0, tVertex1, tVertex2);

        // Bounding box with clamping
        const uint32_t minX = ay_max(0, ay_min3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x) - 1);
        const uint32_t minY = ay_max(0, ay_min3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y) - 1);
        const uint32_t maxX = ay_min(fbWidth-1, ay_max3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x) + 1);
        const uint32_t maxY = ay_min(fbHeight-1, ay_max3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y) + 1);

        // Precompute edge function coefficients
        float ABa, ABb, ABc, BCa, BCb, BCc, CAa, CAb, CAc;
        ay_compute_edge_coeffs(tVertex0, tVertex1, &ABa, &ABb, &ABc);
        ay_compute_edge_coeffs(tVertex1, tVertex2, &BCa, &BCb, &BCc);
        ay_compute_edge_coeffs(tVertex2, tVertex0, &CAa, &CAb, &CAc);

        // initialize at start of row
        float ABP = (ABa * minX + ABb * minY + ABc) * fWindingSign;
        float BCP = (BCa * minX + BCb * minY + BCc) * fWindingSign;
        float CAP = (CAa * minX + CAb * minY + CAc) * fWindingSign;

        const float invABC = 1.0f / ABC;

        for(vertexP.y = (float)minY; vertexP.y <= (float)maxY; vertexP.y++)
        {
            float rowABP = ABP;
            float rowBCP = BCP;
            float rowCAP = CAP;

            for(vertexP.x = (float)minX; vertexP.x <= (float)maxX; vertexP.x++)
            {
                if(rowABP >= 0 && rowBCP >= 0 && rowCAP >= 0)
                {
                    const float weightA = rowBCP * invABC;
                    const float weightB = rowCAP * invABC;
                    const float weightC = rowABP * invABC;

                    ayPixelShaderBuiltIns tBuiltIns = {
                        .tUV = {vertexP.x, vertexP.y}
                    };

                    // Varying system
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

                    for(int varyIndex = 0; varyIndex < iVaryingCount; varyIndex++)
                    {
                        ayVaryingType type = tVaryingData0.atTypes[varyIndex];
                        int componentCount = 0;

                        if(type == AY_VARYING_TYPE_FLOAT) componentCount = 1;
                        else if(type == AY_VARYING_TYPE_VEC2) componentCount = 2;
                        else if(type == AY_VARYING_TYPE_VEC3) componentCount = 3;
                        else if(type == AY_VARYING_TYPE_VEC4) componentCount = 4;

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

                    if(ptData->ptFrameBufferData->bDepthEnabled)
                    {
                        float fPixelDepth = tVertex0.z * weightA + tVertex1.z * weightB + tVertex2.z * weightC;;
                        int iDepthIndex = (int)vertexP.y * fbWidth + (int)vertexP.x;

                        if(fPixelDepth > ptData->ptFrameBufferData->pfDepthBuffer[iDepthIndex]) 
                        {
                            // run pixel shader
                            ayVec4 tFinalColor = ptData->ptPipeline->tPixelShader(tBuiltIns, ptData->tDescriptors, &blendedVaryingData);
                            float alphaScale = tFinalColor.a / 255.0f;
                            tFinalColor.r *= alphaScale;
                            tFinalColor.g *= alphaScale;
                            tFinalColor.b *= alphaScale;
                            ay_set_pixel(ptData->ptFrameBufferData, vertexP, tFinalColor);

                            // update depth buffer
                            ptData->ptFrameBufferData->pfDepthBuffer[iDepthIndex] = fPixelDepth;
                        }
                    }
                    else
                    {
                        // no depth
                        ayVec4 tFinalColor = ptData->ptPipeline->tPixelShader(tBuiltIns, ptData->tDescriptors, &blendedVaryingData);
                        float alphaScale = tFinalColor.a / 255.0f;
                        tFinalColor.r *= alphaScale;
                        tFinalColor.g *= alphaScale;
                        tFinalColor.b *= alphaScale;
                        ay_set_pixel(ptData->ptFrameBufferData, vertexP, tFinalColor);
                    }
                }
                // Incrementally update edge functions for next pixel in row
                rowABP += ABa;
                rowBCP += BCa;
                rowCAP += CAa;
            }
            // Incrementally update edge functions for next row
            ABP += ABb;
            BCP += BCb;
            CAP += CAb;
        }
    }
}

void
ay_draw_indexed(ayGraphicsData* ptData, uint32_t uFirstIndex, uint32_t uIndexCount)
{
    bool bTiledRendering = (ptData->uTileMaxX > 0); // check if tilerendering or full frame

    // set winding sign (CW need negative, CCW needs positive barycentric coords)
    float fWindingSign = (ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_CLOCKWISE) ? -1.0f : 1.0f;

    // calculate frame buffer size cache pointer to depth buffer
    const uint32_t fbWidth = ptData->ptFrameBufferData->uWidth;
    const uint32_t fbHeight = ptData->ptFrameBufferData->uHeight;
    float* pfDepthBuffer = ptData->ptFrameBufferData->pfDepthBuffer;

    // main triangle loop
    for(uint32_t i = 0; i < uIndexCount; i += 3)
    {
        PROFILE_START(VertexShader);
        const uint32_t uIndex0 = ptData->puIndexBufferData[uFirstIndex + i];
        const uint32_t uIndex1 = ptData->puIndexBufferData[uFirstIndex + i + 1];
        const uint32_t uIndex2 = ptData->puIndexBufferData[uFirstIndex + i + 2];

        // type casting void buffer & defining varying data to get out of vertex shader
        ayVaryingData tVaryingData0 = {0};
        ayVaryingData tVaryingData1 = {0};
        ayVaryingData tVaryingData2 = {0};

        // vertex shader stage
        const char* pcVtxBuffer = (char*)ptData->pVerticies;
        ayVec3 tVertex0 = ay_run_vertex_shader(ptData, uIndex0, pcVtxBuffer, &tVaryingData0);
        ayVec3 tVertex1 = ay_run_vertex_shader(ptData, uIndex1, pcVtxBuffer, &tVaryingData1);
        ayVec3 tVertex2 = ay_run_vertex_shader(ptData, uIndex2, pcVtxBuffer, &tVaryingData2);
        PROFILE_END(VertexShader);

        // frame buffer space transformation
        PROFILE_START(TriangleSetup);
        ay_ndc_to_screen(&tVertex0, ptData->uScreenWidth, ptData->uScreenHeight);
        ay_ndc_to_screen(&tVertex1, ptData->uScreenWidth, ptData->uScreenHeight);
        ay_ndc_to_screen(&tVertex2, ptData->uScreenWidth, ptData->uScreenHeight);

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
                    // increment depth index for next pixel
                    uRowDepthIndex += 1; 
                }
                // incrementally update edge functions for next row
                ABP += ABb;
                BCP += BCb;
                CAP += CAb;
                // increment depth index for next row
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
        if(uTileInd >= ptTileData->tRenderer.uTotalTiles) // frame is fully renderered
            break;

        // tile local buffers
        uint8_t auLocalFB[32 * 32 * 4];
        float   afLocalDB[32 * 32];
        memset(auLocalFB, 255, sizeof(auLocalFB));
        memset(afLocalDB, 0, sizeof(afLocalDB));

        uint32_t uMaxX, uMaxY, uMinX, uMinY;
        ay_get_tile_bounds(&ptTileData->tRenderer, uTileInd, &uMinX, &uMinY, &uMaxX, &uMaxY);

        ay_render_tile_local(*ptTileData->ptData, auLocalFB, afLocalDB, uTileInd, ptTileData->ptBins, 
                uMinX, uMinY, uMaxX, uMaxY, ptTileData->uFirstIndex, ptTileData->uIndexCount);
    
        // frame buffer copy needs to block since the main frame buffer will be global
        ay_enter_critical_section(ptTileData->ptFramebufferLock); 
        ay_add_tile_to_frame(ptTileData->ptData->ptFrameBufferData, auLocalFB, uMinX, uMinY, uMaxX, uMaxY);
        ay_leave_critical_section(ptTileData->ptFramebufferLock); 
        
    }
    
    return NULL;
}

void 
ay_draw_indexed_tiled(ayGraphicsData* ptData, uint32_t uFirstIndex, uint32_t uIndexCount)
{
    ayTileRenderer tRenderer = ay_init_tile_renderer(ptData->ptFrameBufferData->uWidth, 
                                                     ptData->ptFrameBufferData->uHeight);
    ayTileBins* tTileBins = ay_bin_triangles(ptData, tRenderer, uIndexCount, uFirstIndex);

    // create synchronization primitives
    ayAtomicCounter* tTileCounter;
    if(ay_create_atomic_counter(0, &tTileCounter) != AY_ATOMICS_RESULT_SUCCESS) 
        return;

    ayCriticalSection* tCriticalSection;
    if(ay_create_critical_section(&tCriticalSection) != AY_THREAD_RESULT_SUCCESS)
    {
        ay_destroy_atomic_counter(&tTileCounter);
        return;
    }

    // setup worker data
    ayTileWorkerData tTileData = {
        .ptBins            = tTileBins,
        .ptData            = ptData,
        .ptFramebufferLock = tCriticalSection,
        .ptNextTileIndex   = tTileCounter,
        .tRenderer         = tRenderer,
        .uFirstIndex       = uFirstIndex,
        .uIndexCount       = uIndexCount
    };

    // create worker threads
    ayThread* threads[THREAD_COUNT];
    for(int i = 0; i < THREAD_COUNT; i++)
    {
        ay_create_thread(tile_worker_thread, &tTileData, &threads[i]);
    }

    // wait for all threads to complete
    for(int i = 0; i < THREAD_COUNT; i++)
    {
        ay_join_thread(threads[i]);
        ay_destroy_thread(&threads[i]);
    }

    // cleanup
    ay_destroy_atomic_counter(&tTileCounter);
    ay_destroy_critical_section(&tCriticalSection);
    ay_free_tile_bins(&tTileBins);
}



void 
ay_test_draw_tile(ayGraphicsData* ptData, uint32_t uFirstIndex, uint32_t uIndexCount)
{
    ay_draw_indexed_tiled(ptData, uFirstIndex, uIndexCount);
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
        ptData->bDepthEnabled = bDepthEnabled;
        ptData->pfDepthBuffer = malloc(sizeof(float) * uHeight * uWidth);
        for(uint32_t i = 0; i < uWidth * uHeight; i++)
        {
            ptData->pfDepthBuffer[i] = 1.0f;
        }
    }

    return ptData;
};

void
ay_output_frame_buffer(ayFrameBufferData* ptData)
{
    stbi_write_png("output.png", ptData->uWidth, ptData->uHeight, 4, ptData->auData, sizeof(char) * 4 * ptData->uWidth);
};

// depth buffer clear value is 0
void
ay_clear_frame_buffer(ayFrameBufferData* ptData)
{
    memset(ptData->auData, 255, sizeof(char) * (ptData->uHeight * 4) * (ptData->uWidth));
        
    if(ptData->bDepthEnabled && ptData->pfDepthBuffer)
    {
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
    // convert UV to pixel coords
    int iPixelX = (int)((tUV.x) * (tTexture.iWidth - 1));
    int iPixelY = (int)((tUV.y) * (tTexture.iHeight - 1));
    // clamp to texture bounds 
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
ay_extract_sprite_texture(ayTexture tTexture, ayVec2 tUV, uint32_t uComponents, int iSpriteX, int iSpriteY, int iSpriteWidth, int iSpriteHeight)
{

    // TODO: needs work 
    // convert sprite bounds to normalized UV coordinates
    float startU = (float)iSpriteX / tTexture.iWidth;
    float startV = (float)iSpriteY / tTexture.iHeight;
    float endU = (float)(iSpriteX + iSpriteWidth) / tTexture.iWidth;
    float endV = (float)(iSpriteY + iSpriteHeight) / tTexture.iHeight;

    // map sprite-local UV (0-1) to atlas UV
    float atlasU = startU + (tUV.x) * (endU - startU);
    float atlasV = startV + (tUV.y) * (endV - startV);

    // convert to pixel coordinates
    int iPixelX = (int)(atlasU * (tTexture.iWidth - 1));
    int iPixelY = (int)(atlasV * (tTexture.iHeight - 1));

    // clamp to texture bounds
    iPixelX = iPixelX < 0 ? 0 : (iPixelX >= tTexture.iWidth ? tTexture.iWidth - 1 : iPixelX);
    iPixelY = iPixelY < 0 ? 0 : (iPixelY >= tTexture.iHeight ? tTexture.iHeight - 1 : iPixelY);

    // compute offset and sample
    int iPixelStart = (iPixelY * tTexture.iWidth + iPixelX) * uComponents;

    return (ayVec4){
    (float)tTexture.pucData[iPixelStart],
    (float)tTexture.pucData[iPixelStart + 1],
    (float)tTexture.pucData[iPixelStart + 2],
    (float)tTexture.pucData[iPixelStart + 3]
    };
}


ayVec4
ay_sample_texture_bilinear(ayTexture tTexture, ayVec2 tUV, uint32_t uComponents)
{
    // convert UV to exact pixel coordinates (floating point)
    float fPixelX = tUV.x * (tTexture.iWidth - 1);
    float fPixelY = tUV.y * (tTexture.iHeight - 1);
    
    // get integer coordinates of surrounding pixels
    int iX0 = (int)fPixelX;
    int iY0 = (int)fPixelY;
    int iX1 = iX0 + 1;
    int iY1 = iY0 + 1;

    // compute offset
    int iOffsetStartForPassthrough = (iY0 * tTexture.iWidth + iX0) * uComponents;
    float alphaPassthrough = (float)tTexture.pucData[iOffsetStartForPassthrough + 3];
    
    // clamp coordinates to texture bounds
    iX1 = iX1 >= tTexture.iWidth ? tTexture.iWidth - 1 : iX1;
    iY1 = iY1 >= tTexture.iHeight ? tTexture.iHeight - 1 : iY1;
    
    // calculate fractional parts for interpolation
    float fFracX = fPixelX - iX0;
    float fFracY = fPixelY - iY0;
    
    // sample all 4 surrounding pixels
    ayVec3 topLeft, bottomLeft, topRight, bottomRight; 
    
    // top left 
    int iOffset = (iY0 * tTexture.iWidth + iX0) * uComponents;
    topLeft = (ayVec3){
        (float)tTexture.pucData[iOffset],
        (float)tTexture.pucData[iOffset + 1],
        (float)tTexture.pucData[iOffset + 2]
    };
    
    // top right 
    iOffset = (iY0 * tTexture.iWidth + iX1) * uComponents;
    topRight = (ayVec3){
        (float)tTexture.pucData[iOffset],
        (float)tTexture.pucData[iOffset + 1],
        (float)tTexture.pucData[iOffset + 2]
    };
    
    // bottom left 
    iOffset = (iY1 * tTexture.iWidth + iX0) * uComponents;
    bottomLeft = (ayVec3){
        (float)tTexture.pucData[iOffset],
        (float)tTexture.pucData[iOffset + 1],
        (float)tTexture.pucData[iOffset + 2]
    };
    
    // bottom right 
    iOffset = (iY1 * tTexture.iWidth + iX1) * uComponents;
    bottomRight = (ayVec3){
        (float)tTexture.pucData[iOffset],
        (float)tTexture.pucData[iOffset + 1],
        (float)tTexture.pucData[iOffset + 2]
    };
    
    // linear interpolation horizontally
    // top
    ayVec3 tTop = {
        topLeft.r + fFracX * (bottomRight.r - topLeft.r),
        topLeft.g + fFracX * (bottomRight.g - topLeft.g),
        topLeft.b + fFracX * (bottomRight.b - topLeft.b)
    };
    
    // bottom
    ayVec3 tBottom = {
        bottomLeft.r + fFracX * (bottomRight.r - bottomLeft.r),
        bottomLeft.g + fFracX * (bottomRight.g - bottomLeft.g),
        bottomLeft.b + fFracX * (bottomRight.b - bottomLeft.b)
    };
    
    // linear interpolation vertically (final result)
    ayVec4 tResult = {
        tTop.r + fFracY * (tBottom.r - tTop.r),
        tTop.g + fFracY * (tBottom.g - tTop.g),
        tTop.b + fFracY * (tBottom.b - tTop.b),
        alphaPassthrough
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

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"