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
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _ayGraphicsData ayGraphicsData;    // opaque

typedef struct _ayVertexBuffer
{
    float* fBuffer;
    int    iCapacity;
    int    iSize;
} ayVertexBuffer;

typedef struct _ayIndexBuffer
{
    uint32_t* uBuffer;
    int    iCapacity;
    int    iSize;
} ayIndexBuffer;

typedef struct _ayFrameBufferData
{
    uint32_t       uWidth;
    uint32_t       uHeight;
    unsigned char* pucData;
} ayFrameBufferData;

typedef struct _ayTexture
{
    unsigned char* pucData;
    int            iHeight;
    int            iWidth;
} ayTexture;

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
    AY_VERTEX_ATTRIBUTE_TYPE_FLOAT
} ayVertexAttributeType;

typedef union _ayVec2
{
    struct { float x, y; };
    struct { float r, g; };
    struct { float u, v; };
    float d[2];
} ayVec2;

typedef union _ayVec3
{
    struct { float x, y, z; };
    struct { float r, g, b; };
    struct { float u, v, __; };
    struct { ayVec2 xy; float ignore0_; };
    struct { ayVec2 rg; float ignore1_; };
    struct { ayVec2 uv; float ignore2_; };
    struct { float ignore3_; ayVec2 yz; };
    struct { float ignore4_; ayVec2 gb; };
    struct { float ignore5_; ayVec2 v__; };
    float d[3];
} ayVec3;

typedef union _ayVec4
{
    struct
    {
        union
        {
            ayVec3 xyz;
            struct{ float x, y, z;};
        };
        float w;
    };
    struct
    {
        union
        {
            ayVec3 rgb;
            struct{ float r, g, b;};
        };
        float a;
    };
    struct
    {
        ayVec2 xy;
        float ignored0_, ignored1_;
    };
    struct
    {
        float ignored2_;
        ayVec2 yz;
        float ignored3_;
    };
    struct
    {
        float ignored4_, ignored5_;
        ayVec2 zw;
    };
    float d[4];
} ayVec4;

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
    const void* pData;
} ayDescriptor;

typedef struct _ayDescriptorInfo
{
    ayDescriptor atDescriptors[16];
} ayDescriptorInfo;

// function pointers
typedef ayVec4 (*ayPixelShader)(ayPixelShaderBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn);
typedef ayVec2 (*ayVertexShader)(ayVertexShaderBuiltIns, const void* pVertexDataIn, ayDescriptorInfo* tInfo, ayVaryingData* ptVaryingDataOut);

typedef struct _ayPipeline
{
    ayVertexShader   tVertexShader;
    ayPixelShader    tPixelShader;
    ayVertexLayout   tLayout;
} ayPipeline;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

//-------------------------------setup-----------------------------------------

ayGraphicsData* initialize_graphics(void);

// framebuffer ops
ayFrameBufferData* ay_initialize_frame_buffer(uint32_t uWidth, uint32_t uHeight);
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

// buffers
void ay_bind_index_buffer (ayGraphicsData*, uint32_t*);
void ay_bind_vertex_buffer(ayGraphicsData*, const void*);
void ay_bind_buffer       (ayGraphicsData*, int bufferIndex, const void*);
void ay_bind_texture      (ayGraphicsData* ptData, int bufferIndex, ayTexture* tTexture);

// buffer helper
void ay_generate_quad_grid(int cols, int rows, float* vertices, uint32_t* indices, bool addRandomColor, bool DEBUG);

// pipelines
void ay_bind_pipeline(ayGraphicsData*, ayPipeline*);

// draw calls
// clock wise vertacies
void ay_draw(ayGraphicsData* ptData, uint32_t uFirstVertex, uint32_t uIndexCount); 
// clock wise vertacies
void ay_draw_indexed(ayGraphicsData*, uint32_t uFirstIndex, uint32_t uIndexCount); 

void ay__draw_indexed(ayGraphicsData* ptData, uint32_t uFirstIndex, uint32_t uIndexCount, uint32_t uIndex);

//----------------------------shader helpers-----------------------------------

void*       ay_set_varying(ayVaryingType tType, ayVaryingData* ptVaryingDataOut);
const void* ay_get_varying(uint32_t uVaryingIndex, const ayVaryingData* ptVaryingDataOut);

const void* ay_get_vertex_attrib(const void* pcVertexDataIn, ayVertexLayout tLayout, uint32_t tAttribLocation);

#endif