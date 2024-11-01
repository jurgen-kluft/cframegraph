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
    // - Pass to Pass dependency:
    //   - Can be deduced from the resource dependencies
    //   - Enable control to manually specify a Pass to Pass dependency
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
        static FgFlags s_ignore_flags = {0xFFFFFFFF};

        struct FgPass
        {
            u16 m_generation;
            u16 m_index;
        };
        static const FgPass s_invalid_pass = {0xff, 0xffff};

        struct FgTexture
        {
            u8  m_generation;
            u8  m_type;
            u16 m_index;
        };
        static const FgTexture s_invalid_texture = {0xff, 0xff, 0xffff};

        struct FgBuffer
        {
            u8  m_generation;
            u8  m_type;
            u16 m_index;
        };
        static const FgBuffer s_invalid_buffer = {0xff, 0xff, 0xffff};

        struct Fg
        {
            struct range_t
            {
                s32 size() const { return end - begin; }
                s32 begin;
                s32 end;
            };

            struct FgPassInfo
            {
                const char*                              m_name;
                callback_t<void, Fg&, GfxRenderContext*> m_execute_fn;
                bool                                     m_has_side_effects;
                u32                                      m_array_index;    // the index into the pass array where this pass is stored
                s32                                      m_ref_count;      // the number of resources that are written by this pass
                range_t                                  m_texture_create; // the begin and end index into the 'create' texture array
                range_t                                  m_texture_read;   // the begin and end index into the 'read' texture array
                range_t                                  m_texture_write;  // the begin and end index into the 'write' texture array
                range_t                                  m_buffer_create;  // the begin and end index into the 'create' buffer array
                range_t                                  m_buffer_read;    // the begin and end index into the 'read' buffer array
                range_t                                  m_buffer_write;   // the begin and end index into the 'write' buffer array
            };

            enum EFgType
            {
                Import,
                Create,
                Read,
                Write,
            };

            struct FgResourceInfo
            {
                const char* m_name;
                FgPassInfo* m_pass;
                FgPassInfo* m_last;
                s32         m_ref_count;
            };

            struct FgTextureInfo
            {
                FgResourceInfo   m_resource;
                GfxTexture*      m_texture;
                GfxTextureDescr* m_textureDescr;
                FgTextureInfo*   m_previous;
            };

            struct FgBufferInfo
            {
                FgResourceInfo  m_resource;
                GfxBuffer*      m_buffer;
                GfxBufferDescr* m_bufferDescr;
                FgBufferInfo*   m_previous;
            };

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

            FgPass add_pass(const char* name, callback_t<void, Fg&, void*> execute);

            FgTexture import(const char* name, GfxTexture* resource, GfxTextureDescr* descr);
            FgBuffer  import(const char* name, GfxBuffer* resource, GfxBufferDescr* descr);

            FgTexture create(FgPass pass, const char* name, GfxTexture* textureObject, GfxTextureDescr* textureDescr);
            FgTexture read(FgPass pass, FgTexture texture, FgFlags descr = s_ignore_flags);
            FgTexture write(FgPass pass, FgTexture texture, FgFlags descr = s_ignore_flags);

            FgBuffer create(FgPass pass, const char* name, GfxBuffer* bufferObject, GfxBufferDescr* bufferDescr);
            FgBuffer read(FgPass pass, FgBuffer buffer, FgFlags descr = s_ignore_flags);
            FgBuffer write(FgPass pass, FgBuffer buffer, FgFlags descr = s_ignore_flags);

            void compile(alloc_t* allocator);
            void execute(GfxRenderContext* ctxt);

            GfxTexture*      get(FgTexture resource) const;
            GfxBuffer*       get(FgBuffer resource) const;
            GfxTextureDescr* getDescr(FgTexture resource) const;
            GfxBufferDescr*  getDescr(FgBuffer resource) const;
            FgFlags          getFlags(FgTexture resource) const;
            FgFlags          getFlags(FgBuffer resource) const;

        private:
            u32 m_resource_array_capacity; // maximum number of resources
            u32 m_fg_generation;           // random number to identify the FG

            callback_t<void, GfxRenderContext*, GfxTexture*, GfxTextureDescr*> m_create_texture;
            callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags>          m_preread_texture;
            callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags>          m_prewrite_texture;
            callback_t<void, GfxRenderContext*, GfxTexture*>                   m_destroy_texture;
            callback_t<void, GfxRenderContext*, GfxBuffer*, GfxBufferDescr*>   m_create_buffer;
            callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags>           m_preread_buffer;
            callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags>           m_prewrite_buffer;
            callback_t<void, GfxRenderContext*, GfxBuffer*>                    m_destroy_buffer;

            u32            m_texture_info_array_size; // current number of used texture info resources
            u32            m_buffer_info_array_size;  // current number of used buffer info resources
            FgTextureInfo* m_texture_info_array;
            FgBufferInfo*  m_buffer_info_array;
            FgFlags*       m_resource_info_flags;

            u32         m_pass_array_capacity;
            u32         m_pass_array_size;
            FgPassInfo* m_pass_info_array;
        };
    } // namespace nframegraph
} // namespace ncore

#endif
