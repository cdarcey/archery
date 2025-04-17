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
    ayDescriptorInfo   tDescriptor; 
} ayGraphicsData;

typedef struct _ayFrameBufferData
{
    uint32_t       uWidth;
    uint32_t       uHeight;
    unsigned char* pucData;
} ayFrameBufferData;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline int
ay_edge_function(ayVec2 one, ayVec2 two, ayVec2 three)
{
    return(two.x - one.x) * (three.y - one.y) - (two.y - one.y) * (three.x - one.x);
};

// 2-value minimum 
static inline int ay_min(int a, int b) {
    return (a < b) ? a : b;
}

// 3-value minimum 
static inline int ay_min3(int a, int b, int c) {
    int temp = (a < b) ? a : b;
    return (temp < c) ? temp : c; 
}

// 2-value maximum 
static inline int ay_max(int a, int b) {
    return (a > b) ? a : b;
}

// 3-value maximum 
static inline int ay_max3(int a, int b, int c) {
    int temp = (a > b) ? a : b;
    return (temp > c) ? temp : c;
}

static void ay_set_pixel(ayFrameBufferData* ptData, ayVec2 input, ayVec4 tColor);

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
ay_bind_buffer(ayGraphicsData* ptData, int bufferIndex, const void* ptDescBuffer)
{
    ptData->tDescriptor.atDescriptors[bufferIndex].pData = ptDescBuffer;
};

void
ay_bind_texture(ayGraphicsData* ptData, int bufferIndex, ayTexture* tTexture)
{
    ptData->tDescriptor.atDescriptors[bufferIndex].pData = tTexture;
};

