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
#include <stddef.h> // size_t

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _ayGraphicsData    ayGraphicsData;    // opaque
typedef struct _ayFrameBufferData ayFrameBufferData; // opaque

typedef enum _ayVaryingType
{
    AY_VARYING_TYPE_NONE = 0,
    AY_VARYING_TYPE_VEC2,
    AY_VARYING_TYPE_VEC3,
    AY_VARYING_TYPE_VEC4,
    AY_VARYING_TYPE_FLOAT,
} ayVaryingType;

typedef enum _ayVertexAttributeType
{
    AY_VERTEX_ATTRIBUTE_TYPE_NONE = 0,
    AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
    AY_VERTEX_ATTRIBUTE_TYPE_VEC3,
    AY_VERTEX_ATTRIBUTE_TYPE_VEC4,
} ayVertexAttributeType;

typedef struct _ayVec2
{
    float x;
    float y;
} ayVec2;

typedef struct _ayVec3
{
    float x;
    float y;
    float z;
} ayVec3;

typedef struct _ayVec4
{
    float x;
    float y;
    float z;
    float w;
} ayVec4;

typedef struct _ayColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ayColor;

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
    ayVaryingType atTypes[16];
    char          acVaryingData[512];

    // internal
    uint32_t _uCurrentVarying;
    uint32_t _uCurrentOffset;
    uint32_t _auOffset[16];
} ayVaryingData;

// function pointers
typedef ayColor (*ayPixelShader)(ayPixelShaderBuiltIns, const ayVaryingData* ptVaryingDataIn);
typedef ayVec2 (*ayVertexShader)(ayVertexShaderBuiltIns, const void* pVertexDataIn, ayVaryingData* ptVaryingDataOut);

typedef struct _ayPipeline
{
    ayVertexShader tVertexShader;
    ayPixelShader  tPixelShader;

    // replace with vertex layout system
    size_t szVertexStride;
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
void ay_bind_index_buffer (ayGraphicsData*, uint32_t*);
void ay_bind_vertex_buffer(ayGraphicsData*, const void*);

// pipelines
void ay_bind_pipeline(ayGraphicsData*, ayPipeline*);

// draw calls
void ay_draw(ayGraphicsData*, uint32_t uFirstVertex, uint32_t uVertexCount);
void ay_draw_indexed(ayGraphicsData*, uint32_t uFirstIndex, uint32_t uIndexCount);


//----------------------------shader helpers-----------------------------------

void*       ay_set_varying(ayVaryingType tType, ayVaryingData* ptVaryingDataOut);
const void* ay_get_varying(uint32_t uVaryingIndex, const ayVaryingData* ptVaryingDataOut);

#endif