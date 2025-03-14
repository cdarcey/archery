#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>



ayVec3
ayPixelShader_0(ayPixelShaderBuiltIns tBuiltIns, const ayVaryingData* ptVaryingDataIn)
{

    const ayVec3* ptColor = ay_get_varying(0, ptVaryingDataIn);
    const float*  pfData2 = ay_get_varying(1, ptVaryingDataIn);

    return (ayVec3){ *pfData2 * ptColor->x  * 255, 
                     *pfData2 * ptColor->y  * 255, 
                     *pfData2 * ptColor->z  * 255};
}

ayVec2
ayVertexShader_0(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayVaryingData* ptVaryingDataOut)
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;
    ayVec2 tPos = *(ayVec2*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 1);
    ayVec3 tColor = *(ayVec3*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 3);


    ayVec3* ptColor = ay_set_varying(AY_VARYING_TYPE_VEC3, ptVaryingDataOut);  // color
    float* pfData2 = ay_set_varying(AY_VARYING_TYPE_FLOAT, ptVaryingDataOut); // dull factor

    *ptColor = tColor;

    if(tBuiltIns.uVertexID == 0)
    {
        *pfData2 = 0.5f;
    }

    else if(tBuiltIns.uVertexID == 1)
    {
        *pfData2 = 0.5f;
    }

    else if(tBuiltIns.uVertexID == 2)
    {
        *pfData2 = 0.5f;
    }
    
    return (ayVec2){tPos.x, tPos.y};
}

int main()
{

    ayGraphicsData* ptData = initialize_graphics();
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(256, 256);
    ay_clear_frame_buffer(ptFrameBuffer, (ayVec3){255, 255, 255});

    // code timing start 
    clock_t start, end;
    double cpu_time_used;
    start = clock();
    
    // vertices 
    float afVertexBuffer[] = { // ?, ?, x, y, ?, r, g, b
        0.5f, 0.0f, -1.0f, -1.0f, 0.5f, 1.0f, 0.0f, 0.0f, // top left
        0.5f, 0.0f,  1.0f, -1.0f, 0.5f, 0.0f, 1.0f, 0.0f, // top right
        0.5f, 0.0f, -1.0f,  1.0f, 0.5f, 0.0f, 0.0f, 1.0f, // bottom left
        0.5f, 0.0f,  1.0f,  1.0f, 0.5f, 0.0f, 0.0f, 1.0f, // bottom left
    };
    uint32_t atIndexBuffer[6] = { 
        0, 1, 2, 2, 1, 3 // triangle 0
    };

    ayPipeline tPipeline = {
        .tPixelShader = ayPixelShader_0,
        .tVertexShader = ayVertexShader_0,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                AY_VERTEX_ATTRIBUTE_TYPE_NONE,
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3
            },
            .szAttribOffset = {
                0,
                sizeof(ayVec2),
                sizeof(ayVec2) + sizeof(ayVec2),
                sizeof(ayVec2) + sizeof(float) + sizeof(ayVec2),
            },
            .szVertexStride = sizeof(float) * 8,

        }
    };
    ay_bind_frame_buffer(ptData, ptFrameBuffer);
    ay_bind_vertex_buffer(ptData, afVertexBuffer);
    ay_bind_pipeline(ptData, &tPipeline);
    ay_bind_index_buffer(ptData, atIndexBuffer);
    ay_draw_indexed(ptData, 0, 6);
    ay_output_frame_buffer(ptFrameBuffer);

    // code timing end 
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
}