#include "ay_helpers.h"
#include <stdlib.h> // Required for RAND_MAX
#include <stdio.h> // printf

ayVertexBuffer
ayNewVertexBuffer(int iCapacity)
{
    // TODO: should this have minimum of 1 quad to avoid automitc increase of buffer on first use
    if(iCapacity <= 0) // make sure input is usable number
    {
        // TODO: handle error here
       return (ayVertexBuffer){0};
    }
   
    // TODO: fix the double malloc
    // allocate memory for vertex buffer
    ayVertexBuffer newBuffer = {
        .fBuffer = malloc(sizeof(float) * iCapacity),
        .iCapacity = iCapacity,
        .iSize = 0
    };
   
    // check for successful allocation
    if(newBuffer.fBuffer == NULL)
    {
        printf("Allocation failed\n");
        return newBuffer;
    }
    else return newBuffer;
}

ayIndexBuffer
ayNewIndexBuffer(int iCapacity)
{
    // TODO: should this have minimum of 1 quad to avoid automitc increase of buffer on first use
    if(iCapacity <= 0) // make sure input is usable number
    {
        printf("Invalid input\n");
       return (ayIndexBuffer){0};
    }

    if(iCapacity < 6)
    {
        iCapacity = 6;
    }
   
    // allocate memory for vertex buffer
    ayIndexBuffer newBuffer = {
        .uBuffer = malloc(sizeof(float) * iCapacity),
        .iCapacity = iCapacity,
        .iSize = 0
    };
   
    // check for successful allocation
    if(newBuffer.uBuffer == NULL)
    {
        printf("Invalid input\n");
        return newBuffer;
    }
    else return newBuffer;
}

ayVertexBuffer
ayIncreaseVertexBufferCapacity(ayVertexBuffer* vertBuffer)
{
    // calculate new capacity
    int newCapacity = vertBuffer->iCapacity * 2;
   
    // create new buffer with larger capacity
    ayVertexBuffer tmpVertBuffer = ayNewVertexBuffer(newCapacity);
    memset(tmpVertBuffer.fBuffer, 0, sizeof(float) * newCapacity);
   
    // copy data from old buffer to new larger buffer
    memcpy(tmpVertBuffer.fBuffer, vertBuffer->fBuffer, sizeof(float) * vertBuffer->iSize);
    tmpVertBuffer.iSize = vertBuffer->iSize;

    // free old buffer
    free(vertBuffer->fBuffer);
    return tmpVertBuffer;
}

ayIndexBuffer
ayIncreaseIndexBufferCapacity(ayIndexBuffer* indexBuffer)
{
    // calculate new capacity
    int newCapacity = indexBuffer->iCapacity * 2;
   
    // create new buffer with larger capacity
    ayIndexBuffer tmpIndexBuffer = ayNewIndexBuffer(newCapacity);
    memset(tmpIndexBuffer.uBuffer, 0, sizeof(float) * newCapacity);
   
    // copy data from old buffer to new larger buffer
    memcpy(tmpIndexBuffer.uBuffer, indexBuffer->uBuffer, sizeof(float) * indexBuffer->iSize);
    tmpIndexBuffer.iSize = indexBuffer->iSize;

    // free old buffer
    free(indexBuffer->uBuffer);
    return tmpIndexBuffer;
}

ayVec2 transform_grid_to_isometric_view(float x, float y) 
{
    ayVec2 result;
    result.x = x - y;
    result.y = (x + y) / 2;
    return result;
}