void
ay_draw(ayGraphicsData* ptData, uint32_t uFirstVertex, uint32_t uIndexCount)
{
    // calculate frame buffer size
    const int fbWidth = ptData->ptFrameBufferData->uWidth;
    const int fbHeight = ptData->ptFrameBufferData->uHeight;

    ayVec2 vertexP = {
        .x = 0,
        .y = 0
    };  

    for(uint32_t i = 0; i < uIndexCount; i+= 3)
    {
        const uint32_t uIndex0 = ptData->puIndexBufferData[uFirstVertex + i];
        const uint32_t uIndex1 = ptData->puIndexBufferData[uFirstVertex + i + 1];
        const uint32_t uIndex2 = ptData->puIndexBufferData[uFirstVertex + i + 2];

        // type casting void buffer
        const char* pcVtxBuffer = (char*)ptData->pVerticies;

        // vertex shader stage
        // setting vertex ID
        ayVertexShaderBuiltIns tVSBuiltIns0 = {
            .uVertexID = uIndex0,
            .tLayout   = ptData->ptPipeline->tLayout
        };
        ayVertexShaderBuiltIns tVSBuiltIns1 = {
            .uVertexID = uIndex1,
            .tLayout   = ptData->ptPipeline->tLayout
        };
        ayVertexShaderBuiltIns tVSBuiltIns2 = {
            .uVertexID = uIndex2,
            .tLayout   = ptData->ptPipeline->tLayout
        };

        // defining varying data
        ayVaryingData tVaryingData0 = {0};
        ayVaryingData tVaryingData1 = {0};
        ayVaryingData tVaryingData2 = {0};

        // function pointer returning ayVec2 containing vertex data
        // Returns vec2        // function ptr                   // built-ins    //VertexDataIn                                                                           // VaryingDataOut
        ayVec2 tOriginalVertex0 = ptData->ptPipeline->tVertexShader(tVSBuiltIns0, &pcVtxBuffer[uIndex0 * ptData->ptPipeline->tLayout.szVertexStride], &ptData->tDescriptor, &tVaryingData0);
        ayVec2 tOriginalVertex1 = ptData->ptPipeline->tVertexShader(tVSBuiltIns1, &pcVtxBuffer[uIndex1 * ptData->ptPipeline->tLayout.szVertexStride], &ptData->tDescriptor, &tVaryingData1);
        ayVec2 tOriginalVertex2 = ptData->ptPipeline->tVertexShader(tVSBuiltIns2, &pcVtxBuffer[uIndex2 * ptData->ptPipeline->tLayout.szVertexStride], &ptData->tDescriptor, &tVaryingData2);

        // frame buffer space
        ayVec2 tVertex0 = tOriginalVertex0;
        ayVec2 tVertex1 = tOriginalVertex1;
        ayVec2 tVertex2 = tOriginalVertex2;

        tVertex0.x = ptData->ptFrameBufferData->uWidth * (0.5f + 0.5f * tVertex0.x);
        tVertex1.x = ptData->ptFrameBufferData->uWidth * (0.5f + 0.5f * tVertex1.x);
        tVertex2.x = ptData->ptFrameBufferData->uWidth * (0.5f + 0.5f * tVertex2.x);

        tVertex0.y = ptData->ptFrameBufferData->uHeight * (0.5f + 0.5f * tVertex0.y);
        tVertex1.y = ptData->ptFrameBufferData->uHeight * (0.5f + 0.5f * tVertex1.y);
        tVertex2.y = ptData->ptFrameBufferData->uHeight * (0.5f + 0.5f * tVertex2.y);

        // edge function for entire triangle 
        float ABC = (float)ay_edge_function(tVertex0, tVertex1, tVertex2);

        // Bounding box with clamping
        const int minX = ay_max(0, ay_min3(tVertex0.x, tVertex1.x, tVertex2.x) - 1);
        const int minY = ay_max(0, ay_min3(tVertex0.y, tVertex1.y, tVertex2.y) - 1);
        const int maxX = ay_min(fbWidth-1, ay_max3(tVertex0.x, tVertex1.x, tVertex2.x) + 1);
        const int maxY = ay_min(fbHeight-1, ay_max3(tVertex0.y, tVertex1.y, tVertex2.y) + 1);

        // Precompute edge function deltas
        const float ABa = tVertex0.y - tVertex1.y;
        const float ABb = tVertex1.x - tVertex0.x;
        const float ABc = tVertex0.x * tVertex1.y - tVertex1.x * tVertex0.y;

        const float BCa = tVertex1.y - tVertex2.y;
        const float BCb = tVertex2.x - tVertex1.x;
        const float BCc = tVertex1.x * tVertex2.y - tVertex2.x * tVertex1.y;

        const float CAa = tVertex2.y - tVertex0.y;
        const float CAb = tVertex0.x - tVertex2.x;
        const float CAc = tVertex2.x * tVertex0.y - tVertex0.x * tVertex2.y;

        // Initialize at start of row
        float ABP = ABa * minX + ABb * minY + ABc;
        float BCP = BCa * minX + BCb * minY + BCc;
        float CAP = CAa * minX + CAb * minY + CAc;

        const float invABC = 1.0f / ABC;

        for(vertexP.y = minY; vertexP.y <= maxY; vertexP.y++)
        {
            float rowABP = ABP;
            float rowBCP = BCP;
            float rowCAP = CAP;
        
            for(vertexP.x = minX; vertexP.x <= maxX; vertexP.x++)
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
                        if(tVaryingData0.atTypes[varyIndex] == AY_VARYING_TYPE_VEC2)
                        {
                            // 1st input 
                            // Vec2 blending
                            const ayVec2 twoFloats0 = *(ayVec2*)&tVaryingData0.acVaryingData[varyDataOffset];
                            const ayVec2 twoFloats1 = *(ayVec2*)&tVaryingData1.acVaryingData[varyDataOffset];
                            const ayVec2 twoFloats2 = *(ayVec2*)&tVaryingData2.acVaryingData[varyDataOffset];

                            ayVec2 blendedVec2 = {
                                .x = ((float)(twoFloats0.x * weightA) + (float)(twoFloats1.x * weightB) + (float)(twoFloats2.x * weightC)),
                                .y = ((float)(twoFloats0.y * weightA) + (float)(twoFloats1.y * weightB) + (float)(twoFloats2.y * weightC)),
                            };
                            memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &blendedVec2, sizeof(ayVec2));
                            varyDataOffset += sizeof(ayVec2);
                        }
                        if(tVaryingData0.atTypes[varyIndex] == AY_VARYING_TYPE_VEC3)
                        {
                            // 2nd input 
                            // Vec3 blending 
                            const ayVec3* ptVecThree0 = (ayVec3*)&tVaryingData0.acVaryingData[varyDataOffset];
                            const ayVec3* ptVecThree1 = (ayVec3*)&tVaryingData1.acVaryingData[varyDataOffset];
                            const ayVec3* ptVecThree2 = (ayVec3*)&tVaryingData2.acVaryingData[varyDataOffset];

                            ayVec4 tBlendedVecThree = {
                                .x = ((float)(ptVecThree0->x * weightA) + (float)(ptVecThree1->x * weightB) + (float)(ptVecThree2->x * weightC)),
                                .y = ((float)(ptVecThree0->y * weightA) + (float)(ptVecThree1->y * weightB) + (float)(ptVecThree2->y * weightC)),
                                .z = ((float)(ptVecThree0->z * weightA) + (float)(ptVecThree1->z * weightB) + (float)(ptVecThree2->z * weightC))
                            };
                            memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &tBlendedVecThree, sizeof(ayVec3));
                            varyDataOffset += sizeof(ayVec3);
                        }
                        if(tVaryingData0.atTypes[varyIndex] == AY_VARYING_TYPE_VEC4)
                        {
                            // 3rd input
                            // Vec4 blending 
                            const ayVec4* ptColor0 = (ayVec4*)&tVaryingData0.acVaryingData[varyDataOffset];
                            const ayVec4* ptColor1 = (ayVec4*)&tVaryingData1.acVaryingData[varyDataOffset];
                            const ayVec4* ptColor2 = (ayVec4*)&tVaryingData2.acVaryingData[varyDataOffset];

                            ayVec4 tBlendedColor = {
                                .x = ((float)(ptColor0->x * weightA) + (float)(ptColor1->x * weightB) + (float)(ptColor2->x * weightC)),
                                .y = ((float)(ptColor0->y * weightA) + (float)(ptColor1->y * weightB) + (float)(ptColor2->y * weightC)),
                                .z = ((float)(ptColor0->z * weightA) + (float)(ptColor1->z * weightB) + (float)(ptColor2->z * weightC)),
                                .w = ((float)(ptColor0->w * weightA) + (float)(ptColor1->w * weightB) + (float)(ptColor2->w * weightC))
                            };
                            memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &tBlendedColor, sizeof(ayVec4));
                            varyDataOffset += sizeof(ayVec4);
                        }
                        if(tVaryingData0.atTypes[varyIndex] == AY_VARYING_TYPE_FLOAT)
                        {
                            // 4th input 
                            // float blending 
                            const float tData0 = *(float*)&tVaryingData0.acVaryingData[varyDataOffset];
                            const float tData1 = *(float*)&tVaryingData1.acVaryingData[varyDataOffset];
                            const float tData2 = *(float*)&tVaryingData2.acVaryingData[varyDataOffset];

                            float fBlendedData2 = ((float)(tData0 * weightA) + (float)(tData1 * weightB) + (float)(tData2 * weightC));

                            memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &fBlendedData2, sizeof(float));
                            varyDataOffset += sizeof(float);
                        }
                    }

                    // run pixel shader
                    ayVec4 tFinalColor = ptData->ptPipeline->tPixelShader(tBuiltIns, &ptData->tDescriptor, &blendedVaryingData);
                    tFinalColor.r *= tFinalColor.a / 255;
                    tFinalColor.g *= tFinalColor.a / 255;
                    tFinalColor.b *= tFinalColor.a / 255;
                    ay_set_pixel(ptData->ptFrameBufferData, vertexP, tFinalColor);
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
    // calculate frame buffer size
    const int fbWidth = ptData->ptFrameBufferData->uWidth;
    const int fbHeight = ptData->ptFrameBufferData->uHeight;
    
    ayVec2 vertexP = {
        .x = 0,
        .y = 0
    };  

    for(uint32_t i = 0; i < uIndexCount; i += 3)
    {
        const uint32_t uIndex0 = ptData->puIndexBufferData[uFirstIndex + i];
        const uint32_t uIndex1 = ptData->puIndexBufferData[uFirstIndex + i + 1];
        const uint32_t uIndex2 = ptData->puIndexBufferData[uFirstIndex + i + 2];

        // type casting void buffer
        const char* pcVtxBuffer = (char*)ptData->pVerticies;

        // vertex shader stage
        // setting vertex ID
        ayVertexShaderBuiltIns tVSBuiltIns0 = {
            .uVertexID = uIndex0,
            .tLayout   = ptData->ptPipeline->tLayout
        };
        ayVertexShaderBuiltIns tVSBuiltIns1 = {
            .uVertexID = uIndex1,
            .tLayout   = ptData->ptPipeline->tLayout
        };
        ayVertexShaderBuiltIns tVSBuiltIns2 = {
            .uVertexID = uIndex2,
            .tLayout   = ptData->ptPipeline->tLayout
        };

        // defining varying data
        ayVaryingData tVaryingData0 = {0};
        ayVaryingData tVaryingData1 = {0};
        ayVaryingData tVaryingData2 = {0};

        // function pointer returning ayVec2 containing vertex data
        // Returns vec2        // function ptr                   // built-ins    //VertexDataIn                                                                           // VaryingDataOut
        ayVec2 tOriginalVertex0 = ptData->ptPipeline->tVertexShader(tVSBuiltIns0, &pcVtxBuffer[uIndex0 * ptData->ptPipeline->tLayout.szVertexStride], &ptData->tDescriptor, &tVaryingData0);
        ayVec2 tOriginalVertex1 = ptData->ptPipeline->tVertexShader(tVSBuiltIns1, &pcVtxBuffer[uIndex1 * ptData->ptPipeline->tLayout.szVertexStride], &ptData->tDescriptor, &tVaryingData1);
        ayVec2 tOriginalVertex2 = ptData->ptPipeline->tVertexShader(tVSBuiltIns2, &pcVtxBuffer[uIndex2 * ptData->ptPipeline->tLayout.szVertexStride], &ptData->tDescriptor, &tVaryingData2);

        // frame buffer space
        ayVec2 tVertex0 = tOriginalVertex0;
        ayVec2 tVertex1 = tOriginalVertex1;
        ayVec2 tVertex2 = tOriginalVertex2;

        tVertex0.x = ptData->ptFrameBufferData->uWidth * (0.5f + 0.5f * tVertex0.x);
        tVertex1.x = ptData->ptFrameBufferData->uWidth * (0.5f + 0.5f * tVertex1.x);
        tVertex2.x = ptData->ptFrameBufferData->uWidth * (0.5f + 0.5f * tVertex2.x);

        tVertex0.y = ptData->ptFrameBufferData->uHeight * (0.5f + 0.5f * tVertex0.y);
        tVertex1.y = ptData->ptFrameBufferData->uHeight * (0.5f + 0.5f * tVertex1.y);
        tVertex2.y = ptData->ptFrameBufferData->uHeight * (0.5f + 0.5f * tVertex2.y);

        // edge function for entire triangle 
        float ABC = (float)ay_edge_function(tVertex0, tVertex1, tVertex2);

        // Bounding box with clamping
        const int minX = ay_max(0, ay_min3(tVertex0.x, tVertex1.x, tVertex2.x) - 1);
        const int minY = ay_max(0, ay_min3(tVertex0.y, tVertex1.y, tVertex2.y) - 1);
        const int maxX = ay_min(fbWidth-1, ay_max3(tVertex0.x, tVertex1.x, tVertex2.x) + 1);
        const int maxY = ay_min(fbHeight-1, ay_max3(tVertex0.y, tVertex1.y, tVertex2.y) + 1);

        // Precompute edge function deltas
        const float ABa = tVertex0.y - tVertex1.y;
        const float ABb = tVertex1.x - tVertex0.x;
        const float ABc = tVertex0.x * tVertex1.y - tVertex1.x * tVertex0.y;

        const float BCa = tVertex1.y - tVertex2.y;
        const float BCb = tVertex2.x - tVertex1.x;
        const float BCc = tVertex1.x * tVertex2.y - tVertex2.x * tVertex1.y;

        const float CAa = tVertex2.y - tVertex0.y;
        const float CAb = tVertex0.x - tVertex2.x;
        const float CAc = tVertex2.x * tVertex0.y - tVertex0.x * tVertex2.y;

        // Initialize at start of row
        float ABP = ABa * minX + ABb * minY + ABc;
        float BCP = BCa * minX + BCb * minY + BCc;
        float CAP = CAa * minX + CAb * minY + CAc;

        const float invABC = 1.0f / ABC;

        for(vertexP.y = minY; vertexP.y <= maxY; vertexP.y++)
        {
            float rowABP = ABP;
            float rowBCP = BCP;
            float rowCAP = CAP;
        
            for(vertexP.x = minX; vertexP.x <= maxX; vertexP.x++)
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
                        if(tVaryingData0.atTypes[varyIndex] == AY_VARYING_TYPE_VEC2)
                        {
                            // 1st input 
                            // Vec2 blending
                            const ayVec2 twoFloats0 = *(ayVec2*)&tVaryingData0.acVaryingData[varyDataOffset];
                            const ayVec2 twoFloats1 = *(ayVec2*)&tVaryingData1.acVaryingData[varyDataOffset];
                            const ayVec2 twoFloats2 = *(ayVec2*)&tVaryingData2.acVaryingData[varyDataOffset];

                            ayVec2 blendedVec2 = {
                                .x = ((float)(twoFloats0.x * weightA) + (float)(twoFloats1.x * weightB) + (float)(twoFloats2.x * weightC)),
                                .y = ((float)(twoFloats0.y * weightA) + (float)(twoFloats1.y * weightB) + (float)(twoFloats2.y * weightC)),
                            };
                            memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &blendedVec2, sizeof(ayVec2));
                            varyDataOffset += sizeof(ayVec2);
                        }
                        if(tVaryingData0.atTypes[varyIndex] == AY_VARYING_TYPE_VEC3)
                        {
                            // 2nd input 
                            // Vec3 blending 
                            const ayVec3* ptVecThree0 = (ayVec3*)&tVaryingData0.acVaryingData[varyDataOffset];
                            const ayVec3* ptVecThree1 = (ayVec3*)&tVaryingData1.acVaryingData[varyDataOffset];
                            const ayVec3* ptVecThree2 = (ayVec3*)&tVaryingData2.acVaryingData[varyDataOffset];

                            ayVec4 tBlendedVecThree = {
                                .x = ((float)(ptVecThree0->x * weightA) + (float)(ptVecThree1->x * weightB) + (float)(ptVecThree2->x * weightC)),
                                .y = ((float)(ptVecThree0->y * weightA) + (float)(ptVecThree1->y * weightB) + (float)(ptVecThree2->y * weightC)),
                                .z = ((float)(ptVecThree0->z * weightA) + (float)(ptVecThree1->z * weightB) + (float)(ptVecThree2->z * weightC))
                            };
                            memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &tBlendedVecThree, sizeof(ayVec3));
                            varyDataOffset += sizeof(ayVec3);
                        }
                        if(tVaryingData0.atTypes[varyIndex] == AY_VARYING_TYPE_VEC4)
                        {
                            // 3rd input
                            // Vec4 blending 
                            const ayVec4* ptColor0 = (ayVec4*)&tVaryingData0.acVaryingData[varyDataOffset];
                            const ayVec4* ptColor1 = (ayVec4*)&tVaryingData1.acVaryingData[varyDataOffset];
                            const ayVec4* ptColor2 = (ayVec4*)&tVaryingData2.acVaryingData[varyDataOffset];

                            ayVec4 tBlendedColor = {
                                .x = ((float)(ptColor0->x * weightA) + (float)(ptColor1->x * weightB) + (float)(ptColor2->x * weightC)),
                                .y = ((float)(ptColor0->y * weightA) + (float)(ptColor1->y * weightB) + (float)(ptColor2->y * weightC)),
                                .z = ((float)(ptColor0->z * weightA) + (float)(ptColor1->z * weightB) + (float)(ptColor2->z * weightC)),
                                .w = ((float)(ptColor0->w * weightA) + (float)(ptColor1->w * weightB) + (float)(ptColor2->w * weightC))
                            };
                            memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &tBlendedColor, sizeof(ayVec4));
                            varyDataOffset += sizeof(ayVec4);
                        }
                        if(tVaryingData0.atTypes[varyIndex] == AY_VARYING_TYPE_FLOAT)
                        {
                            // 4th input 
                            // float blending 
                            const float tData0 = *(float*)&tVaryingData0.acVaryingData[varyDataOffset];
                            const float tData1 = *(float*)&tVaryingData1.acVaryingData[varyDataOffset];
                            const float tData2 = *(float*)&tVaryingData2.acVaryingData[varyDataOffset];

                            float fBlendedData2 = ((float)(tData0 * weightA) + (float)(tData1 * weightB) + (float)(tData2 * weightC));

                            memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &fBlendedData2, sizeof(float));
                            varyDataOffset += sizeof(float);
                        }
                    }

                    // run pixel shader
                    ayVec4 tFinalColor = ptData->ptPipeline->tPixelShader(tBuiltIns, &ptData->tDescriptor, &blendedVaryingData);
                    tFinalColor.r *= tFinalColor.a / 255;
                    tFinalColor.g *= tFinalColor.a / 255;
                    tFinalColor.b *= tFinalColor.a / 255;
                    ay_set_pixel(ptData->ptFrameBufferData, vertexP, tFinalColor);
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
        uint32_t uOffset = tLayout.szAttribOffset[tAttribLocation];
        ptResult = (char*)ptResult + uOffset;
    }
    else if(tLayout.tAttribType[tAttribLocation] == AY_VERTEX_ATTRIBUTE_TYPE_VEC3)
    {
        uint32_t uOffset = tLayout.szAttribOffset[tAttribLocation];
        ptResult = (char*)ptResult + uOffset;
    }
    else if(tLayout.tAttribType[tAttribLocation] == AY_VERTEX_ATTRIBUTE_TYPE_VEC4)
    {
        uint32_t uOffset = tLayout.szAttribOffset[tAttribLocation];
        ptResult = (char*)ptResult + uOffset;
    }
    else if(tLayout.tAttribType[tAttribLocation] == AY_VERTEX_ATTRIBUTE_TYPE_FLOAT)
    {
        uint32_t uOffset = tLayout.szAttribOffset[tAttribLocation];
        ptResult = (char*)ptResult + uOffset;
    }
    return (void*)ptResult;
};

