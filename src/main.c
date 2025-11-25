#include "ay_rasterize.h"
#include "ay_helpers.h"
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

#define screenWidth  500
#define screenHeight 500

ayVec4
ayPixelShader_Textures(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec2* ptUV = ay_get_varying(0, ptVaryingDataIn);
    ayTexture spriteTexture = *(ayTexture*)tInfo->atDescriptors[1].pData;

    ayVec4 tColor = ay_sample_texture(spriteTexture, *ptUV, 4);
    return tColor;
}

ayVec2
ayVertexShader_Textures(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptorInfo* tInfo, ayVaryingData* ptVaryingDataOut)
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;

    // get vertex attributes (inputs)
    ayVec2 tPos = *(ayVec2*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 0); 
    ayVec2 tUV = *(ayVec2*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 1);

    // set varyings (outputs)
    ayVec2* ptUV = ay_set_varying(AY_VARYING_TYPE_VEC2, ptVaryingDataOut);  
    *ptUV = tUV;

    return tPos;
}

ayVec4
ayPixelShader_Colors(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec4* ptColor = ay_get_varying(0, ptVaryingDataIn);
    const ayVec2* ptUV = ay_get_varying(1, ptVaryingDataIn);

    return (ayVec4){ptColor->r * 255, ptColor->g * 255, ptColor->b * 255, 1.f * 255};
}

ayVec2 ayVertexShader_Colors(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptorInfo* tInfo, ayVaryingData* ptVaryingDataOut) 
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;

    // get vertex attributes (inputs)
    ayVec2 tPos = *(ayVec2*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 0); 
    ayVec4 tColor = *(ayVec4*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 2);

    // set varyings (outputs)
    ayVec4* ptColor = ay_set_varying(AY_VARYING_TYPE_VEC2, ptVaryingDataOut);  
    *ptColor = tColor;

    return tPos;
}

int main()
{

    ayGraphicsData* ptData = initialize_graphics();
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight);
    ay_clear_frame_buffer(ptFrameBuffer);

    int iTextureWidth = 0;
    int iTextureHeight = 0;
    ayTexture testTexture = {
        .pucData = ay_load_png("../data/SpriteMapExample.png", &iTextureWidth, &iTextureHeight),
        .iWidth  = iTextureWidth,
        .iHeight = iTextureHeight
    };

    const int iCols = 5;
    const int iRows = 5;

    // Calculate required buffer sizes
    const int iVertexCount = iCols * iRows * 4; // 4 vertices per quad
    const int iIndexCount = iCols * iRows * 6;  // 6 indices per quad

    float* atVertexBuffer = malloc(iVertexCount * 8 * sizeof(float)); // x,y,u,v
    uint32_t* atIndexBuffer = malloc(iIndexCount * sizeof(uint32_t));

    ay_generate_quad_grid(iCols, iRows, atVertexBuffer, atIndexBuffer, true);

    ayPipeline tPipelineTextures = {
        .tPixelShader = ayPixelShader_Textures,
        .tVertexShader = ayVertexShader_Textures,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC4
            },
            .szAttribOffset = {
                0,
                sizeof(ayVec2),
                sizeof(ayVec2) + sizeof(ayVec2)
            },
            .szVertexStride = sizeof(float) * 8,
        }
    };

    ayPipeline tPipelineColors = {
        .tVertexWinding = AY_VERTEX_WINDING_CLOCKWISE,
        .tPixelShader = ayPixelShader_Colors,
        .tVertexShader = ayVertexShader_Colors,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC4
            },
            .szAttribOffset = {
                0,
                sizeof(ayVec2),
                sizeof(ayVec2) + sizeof(ayVec2)
            },
            .szVertexStride = sizeof(float) * 8,
        }
    };


    ay_bind_frame_buffer(ptData, ptFrameBuffer);
    ay_bind_vertex_buffer(ptData, atVertexBuffer);
    ay_bind_index_buffer(ptData, atIndexBuffer);
    ay_bind_texture(ptData, 1, &testTexture);

    // draw 
    ay_bind_pipeline(ptData, &tPipelineColors);
    ay_draw(ptData, 0, iVertexCount);

    // output frame
    ay_output_frame_buffer(ptFrameBuffer);

    free(atVertexBuffer);
    free(atIndexBuffer);
}
