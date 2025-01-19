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

        typedef s8          FgType;
        static const FgType FgCreate = 0;
        static const FgType FgRead   = 1;
        static const FgType FgWrite  = 2;

        enum EFlags
        {
            IMPORTED         = 0x0001,
            TRANSIENT        = 0x0002,
            HAS_SIDE_EFFECTS = 0x8000,
        };

        struct FgPassInfo
        {
            const char* m_name;
            FgExecuteFn m_execute_fn;
            u16         m_flags;
            s16         m_final;      // intermediary or final
            s32         m_ref_count;  // the number of resources that are written by this pass
            FgRange     m_texture[3]; // the begin and end index into the 'create/read/write' texture array
            FgRange     m_buffer[3];  // the begin and end index into the 'create/read/write' buffer array
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
            FgIndex        m_textureinfo_cursor_main;
            FgIndex        m_textureinfo_cursor[3]; // current number of create texture info resources
            FgIndex        m_bufferinfo_cursor_main;
            FgIndex        m_bufferinfo_cursor[3]; // current number of create buffer info resources
            FgTextureInfo* m_textureinfo_array;
            FgFlags*       m_textureinfo_flags;
            FgIndex*       m_textureinfo_crw_array[3];
            FgBufferInfo*  m_bufferinfo_array;
            FgFlags*       m_bufferinfo_flags;
            FgIndex*       m_bufferinfo_crw_array[3];

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
            fg->m_bufferinfo_array  = g_allocate_array_and_clear<FgBufferInfo>(allocator, resource_capacity);
            fg->m_bufferinfo_flags  = g_allocate_array_and_clear<FgFlags>(allocator, resource_capacity);

            for (s32 i = FgCreate; i <= FgWrite; ++i)
            {
                fg->m_textureinfo_crw_array[i] = g_allocate_array_and_clear<FgIndex>(allocator, resource_capacity);
                fg->m_bufferinfo_crw_array[i]  = g_allocate_array_and_clear<FgIndex>(allocator, resource_capacity);
            }

            return fg;
        }

        void fg_teardown(Fg*& fg)
        {
            g_deallocate_array(fg->m_allocator, fg->m_passinfo_array);
            g_deallocate_array(fg->m_allocator, fg->m_textureinfo_array);
            g_deallocate_array(fg->m_allocator, fg->m_textureinfo_flags);
            g_deallocate_array(fg->m_allocator, fg->m_bufferinfo_array);
            g_deallocate_array(fg->m_allocator, fg->m_bufferinfo_flags);

            for (s32 i = FgCreate; i <= FgWrite; ++i)
            {
                g_deallocate_array(fg->m_allocator, fg->m_textureinfo_crw_array[i]);
                g_deallocate_array(fg->m_allocator, fg->m_bufferinfo_crw_array[i]);
            }

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

        static FgPass s_fg_open_pass(Fg* fg, const char* name, FgExecuteFn execute, s16 final)
        {
            ASSERT(fg->m_current_passinfo == nullptr);

            FgPassInfo* pi   = &fg->m_passinfo_array[fg->m_pass_array_size];
            pi->m_name       = name;
            pi->m_execute_fn = execute;
            pi->m_final      = final;
            pi->m_flags      = 0;
            pi->m_ref_count  = 0;
            for (s32 i = FgCreate; i <= FgWrite; ++i)
            {
                pi->m_texture[i].reset(fg->m_textureinfo_cursor[i]);
                pi->m_buffer[i].reset(fg->m_bufferinfo_cursor[i]);
            }

            fg->m_pass_array_size++;
            fg->m_current_passinfo = pi;
            return pi;
        }

        FgPass fg_open_pass(Fg* fg, const char* name, FgExecuteFn execute) { return s_fg_open_pass(fg, name, execute, 0); }
        FgPass fg_final_pass(Fg* fg, const char* name, FgExecuteFn execute) { return s_fg_open_pass(fg, name, execute, 1); }

        void fg_close_pass(Fg* fg)
        {
            ASSERT(fg->m_current_passinfo != nullptr);
            fg->m_current_passinfo = nullptr;
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
            FgIndex&       main        = fg->m_textureinfo_cursor_main;
            FgTextureInfo* ti          = &fg->m_textureinfo_array[main];
            ti->m_resource.m_name      = name;
            ti->m_resource.m_pass      = fg->m_current_passinfo;
            ti->m_resource.m_last      = nullptr;
            ti->m_resource.m_ref_count = 0;
            ti->m_resource.m_flags     = 0;
            ti->m_texture              = textureObject;
            ti->m_textureDescr         = textureDescr;

            FgType const type  = FgCreate;
            FgFlags&     flags = fg->m_textureinfo_flags[main];
            FgRange&     range = fg->m_current_passinfo->m_texture[type];
            FgIndex&     index = fg->m_textureinfo_cursor[type];
            FgIndex*     array = fg->m_textureinfo_crw_array[type];

            flags = s_flags_ignored;
            range.add(index);
            array[index++] = main++;

            FgTexture texture;
            texture.index      = main - 1;
            texture.generation = fg->m_resource_generation;
            return texture;
        }

        FgTexture fg_read(Fg* fg, FgTexture _texture, FgFlags _descr)
        {
            ASSERT(fg->is_valid(_texture));
            ASSERT(!fg->pass_contains(fg->m_current_passinfo, FgWrite, _texture));
            ASSERT(!fg->pass_contains(fg->m_current_passinfo, FgCreate, _texture));
            if (!fg->pass_contains(fg->m_current_passinfo, FgRead, _texture))
            {
                FgType const type  = FgRead;
                FgFlags&     flags = fg->m_textureinfo_flags[_texture.index];
                FgRange&     range = fg->m_current_passinfo->m_texture[type];
                FgIndex&     index = fg->m_textureinfo_cursor[type];
                FgIndex*     array = fg->m_textureinfo_crw_array[type];

                flags = _descr;
                range.add(index);
                array[index] = _texture.index;
                index++;
            }
            return _texture;
        }

        FgTexture fg_write(Fg* fg, FgTexture _texture, FgFlags _descr)
        {
            ASSERT(fg->is_valid(_texture));
            ASSERT(!fg->pass_contains(fg->m_current_passinfo, FgRead, _texture));
            if (fg->pass_contains(fg->m_current_passinfo, FgCreate, _texture))
            {
                FgType const type  = FgWrite;
                FgFlags&     flags = fg->m_textureinfo_flags[_texture.index];
                FgRange&     range = fg->m_current_passinfo->m_texture[type];
                FgIndex&     index = fg->m_textureinfo_cursor[type];
                FgIndex*     array = fg->m_textureinfo_crw_array[type];

                flags = _descr;
                range.add(index);
                array[index++] = _texture.index;

                FgTextureInfo const* si = &fg->m_textureinfo_array[_texture.index];
                fg->m_current_passinfo->m_flags |= si->m_resource.m_flags & IMPORTED;

                return _texture;
            }
            else
            {
                // Also mark the texture as read
                fg_read(fg, _texture, s_flags_ignored);

                // Clone FgTextureInfo
                FgTextureInfo const* si = &fg->m_textureinfo_array[_texture.index];

                FgIndex&       main        = fg->m_textureinfo_cursor_main;
                FgTextureInfo* ti          = &fg->m_textureinfo_array[main];
                ti->m_resource.m_name      = si->m_resource.m_name;
                ti->m_resource.m_pass      = fg->m_current_passinfo;
                ti->m_resource.m_last      = nullptr;
                ti->m_resource.m_ref_count = 0;
                ti->m_resource.m_flags     = si->m_resource.m_flags;
                ti->m_texture              = si->m_texture;
                ti->m_textureDescr         = si->m_textureDescr;

                FgType const type  = FgWrite;
                FgFlags&     flags = fg->m_textureinfo_flags[main];
                FgRange&     range = fg->m_current_passinfo->m_texture[type];
                FgIndex&     index = fg->m_textureinfo_cursor[type];
                FgIndex*     array = fg->m_textureinfo_crw_array[type];

                flags = _descr;
                range.add(index);
                array[index] = main;
                index++;
                main++;

                FgTexture texture;
                texture.index      = main - 1;
                texture.generation = fg->m_resource_generation;
                return texture;
            }

            return s_invalid_texture;
        }

        FgBuffer fg_create(Fg* fg, const char* name, GfxBuffer* bufferObject, GfxBufferDescr* bufferDescr)
        {
            FgIndex&      main         = fg->m_bufferinfo_cursor_main;
            FgBufferInfo* bi           = &fg->m_bufferinfo_array[main];
            bi->m_resource.m_name      = name;
            bi->m_resource.m_pass      = fg->m_current_passinfo;
            bi->m_resource.m_last      = nullptr;
            bi->m_resource.m_ref_count = 0;
            bi->m_buffer               = bufferObject;
            bi->m_bufferDescr          = bufferDescr;

            FgType const type  = FgCreate;
            FgFlags&     flags = fg->m_bufferinfo_flags[main];
            FgRange&     range = fg->m_current_passinfo->m_buffer[type];
            FgIndex&     index = fg->m_bufferinfo_cursor[type];
            FgIndex*     array = fg->m_bufferinfo_crw_array[type];

            flags = s_flags_ignored;
            range.add(index);
            array[index] = main;
            index++;
            main++;

            FgBuffer fb;
            fb.index      = main - 1;
            fb.generation = fg->m_resource_generation;
            return fb;
        }

        FgBuffer fg_read(Fg* fg, FgBuffer _buffer, FgFlags _descr)
        {
            ASSERT(fg->is_valid(_buffer));
            ASSERT(!fg->pass_contains(fg->m_current_passinfo, FgWrite, _buffer));
            ASSERT(!fg->pass_contains(fg->m_current_passinfo, FgCreate, _buffer));

            if (!fg->pass_contains(fg->m_current_passinfo, FgRead, _buffer))
            {
                FgType const type  = FgRead;
                FgFlags&     flags = fg->m_bufferinfo_flags[_buffer.index];
                FgRange&     range = fg->m_current_passinfo->m_buffer[type];
                FgIndex&     index = fg->m_bufferinfo_cursor[type];
                FgIndex*     array = fg->m_bufferinfo_crw_array[type];

                flags = _descr;
                range.add(index);
                array[index] = _buffer.index;
                index++;
            }
            return _buffer;
        }

        FgBuffer fg_write(Fg* fg, FgBuffer _buffer, FgFlags _descr)
        {
            ASSERT(fg->is_valid(_buffer));

            if (fg->pass_contains(fg->m_current_passinfo, FgCreate, _buffer))
            {
                FgBufferInfo const* si = &fg->m_bufferinfo_array[_buffer.index];
                fg->m_current_passinfo->m_flags |= si->m_resource.m_flags & IMPORTED;

                FgType const type  = FgWrite;
                FgFlags&     flags = fg->m_bufferinfo_flags[_buffer.index];
                FgRange&     range = fg->m_current_passinfo->m_buffer[type];
                FgIndex&     index = fg->m_bufferinfo_cursor[type];
                FgIndex*     array = fg->m_bufferinfo_crw_array[type];

                flags = _descr;
                range.add(index);
                array[index] = _buffer.index;
                index++;

                return _buffer;
            }
            else
            {
                fg_read(fg, _buffer, s_flags_ignored);

                // Clone the incoming FgBufferInfo
                FgBufferInfo const* si = &fg->m_bufferinfo_array[_buffer.index];

                // New FgBufferInfo
                FgIndex&      main         = fg->m_bufferinfo_cursor_main;
                FgBufferInfo* bi           = &fg->m_bufferinfo_array[main];
                bi->m_resource.m_name      = si->m_resource.m_name;
                bi->m_resource.m_pass      = fg->m_current_passinfo;
                bi->m_resource.m_last      = nullptr;
                bi->m_resource.m_ref_count = 0;
                bi->m_resource.m_flags     = si->m_resource.m_flags;
                bi->m_buffer               = si->m_buffer;
                bi->m_bufferDescr          = si->m_bufferDescr;

                FgType const type  = FgWrite;
                FgFlags&     flags = fg->m_bufferinfo_flags[main];
                FgRange&     range = fg->m_current_passinfo->m_buffer[type];
                FgIndex&     index = fg->m_bufferinfo_cursor[type];
                FgIndex*     array = fg->m_bufferinfo_crw_array[type];

                flags = _descr;
                range.add(index);
                array[index] = main;
                index++;
                main++;

                FgBuffer buffer;
                buffer.index      = main - 1;
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
            ASSERT(fg->m_current_passinfo == nullptr);

            if ((fg->m_pass_array_size == 0) && (fg->m_textureinfo_cursor_main == 0) && (fg->m_bufferinfo_cursor_main == 0))
                return;

            // Reset ref-counts
            for (s32 i = 0; i < fg->m_textureinfo_cursor[FgCreate]; ++i)
            {
                FgResourceInfo* resource = &fg->m_textureinfo_array[i].m_resource;
                resource->m_ref_count    = 0;
            }
            for (s32 i = 0; i < fg->m_bufferinfo_cursor[FgCreate]; ++i)
            {
                FgResourceInfo* resource = &fg->m_bufferinfo_array[i].m_resource;
                resource->m_ref_count    = 0;
            }

            // Calculate ref-counts of resources used by passes
            {
                for (s32 i = 0; i < fg->m_pass_array_size; ++i)
                {
                    FgPassInfo* pass  = &fg->m_passinfo_array[i];
                    pass->m_ref_count = static_cast<s32>(pass->m_texture[FgWrite].size()) + static_cast<s32>(pass->m_buffer[FgWrite].size());

                    // Texture and Buffer read
                    for (s32 j = pass->m_texture[FgRead].begin; j < pass->m_texture[FgRead].end; ++j)
                    {
                        FgIndex const  index    = fg->m_textureinfo_crw_array[FgRead][j];
                        FgTextureInfo* consumed = &fg->m_textureinfo_array[index];
                        consumed->m_resource.m_ref_count++;
                    }
                    for (s32 j = pass->m_buffer[FgRead].begin; j < pass->m_buffer[FgRead].end; ++j)
                    {
                        FgIndex const index    = fg->m_bufferinfo_crw_array[FgRead][j];
                        FgBufferInfo* consumed = &fg->m_bufferinfo_array[index];
                        consumed->m_resource.m_ref_count++;
                    }

                    // Texture and Buffer write, assign producer
                    for (s32 j = pass->m_texture[FgWrite].begin; j < pass->m_texture[FgWrite].end; ++j)
                    {
                        FgIndex const   index    = fg->m_textureinfo_crw_array[FgWrite][j];
                        FgResourceInfo* resource = &fg->m_textureinfo_array[index].m_resource;
                        resource->m_pass         = pass;
                        resource->m_ref_count += (pass->m_final == 1) ? 1 : 0;
                    }
                    for (s32 j = pass->m_buffer[FgWrite].begin; j < pass->m_buffer[FgWrite].end; ++j)
                    {
                        FgIndex const   index    = fg->m_bufferinfo_crw_array[FgWrite][j];
                        FgResourceInfo* resource = &fg->m_bufferinfo_array[index].m_resource;
                        resource->m_pass         = pass;
                        resource->m_ref_count += (pass->m_final == 1) ? 1 : 0;
                    }
                }
            }

            // Culling
            {
                // Textures
                s32              stack_size     = 0;
                s32 const        max_stack_size = fg->m_textureinfo_cursor_main + fg->m_bufferinfo_cursor_main;
                FgResourceInfo** stack          = g_allocate_array_and_clear<FgResourceInfo*>(allocator, max_stack_size);
                for (s32 i = 0; i < fg->m_textureinfo_cursor_main; ++i)
                {
                    FgResourceInfo* resource = &fg->m_textureinfo_array[i].m_resource;
                    if (resource->m_ref_count == 0)
                        stack[stack_size++] = resource;
                }
                for (s32 i = 0; i < fg->m_bufferinfo_cursor_main; ++i)
                {
                    FgResourceInfo* resource = &fg->m_bufferinfo_array[i].m_resource;
                    if (resource->m_ref_count == 0)
                        stack[stack_size++] = resource;
                }

                while (stack_size > 0)
                {
                    FgResourceInfo* rsc      = stack[--stack_size];
                    FgPassInfo*     producer = rsc->m_pass;
                    if (producer == nullptr || ((producer->m_flags & HAS_SIDE_EFFECTS) == HAS_SIDE_EFFECTS))
                        continue;

                    ASSERT(producer->m_ref_count >= 1);
                    if (--producer->m_ref_count == 0 && producer->m_final == 0)
                    {
                        for (s32 j = producer->m_texture[FgRead].begin; j < producer->m_texture[FgRead].end; ++j)
                        {
                            FgIndex const  index    = fg->m_textureinfo_crw_array[FgRead][j];
                            FgTextureInfo* consumed = &fg->m_textureinfo_array[index];
                            if (--consumed->m_resource.m_ref_count == 0)
                                stack[stack_size++] = &consumed->m_resource;
                        }
                        for (s32 j = producer->m_buffer[FgRead].begin; j < producer->m_buffer[FgRead].end; ++j)
                        {
                            FgIndex const index    = fg->m_bufferinfo_crw_array[FgRead][j];
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
                        if (pass->m_ref_count == 0 && pass->m_final == 0)
                            continue;

                        // Created Textures and Buffers
                        for (s32 j = pass->m_texture[FgCreate].begin; j < pass->m_texture[FgCreate].end; ++j)
                        {
                            FgIndex const index                              = fg->m_textureinfo_crw_array[FgCreate][j];
                            fg->m_textureinfo_array[index].m_resource.m_pass = pass;
                        }
                        for (s32 j = pass->m_buffer[FgCreate].begin; j < pass->m_buffer[FgCreate].end; ++j)
                        {
                            FgIndex const index                             = fg->m_bufferinfo_crw_array[FgCreate][j];
                            fg->m_bufferinfo_array[index].m_resource.m_pass = pass;
                        }

                        // Read Textures and Buffers
                        for (s32 j = pass->m_texture[FgRead].begin; j < pass->m_texture[FgRead].end; ++j)
                        {
                            FgIndex const index                              = fg->m_textureinfo_crw_array[FgRead][j];
                            fg->m_textureinfo_array[index].m_resource.m_last = pass;
                        }
                        for (s32 j = pass->m_buffer[FgRead].begin; j < pass->m_buffer[FgRead].end; ++j)
                        {
                            FgIndex const index                             = fg->m_bufferinfo_crw_array[FgRead][j];
                            fg->m_bufferinfo_array[index].m_resource.m_last = pass;
                        }

                        // Written Textures and Buffers
                        for (s32 j = pass->m_texture[FgWrite].begin; j < pass->m_texture[FgWrite].end; ++j)
                        {
                            FgIndex const index                              = fg->m_textureinfo_crw_array[FgWrite][j];
                            fg->m_textureinfo_array[index].m_resource.m_last = pass;
                        }
                        for (s32 j = pass->m_buffer[FgWrite].begin; j < pass->m_buffer[FgWrite].end; ++j)
                        {
                            FgIndex const index                             = fg->m_bufferinfo_crw_array[FgWrite][j];
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
                if (pass->m_ref_count == 0 && !((pass->m_flags & HAS_SIDE_EFFECTS) == HAS_SIDE_EFFECTS) && !(pass->m_final == 1))
                    continue;

                // Create transient resources
                for (s32 j = pass->m_texture[FgCreate].begin; j < pass->m_texture[FgCreate].end; ++j)
                {
                    FgIndex        index   = fg->m_textureinfo_crw_array[FgCreate][j];
                    FgTextureInfo* texture = &fg->m_textureinfo_array[index];
                    fg->m_create_texture.Call(ctxt, texture->m_texture, texture->m_textureDescr);
                }
                for (s32 j = pass->m_buffer[FgCreate].begin; j < pass->m_buffer[FgCreate].end; ++j)
                {
                    FgIndex       index  = fg->m_bufferinfo_crw_array[FgCreate][j];
                    FgBufferInfo* buffer = &fg->m_bufferinfo_array[index];
                    fg->m_create_buffer.Call(ctxt, buffer->m_buffer, buffer->m_bufferDescr);
                }

                // Pre-read and pre-write
                for (s32 j = pass->m_texture[FgRead].begin; j < pass->m_texture[FgRead].end; ++j)
                {
                    if (!fg_flags_ignored(fg->m_textureinfo_flags[j]))
                        fg->m_preread_texture.Call(ctxt, fg->m_textureinfo_array[j].m_texture, fg->m_textureinfo_flags[j]);
                }
                for (s32 j = pass->m_buffer[FgRead].begin; j < pass->m_buffer[FgRead].end; ++j)
                {
                    if (!fg_flags_ignored(fg->m_bufferinfo_flags[j]))
                        fg->m_preread_buffer.Call(ctxt, fg->m_bufferinfo_array[j].m_buffer, fg->m_bufferinfo_flags[j]);
                }
                for (s32 j = pass->m_texture[FgWrite].begin; j < pass->m_texture[FgWrite].end; ++j)
                {
                    if (!fg_flags_ignored(fg->m_textureinfo_flags[j]))
                        fg->m_prewrite_texture.Call(ctxt, fg->m_textureinfo_array[j].m_texture, fg->m_textureinfo_flags[j]);
                }
                for (s32 j = pass->m_buffer[FgWrite].begin; j < pass->m_buffer[FgWrite].end; ++j)
                {
                    if (!fg_flags_ignored(fg->m_bufferinfo_flags[j]))
                        fg->m_prewrite_buffer.Call(ctxt, fg->m_bufferinfo_array[j].m_buffer, fg->m_bufferinfo_flags[j]);
                }

                // Execute pass
                pass->m_execute_fn.Call(fg, ctxt);

                // Destroy transient resources
                for (s32 j = 0; j < fg->m_textureinfo_cursor_main; ++j)
                {
                    FgTextureInfo* texture = &fg->m_textureinfo_array[j];
                    if (texture->m_resource.m_last == pass /* && texture is transient*/)
                        fg->m_destroy_texture.Call(ctxt, texture->m_texture);
                }
                for (s32 j = 0; j < fg->m_bufferinfo_cursor_main; ++j)
                {
                    FgBufferInfo* buffer = &fg->m_bufferinfo_array[j];
                    if (buffer->m_resource.m_last == pass /* && buffer is transient*/)
                        fg->m_destroy_buffer.Call(ctxt, buffer->m_buffer);
                }
            }
        }

        bool Fg::is_valid(FgTexture resource) const { return resource.index < m_textureinfo_cursor_main && resource.generation == m_resource_generation; }
        bool Fg::is_valid(FgBuffer resource) const { return resource.index < m_bufferinfo_cursor_main && resource.generation == m_resource_generation; }

        bool Fg::pass_contains(FgPass pass, FgType type, FgTexture resource) const
        {
            FgIndex* array = nullptr;
            FgRange* range = nullptr;
            switch (type)
            {
                case FgCreate:
                    array = m_textureinfo_crw_array[FgCreate];
                    range = &pass->m_texture[FgCreate];
                    break;
                case FgRead:
                    array = m_textureinfo_crw_array[FgRead];
                    range = &pass->m_texture[FgRead];
                    break;
                case FgWrite:
                    array = m_textureinfo_crw_array[FgWrite];
                    range = &pass->m_texture[FgWrite];
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
                    array = m_bufferinfo_crw_array[FgCreate];
                    range = &pass->m_buffer[FgCreate];
                    break;
                case FgRead:
                    array = m_bufferinfo_crw_array[FgRead];
                    range = &pass->m_buffer[FgRead];
                    break;
                case FgWrite:
                    array = m_bufferinfo_crw_array[FgWrite];
                    range = &pass->m_buffer[FgWrite];
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
