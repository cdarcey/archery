#include "rasterize.h"

int main()
{
    /* code timing start */
    clock_t start, end;
    double cpu_time_used;
    start = clock();

    ay_Data tData = {0};
    ay_initialize_frame_buffer(&tData, 256, 256);
    ay_clear_frame_buffer(&tData, (ay_Color){255, 255, 255});

    /* vertices */
    ay_Vertex VertexP = {0};

    ay_Vertex a = {
        .xPos = 10,
        .yPos = 100,
        .r    = 255,
        .g    = 0,
        .b    = 0
    };

    ay_Vertex b = {
        .xPos = 75,
        .yPos = 10,
        .r    = 0,
        .g    = 255,
        .b    = 0
    };
    ay_Vertex c = {
        .xPos = 100,
        .yPos = 100,
        .r    = 0,
        .g    = 0,
        .b    = 255
    };
    ay_Vertex d = {
        .xPos = 125,
        .yPos = 175,
        .r    = 255,
        .g    = 0,
        .b    = 0
    };
    ay_Vertex e = {
        .xPos = 200,
        .yPos = 125,
        .r    = 0,
        .g    = 255,
        .b    = 0
    };
    ay_Vertex f = {
        .xPos = 200,
        .yPos = 200,
        .r    = 0,
        .g    = 0,
        .b    = 255
    };

    ay_Vertex atVertexBuffer[6] = {a, b, c, d, e, f};

    ay_rasterize_triangles(&tData, atVertexBuffer, 6);
    ay_output_frame_buffer(&tData);

    /* code timing end */
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
}