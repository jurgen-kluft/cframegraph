#ifndef __CFRAMEGRAPH_FRAMEGRAPH_H__
#define __CFRAMEGRAPH_FRAMEGRAPH_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_callback.h"
#include "callocator/c_allocator_linear.h"

namespace ncore
{
    struct GfxTexture;
    struct GfxBuffer;
    struct GfxTextureDescr;
    struct GfxBufferDescr;
    struct GfxRenderContext;

    // Parts/Requirements/Constraints/Functionality of a RenderGraph (RG) / FrameGraph (FG):
    //
    // - A FG/RG:
    //   - Fundamentally has 2 types of Resources:
    //     - Buffers
    //     - Textures
    //   - Has a clear limit to the amount of used Resources
    //   - Can Import Resources that are not created by the FG/RG
    //   - Consists of one or more Passes
    //
    // - A Pass:
    //   - Creates resources (buffers, textures)
    //   - Reads resources (buffers, textures)
    //   - Writes resources (buffers, textures)
    //
    // - A Resource (Buffer, Texture):
    //   - Has a State:
    //     - Clear; the state of the resource before the pass starts, it is empty and not usable
    //     - Build; the state of the resource after the pass ends, it is build and usable, e.g.
    //              a resource that is loaded from disk or created and build in a pass.
    //   - Has a Version: this makes the resource unique wherever it is in the graph. Whenever a
    //     resource is created, it is assigned a unique Version. Then when it is modified in a pass,
    //     the Version is incremented.
    //
    // - User Functions:
    //   - Create: A function to create transient resource
    //   - Destroy: A function to destroy transient resource
    //   - PreRead: A function called before an execution of a pass: build DescriptorSet tables, insert barriers
    //   - PreWrite: A function called before an execution of a pass: build Attachments, insert barriers
    //

    namespace nframegraph
    {
        struct FgFlags
        {
            u32 m_descr;
        };
        static FgFlags s_flags_ignored = {0xFFFFFFFF};
        inline bool    fg_flags_ignored(FgFlags flags) { return flags.m_descr == s_flags_ignored.m_descr; }

        struct FgPassInfo;
        typedef FgPassInfo* FgPass;
        static const FgPass s_invalid_pass = nullptr;

        typedef u16 FgIndex;
        typedef u16 FgGeneration;

        struct FgTextureInfo;
        struct FgTexture
        {
            FgIndex      index;
            FgGeneration generation;
        };
        static const FgTexture s_invalid_texture = {0xFFFF};

        struct FgBufferInfo;
        struct FgBuffer
        {
            FgIndex      index;
            FgGeneration generation;
        };
        static const FgBuffer s_invalid_buffer = {0xFFFF};

        struct Fg;

        typedef callback_t<void, Fg*, GfxRenderContext*> FgExecuteFn;

        Fg*  fg_setup(alloc_t* allocator, u32 resource_capacity, u32 pass_capacity);
        void fg_teardown(Fg*& fg);

        void fg_set_create_texture(Fg* fg, callback_t<void, GfxRenderContext*, GfxTexture*, GfxTextureDescr*> fn);
        void fg_set_preread_texture(Fg* fg, callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags> fn);
        void fg_set_prewrite_texture(Fg* fg, callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags> fn);
        void fg_set_destroy_texture(Fg* fg, callback_t<void, GfxRenderContext*, GfxTexture*> fn);

        void fg_set_create_buffer(Fg* fg, callback_t<void, GfxRenderContext*, GfxBuffer*, GfxBufferDescr*> fn);
        void fg_set_preread_buffer(Fg* fg, callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags> fn);
        void fg_set_prewrite_buffer(Fg* fg, callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags> fn);
        void fg_set_destroy_buffer(Fg* fg, callback_t<void, GfxRenderContext*, GfxBuffer*> fn);

        FgPass fg_open_pass(Fg* fg, const char* name, FgExecuteFn execute);
        FgPass fg_final_pass(Fg* fg, const char* name, FgExecuteFn execute);
        void   fg_close_pass(Fg* fg);

        FgTexture fg_import(Fg* fg, const char* name, GfxTexture* resource, GfxTextureDescr* descr);
        FgBuffer  fg_import(Fg* fg, const char* name, GfxBuffer* resource, GfxBufferDescr* descr);

        FgTexture fg_create(Fg* fg, const char* name, GfxTexture* textureObject, GfxTextureDescr* textureDescr);
        FgTexture fg_read(Fg* fg, FgTexture texture, FgFlags descr = s_flags_ignored);
        FgTexture fg_write(Fg* fg, FgTexture texture, FgFlags descr = s_flags_ignored);

        FgBuffer fg_create(Fg* fg, const char* name, GfxBuffer* bufferObject, GfxBufferDescr* bufferDescr);
        FgBuffer fg_read(Fg* fg, FgBuffer buffer, FgFlags descr = s_flags_ignored);
        FgBuffer fg_write(Fg* fg, FgBuffer buffer, FgFlags descr = s_flags_ignored);

        void fg_compile(Fg* fg, alloc_t* allocator);
        void fg_execute(Fg* fg, GfxRenderContext* ctxt);

        GfxTexture*      fg_get(Fg* fg, FgTexture resource);
        GfxBuffer*       fg_get(Fg* fg, FgBuffer resource);
        GfxTextureDescr* fg_getDescr(Fg* fg, FgTexture resource);
        GfxBufferDescr*  fg_getDescr(Fg* fg, FgBuffer resource);
        FgFlags          fg_getFlags(Fg* fg, FgTexture resource);
        FgFlags          fg_getFlags(Fg* fg, FgBuffer resource);

    } // namespace nframegraph
} // namespace ncore

#endif
