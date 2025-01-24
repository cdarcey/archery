#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

ayColor
ayPixelShader_0(ayPixelShaderBuiltIns tBuiltIns, const ayVaryingData* ptVaryingDataIn)
{
    return (ayColor){tBuiltIns.tUV.x * 255, tBuiltIns.tUV.y * 255, 255};
}

ayVec2
ayVertexShader_0(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayVaryingData* ptVaryingDataOut)
{
    ayVertex* ptVertex = (ayVertex*)pVertexDataIn;
    return (ayVec2){ptVertex->xPos, ptVertex->yPos};
}

int main()
{

    ayGraphicsData* ptData = initialize_graphics();

    /* code timing start */
    clock_t start, end;
    double cpu_time_used;
    start = clock();

    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(256, 256);
    ay_clear_frame_buffer(ptFrameBuffer, (ayColor){255, 255, 255});

    /* vertices */
    ayVertex VertexP = {0};

    ayVertex a = {
        .xPos = -1.0f,
        .yPos = -1.0f
    };

    ayVertex b = {
        .xPos = -1.0f,
        .yPos = 1.0f
    };
    ayVertex c = {
        .xPos = 1.0f,
        .yPos = 1.0f
    };


    ayVertex* atVertexBuffer = malloc(sizeof(ayVertex) * 3);
    atVertexBuffer[0] = a;
    atVertexBuffer[1] = b;
    atVertexBuffer[2] = c;

    int* tVertexBuffer = malloc(sizeof(int) * 3);
    tVertexBuffer[0] = 0;
    tVertexBuffer[1] = 2;
    tVertexBuffer[2] = 1;

    ayPipeline tPipeline = {
        .tPixelShader = ayPixelShader_0,
        .tVertexShader = ayVertexShader_0
    };

    ay_bind_frame_buffer(ptData, ptFrameBuffer);
    ay_bind_vertex_buffer(ptData, atVertexBuffer);
    ay_bind_pipeline(ptData, &tPipeline);
    ay_bind_index_buffer(ptData, tVertexBuffer);
    ay_draw_indexed(ptData, 0, 3);

    ay_output_frame_buffer(ptFrameBuffer);

    /* code timing end */
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
}