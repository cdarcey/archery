

#include <stdio.h>
#include <stdlib.h>
#include "ay_rasterize.h"

#define screenWidth  1280
#define screenHeight 720

ayVec4 color_gradient_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn);
ayVec3 color_gradient_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut);
ayVec4 sphere_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn);
ayVec3 sphere_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut);
void   generate_sphere(float** vertices, uint32_t** indices, int* vertex_count, int* index_count, int segments, int rings);

int main()
{
    float quad_vertices[] = {
        -1.0f, -1.0f, 0.5f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 0.5f,  0.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.5f,  1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.5f,  0.0f, 1.0f, 0.0f
    };
    uint32_t quad_indices[] = {0, 1, 2, 2, 3, 0};

    float* sphere_vertices;
    uint32_t* sphere_indices;
    int sphere_vertex_count, sphere_index_count;
    generate_sphere(&sphere_vertices, &sphere_indices, &sphere_vertex_count, &sphere_index_count, 30, 30);

    ayGraphicsData* ptData = initialize_graphics(screenWidth, screenHeight);
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, true);
    ayWindow* ptWindow = ay_create_window(screenWidth, screenHeight, "Renderer Test");
    
    ayPipeline quadPipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader   = color_gradient_pixel_shader,
        .tVertexShader  = color_gradient_vertex_shader,
        .tLayout = {
            .tAttribType    = {AY_VERTEX_ATTRIBUTE_TYPE_VEC3, AY_VERTEX_ATTRIBUTE_TYPE_VEC3},
            .szAttribOffset = {0, sizeof(float) * 3},
            .szVertexStride = sizeof(float) * 6,
            .uVertexCount   = 4
        }
    };

    ayPipeline spherePipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader   = sphere_pixel_shader,
        .tVertexShader  = sphere_vertex_shader,
        .tLayout = {
            .tAttribType    = {AY_VERTEX_ATTRIBUTE_TYPE_VEC3, AY_VERTEX_ATTRIBUTE_TYPE_VEC2},
            .szAttribOffset = {0, sizeof(ayVec3)},
            .szVertexStride = sizeof(float) * 5,
            .uVertexCount   = sphere_vertex_count
        }
    };
    
    ayMat4 identity = {
        1, 0, 0, 0, 
        0, 1, 0, 0, 
        0, 0, 1, 0, 
        0, 0, 0, 1
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
            sprintf(title, "Renderer Test | FPS: %.1f (%.2f ms)", fps, 1000.0 / fps);
            glfwSetWindowTitle(ptWindow->pWindow, title);
            iFrameCount = 0;
            dLastFPSTime = dCurrentTime;
        }
        
        fRotation += fDeltaTime;
        ayMat4 rotation = ay_mat4_rotate_y(fRotation);
        ayMat4 translation = ay_mat4_translate(0.0f, 0.0f, -2.2f);
        ayMat4 model = ay_mat4_multiply(translation, rotation);
        ayMat4 mvp = ay_mat4_multiply(projection, model);
        
        ay_clear_frame_buffer(ptFrameBuffer);
        ay_bind_frame_buffer(ptData, ptFrameBuffer);
        
        // quad setup
        // ay_bind_vertex_buffer(ptData, quad_vertices);
        // ay_bind_index_buffer(ptData, quad_indices);
        // ay_bind_descriptor(ptData, 0, AY_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &identity);
        // ay_bind_pipeline(ptData, &quadPipeline);
        
        // sphere setup
        ay_bind_vertex_buffer(ptData, sphere_vertices);
        ay_bind_index_buffer(ptData, sphere_indices);
        ay_bind_descriptor(ptData, 0, AY_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &mvp);
        ay_bind_pipeline(ptData, &spherePipeline);
        
        // ay_draw_indexed_tiled(ptData, 0, 6);
        // ay_draw_indexed(ptData, 0, 6);
        ay_draw_indexed_tiled(ptData, 0, sphere_index_count);
        // ay_draw_indexed(ptData, 0, sphere_index_count);
        
        ay_present_frame(ptWindow, ptFrameBuffer);
        // ay_output_frame_buffer(ptFrameBuffer);
    }
    
    ay_destroy_window(ptWindow);
    free(ptFrameBuffer->pfDepthBuffer);
    free(ptFrameBuffer->auData);
    free(ptFrameBuffer);
    free(ptData);
    free(sphere_vertices);
    free(sphere_indices);
    
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

ayVec4 sphere_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec3* normal = ay_get_varying(0, ptVaryingDataIn);
    float light = normal->x * 0.4f + normal->y * 0.6f + normal->z * 0.5f;
    light = light * 0.5f + 0.5f;
    float gray = 60 + light * 190;
    return (ayVec4){gray, gray, gray * 1.05f, 255};
}

ayVec3 sphere_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut)
{
    ayVec3 tPos = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, tBuiltIns.tLayout, 0);
    ayMat4* pMVP = (ayMat4*)tDescriptor[0].pData;
    
    ayVec4 pos = {tPos.x, tPos.y, tPos.z, 1.0f};
    pos = ay_mat4_mul_vec4(*pMVP, pos);
    
    ayVec3* pNormal = ay_set_varying(AY_VARYING_TYPE_VEC3, ptVaryingDataOut);
    *pNormal = tPos;
    
    return (ayVec3){pos.x / pos.w, pos.y / pos.w, pos.z / pos.w};
}

void generate_sphere(float** vertices, uint32_t** indices, int* vertex_count, int* index_count, int segments, int rings)
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