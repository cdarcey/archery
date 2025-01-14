#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stb_image_write.h"

typedef struct _Data
{
    int            iWidth;
    int            iHeight;
    unsigned char* pucData;
} Data;

typedef struct _Color
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} Color;

typedef struct _Vertex
{
    int xPos;
    int yPos;
} Vertex;

void initialize_frame_buffer(Data* ptData, int iWidth, int iHeight);
void output_frame_buffer(Data* ptData);
void clear_frame_buffer(Data* ptData, Color tColor);
void set_pixel(Data* ptData, Vertex input, Color tColor);
void rasterize_triangle(Data* ptData, Vertex a, Vertex b, Vertex c, Color tColor);
int edgeFunction(Vertex a, Vertex b, Vertex c);


int main()
{

    Data tData = {0};
    initialize_frame_buffer(&tData, 256, 256);
    // clear_frame_buffer(&tData, (Color){0});

    Vertex a = {
        .xPos = 50,
        .yPos = 50
    };

    Vertex b = {
        .xPos = 150,
        .yPos = 50
    };

    Vertex c = {
        .xPos = 75,
        .yPos = 150
    };

    Color tColor = {
        .r = 255,
        .g = 255,
        .b = 255
    };


    rasterize_triangle(&tData, a, b, c, tColor);
    output_frame_buffer(&tData);
    
}


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void initialize_frame_buffer(Data* ptData, int iWidth, int iHeight)
{
    ptData->iWidth = iWidth;
    ptData->iHeight = iHeight;
    ptData->pucData = malloc(sizeof(char) * 3 * iWidth * iHeight);
    memset(ptData->pucData, 0, sizeof(char) * 3 * iWidth * iHeight);
}

void output_frame_buffer(Data* ptData)
{
    stbi_write_png("output.png", ptData->iWidth, ptData->iHeight, 3, ptData->pucData, sizeof(char) * 3 * ptData->iWidth);
}

void clear_frame_buffer(Data* ptData, Color tColor)
{

    for(int iRow = 0; iRow < ptData->iHeight; iRow++)
    {
        for(int iColumn = 0; iColumn < ptData->iWidth; iColumn++)
        {
//            set_pixel(ptData, iColumn, iRow, tColor);
        }
    }

}

void set_pixel(Data* ptData, Vertex input, Color tColor)
{

    if(input.xPos < 0)
        return;

    if(input.yPos < 0)
        return;

    if(input.xPos >= ptData->iWidth)
        return;

    if(input.yPos >= ptData->iHeight)
        return;

    int iRowOffset = ptData->iWidth * 3 * input.yPos;
    int iPixelStart = iRowOffset + input.xPos * 3;

    ptData->pucData[iPixelStart + 0] = tColor.r;
    ptData->pucData[iPixelStart + 1] = tColor.g;
    ptData->pucData[iPixelStart + 2] = tColor.b;

}

void rasterize_triangle(Data* ptData, Vertex a, Vertex b, Vertex c, Color tColor)
{

    /* 
    make min and max function to loop inside of a bounding box

    const minX = MIN(A.x, B.x, C.x);
    const minY = MIN(A.y, B.y, C.y);
    const maxX = MAX(A.x, B.x, C.x);
    const maxY = MAX(A.y, B.y, C.y);  
    
    */

    Vertex p = {
        .xPos = 0,
        .yPos = 0
    };

    for(p.yPos = 0; p.yPos < ptData->iHeight; p.yPos++)
    {
        for(p.xPos = 0; p.xPos < ptData->iWidth; p.xPos++)
        {
            const ABP = edgeFunction(a, b, p);
            const BCP = edgeFunction(b, c, p);
            const CAP = edgeFunction(c, a, p);

            if(ABP >= 0 && BCP >= 0 && CAP >= 0)
            {
                set_pixel(ptData, p, (Color){tColor.r, tColor.g, tColor.b});
            }
        }
    }
}

int edgeFunction(Vertex a, Vertex b, Vertex c)
{
    return(b.xPos - a.xPos) * (c.yPos - a.yPos) - (b.yPos - a.yPos) * (c.xPos - a.xPos);
}