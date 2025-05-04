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
//   * fix applyIsometricToUV function              |


#define screenWidth  256
#define screenHeight 256

ayVec2 applyIsometricToUV(float u, float v);
ayVec2 transdormGridtoIsometric(float x, float y);
void ay_generate_quad_grid(int cols, int rows, float* vertices, uint32_t* indices, bool addRandomColor, bool DEBUG);

// for texture 
ayVec4
ayPixelShader_1(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{

    const ayVec4* ptColor = ay_get_varying(0, ptVaryingDataIn);
    const ayVec2* ptUV = ay_get_varying(1, ptVaryingDataIn);

    ayTexture spriteTexture = *(ayTexture*)tInfo->atDescriptors[1].pData;

    ayVec4 spriteColor = ay_sample_texture(spriteTexture, *ptUV, 4); 
    // ayVec4 spriteColor = ay_sample_texture_bilinear(spriteTexture, *ptUV, 4);
    // ayVec4 spriteColor = ay_extract_sprite_texture(spriteTexture, *ptUV, 4, 0, 32, 32, 32);

    // textures
    // return (ayVec4){spriteColor.r * 255, spriteColor.g * 255, spriteColor.b * 255, spriteColor.a * 255};

    // vertex colors
    return (ayVec4){ptColor->r * 255, ptColor->g * 255, ptColor->b * 255, ptColor->a * 255};
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

    // TODO: fix this
    // float transU = ptUV->u;
    // float transV = ptUV->v;
    // *ptUV = applyIsometricToUV(transU, transV);

    // ayVec2 transCoords = transdormGridtoIsometric(tPos.x, tPos.y);

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
        .pucData = ay_load_png("../../assets/separate_images/tile_025.png", &iTextureWidth, &iTextureHeight),
        .iWidth  = 32,
        .iHeight = 32
    };

    // code timing start 
    clock_t start, end;
    double cpu_time_used;
    start = clock();

    const int iCols = 5;
    const int iRows = 5;
    
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

    // draw texture
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



void ay_generate_quad_grid(int cols, int rows, float* vertices, uint32_t* indices, bool addRandomColor, bool DEBUG) 
{
    const float fQuadWidth = 2.0f / cols;
    const float fQuadHeight = 2.0f / rows;
    
    // Seed the random number generator if we're using random colors
    if (addRandomColor) {
        srand((unsigned int)time(NULL));
    }
    
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            int QuadIdx = y * cols + x;
            int BaseVtx = QuadIdx * 32; // 4 vertices × 8 components
            
            float left = -1.0f + x * fQuadWidth;
            float right = left + fQuadWidth;
            float top = -1.0f + y * fQuadHeight;
            float bottom = top + fQuadHeight;
            
            // Generate random colors if enabled
            float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
            if (addRandomColor) {
                r = (float)rand() / (float)RAND_MAX;
                g = (float)rand() / (float)RAND_MAX;
                b = (float)rand() / (float)RAND_MAX;
                a = 1.0f; // alpha does not change
            }
            
            // Top-left (x, y, u, v, r, g, b, a)
            vertices[BaseVtx + 0] = left;
            vertices[BaseVtx + 1] = top;
            vertices[BaseVtx + 2] = 0.0f;
            vertices[BaseVtx + 3] = 0.0f;
            vertices[BaseVtx + 4] = r;
            vertices[BaseVtx + 5] = g;
            vertices[BaseVtx + 6] = b;
            vertices[BaseVtx + 7] = a;
            
            // Top-right
            vertices[BaseVtx + 8] = right;
            vertices[BaseVtx + 9] = top;
            vertices[BaseVtx + 10] = 1.0f;
            vertices[BaseVtx + 11] = 0.0f;
            vertices[BaseVtx + 12] = r;
            vertices[BaseVtx + 13] = g;
            vertices[BaseVtx + 14] = b;
            vertices[BaseVtx + 15] = a;
            
            // Bottom-left
            vertices[BaseVtx + 16] = left;
            vertices[BaseVtx + 17] = bottom;
            vertices[BaseVtx + 18] = 0.0f;
            vertices[BaseVtx + 19] = 1.0f;
            vertices[BaseVtx + 20] = r;
            vertices[BaseVtx + 21] = g;
            vertices[BaseVtx + 22] = b;
            vertices[BaseVtx + 23] = a;
            
            // Bottom-right
            vertices[BaseVtx + 24] = right;
            vertices[BaseVtx + 25] = bottom;
            vertices[BaseVtx + 26] = 1.0f;
            vertices[BaseVtx + 27] = 1.0f;
            vertices[BaseVtx + 28] = r;
            vertices[BaseVtx + 29] = g;
            vertices[BaseVtx + 30] = b;
            vertices[BaseVtx + 31] = a;
            
            if(DEBUG)
            {
                // Debug print for each quad
                printf("Quad %d (Col %d, Row %d):\n", QuadIdx, x, y);
                printf("  TL: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n", 
                       vertices[BaseVtx], vertices[BaseVtx+1],
                       vertices[BaseVtx+2], vertices[BaseVtx+3],
                       vertices[BaseVtx+4], vertices[BaseVtx+5],
                       vertices[BaseVtx+6], vertices[BaseVtx+7]);
                printf("  TR: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n", 
                       vertices[BaseVtx+8], vertices[BaseVtx+9],
                       vertices[BaseVtx+10], vertices[BaseVtx+11],
                       vertices[BaseVtx+12], vertices[BaseVtx+13],
                       vertices[BaseVtx+14], vertices[BaseVtx+15]);
                printf("  BL: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n", 
                       vertices[BaseVtx+16], vertices[BaseVtx+17],
                       vertices[BaseVtx+18], vertices[BaseVtx+19],
                       vertices[BaseVtx+20], vertices[BaseVtx+21],
                       vertices[BaseVtx+22], vertices[BaseVtx+23]);
                printf("  BR: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n\n", 
                       vertices[BaseVtx+24], vertices[BaseVtx+25],
                       vertices[BaseVtx+26], vertices[BaseVtx+27],
                       vertices[BaseVtx+28], vertices[BaseVtx+29],
                       vertices[BaseVtx+30], vertices[BaseVtx+31]);
            }
        }
    }
    
    // Generate indices for index buffer
    for (int i = 0; i < cols * rows; i++) {
        int ibaseIdx = i * 4;
        int iBaseInd = i * 6;
        
        indices[iBaseInd + 0] = ibaseIdx + 0;
        indices[iBaseInd + 1] = ibaseIdx + 1;
        indices[iBaseInd + 2] = ibaseIdx + 2;
        
        indices[iBaseInd + 3] = ibaseIdx + 1;
        indices[iBaseInd + 4] = ibaseIdx + 3;
        indices[iBaseInd + 5] = ibaseIdx + 2;
    }
}


ayVec2 transdormGridtoIsometric(float x, float y) 
{
    ayVec2 result;
    result.x = x - y;
    result.y = (x + y) / 2;
    return result;
}

ayVec2 applyIsometricToUV(float u, float v) 
{
    ayVec2 result;
    result.u = u - v;         
    result.v = (u + v) / 2;   
    return result;
}