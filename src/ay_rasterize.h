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

// high level
ayGraphicsData* initialize_graphics(void);

// framebuffer ops
ayFrameBufferData* ay_initialize_frame_buffer(int iWidth, int iHeight);
void               ay_output_frame_buffer    (ayFrameBufferData*);
void               ay_clear_frame_buffer     (ayFrameBufferData*, ayColor);

// command buffer
void ay_bind_frame_buffer (ayGraphicsData*, ayFrameBufferData*);
void ay_bind_vertex_buffer(ayGraphicsData*, ayVertex*);
void ay_bind_pixel_shader (ayGraphicsData*, ayPixelShader);
void ay_bind_vertex_shader(ayGraphicsData*, ayVertexShader);

// TODO: add first vertex
void ay_draw(ayGraphicsData*, int iVertexCount);

// TODO: index buffering
// void ay_draw_indexed(ayGraphicsData*, int iIndexCount, int iFirstIndex);
// void ay_bind_index_buffer(ayGraphicsData*, int*);

#endif