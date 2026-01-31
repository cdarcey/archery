#include "ay_rasterize.h"
#include "ay_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define screenWidth  750
#define screenHeight 750

typedef struct _ayProfileInfo
{
    char   pcName[64];
    double dStartTime;
    double dEndTime;
    double dTotalTime;
    double dDuration;
    int    iCallCount;
} ayProfileInfo;

void start_profile(ayProfileInfo* tInfo, const char* pcName);
void end_profile(ayProfileInfo* tInfo);

ayVec4
ayPixelShader_test(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec2* ptUV = ay_get_varying(0, ptVaryingDataIn);
    
    // cast descriptor at binding 0 to texture
    ayTexture* pTexture = (ayTexture*)tDescriptor[1].pData;
    
    ayVec4 tColor = ay_sample_texture(*pTexture, *ptUV, 4);
    return tColor;
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
    ayVec4 pos = {tPos.x, tPos.y, tPos.z, 1.0f};
    pos = ay_mat4_mul_vec4(*pMVP, pos);
    ayVec3 tResult = (ayVec3){pos.x / pos.w, pos.y / pos.w, pos.z / pos.w};
    
    ayVec2* ptUV = ay_set_varying(AY_VARYING_TYPE_VEC2, ptVaryingDataOut);  
    *ptUV = tUV;

    return tResult;
}

int main()
{
    ayGraphicsData* ptData = initialize_graphics();
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, true); // depth enabled
    ayWindow* ptTestWindow = ay_create_window(screenWidth, screenHeight, "Test Window");
    ay_clear_frame_buffer(ptFrameBuffer);

    float fCubeVertices[] = {
        // front face (z = 0.5)
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  // 0: bottom-left
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  // 1: bottom-right
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  // 2: top-right
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  // 3: top-left
        
        // back face (z = -0.5)
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  // 4: bottom-left
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  // 5: bottom-right
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  // 6: top-right
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  // 7: top-left
    };

    uint32_t uCubeIndices[] = {
        0, 1, 2, // front face
        0, 2, 3,
        1, 5, 6, // right face
        1, 6, 2,
        5, 4, 7, // back face
        5, 7, 6,
        4, 0, 3, // left face
        4, 3, 7,
        3, 2, 6, // top face
        3, 6, 7,
        4, 5, 1, // bottom face
        4, 1, 0
    };


    ayPipeline tPipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_CLOCKWISE,
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

    // texture loading
    int iTextureWidth = 0;
    int iTextureHeight = 0;
    ayTexture testTexture = {
        .pucData = ay_load_png("../data/sprites.png", &iTextureWidth, &iTextureHeight),
        .iWidth  = iTextureWidth,
        .iHeight = iTextureHeight
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
    ayProfileInfo* tTestProfile = malloc(sizeof(ayProfileInfo));

    while(!ay_window_should_close(ptTestWindow)) 
    {
        glfwPollEvents();


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

        // create model matrix with rotation and translation
        ayMat4 rotation = ay_mat4_rotate_y(fRotation);
        ayMat4 translation = ay_mat4_translate(0.0f, 0.0f, -3.0f);
        ayMat4 model = ay_mat4_multiply(translation, rotation);

        ayMat4 mvp = ay_mat4_multiply(projection, model);
    
        // clear buffers each frame
        ay_clear_frame_buffer(ptFrameBuffer);
        
        
        ay_bind_frame_buffer(ptData, ptFrameBuffer);
        ay_bind_vertex_buffer(ptData, fCubeVertices);
        ay_bind_index_buffer(ptData, uCubeIndices);
        ay_bind_descriptor(ptData, 0, AY_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &mvp);
        ay_bind_descriptor(ptData, 1, AY_DESCRIPTOR_TYPE_TEXTURE, &testTexture);
        

        // draw both triangles
        ay_bind_pipeline(ptData, &tPipeline);
        ay_draw_indexed(ptData, 0, 36);  // 36 indices = 12 triangles

        // present to window
        ay_present_frame(ptTestWindow, ptFrameBuffer); 
    }

    free(tTestProfile);

    // clean up
    ay_destroy_window(ptTestWindow);
    free(ptFrameBuffer->pfDepthBuffer);
    free(ptFrameBuffer->pucData);
    free(ptFrameBuffer);
    free(ptData);

    return 0;
}

void 
start_profile(ayProfileInfo* tInfo, const char* pcName)
{
    tInfo->dStartTime = glfwGetTime();
    strncpy(tInfo->pcName, pcName, sizeof(tInfo->pcName) - 1);
    tInfo->pcName[sizeof(tInfo->pcName) - 1] = '\0';  // null terminate
    tInfo->iCallCount++;
    tInfo->dTotalTime = 0.0;
}

void 
end_profile(ayProfileInfo* tInfo)
{
    tInfo->dEndTime = glfwGetTime();
    tInfo->dDuration = (tInfo->dEndTime - tInfo->dStartTime) * 1000;
    tInfo->dTotalTime += tInfo->dDuration;
    if(tInfo->iCallCount > 60) 
    {
        tInfo->iCallCount = 0;
        printf("%s: = %lfms\n",tInfo->pcName, tInfo->dDuration); 
    }
}