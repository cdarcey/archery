
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ay_rasterize.h"
#define AY_RASTERIZE_PROFILE_ENABLED 
#include "ay_rasterize_profile.h"

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c.h"


#define screenWidth  1280
#define screenHeight 720

ayVec4 ayPixelShader_test(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn);
ayVec3 ayVertexShader_test(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut);
void   get_file_data(void* ctx, const char* filename, const int is_mtl, const char* obj_filename, char** data, size_t* len);

int main()
{
    tinyobj_attrib_t attrib;
    tinyobj_shape_t* shapes = NULL;
    size_t num_shapes;
    tinyobj_material_t* materials = NULL;
    size_t num_materials;

    int ret = tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials, &num_materials, 
                             "../data/bunny2.obj", get_file_data, NULL, TINYOBJ_FLAG_TRIANGULATE);
    if (ret != TINYOBJ_SUCCESS) {
        printf("Failed to load OBJ\n");
        return;
    }

    int num_vertices = attrib.num_vertices;
    int num_indices = attrib.num_faces;

    float* vertices = malloc(num_vertices * 5 * sizeof(float));
    uint32_t* indices = malloc(num_indices * sizeof(uint32_t));

    for(int i = 0; i < num_vertices; i++) {
        vertices[i * 5 + 0] = attrib.vertices[i * 3 + 0];  // x
        vertices[i * 5 + 1] = attrib.vertices[i * 3 + 1];  // y
        vertices[i * 5 + 2] = attrib.vertices[i * 3 + 2];  // z
        vertices[i * 5 + 3] = 0.0f;  // u (dummy)
        vertices[i * 5 + 4] = 0.0f;  // v (dummy)
    }
    // copy indices
    for(int i = 0; i < num_indices; i++) {
        indices[i] = attrib.faces[shapes[0].face_offset + i].v_idx;
    }
    
    ayGraphicsData* ptData = initialize_graphics();
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, true); // depth enabled
    ayWindow* ptTestWindow = ay_create_window(screenWidth, screenHeight, "Test Window");
    ay_clear_frame_buffer(ptFrameBuffer);

    ayPipeline tPipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader = ayPixelShader_test,
        .tVertexShader = ayVertexShader_test,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3,  // position (x,y,z)
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2   // uv
            },
            .szAttribOffset = {
                0,
                sizeof(ayVec3)
            },
            .szVertexStride = sizeof(float) * 5,  // 3 pos + 2 uv
        }
    };

    // create MVP matrix
    ayMat4 projection = ay_mat4_perspective(
        60.0f * 3.14159f / 180.0f,
        (float)screenWidth / screenHeight,
        0.1f,
        100.0f
    );

    double dLastFPSTime = glfwGetTime();
    double dLastFrameTime = glfwGetTime();
    int    iFrameCount = 0;
    float  fRotation = 0.0f;

    while(!ay_window_should_close(ptTestWindow)) 
    {
        glfwPollEvents();

        if(g_raster_profiler.uCurrentFrame >= 120) break;


        double dCurrentTime = glfwGetTime();
        float fDeltaTime = (float)(dCurrentTime - dLastFrameTime);
        dLastFrameTime = dCurrentTime;
        iFrameCount++;
        
        // update fps every second
        if(dCurrentTime - dLastFPSTime >= 1.0) 
        {
            double fps = iFrameCount / (dCurrentTime - dLastFPSTime);
            double ms = 1000.0 / fps;
            
            char title[256];
            sprintf(title, "Spinning Cube | FPS: %.1f (%.2f ms)", fps, ms);
            glfwSetWindowTitle(ptTestWindow->pWindow, title);
            
            iFrameCount = 0;
            dLastFPSTime = dCurrentTime;
        }
        fRotation += fDeltaTime * 1.0f;

        PROFILE_START(FrameTime);
        // scale - rotate - translate
        ayMat4 scale = ay_mat4_scale(10.0f, 10.0f, 10.0f);
        ayMat4 rotation = ay_mat4_rotate_y(fRotation);
        ayMat4 translation = ay_mat4_translate(0.0f, 1.0f, -3.0f); // TODO: if you use "-2.0f" for z app crashed 

        ayMat4 model = ay_mat4_multiply(rotation, scale); // scale, then rotate
        model = ay_mat4_multiply(translation, model); // translate away from camera

        ayMat4 mvp = ay_mat4_multiply(projection, model);
    
        // clear buffers each frame
        PROFILE_START(ClearFrame);
        ay_clear_frame_buffer(ptFrameBuffer);
        PROFILE_END(ClearFrame);

        // set bindingings
        ay_bind_frame_buffer(ptData, ptFrameBuffer);
        ay_bind_vertex_buffer(ptData, vertices);
        ay_bind_index_buffer(ptData, indices);
        ay_bind_descriptor(ptData, 0, AY_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &mvp);
        ay_bind_pipeline(ptData, &tPipeline);
        // ay_bind_descriptor(ptData, 1, AY_DESCRIPTOR_TYPE_TEXTURE, &testTexture);

        // draw call
        ay_draw_indexed(ptData, 0, num_indices); // 4968 triangles

        // present to window
        ay_present_frame(ptTestWindow, ptFrameBuffer); 

        PROFILE_END(FrameTime);
        g_raster_profiler.uCurrentFrame++;
    }

    // profiler results
    for(uint32_t i = 0; i < 120; i++)
    {
        printf("FT:%.2lfms - CF:%.2lfms - PL:%.2lfms\n", 
            g_raster_profiler.dFrameTime[i],
            g_raster_profiler.dClearFrame[i],
            g_raster_profiler.dPixelLoop[i]);
    }


    // clean up
    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);
    free(vertices);
    free(indices);

    
    ay_destroy_window(ptTestWindow);
    free(ptFrameBuffer->pfDepthBuffer);
    free(ptFrameBuffer->pucData);
    free(ptFrameBuffer);
    free(ptData);

    return 0;
}

