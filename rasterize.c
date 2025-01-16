#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

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
void rasterize_triangle(Data* ptData, Vertex a, Vertex b, Vertex c, Color rColor, Color gColor, Color bColor);
int edgeFunction(Vertex a, Vertex b, Vertex c);
int maxNum(int a, int b, int c);
int minNum(int a, int b, int c);


int main()
{


    Data tData = {0};
    initialize_frame_buffer(&tData, 256, 256);
    clear_frame_buffer(&tData, (Color){0});

    Vertex a = {
        .xPos = 50,
        .yPos = 200
    };

    Vertex b = {
        .xPos = 125,
        .yPos = 50
    };

    Vertex c = {
        .xPos = 200,
        .yPos = 200
    };

    Color tColor = {
        .r = 255,
        .g = 255,
        .b = 255
    };

    Color red = {
        .r = 255,
        .g = 0,
        .b = 0
    };
    
    Color green = {
        .r = 0,
        .g = 255,
        .b = 0
    };

    Color blue = {
        .r = 0,
        .g = 0,
        .b = 255
    };


    rasterize_triangle(&tData, a, b, c, red, green, blue);
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
    Vertex p = {
        .xPos = 0,
        .yPos = 0
    };

    for(int iRow = 0; iRow < ptData->iHeight; iRow++)
    {
        for(int iColumn = 0; iColumn < ptData->iWidth; iColumn++)
        {
            set_pixel(ptData, p , tColor);
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

void rasterize_triangle(Data* ptData, Vertex a, Vertex b, Vertex c, Color rColor, Color gColor, Color bColor)
{ 
    /* p usedfor iterating through pixels*/
    Vertex p = {
        .xPos = 0,
        .yPos = 0
    };

    /* edge function for entire triangle */
    float ABC = edgeFunction(a, b, c);

    /* min & max to only check pixels in a bounding box*/
    const minX = minNum(a.xPos, b.xPos, c.xPos);
    const minY = minNum(a.yPos, b.yPos, c.yPos);
    const maxX = maxNum(a.xPos, b.xPos, c.xPos);
    const maxY = maxNum(a.yPos, b.yPos, c.yPos);  

    /* loop and set pixels inside triangle */
    for(p.yPos = minY; p.yPos < maxY; p.yPos++)
    {
        for(p.xPos = minX; p.xPos < maxX; p.xPos++)
        {
            const int ABP = edgeFunction(a, b, p);
            const int BCP = edgeFunction(b, c, p);
            const int CAP = edgeFunction(c, a, p);

            const float weightA = BCP / ABC;
            const float weightB = CAP / ABC;
            const float weightC = ABP / ABC;

            if(ABP >= 0 && BCP >= 0 && CAP >= 0)
            {
                const r = (rColor.r * weightA) + (gColor.r * weightB) + (bColor.r * weightC);
                const g = rColor.g * weightA + gColor.g * weightB + bColor.g * weightC;
                const b = rColor.b * weightA + gColor.b * weightB + bColor.b * weightC;
                Color colorP = {r, g, b};
                set_pixel(ptData, p, colorP);
            }
        }
    }
}

int edgeFunction(Vertex a, Vertex b, Vertex c)
{
    return(b.xPos - a.xPos) * (c.yPos - a.yPos) - (b.yPos - a.yPos) * (c.xPos - a.xPos);
}

int maxNum(int a, int b, int c)
{
    if(a > b && a > c)
    {return a;}
    else if(b > a && b > c)
    {return b;}
    else return c;
}

int minNum(int a, int b, int c)
{
    if(a < b && a < c)
    {return a;}
    else if(b < a && b < c)
    {return b;}
    else return c;
}