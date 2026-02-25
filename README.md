# Archery

A software rasterizer built from scratch to learn graphics programming fundamentals.

## Overview

Archery is a CPU-based 3D renderer implementing the full graphics pipeline without GPU acceleration. Started as a learning project, it has evolved into a functional prototype exploring low-level rendering techniques.

## Features

- Software rasterization with programmable vertex and pixel shaders
- Multi-threaded tile-based rendering (8 threads by default)
- Triangle binning for efficient culling
- Depth testing and backface culling
- Perspective-correct interpolation
- Custom vertex attribute layouts

## Status

Functional prototype. The core rendering pipeline works but the API and performance characteristics are still evolving.

## Platform Support

- Windows (Win32) - Primary platform
- Linux/macOS - Planned

## Dependencies

- GLFW - Window management and input
- stb_image - Image loading

## Quick Start

Build instructions coming soon. Currently uses custom batch scripts.

### Basic Usage
```c
#include <stdio.h>
#include <stdlib.h>
#include "ay_rasterize.h"

#define screenWidth  1280
#define screenHeight 720

ayVec4 color_gradient_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn);
ayVec3 color_gradient_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut);

int main()
{
    float quad_vertices[] = {
        -1.0f, -1.0f, 0.5f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 0.5f,  0.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.5f,  1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.5f,  0.0f, 1.0f, 0.0f
    };
    uint32_t quad_indices[] = {0, 1, 2, 2, 3, 0};

    ayGraphicsData* ptData = initialize_graphics(screenWidth, screenHeight);
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, true);
    ayWindow* ptWindow = ay_create_window(screenWidth, screenHeight, "Archery Example");
    
    ayPipeline quadPipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader = color_gradient_pixel_shader,
        .tVertexShader = color_gradient_vertex_shader,
        .tLayout = {
            .tAttribType = {AY_VERTEX_ATTRIBUTE_TYPE_VEC3, AY_VERTEX_ATTRIBUTE_TYPE_VEC3},
            .szAttribOffset = {0, sizeof(float) * 3},
            .szVertexStride = sizeof(float) * 6,
        }
    };
    
    ayMat4 identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    
    double dLastFPSTime = glfwGetTime();
    double dLastFrameTime = glfwGetTime();
    int iFrameCount = 0;
    
    while(!ay_window_should_close(ptWindow)) 
    {
        glfwPollEvents();
        
        double dCurrentTime = glfwGetTime();
        dLastFrameTime = dCurrentTime;
        iFrameCount++;
        
        if(dCurrentTime - dLastFPSTime >= 1.0) {
            double fps = iFrameCount / (dCurrentTime - dLastFPSTime);
            char title[256];
            sprintf(title, "Archery Example | FPS: %.1f (%.2f ms)", fps, 1000.0 / fps);
            glfwSetWindowTitle(ptWindow->pWindow, title);
            iFrameCount = 0;
            dLastFPSTime = dCurrentTime;
        }
        
        ay_clear_frame_buffer(ptFrameBuffer);
        ay_bind_frame_buffer(ptData, ptFrameBuffer);
        ay_bind_vertex_buffer(ptData, quad_vertices);
        ay_bind_index_buffer(ptData, quad_indices);
        ay_bind_descriptor(ptData, 0, AY_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &identity);
        ay_bind_pipeline(ptData, &quadPipeline);
        
        ay_draw_indexed_tiled(ptData, 0, 6);
        
        ay_present_frame(ptWindow, ptFrameBuffer);
    }
    
    ay_destroy_window(ptWindow);
    free(ptFrameBuffer->pfDepthBuffer);
    free(ptFrameBuffer->auData);
    free(ptFrameBuffer);
    free(ptData);
    
    return 0;
}

ayVec4 color_gradient_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec3* color = ay_get_varying(0, ptVaryingDataIn);
    return (ayVec4){color->x * 255.0f, color->y * 255.0f, color->z * 255.0f, 255};
}

ayVec3 color_gradient_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut)
{
    ayVec3 position = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, tBuiltIns.tLayout, 0);
    ayVec3 color = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, tBuiltIns.tLayout, 1);
    
    ayVec3* pColorOut = ay_set_varying(AY_VARYING_TYPE_VEC3, ptVaryingDataOut);
    *pColorOut = color;
    
    ayMat4* pMVP = (ayMat4*)tDescriptor[0].pData;
    ayVec4 pos = {position.x, position.y, position.z, 1.0f};
    pos = ay_mat4_mul_vec4(*pMVP, pos);
    
    return (ayVec3){pos.x / pos.w, pos.y / pos.w, pos.z / pos.w};
}
```

## Architecture

### Rendering Pipeline

1. Vertex shader transforms positions to NDC space
2. Triangle binning determines which screen tiles each triangle overlaps
3. Worker threads render tiles in parallel using local buffers
4. Tiles are copied to the main framebuffer with critical section protection

### Threading Model

Uses Win32 threading primitives with atomic work-stealing. Each worker thread:
- Atomically fetches the next tile index
- Renders all triangles overlapping that tile
- Copies result to main framebuffer under lock
- Repeats until all tiles complete

## License

MIT License - Use freely for any purpose.

## Acknowledgments

Built by following graphics programming fundamentals and studying production renderer architectures.
