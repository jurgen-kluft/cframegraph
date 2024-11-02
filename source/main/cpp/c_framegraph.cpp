#include "callocator/c_allocator_ocs.h"
#include "cframegraph/c_framegraph.h"

namespace ncore
{
    namespace nframegraph
    {
        struct FgRange
        {
            s32  size() const { return end - begin; }
            void reset(s32 index) { begin = end = index; }
            void add(s32 index) { end = index + 1; }
            s32  begin;
            s32  end;
        };

        enum EFlags
        {
            IMPORTED         = 1,
            HAS_SIDE_EFFECTS = 2,
        };

        struct FgPassInfo
        {
            const char*                              m_name;
            callback_t<void, Fg&, GfxRenderContext*> m_execute_fn;
            u16                                      m_flags;
            u16                                      m_array_index;    // the index into the pass array where this pass is stored
            s32                                      m_ref_count;      // the number of resources that are written by this pass
            FgRange                                  m_texture_create; // the begin and end index into the 'create' texture array
            FgRange                                  m_texture_read;   // the begin and end index into the 'read' texture array
            FgRange                                  m_texture_write;  // the begin and end index into the 'write' texture array
            FgRange                                  m_buffer_create;  // the begin and end index into the 'create' buffer array
            FgRange                                  m_buffer_read;    // the begin and end index into the 'read' buffer array
            FgRange                                  m_buffer_write;   // the begin and end index into the 'write' buffer array
        };

        struct FgResourceInfo
        {
            const char* m_name;
            FgPass      m_pass;
            FgPass      m_last;
            u32         m_flags;
            s32         m_ref_count;
        };

        struct FgTextureInfo
        {
            FgResourceInfo   m_resource;
            GfxTexture*      m_texture;
            GfxTextureDescr* m_textureDescr;
        };

        struct FgBufferInfo
        {
            FgResourceInfo  m_resource;
            GfxBuffer*      m_buffer;
            GfxBufferDescr* m_bufferDescr;
        };

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
            pi->m_texture_create.reset(m_textureinfo_cursor[FgCreate]);
            pi->m_texture_read.reset(m_textureinfo_cursor[FgRead]);
            pi->m_texture_write.reset(m_textureinfo_cursor[FgWrite]);
            pi->m_buffer_create.reset(m_bufferinfo_cursor[FgCreate]);
            pi->m_buffer_read.reset(m_bufferinfo_cursor[FgRead]);
            pi->m_buffer_write.reset(m_bufferinfo_cursor[FgWrite]);
            m_pass_array_size++;
        }

        FgTexture Fg::import(const char* name, GfxTexture* resource, GfxTextureDescr* descr)
        {
            // ...
            // ti->m_resource.m_flags     = IMPORTED;
        }

        FgBuffer Fg::import(const char* name, GfxBuffer* resource, GfxBufferDescr* descr)
        {
            // ...
            // ti->m_resource.m_flags     = IMPORTED;
        }

        FgTexture Fg::create(const char* name, GfxTexture* textureObject, GfxTextureDescr* textureDescr)
        {
            FgTextureInfo* ti          = &m_textureinfo_array[m_textureinfo_cursor[FgMain]];
            ti->m_resource.m_name      = name;
            ti->m_resource.m_pass      = m_current_passinfo;
            ti->m_resource.m_last      = nullptr;
            ti->m_resource.m_ref_count = 0;
            ti->m_resource.m_flags     = 0;
            ti->m_texture              = textureObject;
            ti->m_textureDescr         = textureDescr;

            m_textureinfo_create_array[m_textureinfo_cursor[FgCreate]] = m_textureinfo_cursor[FgMain];
            m_current_passinfo->m_texture_create.add(m_textureinfo_cursor[FgCreate]);

            m_textureinfo_cursor[FgCreate] += 1;
            m_textureinfo_cursor[FgMain] += 1;

            FgTexture texture;
            texture.index = m_textureinfo_cursor[FgCreate] - 1;
            return texture;
        }

