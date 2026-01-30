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

#include "ay_rasterize.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "stb_image_write.h"
#include "stb_image.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _ayGraphicsData
{
    ayFrameBufferData* ptFrameBufferData;
    const void*        pVerticies;
    ayPipeline*        ptPipeline;
    uint32_t*          puIndexBufferData;
    ayDescriptor       tDescriptors[16]; 
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
ay_run_vertex_shader(ayGraphicsData* ptData, uint32_t idx, const char* pcVtxBuffer, ayVaryingData* pVarying) 
{
    ayVertexShaderBuiltIns builtIns = {.uVertexID = idx, .tLayout = ptData->ptPipeline->tLayout};
    return ptData->ptPipeline->tVertexShader(builtIns, &pcVtxBuffer[idx * ptData->ptPipeline->tLayout.szVertexStride], ptData->tDescriptors, pVarying);
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

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

ayGraphicsData*
initialize_graphics(void)
{
    ayGraphicsData* ptData = malloc(sizeof(ayGraphicsData));
    memset(ptData, 0, sizeof(ayGraphicsData));

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
                    GL_RGBA, GL_UNSIGNED_BYTE, ptFrameBuffer->pucData);
    
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
ay_draw(ayGraphicsData* ptData, uint32_t uFirstVertex, uint32_t uVertexCount)
{
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

        // Initialize at start of row
        float ABP = ABa * minX + ABb * minY + ABc;
        float BCP = BCa * minX + BCb * minY + BCc;
        float CAP = CAa * minX + CAb * minY + CAc;

        const float invABC = 1.0f / ABC;

        for(vertexP.y = (float)minY; vertexP.y <= (float)maxY; vertexP.y++)
        {
            float rowABP = ABP;
            float rowBCP = BCP;
            float rowCAP = CAP;

            for(vertexP.x = (float)minX; vertexP.x <= (float)maxX; vertexP.x++)
            {
                if((rowABP * fWindingSign) >= 0 && 
                   (rowBCP * fWindingSign) >= 0 && 
                   (rowCAP * fWindingSign) >= 0)
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
                        blendedVaryingData._auOffset[j] = tVaryingData0._auOffset[j];

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

                        if(fPixelDepth < ptData->ptFrameBufferData->pfDepthBuffer[iDepthIndex]) 
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
    // set winding sign
    float fWindingSign = (ptData->ptPipeline->tVertexWinding == AY_VERTEX_WINDING_CLOCKWISE) ? 1.0f : -1.0f;

    // calculate frame buffer size
    const uint32_t fbWidth = ptData->ptFrameBufferData->uWidth;
    const uint32_t fbHeight = ptData->ptFrameBufferData->uHeight;

    ayVec2 vertexP = {.x = 0, .y = 0};

    for(uint32_t i = 0; i < uIndexCount; i += 3)
    {
        const uint32_t uIndex0 = ptData->puIndexBufferData[uFirstIndex + i];
        const uint32_t uIndex1 = ptData->puIndexBufferData[uFirstIndex + i + 1];
        const uint32_t uIndex2 = ptData->puIndexBufferData[uFirstIndex + i + 2];

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

        // bounding box with clamping
        const uint32_t minX = ay_max(0, ay_min3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x) - 1);
        const uint32_t minY = ay_max(0, ay_min3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y) - 1);
        const uint32_t maxX = ay_min(fbWidth-1, ay_max3((uint32_t)tVertex0.x, (uint32_t)tVertex1.x, (uint32_t)tVertex2.x) + 1);
        const uint32_t maxY = ay_min(fbHeight-1, ay_max3((uint32_t)tVertex0.y, (uint32_t)tVertex1.y, (uint32_t)tVertex2.y) + 1);

        // precompute edge function coefficients
        float ABa, ABb, ABc, BCa, BCb, BCc, CAa, CAb, CAc;
        ay_compute_edge_coeffs(tVertex0, tVertex1, &ABa, &ABb, &ABc);
        ay_compute_edge_coeffs(tVertex1, tVertex2, &BCa, &BCb, &BCc);
        ay_compute_edge_coeffs(tVertex2, tVertex0, &CAa, &CAb, &CAc);

        // initialize at start of row
        float ABP = ABa * minX + ABb * minY + ABc;
        float BCP = BCa * minX + BCb * minY + BCc;
        float CAP = CAa * minX + CAb * minY + CAc;

        const float invABC = 1.0f / ABC;

        for(vertexP.y = (float)minY; vertexP.y <= (float)maxY; vertexP.y++)
        {
            float rowABP = ABP;
            float rowBCP = BCP;
            float rowCAP = CAP;

            for(vertexP.x = (float)minX; vertexP.x <= (float)maxX; vertexP.x++)
            {
                if((rowABP * fWindingSign) >= 0 && 
                   (rowBCP * fWindingSign) >= 0 && 
                   (rowCAP * fWindingSign) >= 0)
                {
                    const float weightA = rowBCP * invABC;
                    const float weightB = rowCAP * invABC;
                    const float weightC = rowABP * invABC;

                    ayPixelShaderBuiltIns tBuiltIns = {
                        .tUV = {vertexP.x, vertexP.y}
                    };

                    // varying system
                    int varyDataOffset = 0;
                    ayVaryingData blendedVaryingData = {0};

                    for(uint32_t j = 0; j < 16; j++)
                        blendedVaryingData._auOffset[j] = tVaryingData0._auOffset[j];

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

                        if(fPixelDepth < ptData->ptFrameBufferData->pfDepthBuffer[iDepthIndex]) 
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
    ptData->pucData = malloc(sizeof(char) * 4 * uWidth * uHeight);
    memset(ptData->pucData, 0, sizeof(char) * 4 * uWidth * uHeight);

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
    stbi_write_png("output.png", ptData->uWidth, ptData->uHeight, 4, ptData->pucData, sizeof(char) * 4 * ptData->uWidth);
};

void
ay_clear_frame_buffer(ayFrameBufferData* ptData)
{
    memset(ptData->pucData, 255, sizeof(char) * (ptData->uHeight * 4) * (ptData->uWidth));
    
    // clear depth buffer if enabled
    if(ptData->bDepthEnabled && ptData->pfDepthBuffer)
    {
        for(uint32_t i = 0; i < ptData->uWidth * ptData->uHeight; i++)
            ptData->pfDepthBuffer[i] = 1.0f;
    }
};

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
    int iPixelY = (int)((1.0f - tUV.y) * (tTexture.iHeight - 1));
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
    float fPixelY = (1.0f - tUV.y) * (tTexture.iHeight - 1);
    
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

    ptData->pucData[iPixelStart + 0] = (unsigned char)tColor.r;
    ptData->pucData[iPixelStart + 1] = (unsigned char)tColor.g;
    ptData->pucData[iPixelStart + 2] = (unsigned char)tColor.b;
    ptData->pucData[iPixelStart + 3] = (unsigned char)tColor.a;

};

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"