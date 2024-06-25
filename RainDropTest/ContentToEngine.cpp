#include "ContentToEngine.h"
#include "Content.h"
#include "FreeList.h"

namespace d3d12::content
{
    namespace {

        constexpr uintptr_t single_mesh_marker{ (uintptr_t)0x01 };
        utl::free_list<UINT8*> geometry_hierarchies;
        std::mutex geometry_mutex;

        UINT get_geometry_hierarchy_buffer_size(const void* const data)
        {
            const UINT8* data_ptr{ (UINT8*)data };
            const geometry_data* geometry_ptr = (geometry_data*)(data);
            const UINT level_of_detail_count = geometry_ptr->level_of_detail_count;

            UINT size{ sizeof(UINT) + (sizeof(float) + sizeof(level_of_detail_offset_count) * level_of_detail_count) };
            // Get the gpu_ids for the total_number_of_sub_meshes
            UINT64 offset = sizeof(UINT);
            for (UINT i{ 0 }; i < level_of_detail_count; ++i)
            {
                geometry_header* mesh_ptr = (geometry_header*)&data_ptr[offset];
                size += sizeof(UINT) * mesh_ptr->sub_mesh_count;
                offset += sizeof(geometry_header) + mesh_ptr->size_of_sub_meshes;
            }

            return size;
        }

        UINT create_material_resource(const void* const data)
        {
            return 0;
        }

        UINT create_single_sub_mesh(const void* const data)
        {
            const UINT8* ptr = (UINT8*)(data)+sizeof(geometry_data);
            const UINT gpu_id{ sub_mesh::add(ptr) };

            // Create a fake pointer and it in the hierarchies.
            constexpr UINT8 shift_bits{ (sizeof(uintptr_t) - sizeof(UINT)) << 3 };
            UINT8* const fake_pointer{ (UINT8* const)((((uintptr_t)gpu_id) << shift_bits) | single_mesh_marker) };
            std::lock_guard lock{ geometry_mutex };

            return geometry_hierarchies.add(fake_pointer);
        }

        // struct{
        //     u32 lod_count,
        //     struct {
        //         f32 lod_threshold,
        //         u32 submesh_count,
        //         u32 size_of_submeshes,
        //         struct {
        //             u32 element_size, u32 vertex_count,
        //             u32 index_count, u32 elements_type, u32 primitive_topology
        //             u8 positions[sizeof(f32) * 3 * vertex_count],     // sizeof(positions) must be a multiple of 4 bytes. Pad if needed.
        //             u8 elements[sizeof(element_size) * vertex_count], // sizeof(elements) must be a multiple of 4 bytes. Pad if needed.
        //             u8 indices[index_size * index_count]
        //         } submeshes[sub_mesh_count]
        //     } mesh_lods[lod_count]
        // } geometry;
        //
        // Output format
        // If geometry has more than one LOD or sub_mesh:
        // struct {
        //     u32 lod_count,
        //     f32 thresholds[lod_count]
        //     struct {
        //         u16 offset,
        //         u16 count
        //     } lod_offsets[lod_count],
        //     id::id_type gpu_ids[total_number_of_sub_meshes]
        // } geometry_hierarchy
        UINT create_mesh_hierarchy(const void* const data)
        {
            const UINT size{ get_geometry_hierarchy_buffer_size(data) };
            UINT8* const hierarchy_buffer{ (UINT8* const)malloc(size) };
            assert(hierarchy_buffer);

            struct geometry_data* ptr = (geometry_data*)data;
            const UINT level_of_detail_count{ ptr->level_of_detail_count };
            assert(level_of_detail_count);

            *(UINT*)hierarchy_buffer = level_of_detail_count;

            float* const thresholds{ (float*)&hierarchy_buffer[sizeof(UINT)] };
            level_of_detail_offset_count* lod_offset_count{ (level_of_detail_offset_count*)&hierarchy_buffer[sizeof(UINT) + (sizeof(float) * level_of_detail_count)] };
            UINT* const gpu_ids{ (UINT*)&hierarchy_buffer[sizeof(UINT) + ((sizeof(float) + sizeof(level_of_detail_offset_count)) * level_of_detail_count)] };

            UINT sub_mesh_index{ 0 };
            for (UINT level_of_detail_idx{ 0 }; level_of_detail_idx < level_of_detail_count; ++level_of_detail_idx)
            {
                thresholds[level_of_detail_idx] = ptr->header.level_of_detail_threshold;
                const UINT sub_mesh_count = ptr->header.sub_mesh_count;
                assert(sub_mesh_count);
                lod_offset_count[level_of_detail_idx].count = sub_mesh_count;
                lod_offset_count[level_of_detail_idx].offset = sub_mesh_index;
                const UINT8* sub_mesh_ptr = (UINT8 *)data + sizeof(geometry_data);
                for (UINT id_idx{ 0 }; id_idx < sub_mesh_count; ++id_idx)
                {
                    gpu_ids[sub_mesh_index] = sub_mesh::add(sub_mesh_ptr);
                    ++sub_mesh_index;

                    sub_mesh_ptr += ptr->header.size_of_sub_meshes;
                }
            }

            // Check the thresholds are increasing
            assert([&]() {
                float previous_threshold{ thresholds[0] };
                for (UINT i{ 0 }; i < level_of_detail_count; ++i)
                {
                    if (thresholds[i] < previous_threshold) return false;
                    previous_threshold = thresholds[i];
                }
                return true;
            }());
            return geometry_hierarchies.add(hierarchy_buffer);
        }

