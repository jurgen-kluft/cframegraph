#include "ccore/c_callback.h"
#include "ccore/c_memory.h"
#include "callocator/c_allocator_linear.h"
#include "cframegraph/c_framegraph.h"

#include "cunittest/cunittest.h"

namespace ncore
{
    using namespace nframegraph;

    // --------------------------------------------------------------------------------------------
    // --------------------------------------------------------------------------------------------
    // Depending on the backend implementation; Vulkan, OpenGL, DirectX, Metal, etc.

    struct GfxTexture
    {
        // ...
    };

    struct GfxTextureDescr
    {
        u16 width      = 1;
        u16 height     = 1;
        u32 depth      = 1;
        u32 mip_levels = 1;
        // type
        // format
        // ...
    };

    struct GfxBuffer
    {
    };

    struct GfxBufferDescr
    {
        // ...
    };

    struct GfxRenderContext
    {
        void beginRenderPass(GfxTexture* targetA) {}
        void beginRenderPass(GfxTexture* depth, GfxTexture* normal, GfxTexture* albedo) {}
        void endRenderPass() {}

        void bindTexture(u32 index, GfxTexture* texture) {}
    };

    struct GfxRenderables
    {
    };

    // --------------------------------------------------------------------------------------------
    // --------------------------------------------------------------------------------------------
    // --------------------------------------------------------------------------------------------

    // The Create and Destroy functions to create/destroy the transient textures should be
    // provided by the user. The FrameGraph will call these functions when it needs to create
    // or destroy a transient texture.

    void createTexture(GfxRenderContext* ctxt, GfxTexture* texture, GfxTextureDescr* descr)
    {
        // ...
    }

    void destroyTexture(GfxRenderContext* ctxt, GfxTexture* texture)
    {
        // ...
    }

    void createBuffer(GfxRenderContext* ctxt, GfxBuffer* buffer, GfxBufferDescr* descr)
    {
        // ...
    }

    void destroyBuffer(GfxRenderContext* ctxt, GfxBuffer* buffer)
    {
        // ...
    }

    namespace UserExperience
    {
        namespace nfg
        {
            struct SimplePass
            {
                FgPass          pass;
                FgTexture       out_RT;
                GfxTexture      targetTexture;
                GfxTextureDescr targetTextureDescr;

                void setup(Fg& fg)
                {
                    pass = fg.add_pass("SimplePass", callback_t(this, &SimplePass::execute));

                    out_RT = fg.create("SimplePassOutput", &targetTexture, &targetTextureDescr);
                    fg.write(out_RT);
                }

                void execute(Fg& fg, GfxRenderContext* ctxt)
                {
                    GfxTexture* gtarget = fg.get(out_RT);

                    // ...
                }

                SimplePass(u16 width, u16 height)
                    : pass(s_invalid_pass)
                    , out_RT(s_invalid_texture)
                {
                    targetTextureDescr.width  = width;
                    targetTextureDescr.height = height;
                }
            }; // namespace SimplePass

            static void SimpleExample(alloc_t* allocator)
            {
                GfxRenderContext ctxt;

                Fg fg;
                fg.setup(allocator, 4096, 1024);
                {
                    fg.set_create_texture(callback_t<void, GfxRenderContext*, GfxTexture*, GfxTextureDescr*>(createTexture));
                    fg.set_destroy_texture(callback_t<void, GfxRenderContext*, GfxTexture*>(destroyTexture));

                    fg.set_create_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*, GfxBufferDescr*>(createBuffer));
                    fg.set_destroy_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*>(destroyBuffer));

                    SimplePass simplePass(1280, 720);
                    simplePass.setup(fg);

