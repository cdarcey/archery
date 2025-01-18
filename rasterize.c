#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "stb_image_write.h"

typedef struct _cd_Data
{
    int            iWidth;
    int            iHeight;
    unsigned char* pucData;
} cd_Data;

typedef struct _cd_Color
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} cd_Color;

typedef struct _cd_Vertex
{

    // position
    int xPos;
    int yPos;

    // color
    unsigned char r;
    unsigned char g;
    unsigned char b;

} cd_Vertex;

// public
void cd_initialize_frame_buffer(cd_Data* ptData, int iWidth, int iHeight);
void cd_output_frame_buffer(cd_Data* ptData);
void cd_clear_frame_buffer(cd_Data* ptData, cd_Color tColor);
void cd_set_pixel(cd_Data* ptData, cd_Vertex input, cd_Color tColor);
// void cd_rasterize_triangle(cd_Data* ptData, cd_Vertex a, cd_Vertex b, cd_Vertex c, cd_Color rColor, cd_Color gColor, cd_Color bColor);

// homework
void cd_rasterize_triangles(cd_Data* ptData, cd_Vertex* atVerticies, int iVertexCount);

int main()
{

    cd_Data tData = {0};
    cd_initialize_frame_buffer(&tData, 256, 256);
    cd_clear_frame_buffer(&tData, (cd_Color){255, 255, 255});

    /* vertices */
    cd_Vertex VertexP = {
        .xPos = 0,
        .yPos = 0,
        .r    = 0,
        .g    = 0,
        .b    = 0
    };

    cd_Vertex a = {
        .xPos = 10,
        .yPos = 100,
        .r    = 255,
        .g    = 0,
        .b    = 0
    };

    cd_Vertex b = {
        .xPos = 75,
        .yPos = 10,
        .r    = 0,
        .g    = 255,
        .b    = 0
    };
    cd_Vertex c = {
        .xPos = 100,
        .yPos = 100,
        .r    = 0,
        .g    = 0,
        .b    = 255
    };
    cd_Vertex d = {
        .xPos = 125,
        .yPos = 175,
        .r    = 255,
        .g    = 0,
        .b    = 0
    };

    cd_Vertex e = {
        .xPos = 200,
        .yPos = 125,
        .r    = 0,
        .g    = 255,
        .b    = 0
    };
    cd_Vertex f = {
        .xPos = 200,
        .yPos = 200,
        .r    = 0,
        .g    = 0,
        .b    = 255
    };

    cd_Vertex atVertexBuffer[6] = {a, b, c, d, e, f};

    cd_rasterize_triangles(&tData, atVertexBuffer, 6);
    cd_output_frame_buffer(&tData);
    
}

void cd_initialize_frame_buffer(cd_Data* ptData, int iWidth, int iHeight)
{
    ptData->iWidth = iWidth;
    ptData->iHeight = iHeight;
    ptData->pucData = malloc(sizeof(char) * 3 * iWidth * iHeight);
    memset(ptData->pucData, 0, sizeof(char) * 3 * iWidth * iHeight);
}

void cd_output_frame_buffer(cd_Data* ptData)
{
    stbi_write_png("output.png", ptData->iWidth, ptData->iHeight, 3, ptData->pucData, sizeof(char) * 3 * ptData->iWidth);
}

void cd_clear_frame_buffer(cd_Data* ptData, cd_Color tColor)
{

    for(int iRow = 0; iRow < ptData->iHeight; iRow++)
    {
        for(int iColumn = 0; iColumn < ptData->iWidth; iColumn++)
        {
            cd_set_pixel(ptData, (cd_Vertex){iColumn, iRow}, tColor);
        }
    }

}

int cd_edgeFunction(cd_Vertex one, cd_Vertex two, cd_Vertex three)
{
    return(two.xPos - one.xPos) * (three.yPos - one.yPos) - (two.yPos - one.yPos) * (three.xPos - one.xPos);
    // return(atVerticies[1].xPos - atVerticies[0].xPos) * (VertexP.yPos - atVerticies[0].yPos) - (atVerticies[1].yPos - atVerticies[0].yPos) * (VertexP.xPos - atVerticies[1].xPos);
}

int cd_maxNum(int a, int b, int c)
{
    if(a > b && a > c)
    {return a;}
    else if(b > a && b > c)
    {return b;}
    else return c;
}

int cd_minNum(int a, int b, int c)
{
    if(a < b && a < c)
    {return a;}
    else if(b < a && b < c)
    {return b;}
    else return c;
}