        bool is_single_mesh(const void* const data)
        {
            struct geometry_data* ptr = (geometry_data*)data;
            assert(ptr->level_of_detail_count);
            if (ptr->level_of_detail_count > 1) return false;

            assert(ptr->header.sub_mesh_count);
            return ptr->header.sub_mesh_count == 1;
        }

        constexpr UINT gpu_id_from_fake_pointer(UINT8* const pointer)
        {
            assert((uintptr_t)pointer & single_mesh_marker);
            constexpr UINT8 shift_bits{ (sizeof(uintptr_t) - sizeof(UINT)) << 3 };
            return (((uintptr_t)pointer) >> shift_bits) & (uintptr_t)Invalid_Index;
        }

        // NOTE: Expects 'data' to contain:
        // struct{
        //     u32 lod_count,
        //     struct {
        //         f32 lod_threshold,
        //         u32 sub_mesh_count,
        //         u32 size_of_sub_meshes,
        //         struct {
        //             u32 element_size, u32 vertex_count,
        //             u32 index_count, u32 elements_type, u32 primitive_topology
        //             u8 positions[sizeof(f32) * 3 * vertex_count],     // sizeof(positions) must be a multiple of 4 bytes. Pad if needed.
        //             u8 elements[sizeof(element_size) * vertex_count], // sizeof(elements) must be a multiple of 4 bytes. Pad if needed.
        //             u8 indices[index_size * index_count]
        //         } sub_meshes[sub_mesh_count]
        //     } mesh_level_of_details[lod_count]
        // } geometry;
        //
        // Output format
        //
        // If geometry has more than one LOD or sub_mesh:
        // struct {
        //     u32 lod_count,
        //     f32 thresholds[lod_count]
        //     struct {
        //         u16 offset,
        //         u16 count
        //     } lod_offsets[lod_count],
        //     id::id_type gpu_ids[total_number_of_sub_meshes]
        // } geometry_hierarchy
        // 
        // If geometry has a single LOD and sub_mesh:
        //
        // (gpu_id << 32) | 0x01
        //
        UINT create_geometry_resource(const void* const data)
        {
            return is_single_mesh(data) ? create_single_sub_mesh(data) : create_mesh_hierarchy(data);
        }

        UINT create_texture_resource(const void* const data)
        {
            return 0;
        }

        void destroy_material_resource(UINT id)
        {}

        // If geometry has more than one LOD or sub_mesh:
        // struct {
        //     u32 lod_count,
        //     f32 thresholds[lod_count]
        //     struct {
        //         u16 offset,
        //         u16 count
        //     } lod_offsets[lod_count],
        //     id::id_type gpu_ids[total_number_of_sub_meshes]
        // } geometry_hierarchy
        void destroy_geometry_resource(UINT id)
        {
            std::lock_guard lock{ geometry_mutex };
            UINT8* const pointer{ geometry_hierarchies[id] };
            if ((uintptr_t)pointer & single_mesh_marker)
            {
                content::sub_mesh::remove(gpu_id_from_fake_pointer(pointer));
            }
            else
            {
                struct geometry_data* ptr = (geometry_data*)pointer;
                const UINT level_of_detail_count{ ptr->level_of_detail_count };
                assert(level_of_detail_count);
                level_of_detail_offset_count* lod_offset_count{ (level_of_detail_offset_count*)&pointer[sizeof(UINT) + (sizeof(float) * level_of_detail_count)] };
                UINT* const gpu_ids{ (UINT*)&pointer[sizeof(UINT) + ((sizeof(float) + sizeof(level_of_detail_offset_count)) * level_of_detail_count)] };
                UINT id_index{ 0 };
                for (UINT lod_idx{ 0 }; lod_idx < level_of_detail_count; ++lod_idx)
                {
                    for (UINT i{ 0 }; i < lod_offset_count[lod_idx].count; ++i)
                    {
                        content::sub_mesh::remove(gpu_ids[id_index]);
                        ++id_index;
                    }
                }

                free(pointer);
            }
            geometry_hierarchies.remove(id);
        }

        void destroy_texture_resource(UINT id)
        {}

    } // anonymous namespace

    UINT create_resource(const void* const data, asset_type::type type)
    {
        assert(data);
        UINT id = Invalid_Index;

        switch (type)
        {
        case asset_type::material: id = create_material_resource(data); break;
        case asset_type::mesh: id = create_geometry_resource(data); break;
        case asset_type::texture: id = create_texture_resource(data); break;
        }

        assert(id != Invalid_Index);
        return id;
    }

    void destroy_resource(const UINT id, asset_type::type type)
    {
        assert(id != Invalid_Index);
        switch (type)
        {
        case asset_type::material: destroy_material_resource(id); break;
        case asset_type::mesh: destroy_geometry_resource(id); break;
        case asset_type::texture: destroy_texture_resource(id); break;
        }
    }
}
