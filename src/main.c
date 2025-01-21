#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

ayColor ayPixelShader_0(ayColor tColor)
{
    return tColor;
}

ayVertex ayVertexShader_0(ayVertex tVertex)
{
    return tVertex;
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
        .xPos = 10,
        .yPos = 100,
        .r    = 255,
        .g    = 0,
        .b    = 0
    };

    ayVertex b = {
        .xPos = 75,
        .yPos = 10,
        .r    = 0,
        .g    = 255,
        .b    = 0
    };
    ayVertex c = {
        .xPos = 100,
        .yPos = 100,
        .r    = 0,
        .g    = 0,
        .b    = 255
    };
    ayVertex d = {
        .xPos = 125,
        .yPos = 175,
        .r    = 255,
        .g    = 0,
        .b    = 0
    };
    ayVertex e = {
        .xPos = 200,
        .yPos = 125,
        .r    = 0,
        .g    = 255,
        .b    = 255
    };
    ayVertex f = {
        .xPos = 200,
        .yPos = 200,
        .r    = 0,
        .g    = 255,
        .b    = 255
    };

    ayVertex* atVertexBuffer = malloc(sizeof(ayVertex) * 6);
    atVertexBuffer[0] = a;
    atVertexBuffer[1] = b;
    atVertexBuffer[2] = c;
    atVertexBuffer[3] = d;
    atVertexBuffer[4] = e;
    atVertexBuffer[5] = f;

    ay_bind_frame_buffer(ptData, ptFrameBuffer);
    ay_bind_vertex_buffer(ptData, atVertexBuffer);
    ay_bind_pixel_shader(ptData, ayPixelShader_0);
    ay_bind_vertex_shader(ptData, ayVertexShader_0);
    ay_draw(ptData, 3);

    ay_output_frame_buffer(ptFrameBuffer);

    /* code timing end */
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
}