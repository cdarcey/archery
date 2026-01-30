

/*
    TODO. add tests for all major rasterization realted functions
        . create known output copies for comparisons 
        .
*/

#include "ay_rasterize.h"
#include "ay_helpers.h"


#define screenWidth  500
#define screenHeight 500

ayVec4
draw_function_test_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec4* ptColor = ay_get_varying(0, ptVaryingDataIn);
    const ayVec2* ptUV = ay_get_varying(1, ptVaryingDataIn); // not used currently 

    return (ayVec4){ptColor->r * 255, ptColor->g * 255, ptColor->b * 255, 1.f * 255};
}

ayVec3 
draw_function_test_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptorInfo* tInfo, ayVaryingData* ptVaryingDataOut) 
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;

    // get vertex attributes (inputs)
    ayVec3 tPos = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 0); 
    ayVec4 tColor = *(ayVec4*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 2);

    // set varyings (outputs)
    ayVec4* ptColor = ay_set_varying(AY_VARYING_TYPE_VEC4, ptVaryingDataOut);  
    *ptColor = tColor;

    return tPos;
}


int main()
{
    ayGraphicsData* ptData = initialize_graphics();
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, true);
    ay_clear_frame_buffer(ptFrameBuffer);

    float afVertexBuffer[] = {
        // triangle 1 (bottom-left, bottom-right, top-right)

        // vertex 0 -> bottom-left (red)
        -0.75f, 0.75f, 0.5f,      // position (x, y, z)
        0.0f,  0.0f,             // uv (for adding textures to test later)
        1.0f, 0.0f, 0.0f, 1.0f,  // color: red

        // vertex 1 -> bottom-right (green)
        0.75f, 0.75f, 0.5f,      // position
        1.0f,  0.0f,             // uv
        0.0f, 1.0f, 0.0f, 1.0f,  // color: green

        // vertex 2 -> top-right (blue)
        0.75f, -0.75f, 0.5f,     // position
        1.0f,  1.0f,             // uv
        0.0f, 0.0f, 1.0f, 1.0f,  // color: blue

        // triangle 2 (bottom-left, top-right, top-left)

        // vertex 3 -> bottom-left (red) - DUPLICATE
        -0.75f, 0.75f, 0.5f,      // position
        0.0f,  0.0f,             // uv
        1.0f, 0.0f, 0.0f, 1.0f,  // color: red

        // vertex 4 -> top-right (blue) - DUPLICATE
        0.75f, -0.75f, 0.5f,     // position 
        1.0f,  1.0f,             // uv
        0.0f, 0.0f, 1.0f, 1.0f,  // color: blue

        // vertex 5 -> top-left (yellow)
        -0.75f, -0.75f, 0.5f,     // position 
        0.0f,  1.0f,             // uv
        1.0f, 1.0f, 0.0f, 1.0f   // color: yellow
    };

    ayPipeline tDrawFunctionTestPipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader = draw_function_test_pixel_shader,
        .tVertexShader = draw_function_test_vertex_shader,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3,  // position (x, y, z)
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2,  // uv
                AY_VERTEX_ATTRIBUTE_TYPE_VEC4   // color
            },
            .szAttribOffset = {
                0,
                sizeof(ayVec3),
                sizeof(ayVec3) + sizeof(ayVec2)
            },
            .szVertexStride = sizeof(float) * 9,  // 3 + 2 + 4
        }
    };

    ay_bind_frame_buffer(ptData, ptFrameBuffer);
    ay_bind_vertex_buffer(ptData, afVertexBuffer);
    ay_bind_pipeline(ptData, &tDrawFunctionTestPipeline);

    ay_draw(ptData, 0, 6);

    // output frame
    ay_output_frame_buffer(ptFrameBuffer);

    return 0;
}