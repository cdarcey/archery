#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

ayColor
ayPixelShader_0(ayPixelShaderBuiltIns tBuiltIns, const ayVaryingData* ptVaryingDataIn)
{
    const ayColor* ptColor = (ayColor*)&ptVaryingDataIn->acVaryingData[0];
    return (ayColor){ptColor->r, ptColor->g, ptColor->b};
}

ayVec2
ayVertexShader_0(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayVaryingData* ptVaryingDataOut)
{

    // ayVec2 tPos;
    // ayVec4 tColor = ay_get_vertex_attrib(pVertexDataIn, AY_VERTEX_LAYOUT_TYPE_VEC4, 1);

    ayVec2 tPos = *(ayVec2*)pVertexDataIn;

    ayColor* ptColor = (ayColor*)&ptVaryingDataOut->acVaryingData[0];

    if(tBuiltIns.uVertexID == 0)
    {
        ptColor->r = 255;
        ptColor->g = 0;
        ptColor->b = 0;
    }

    else if(tBuiltIns.uVertexID == 1)
    {
        ptColor->r = 0;
        ptColor->g = 255;
        ptColor->b = 0;
    }

    else if(tBuiltIns.uVertexID == 2)
    {
        ptColor->r = 0;
        ptColor->g = 0;
        ptColor->b = 255;
    }
    
    return (ayVec2){tPos.x, tPos.y};
}

int main()
{

    ayGraphicsData* ptData = initialize_graphics();

    // code timing start 
    clock_t start, end;
    double cpu_time_used;
    start = clock();

    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(256, 256);
    ay_clear_frame_buffer(ptFrameBuffer, (ayColor){255, 255, 255});



    // vertices 
    float afVertexBuffer[6] = { // x, y
        -1.0f, -1.0f, // top left
        -1.0f,  1.0f, // bottom left
         1.0f,  1.0f  // bottom right
    };

    uint32_t atIndexBuffer[3] = { 
        0, 2, 1 // triangle 0
    };

    ayPipeline tPipeline = {
        .tPixelShader = ayPixelShader_0,
        .tVertexShader = ayVertexShader_0,
        .szVertexStride = sizeof(float) * 2
    };

    ay_bind_frame_buffer(ptData, ptFrameBuffer);
    ay_bind_vertex_buffer(ptData, afVertexBuffer);
    ay_bind_pipeline(ptData, &tPipeline);
    ay_bind_index_buffer(ptData, atIndexBuffer);
    ay_draw_indexed(ptData, 0, 3);

    ay_output_frame_buffer(ptFrameBuffer);

    // code timing end 
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
}