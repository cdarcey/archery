#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stb_image_write.h"

typedef struct _plData
{
    int            iWidth;
    int            iHeight;
    unsigned char* pucData;
} plData;

typedef struct _plColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} plColor;

void pl_initialize_frame_buffer(plData* ptData, int iWidth, int iHeight);
void pl_output_frame_buffer(plData* ptData);
void pl_clear_frame_buffer(plData* ptData, plColor tColor);
void pl_set_pixel(plData* ptData, int iX, int iY, plColor tColor);

void pl_rasterize_triangle(plData* ptData, int iX0, int iY0, int iX1, int iY1, int iX2, int iY2, plColor tColor)
{

}


int main()
{
    plData tData = {0};
    pl_initialize_frame_buffer(&tData, 256, 256);
    pl_clear_frame_buffer(&tData, (plColor){0});

    pl_set_pixel(&tData, 100, 100, (plColor){255, 255, 0});

    pl_output_frame_buffer(&tData);
    
}


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void pl_initialize_frame_buffer(plData* ptData, int iWidth, int iHeight)
{
    ptData->iWidth = iWidth;
    ptData->iHeight = iHeight;
    ptData->pucData = malloc(sizeof(char) * 3 * iWidth * iHeight);
    memset(ptData->pucData, 0, sizeof(char) * 3 * iWidth * iHeight);
}

void pl_output_frame_buffer(plData* ptData)
{
    stbi_write_png("output.png", ptData->iWidth, ptData->iHeight, 3, ptData->pucData, sizeof(char) * 3 * ptData->iWidth);
}

void pl_clear_frame_buffer(plData* ptData, plColor tColor)
{

    for(int iRow = 0; iRow < ptData->iHeight; iRow++)
    {
        for(int iColumn = 0; iColumn < ptData->iWidth; iColumn++)
        {
            pl_set_pixel(ptData, iColumn, iRow, tColor);
        }
    }

}

void pl_set_pixel(plData* ptData, int iX, int iY, plColor tColor)
{

    if(iX < 0)
        return;

    if(iY < 0)
        return;

    if(iX >= ptData->iWidth)
        return;

    if(iY >= ptData->iHeight)
        return;

    int iRowOffset = ptData->iWidth * 3 * iY;
    int iPixelStart = iRowOffset + iX * 3;

    ptData->pucData[iPixelStart + 0] = tColor.r;
    ptData->pucData[iPixelStart + 1] = tColor.g;
    ptData->pucData[iPixelStart + 2] = tColor.b;

}