                    fg.compile(allocator);
                    fg.execute(&ctxt);
                }
                fg.teardown(allocator);
            }

        } // namespace nfg

        namespace nfg
        {
            namespace DeferredLighting
            {
                struct GBufferPass
                {
                    FgPass pass;

                    FgTexture out_depthRT;
                    FgTexture out_normalRT;
                    FgTexture out_albedoRT;

                    GfxTexture depthTexture;
                    GfxTexture normalTexture;
                    GfxTexture albedoTexture;

                    GfxTextureDescr depthTextureDescr;
                    GfxTextureDescr normalTextureDescr;
                    GfxTextureDescr albedoTextureDescr;

                    void setup(Fg& fg, GfxRenderables* ra)
                    {
                        pass = fg.add_pass("GBufferPass", callback_t(this, &GBufferPass::execute));

                        // Create GBuffer render targets
                        out_depthRT = fg.create("depthRT", &depthTexture, &depthTextureDescr);
                        out_depthRT = fg.write(out_depthRT);

                        out_normalRT = fg.create("normalRT", &normalTexture, &normalTextureDescr);
                        out_normalRT = fg.write(out_normalRT);

                        out_albedoRT = fg.create("albedoRT", &albedoTexture, &albedoTextureDescr);
                        out_albedoRT = fg.write(out_albedoRT);
                    }

                    void execute(Fg& fg, GfxRenderContext* ctxt)
                    {
                        GfxTexture* gdepth  = fg.get(out_depthRT);
                        GfxTexture* gnormal = fg.get(out_normalRT);
                        GfxTexture* galbedo = fg.get(out_albedoRT);

                        GfxRenderContext* rc = (GfxRenderContext*)ctxt;
                        rc->beginRenderPass(gdepth, gnormal, galbedo);
                        {
                            // Iterate over renderables and render them, e.g. drawMesh(rc, renderable.mesh, renderable.material)
                        }
                        rc->endRenderPass();
                    }

                    GBufferPass(u16 width, u16 height)
                        : pass(s_invalid_pass)
                        , out_depthRT(s_invalid_texture)
                        , out_normalRT(s_invalid_texture)
                        , out_albedoRT(s_invalid_texture)
                    {
                        depthTextureDescr.width   = width;
                        depthTextureDescr.height  = height;
                        normalTextureDescr.width  = width;
                        normalTextureDescr.height = height;
                        albedoTextureDescr.width  = width;
                        albedoTextureDescr.height = height;
                    }

                }; // namespace GBufferPass

                struct LightingPass
                {
                    FgPass    pass;
                    FgTexture output_HDR;
                    FgTexture in_depthRT;
                    FgTexture in_normalRT;
                    FgTexture in_albedoRT;

                    GfxTexture      hdrTexture;
                    GfxTextureDescr hdrTextureDescr;

                    void setup(Fg& fg, FgTexture _in_depthRT, FgTexture _in_normalRT, FgTexture _in_albedoRT)
                    {
                        in_depthRT  = _in_depthRT;
                        in_normalRT = _in_normalRT;
                        in_albedoRT = _in_albedoRT;

                        pass = fg.add_pass("LightingPass", callback_t(this, &LightingPass::execute));
                        {
                            // Lighting pass is reading gbuffer render targets
                            fg.read(in_depthRT);
                            fg.read(in_normalRT);
                            fg.read(in_albedoRT);

                            // Lighting pass is generating a HDR render target
                            output_HDR = fg.create("HDR", &hdrTexture, &hdrTextureDescr);
                            output_HDR = fg.write(output_HDR);
                        }
                    }

                    void execute(Fg& fg, GfxRenderContext* ctxt)
                    {
                        GfxTexture* ghdr    = fg.get(output_HDR);
                        GfxTexture* gdepth  = fg.get(in_depthRT);
                        GfxTexture* gnormal = fg.get(in_normalRT);
                        GfxTexture* galbedo = fg.get(in_albedoRT);

                        GfxRenderContext* rc = (GfxRenderContext*)ctxt;
                        rc->beginRenderPass(ghdr);
                        {
                            // Bind gbuffer textures
                            rc->bindTexture(0, gdepth);
                            rc->bindTexture(1, gnormal);
                            rc->bindTexture(2, galbedo);

                            // Draw full screen quad
                            // ...
                        }
                        rc->endRenderPass();
                    }

                    LightingPass(u16 width, u16 height)
                        : pass(s_invalid_pass)
                        , output_HDR(s_invalid_texture)
                        , in_depthRT(s_invalid_texture)
                        , in_normalRT(s_invalid_texture)
                        , in_albedoRT(s_invalid_texture)
                    {
                        hdrTextureDescr.width  = width;
                        hdrTextureDescr.height = height;
                    }

                }; // namespace LightingPass
            } // namespace DeferredLighting

            static void DeferredLightingExample(alloc_t* allocator)
            {
                const u16 width  = 1280;
                const u16 height = 720;

                GfxRenderContext ctxt;
                GfxRenderables   gbuffer_pass_ra;

                Fg fg;
                fg.setup(allocator, 4096, 1024);
                {
                    fg.set_create_texture(callback_t<void, GfxRenderContext*, GfxTexture*, GfxTextureDescr*>(createTexture));
                    fg.set_destroy_texture(callback_t<void, GfxRenderContext*, GfxTexture*>(destroyTexture));

                    fg.set_create_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*, GfxBufferDescr*>(createBuffer));
                    fg.set_destroy_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*>(destroyBuffer));

                    DeferredLighting::GBufferPass gbufferPass(width, height);
                    gbufferPass.setup(fg, &gbuffer_pass_ra);

                    DeferredLighting::LightingPass lightingPass(width, height);
                    lightingPass.setup(fg, gbufferPass.out_depthRT, gbufferPass.out_normalRT, gbufferPass.out_albedoRT);

                    fg.compile(allocator);
                    fg.execute(&ctxt);
                }
                fg.teardown(allocator);
            }
        } // namespace nfg

        namespace nfg
        {
            namespace AutomaticResourceBindingsAndBarriers
            {
                // This here is just an example to test out the user experience of the FrameGraph

                typedef f32x4 ClearValue;

                struct Attachment // 21 bits
                {
                    u32        index{0};
                    u32        layer;
                    u32        face;
                    ClearValue clearValue;
                };

                struct Location // 7 bits
                {
                    u32 set{0};
                    u32 binding{0};
                };

                enum EPipelineState
                {
                    PipelineStage_FragmentShader = 1,
                };

                struct BindingInfo // 13 bits
                {
                    Location location;
                    u32      pipelineStage{0};
                };

                struct TextureRead // 15 bits
                {
                    BindingInfo binding;

                    enum class Type
                    {
                        CombinedImageSampler,
                        SampledImage,
                        StorageImage
                    }; // 2 bits
                    Type type;
                };

                static TextureRead setupTextureRead(u16 set, u16 binding, u32 pipelineStage, TextureRead::Type type)
                {
                    TextureRead tr;
                    tr.binding.location.set     = set;
                    tr.binding.location.binding = binding;
                    tr.binding.pipelineStage    = pipelineStage;
                    tr.type                     = type;
                    return tr;
                }

                static FgFlags encodeTextureRead(TextureRead& tr)
                {
                    FgFlags descr;
                    // ...
                    return descr;
                }
                static TextureRead decodeTextureRead(FgFlags descr)
                {
                    TextureRead tr;
                    // ...
                    return tr;
                }

                namespace AutomaticBinding
                {
                    struct FXAA
                    {
                        FgPass    pass;
                        FgTexture out_FXAA;

                        GfxTexture      outputTexture;
                        GfxTextureDescr outputTextureDescr;

                        FXAA()
                            : pass(s_invalid_pass)
                            , out_FXAA(s_invalid_texture)
                        {
                        }

                        void setup(Fg& fg, FgTexture input)
                        {
                            pass = fg.add_pass("FXAA", callback_t(this, &FXAA::execute));
                            {
                                GfxTextureDescr* inputDescr = fg.getDescr(input);
                                // Copy the input texture description to the output texture description
                                outputTextureDescr.width  = inputDescr->width;
                                outputTextureDescr.height = inputDescr->height;
                                // ...

                                // The input and how and from where we are going to read it
                                TextureRead input_tr = setupTextureRead(2, 0, PipelineStage_FragmentShader, TextureRead::Type::CombinedImageSampler);
                                fg.read(input, encodeTextureRead(input_tr));

                                // We require an output render target and we will write the result of FXAA to that target
                                out_FXAA = fg.create("FXAA_RT", &outputTexture, &outputTextureDescr);
                                out_FXAA = fg.write(out_FXAA);
                            }
                        }

                        void execute(Fg& fg, GfxRenderContext* ctxt)
                        {
                            GfxRenderContext& rc = *(GfxRenderContext*)ctxt;
                            // renderFullScreenPostProcess(rc);
                        }
                    };
                } // namespace AutomaticBinding

                static void ExampleFXAA(alloc_t* allocator)
                {
                    const u16 width  = 1280;
                    const u16 height = 720;

                    GfxRenderables   gbuffer_pass_ra;
                    GfxRenderContext ctxt;

                    Fg fg;
                    fg.setup(allocator, 4096, 1024);
                    {
                        fg.set_create_texture(callback_t<void, GfxRenderContext*, GfxTexture*, GfxTextureDescr*>(createTexture));
                        fg.set_destroy_texture(callback_t<void, GfxRenderContext*, GfxTexture*>(destroyTexture));

                        fg.set_create_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*, GfxBufferDescr*>(createBuffer));
                        fg.set_destroy_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*>(destroyBuffer));

                        DeferredLighting::GBufferPass gbufferPass(width, height);
                        gbufferPass.setup(fg, &gbuffer_pass_ra);

                        DeferredLighting::LightingPass lightingPass(width, height);
                        lightingPass.setup(fg, gbufferPass.out_depthRT, gbufferPass.out_normalRT, gbufferPass.out_albedoRT);

                        AutomaticBinding::FXAA fxaa;
                        fxaa.setup(fg, lightingPass.output_HDR);

                        fg.compile(allocator);
                        fg.execute(&ctxt);
                    }
                    fg.teardown(allocator);
                }

            } // namespace AutomaticResourceBindingsAndBarriers
        } // namespace nfg

    } // namespace UserExperience

} // namespace ncore

using namespace ncore;

UNITTEST_SUITE_BEGIN(framegraph)
{
    UNITTEST_FIXTURE(basics)
    {
        UNITTEST_ALLOCATOR;

        void*          alloc_mem  = nullptr;
        const u32      alloc_size = 10 * cMB;
        linear_alloc_t alloc;

        UNITTEST_FIXTURE_SETUP()
        {
            alloc_mem = Allocator->allocate(alloc_size);
            alloc.setup(alloc_mem, alloc_size);
        }

        UNITTEST_FIXTURE_TEARDOWN() { Allocator->deallocate(alloc_mem); }

        UNITTEST_TEST(setup_teardown)
        {
            Fg fg;
            fg.setup(&alloc, 4096, 1024);

            fg.teardown(&alloc);
        }


    }
}
