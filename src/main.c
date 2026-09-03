#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ay_rasterize.h"
#define AY_RASTERIZE_PROFILE_ENABLED
#include "ay_rasterize_profile.h"

#include "cube_mesh.h"
#include "cone_mesh.h"
#include "cylinder_mesh.h"
#include "sphere_mesh.h"

#define bUpscaleEnabled false
#define tUpscaleFilterUsed AY_UPSCALE_FILTER_BILINEAR

// real output/window resolution, always the same regardless of the toggle
#define outputWidth  1280
#define outputHeight 720

// render resolution: low-res when upscaling is on, full output res when off
#define screenWidth  (bUpscaleEnabled ? 640 : outputWidth)
#define screenHeight (bUpscaleEnabled ? 360 : outputHeight)

#define uWarmupFrames     60
#define uBenchmarkFrames  300
#define fSceneMeshScale   0.6f

typedef struct _ayTestSceneObject
{
    const float*    pfVertices;
    const uint32_t* puIndices;
    uint32_t        uVertexCount;
    uint32_t        uIndexCount;
    float           fPositionX;
    ayVec3*         ptVertexColors; // one entry per vertex, generated once at startup
} ayTestSceneObject;

static ayVec3*
generate_vertex_colors(uint32_t uVertexCount)
{
    ayVec3* ptColors = malloc(sizeof(ayVec3) * uVertexCount);

    for(uint32_t i = 0; i < uVertexCount; i++)
    {
        ptColors[i].r = (float)((i * 37) % 256);
        ptColors[i].g = (float)((i * 91) % 256);
        ptColors[i].b = (float)((i * 53) % 256);
    }

    return ptColors;
}

ayVec4 scene_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn);
ayVec3 scene_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut);