        FgTexture Fg::read(FgTexture texture, FgFlags descr = s_flags_ignored)
        {
            ASSERT(is_valid(texture));
            ASSERT(!pass_contains(m_current_passinfo, FgWrite, texture));
            ASSERT(!pass_contains(m_current_passinfo, FgCreate, texture));

            if (!pass_contains(m_current_passinfo, FgRead, texture))
            {
                m_textureinfo_flags[texture.index]                     = descr;
                m_textureinfo_read_array[m_textureinfo_cursor[FgRead]] = texture.index;
                m_current_passinfo->m_texture_read.add(m_textureinfo_cursor[FgRead]);
                m_textureinfo_cursor[FgRead] += 1;
            }
            return texture;
        }

        FgTexture Fg::write(FgTexture texture, FgFlags descr = s_flags_ignored)
        {
            ASSERT(is_valid(texture));

            /*
                assert(m_frameGraph.isValid(id));
                if (m_frameGraph._getResourceEntry(id).isImported()) setSideEffect();

                if (m_passNode.creates(id)) {
                    return m_passNode._write(id, flags);
                } else {
                    // Writing to a texture produces a renamed handle.
                    // This allows us to catch errors when resources are modified in
                    // undefined order (when same resource is written by different passes).
                    // Renaming resources enforces a specific execution order of the render
                    // passes.
                    m_passNode._read(id, kFlagsIgnored);
                    return m_passNode._write(m_frameGraph._clone(id), flags);
                }
            */
            ASSERT(!pass_contains(m_current_passinfo, FgRead, texture));

            if (pass_contains(m_current_passinfo, FgCreate, texture))
            {
                FgTextureInfo const* si                                  = &m_textureinfo_array[texture.index];
                m_textureinfo_write_array[m_textureinfo_cursor[FgWrite]] = texture.index;
                m_current_passinfo->m_texture_write.add(m_textureinfo_cursor[FgWrite]);
                m_textureinfo_cursor[FgWrite] += 1;
                m_current_passinfo->m_flags |= si->m_resource.m_flags & IMPORTED;
                return texture;
            }
            else
            {
                read(texture, s_flags_ignored);

                // Clone FgTextureInfo
                FgTextureInfo const* si    = &m_textureinfo_array[texture.index];
                FgTextureInfo*       ti    = &m_textureinfo_array[m_textureinfo_cursor[FgMain]];
                ti->m_resource.m_name      = si->m_resource.m_name;
                ti->m_resource.m_pass      = m_current_passinfo;
                ti->m_resource.m_last      = nullptr;
                ti->m_resource.m_ref_count = 0;
                ti->m_resource.m_flags     = si->m_resource.m_flags;
                ti->m_texture              = si->m_texture;
                ti->m_textureDescr         = si->m_textureDescr;

                m_textureinfo_write_array[m_textureinfo_cursor[FgWrite]] = m_textureinfo_cursor[FgMain];
                m_current_passinfo->m_texture_write.add(m_textureinfo_cursor[FgWrite]);
                m_textureinfo_cursor[FgWrite] += 1;

                FgTexture texture;
                texture.index = m_textureinfo_cursor[FgWrite] - 1;
                return texture;
            }

            return s_invalid_texture;
        }

        FgBuffer Fg::create(const char* name, GfxBuffer* bufferObject, GfxBufferDescr* bufferDescr)
        {
            FgBufferInfo* bi           = &m_bufferinfo_array[m_bufferinfo_cursor[FgMain]];
            bi->m_resource.m_name      = name;
            bi->m_resource.m_pass      = m_current_passinfo;
            bi->m_resource.m_last      = nullptr;
            bi->m_resource.m_ref_count = 0;
            bi->m_buffer               = bufferObject;
            bi->m_bufferDescr          = bufferDescr;

            m_bufferinfo_create_array[m_bufferinfo_cursor[FgCreate]] = m_bufferinfo_cursor[FgMain];
            m_current_passinfo->m_buffer_create.add(m_bufferinfo_cursor[FgCreate]);

            m_bufferinfo_cursor[FgCreate] += 1;
            m_bufferinfo_cursor[FgMain] += 1;

            FgBuffer fb;
            fb.index = m_bufferinfo_cursor[FgCreate] - 1;
            return fb;
        }

