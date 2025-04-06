#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

// TODO:
//   * create a texture type/struct                 | - Done
//   * little quads                                 |
//   * add sample function to the actual ay library | - Done
//   * alpha blending settings                      |
//   * bilinear sampling                            | - Done ???
//   * texture scaling and not clipping             | - Done

#define screenWidth 416
#define screenHeight 384
 

ayVec3
ayPixelShader_0(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec3* ptColor = ay_get_varying(0, ptVaryingDataIn);

    return (ayVec3){ptColor->x * 255, ptColor->y * 255, ptColor->z * 255};
}

ayVec3
ayPixelShader_1(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{

    const ayVec3* ptColor = ay_get_varying(0, ptVaryingDataIn);

    ayTexture spriteTexture = *(ayTexture*)tInfo->atDescriptors[1].pData;

    ayVec2 tUV = {tBuiltIns.tUV.x / screenWidth, tBuiltIns.tUV.y / screenHeight};

    // ayVec3 tColor = ay_sample_texture(spriteTexture, tUV, 4);
    ayVec3 tColor = ay_sample_texture_bilinear(spriteTexture, tUV, 4);

    return (ayVec3){tColor.x, tColor.y, tColor.z};
}

ayVec2
ayVertexShader_0(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptorInfo* tInfo, ayVaryingData* ptVaryingDataOut)
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;

    // get vertex attributes (inputs)
    ayVec2 tPos = *(ayVec2*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 0); 
    ayVec3 tColor = *(ayVec3*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 1);

    // set varyings (outputs)
    ayVec3* ptColor = ay_set_varying(AY_VARYING_TYPE_VEC3, ptVaryingDataOut);  // color
    *ptColor = tColor;

    return (ayVec2){tPos.x, tPos.y};
}

int main()
{

    ayGraphicsData* ptData = initialize_graphics();
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight);
    ay_clear_frame_buffer(ptFrameBuffer, (ayVec3){255, 255, 255});

    int iTextureWidth = 0;
    int iTextureHeight = 0;
    ayTexture testTexture = {
        .pucData = ay_load_png("../data/SpriteMapExample.png", &iTextureWidth, &iTextureHeight),
        .iWidth = 416,
        .iHeight = 384
    };

    // code timing start 
    clock_t start, end;
    double cpu_time_used;
    start = clock();
    
    // vertex buffer
    float afVertexBuffer[] = { // x, y, r, g, b
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, // top left
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f, // top right
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, // bottom left
         1.0f,  1.0f, 0.0f, 0.0f, 0.0f  // bottom right
    };

    // index buffer
    uint32_t atIndexBuffer[6] = { 
        0, 1, 2, 1, 3, 2
    };

    ayPipeline tPipeline0 = {
        .tPixelShader = ayPixelShader_1,
        .tVertexShader = ayVertexShader_0,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3
            },
            .szAttribOffset = {
                0,
                sizeof(ayVec2),
            },
            .szVertexStride = sizeof(float) * 5,

        }
    };

    ayPipeline tPipeline1 = {
        .tPixelShader = ayPixelShader_0,
        .tVertexShader = ayVertexShader_0,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3
            },
            .szAttribOffset = {
                0,
                sizeof(ayVec2),
            },
            .szVertexStride = sizeof(float) * 5,

        }
    };

    ay_bind_frame_buffer(ptData, ptFrameBuffer);
    ay_bind_vertex_buffer(ptData, afVertexBuffer);
    ay_bind_pipeline(ptData, &tPipeline0);
    ay_bind_texture(ptData, 1, &testTexture);
    ay_bind_index_buffer(ptData, atIndexBuffer);
    ay_draw_indexed(ptData, 0, 6);

    //ay_bind_pipeline(ptData, &tPipeline1);
    //ay_draw(ptData, 0, 3);


    ay_output_frame_buffer(ptFrameBuffer);

    // code timing end 
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
}