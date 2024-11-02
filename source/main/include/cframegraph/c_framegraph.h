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
        struct Fg;

        struct FgFlags
        {
            u32 m_descr;
        };
        static FgFlags s_flags_ignored = {0xFFFFFFFF};

        struct FgPassInfo;
        typedef FgPassInfo* FgPass;
        static const FgPass s_invalid_pass = nullptr;

        typedef u16 FgIndex;

        struct FgTexture
        {
            FgIndex index;
        };
        static const FgTexture s_invalid_texture = {0xFFFF};

        struct FgBuffer
        {
            FgIndex index;
        };
        static const FgBuffer s_invalid_buffer = {0xFFFF};

        struct Fg
        {
            void setup(alloc_t* allocator, u32 resource_capacity, u32 pass_capacity);
            void teardown(alloc_t* allocator);

            void set_create_texture(callback_t<void, GfxRenderContext*, GfxTexture*, GfxTextureDescr*> fn);
            void set_preread_texture(callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags> fn);
            void set_prewrite_texture(callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags> fn);
            void set_destroy_texture(callback_t<void, GfxRenderContext*, GfxTexture*> fn);

            void set_create_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*, GfxBufferDescr*> fn);
            void set_preread_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags> fn);
            void set_prewrite_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags> fn);
            void set_destroy_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*> fn);

            FgPass add_pass(const char* name, callback_t<void, Fg&, GfxRenderContext*> execute);

            FgTexture import(const char* name, GfxTexture* resource, GfxTextureDescr* descr);
            FgBuffer  import(const char* name, GfxBuffer* resource, GfxBufferDescr* descr);

            FgTexture create(const char* name, GfxTexture* textureObject, GfxTextureDescr* textureDescr);
            FgTexture read(FgTexture texture, FgFlags descr = s_flags_ignored);
            FgTexture write(FgTexture texture, FgFlags descr = s_flags_ignored);

            FgBuffer create(const char* name, GfxBuffer* bufferObject, GfxBufferDescr* bufferDescr);
            FgBuffer read(FgBuffer buffer, FgFlags descr = s_flags_ignored);
            FgBuffer write(FgBuffer buffer, FgFlags descr = s_flags_ignored);

            void compile(alloc_t* allocator);
            void execute(GfxRenderContext* ctxt);

            GfxTexture*      get(FgTexture resource) const;
            GfxBuffer*       get(FgBuffer resource) const;
            GfxTextureDescr* getDescr(FgTexture resource) const;
            GfxBufferDescr*  getDescr(FgBuffer resource) const;
            FgFlags          getFlags(FgTexture resource) const;
            FgFlags          getFlags(FgBuffer resource) const;

        private:
            typedef s8          FgType;
            static const FgType FgMain   = 0;
            static const FgType FgCreate = 1;
            static const FgType FgRead   = 2;
            static const FgType FgWrite  = 3;

            bool is_valid(FgTexture resource) const;
            bool is_valid(FgBuffer resource) const;
            bool pass_contains(FgPass pass, FgType type, FgTexture resource) const;
            bool pass_contains(FgPass pass, FgType type, FgBuffer resource) const;

            alloc_t*       m_allocator;
            u32            m_resource_array_capacity; // maximum number of resources
            u32            m_fg_generation;           // random number to identify the FG
            u32            m_pass_array_capacity;
            u32            m_pass_array_size;
            FgPassInfo**   m_passinfo_array;
            FgPass         m_current_passinfo;
            u32            m_textureinfo_cursor[4]; // current number of create texture info resources
            u32            m_bufferinfo_cursor[4];  // current number of create buffer info resources
            FgTextureInfo* m_textureinfo_array;
            FgFlags*       m_textureinfo_flags;
            FgIndex*       m_textureinfo_create_array;
            FgIndex*       m_textureinfo_read_array;
            FgIndex*       m_textureinfo_write_array;
            FgBufferInfo*  m_bufferinfo_array;
            FgFlags*       m_bufferinfo_flags;
            FgIndex*       m_bufferinfo_create_array;
            FgIndex*       m_bufferinfo_read_array;
            FgIndex*       m_bufferinfo_write_array;

            callback_t<void, GfxRenderContext*, GfxTexture*, GfxTextureDescr*> m_create_texture;
            callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags>          m_preread_texture;
            callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags>          m_prewrite_texture;
            callback_t<void, GfxRenderContext*, GfxTexture*>                   m_destroy_texture;
            callback_t<void, GfxRenderContext*, GfxBuffer*, GfxBufferDescr*>   m_create_buffer;
            callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags>           m_preread_buffer;
            callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags>           m_prewrite_buffer;
            callback_t<void, GfxRenderContext*, GfxBuffer*>                    m_destroy_buffer;
        };
    } // namespace nframegraph
} // namespace ncore

#endif