        FgBuffer Fg::read(FgBuffer buffer, FgFlags descr = s_flags_ignored)
        {
            ASSERT(is_valid(buffer));
            ASSERT(!pass_contains(m_current_passinfo, FgWrite, buffer));
            ASSERT(!pass_contains(m_current_passinfo, FgCreate, buffer));

            if (!pass_contains(m_current_passinfo, FgRead, buffer))
            {
                m_bufferinfo_flags[buffer.index]                     = descr;
                m_bufferinfo_read_array[m_bufferinfo_cursor[FgRead]] = buffer.index;
                m_current_passinfo->m_buffer_read.add(m_bufferinfo_cursor[FgRead]);
                m_bufferinfo_cursor[FgRead] += 1;
            }
            return buffer;
        }

        FgBuffer Fg::write(FgBuffer buffer, FgFlags descr = s_flags_ignored)
        {
            ASSERT(is_valid(buffer));

            if (pass_contains(m_current_passinfo, FgCreate, buffer))
            {
                FgBufferInfo const* si                                 = &m_bufferinfo_array[buffer.index];
                m_bufferinfo_write_array[m_bufferinfo_cursor[FgWrite]] = buffer.index;
                m_current_passinfo->m_buffer_write.add(m_bufferinfo_cursor[FgWrite]);
                m_bufferinfo_cursor[FgWrite] += 1;
                m_current_passinfo->m_flags |= si->m_resource.m_flags & IMPORTED;
                return buffer;
            }
            else
            {
                read(buffer, s_flags_ignored);

                // Clone FgBufferInfo
                FgBufferInfo const* si     = &m_bufferinfo_array[buffer.index];
                FgBufferInfo*       bi     = &m_bufferinfo_array[m_bufferinfo_cursor[FgMain]];
                bi->m_resource.m_name      = si->m_resource.m_name;
                bi->m_resource.m_pass      = m_current_passinfo;
                bi->m_resource.m_last      = nullptr;
                bi->m_resource.m_ref_count = 0;
                bi->m_resource.m_flags     = si->m_resource.m_flags;
                bi->m_buffer               = si->m_buffer;
                bi->m_bufferDescr          = si->m_bufferDescr;

                m_bufferinfo_write_array[m_bufferinfo_cursor[FgWrite]] = m_bufferinfo_cursor[FgMain];
                m_current_passinfo->m_buffer_write.add(m_bufferinfo_cursor[FgWrite]);
                m_bufferinfo_cursor[FgWrite] += 1;

                FgBuffer buffer;
                buffer.index = m_bufferinfo_cursor[FgWrite] - 1;
                return buffer;
            }

            return s_invalid_buffer;
        }

        GfxTexture*      Fg::get(FgTexture resource) const { return m_textureinfo_array[resource.index].m_texture; }
        GfxBuffer*       Fg::get(FgBuffer resource) const { return m_bufferinfo_array[resource.index].m_buffer; }
        GfxTextureDescr* Fg::getDescr(FgTexture resource) const { return m_textureinfo_array[resource.index].m_textureDescr; }
        GfxBufferDescr*  Fg::getDescr(FgBuffer resource) const { return m_bufferinfo_array[resource.index].m_bufferDescr; }
        FgFlags          Fg::getFlags(FgTexture resource) const { return m_textureinfo_flags[resource.index]; }
        FgFlags          Fg::getFlags(FgBuffer resource) const { return m_bufferinfo_flags[resource.index]; }

