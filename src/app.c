

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <string.h> // memset
#include "pl.h"
#include "ay_rasterize.h"

#define PL_MATH_INCLUDE_FUNCTIONS // required to expose some of the color helpers
#include "pl_math.h"

// extensions
#include "pl_draw_ext.h"
#include "pl_starter_ext.h"
#include "pl_ui_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
#include "pl_stats_ext.h"
#include "pl_console_ext.h"
#include "pl_graphics_ext.h"
#include "pl_draw_backend_ext.h"

// out extension
#include "pl_example_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] shaders
//-----------------------------------------------------------------------------

// for texture 
ayVec4
ayPixelShader_0(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{

    const ayVec4* ptColor = ay_get_varying(0, ptVaryingDataIn);
    const ayVec2* ptUV = ay_get_varying(1, ptVaryingDataIn);

    ayTexture spriteTexture = *(ayTexture*)tInfo->atDescriptors[1].pData;

    ayVec4 tColor = ay_sample_texture(spriteTexture, *ptUV, 4);
    // ayVec4 tColor = ay_sample_texture_bilinear(spriteTexture, *ptUV, 4);

    return (ayVec4){tColor.r, tColor.g, tColor.b, tColor.a};
};

ayVec4
ayPixelShader_1(ayPixelShaderBuiltIns tBuiltIns, ayDescriptorInfo* tInfo, const ayVaryingData* ptVaryingDataIn)
{
    const ayVec4* ptColor = ay_get_varying(0, ptVaryingDataIn);

    return (ayVec4){ptColor->r * 255, 
                    ptColor->g * 255, 
                    ptColor->b * 255,
                    ptColor->a * 255};
}

ayVec2
ayVertexShader_0(ayVertexShaderBuiltIns tBuiltIns, const void* pVertexDataIn, ayDescriptorInfo* tInfo, ayVaryingData* ptVaryingDataOut)
{
    ayVertexLayout vertLayout = tBuiltIns.tLayout;
    const char* pcVertexDataIn = pVertexDataIn;

    // get vertex attributes (inputs)
    ayVec2 tPos = *(ayVec2*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 0); 
    ayVec2 tUV = *(ayVec2*)ay_get_vertex_attrib(pVertexDataIn, vertLayout, 1); 
    ayVec4 tColor = *(ayVec4*)ay_get_vertex_attrib(pcVertexDataIn, vertLayout, 2);

    // set varyings (outputs)
    ayVec4* ptColor = ay_set_varying(AY_VARYING_TYPE_VEC4, ptVaryingDataOut);  // color
    *ptColor = tColor;

    ayVec2* ptUV = ay_set_varying(AY_VARYING_TYPE_VEC2, ptVaryingDataOut);  // uv
    *ptUV = tUV;

    return (ayVec2){tPos.x, tPos.y};
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // log channel
    uint64_t uExampleLogChannel;

    // options
    bool bDynamicRasterize;
    bool bRasterizeNextFrame;

    // archery data
    ayGraphicsData* ayGfxData;
    ayFrameBufferData* ptFBData;

    // texture stuff
    plBufferHandle    atTextureBuffers[PL_MAX_FRAMES_IN_FLIGHT];
    plTextureHandle   atTextures[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle atTextureBindGroups[PL_MAX_FRAMES_IN_FLIGHT];
    plSamplerHandle   tSampler;
    
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plGraphicsI*    gptGfx         = NULL;
const plDrawI*        gptDraw        = NULL;
const plDrawBackendI* gptDrawBackend = NULL;
const plStarterI*     gptStarter     = NULL;
const plUiI*          gptUI          = NULL;
const plScreenLogI*   gptScreenLog   = NULL;
const plProfileI*     gptProfile     = NULL;
const plStatsI*       gptStats       = NULL;
const plMemoryI*      gptMemory      = NULL;
const plLogI*         gptLog         = NULL;
const plConsoleI*     gptConsole     = NULL;
const plExampleI*     gptExample     = NULL;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptUI          = pl_get_api_latest(ptApiRegistry, plUiI);
        gptScreenLog   = pl_get_api_latest(ptApiRegistry, plScreenLogI);
        gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptStats       = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptLog         = pl_get_api_latest(ptApiRegistry, plLogI);
        gptConsole     = pl_get_api_latest(ptApiRegistry, plConsoleI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);

        return ptAppData;
    }

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    //   * first argument is the shared library name WITHOUT the extension
    //   * second & third argument is the load/unload functions names (use NULL for the default of "pl_load_ext" &
    //     "pl_unload_ext")
    //   * fourth argument indicates if the extension is reloadable (should we check for changes and reload if changed)
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false); // provides the file API used by the drawing ext
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptUI          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptScreenLog   = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptStats       = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptLog         = pl_get_api_latest(ptApiRegistry, plLogI);
    gptConsole     = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example Basic 2",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 1000,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    // initialize the starter API (handles alot of boilerplate)
    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };
    gptStarter->initialize(tStarterInit);
    gptStarter->finalize();

    // add a log channel
    ptAppData->uExampleLogChannel = gptLog->add_channel("Example 2", (plLogExtChannelInit){.tType = PL_LOG_CHANNEL_TYPE_BUFFER});

    //-------------------------------------------------------------------------
    // Archery stuff ----------------------------------------------------------
    //-------------------------------------------------------------------------

    uint32_t uFrameBufferWidth = 400;
    uint32_t uFrameBufferHeight = 400;

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {

        // create vertex buffer
        const plBufferDesc tStagingBufferDesc = {
            .tUsage      = PL_BUFFER_USAGE_STAGING,
            .szByteSize  = uFrameBufferWidth * uFrameBufferHeight * 4 * sizeof(char),
            .pcDebugName = "staging buffer"
        };
        ptAppData->atTextureBuffers[i] = gptGfx->create_buffer(gptStarter->get_device(), &tStagingBufferDesc, NULL);

        // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(gptStarter->get_device(), ptAppData->atTextureBuffers[i]);

        // allocate memory for the vertex buffer
        const plDeviceMemoryAllocation tStagingBufferAllocation = gptGfx->allocate_memory(gptStarter->get_device(),
            ptStagingBuffer->tMemoryRequirements.ulSize,
            PL_MEMORY_GPU_CPU,
            ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
            "staging buffer memory");

        // bind the buffer to the new memory allocation
        gptGfx->bind_buffer_to_memory(gptStarter->get_device(), ptAppData->atTextureBuffers[i], &tStagingBufferAllocation);

        // create texture
        const plTextureDesc tTextureDesc = {
            .tDimensions = { (float)uFrameBufferWidth, (float)uFrameBufferHeight, 1},
            .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
            .uLayers     = 1,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_2D,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED,
            .pcDebugName = "frame buffer texture"
        };
        plTexture* ptTexture = NULL;
        ptAppData->atTextures[i] = gptGfx->create_texture(gptStarter->get_device(), &tTextureDesc, &ptTexture);

        // allocate memory
        const plDeviceMemoryAllocation tTextureAllocation = gptGfx->allocate_memory(gptStarter->get_device(),
            ptTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_GPU,
            ptTexture->tMemoryRequirements.uMemoryTypeBits,
            "texture memory");

        // bind memory
        gptGfx->bind_texture_to_memory(gptStarter->get_device(), ptAppData->atTextures[i], &tTextureAllocation);

        // set the initial texture usage (this is a no-op in metal but does layout transition for vulkan)
        plBlitEncoder* ptEncoder = gptStarter->get_blit_encoder();
        gptGfx->set_texture_usage(ptEncoder, ptAppData->atTextures[i], PL_TEXTURE_USAGE_SAMPLED, 0);
        gptStarter->return_blit_encoder(ptEncoder);

        ptAppData->atTextureBindGroups[i] = gptDrawBackend->create_bind_group_for_texture(ptAppData->atTextures[i]);
    }


    ptAppData->ayGfxData = initialize_graphics();
    ptAppData->ptFBData = ay_initialize_frame_buffer(uFrameBufferWidth, uFrameBufferHeight);

    int iTextureWidth = 0;
    int iTextureHeight = 0;
    ayTexture testTexture = {
        .pucData = ay_load_png("../data/SpriteMapExample.png", &iTextureWidth, &iTextureHeight),
        .iWidth  = 416,
        .iHeight = 384
    };

    // ay_bind_texture(ptAppData->ayGfxData, 1, &testTexture);
    
    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    gptStarter->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    // this needs to be the first call when using the starter
    // extension. You must return if it returns false (usually a swapchain recreation).
    if(!gptStarter->begin_frame())
        return;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UI & Screen Log API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptProfile->begin_sample(0, "Archery CPU Rasterize");


    if(ptAppData->bDynamicRasterize || ptAppData->bRasterizeNextFrame)
    {

        // TODO: need to malloc memory for all draw data
        
        // vertex buffer
        float afVertexBuffer[] = { // x, y, u, v, r, g, b, a
            -0.75f, -0.75f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.75f, // top left
             0.75f, -0.75f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.75f, // top right
            -0.75f,  0.75f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.75f, // bottom left
             0.75f,  0.75f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.75f, // bottom right
        };
    
        // index buffer
        uint32_t atIndexBuffer[6] = { 
            0, 1, 2, 1, 3, 2
        };

        ayPipeline tPipeline0 = {
            .tPixelShader = ayPixelShader_1,
            .tVertexShader = ayVertexShader_0,
            .tLayout = {
                .tAttribType = {
                    AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                    AY_VERTEX_ATTRIBUTE_TYPE_VEC2,
                    AY_VERTEX_ATTRIBUTE_TYPE_VEC4,
                },
                .szAttribOffset = {
                    0,
                    sizeof(ayVec2),
                    sizeof(ayVec2) + sizeof(ayVec2),
                },
                .szVertexStride = sizeof(float) * 8,
            }
        };
    
        gptProfile->begin_sample(0, "Clear Frame Buffer");
        ay_clear_frame_buffer(ptAppData->ptFBData, (ayVec4){255, 255, 255, 1.0f});
        gptProfile->end_sample(0);

        ay_bind_frame_buffer(ptAppData->ayGfxData, ptAppData->ptFBData);
        ay_bind_pipeline(ptAppData->ayGfxData, &tPipeline0);
        ay_bind_vertex_buffer(ptAppData->ayGfxData, afVertexBuffer);
        ay_bind_index_buffer(ptAppData->ayGfxData, atIndexBuffer);

        gptProfile->begin_sample(0, "Draw");
        ay_draw(ptAppData->ayGfxData, 0, 6);  
        gptProfile->end_sample(0);
        // ay_output_frame_buffer(ptAppData->ptFBData);
        
        ptAppData->bRasterizeNextFrame = false;
    }

    plBuffer* ptStagingBuffer = gptGfx->get_buffer(gptStarter->get_device(), ptAppData->atTextureBuffers[gptGfx->get_current_frame_index()]);
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptAppData->ptFBData->pucData, ptAppData->ptFBData->uWidth * ptAppData->ptFBData->uHeight * 4);

    plBlitEncoder* ptEncoder = gptStarter->get_blit_encoder();

    plBufferImageCopy tImageCopy = {
        .uImageWidth    = ptAppData->ptFBData->uWidth,
        .uImageHeight   = ptAppData->ptFBData->uHeight,
        .uImageDepth    = 1,
        .uLayerCount    = 1
    };
    gptGfx->copy_buffer_to_texture(ptEncoder, ptAppData->atTextureBuffers[gptGfx->get_current_frame_index()], ptAppData->atTextures[gptGfx->get_current_frame_index()], 1, &tImageCopy);

    gptStarter->return_blit_encoder(ptEncoder); // blocking

    gptProfile->end_sample(0);

    if(gptUI->begin_window("Frame Buffer", NULL, PL_UI_WINDOW_FLAGS_AUTO_SIZE))
    {
        gptUI->image(ptAppData->atTextureBindGroups[gptGfx->get_current_frame_index()].uIndex, (plVec2){(float)ptAppData->ptFBData->uWidth, (float)ptAppData->ptFBData->uHeight});
        gptUI->end_window();
    }

    // creating another window
    if(gptUI->begin_window("Pilot Light", NULL, PL_UI_WINDOW_FLAGS_NONE))
    {
        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);

        gptUI->checkbox("Dynamic Rasterize", &ptAppData->bDynamicRasterize);
        if(gptUI->button("rasterize triangle"))
        {
            ptAppData->bRasterizeNextFrame = true;
        }


        gptUI->end_window();
    }

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}



#include "ay_rasterize.c"