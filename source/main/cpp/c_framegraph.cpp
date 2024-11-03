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
            const char* m_name;
            FgExecuteFn m_execute_fn;
            u16         m_flags;
            u16         m_array_index;    // the index into the pass array where this pass is stored
            s32         m_ref_count;      // the number of resources that are written by this pass
            FgRange     m_texture_create; // the begin and end index into the 'create' texture array
            FgRange     m_texture_read;   // the begin and end index into the 'read' texture array
            FgRange     m_texture_write;  // the begin and end index into the 'write' texture array
            FgRange     m_buffer_create;  // the begin and end index into the 'create' buffer array
            FgRange     m_buffer_read;    // the begin and end index into the 'read' buffer array
            FgRange     m_buffer_write;   // the begin and end index into the 'write' buffer array
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

        typedef s8          FgType;
        static const FgType FgMain   = 0;
        static const FgType FgCreate = 1;
        static const FgType FgRead   = 2;
        static const FgType FgWrite  = 3;

        struct Fg
        {
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            bool is_valid(FgTexture resource) const;
            bool is_valid(FgBuffer resource) const;
            bool pass_contains(FgPass pass, FgType type, FgTexture resource) const;
            bool pass_contains(FgPass pass, FgType type, FgBuffer resource) const;

            alloc_t*       m_allocator;
            u32            m_resource_array_capacity; // maximum number of resources
            u32            m_resource_generation;     // ID to make resources unique and recognize invalid resources
            u32            m_pass_array_capacity;
            u32            m_pass_array_size;
            FgPassInfo*    m_passinfo_array;
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

        Fg* fg_setup(alloc_t* allocator, u32 resource_capacity, u32 pass_capacity)
        {
            Fg* fg = g_allocate_and_clear<Fg>(allocator);

            fg->m_allocator = allocator;

            fg->m_resource_array_capacity = resource_capacity;
            fg->m_resource_generation     = 0;

            fg->m_pass_array_size     = 0;
            fg->m_pass_array_capacity = pass_capacity;
            fg->m_passinfo_array      = g_allocate_array_and_clear<FgPassInfo>(allocator, pass_capacity);

            fg->m_textureinfo_array = g_allocate_array_and_clear<FgTextureInfo>(allocator, resource_capacity);
            fg->m_textureinfo_flags = g_allocate_array_and_clear<FgFlags>(allocator, resource_capacity);

            fg->m_textureinfo_create_array = g_allocate_array_and_clear<FgIndex>(allocator, resource_capacity);
            fg->m_textureinfo_read_array   = g_allocate_array_and_clear<FgIndex>(allocator, resource_capacity);
            fg->m_textureinfo_write_array  = g_allocate_array_and_clear<FgIndex>(allocator, resource_capacity);

            fg->m_bufferinfo_array = g_allocate_array_and_clear<FgBufferInfo>(allocator, resource_capacity);
            fg->m_bufferinfo_flags = g_allocate_array_and_clear<FgFlags>(allocator, resource_capacity);

            fg->m_bufferinfo_create_array = g_allocate_array_and_clear<FgIndex>(allocator, resource_capacity);
            fg->m_bufferinfo_read_array   = g_allocate_array_and_clear<FgIndex>(allocator, resource_capacity);
            fg->m_bufferinfo_write_array  = g_allocate_array_and_clear<FgIndex>(allocator, resource_capacity);

            return fg;
        }

        void fg_teardown(Fg*& fg)
        {
            g_deallocate_array(fg->m_allocator, fg->m_passinfo_array);
            g_deallocate_array(fg->m_allocator, fg->m_textureinfo_array);
            g_deallocate_array(fg->m_allocator, fg->m_textureinfo_flags);
            g_deallocate_array(fg->m_allocator, fg->m_textureinfo_create_array);
            g_deallocate_array(fg->m_allocator, fg->m_textureinfo_read_array);
            g_deallocate_array(fg->m_allocator, fg->m_textureinfo_write_array);
            g_deallocate_array(fg->m_allocator, fg->m_bufferinfo_array);
            g_deallocate_array(fg->m_allocator, fg->m_bufferinfo_flags);
            g_deallocate_array(fg->m_allocator, fg->m_bufferinfo_create_array);
            g_deallocate_array(fg->m_allocator, fg->m_bufferinfo_read_array);
            g_deallocate_array(fg->m_allocator, fg->m_bufferinfo_write_array);

            g_deallocate(fg->m_allocator, fg);
            fg = nullptr;
        }

        void fg_set_create_texture(Fg* fg, callback_t<void, GfxRenderContext*, GfxTexture*, GfxTextureDescr*> fn) { fg->m_create_texture = fn; }
        void fg_set_preread_texture(Fg* fg, callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags> fn) { fg->m_preread_texture = fn; }
        void fg_set_prewrite_texture(Fg* fg, callback_t<void, GfxRenderContext*, GfxTexture*, FgFlags> fn) { fg->m_prewrite_texture = fn; }
        void fg_set_destroy_texture(Fg* fg, callback_t<void, GfxRenderContext*, GfxTexture*> fn) { fg->m_destroy_texture = fn; }

        void fg_set_create_buffer(Fg* fg, callback_t<void, GfxRenderContext*, GfxBuffer*, GfxBufferDescr*> fn) { fg->m_create_buffer = fn; }
        void fg_set_preread_buffer(Fg* fg, callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags> fn) { fg->m_preread_buffer = fn; }
        void fg_set_prewrite_buffer(Fg* fg, callback_t<void, GfxRenderContext*, GfxBuffer*, FgFlags> fn) { fg->m_prewrite_buffer = fn; }
        void fg_set_destroy_buffer(Fg* fg, callback_t<void, GfxRenderContext*, GfxBuffer*> fn) { fg->m_destroy_buffer = fn; }

        FgPass fg_add_pass(Fg* fg, const char* name, FgExecuteFn execute)
        {
            FgPassInfo* pi    = &fg->m_passinfo_array[fg->m_pass_array_size];
            pi->m_name        = name;
            pi->m_execute_fn  = execute;
            pi->m_flags       = 0;
            pi->m_array_index = fg->m_pass_array_size;
            pi->m_ref_count   = 0;
            pi->m_texture_create.reset(fg->m_textureinfo_cursor[FgCreate]);
            pi->m_texture_read.reset(fg->m_textureinfo_cursor[FgRead]);
            pi->m_texture_write.reset(fg->m_textureinfo_cursor[FgWrite]);
            pi->m_buffer_create.reset(fg->m_bufferinfo_cursor[FgCreate]);
            pi->m_buffer_read.reset(fg->m_bufferinfo_cursor[FgRead]);
            pi->m_buffer_write.reset(fg->m_bufferinfo_cursor[FgWrite]);
            fg->m_pass_array_size++;

            return pi;
        }

        FgTexture fg_import(Fg* fg, const char* name, GfxTexture* resource, GfxTextureDescr* descr)
        {
            // ...
            // ti->m_resource.m_flags     = IMPORTED;

            return s_invalid_texture;
        }

        FgBuffer fg_import(Fg* fg, const char* name, GfxBuffer* resource, GfxBufferDescr* descr)
        {
            // ...
            // ti->m_resource.m_flags     = IMPORTED;

            return s_invalid_buffer;
        }

        FgTexture fg_create(Fg* fg, const char* name, GfxTexture* textureObject, GfxTextureDescr* textureDescr)
        {
            FgTextureInfo* ti          = &fg->m_textureinfo_array[fg->m_textureinfo_cursor[FgMain]];
            ti->m_resource.m_name      = name;
            ti->m_resource.m_pass      = fg->m_current_passinfo;
            ti->m_resource.m_last      = nullptr;
            ti->m_resource.m_ref_count = 0;
            ti->m_resource.m_flags     = 0;
            ti->m_texture              = textureObject;
            ti->m_textureDescr         = textureDescr;

            fg->m_textureinfo_create_array[fg->m_textureinfo_cursor[FgCreate]] = fg->m_textureinfo_cursor[FgMain];
            fg->m_current_passinfo->m_texture_create.add(fg->m_textureinfo_cursor[FgCreate]);

            fg->m_textureinfo_cursor[FgCreate] += 1;
            fg->m_textureinfo_cursor[FgMain] += 1;

            FgTexture texture;
            texture.index      = fg->m_textureinfo_cursor[FgCreate] - 1;
            texture.generation = fg->m_resource_generation;
            return texture;
        }

        FgTexture fg_read(Fg* fg, FgTexture texture, FgFlags descr)
        {
            ASSERT(fg->is_valid(texture));
            ASSERT(!fg->pass_contains(fg->m_current_passinfo, FgWrite, texture));
            ASSERT(!fg->pass_contains(fg->m_current_passinfo, FgCreate, texture));

            if (!fg->pass_contains(fg->m_current_passinfo, FgRead, texture))
            {
                fg->m_textureinfo_flags[texture.index]                         = descr;
                fg->m_textureinfo_read_array[fg->m_textureinfo_cursor[FgRead]] = texture.index;
                fg->m_current_passinfo->m_texture_read.add(fg->m_textureinfo_cursor[FgRead]);
                fg->m_textureinfo_cursor[FgRead] += 1;
            }
            return texture;
        }

        FgTexture fg_write(Fg* fg, FgTexture texture, FgFlags descr)
        {
            ASSERT(fg->is_valid(texture));

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
            ASSERT(!fg->pass_contains(fg->m_current_passinfo, FgRead, texture));

            if (fg->pass_contains(fg->m_current_passinfo, FgCreate, texture))
            {
                FgTextureInfo const* si                                          = &fg->m_textureinfo_array[texture.index];
                fg->m_textureinfo_write_array[fg->m_textureinfo_cursor[FgWrite]] = texture.index;
                fg->m_current_passinfo->m_texture_write.add(fg->m_textureinfo_cursor[FgWrite]);
                fg->m_textureinfo_cursor[FgWrite] += 1;
                fg->m_current_passinfo->m_flags |= si->m_resource.m_flags & IMPORTED;
                return texture;
            }
            else
            {
                fg_read(fg, texture, s_flags_ignored);

                // Clone FgTextureInfo
                FgTextureInfo const* si    = &fg->m_textureinfo_array[texture.index];
                FgTextureInfo*       ti    = &fg->m_textureinfo_array[fg->m_textureinfo_cursor[FgMain]];
                ti->m_resource.m_name      = si->m_resource.m_name;
                ti->m_resource.m_pass      = fg->m_current_passinfo;
                ti->m_resource.m_last      = nullptr;
                ti->m_resource.m_ref_count = 0;
                ti->m_resource.m_flags     = si->m_resource.m_flags;
                ti->m_texture              = si->m_texture;
                ti->m_textureDescr         = si->m_textureDescr;

                fg->m_textureinfo_write_array[fg->m_textureinfo_cursor[FgWrite]] = fg->m_textureinfo_cursor[FgMain];
                fg->m_current_passinfo->m_texture_write.add(fg->m_textureinfo_cursor[FgWrite]);
                fg->m_textureinfo_cursor[FgWrite] += 1;

                FgTexture texture;
                texture.index      = fg->m_textureinfo_cursor[FgWrite] - 1;
                texture.generation = fg->m_resource_generation;
                return texture;
            }

            return s_invalid_texture;
        }

        FgBuffer fg_create(Fg* fg, const char* name, GfxBuffer* bufferObject, GfxBufferDescr* bufferDescr)
        {
            FgBufferInfo* bi           = &fg->m_bufferinfo_array[fg->m_bufferinfo_cursor[FgMain]];
            bi->m_resource.m_name      = name;
            bi->m_resource.m_pass      = fg->m_current_passinfo;
            bi->m_resource.m_last      = nullptr;
            bi->m_resource.m_ref_count = 0;
            bi->m_buffer               = bufferObject;
            bi->m_bufferDescr          = bufferDescr;

            fg->m_bufferinfo_create_array[fg->m_bufferinfo_cursor[FgCreate]] = fg->m_bufferinfo_cursor[FgMain];
            fg->m_current_passinfo->m_buffer_create.add(fg->m_bufferinfo_cursor[FgCreate]);

            fg->m_bufferinfo_cursor[FgCreate] += 1;
            fg->m_bufferinfo_cursor[FgMain] += 1;

            FgBuffer fb;
            fb.index      = fg->m_bufferinfo_cursor[FgCreate] - 1;
            fb.generation = fg->m_resource_generation;
            return fb;
        }

        FgBuffer fg_read(Fg* fg, FgBuffer buffer, FgFlags descr)
        {
            ASSERT(fg->is_valid(buffer));
            ASSERT(!fg->pass_contains(fg->m_current_passinfo, FgWrite, buffer));
            ASSERT(!fg->pass_contains(fg->m_current_passinfo, FgCreate, buffer));

            if (!fg->pass_contains(fg->m_current_passinfo, FgRead, buffer))
            {
                fg->m_bufferinfo_flags[buffer.index]                         = descr;
                fg->m_bufferinfo_read_array[fg->m_bufferinfo_cursor[FgRead]] = buffer.index;
                fg->m_current_passinfo->m_buffer_read.add(fg->m_bufferinfo_cursor[FgRead]);
                fg->m_bufferinfo_cursor[FgRead] += 1;
            }
            return buffer;
        }

        FgBuffer fg_write(Fg* fg, FgBuffer buffer, FgFlags descr)
        {
            ASSERT(fg->is_valid(buffer));

            if (fg->pass_contains(fg->m_current_passinfo, FgCreate, buffer))
            {
                FgBufferInfo const* si                                         = &fg->m_bufferinfo_array[buffer.index];
                fg->m_bufferinfo_write_array[fg->m_bufferinfo_cursor[FgWrite]] = buffer.index;
                fg->m_current_passinfo->m_buffer_write.add(fg->m_bufferinfo_cursor[FgWrite]);
                fg->m_bufferinfo_cursor[FgWrite] += 1;
                fg->m_current_passinfo->m_flags |= si->m_resource.m_flags & IMPORTED;
                return buffer;
            }
            else
            {
                fg_read(fg, buffer, s_flags_ignored);

                // Clone FgBufferInfo
                FgBufferInfo const* si     = &fg->m_bufferinfo_array[buffer.index];
                FgBufferInfo*       bi     = &fg->m_bufferinfo_array[fg->m_bufferinfo_cursor[FgMain]];
                bi->m_resource.m_name      = si->m_resource.m_name;
                bi->m_resource.m_pass      = fg->m_current_passinfo;
                bi->m_resource.m_last      = nullptr;
                bi->m_resource.m_ref_count = 0;
                bi->m_resource.m_flags     = si->m_resource.m_flags;
                bi->m_buffer               = si->m_buffer;
                bi->m_bufferDescr          = si->m_bufferDescr;

                fg->m_bufferinfo_write_array[fg->m_bufferinfo_cursor[FgWrite]] = fg->m_bufferinfo_cursor[FgMain];
                fg->m_current_passinfo->m_buffer_write.add(fg->m_bufferinfo_cursor[FgWrite]);
                fg->m_bufferinfo_cursor[FgWrite] += 1;

                FgBuffer buffer;
                buffer.index      = fg->m_bufferinfo_cursor[FgWrite] - 1;
                buffer.generation = fg->m_resource_generation;
                return buffer;
            }

            return s_invalid_buffer;
        }

        GfxTexture*      fg_get(Fg* fg, FgTexture resource) { return fg->m_textureinfo_array[resource.index].m_texture; }
        GfxBuffer*       fg_get(Fg* fg, FgBuffer resource) { return fg->m_bufferinfo_array[resource.index].m_buffer; }
        GfxTextureDescr* fg_getDescr(Fg* fg, FgTexture resource) { return fg->m_textureinfo_array[resource.index].m_textureDescr; }
        GfxBufferDescr*  fg_getDescr(Fg* fg, FgBuffer resource) { return fg->m_bufferinfo_array[resource.index].m_bufferDescr; }
        FgFlags          fg_getFlags(Fg* fg, FgTexture resource) { return fg->m_textureinfo_flags[resource.index]; }
        FgFlags          fg_getFlags(Fg* fg, FgBuffer resource) { return fg->m_bufferinfo_flags[resource.index]; }

        void fg_compile(Fg* fg, alloc_t* allocator)
        {
            // Reset ref-counts
            for (s32 i = 0; i < fg->m_textureinfo_cursor[FgCreate]; ++i)
            {
                FgTextureInfo* texture          = &fg->m_textureinfo_array[i];
                texture->m_resource.m_ref_count = 0;
            }
            for (s32 i = 0; i < fg->m_bufferinfo_cursor[FgCreate]; ++i)
            {
                FgBufferInfo* buffer           = &fg->m_bufferinfo_array[i];
                buffer->m_resource.m_ref_count = 0;
            }

            // Calculate ref-counts of resources used by passes
            {
                for (s32 i = 0; i < fg->m_pass_array_size; ++i)
                {
                    FgPassInfo* pass  = &fg->m_passinfo_array[i];
                    pass->m_ref_count = static_cast<s32>(pass->m_texture_write.size()) + static_cast<s32>(pass->m_buffer_write.size());

                    // Texture and Buffer read
                    for (s32 j = pass->m_texture_read.begin; j < pass->m_texture_read.end; ++j)
                    {
                        FgIndex const  index    = fg->m_textureinfo_read_array[j];
                        FgTextureInfo* consumed = &fg->m_textureinfo_array[index];
                        consumed->m_resource.m_ref_count++;
                    }
                    for (s32 j = pass->m_buffer_read.begin; j < pass->m_buffer_read.end; ++j)
                    {
                        FgIndex const index    = fg->m_bufferinfo_read_array[j];
                        FgBufferInfo* consumed = &fg->m_bufferinfo_array[index];
                        consumed->m_resource.m_ref_count++;
                    }
                }
            }

            // Culling
            {
                // Textures
                FgResourceInfo** stack      = g_allocate_array_and_clear<FgResourceInfo*>(allocator, fg->m_textureinfo_cursor[FgCreate]);
                s32              stack_size = 0;
                for (s32 i = 0; i < fg->m_textureinfo_cursor[FgCreate]; ++i)
                {
                    FgTextureInfo* texture = &fg->m_textureinfo_array[i];
                    if (texture->m_resource.m_ref_count == 0)
                        stack[stack_size++] = &texture->m_resource;
                }
                for (s32 i = 0; i < fg->m_bufferinfo_cursor[FgCreate]; ++i)
                {
                    FgBufferInfo* buffer = &fg->m_bufferinfo_array[i];
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
                            FgIndex const  index    = fg->m_textureinfo_read_array[j];
                            FgTextureInfo* consumed = &fg->m_textureinfo_array[index];
                            if (--consumed->m_resource.m_ref_count == 0)
                                stack[stack_size++] = &consumed->m_resource;
                        }
                        for (s32 j = producer->m_buffer_read.begin; j < producer->m_buffer_read.end; ++j)
                        {
                            FgIndex const index    = fg->m_bufferinfo_read_array[j];
                            FgBufferInfo* consumed = &fg->m_bufferinfo_array[index];
                            if (--consumed->m_resource.m_ref_count == 0)
                                stack[stack_size++] = &consumed->m_resource;
                        }
                    }
                    g_deallocate_array(allocator, stack);
                }

                // Calculate resources lifetime
                {
                    for (s32 i = 0; i < fg->m_pass_array_size; ++i)
                    {
                        FgPassInfo* pass = &fg->m_passinfo_array[i];
                        if (pass->m_ref_count == 0)
                            continue;

                        // Created Textures and Buffers
                        for (s32 j = pass->m_texture_create.begin; j < pass->m_texture_create.end; ++j)
                        {
                            FgIndex const index                              = fg->m_textureinfo_create_array[j];
                            fg->m_textureinfo_array[index].m_resource.m_pass = pass;
                        }
                        for (s32 j = pass->m_buffer_create.begin; j < pass->m_buffer_create.end; ++j)
                        {
                            FgIndex const index                             = fg->m_bufferinfo_create_array[j];
                            fg->m_bufferinfo_array[index].m_resource.m_pass = pass;
                        }

                        // Read Textures and Buffers
                        for (s32 j = pass->m_texture_read.begin; j < pass->m_texture_read.end; ++j)
                        {
                            FgIndex const index                              = fg->m_textureinfo_read_array[j];
                            fg->m_textureinfo_array[index].m_resource.m_last = pass;
                        }
                        for (s32 j = pass->m_buffer_read.begin; j < pass->m_buffer_read.end; ++j)
                        {
                            FgIndex const index                             = fg->m_bufferinfo_read_array[j];
                            fg->m_bufferinfo_array[index].m_resource.m_last = pass;
                        }

                        // Written Textures and Buffers
                        for (s32 j = pass->m_texture_write.begin; j < pass->m_texture_write.end; ++j)
                        {
                            FgIndex const index                              = fg->m_textureinfo_write_array[j];
                            fg->m_textureinfo_array[index].m_resource.m_last = pass;
                        }
                        for (s32 j = pass->m_buffer_write.begin; j < pass->m_buffer_write.end; ++j)
                        {
                            FgIndex const index                             = fg->m_bufferinfo_write_array[j];
                            fg->m_bufferinfo_array[index].m_resource.m_last = pass;
                        }
                    }
                }
            }
        }

        void fg_execute(Fg* fg, GfxRenderContext* ctxt)
        {
            for (s32 i = 0; i < fg->m_pass_array_size; ++i)
            {
                FgPassInfo* pass = &fg->m_passinfo_array[i];
                if (pass->m_ref_count == 0 && !((pass->m_flags & HAS_SIDE_EFFECTS) == HAS_SIDE_EFFECTS))
                    continue;

                // Create transient resources
                for (s32 j = pass->m_texture_create.begin; j < pass->m_texture_create.end; ++j)
                {
                    FgIndex        index   = fg->m_textureinfo_create_array[j];
                    FgTextureInfo* texture = &fg->m_textureinfo_array[index];
                    if (texture->m_resource.m_last == pass)
                        fg->m_create_texture.Call(ctxt, texture->m_texture, texture->m_textureDescr);
                }
                for (s32 j = pass->m_buffer_create.begin; j < pass->m_buffer_create.end; ++j)
                {
                    FgIndex       index  = fg->m_bufferinfo_create_array[j];
                    FgBufferInfo* buffer = &fg->m_bufferinfo_array[index];
                    if (buffer->m_resource.m_last == pass)
                        fg->m_create_buffer.Call(ctxt, buffer->m_buffer, buffer->m_bufferDescr);
                }

                // Pre-read and pre-write
                for (s32 j = pass->m_texture_read.begin; j < pass->m_texture_read.end; ++j)
                {
                    if (fg->m_textureinfo_flags[j].m_descr != s_flags_ignored.m_descr)
                        fg->m_preread_texture.Call(ctxt, fg->m_textureinfo_array[j].m_texture, fg->m_textureinfo_flags[j]);
                }
                for (s32 j = pass->m_buffer_read.begin; j < pass->m_buffer_read.end; ++j)
                {
                    if (fg->m_bufferinfo_flags[j].m_descr != s_flags_ignored.m_descr)
                        fg->m_preread_buffer.Call(ctxt, fg->m_bufferinfo_array[j].m_buffer, fg->m_bufferinfo_flags[j]);
                }
                for (s32 j = pass->m_texture_write.begin; j < pass->m_texture_write.end; ++j)
                {
                    if (fg->m_textureinfo_flags[j].m_descr != s_flags_ignored.m_descr)
                        fg->m_prewrite_texture.Call(ctxt, fg->m_textureinfo_array[j].m_texture, fg->m_textureinfo_flags[j]);
                }
                for (s32 j = pass->m_buffer_write.begin; j < pass->m_buffer_write.end; ++j)
                {
                    if (fg->m_bufferinfo_flags[j].m_descr != s_flags_ignored.m_descr)
                        fg->m_prewrite_buffer.Call(ctxt, fg->m_bufferinfo_array[j].m_buffer, fg->m_bufferinfo_flags[j]);
                }

                // Execute pass
                pass->m_execute_fn.Call(fg, ctxt);

                // Destroy transient resources
                for (s32 j = 0; j < fg->m_textureinfo_cursor[FgMain]; ++j)
                {
                    FgTextureInfo* texture = &fg->m_textureinfo_array[j];
                    if (texture->m_resource.m_last == pass /* && texture is transient*/)
                        fg->m_destroy_texture.Call(ctxt, texture->m_texture);
                }
                for (s32 j = 0; j < fg->m_bufferinfo_cursor[FgMain]; ++j)
                {
                    FgBufferInfo* buffer = &fg->m_bufferinfo_array[j];
                    if (buffer->m_resource.m_last == pass /* && buffer is transient*/)
                        fg->m_destroy_buffer.Call(ctxt, buffer->m_buffer);
                }
            }
        }

        bool Fg::is_valid(FgTexture resource) const { return resource.index < m_textureinfo_cursor[FgMain] && resource.generation == m_resource_generation; }
        bool Fg::is_valid(FgBuffer resource) const { return resource.index < m_bufferinfo_cursor[FgMain] && resource.generation == m_resource_generation; }

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