        void Fg::compile(alloc_t* allocator)
        {
            // Reset ref-counts
            for (s32 i = 0; i < m_textureinfo_cursor[FgCreate]; ++i)
            {
                FgTextureInfo* texture          = &m_textureinfo_array[i];
                texture->m_resource.m_ref_count = 0;
            }
            for (s32 i = 0; i < m_bufferinfo_cursor[FgCreate]; ++i)
            {
                FgBufferInfo* buffer           = &m_bufferinfo_array[i];
                buffer->m_resource.m_ref_count = 0;
            }

            // Calculate ref-counts of resources used by passes
            {
                for (s32 i = 0; i < m_pass_array_size; ++i)
                {
                    FgPassInfo* pass  = m_passinfo_array[i];
                    pass->m_ref_count = static_cast<s32>(pass->m_texture_write.size()) + static_cast<s32>(pass->m_buffer_write.size());

                    // Texture and Buffer read
                    for (s32 j = pass->m_texture_read.begin; j < pass->m_texture_read.end; ++j)
                    {
                        FgIndex const  index    = m_textureinfo_read_array[j];
                        FgTextureInfo* consumed = &m_textureinfo_array[index];
                        consumed->m_resource.m_ref_count++;
                    }
                    for (s32 j = pass->m_buffer_read.begin; j < pass->m_buffer_read.end; ++j)
                    {
                        FgIndex const index    = m_bufferinfo_read_array[j];
                        FgBufferInfo* consumed = &m_bufferinfo_array[index];
                        consumed->m_resource.m_ref_count++;
                    }
                }
            }

            // Culling
            {
                // Textures
                FgResourceInfo** stack      = g_allocate_array_and_clear<FgResourceInfo*>(allocator, m_textureinfo_cursor[FgCreate]);
                s32              stack_size = 0;
                for (s32 i = 0; i < m_textureinfo_cursor[FgCreate]; ++i)
                {
                    FgTextureInfo* texture = &m_textureinfo_array[i];
                    if (texture->m_resource.m_ref_count == 0)
                        stack[stack_size++] = &texture->m_resource;
                }
                for (s32 i = 0; i < m_bufferinfo_cursor[FgCreate]; ++i)
                {
                    FgBufferInfo* buffer = &m_bufferinfo_array[i];
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
                            FgIndex const  index    = m_textureinfo_read_array[j];
                            FgTextureInfo* consumed = &m_textureinfo_array[index];
                            if (--consumed->m_resource.m_ref_count == 0)
                                stack[stack_size++] = &consumed->m_resource;
                        }
                        for (s32 j = producer->m_buffer_read.begin; j < producer->m_buffer_read.end; ++j)
                        {
                            FgIndex const index    = m_bufferinfo_read_array[j];
                            FgBufferInfo* consumed = &m_bufferinfo_array[index];
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
                        {
                            FgIndex const index                          = m_textureinfo_create_array[j];
                            m_textureinfo_array[index].m_resource.m_pass = pass;
                        }
                        for (s32 j = pass->m_buffer_create.begin; j < pass->m_buffer_create.end; ++j)
                        {
                            FgIndex const index                         = m_bufferinfo_create_array[j];
                            m_bufferinfo_array[index].m_resource.m_pass = pass;
                        }

                        // Read Textures and Buffers
                        for (s32 j = pass->m_texture_read.begin; j < pass->m_texture_read.end; ++j)
                        {
                            FgIndex const index                          = m_textureinfo_read_array[j];
                            m_textureinfo_array[index].m_resource.m_last = pass;
                        }
                        for (s32 j = pass->m_buffer_read.begin; j < pass->m_buffer_read.end; ++j)
                        {
                            FgIndex const index                         = m_bufferinfo_read_array[j];
                            m_bufferinfo_array[index].m_resource.m_last = pass;
                        }

                        // Written Textures and Buffers
                        for (s32 j = pass->m_texture_write.begin; j < pass->m_texture_write.end; ++j)
                        {
                            FgIndex const index                          = m_textureinfo_write_array[j];
                            m_textureinfo_array[index].m_resource.m_last = pass;
                        }
                        for (s32 j = pass->m_buffer_write.begin; j < pass->m_buffer_write.end; ++j)
                        {
                            FgIndex const index                         = m_bufferinfo_write_array[j];
                            m_bufferinfo_array[index].m_resource.m_last = pass;
                        }
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
                    FgIndex        index   = m_textureinfo_create_array[j];
                    FgTextureInfo* texture = &m_textureinfo_array[index];
                    if (texture->m_resource.m_last == pass)
                        m_create_texture.Call(ctxt, texture->m_texture, texture->m_textureDescr);
                }
                for (s32 j = pass->m_buffer_create.begin; j < pass->m_buffer_create.end; ++j)
                {
                    FgIndex       index  = m_bufferinfo_create_array[j];
                    FgBufferInfo* buffer = &m_bufferinfo_array[index];
                    if (buffer->m_resource.m_last == pass)
                        m_create_buffer.Call(ctxt, buffer->m_buffer, buffer->m_bufferDescr);
                }

                // Pre-read and pre-write
                for (s32 j = pass->m_texture_read.begin; j < pass->m_texture_read.end; ++j)
                {
                    if (m_textureinfo_flags[j].m_descr != s_flags_ignored.m_descr)
                        m_preread_texture.Call(ctxt, m_textureinfo_array[j].m_texture, m_textureinfo_flags[j]);
                }
                for (s32 j = pass->m_buffer_read.begin; j < pass->m_buffer_read.end; ++j)
                {
                    if (m_bufferinfo_flags[j].m_descr != s_flags_ignored.m_descr)
                        m_preread_buffer.Call(ctxt, m_bufferinfo_array[j].m_buffer, m_bufferinfo_flags[j]);
                }
                for (s32 j = pass->m_texture_write.begin; j < pass->m_texture_write.end; ++j)
                {
                    if (m_textureinfo_flags[j].m_descr != s_flags_ignored.m_descr)
                        m_prewrite_texture.Call(ctxt, m_textureinfo_array[j].m_texture, m_textureinfo_flags[j]);
                }
                for (s32 j = pass->m_buffer_write.begin; j < pass->m_buffer_write.end; ++j)
                {
                    if (m_bufferinfo_flags[j].m_descr != s_flags_ignored.m_descr)
                        m_prewrite_buffer.Call(ctxt, m_bufferinfo_array[j].m_buffer, m_bufferinfo_flags[j]);
                }

                // Execute pass
                pass->m_execute_fn.Call(*this, ctxt);

                // Destroy transient resources
                for (s32 j = 0; j < m_textureinfo_cursor[FgMain]; ++j)
                {
                    FgTextureInfo* texture = &m_textureinfo_array[j];
                    if (texture->m_resource.m_last == pass /* && texture is transient*/)
                        m_destroy_texture.Call(ctxt, texture->m_texture);
                }
                for (s32 j = 0; j < m_bufferinfo_cursor[FgMain]; ++j)
                {
                    FgBufferInfo* buffer = &m_bufferinfo_array[j];
                    if (buffer->m_resource.m_last == pass /* && buffer is transient*/)
                        m_destroy_buffer.Call(ctxt, buffer->m_buffer);
                }
            }
        }