ayVec4
ayPixelShader_test(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn)
{
    // const ayVec2* ptUV = ay_get_varying(0, ptVaryingDataIn);
    
    // cast descriptor at binding 0 to texture
    // ayTexture* pTexture = (ayTexture*)tDescriptor[1].pData;
    
    // ayVec4 tColor = ay_sample_texture(*pTexture, *ptUV, 4);

    const float* fDepth = ay_get_varying(1, ptVaryingDataIn);

    return (ayVec4){*fDepth * 255, *fDepth * 255, *fDepth * 255, 255};
}

ayVec3 
ayVertexShader_test(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut) 
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;

    ayVec3 tPos = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 0); 
    ayVec2 tUV = *(ayVec2*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 1);

    // get mvp matrix from descriptor
    ayMat4* pMVP = (ayMat4*)tDescriptor[0].pData;
    
    // transform position
    ayVec4 pos = {tPos.x, (tPos.y * -1.0f), tPos.z, 1.0f}; //flip y
    pos = ay_mat4_mul_vec4(*pMVP, pos);
    ayVec3 tResult = (ayVec3){pos.x / pos.w, pos.y / pos.w, pos.z / pos.w};
    
    ayVec2* ptUV = ay_set_varying(AY_VARYING_TYPE_VEC2, ptVaryingDataOut);  
    *ptUV = tUV;

    float* pDepth = ay_set_varying(AY_VARYING_TYPE_FLOAT, ptVaryingDataOut);
    *pDepth = tResult.z;

    return tResult;
}

void get_file_data(void* ctx, const char* filename, const int is_mtl,
                   const char* obj_filename, char** data, size_t* len) 
{
    FILE* f = fopen(filename, "rb");
    if (!f) 
    {
        *data = NULL;
        *len = 0;
        return;
    }
    fseek(f, 0, SEEK_END);
    *len = ftell(f);
    fseek(f, 0, SEEK_SET);
    *data = malloc(*len);
    fread(*data, 1, *len, f);
    fclose(f);
}