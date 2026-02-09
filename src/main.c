#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define AY_RASTERIZE_PROFILE_ENABLED 
#include "ay_rasterize.h"
#include "ay_rasterize_profile.h"

#define screenWidth  1280
#define screenHeight 720

ayVec4 ayPixelShader_lit(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn);
ayVec3 ayVertexShader_lit(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut);

void generate_sphere(float** vertices, uint32_t** indices, int* vertex_count, int* index_count, int segments, int rings);

int main()
{
    float vertices[] = {
        // position (x, y, z)    // UV (not used)
        -1.0f, -1.0f, 0.5f,    0.0f, 0.0f,  // bottom left
         1.0f, -1.0f, 0.5f,    1.0f, 0.0f,  // bottom right
         1.0f,  1.0f, 0.5f,    1.0f, 1.0f,  // top right
        -1.0f,  1.0f, 0.5f,    0.0f, 1.0f   // top left
    };

    uint32_t indices[] = {
        0, 1, 2, 
        2, 3, 0 
    };

    int num_vertices = 4;
    int num_indices = 6;
    
    
    ayGraphicsData* ptData = initialize_graphics(screenWidth,screenHeight);
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, true);
    ayWindow* ptWindow = ay_create_window(screenWidth, screenHeight, "Sphere");
    
    ayPipeline tPipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader = ayPixelShader_lit,
        .tVertexShader = ayVertexShader_lit,
        .tLayout = {
            .tAttribType = {
                AY_VERTEX_ATTRIBUTE_TYPE_VEC3, 
                AY_VERTEX_ATTRIBUTE_TYPE_VEC2
            },
            .szAttribOffset = {
                0, 
                sizeof(ayVec3)
            },
            .szVertexStride = sizeof(float) * 5,
        }
    };
    
    ayMat4 projection = ay_mat4_perspective(60.0f * 3.14159f / 180.0f, (float)screenWidth / screenHeight, 0.1f, 100.0f);
    
    double dLastFPSTime = glfwGetTime();
    double dLastFrameTime = glfwGetTime();
    int iFrameCount = 0;
    float fRotation = 0.0f;

    
    while(!ay_window_should_close(ptWindow)) 
    {
        glfwPollEvents();
        
        double dCurrentTime = glfwGetTime();
        float fDeltaTime = (float)(dCurrentTime - dLastFrameTime);
        dLastFrameTime = dCurrentTime;
        iFrameCount++;
        
        if(dCurrentTime - dLastFPSTime >= 1.0) {
            double fps = iFrameCount / (dCurrentTime - dLastFPSTime);
            char title[256];
            sprintf(title, "Sphere | FPS: %.1f (%.2f ms)", fps, 1000.0 / fps);
            glfwSetWindowTitle(ptWindow->pWindow, title);
            iFrameCount = 0;
            dLastFPSTime = dCurrentTime;
        }
        fRotation += fDeltaTime;
        
        ayMat4 rotation = ay_mat4_rotate_y(fRotation);
        ayMat4 translation = ay_mat4_translate(0.0f, 0.0f, -3.0f);
        ayMat4 model = ay_mat4_multiply(translation, rotation);
        ayMat4 mvp = ay_mat4_multiply(projection, model);
        
        
        ay_clear_frame_buffer(ptFrameBuffer);
        
        ay_bind_frame_buffer(ptData, ptFrameBuffer);
        ay_bind_vertex_buffer(ptData, vertices);
        ay_bind_index_buffer(ptData, indices);
        ay_bind_descriptor(ptData, 0, AY_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &mvp);
        ay_bind_pipeline(ptData, &tPipeline);
        
        // ay_draw_indexed(ptData, 0, num_indices);
        ay_test_draw_tile(ptData, 0, num_indices);

        ay_present_frame(ptWindow, ptFrameBuffer);
        
    }
    
    
    ay_destroy_window(ptWindow);
    free(ptFrameBuffer->pfDepthBuffer);
    free(ptFrameBuffer->auData);
    free(ptFrameBuffer);
    free(ptData);
    free(vertices);
    free(indices);
    
    return 0;
}

ayVec4 ayPixelShader_lit(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec3* normal = ay_get_varying(0, ptVaryingDataIn);
    
    // directional light from top-right-front
    float light = normal->x * 0.4f + normal->y * 0.6f + normal->z * 0.5f;
    light = light * 0.5f + 0.5f;  // remap [-1,1] to [0,1]
    
    float gray = 60 + light * 190;
    return (ayVec4){255, 255, 255, 255};
}

ayVec3 ayVertexShader_lit(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut)
{
    ayVec3 tPos = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, tBuiltIns.tLayout, 0);
    ayMat4* pMVP = (ayMat4*)tDescriptor[0].pData;
    
    ayVec4 pos = {tPos.x, tPos.y, tPos.z, 1.0f};
    pos = ay_mat4_mul_vec4(*pMVP, pos);
    
    // pass normal (for unit sphere, position = normal)
    ayVec3* pNormal = ay_set_varying(AY_VARYING_TYPE_VEC3, ptVaryingDataOut);
    *pNormal = tPos;
    
    return (ayVec3){pos.x / pos.w, pos.y / pos.w, pos.z / pos.w};
}

void 
generate_sphere(float** vertices, uint32_t** indices, int* vertex_count, int* index_count, int segments, int rings)
{
    *vertex_count = (rings + 1) * (segments + 1);
    *index_count = rings * segments * 6;
    
    *vertices = malloc(*vertex_count * 5 * sizeof(float));
    *indices = malloc(*index_count * sizeof(uint32_t));
    
    int v_idx = 0;
    for(int ring = 0; ring <= rings; ring++) 
    {
        float phi = 3.14159f * ring / rings;
        for(int seg = 0; seg <= segments; seg++) 
        {
            float theta = 2.0f * 3.14159f * seg / segments;
            
            float x = sinf(phi) * cosf(theta);
            float y = cosf(phi);
            float z = sinf(phi) * sinf(theta);
            
            (*vertices)[v_idx++] = x;
            (*vertices)[v_idx++] = y;
            (*vertices)[v_idx++] = z;
            (*vertices)[v_idx++] = 0.0f;
            (*vertices)[v_idx++] = 0.0f;
        }
    }
    
    int i_idx = 0;
    for(int ring = 0; ring < rings; ring++) 
    {
        for(int seg = 0; seg < segments; seg++) 
        {
            int current = ring * (segments + 1) + seg;
            int next = current + segments + 1;
            
            (*indices)[i_idx++] = current;
            (*indices)[i_idx++] = next;
            (*indices)[i_idx++] = current + 1;
            
            (*indices)[i_idx++] = current + 1;
            (*indices)[i_idx++] = next;
            (*indices)[i_idx++] = next + 1;
        }
    }
}