int main()
{
    ayCreateGraphicsInfo tGraphicsInfo = {
        .uScreenWidth   = screenWidth,
        .uScreenHeight  = screenHeight,
        .bTileRendering = true,
        .tTileSettings = {
            .uTileSize         = 32,
            .uTriangleCapacity = 275
        },
        .bUpscale       = bUpscaleEnabled,
        .tUpscaleSettings = {
            .uOutputWidth  = outputWidth,
            .uOutputHeight = outputHeight,
            .tFilter       = tUpscaleFilterUsed
        }
    };
    ayGraphicsData* ptData = ay_initialize_graphics(&tGraphicsInfo);
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, true);
    ayWindow* ptWindow = ay_create_window(outputWidth, outputHeight, "Scene Test");

    // shared shaders/layout, uVertexCount gets patched per object below
    // since each mesh has a different vertex count
    ayPipeline scenePipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader   = scene_pixel_shader,
        .tVertexShader  = scene_vertex_shader,
        .tLayout = {
            .tAttribType    = {AY_VERTEX_ATTRIBUTE_TYPE_VEC3, AY_VERTEX_ATTRIBUTE_TYPE_VEC2},
            .szAttribOffset = {0, sizeof(float) * 3},
            .szVertexStride = sizeof(float) * 5
        }
    };

    ayTestSceneObject atObjects[] = {
        { cube_vertices,     cube_indices,     CUBE_VERTEX_COUNT,     CUBE_INDEX_COUNT,     -4.5f },
        { cone_vertices,     cone_indices,     CONE_VERTEX_COUNT,     CONE_INDEX_COUNT,     -1.5f },
        { cylinder_vertices, cylinder_indices, CYLINDER_VERTEX_COUNT, CYLINDER_INDEX_COUNT,  1.5f },
        { sphere_vertices,   sphere_indices,   SPHERE_VERTEX_COUNT,   SPHERE_INDEX_COUNT,    4.5f }
    };
    const uint32_t uObjectCount = sizeof(atObjects) / sizeof(atObjects[0]);

    for(uint32_t i = 0; i < uObjectCount; i++)
        atObjects[i].ptVertexColors = generate_vertex_colors(atObjects[i].uVertexCount);

    ayMat4 projection = ay_mat4_perspective(60.0f * 3.14159f / 180.0f, (float)screenWidth / screenHeight, 0.1f, 100.0f);

    double dLastFPSTime = glfwGetTime();
    double dLastFrameTime = glfwGetTime();
    int iFrameCount = 0;
    float fRotation = 0.0f;

    // benchmark state
    uint32_t uFramesSeen = 0;
    double dBenchmarkTimeSum = 0.0;
    double dUpscaleTimeAtWindowStart = 0.0;
    bool bBenchmarkDone = false;

    while(!ay_window_should_close(ptWindow))
    {
        glfwPollEvents();

        double dCurrentTime = glfwGetTime();
        double dFrameTime = dCurrentTime - dLastFrameTime;
        float fDeltaTime = (float)dFrameTime;
        dLastFrameTime = dCurrentTime;

        if(!bBenchmarkDone)
        {
            uFramesSeen++;

            if(uFramesSeen == uWarmupFrames + 1)
            {
                dUpscaleTimeAtWindowStart = g_raster_profiler.dUpscalePass[0];
            }

            if(uFramesSeen > uWarmupFrames)
            {
                dBenchmarkTimeSum += dFrameTime;

                if(uFramesSeen - uWarmupFrames == uBenchmarkFrames)
                {
                    double dAverageFrameTimeMs = (dBenchmarkTimeSum / uBenchmarkFrames) * 1000.0;
                    double dUpscaleTimeInWindow = g_raster_profiler.dUpscalePass[0] - dUpscaleTimeAtWindowStart;
                    double dAverageUpscaleTimeMs = dUpscaleTimeInWindow / uBenchmarkFrames;

                    printf("average frame time over %u frames (after %u warmup): %.3f ms (%.1f fps)\n",
                           uBenchmarkFrames, uWarmupFrames, dAverageFrameTimeMs, 1000.0 / dAverageFrameTimeMs);
                    printf("  of which average upscale pass time: %.3f ms (%.1f%%)\n",
                           dAverageUpscaleTimeMs, (dAverageUpscaleTimeMs / dAverageFrameTimeMs) * 100.0);
                    bBenchmarkDone = true;
                }
            }
        }

        iFrameCount++;

        if(dCurrentTime - dLastFPSTime >= 1.0)
        {
            double fps = iFrameCount / (dCurrentTime - dLastFPSTime);
            char title[256];
            sprintf(title, "Scene Test | FPS: %.1f (%.2f ms)", fps, 1000.0 / fps);
            ay_window_set_title(ptWindow, title);
            iFrameCount = 0;
            dLastFPSTime = dCurrentTime;
        }

        fRotation += fDeltaTime;

        ay_clear_frame_buffer(ptFrameBuffer);
        ay_bind_frame_buffer(ptData, ptFrameBuffer);

        for(uint32_t i = 0; i < uObjectCount; i++)
        {
            ayTestSceneObject* ptObject = &atObjects[i];

            ayMat4 rotation = ay_mat4_rotate_y(fRotation);
            ayMat4 translation = ay_mat4_translate(ptObject->fPositionX, 0.0f, -6.0f);
            ayMat4 model = ay_mat4_multiply(translation, rotation);
            ayMat4 mvp = ay_mat4_multiply(projection, model);

            scenePipeline.tLayout.uVertexCount = ptObject->uVertexCount;

            ay_bind_vertex_buffer(ptData, ptObject->pfVertices);
            ay_bind_index_buffer(ptData, (uint32_t*)ptObject->puIndices);
            ay_bind_descriptor(ptData, 0, AY_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &mvp);
            ay_bind_descriptor(ptData, 1, AY_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ptObject->ptVertexColors);
            ay_bind_pipeline(ptData, &scenePipeline);

            ay_draw_indexed(ptData, 0, ptObject->uIndexCount);
        }

        ay_present_frame(ptData, ptWindow);
    }

    for(uint32_t i = 0; i < uObjectCount; i++)
        free(atObjects[i].ptVertexColors);

    ay_destroy_window(ptWindow);
    ay_destroy_graphics(&ptData);

    free(ptFrameBuffer->pfDepthBuffer);
    free(ptFrameBuffer->auData);
    free(ptFrameBuffer);

    return 0;
}

ayVec4 scene_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec3* ptColor = ay_get_varying(0, ptVaryingDataIn);
    return (ayVec4){ ptColor->r, ptColor->g, ptColor->b, 255.0f };
}

ayVec3 scene_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut)
{
    ayVec3  position = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, tBuiltIns.tLayout, 0);
    ayMat4* pMVP = (ayMat4*)tDescriptor[0].pData;
    ayVec3  tConverted = { position.x, position.z, -position.y };
    ayVec3* ptVertexColors = (ayVec3*)tDescriptor[1].pData;
    ayVec3  tColor = ptVertexColors[tBuiltIns.uVertexID];

    ayVec3* pColorOut = ay_set_varying(AY_VARYING_TYPE_VEC3, ptVaryingDataOut);
    *pColorOut = tColor;

    ayVec4 pos = {tConverted.x * fSceneMeshScale, tConverted.y * fSceneMeshScale, tConverted.z * fSceneMeshScale, 1.0f};
    pos = ay_mat4_mul_vec4(*pMVP, pos);

    return (ayVec3){pos.x / pos.w, pos.y / pos.w, pos.z / pos.w};
}
