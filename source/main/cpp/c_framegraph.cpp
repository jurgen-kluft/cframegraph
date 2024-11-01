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

        FgPass Fg::add_pass(const char* name, callback_t<void, Fg&, void*> execute)
        {
            // ...
        }

        GfxTexture*      Fg::get(FgTexture resource) const { return m_texture_info_array[resource.m_index].m_texture; }
        GfxBuffer*       Fg::get(FgBuffer resource) const { return m_buffer_info_array[resource.m_index].m_buffer; }
        GfxTextureDescr* Fg::getDescr(FgTexture resource) const { return m_texture_info_array[resource.m_index].m_textureDescr; }
        GfxBufferDescr*  Fg::getDescr(FgBuffer resource) const { return m_buffer_info_array[resource.m_index].m_bufferDescr; }
        FgFlags          Fg::getFlags(FgTexture resource) const { return m_resource_info_flags[resource.m_index]; }
        FgFlags          Fg::getFlags(FgBuffer resource) const { return m_resource_info_flags[resource.m_index]; }

        void Fg::compile(alloc_t* allocator)
        {
            // Based on the order on how resources are created and used (read/write) we can figure out the
            // pass order and dependency.
            // Each pass knows the textures and buffers that are created, read and written.
            // Every texture and buffer knows their history (previous usage), so we can easily figure out
            // the pass dependency.
            // With the above information we can create the Directed Acyclic Graph (DAG) of the passes and
            // trim the passes and resources that are not needed.

            for (s32 i = 0; i < m_texture_info_array_size; ++i)
            {
                FgTextureInfo& texture         = m_texture_info_array[i];
                texture.m_resource.m_ref_count = 0;
            }
            for (s32 i = 0; i < m_buffer_info_array_size; ++i)
            {
                FgBufferInfo& buffer          = m_buffer_info_array[i];
                buffer.m_resource.m_ref_count = 0;
            }

            // -- Calculate ref-counts of resources used by passes:
            {
                for (s32 i = 0; i < m_pass_array_size; ++i)
                {
                    FgPassInfo& pass = m_pass_info_array[i];
                    pass.m_ref_count = static_cast<s32>(pass.m_texture_write.size()) + static_cast<s32>(pass.m_buffer_write.size());

                    // Texture and Buffer read
                    for (s32 j = pass.m_texture_read.begin; j < pass.m_texture_read.end; ++j)
                    {
                        FgTextureInfo& consumed = m_texture_info_array[j];
                        consumed.m_resource.m_ref_count++;
                    }
                    for (s32 j = pass.m_buffer_read.begin; j < pass.m_buffer_read.end; ++j)
                    {
                        FgBufferInfo& consumed = m_buffer_info_array[j];
                        consumed.m_resource.m_ref_count++;
                    }
                }
            }

            // -- Culling:
            {
                // -- Textures
                FgResourceInfo** stack      = g_allocate_array_and_clear<FgResourceInfo*>(allocator, m_texture_info_array_size);
                s32              stack_size = 0;
                for (s32 i = 0; i < m_texture_info_array_size; ++i)
                {
                    FgTextureInfo& texture = m_texture_info_array[i];
                    if (texture.m_resource.m_ref_count == 0)
                        stack[stack_size++] = &texture.m_resource;
                }
                for (s32 i = 0; i < m_buffer_info_array_size; ++i)
                {
                    FgBufferInfo& buffer = m_buffer_info_array[i];
                    if (buffer.m_resource.m_ref_count == 0)
                        stack[stack_size++] = &buffer.m_resource;
                }

                while (stack_size > 0)
                {
                    FgResourceInfo* rsc      = stack[--stack_size];
                    FgPassInfo*     producer = rsc->m_pass;
                    if (producer == nullptr || producer->m_has_side_effects)
                        continue;

                    ASSERT(producer->m_ref_count >= 1);
                    if (--producer->m_ref_count == 0)
                    {
                        for (s32 j = producer->m_texture_read.begin; j < producer->m_texture_read.end; ++j)
                        {
                            FgTextureInfo& consumed = m_texture_info_array[j];
                            if (--consumed.m_resource.m_ref_count == 0)
                                stack[stack_size++] = &consumed.m_resource;
                        }
                        for (s32 j = producer->m_buffer_read.begin; j < producer->m_buffer_read.end; ++j)
                        {
                            FgBufferInfo& consumed = m_buffer_info_array[j];
                            if (--consumed.m_resource.m_ref_count == 0)
                                stack[stack_size++] = &consumed.m_resource;
                        }
                    }
                    g_deallocate_array(allocator, stack);
                }

                // -- Calculate resources lifetime:
                {
                    for (s32 i = 0; i < m_pass_array_size; ++i)
                    {
                        FgPassInfo& pass = m_pass_info_array[i];
                        if (pass.m_ref_count == 0)
                            continue;

                        // Created Textures and Buffers
                        for (s32 j = pass.m_texture_create.begin; j < pass.m_texture_create.end; ++j)
                            m_texture_info_array[j].m_resource.m_pass = &pass;
                        for (s32 j = pass.m_buffer_create.begin; j < pass.m_buffer_create.end; ++j)
                            m_buffer_info_array[j].m_resource.m_pass = &pass;

                        // Read Textures and Buffers
                        for (s32 j = pass.m_texture_read.begin; j < pass.m_texture_read.end; ++j)
                            m_texture_info_array[j].m_resource.m_last = &pass;
                        for (s32 j = pass.m_buffer_read.begin; j < pass.m_buffer_read.end; ++j)
                            m_buffer_info_array[j].m_resource.m_last = &pass;

                        // Written Textures and Buffers
                        for (s32 j = pass.m_texture_write.begin; j < pass.m_texture_write.end; ++j)
                            m_texture_info_array[j].m_resource.m_last = &pass;
                        for (s32 j = pass.m_buffer_write.begin; j < pass.m_buffer_write.end; ++j)
                            m_buffer_info_array[j].m_resource.m_last = &pass;
                    }
                }
            }
        }

        void Fg::execute(GfxRenderContext* ctxt)
        {
            for (s32 i = 0; i < m_pass_array_size; ++i)
            {
                FgPassInfo& pass = m_pass_info_array[i];
                if (pass.m_ref_count == 0 && !pass.m_has_side_effects)
                    continue;

                // Create transient resources
                for (s32 j = pass.m_texture_create.begin; j < pass.m_texture_create.end; ++j)
                {
                    FgTextureInfo& texture = m_texture_info_array[j];
                    if (texture.m_resource.m_last == &pass)
                        m_create_texture.Call(ctxt, texture.m_texture, texture.m_textureDescr);
                }
                for (s32 j = pass.m_buffer_create.begin; j < pass.m_buffer_create.end; ++j)
                {
                    FgBufferInfo& buffer = m_buffer_info_array[j];
                    if (buffer.m_resource.m_last == &pass)
                        m_create_buffer.Call(ctxt, buffer.m_buffer, buffer.m_bufferDescr);
                }

                // Pre-read and pre-write
                for (s32 j = pass.m_texture_read.begin; j < pass.m_texture_read.end; ++j)
                {
                    if (m_resource_info_flags[j].m_descr != s_ignore_flags.m_descr)
                        m_preread_texture.Call(ctxt, m_texture_info_array[j].m_texture, m_resource_info_flags[j]);
                }
                for (s32 j = pass.m_buffer_read.begin; j < pass.m_buffer_read.end; ++j)
                {
                    if (m_resource_info_flags[j].m_descr != s_ignore_flags.m_descr)
                        m_preread_buffer.Call(ctxt, m_buffer_info_array[j].m_buffer, m_resource_info_flags[j]);
                }
                for (s32 j = pass.m_texture_write.begin; j < pass.m_texture_write.end; ++j)
                {
                    if (m_resource_info_flags[j].m_descr != s_ignore_flags.m_descr)
                        m_prewrite_texture.Call(ctxt, m_texture_info_array[j].m_texture, m_resource_info_flags[j]);
                }
                for (s32 j = pass.m_buffer_write.begin; j < pass.m_buffer_write.end; ++j)
                {
                    if (m_resource_info_flags[j].m_descr != s_ignore_flags.m_descr)
                        m_prewrite_buffer.Call(ctxt, m_buffer_info_array[j].m_buffer, m_resource_info_flags[j]);
                }

                // Execute pass
                pass.m_execute_fn.Call(*this, ctxt);

                // Destroy transient resources
                for (s32 j = 0; j < m_texture_info_array_size; ++j)
                {
                    FgTextureInfo& texture = m_texture_info_array[j];
                    if (texture.m_resource.m_last == &pass /* && texture is transient*/)
                        m_destroy_texture.Call(ctxt, texture.m_texture);
                }
                for (s32 j = 0; j < m_buffer_info_array_size; ++j)
                {
                    FgBufferInfo& buffer = m_buffer_info_array[j];
                    if (buffer.m_resource.m_last == &pass /* && buffer is transient*/)
                        m_destroy_buffer.Call(ctxt, buffer.m_buffer);
                }
            }
        }

    } // namespace nframegraph
} // namespace ncore
