/*
   ay_rasterize.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
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
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _ayGraphicsData
{
    ayFrameBufferData* ptFrameBufferData;
    ayVertex*          atVerticies;
    ayPixelShader      tPixelShader;
    ayVertexShader     tVertexShader;
    uint32_t*          puIndexBufferData;
} ayGraphicsData;

typedef struct _ayFrameBufferData
{
    uint32_t       uWidth;
    uint32_t       uHeight;
    unsigned char* pucData;
} ayFrameBufferData;

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

static void ay_set_pixel(ayFrameBufferData* ptData, ayVertex input, ayColor tColor);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

ayGraphicsData*
initialize_graphics(void)
{
    ayGraphicsData* ptData = malloc(sizeof(ayGraphicsData));
    memset(ptData, 0, sizeof(ayGraphicsData));
    return ptData;
}

void
ay_bind_frame_buffer(ayGraphicsData* ptData, ayFrameBufferData* ptFrameBuffer)
{
    ptData->ptFrameBufferData = ptFrameBuffer;
}

void
ay_bind_vertex_buffer(ayGraphicsData* ptData, ayVertex* atVerticies)
{
    ptData->atVerticies = atVerticies;
}

void
ay_bind_pixel_shader(ayGraphicsData* ptData, ayPixelShader tShader)
{
    ptData->tPixelShader = tShader;
}

void
ay_bind_vertex_shader(ayGraphicsData* ptData, ayVertexShader tShader)
{
    ptData->tVertexShader = tShader;
}

void
ay_bind_index_buffer(ayGraphicsData* ptData, uint32_t* puIndexBuffer)
{
    ptData->puIndexBufferData = puIndexBuffer;
};

void
ay_draw(ayGraphicsData* ptData, uint32_t uFirstVertex, uint32_t uVertexCount)
{
    /* p usedfor iterating through pixels*/
    ayVertex vertexP = {
        .xPos = 0,
        .yPos = 0
    };  

    /* loop and set pixels inside triangle */
    for(uint32_t i = 0; i < uVertexCount; i += 3)
    {

        const uint32_t uIndex0 = uFirstVertex + i;
        const uint32_t uIndex1 = uFirstVertex + i + 1;
        const uint32_t uIndex2 = uFirstVertex + i + 2;

        ayVertex tVertex0 = ptData->tVertexShader(ptData->atVerticies[uIndex0]);
        ayVertex tVertex1 = ptData->tVertexShader(ptData->atVerticies[uIndex1]);
        ayVertex tVertex2 = ptData->tVertexShader(ptData->atVerticies[uIndex2]);

        /* edge function for entire triangle */
        float ABC = (float)ay_edge_function(tVertex0, ptData->atVerticies[uIndex1], ptData->atVerticies[uIndex2]);

        /* min & max to only check pixels in a bounding box*/
        const int minX = ay_min_num(tVertex0.xPos, tVertex1.xPos, tVertex2.xPos);
        const int minY = ay_min_num(tVertex0.yPos, tVertex1.yPos, tVertex2.yPos);
        const int maxX = ay_max_num(tVertex0.xPos, tVertex1.xPos, tVertex2.xPos);
        const int maxY = ay_max_num(tVertex0.yPos, tVertex1.yPos, tVertex2.yPos); 

        for(vertexP.yPos = 0; vertexP.yPos < ptData->ptFrameBufferData->uHeight; vertexP.yPos++)
        {
            for(vertexP.xPos = 0; vertexP.xPos < ptData->ptFrameBufferData->uWidth; vertexP.xPos++)
            {
                const int ABP = ay_edge_function(tVertex0, tVertex1, vertexP);
                const int BCP = ay_edge_function(tVertex1, tVertex2, vertexP);
                const int CAP = ay_edge_function(tVertex2, tVertex0, vertexP);

                const float weightA = BCP / ABC;
                const float weightB = CAP / ABC;
                const float weightC = ABP / ABC;

                if(ABP >= 0 && BCP >= 0 && CAP >= 0)
                {
                    unsigned char red   = (unsigned char)((float)(tVertex0.r * weightA) + (float)(tVertex1.r * weightB) + (float)(tVertex2.r * weightC));
                    unsigned char green = (unsigned char)((float)(tVertex0.g * weightA) + (float)(tVertex1.g * weightB) + (float)(tVertex2.g * weightC));
                    unsigned char blue  = (unsigned char)((float)(tVertex0.b * weightA) + (float)(tVertex1.b * weightB) + (float)(tVertex2.b * weightC));

                    ayColor colorP = {red, green, blue};
                    ayColor tFinalColor = ptData->tPixelShader(colorP);
                    ay_set_pixel(ptData->ptFrameBufferData, vertexP, tFinalColor);
                }
            }
        }
    }
}

