#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

// TODO:
//   * create a texture type/struct
//   * little quads
//   * add sample function to the actual ay library
//   * alpha blending settings

ayVec3 ay_sample_texture(unsigned char* pucTextureData, ayVec2 tUV)
{
    const int iTextureWidth = 416;
    const int iTextureHeight = 384;

    // tUV.x = 0.5f;
    // tUV.y = 0.5f;

    int iColOffset = tUV.x * 4 * iTextureWidth;
    int iRowOffset = iTextureWidth * 4 * tUV.y * iTextureHeight;
    int iPixelStart = iRowOffset + iColOffset;

    return (ayVec3){
        (float)pucTextureData[iPixelStart], 
        (float)pucTextureData[iPixelStart + 1], 
        (float)pucTextureData[iPixelStart + 2]};
}

ayVec3
ayPixelShader_0(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{

    const ayVec3* ptColor = ay_get_varying(0, ptVaryingDataIn);
    const float* pfDullFactor = ay_get_varying(1, ptVaryingDataIn);

    return (ayVec3){ *pfDullFactor * ptColor->x * 255, 
                     *pfDullFactor * ptColor->y * 255, 
                     *pfDullFactor * ptColor->z * 255};
}

ayVec3
ayPixelShader_1(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{

    const ayVec3* ptColor = ay_get_varying(0, ptVaryingDataIn);
    const float* pfDullFactor = ay_get_varying(1, ptVaryingDataIn);

    unsigned char* pucTextureData = (unsigned char*)tInfo->atDescriptors[1].pData;

    ayVec2 tUV = {tBuiltIns.tUV.x / 416, tBuiltIns.tUV.y / 384};
    ayVec3 tColor = ay_sample_texture(pucTextureData, tUV);

    return (ayVec3){ *pfDullFactor * tColor.x, 
                     *pfDullFactor * tColor.y, 
                     *pfDullFactor * tColor.z};
}

ayVec2
ayVertexShader_0(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptorInfo* tInfo, ayVaryingData* ptVaryingDataOut)
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;

    // get vertex attributes (inputs)
    ayVec2 tPos = *(ayVec2*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 0);
    ayVec3 tColor = *(ayVec3*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 1);\

    // set varyings (outputs)
    ayVec3* ptColor = ay_set_varying(AY_VARYING_TYPE_VEC3, ptVaryingDataOut);  // color
    *ptColor = tColor;

    float* pfDullFactor = ay_set_varying(AY_VARYING_TYPE_FLOAT, ptVaryingDataOut);

    float* pfStorageBuffer = (float*)tInfo->atDescriptors[0].pData;
    *pfDullFactor = pfStorageBuffer[tBuiltIns.uVertexID];

    return (ayVec2){tPos.x, tPos.y};
}

int main()
{

    ayGraphicsData* ptData = initialize_graphics();
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(416, 384);
    ay_clear_frame_buffer(ptFrameBuffer, (ayVec3){255, 255, 255});

    int iTextureWidth = 0;
    int iTextureHeight = 0;
    unsigned char* pucTextureData = ay_load_png("../data/SpriteMapExample.png", &iTextureWidth, &iTextureHeight);

    // code timing start 
    clock_t start, end;
    double cpu_time_used;
    start = clock();
    
    // vertex buffer
    float afVertexBuffer[] = { // ?, ?, x, y, ?, r, g, b
        0.5f, 0.0f, -1.0f, -1.0f, 0.5f, 1.0f, 0.0f, 0.0f, // top left
        0.5f, 0.0f,  1.0f, -1.0f, 0.5f, 0.0f, 1.0f, 0.0f, // top right
        0.5f, 0.0f, -1.0f,  1.0f, 0.5f, 0.0f, 0.0f, 1.0f, // bottom left
        0.5f, 0.0f,  1.0f,  1.0f, 0.5f, 1.0f, 0.0f, 0.0f, // bottom left
    };

    // index buffer
    uint32_t atIndexBuffer[6] = { 
        0, 1, 2, 2, 1, 3 // triangle 0
    };

    // storage buffers
    float atStorageBuffer[] = {
        0.5f, 0.5f, 1.0f, 0.5f
    };

    ayPipeline tPipeline0 = {
        .tPixelShader = ayPixelShader_0,
        .tVertexShader = ayVertexShader_0,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3
            },
            .szAttribOffset = {
                sizeof(ayVec2),
                sizeof(ayVec2) + sizeof(float) + sizeof(ayVec2),
            },
            .szVertexStride = sizeof(float) * 8,

        }
    };

    ayPipeline tPipeline1 = {
        .tPixelShader = ayPixelShader_1,
        .tVertexShader = ayVertexShader_0,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3
            },
            .szAttribOffset = {
                sizeof(ayVec2),
                sizeof(ayVec2) + sizeof(float) + sizeof(ayVec2),
            },
            .szVertexStride = sizeof(float) * 8,

        }
    };

    ay_bind_frame_buffer(ptData, ptFrameBuffer);
    ay_bind_vertex_buffer(ptData, afVertexBuffer);
    ay_bind_pipeline(ptData, &tPipeline0);
    ay_bind_index_buffer(ptData, atIndexBuffer);
    ay_bind_buffer(ptData, 0, (void*)atStorageBuffer);
    ay_bind_texture(ptData, 1, (void*)pucTextureData);
    ay_draw_indexed(ptData, 0, 6);

    ay_bind_pipeline(ptData, &tPipeline1);
    ay_draw_indexed(ptData, 0, 3);


    ay_output_frame_buffer(ptFrameBuffer);

    // code timing end 
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
}