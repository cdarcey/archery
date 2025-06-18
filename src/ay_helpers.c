#include "ay_helpers.h"
#include <stdlib.h> // Required for RAND_MAX
#include <stdio.h> // printf

ayVec2 transform_grid_to_isometric_view(float x, float y) 
{
    ayVec2 result;
    result.x = x - y;
    result.y = (x + y) / 2;
    return result;
}

void ay_generate_quad_grid(int iCols, int iRows, float* vertexBuffer, uint32_t* indexBuffer, bool bAddRandomColor, bool bDEBUG) 
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

            // Calculate normalized UV coordinates
            float u0 = (float)x / iCols;
            float u1 = (float)(x + 1) / iCols;
            float v0 = (float)y / iRows;
            float v1 = (float)(y + 1) / iRows;

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
            vertexBuffer[iStartingVertexindex + 2] = u0;
            vertexBuffer[iStartingVertexindex + 3] = v0;
            vertexBuffer[iStartingVertexindex + 4] = r;
            vertexBuffer[iStartingVertexindex + 5] = g;
            vertexBuffer[iStartingVertexindex + 6] = b;
            vertexBuffer[iStartingVertexindex + 7] = a;

            // Top right
            vertexBuffer[iStartingVertexindex + 8] = right;
            vertexBuffer[iStartingVertexindex + 9] = top;
            vertexBuffer[iStartingVertexindex + 10] = u1;
            vertexBuffer[iStartingVertexindex + 11] = v0;
            vertexBuffer[iStartingVertexindex + 12] = r;
            vertexBuffer[iStartingVertexindex + 13] = g;
            vertexBuffer[iStartingVertexindex + 14] = b;
            vertexBuffer[iStartingVertexindex + 15] = a;

            // Bottom left
            vertexBuffer[iStartingVertexindex + 16] = left;
            vertexBuffer[iStartingVertexindex + 17] = bottom;
            vertexBuffer[iStartingVertexindex + 18] = u0;
            vertexBuffer[iStartingVertexindex + 19] = v1;
            vertexBuffer[iStartingVertexindex + 20] = r;
            vertexBuffer[iStartingVertexindex + 21] = g;
            vertexBuffer[iStartingVertexindex + 22] = b;
            vertexBuffer[iStartingVertexindex + 23] = a;

            // Bottom right
            vertexBuffer[iStartingVertexindex + 24] = right;
            vertexBuffer[iStartingVertexindex + 25] = bottom;
            vertexBuffer[iStartingVertexindex + 26] = u1;
            vertexBuffer[iStartingVertexindex + 27] = v1;
            vertexBuffer[iStartingVertexindex + 28] = r;
            vertexBuffer[iStartingVertexindex + 29] = g;
            vertexBuffer[iStartingVertexindex + 30] = b;
            vertexBuffer[iStartingVertexindex + 31] = a;
            
            if(bDEBUG)
            {
                // Debug print for each quad
                printf("Quad %d (Col %d, Row %d):\n", iQuadIndex, x, y);
                printf("  TL: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n", 
                       vertexBuffer[iStartingVertexindex],     vertexBuffer[iStartingVertexindex + 1],
                       vertexBuffer[iStartingVertexindex + 2], vertexBuffer[iStartingVertexindex + 3],
                       vertexBuffer[iStartingVertexindex + 4], vertexBuffer[iStartingVertexindex + 5],
                       vertexBuffer[iStartingVertexindex + 6], vertexBuffer[iStartingVertexindex + 7]);
                printf("  TR: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n", 
                       vertexBuffer[iStartingVertexindex + 8],  vertexBuffer[iStartingVertexindex + 9],
                       vertexBuffer[iStartingVertexindex + 10], vertexBuffer[iStartingVertexindex + 11],
                       vertexBuffer[iStartingVertexindex + 12], vertexBuffer[iStartingVertexindex + 13],
                       vertexBuffer[iStartingVertexindex + 14], vertexBuffer[iStartingVertexindex + 15]);
                printf("  BL: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n", 
                       vertexBuffer[iStartingVertexindex + 16], vertexBuffer[iStartingVertexindex + 17],
                       vertexBuffer[iStartingVertexindex + 18], vertexBuffer[iStartingVertexindex + 19],
                       vertexBuffer[iStartingVertexindex + 20], vertexBuffer[iStartingVertexindex + 21],
                       vertexBuffer[iStartingVertexindex + 22], vertexBuffer[iStartingVertexindex + 23]);
                printf("  BR: (%.2f, %.2f) UV(%.2f, %.2f) RGBA(%.2f, %.2f, %.2f, %.2f)\n\n", 
                       vertexBuffer[iStartingVertexindex + 24], vertexBuffer[iStartingVertexindex + 25],
                       vertexBuffer[iStartingVertexindex + 26], vertexBuffer[iStartingVertexindex + 27],
                       vertexBuffer[iStartingVertexindex + 28], vertexBuffer[iStartingVertexindex + 29],
                       vertexBuffer[iStartingVertexindex + 30], vertexBuffer[iStartingVertexindex + 31]);
            }
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
