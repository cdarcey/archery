#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

// TODO:
//   * create a texture type/struct                 | - Done
//   * little quads                                 | - Done
//   * add sample function to the actual ay library | - Done
//   * alpha aware (pixel shader returns ayVec4)    | - Done 
//   * alpha blending settings                      |
//   * sampling wrap modes                          |
//   * bilinear sampling                            | - Done
//   * texture scaling and not clipping             | - Done
//   * depth buffering                              |
//   * compute shaders (threading)                  |


#define screenWidth  416
#define screenHeight 384
 
// for triangle 
ayVec4
ayPixelShader_0(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec4* ptColor = ay_get_varying(0, ptVaryingDataIn);

    return (ayVec4){ptColor->r * 255, 
                    ptColor->g * 255, 
                    ptColor->b * 255,
                    ptColor->a * 255};
}

// for texture 
ayVec4
ayPixelShader_1(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{

    const ayVec4* ptColor = ay_get_varying(0, ptVaryingDataIn);
    const ayVec2* ptUV = ay_get_varying(1, ptVaryingDataIn);

    ayTexture spriteTexture = *(ayTexture*)tInfo->atDescriptors[1].pData;

    ayVec4 tColor = ay_sample_texture(spriteTexture, *ptUV, 4);
    // ayVec4 tColor = ay_sample_texture_bilinear(spriteTexture, *ptUV, 4);

    return (ayVec4){tColor.r, tColor.g, tColor.b, tColor.a};
}

ayVec2
ayVertexShader_0(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptorInfo* tInfo, ayVaryingData* ptVaryingDataOut)
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;

    // get vertex attributes (inputs)
    ayVec2 tPos = *(ayVec2*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 0); 
    ayVec2 tUV = *(ayVec2*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 1); 
    ayVec4 tColor = *(ayVec4*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 2);

    // set varyings (outputs)
    ayVec4* ptColor = ay_set_varying(AY_VARYING_TYPE_VEC4, ptVaryingDataOut);  // color
    *ptColor = tColor;

    ayVec2* ptUV = ay_set_varying(AY_VARYING_TYPE_VEC2, ptVaryingDataOut);  // uv
    *ptUV = tUV;

    return (ayVec2){tPos.x, tPos.y};
}

int main()
{

    ayGraphicsData* ptData = initialize_graphics();
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight);
    ay_clear_frame_buffer(ptFrameBuffer, (ayVec4){255, 255, 255, 1.0f});

    int iTextureWidth = 0;
    int iTextureHeight = 0;
    ayTexture testTexture = {
        .pucData = ay_load_png("../data/SpriteMapExample.png", &iTextureWidth, &iTextureHeight),
        .iWidth  = 416,
        .iHeight = 384
    };


    // code timing start 
    clock_t start, end;
    double cpu_time_used;
    start = clock();
    
    // vertex buffer
    float afVertexBuffer[] = { // x, y, u, v, r, g, b, a
        -0.75f, -0.75f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.75f, // top left
         0.75f, -0.75f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.75f, // top right
        -0.75f,  0.75f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.75f, // bottom left
         0.75f,  0.75f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.75f, // bottom right
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
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC4,
            },
            .szAttribOffset = {
                0,
                sizeof(ayVec2),
                sizeof(ayVec2) + sizeof(ayVec2),
            },
            .szVertexStride = sizeof(float) * 8,

        }
    };

    ayPipeline tPipeline1 = {
        .tPixelShader = ayPixelShader_0,
        .tVertexShader = ayVertexShader_0,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC4,
            },
            .szAttribOffset = {
                0,
                sizeof(ayVec2),
                sizeof(ayVec2) + sizeof(ayVec2),
            },
            .szVertexStride = sizeof(float) * 8,

        }
    };

    ay_bind_frame_buffer(ptData, ptFrameBuffer);
    ay_bind_vertex_buffer(ptData, afVertexBuffer);
    ay_bind_index_buffer(ptData, atIndexBuffer);
    ay_bind_texture(ptData, 1, &testTexture);

    // draw texture
    ay_bind_pipeline(ptData, &tPipeline0);
    ay_draw(ptData, 0, 6);  

    // draw triangle
    ay_bind_pipeline(ptData, &tPipeline1);
    ay_draw(ptData, 0, 3);

    // output frame
    ay_output_frame_buffer(ptFrameBuffer);


    // code timing end 
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
}


