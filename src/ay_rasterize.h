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
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h> // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _ayGraphicsData    ayGraphicsData;    // opaque
typedef struct _ayFrameBufferData ayFrameBufferData; // opaque

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

// function pointers
typedef ayColor (*ayPixelShader)(ayColor);
typedef ayVertex (*ayVertexShader)(ayVertex);

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

//-------------------------------setup-----------------------------------------

ayGraphicsData* initialize_graphics(void);

// framebuffer ops
ayFrameBufferData* ay_initialize_frame_buffer(uint32_t uWidth, uint32_t uHeight);
void               ay_output_frame_buffer    (ayFrameBufferData*);
void               ay_clear_frame_buffer     (ayFrameBufferData*, ayColor);

//------------------------------commands---------------------------------------

// frame buffers
void ay_bind_frame_buffer(ayGraphicsData*, ayFrameBufferData*);

// buffers
void ay_bind_vertex_buffer(ayGraphicsData*, ayVertex*);
void ay_bind_index_buffer (ayGraphicsData*, uint32_t*);

// shaders
void ay_bind_pixel_shader (ayGraphicsData*, ayPixelShader);
void ay_bind_vertex_shader(ayGraphicsData*, ayVertexShader);

// draw calls
void ay_draw(ayGraphicsData*, uint32_t uFirstVertex, uint32_t uVertexCount);
void ay_draw_indexed(ayGraphicsData*, uint32_t uFirstIndex, uint32_t uIndexCount);

#endif