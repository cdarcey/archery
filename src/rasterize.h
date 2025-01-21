#ifndef RASTERIZE_H
#define RASTERIZE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "stb_image_write.h"
#include <time.h>




typedef struct _ay_Data
{
    int            iWidth;
    int            iHeight;
    unsigned char* pucData;
} ay_Data;

typedef struct _ay_Color
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ay_Color;

typedef struct _ay_Vertex
{

    // position
    int xPos;
    int yPos;

    // color
    unsigned char r;
    unsigned char g;
    unsigned char b;

} ay_Vertex;

// public
void ay_initialize_frame_buffer(ay_Data* ptData, int iWidth, int iHeight);
void ay_output_frame_buffer(ay_Data* ptData);
void ay_clear_frame_buffer(ay_Data* ptData, ay_Color tColor);
void ay_set_pixel(ay_Data* ptData, ay_Vertex input, ay_Color tColor);
void ay_rasterize_triangles(ay_Data* ptData, ay_Vertex* atVerticies, int iVertexCount);

int ay_edgeFunction(ay_Vertex one, ay_Vertex two, ay_Vertex three);
int ay_maxNum(int a, int b, int c);
int ay_minNum(int a, int b, int c);

#endif