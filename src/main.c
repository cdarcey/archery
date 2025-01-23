#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

ayColor ayPixelShader_0(ayVec2 tUV)
{
    return (ayColor){tUV.x * 255, tUV.y * 255, 255};
}

ayVec2 ayVertexShader_0(ayVertex tVertex)
{
    return (ayVec2){tVertex.xPos, tVertex.yPos};
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
        .yPos = -1.0f,
        .r    = 255,
        .g    = 0,
        .b    = 0
    };

    ayVertex b = {
        .xPos = -1.0f,
        .yPos = 1.0f,
        .r    = 0,
        .g    = 255,
        .b    = 0
    };
    ayVertex c = {
        .xPos = 1.0f,
        .yPos = 1.0f,
        .r    = 0,
        .g    = 0,
        .b    = 255
    };


    ayVertex* atVertexBuffer = malloc(sizeof(ayVertex) * 3);
    atVertexBuffer[0] = a;
    atVertexBuffer[1] = b;
    atVertexBuffer[2] = c;

    int* tVertexBuffer = malloc(sizeof(int) * 3);
    tVertexBuffer[0] = 0;
    tVertexBuffer[1] = 2;
    tVertexBuffer[2] = 1;

    ay_bind_frame_buffer(ptData, ptFrameBuffer);
    ay_bind_vertex_buffer(ptData, atVertexBuffer);
    ay_bind_pixel_shader(ptData, ayPixelShader_0);
    ay_bind_vertex_shader(ptData, ayVertexShader_0);
    ay_bind_index_buffer(ptData, tVertexBuffer);
    ay_draw_indexed(ptData, 0, 3);

    ay_output_frame_buffer(ptFrameBuffer);

    /* code timing end */
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
}