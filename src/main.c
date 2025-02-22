#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

ayColor
ayPixelShader_0(ayPixelShaderBuiltIns tBuiltIns, const ayVaryingData* ptVaryingDataIn)
{
    // ayVec2* blah    = (ayVec2*)&ptVaryingDataIn->acVaryingData[0];
    // ayVec3* blah2   = (ayVec3*)&ptVaryingDataIn->acVaryingData[sizeof(ayVec2)];
    ayVec4* ptColor = (ayVec4*)&ptVaryingDataIn->acVaryingData[sizeof(ayVec2) + sizeof(ayVec3)];
    float*  pfData2 =  (float*)&ptVaryingDataIn->acVaryingData[sizeof(ayVec2) + sizeof(ayVec3) + sizeof(ayVec4)];
    return (ayColor){/* *pfData2 * */ ptColor->x  * 255, 
                     /* *pfData2 * */ ptColor->y  * 255, 
                     /* *pfData2 * */ ptColor->z  * 255};
}

ayVec2
ayVertexShader_0(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayVaryingData* ptVaryingDataOut)
{
    tTypeFlags.ayVec4Type = true;
    tTypeFlags.ayFloatType = false;


    ayVec2 tPos = *(ayVec2*)pVertexDataIn;

    // ayVec2* blah    = (ayVec2*)&ptVaryingDataOut->acVaryingData[0];
    // ayVec3* blah2   = (ayVec3*)&ptVaryingDataOut->acVaryingData[sizeof(ayVec2)];
    ayVec4* ptColor = (ayVec4*)&ptVaryingDataOut->acVaryingData[sizeof(ayVec2) + sizeof(ayVec3)];
    // float* pfData2  =  (float*)&ptVaryingDataOut->acVaryingData[sizeof(ayVec2) + sizeof(ayVec3) + sizeof(ayVec4)];

    if(tBuiltIns.uVertexID == 0)
    {
        ptColor->x = 1.0f;
        ptColor->y = 0;
        ptColor->z = 0;
        ptColor->w = 1.0f;

        // *pfData2 = 0.5f;
    }

    else if(tBuiltIns.uVertexID == 1)
    {
        ptColor->x = 0;
        ptColor->y = 1.0f;
        ptColor->z = 0;
        ptColor->w = 1.0f;

        // *pfData2 = 0.5f;
    }

    else if(tBuiltIns.uVertexID == 2)
    {
        ptColor->x = 0;
        ptColor->y = 0;
        ptColor->z = 1.0f;
        ptColor->w = 1.0f;

        // *pfData2 = 0.5f;
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
         0.0f, -1.0f, // top left
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