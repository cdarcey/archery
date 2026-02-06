#include <stdio.h>
#include <stdlib.h>

#include "ay_rasterize.h"

#define screenWidth  1280
#define screenHeight 720

ayVec4 color_gradient_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn);
ayVec3 color_gradient_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut);

int main()
{
    float vertices[] = {
        // position          // color
        -1.0f, -1.0f, 0.5f,  1.0f, 0.0f, 0.0f,  // top left - red
         1.0f, -1.0f, 0.5f,  0.0f, 0.0f, 1.0f,  // top right blue
        -1.0f,  1.0f, 0.5f,  0.0f, 1.0f, 0.0f,  // bottom left green
         1.0f,  1.0f, 0.5f,  1.0f, 0.0f, 0.0f   // bottom right red
    };
    
    uint32_t indices[] = {0, 1, 2, 2, 1, 3};
    
    ayGraphicsData* ptData = initialize_graphics(screenWidth, screenHeight);
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, true);
    ayWindow* ptWindow = ay_create_window(screenWidth, screenHeight, "RGB Triangle");
    
    ayPipeline tPipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader = color_gradient_pixel_shader,
        .tVertexShader = color_gradient_vertex_shader,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3,  // position
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3   // color
            },
            .szAttribOffset = {
                0,                    // position offset
                sizeof(float) * 3     // color offset
            },
            .szVertexStride = sizeof(float) * 6,  // 3 pos + 3 color
        }
    };
    
    // identity matrix (no transformation)
    ayMat4 identity = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    while(!ay_window_should_close(ptWindow)) 
    {
        glfwPollEvents();
        
        ay_clear_frame_buffer(ptFrameBuffer);
        
        ay_bind_frame_buffer(ptData, ptFrameBuffer);
        ay_bind_vertex_buffer(ptData, vertices);
        ay_bind_index_buffer(ptData, indices);
        ay_bind_descriptor(ptData, 0, AY_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &identity);
        ay_bind_pipeline(ptData, &tPipeline);
        
        // ay_test_draw_tile(ptData, 0, 6);
        ay_draw_indexed(ptData, 0, 6);

        // ay_output_frame_buffer(ptFrameBuffer);
        ay_present_frame(ptWindow, ptFrameBuffer);
    }
    
    // ay_destroy_window(ptWindow);
    // free(ptFrameBuffer->pfDepthBuffer);
    // free(ptFrameBuffer->pucData);
    // free(ptFrameBuffer);
    // free(ptData);
    
    return 0;
}

ayVec4 color_gradient_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn)
{
    // get interpolated color from varying
    const ayVec3* color = ay_get_varying(0, ptVaryingDataIn);
    
    // convert from [0,1] to [0,255]
    return (ayVec4){
        color->x * 255.0f, 
        color->y * 255.0f, 
        color->z * 255.0f, 
        255
    };
}

ayVec3 color_gradient_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut)
{
    // get position and color from vertex data
    ayVec3 position = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, tBuiltIns.tLayout, 0);
    ayVec3 color = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, tBuiltIns.tLayout, 1);
    
    // pass color through to pixel shader as varying
    ayVec3* pColorOut = ay_set_varying(AY_VARYING_TYPE_VEC3, ptVaryingDataOut);
    *pColorOut = color;
    
    // transform position
    ayMat4* pMVP = (ayMat4*)tDescriptor[0].pData;
    ayVec4 pos = {position.x, position.y, position.z, 1.0f};
    pos = ay_mat4_mul_vec4(*pMVP, pos);
    
    return (ayVec3){pos.x / pos.w, pos.y / pos.w, pos.z / pos.w};
}