        bool Fg::pass_contains(FgPass pass, FgType type, FgTexture resource) const
        {
            FgIndex* array = nullptr;
            FgRange* range = nullptr;
            switch (type)
            {
                case FgCreate:
                    array = m_textureinfo_create_array;
                    range = &pass->m_texture_create;
                    break;
                case FgRead:
                    array = m_textureinfo_read_array;
                    range = &pass->m_texture_read;
                    break;
                case FgWrite:
                    array = m_textureinfo_write_array;
                    range = &pass->m_texture_write;
                    break;
                default: ASSERT(false); break;
            }
            for (s32 i = range->begin; i < range->end; ++i)
            {
                if (array[i] == resource.index)
                    return true;
            }
            return false;
        }

        bool Fg::pass_contains(FgPass pass, FgType type, FgBuffer resource) const
        {
            FgIndex* array = nullptr;
            FgRange* range = nullptr;
            switch (type)
            {
                case FgCreate:
                    array = m_bufferinfo_create_array;
                    range = &pass->m_buffer_create;
                    break;
                case FgRead:
                    array = m_bufferinfo_read_array;
                    range = &pass->m_buffer_read;
                    break;
                case FgWrite:
                    array = m_bufferinfo_write_array;
                    range = &pass->m_buffer_write;
                    break;
                default: ASSERT(false); break;
            }
            for (s32 i = range->begin; i < range->end; ++i)
            {
                if (array[i] == resource.index)
                    return true;
            }
            return false;
        }

    } // namespace nframegraph
} // namespace ncore