void cd_set_pixel(cd_Data* ptData, cd_Vertex input, cd_Color tColor)
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

/*
void cd_rasterize_triangle(cd_Data* ptData, cd_Vertex a, cd_Vertex b, cd_Vertex c, cd_Color rColor, cd_Color gColor, cd_Color bColor)
{ 
    // p usedfor iterating through pixels
    cd_Vertex p = {
        .xPos = 0,
        .yPos = 0
    };

    // edge function for entire triangle 
    float ABC = (float)edgeFunction(a, b, c);

    // min & max to only check pixels in a bounding box
    const int minX = cd_minNum(a.xPos, b.xPos, c.xPos);
    const int minY = cd_minNum(a.yPos, b.yPos, c.yPos);
    const int maxX = cd_maxNum(a.xPos, b.xPos, c.xPos);
    const int maxY = cd_maxNum(a.yPos, b.yPos, c.yPos);  

    // loop and set pixels inside triangle 
    for(p.yPos = minY; p.yPos < maxY; p.yPos++)
    {
        for(p.xPos = minX; p.xPos < maxX; p.xPos++)
        {
            const int ABP = cd_edgeFunction(a, b, p);
            const int BCP = cd_edgeFunction(b, c, p);
            const int CAP = cd_edgeFunction(c, a, p);

            const float weightA = BCP / ABC;
            const float weightB = CAP / ABC;
            const float weightC = ABP / ABC;

            if(ABP >= 0 && BCP >= 0 && CAP >= 0)
            {
                unsigned char r = (unsigned char)((float)rColor.r * weightA + (float)gColor.r * weightB + (float)bColor.r * weightC);
                unsigned char g = (unsigned char)((float)rColor.g * weightA + (float)gColor.g * weightB + (float)bColor.g * weightC);
                unsigned char b = (unsigned char)((float)rColor.b * weightA + (float)gColor.b * weightB + (float)bColor.b * weightC);
                
                Color colorP = {r, g, b};
                cd_set_pixel(ptData, p, colorP);
            }
        }
    }
}
*/

void cd_rasterize_triangles(cd_Data* ptData, cd_Vertex* atVerticies, int iVertexCount)
{
    /* p usedfor iterating through pixels*/
    cd_Vertex cd_P = {
        .xPos = 0,
        .yPos = 0
    };

    /* edge function for entire triangle */
    float ABC = (float)cd_edgeFunction(atVerticies[0], atVerticies[1], atVerticies[2]);

    /* min & max to only check pixels in a bounding box*/
    // const int minX = cd_minNum(atVerticies[0].xPos, atVerticies[1].xPos, atVerticies[2].xPos);
    // const int minY = cd_minNum(atVerticies[0].yPos, atVerticies[1].yPos, atVerticies[2].yPos);
    // const int maxX = cd_maxNum(atVerticies[0].xPos, atVerticies[1].xPos, atVerticies[2].xPos);
    // const int maxY = cd_maxNum(atVerticies[0].yPos, atVerticies[1].yPos, atVerticies[2].yPos);  

    /* loop and set pixels inside triangle */
    for(int i = 0; i < iVertexCount; i += 3)
    {
        for(cd_P.yPos = 0; cd_P.yPos < 256; cd_P.yPos++)
        {
            for(cd_P.xPos = 0; cd_P.xPos < 256; cd_P.xPos++)
            {
                const int ABP = cd_edgeFunction(atVerticies[i], atVerticies[i + 1], cd_P);
                const int BCP = cd_edgeFunction(atVerticies[i + 1], atVerticies[i + 2], cd_P);
                const int CAP = cd_edgeFunction(atVerticies[i + 2], atVerticies[i], cd_P);

                const float weightA = BCP / ABC;
                const float weightB = CAP / ABC;
                const float weightC = ABP / ABC;

                if(ABP >= 0 && BCP >= 0 && CAP >= 0)
                {
                    unsigned char red   = (unsigned char)((float)(atVerticies[0].r * weightA) + (float)(atVerticies[1].r * weightB) + (float)(atVerticies[2].r * weightC));
                    unsigned char green = (unsigned char)((float)(atVerticies[0].g * weightA) + (float)(atVerticies[1].g * weightB) + (float)(atVerticies[2].g * weightC));
                    unsigned char blue  = (unsigned char)((float)(atVerticies[0].b * weightA) + (float)(atVerticies[1].b * weightB) + (float)(atVerticies[2].b * weightC));

                    cd_Color colorP = {red, green, blue};
                    cd_set_pixel(ptData, cd_P, colorP);
                }
            }
        }
    }
}

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"