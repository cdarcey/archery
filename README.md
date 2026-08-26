# Archery

A software rasterizer built from scratch to learn graphics programming fundamentals.

## Overview

Archery is a CPU based 3D renderer implementing the full graphics pipeline without GPU acceleration. Started as a learning project, it has evolved into a functional prototype exploring low level rendering techniques.

## Features

- Software rasterization with programmable vertex and pixel shaders
- Multi-threaded tile based rendering via a persistent worker pool (8 threads by default)
- Triangle binning for efficient culling
- Optional internal resolution rendering with a threaded upscale pass (nearest, bilinear, or bicubic filtering)
- Depth testing and backface culling
- Custom vertex attribute layouts

## Status

Functional prototype. The core rendering pipeline works but the API and performance characteristics are still evolving.

## Platform Support

- Windows (Win32) - Primary platform
- Linux support is very spotty on any given build

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

// render resolution: what the rasterizer actually draws into
#define screenWidth  640
#define screenHeight 360

// output/window resolution: what the upscale pass targets
#define outputWidth  1280
#define outputHeight 720

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

    // tile rendering and upscaling are both opt-in via the create info struct
    ayCreateGraphicsInfo tGraphicsInfo = {
        .uScreenWidth   = screenWidth,
        .uScreenHeight  = screenHeight,
        .bTileRendering = true,
        .bUpscale       = true,
        .tUpscaleSettings = {
            .uOutputWidth  = outputWidth,
            .uOutputHeight = outputHeight,
            .tFilter       = AY_UPSCALE_FILTER_BILINEAR
        }
    };
    ayGraphicsData* ptData = ay_initialize_graphics(&tGraphicsInfo);

    // framebuffer stays at render resolution, the window is the real output size
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, true);
    ayWindow* ptWindow = ay_create_window(outputWidth, outputHeight, "Archery Example");
    
    ayPipeline quadPipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader = color_gradient_pixel_shader,
        .tVertexShader = color_gradient_vertex_shader,
        .tLayout = {
            .tAttribType = {AY_VERTEX_ATTRIBUTE_TYPE_VEC3, AY_VERTEX_ATTRIBUTE_TYPE_VEC3},
            .szAttribOffset = {0, sizeof(float) * 3},
            .szVertexStride = sizeof(float) * 6,
            .uVertexCount = 4
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
        
        // tiling is picked up automatically from bTileRendering on the create info,
        // no separate "_tiled" entry point needed
        ay_draw_indexed(ptData, 0, 6);
        
        // upscale pass (render res -> output res) also happens automatically here
        // since bUpscale was set on the create info
        ay_present_frame(ptData, ptWindow);
    }
    
    ay_destroy_window(ptWindow);
    ay_destroy_graphics(&ptData);
    free(ptFrameBuffer->pfDepthBuffer);
    free(ptFrameBuffer->auData);
    free(ptFrameBuffer);
    
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
5. If upscaling is enabled, the native framebuffer is sampled (nearest/bilinear/bicubic) into a separate output resolution buffer before present

### Threading Model

Uses Win32 threading primitives with a persistent worker pool created once at graphics init (rather than spawned per draw call). Both tiled rendering and upscaling dispatch work to the same pool:

- **Tile rendering** uses atomic work-stealing: each worker atomically fetches the next unclaimed tile index, renders all triangles overlapping that tile, copies the result to the main framebuffer under a critical section, and repeats until all tiles are claimed. Since triangle density varies per tile, this keeps work balanced across threads regardless of scene distribution.
- **Upscaling** uses a static split: each worker is assigned a fixed, disjoint band of output rows up front. Since every output pixel costs the same fixed amount of work, no dynamic claiming is needed and no locking is required between workers.

## License

MIT License - Use freely for any purpose.

## Acknowledgments

https://github.com/hoffstadt & https://github.com/PilotLightTech/pilotlight

Built by following graphics programming fundamentals and studying production renderer architectures.