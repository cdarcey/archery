#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


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

void ay_generate_quad_grid(int cols, int rows, float* vertices, uint32_t* indices, bool addRandomColor, bool DEBUG);

// for texture 
ayVec4
ayPixelShader_1(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{

    const ayVec4* ptColor = ay_get_varying(0, ptVaryingDataIn);
    const ayVec2* ptUV = ay_get_varying(1, ptVaryingDataIn);

    // ayTexture spriteTexture = *(ayTexture*)tInfo->atDescriptors[1].pData;
    // ayVec4 spriteColor = ay_sample_texture(spriteTexture, *ptUV, 4); 

    // vertex colors
    return (ayVec4){ptColor->r * 255, ptColor->g * 255, ptColor->b * 255, ptColor->a * 255};
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

    // Transform position
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
        .pucData = ay_load_png("../../assets/terrainSingle.png", &iTextureWidth, &iTextureHeight),
        .iWidth  = 256,
        .iHeight = 128
    };

    // code timing start 
    clock_t start, end;
    double cpu_time_used;
    start = clock();


    const int iCols = 30;
    const int iRows = 30;
    
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



void ay_generate_quad_grid(int iCols, int iRows, float* vertexBuffer, uint32_t* indexBuffer, bool bAddRandomColor, bool bDEBUG) 
{
    const float fQuadWidth = 2.0f / iCols;
    const float fQuadHeight = 2.0f / iRows;
    
    // Seed the random number generator if we're using random colors
    if (bAddRandomColor) {
        srand((unsigned int)time(NULL));
    }
    
    for (int y = 0; y < iRows; y++) {
        for (int x = 0; x < iCols; x++) {
            int iQuadIndex = y * iCols + x;
            int iStartingVertexindex = iQuadIndex * 32; // 4 vertices × 8 components
            
            float left = -1.0f + x * fQuadWidth;
            float right = left + fQuadWidth;
            float top = -1.0f + y * fQuadHeight;
            float bottom = top + fQuadHeight;
            
            // Generate random colors if enabled
            float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
            if (bAddRandomColor) {
                r = (float)rand() / (float)RAND_MAX;
                g = (float)rand() / (float)RAND_MAX;
                b = (float)rand() / (float)RAND_MAX;
                a = 1.0f; // alpha does not change
            }
            
            // Top left (x, y, u, v, r, g, b, a)
            vertexBuffer[iStartingVertexindex + 0] = left;
            vertexBuffer[iStartingVertexindex + 1] = top;
            vertexBuffer[iStartingVertexindex + 2] = 0.0f;
            vertexBuffer[iStartingVertexindex + 3] = 0.0f;
            vertexBuffer[iStartingVertexindex + 4] = r;
            vertexBuffer[iStartingVertexindex + 5] = g;
            vertexBuffer[iStartingVertexindex + 6] = b;
            vertexBuffer[iStartingVertexindex + 7] = a;
            
            // Top right
            vertexBuffer[iStartingVertexindex + 8] = right;
            vertexBuffer[iStartingVertexindex + 9] = top;
            vertexBuffer[iStartingVertexindex + 10] = 1.0f;
            vertexBuffer[iStartingVertexindex + 11] = 0.0f;
            vertexBuffer[iStartingVertexindex + 12] = r;
            vertexBuffer[iStartingVertexindex + 13] = g;
            vertexBuffer[iStartingVertexindex + 14] = b;
            vertexBuffer[iStartingVertexindex + 15] = a;
            
            // Bottom left
            vertexBuffer[iStartingVertexindex + 16] = left;
            vertexBuffer[iStartingVertexindex + 17] = bottom;
            vertexBuffer[iStartingVertexindex + 18] = 0.0f;
            vertexBuffer[iStartingVertexindex + 19] = 1.0f;
            vertexBuffer[iStartingVertexindex + 20] = r;
            vertexBuffer[iStartingVertexindex + 21] = g;
            vertexBuffer[iStartingVertexindex + 22] = b;
            vertexBuffer[iStartingVertexindex + 23] = a;
            
            // Bottom right
            vertexBuffer[iStartingVertexindex + 24] = right;
            vertexBuffer[iStartingVertexindex + 25] = bottom;
            vertexBuffer[iStartingVertexindex + 26] = 1.0f;
            vertexBuffer[iStartingVertexindex + 27] = 1.0f;
            vertexBuffer[iStartingVertexindex + 28] = r;
            vertexBuffer[iStartingVertexindex + 29] = g;
            vertexBuffer[iStartingVertexindex + 30] = b;
            vertexBuffer[iStartingVertexindex + 31] = a;
            
            if(bDEBUG)
            {
                // Debug print for each quad
                printf("Quad %d (Col %d, Row %d):\n", iQuadIndex, x, y);
                printf("  TL: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n", 
                       vertexBuffer[iStartingVertexindex],     vertexBuffer[iStartingVertexindex + 1],
                       vertexBuffer[iStartingVertexindex + 2], vertexBuffer[iStartingVertexindex + 3],
                       vertexBuffer[iStartingVertexindex + 4], vertexBuffer[iStartingVertexindex + 5],
                       vertexBuffer[iStartingVertexindex + 6], vertexBuffer[iStartingVertexindex + 7]);
                printf("  TR: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n", 
                       vertexBuffer[iStartingVertexindex + 8],  vertexBuffer[iStartingVertexindex + 9],
                       vertexBuffer[iStartingVertexindex + 10], vertexBuffer[iStartingVertexindex + 11],
                       vertexBuffer[iStartingVertexindex + 12], vertexBuffer[iStartingVertexindex + 13],
                       vertexBuffer[iStartingVertexindex + 14], vertexBuffer[iStartingVertexindex + 15]);
                printf("  BL: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n", 
                       vertexBuffer[iStartingVertexindex + 16], vertexBuffer[iStartingVertexindex + 17],
                       vertexBuffer[iStartingVertexindex + 18], vertexBuffer[iStartingVertexindex + 19],
                       vertexBuffer[iStartingVertexindex + 20], vertexBuffer[iStartingVertexindex + 21],
                       vertexBuffer[iStartingVertexindex + 22], vertexBuffer[iStartingVertexindex + 23]);
                printf("  BR: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n\n", 
                       vertexBuffer[iStartingVertexindex + 24], vertexBuffer[iStartingVertexindex + 25],
                       vertexBuffer[iStartingVertexindex + 26], vertexBuffer[iStartingVertexindex + 27],
                       vertexBuffer[iStartingVertexindex + 28], vertexBuffer[iStartingVertexindex + 29],
                       vertexBuffer[iStartingVertexindex + 30], vertexBuffer[iStartingVertexindex + 31]);
            }
        }
    }
    
    // Generate indices for index buffer
    for (int i = 0; i < iCols * iRows; i++) {
        int iCurrentQuad = i * 4;
        int iTriangleStartIndex = i * 6;
        
        // first traingle
        indexBuffer[iTriangleStartIndex + 0] = iCurrentQuad + 0;
        indexBuffer[iTriangleStartIndex + 1] = iCurrentQuad + 1;
        indexBuffer[iTriangleStartIndex + 2] = iCurrentQuad + 2;
        
        // second triangle
        indexBuffer[iTriangleStartIndex + 3] = iCurrentQuad + 1;
        indexBuffer[iTriangleStartIndex + 4] = iCurrentQuad + 3;
        indexBuffer[iTriangleStartIndex + 5] = iCurrentQuad + 2;
    }
}


ayVec2 transform_grid_to_isometric_view(float x, float y) 
{
    ayVec2 result;
    result.x = x - y;
    result.y = (x + y) / 2;
    return result;
}