void ay_generate_quad_grid(int iCols, int iRows, float* vertexBuffer, uint32_t* indexBuffer, bool bAddRandomColor) 
{
    // Pre-calculate all column positions (x coordinates)
    float* columnPositions = (float*)malloc((iCols + 1) * sizeof(float));
    for (int x = 0; x <= iCols; x++) {
        columnPositions[x] = -1.0f + (2.0f * x) / iCols;
    }

    // Pre-calculate all row positions (y coordinates)
    float* rowPositions = (float*)malloc((iRows + 1) * sizeof(float));
    for (int y = 0; y <= iRows; y++) {
        rowPositions[y] = -1.0f + (2.0f * y) / iRows;
    }

    if (bAddRandomColor) {
        srand((unsigned int)time(NULL));
    }

    for (int y = 0; y < iRows; y++) {
        for (int x = 0; x < iCols; x++) {
            int iQuadIndex = y * iCols + x;
            int iStartingVertexindex = iQuadIndex * 32; // 4 vertices × 8 components

            // Get pre-calculated positions
            float left = columnPositions[x];
            float right = columnPositions[x + 1];
            float top = rowPositions[y];
            float bottom = rowPositions[y + 1];

            // Generate random colors if enabled
            float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
            if (bAddRandomColor) {
                r = (float)rand() / (float)RAND_MAX;
                g = (float)rand() / (float)RAND_MAX;
                b = (float)rand() / (float)RAND_MAX;
            }

            // Top left (x, y, u, v, r, g, b, a)
            vertexBuffer[iStartingVertexindex + 0] = left;
            vertexBuffer[iStartingVertexindex + 1] = top;
            vertexBuffer[iStartingVertexindex + 2] = 0.0f;
            vertexBuffer[iStartingVertexindex + 3] = 1.0f;
            vertexBuffer[iStartingVertexindex + 4] = r;
            vertexBuffer[iStartingVertexindex + 5] = g;
            vertexBuffer[iStartingVertexindex + 6] = b;
            vertexBuffer[iStartingVertexindex + 7] = a;

            // Top right
            vertexBuffer[iStartingVertexindex + 8] = right;
            vertexBuffer[iStartingVertexindex + 9] = top;
            vertexBuffer[iStartingVertexindex + 10] = 1.0f;
            vertexBuffer[iStartingVertexindex + 11] = 1.0f;
            vertexBuffer[iStartingVertexindex + 12] = r;
            vertexBuffer[iStartingVertexindex + 13] = g;
            vertexBuffer[iStartingVertexindex + 14] = b;
            vertexBuffer[iStartingVertexindex + 15] = a;

            // Bottom left
            vertexBuffer[iStartingVertexindex + 16] = left;
            vertexBuffer[iStartingVertexindex + 17] = bottom;
            vertexBuffer[iStartingVertexindex + 18] = 0.0f;
            vertexBuffer[iStartingVertexindex + 19] = 0.0f;
            vertexBuffer[iStartingVertexindex + 20] = r;
            vertexBuffer[iStartingVertexindex + 21] = g;
            vertexBuffer[iStartingVertexindex + 22] = b;
            vertexBuffer[iStartingVertexindex + 23] = a;

            // Bottom right
            vertexBuffer[iStartingVertexindex + 24] = right;
            vertexBuffer[iStartingVertexindex + 25] = bottom;
            vertexBuffer[iStartingVertexindex + 26] = 1.0f;
            vertexBuffer[iStartingVertexindex + 27] = 0.0f;
            vertexBuffer[iStartingVertexindex + 28] = r;
            vertexBuffer[iStartingVertexindex + 29] = g;
            vertexBuffer[iStartingVertexindex + 30] = b;
            vertexBuffer[iStartingVertexindex + 31] = a;
        }
    }
    
    // Generate indices for index buffer
    for (int i = 0; i < iCols * iRows; i++) {
        int iCurrentQuad = i * 4;
        int iTriangleStartIndex = i * 6;
        
        // first traingle
        indexBuffer[iTriangleStartIndex + 0] = iCurrentQuad + 0;
        indexBuffer[iTriangleStartIndex + 1] = iCurrentQuad + 1;
        indexBuffer[iTriangleStartIndex + 2] = iCurrentQuad + 2;
        
        // second triangle
        indexBuffer[iTriangleStartIndex + 3] = iCurrentQuad + 1;
        indexBuffer[iTriangleStartIndex + 4] = iCurrentQuad + 3;
        indexBuffer[iTriangleStartIndex + 5] = iCurrentQuad + 2;
    }

    free(rowPositions);
    free(columnPositions);
}
