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



#define screenWidth  1080
#define screenHeight 1080


ayVec2 transform_grid_to_isometric_view(float x, float y);


// for texture 
ayVec4
ayPixelShader_1(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{

    const ayVec4* ptColor = ay_get_varying(0, ptVaryingDataIn);
    const ayVec2* ptUV = ay_get_varying(1, ptVaryingDataIn);

    ayTexture spriteTexture = *(ayTexture*)tInfo->atDescriptors[1].pData;
    ayVec4 spriteColor = ay_sample_texture(spriteTexture, *ptUV, 4); 

    // vertex colors
    return (ayVec4){spriteColor.r * 255, spriteColor.g * 255, spriteColor.b * 255, spriteColor.a * 255};
}

ayVec2 ayVertexShader_0(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptorInfo* tInfo, ayVaryingData* ptVaryingDataOut) 
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;

    // Get vertex attributes
    ayVec2 tPos = *(ayVec2*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 0); 
    ayVec2 tUV = *(ayVec2*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 1); 
    ayVec4 tColor = *(ayVec4*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 2);

    // Set varyings
    ayVec4* ptColor = ay_set_varying(AY_VARYING_TYPE_VEC4, ptVaryingDataOut);
    *ptColor = tColor;

    ayVec2* ptUV = ay_set_varying(AY_VARYING_TYPE_VEC2, ptVaryingDataOut);
    *ptUV = tUV;

    ayVec2 transCoords = transform_grid_to_isometric_view(tPos.x, tPos.y);

    // Transform position
    return transCoords;
}

int main()
{

    ayGraphicsData* ptData = initialize_graphics();
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight);
    ay_clear_frame_buffer(ptFrameBuffer);

    int iTextureWidth = 0;
    int iTextureHeight = 0;
    ayTexture testTexture = {
        .pucData = ay_load_png("../../assets/terrainSingle.png", &iTextureWidth, &iTextureHeight),
        .iWidth  = 256,
        .iHeight = 128
    };

    // code timing start 
    clock_t start, end;
    double cpu_time_used;
    start = clock();

    const int iCols = 4;
    const int iRows = 4;
    
    // Calculate required buffer sizes
    const int iVertexCount = iCols * iRows * 4; // 4 vertices per quad
    const int iIndexCount = iCols * iRows * 6;  // 6 indices per quad
    
    float* atVertexBuffer = malloc(iVertexCount * 8 * sizeof(float)); // x,y,u,v
    uint32_t* atIndexBuffer = malloc(iIndexCount * sizeof(uint32_t));
    
    ay_generate_quad_grid(iCols, iRows, atVertexBuffer, atIndexBuffer, true, false);

    ayPipeline tPipeline0 = {
        .tPixelShader = ayPixelShader_1,
        .tVertexShader = ayVertexShader_0,
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
    ay_bind_pipeline(ptData, &tPipeline0);
    ay_draw_indexed(ptData, 0, iIndexCount);  

    // output frame
    ay_output_frame_buffer(ptFrameBuffer);

    // code timing end 
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    

    free(atVertexBuffer);
    free(atIndexBuffer);
}






ayVec2 transform_grid_to_isometric_view(float x, float y) 
{
    ayVec2 result;
    result.x = x - y;
    result.y = (x + y) / 2;
    return result;
}

