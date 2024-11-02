#include "callocator/c_allocator_ocs.h"
#include "cframegraph/c_framegraph.h"

namespace ncore
{
    namespace nframegraph
    {
        void Fg::setup(alloc_t* allocator, u32 resource_capacity, u32 pass_capacity)
        {
            // ...
        }

        void Fg::teardown(alloc_t* allocator)
        {
            // ...
        }

        void Fg::set_create_texture(callback_t<void, GfxRenderContext*, GfxTexture*, GfxTextureDescr*> fn) { m_create_texture = fn; }
        void Fg::set_preread_texture(callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags> fn) { m_preread_texture = fn; }
        void Fg::set_prewrite_texture(callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags> fn) { m_prewrite_texture = fn; }
        void Fg::set_destroy_texture(callback_t<void, GfxRenderContext*, GfxTexture*> fn) { m_destroy_texture = fn; }

        void Fg::set_create_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*, GfxBufferDescr*> fn) { m_create_buffer = fn; }
        void Fg::set_preread_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags> fn) { m_preread_buffer = fn; }
        void Fg::set_prewrite_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags> fn) { m_prewrite_buffer = fn; }
        void Fg::set_destroy_buffer(callback_t<void, GfxRenderContext*, GfxBuffer*> fn) { m_destroy_buffer = fn; }

        FgPass Fg::add_pass(const char* name, callback_t<void, Fg&, GfxRenderContext*> execute)
        {
            FgPassInfo*& pi   = m_passinfo_array[m_pass_array_size];
            pi                = m_allocator->construct<FgPassInfo>();
            pi->m_name        = name;
            pi->m_execute_fn  = execute;
            pi->m_flags       = 0;
            pi->m_array_index = m_pass_array_size;
            pi->m_ref_count   = 0;
            pi->m_texture_create.reset(m_texture_info_array_size);
            pi->m_texture_read.reset(m_texture_info_array_size);
            pi->m_texture_write.reset(m_texture_info_array_size);
            pi->m_buffer_create.reset(m_buffer_info_array_size);
            pi->m_buffer_read.reset(m_buffer_info_array_size);
            pi->m_buffer_write.reset(m_buffer_info_array_size);
            m_pass_array_size++;
        }

        FgTexture Fg::import(const char* name, GfxTexture* resource, GfxTextureDescr* descr)
        {
            // ...
        }

        FgBuffer Fg::import(const char* name, GfxBuffer* resource, GfxBufferDescr* descr)
        {
            // ...
        }

        FgTexture Fg::create(const char* name, GfxTexture* textureObject, GfxTextureDescr* textureDescr)
        {
            FgTextureInfo*& ti         = m_texture_info_array[m_texture_info_array_size];
            ti->m_resource.m_name      = name;
            ti->m_resource.m_pass      = m_current_passinfo;
            ti->m_resource.m_last      = nullptr;
            ti->m_resource.m_ref_count = 0;
            ti->m_texture              = textureObject;
            ti->m_textureDescr         = textureDescr;

            m_current_passinfo->m_texture_create.add(m_texture_info_array_size);

            m_texture_info_array_size += 1;

            FgTexture ft;
            ft.m_index      = m_texture_info_array_size - 1;
            ft.m_generation = m_fg_generation;
            return ft;
        }

        FgTexture Fg::read(FgTexture texture, FgFlags descr = s_ignore_flags)
        {
            // TODO: verify that this texture is not part of 'create' and 'write'
            // TODO: if this texture is already part of 'read' then return that one

            FgTextureInfo*& rti = m_texture_info_array[texture.m_index];
            FgTextureInfo*& nti = m_texture_info_array[m_texture_info_array_size];

            nti->m_resource.m_name      = rti->m_resource.m_name;
            nti->m_resource.m_pass      = m_current_passinfo;
            nti->m_resource.m_last      = nullptr;
            nti->m_resource.m_ref_count = 0;
            nti->m_texture              = rti->m_texture;
            nti->m_textureDescr         = rti->m_textureDescr;

            m_current_passinfo->m_texture_read.add(m_texture_info_array_size);

            m_texture_info_array_size += 1;

            FgTexture ft;
            ft.m_index      = m_texture_info_array_size - 1;
            ft.m_generation = m_fg_generation;
            return ft;
        }

        FgTexture Fg::write(FgTexture texture, FgFlags descr = s_ignore_flags)
        {
            // TODO: verify that this texture is not part of 'read'
            // TODO: if this texture is part of 'write' then return that one
            // TODO: if this texture is part of 'create' then use that pointer and register a write

            FgTextureInfo*& wti = m_texture_info_array[texture.m_index];
            FgTextureInfo*& nti = m_texture_info_array[m_texture_info_array_size];

            nti->m_resource.m_name      = wti->m_resource.m_name;
            nti->m_resource.m_pass      = m_current_passinfo;
            nti->m_resource.m_last      = nullptr;
            nti->m_resource.m_ref_count = 0;
            nti->m_texture              = wti->m_texture;
            nti->m_textureDescr         = wti->m_textureDescr;

            m_current_passinfo->m_texture_write.add(m_texture_info_array_size);
            m_texture_info_array_size += 1;

            if (texture.m_flags & IMPORTED == IMPORTED)
            {
                m_current_passinfo->m_flags |= HAS_SIDE_EFFECTS;
            }

            FgTexture ft;
            ft.m_index      = m_texture_info_array_size - 1;
            ft.m_generation = m_fg_generation;
            return ft;
        }

        FgBuffer Fg::create(const char* name, GfxBuffer* bufferObject, GfxBufferDescr* bufferDescr)
        {
            FgBufferInfo*& bi          = m_buffer_info_array[m_buffer_info_array_size];
            bi->m_resource.m_name      = name;
            bi->m_resource.m_pass      = m_current_passinfo;
            bi->m_resource.m_last      = nullptr;
            bi->m_resource.m_ref_count = 0;
            bi->m_buffer               = bufferObject;
            bi->m_bufferDescr          = bufferDescr;

            m_current_passinfo->m_buffer_create.add(m_buffer_info_array_size);
            m_buffer_info_array_size += 1;

            FgBuffer fb;
            fb.m_index      = m_buffer_info_array_size - 1;
            fb.m_generation = m_fg_generation;
            return fb;
        }

        FgBuffer Fg::read(FgBuffer buffer, FgFlags descr = s_ignore_flags)
        {
            FgBufferInfo*& rbi = m_buffer_info_array[buffer.m_index];
            FgBufferInfo*& nbi = m_buffer_info_array[m_buffer_info_array_size];

            nbi->m_resource.m_name      = rbi->m_resource.m_name;
            nbi->m_resource.m_pass      = m_current_passinfo;
            nbi->m_resource.m_last      = nullptr;
            nbi->m_resource.m_ref_count = 0;
            nbi->m_buffer               = rbi->m_buffer;
            nbi->m_bufferDescr          = rbi->m_bufferDescr;

            m_current_passinfo->m_buffer_read.add(m_buffer_info_array_size);
            m_buffer_info_array_size += 1;

            FgBuffer fb;
            fb.m_index      = m_buffer_info_array_size - 1;
            fb.m_generation = m_fg_generation;
            return fb;
        }

        FgBuffer Fg::write(FgBuffer buffer, FgFlags descr = s_ignore_flags)
        {
            FgBufferInfo*& wbi = m_buffer_info_array[buffer.m_index];
            FgBufferInfo*& nbi = m_buffer_info_array[m_buffer_info_array_size];

            nbi->m_resource.m_name      = wbi->m_resource.m_name;
            nbi->m_resource.m_pass      = m_current_passinfo;
            nbi->m_resource.m_last      = nullptr;
            nbi->m_resource.m_ref_count = 0;
            nbi->m_buffer               = wbi->m_buffer;
            nbi->m_bufferDescr          = wbi->m_bufferDescr;

            m_current_passinfo->m_buffer_write.add(m_buffer_info_array_size);
            m_buffer_info_array_size += 1;

            FgBuffer fb;
            fb.m_index      = m_buffer_info_array_size - 1;
            fb.m_generation = m_fg_generation;
            return fb;
        }

        GfxTexture*      Fg::get(FgTexture resource) const { return m_texture_info_array[resource.m_index]->m_texture; }
        GfxBuffer*       Fg::get(FgBuffer resource) const { return m_buffer_info_array[resource.m_index]->m_buffer; }
        GfxTextureDescr* Fg::getDescr(FgTexture resource) const { return m_texture_info_array[resource.m_index]->m_textureDescr; }
        GfxBufferDescr*  Fg::getDescr(FgBuffer resource) const { return m_buffer_info_array[resource.m_index]->m_bufferDescr; }
        FgFlags          Fg::getFlags(FgTexture resource) const { return m_resource_info_flags[resource.m_index]; }
        FgFlags          Fg::getFlags(FgBuffer resource) const { return m_resource_info_flags[resource.m_index]; }

        void Fg::compile(alloc_t* allocator)
        {
            // Reset ref-counts
            for (s32 i = 0; i < m_texture_info_array_size; ++i)
            {
                FgTextureInfo*& texture        = m_texture_info_array[i];
                texture->m_resource.m_ref_count = 0;
            }
            for (s32 i = 0; i < m_buffer_info_array_size; ++i)
            {
                FgBufferInfo*& buffer          = m_buffer_info_array[i];
                buffer->m_resource.m_ref_count = 0;
            }

            // Calculate ref-counts of resources used by passes
            {
                for (s32 i = 0; i < m_pass_array_size; ++i)
                {
                    FgPassInfo*& pass = m_passinfo_array[i];
                    pass->m_ref_count = static_cast<s32>(pass->m_texture_write.size()) + static_cast<s32>(pass->m_buffer_write.size());

                    // Texture and Buffer read
                    for (s32 j = pass->m_texture_read.begin; j < pass->m_texture_read.end; ++j)
                    {
                        FgTextureInfo*& consumed = m_texture_info_array[j];
                        consumed->m_resource.m_ref_count++;
                    }
                    for (s32 j = pass->m_buffer_read.begin; j < pass->m_buffer_read.end; ++j)
                    {
                        FgBufferInfo*& consumed = m_buffer_info_array[j];
                        consumed->m_resource.m_ref_count++;
                    }
                }
            }

            // Culling
            {
                // Textures
                FgResourceInfo** stack      = g_allocate_array_and_clear<FgResourceInfo*>(allocator, m_texture_info_array_size);
                s32              stack_size = 0;
                for (s32 i = 0; i < m_texture_info_array_size; ++i)
                {
                    FgTextureInfo*& texture = m_texture_info_array[i];
                    if (texture->m_resource.m_ref_count == 0)
                        stack[stack_size++] = &texture->m_resource;
                }
                for (s32 i = 0; i < m_buffer_info_array_size; ++i)
                {
                    FgBufferInfo*& buffer = m_buffer_info_array[i];
                    if (buffer->m_resource.m_ref_count == 0)
                        stack[stack_size++] = &buffer->m_resource;
                }

                while (stack_size > 0)
                {
                    FgResourceInfo* rsc      = stack[--stack_size];
                    FgPassInfo*     producer = rsc->m_pass;
                    if (producer == nullptr || ((producer->m_flags & HAS_SIDE_EFFECTS) == HAS_SIDE_EFFECTS))
                        continue;

                    ASSERT(producer->m_ref_count >= 1);
                    if (--producer->m_ref_count == 0)
                    {
                        for (s32 j = producer->m_texture_read.begin; j < producer->m_texture_read.end; ++j)
                        {
                            FgTextureInfo*& consumed = m_texture_info_array[j];
                            if (--consumed->m_resource.m_ref_count == 0)
                                stack[stack_size++] = &consumed->m_resource;
                        }
                        for (s32 j = producer->m_buffer_read.begin; j < producer->m_buffer_read.end; ++j)
                        {
                            FgBufferInfo*& consumed = m_buffer_info_array[j];
                            if (--consumed->m_resource.m_ref_count == 0)
                                stack[stack_size++] = &consumed->m_resource;
                        }
                    }
                    g_deallocate_array(allocator, stack);
                }

                // Calculate resources lifetime
                {
                    for (s32 i = 0; i < m_pass_array_size; ++i)
                    {
                        FgPassInfo* pass = m_passinfo_array[i];
                        if (pass->m_ref_count == 0)
                            continue;

                        // Created Textures and Buffers
                        for (s32 j = pass->m_texture_create.begin; j < pass->m_texture_create.end; ++j)
                            m_texture_info_array[j]->m_resource.m_pass = pass;
                        for (s32 j = pass->m_buffer_create.begin; j < pass->m_buffer_create.end; ++j)
                            m_buffer_info_array[j]->m_resource.m_pass = pass;

                        // Read Textures and Buffers
                        for (s32 j = pass->m_texture_read.begin; j < pass->m_texture_read.end; ++j)
                            m_texture_info_array[j]->m_resource.m_last = pass;
                        for (s32 j = pass->m_buffer_read.begin; j < pass->m_buffer_read.end; ++j)
                            m_buffer_info_array[j]->m_resource.m_last = pass;

                        // Written Textures and Buffers
                        for (s32 j = pass->m_texture_write.begin; j < pass->m_texture_write.end; ++j)
                            m_texture_info_array[j]->m_resource.m_last = pass;
                        for (s32 j = pass->m_buffer_write.begin; j < pass->m_buffer_write.end; ++j)
                            m_buffer_info_array[j]->m_resource.m_last = pass;
                    }
                }
            }
        }

        void Fg::execute(GfxRenderContext* ctxt)
        {
            for (s32 i = 0; i < m_pass_array_size; ++i)
            {
                FgPassInfo* pass = m_passinfo_array[i];
                if (pass->m_ref_count == 0 && !((pass->m_flags & HAS_SIDE_EFFECTS) == HAS_SIDE_EFFECTS))
                    continue;

                // Create transient resources
                for (s32 j = pass->m_texture_create.begin; j < pass->m_texture_create.end; ++j)
                {
                    FgTextureInfo*& texture = m_texture_info_array[j];
                    if (texture->m_resource.m_last == pass)
                        m_create_texture.Call(ctxt, texture->m_texture, texture->m_textureDescr);
                }
                for (s32 j = pass->m_buffer_create.begin; j < pass->m_buffer_create.end; ++j)
                {
                    FgBufferInfo*& buffer = m_buffer_info_array[j];
                    if (buffer->m_resource.m_last == pass)
                        m_create_buffer.Call(ctxt, buffer->m_buffer, buffer->m_bufferDescr);
                }

                // Pre-read and pre-write
                for (s32 j = pass->m_texture_read.begin; j < pass->m_texture_read.end; ++j)
                {
                    if (m_resource_info_flags[j].m_descr != s_ignore_flags.m_descr)
                        m_preread_texture.Call(ctxt, m_texture_info_array[j]->m_texture, m_resource_info_flags[j]);
                }
                for (s32 j = pass->m_buffer_read.begin; j < pass->m_buffer_read.end; ++j)
                {
                    if (m_resource_info_flags[j].m_descr != s_ignore_flags.m_descr)
                        m_preread_buffer.Call(ctxt, m_buffer_info_array[j]->m_buffer, m_resource_info_flags[j]);
                }
                for (s32 j = pass->m_texture_write.begin; j < pass->m_texture_write.end; ++j)
                {
                    if (m_resource_info_flags[j].m_descr != s_ignore_flags.m_descr)
                        m_prewrite_texture.Call(ctxt, m_texture_info_array[j]->m_texture, m_resource_info_flags[j]);
                }
                for (s32 j = pass->m_buffer_write.begin; j < pass->m_buffer_write.end; ++j)
                {
                    if (m_resource_info_flags[j].m_descr != s_ignore_flags.m_descr)
                        m_prewrite_buffer.Call(ctxt, m_buffer_info_array[j]->m_buffer, m_resource_info_flags[j]);
                }

                // Execute pass
                pass->m_execute_fn.Call(*this, ctxt);

                // Destroy transient resources
                for (s32 j = 0; j < m_texture_info_array_size; ++j)
                {
                    FgTextureInfo*& texture = m_texture_info_array[j];
                    if (texture->m_resource.m_last == pass /* && texture is transient*/)
                        m_destroy_texture.Call(ctxt, texture->m_texture);
                }
                for (s32 j = 0; j < m_buffer_info_array_size; ++j)
                {
                    FgBufferInfo*& buffer = m_buffer_info_array[j];
                    if (buffer->m_resource.m_last == pass /* && buffer is transient*/)
                        m_destroy_buffer.Call(ctxt, buffer->m_buffer);
                }
            }
        }

    } // namespace nframegraph
} // namespace ncore
