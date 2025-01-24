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

typedef struct _ayVec2
{
    float x;
    float y;
} ayVec2;

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
    // unsigned char r;
    // unsigned char g;
    // unsigned char b;

} ayVertex;

typedef struct _ayPixelShaderBuiltIns
{
    ayVec2 tUV;
} ayPixelShaderBuiltIns;

typedef struct _ayVertexShaderBuiltIns
{
    uint32_t uVertexID;
} ayVertexShaderBuiltIns;

typedef struct _ayVaryingData
{
    // TODO: add system in here for varying layout
    //       so you can interpolate them correctly
    char acVaryingData[256]; // this should be cast to correct types once you decide on layout system
} ayVaryingData;

// function pointers
typedef ayColor (*ayPixelShader)(ayPixelShaderBuiltIns, const ayVaryingData* ptVaryingDataIn);
typedef ayVec2 (*ayVertexShader)(ayVertexShaderBuiltIns, const void* pVertexDataIn, ayVaryingData* ptVaryingDataOut);

typedef struct _ayPipeline
{
    ayVertexShader tVertexShader;
    ayPixelShader  tPixelShader;

    // TODO:
    //   * vertex layout
} ayPipeline;


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
void ay_bind_pipeline(ayGraphicsData*, ayPipeline*);

// shaders
void ay_bind_pixel_shader (ayGraphicsData*, ayPixelShader);
void ay_bind_vertex_shader(ayGraphicsData*, ayVertexShader);

// draw calls
void ay_draw(ayGraphicsData*, uint32_t uFirstVertex, uint32_t uVertexCount);
void ay_draw_indexed(ayGraphicsData*, uint32_t uFirstIndex, uint32_t uIndexCount);

#endif