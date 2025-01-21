/*
   ay_rasterize.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] structs
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef AY_RASTERIZE_H
#define AY_RASTERIZE_H

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _ayFrameBufferData
{
    int            iWidth;
    int            iHeight;
    unsigned char* pucData;
} ayFrameBufferData;

typedef struct _ayColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ayColor;

typedef struct _ayVertex
{

    // position
    int xPos;
    int yPos;

    // color
    unsigned char r;
    unsigned char g;
    unsigned char b;

} ayVertex;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void ay_initialize_frame_buffer(ayFrameBufferData* ptData, int iWidth, int iHeight);
void ay_output_frame_buffer(ayFrameBufferData* ptData);
void ay_clear_frame_buffer(ayFrameBufferData* ptData, ayColor tColor);
void ay_set_pixel(ayFrameBufferData* ptData, ayVertex input, ayColor tColor);
void ay_rasterize_triangles(ayFrameBufferData* ptData, ayVertex* atVerticies, int iVertexCount);

#endif