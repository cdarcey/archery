// a file for side project functions or stuff not necessary to Archery develpoment 

#ifndef AY_HELPERS_H
#define AY_HELPERS_H

#include "ay_rasterize.h"


// Structs
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

// functions declerations
ayVec2 transform_grid_to_isometric_view(float x, float y); // not a good version of this
void ay_generate_quad_grid(int cols, int rows, float* vertices, uint32_t* indices, bool addRandomColor);

// call malloc and need to be freed when done
ayVertexBuffer ayNewVertexBuffer(int iSize);
ayIndexBuffer  ayNewIndexBuffer (int iCapacity);

// maybe should be an internal function to avoid data being added that doesnt fit design pattern
ayVertexBuffer ayIncreaseVertexBufferCapacity(ayVertexBuffer* vertBuffer);
ayIndexBuffer  ayIncreaseIndexBufferCapacity (ayIndexBuffer* indexBuffer);

#endif