void
ay_draw_indexed(ayGraphicsData* ptData, uint32_t uFirstIndex, uint32_t uIndexCount)
{

    ayVertex vertexP = {
        .xPos = 0,
        .yPos = 0
    };  

    for(uint32_t i = 0; i < uIndexCount; i += 3)
    {

        const uint32_t uIndex0 = ptData->puIndexBufferData[uFirstIndex + i];
        const uint32_t uIndex1 = ptData->puIndexBufferData[uFirstIndex + i + 1];
        const uint32_t uIndex2 = ptData->puIndexBufferData[uFirstIndex + i + 2];

        ayVertex tVertex0 = ptData->tVertexShader(ptData->atVerticies[uIndex0]);
        ayVertex tVertex1 = ptData->tVertexShader(ptData->atVerticies[uIndex1]);
        ayVertex tVertex2 = ptData->tVertexShader(ptData->atVerticies[uIndex2]);

        /* edge function for entire triangle */
        float ABC = (float)ay_edge_function(tVertex0, ptData->atVerticies[uIndex1], ptData->atVerticies[uIndex2]);

        /* min & max to only check pixels in a bounding box*/
        const int minX = ay_min_num(tVertex0.xPos, tVertex1.xPos, tVertex2.xPos);
        const int minY = ay_min_num(tVertex0.yPos, tVertex1.yPos, tVertex2.yPos);
        const int maxX = ay_max_num(tVertex0.xPos, tVertex1.xPos, tVertex2.xPos);
        const int maxY = ay_max_num(tVertex0.yPos, tVertex1.yPos, tVertex2.yPos); 

        for(vertexP.yPos = 0; vertexP.yPos < ptData->ptFrameBufferData->uHeight; vertexP.yPos++)
        {
            for(vertexP.xPos = 0; vertexP.xPos < ptData->ptFrameBufferData->uWidth; vertexP.xPos++)
            {
                const int ABP = ay_edge_function(tVertex0, tVertex1, vertexP);
                const int BCP = ay_edge_function(tVertex1, tVertex2, vertexP);
                const int CAP = ay_edge_function(tVertex2, tVertex0, vertexP);

                const float weightA = BCP / ABC;
                const float weightB = CAP / ABC;
                const float weightC = ABP / ABC;

                if(ABP >= 0 && BCP >= 0 && CAP >= 0)
                {
                    unsigned char red   = (unsigned char)((float)(tVertex0.r * weightA) + (float)(tVertex1.r * weightB) + (float)(tVertex2.r * weightC));
                    unsigned char green = (unsigned char)((float)(tVertex0.g * weightA) + (float)(tVertex1.g * weightB) + (float)(tVertex2.g * weightC));
                    unsigned char blue  = (unsigned char)((float)(tVertex0.b * weightA) + (float)(tVertex1.b * weightB) + (float)(tVertex2.b * weightC));

                    ayColor colorP = {red, green, blue};
                    ayColor tFinalColor = ptData->tPixelShader(colorP);
                    ay_set_pixel(ptData->ptFrameBufferData, vertexP, tFinalColor);
                }
            }
        }
    }
}

ayFrameBufferData*
ay_initialize_frame_buffer(uint32_t uWidth, uint32_t uHeight)
{

    ayFrameBufferData* ptData = malloc(sizeof(ayFrameBufferData));
    memset(ptData, 0, sizeof(ayFrameBufferData));

    ptData->uWidth = uWidth;
    ptData->uHeight = uHeight;
    ptData->pucData = malloc(sizeof(char) * 3 * uWidth * uHeight);
    memset(ptData->pucData, 0, sizeof(char) * 3 * uWidth * uHeight);

    return ptData;
};

void
ay_output_frame_buffer(ayFrameBufferData* ptData)
{
    stbi_write_png("output.png", ptData->uWidth, ptData->uHeight, 3, ptData->pucData, sizeof(char) * 3 * ptData->uWidth);
};

void
ay_clear_frame_buffer(ayFrameBufferData* ptData, ayColor tColor)
{

    for(uint32_t iRow = 0; iRow < ptData->uHeight; iRow++)
    {
        for(uint32_t iColumn = 0; iColumn < ptData->uWidth; iColumn++)
        {
            ay_set_pixel(ptData, (ayVertex){iColumn, iRow}, tColor);
        }
    }

};

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
ay_set_pixel(ayFrameBufferData* ptData, ayVertex input, ayColor tColor)
{

    if(input.xPos < 0)
        return;

    if(input.yPos < 0)
        return;

    if(input.xPos >= ptData->uWidth)
        return;

    if(input.yPos >= ptData->uHeight)
        return;

    int iRowOffset = ptData->uWidth * 3 * input.yPos;
    int iPixelStart = iRowOffset + input.xPos * 3;

    ptData->pucData[iPixelStart + 0] = tColor.r;
    ptData->pucData[iPixelStart + 1] = tColor.g;
    ptData->pucData[iPixelStart + 2] = tColor.b;

};

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"