ayFrameBufferData*
ay_initialize_frame_buffer(uint32_t uWidth, uint32_t uHeight)
{
    ayFrameBufferData* ptData = malloc(sizeof(ayFrameBufferData));
    memset(ptData, 0, sizeof(ayFrameBufferData));

    ptData->uWidth = uWidth;
    ptData->uHeight = uHeight;
    ptData->pucData = malloc(sizeof(char) * 3 * uWidth * uHeight);
    memset(ptData->pucData, 0, sizeof(char) * 3 * uWidth * uHeight);

    return ptData;
};

void
ay_output_frame_buffer(ayFrameBufferData* ptData)
{
    stbi_write_png("output.png", ptData->uWidth, ptData->uHeight, 3, ptData->pucData, sizeof(char) * 3 * ptData->uWidth);
};

void
ay_clear_frame_buffer(ayFrameBufferData* ptData, ayVec4 tColor)
{
    for(uint32_t iRow = 0; iRow < ptData->uHeight; iRow++)
    {
        for(uint32_t iColumn = 0; iColumn < ptData->uWidth; iColumn++)
        {
            ay_set_pixel(ptData, (ayVec2){iColumn, iRow}, tColor);
        }
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
    int iPixelX = tUV.x * (tTexture.iWidth - 1);
    int iPixelY = tUV.y * (tTexture.iHeight - 1);
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
ay_sample_texture_bilinear(ayTexture tTexture, ayVec2 tUV, uint32_t uComponents)
{
    // Convert UV to exact pixel coordinates (floating point)
    float fPixelX = tUV.x * (tTexture.iWidth - 1);
    float fPixelY = tUV.y * (tTexture.iHeight - 1);
    
    // Get integer coordinates of surrounding pixels
    int iX0 = (int)fPixelX;
    int iY0 = (int)fPixelY;
    int iX1 = iX0 + 1;
    int iY1 = iY0 + 1;

    // compute offset
    int iOffsetStartForPassthrough = (iY0 * tTexture.iWidth + iX0) * uComponents;
    float alphaPassthrough = (float)tTexture.pucData[iOffsetStartForPassthrough + 3];
    
    // Clamp coordinates to texture bounds
    iX1 = iX1 >= tTexture.iWidth ? tTexture.iWidth - 1 : iX1;
    iY1 = iY1 >= tTexture.iHeight ? tTexture.iHeight - 1 : iY1;
    
    // Calculate fractional parts for interpolation
    float fFracX = fPixelX - iX0;
    float fFracY = fPixelY - iY0;
    
    // Sample all 4 surrounding pixels
    ayVec3 topLeft, bottomLeft, topRight, bottomRight; 
    
    // Top left 
    int iOffset = (iY0 * tTexture.iWidth + iX0) * uComponents;
    topLeft = (ayVec3){
        (float)tTexture.pucData[iOffset],
        (float)tTexture.pucData[iOffset + 1],
        (float)tTexture.pucData[iOffset + 2]
    };
    
    // Top right 
    iOffset = (iY0 * tTexture.iWidth + iX1) * uComponents;
    topRight = (ayVec3){
        (float)tTexture.pucData[iOffset],
        (float)tTexture.pucData[iOffset + 1],
        (float)tTexture.pucData[iOffset + 2]
    };
    
    // Bottom left 
    iOffset = (iY1 * tTexture.iWidth + iX0) * uComponents;
    bottomLeft = (ayVec3){
        (float)tTexture.pucData[iOffset],
        (float)tTexture.pucData[iOffset + 1],
        (float)tTexture.pucData[iOffset + 2]
    };
    
    // Bottom right 
    iOffset = (iY1 * tTexture.iWidth + iX1) * uComponents;
    bottomRight = (ayVec3){
        (float)tTexture.pucData[iOffset],
        (float)tTexture.pucData[iOffset + 1],
        (float)tTexture.pucData[iOffset + 2]
    };
    
    // Linear interpolation horizontally
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
    
    // Linear interpolation vertically (final result)
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

    int iRowOffset = ptData->uWidth * 3 * input.y;
    int iPixelStart = iRowOffset + input.x * 3;

    ptData->pucData[iPixelStart + 0] = (unsigned char)tColor.r;
    ptData->pucData[iPixelStart + 1] = (unsigned char)tColor.g;
    ptData->pucData[iPixelStart + 2] = (unsigned char)tColor.b;

};

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"