#include "ay_rasterize.h"
#include "ay_helpers.h"
#include <stdio.h>
#include <stdlib.h>

#define screenWidth  750
#define screenHeight 750

ayVec4
ayPixelShader_Colors(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec4* ptColor = ay_get_varying(0, ptVaryingDataIn);
    return (ayVec4){ptColor->r * 255, ptColor->g * 255, ptColor->b * 255, 1.f * 255};
}

ayVec3 
ayVertexShader_Colors(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptorInfo* tInfo, ayVaryingData* ptVaryingDataOut) 
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;

    // get vertex attributes (inputs)
    ayVec3 tPos = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 0); 
    ayVec4 tColor = *(ayVec4*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 1);

    // set varyings (outputs)
    ayVec4* ptColor = ay_set_varying(AY_VARYING_TYPE_VEC4, ptVaryingDataOut);  
    *ptColor = tColor;

    return tPos;
}

int main()
{
    ayGraphicsData* ptData = initialize_graphics();
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, true); // depth enabled
    ayWindow* ptTestWindow = ay_create_window(screenWidth, screenHeight, "Depth Test");
    ay_clear_frame_buffer(ptFrameBuffer);

    // two overlapping triangles to test depth
    // format: x, y, z, r, g, b, a
    float atVertexBuffer[] = {
        // triangle 1 (red) - closer (z = 0.3)
        -0.5f,  0.5f, 0.3f,  1.0f, 0.0f, 0.0f, 1.0f,  // top left
         0.5f,  0.5f, 0.3f,  1.0f, 0.0f, 0.0f, 1.0f,  // top right
         0.0f, -0.3f, 0.3f,  1.0f, 0.0f, 0.0f, 1.0f,  // bottom center

        // triangle 2 (blue) - farther (z = 0.7)
         0.0f,  0.4f, 0.7f,  0.0f, 0.0f, 1.0f, 1.0f,  // top center
         0.8f, -0.5f, 0.7f,  0.0f, 0.0f, 1.0f, 1.0f,  // bottom right
        -0.8f, -0.5f, 0.7f,  0.0f, 0.0f, 1.0f, 1.0f,  // bottom left
    };

    uint32_t atIndexBuffer[] = {
        0, 1, 2,  // red triangle
        3, 4, 5   // blue triangle
    };

    ayPipeline tPipelineColors = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader = ayPixelShader_Colors,
        .tVertexShader = ayVertexShader_Colors,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3,  // position (x,y,z)
                AY_VERTEX_ATTRIBUTE_TYPE_VEC4   // color (r,g,b,a)
            },
            .szAttribOffset = {
                0,
                sizeof(ayVec3)
            },
            .szVertexStride = sizeof(float) * 7,  // 3 pos + 4 color
        }
    };

    double lastTime = glfwGetTime();
    int frameCount = 0;

    while(!ay_window_should_close(ptTestWindow)) 
    {
        double currentTime = glfwGetTime();
        frameCount++;
        
        // update fps every second
        if(currentTime - lastTime >= 1.0) 
        {
            double fps = frameCount / (currentTime - lastTime);
            double ms = 1000.0 / fps;
            
            char title[256];
            sprintf(title, "Depth Test | FPS: %.1f (%.2f ms)", fps, ms);
            glfwSetWindowTitle(ptTestWindow->pWindow, title);
            
            frameCount = 0;
            lastTime = currentTime;
        }
        
        glfwPollEvents();

        // clear buffers each frame
        ay_clear_frame_buffer(ptFrameBuffer);

        ay_bind_frame_buffer(ptData, ptFrameBuffer);
        ay_bind_vertex_buffer(ptData, atVertexBuffer);
        ay_bind_index_buffer(ptData, atIndexBuffer);

        // draw both triangles
        ay_bind_pipeline(ptData, &tPipelineColors);
        ay_draw_indexed(ptData, 0, 6);

        // present to window
        ay_present_frame(ptTestWindow, ptFrameBuffer);
        ay_output_frame_buffer(ptFrameBuffer);
    }

    // clean up
    ay_destroy_window(ptTestWindow);
    free(ptFrameBuffer->pfDepthBuffer);
    free(ptFrameBuffer->pucData);
    free(ptFrameBuffer);
    free(ptData);

    return 0;
}