#include "ay_rasterize.h"
#include <time.h>
#include <stdio.h>

int main()
{
    /* code timing start */
    clock_t start, end;
    double cpu_time_used;
    start = clock();

    ayFrameBufferData tData = {0};
    ay_initialize_frame_buffer(&tData, 256, 256);
    ay_clear_frame_buffer(&tData, (ayColor){255, 255, 255});

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
        .b    = 0
    };
    ayVertex f = {
        .xPos = 200,
        .yPos = 200,
        .r    = 0,
        .g    = 0,
        .b    = 255
    };

    ayVertex atVertexBuffer[6] = {a, b, c, d, e, f};

    ay_rasterize_triangles(&tData, atVertexBuffer, 6);
    ay_output_frame_buffer(&tData);

    /* code timing end */
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
}