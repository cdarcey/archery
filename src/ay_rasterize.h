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

#include <glfw3.h> // windowing
#include <stdint.h> // uint*_t
#include <stddef.h> // size_t
#include <stdbool.h> // bool

#include "ay_math.h"

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

typedef enum _ayVaryingType
{
    AY_VARYING_TYPE_NONE = 0,
    AY_VARYING_TYPE_VEC2,
    AY_VARYING_TYPE_VEC3,
    AY_VARYING_TYPE_VEC4,
    AY_VARYING_TYPE_FLOAT,
    AY_MAX_VARYINGS
} ayVaryingType;

typedef enum _ayVertexAttributeType
{
    AY_VERTEX_ATTRIBUTE_TYPE_NONE = 0,
    AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
    AY_VERTEX_ATTRIBUTE_TYPE_VEC3,
    AY_VERTEX_ATTRIBUTE_TYPE_VEC4,
    AY_VERTEX_ATTRIBUTE_TYPE_FLOAT
} ayVertexAttributeType;

typedef enum _ayVertexWinding
{
    AY_VERTEX_WINDING_CLOCKWISE,
    AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
    AY_VERTEX_WINDING_NONE
} ayVertexWinding;

typedef enum _ayDescriptorType
{
    AY_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    AY_DESCRIPTOR_TYPE_TEXTURE,
    AY_DESCRIPTOR_TYPE_STORAGE
} ayDescriptorType;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _ayGraphicsData ayGraphicsData;    // opaque

typedef struct ayWindow {
    GLFWwindow* pWindow;
    GLuint      uframebufferTexture;      // gl texture id for framebuffer
    uint32_t    uWidth;
    uint32_t    uHeight;
} ayWindow;

typedef struct _ayFrameBufferData
{
    uint32_t       uWidth;
    uint32_t       uHeight;
    unsigned char* pucData;
    float*         pfDepthBuffer;
    bool           bDepthEnabled;
} ayFrameBufferData;

typedef struct _ayTexture
{
    unsigned char* pucData;
    int            iHeight;
    int            iWidth;
} ayTexture;

typedef struct _ayVertexLayout
{
    ayVertexAttributeType tAttribType[16];
    size_t                szAttribOffset[16];
    size_t                szVertexStride;
} ayVertexLayout;

typedef struct _ayPixelShaderBuiltIns
{
    ayVec2    tUV;
} ayPixelShaderBuiltIns;

typedef struct _ayVertexShaderBuiltIns
{
    uint32_t       uVertexID;
    ayVertexLayout tLayout;
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

typedef struct _ayDescriptor
{
    const void*      pData;
    ayDescriptorType eType;
} ayDescriptor;

// function pointers
typedef ayVec4 (*ayPixelShader)(ayPixelShaderBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn);
typedef ayVec3 (*ayVertexShader)(ayVertexShaderBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut);

typedef struct _ayPipeline
{
    ayVertexShader   tVertexShader;
    ayPixelShader    tPixelShader;
    ayVertexLayout   tLayout;
    ayVertexWinding  tVertexWinding;
} ayPipeline;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

//-------------------------------setup-----------------------------------------

ayGraphicsData* initialize_graphics(void);

// windowing & presenting
ayWindow* ay_create_window(uint32_t uWidth, uint32_t uHeight, const char* pcTitle);
void      ay_destroy_window(ayWindow* ptWindow);
bool      ay_window_should_close(ayWindow* ptWindow);
void      ay_present_frame(ayWindow* ptWindow, ayFrameBufferData* ptFrameBuffer);

// framebuffer ops
ayFrameBufferData* ay_initialize_frame_buffer(uint32_t uWidth, uint32_t uHeight, bool bDepthEnabled);
void               ay_output_frame_buffer    (ayFrameBufferData*);
void               ay_clear_frame_buffer     (ayFrameBufferData*);

// helpers
unsigned char* ay_load_png(const char* pcFileName, int* iWidthOut, int* iHeightOut);

//------------------------------commands---------------------------------------

// textures
ayVec4 ay_sample_texture         (ayTexture tTexture, ayVec2 tUV, uint32_t uComponents);
ayVec4 ay_extract_sprite_texture (ayTexture tTexture, ayVec2 tUV, uint32_t uComponents, int spriteX, int spriteY, int spriteWidth, int spriteHeight);
ayVec4 ay_sample_texture_bilinear(ayTexture tTexture, ayVec2 tUV, uint32_t uComponents);

// frame buffers
void ay_bind_frame_buffer(ayGraphicsData*, ayFrameBufferData*);

// buffers & descriptors
void ay_bind_index_buffer (ayGraphicsData*, uint32_t*);
void ay_bind_vertex_buffer(ayGraphicsData*, const void*);
void ay_bind_descriptor(ayGraphicsData* ptData, uint32_t binding, ayDescriptorType type, const void* pData);

// pipelines
void ay_bind_pipeline(ayGraphicsData*, ayPipeline*);

// draw calls
void ay_draw(ayGraphicsData* ptData, uint32_t uFirstVertex, uint32_t uVertexCount); 
void ay_draw_indexed(ayGraphicsData*, uint32_t uFirstIndex, uint32_t uIndexCount); 

//----------------------------shader helpers-----------------------------------

void*       ay_set_varying(ayVaryingType tType, ayVaryingData* ptVaryingDataOut);
const void* ay_get_varying(uint32_t uVaryingIndex, const ayVaryingData* ptVaryingDataOut);
const void* ay_get_vertex_attrib(const void* pcVertexDataIn, ayVertexLayout tLayout, uint32_t tAttribLocation);

#endif