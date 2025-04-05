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

static inline int
ay_max_num(int a, int b, int c)
{
    if(a > b && a > c)
        return a;
    else if(b > a && b > c)
        return b;
    return c;
};

static inline int
ay_min_num(int a, int b, int c)
{
    if(a < b && a < c)
        return a;
    else if(b < a && b < c)
        return b;
    return c;
};

static void ay_set_pixel(ayFrameBufferData* ptData, ayVec2 input, ayVec3 tColor);

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
    ayVec2 vertexP = {
        .x = 0,
        .y = 0
    };  
    for(uint32_t i = 0; i < uIndexCount; i+= 3)
    {

        const uint32_t uIndex0 = uFirstVertex + i;
        const uint32_t uIndex1 = uFirstVertex + i + 1;
        const uint32_t uIndex2 = uFirstVertex + i + 2;

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
        /* edge function for entire triangle */
        float ABC = (float)ay_edge_function(tVertex0, tVertex1, tVertex2);
        /* min & max to only check pixels in a bounding box*/
        const int minX = ay_min_num(tVertex0.x, tVertex1.x, tVertex2.x);
        const int minY = ay_min_num(tVertex0.y, tVertex1.y, tVertex2.y);
        const int maxX = ay_max_num(tVertex0.x, tVertex1.x, tVertex2.x);
        const int maxY = ay_max_num(tVertex0.y, tVertex1.y, tVertex2.y); 
        for(vertexP.y = 0; vertexP.y < ptData->ptFrameBufferData->uHeight; vertexP.y++)
        {
            for(vertexP.x = 0; vertexP.x < ptData->ptFrameBufferData->uWidth; vertexP.x++)
            {
                const int ABP = ay_edge_function(tVertex0, tVertex1, vertexP);
                const int BCP = ay_edge_function(tVertex1, tVertex2, vertexP);
                const int CAP = ay_edge_function(tVertex2, tVertex0, vertexP);
                const float weightA = BCP / ABC;
                const float weightB = CAP / ABC;
                const float weightC = ABP / ABC;
                if(ABP >= 0 && BCP >= 0 && CAP >= 0)
                {
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
                            ayVec2 blendedVec2 = {
                                .x = ((float)(twoFloats0.x * weightA) + (float)(twoFloats0.x * weightB) + (float)(twoFloats0.x * weightC)),
                                .y = ((float)(twoFloats0.y * weightA) + (float)(twoFloats0.y * weightB) + (float)(twoFloats0.y * weightC)),
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
                    ayVec3 tFinalColor = ptData->ptPipeline->tPixelShader(tBuiltIns, &ptData->tDescriptor, &blendedVaryingData);
                    ay_set_pixel(ptData->ptFrameBufferData, vertexP, tFinalColor);
                }
            }
        }
    }
}   

