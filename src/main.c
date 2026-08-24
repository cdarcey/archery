#include <stdio.h>
#include <stdlib.h>
#include "ay_rasterize.h"
#define AY_RASTERIZE_PROFILE_ENABLED
#include "ay_rasterize_profile.h"

#define bUpscaleEnabled true

// real output/window resolution, always the same regardless of the toggle
#define outputWidth  1280
#define outputHeight 720

// render resolution: low-res when upscaling is on, full output res when off
#define screenWidth  (bUpscaleEnabled ? 640 : outputWidth)
#define screenHeight (bUpscaleEnabled ? 360 : outputHeight)

#define uWarmupFrames     60
#define uBenchmarkFrames  300

ayVec4 texture_sample_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn);
ayVec3 texture_sample_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut);

int main()
{
    // fullscreen quad, position only (uv comes from tBuiltIns.tUV)
    float quad_vertices[] = {
        -1.0f, -1.0f, 0.5f,
         1.0f, -1.0f, 0.5f,
         1.0f,  1.0f, 0.5f,
        -1.0f,  1.0f, 0.5f
    };
    uint32_t quad_indices[] = {0, 1, 2, 2, 3, 0};

    // placeholder path, drop any png here to test
    int iTexWidth, iTexHeight;
    unsigned char* pucTexData = ay_load_png("../data/test.png", &iTexWidth, &iTexHeight);
    if(!pucTexData)
    {
        printf("failed to load texture.png\n");
        return 1;
    }
    ayTexture tTexture = {
        .pucData = pucTexData,
        .iWidth  = iTexWidth,
        .iHeight = iTexHeight
    };

    ayCreateGraphicsInfo tGraphicsInfo = {
        .uScreenWidth   = screenWidth,
        .uScreenHeight  = screenHeight,
        .bTileRendering = true,
        .bUpscale       = bUpscaleEnabled,
        .tUpscaleSettings = {
            .uOutputWidth  = outputWidth,
            .uOutputHeight = outputHeight
        }
    };
    ayGraphicsData* ptData = ay_initialize_graphics(&tGraphicsInfo);
    ayFrameBufferData* ptFrameBuffer = ay_initialize_frame_buffer(screenWidth, screenHeight, false);
    ayWindow* ptWindow = ay_create_window(outputWidth, outputHeight, "Upscale Test");

    ayPipeline texturePipeline = {
        .tVertexWinding = AY_VERTEX_WINDING_COUNTER_CLOCKWISE,
        .tPixelShader   = texture_sample_pixel_shader,
        .tVertexShader  = texture_sample_vertex_shader,
        .tLayout = {
            .tAttribType    = {AY_VERTEX_ATTRIBUTE_TYPE_VEC3},
            .szAttribOffset = {0},
            .szVertexStride = sizeof(float) * 3,
            .uVertexCount   = 4
        }
    };

    double dLastFPSTime = glfwGetTime();
    int iFrameCount = 0;

    // benchmark state
    uint32_t uFramesSeen = 0;
    double dBenchmarkTimeSum = 0.0;
    double dLastFrameTime = glfwGetTime();
    double dUpscaleTimeAtWindowStart = 0.0;
    bool bBenchmarkDone = false;

    while(!ay_window_should_close(ptWindow))
    {
        glfwPollEvents();

        double dCurrentTime = glfwGetTime();
        double dFrameTime = dCurrentTime - dLastFrameTime;
        dLastFrameTime = dCurrentTime;

        if(!bBenchmarkDone)
        {
            uFramesSeen++;

            // snapshot the profiler's running total right as the benchmark window begins
            if(uFramesSeen == uWarmupFrames + 1)
            {
                dUpscaleTimeAtWindowStart = g_raster_profiler.dUpscalePass[0];
            }

            // skip warmup frames, then accumulate the benchmark window
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
            sprintf(title, "Upscale Test | FPS: %.1f (%.2f ms)", fps, 1000.0 / fps);
            glfwSetWindowTitle(ptWindow->pWindow, title);
            iFrameCount = 0;
            dLastFPSTime = dCurrentTime;
        }

        ay_clear_frame_buffer(ptFrameBuffer);
        ay_bind_frame_buffer(ptData, ptFrameBuffer);

        ay_bind_vertex_buffer(ptData, quad_vertices);
        ay_bind_index_buffer(ptData, quad_indices);
        ay_bind_descriptor(ptData, 0, AY_DESCRIPTOR_TYPE_TEXTURE, &tTexture);
        ay_bind_pipeline(ptData, &texturePipeline);

        ay_draw_indexed(ptData, 0, 6);

        ay_present_frame(ptData, ptWindow);
    }

    ay_destroy_window(ptWindow);
    ay_destroy_graphics(&ptData);

    free(ptFrameBuffer->auData);
    free(ptFrameBuffer);
    free(pucTexData);

    return 0;
}

ayVec4 texture_sample_pixel_shader(ayPixelShaderBuiltIns tBuiltIns, ayDescriptor* tDescriptor, const ayVaryingData* ptVaryingDataIn)
{
    ayTexture* ptTexture = (ayTexture*)tDescriptor[0].pData;

    // tBuiltIns.tUV is raw screen pixel coords, normalize to [0,1] before sampling
    ayVec2 tNormalizedUV = {
        tBuiltIns.tUV.x / (float)(screenWidth - 1),
        tBuiltIns.tUV.y / (float)(screenHeight - 1)
    };

    return ay_sample_texture(*ptTexture, tNormalizedUV, 4);
}

ayVec3 texture_sample_vertex_shader(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptor* tDescriptor, ayVaryingData* ptVaryingDataOut)
{
    ayVec3 position = *(ayVec3*)ay_get_vertex_attrib(pVertexDataIn, tBuiltIns.tLayout, 0);
    return position;
}
