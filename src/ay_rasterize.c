/*
   ay_rasterize.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "ay_rasterize.h"
#include <stdlib.h>
#include <string.h>
#include "stb_image_write.h"

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline int
ay_edge_function(ayVertex one, ayVertex two, ayVertex three)
{
    return(two.xPos - one.xPos) * (three.yPos - one.yPos) - (two.yPos - one.yPos) * (three.xPos - one.xPos);
};

static inline int
ay_max_num(int a, int b, int c)
{
    if(a > b && a > c)
        return a;
    else if(b > a && b > c)
        return b;
    return c;
};

static inline int
ay_min_num(int a, int b, int c)
{
    if(a < b && a < c)
        return a;
    else if(b < a && b < c)
        return b;
    return c;
};

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
ay_initialize_frame_buffer(ayFrameBufferData* ptData, int iWidth, int iHeight)
{
    ptData->iWidth = iWidth;
    ptData->iHeight = iHeight;
    ptData->pucData = malloc(sizeof(char) * 3 * iWidth * iHeight);
    memset(ptData->pucData, 0, sizeof(char) * 3 * iWidth * iHeight);
};

void
ay_output_frame_buffer(ayFrameBufferData* ptData)
{
    stbi_write_png("output.png", ptData->iWidth, ptData->iHeight, 3, ptData->pucData, sizeof(char) * 3 * ptData->iWidth);
};

void
ay_clear_frame_buffer(ayFrameBufferData* ptData, ayColor tColor)
{

    for(int iRow = 0; iRow < ptData->iHeight; iRow++)
    {
        for(int iColumn = 0; iColumn < ptData->iWidth; iColumn++)
        {
            ay_set_pixel(ptData, (ayVertex){iColumn, iRow}, tColor);
        }
    }

};

void
ay_set_pixel(ayFrameBufferData* ptData, ayVertex input, ayColor tColor)
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

};

void
ay_rasterize_triangles(ayFrameBufferData* ptData, ayVertex* atVerticies, int iVertexCount)
{
    /* p usedfor iterating through pixels*/
    ayVertex vertexP = {
        .xPos = 0,
        .yPos = 0
    };  

    /* loop and set pixels inside triangle */
    for(int i = 0; i < iVertexCount; i += 3)
    {

        /* edge function for entire triangle */
        float ABC = (float)ay_edge_function(atVerticies[i], atVerticies[i + 1], atVerticies[i + 2]);

        /* min & max to only check pixels in a bounding box*/
        const int minX = ay_min_num(atVerticies[i].xPos, atVerticies[i + 1].xPos, atVerticies[i + 2].xPos);
        const int minY = ay_min_num(atVerticies[i].yPos, atVerticies[i + 1].yPos, atVerticies[i + 2].yPos);
        const int maxX = ay_max_num(atVerticies[i].xPos, atVerticies[i + 1].xPos, atVerticies[i + 2].xPos);
        const int maxY = ay_max_num(atVerticies[i].yPos, atVerticies[i + 1].yPos, atVerticies[i + 2].yPos); 

        for(vertexP.yPos = 0; vertexP.yPos < 256; vertexP.yPos++)
        {
            for(vertexP.xPos = 0; vertexP.xPos < 256; vertexP.xPos++)
            {
                const int ABP = ay_edge_function(atVerticies[i],     atVerticies[i + 1], vertexP);
                const int BCP = ay_edge_function(atVerticies[i + 1], atVerticies[i + 2], vertexP);
                const int CAP = ay_edge_function(atVerticies[i + 2], atVerticies[i],     vertexP);

                const float weightA = BCP / ABC;
                const float weightB = CAP / ABC;
                const float weightC = ABP / ABC;

                if(ABP >= 0 && BCP >= 0 && CAP >= 0)
                {
                    unsigned char red   = (unsigned char)((float)(atVerticies[0].r * weightA) + (float)(atVerticies[1].r * weightB) + (float)(atVerticies[2].r * weightC));
                    unsigned char green = (unsigned char)((float)(atVerticies[0].g * weightA) + (float)(atVerticies[1].g * weightB) + (float)(atVerticies[2].g * weightC));
                    unsigned char blue  = (unsigned char)((float)(atVerticies[0].b * weightA) + (float)(atVerticies[1].b * weightB) + (float)(atVerticies[2].b * weightC));

                    ayColor colorP = {red, green, blue};
                    ay_set_pixel(ptData, vertexP, colorP);
                }
            }
        }
    }
};

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"