void    
ay_draw_indexed(ayGraphicsData* ptData, uint32_t uFirstIndex, uint32_t uIndexCount)
{
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

        /* edge function for entire triangle */
        float ABC = (float)ay_edge_function(tVertex0, tVertex1, tVertex2);

        /* min & max to only check pixels in a bounding box*/
        const int minX = ay_min_num(tVertex0.x, tVertex1.x, tVertex2.x);
        const int minY = ay_min_num(tVertex0.y, tVertex1.y, tVertex2.y);
        const int maxX = ay_max_num(tVertex0.x, tVertex1.x, tVertex2.x);
        const int maxY = ay_max_num(tVertex0.y, tVertex1.y, tVertex2.y); 

        for(vertexP.y = 0; vertexP.y < ptData->ptFrameBufferData->uHeight; vertexP.y++)
        {
            for(vertexP.x = 0; vertexP.x < ptData->ptFrameBufferData->uWidth; vertexP.x++)
            {
                const int ABP = ay_edge_function(tVertex0, tVertex1, vertexP);
                const int BCP = ay_edge_function(tVertex1, tVertex2, vertexP);
                const int CAP = ay_edge_function(tVertex2, tVertex0, vertexP);

                const float weightA = BCP / ABC;
                const float weightB = CAP / ABC;
                const float weightC = ABP / ABC;

                if(ABP >= 0 && BCP >= 0 && CAP >= 0)
                {
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

                            ayVec2 blendedVec2 = {
                                .x = ((float)(twoFloats0.x * weightA) + (float)(twoFloats0.x * weightB) + (float)(twoFloats0.x * weightC)),
                                .y = ((float)(twoFloats0.y * weightA) + (float)(twoFloats0.y * weightB) + (float)(twoFloats0.y * weightC)),
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
                    ayVec3 tFinalColor = ptData->ptPipeline->tPixelShader(tBuiltIns, &ptData->tDescriptor, &blendedVaryingData);
                    ay_set_pixel(ptData->ptFrameBufferData, vertexP, tFinalColor);
                }
            }
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
ay_clear_frame_buffer(ayFrameBufferData* ptData, ayVec3 tColor)
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

ayVec3 
ay_sample_texture(ayTexture tTexture, ayVec2 tUV, uint32_t uComponents)
{

    int iPixelX = (int)(tUV.x * tTexture.iWidth);
    int iPixelY = (int)(tUV.y * tTexture.iHeight);

    iPixelX = iPixelX < 0 ? 0 : (iPixelX >= tTexture.iWidth ? tTexture.iWidth - 1 : iPixelX);
    iPixelY = iPixelY < 0 ? 0 : (iPixelY >= tTexture.iHeight ? tTexture.iHeight - 1 : iPixelY);

    int iPixelStart = (iPixelY * tTexture.iWidth + iPixelX) * uComponents;

    return (ayVec3){
        (float)tTexture.pucData[iPixelStart], 
        (float)tTexture.pucData[iPixelStart + 1], 
        (float)tTexture.pucData[iPixelStart + 2]};
}

ayVec3 
ay_bilinear_sample_texture(ayTexture tTexture, ayVec2 tUV, uint32_t uComponents)
{
    // need to weiht the pixel value based on pixel above, below, left, and right 
    // should look at varying for a possible solution to this 

    // need to calculate a weight per pixel and average those to give the current pixel a weight 
    // need the color for the pixels to be sampled, below is just getting coords for each pixel top sample 

    // this "solution" is dog shit, you gotta do better than that
    //----------------------------------------------------------//
    int iPixelX = (int)(tUV.x * tTexture.iWidth);
    int iPixelY = (int)(tUV.y * tTexture.iHeight);
    
    ayVec2 iPixelSample1 = { // pixel to the right 
        .x = (int)iPixelX + 1,
        .y = iPixelY
    };
    ayVec2 iPixelSample2 = { // pixel to the left
        .x = (int)iPixelX - 1,
        .y = iPixelY
    };
    ayVec2 iPixelSample3 = { // pixel below
        .x = (int)iPixelX + tTexture.iWidth,
        .y = iPixelY
    };
    ayVec2 iPixelSample4 = { // pixel above 
        .x = (int)iPixelX + tTexture.iWidth,
        .y = iPixelY
    };

    int interpolatedPixel = (iPixelSample1.x *
                             iPixelSample2.x * 
                             iPixelSample3.x * 
                             iPixelSample4.x) / 4; 

    //----------------------------------------------------------//

    iPixelX = iPixelX < 0 ? 0 : (iPixelX >= tTexture.iWidth ? tTexture.iWidth - 1 : iPixelX);
    iPixelY = iPixelY < 0 ? 0 : (iPixelY >= tTexture.iHeight ? tTexture.iHeight - 1 : iPixelY);

    int iPixelStart = (iPixelY * tTexture.iWidth + (iPixelX * interpolatedPixel)) * uComponents;

    return (ayVec3){
        (float)tTexture.pucData[iPixelStart], 
        (float)tTexture.pucData[iPixelStart + 1], 
        (float)tTexture.pucData[iPixelStart + 2]};
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
ay_set_pixel(ayFrameBufferData* ptData, ayVec2 input, ayVec3 tColor)
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

    ptData->pucData[iPixelStart + 0] = (unsigned char)tColor.x;
    ptData->pucData[iPixelStart + 1] = (unsigned char)tColor.y;
    ptData->pucData[iPixelStart + 2] = (unsigned char)tColor.z;
};

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"