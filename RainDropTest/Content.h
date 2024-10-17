#pragma once
#include "stdafx.h"
#include "Shaders.h"
#include "Core.h"

namespace graphic_pass {
    struct graphic_cache;
}

namespace content
{
    struct opaque_root_parameter {
        enum parameter : UINT {
            global_shader_data,
            per_object_data,
            position_buffer,
            element_buffer,
            srv_indices,
            directional_lights,
            cullable_lights,
            light_grid,
            light_index_list,

            count
        };
    };

    struct texture_flags {
        enum flags : UINT {
            is_hdr = 0x01,
            has_alpha = 0x02,
            is_premultiplied_alpha = 0x04,
            is_imported_as_normal_map = 0x08,
            is_cube_map = 0x10,
            is_volume_map = 0x20,
            is_srgb = 0x40,
        };
    };

    // ContentToEngine.cpp
    struct primitive_topology {
        enum type : UINT {
            point_list = 1,
            line_list,
            line_strip,
            triangle_list,
            triangle_strip,

            count
        };
    };

    struct asset_type {
        enum type : UINT {
            unknown = 0,
            material,
            mesh,
            texture,

            count
        };
    };

    struct geometry_header
    {
        float level_of_detail_threshold;
        UINT sub_mesh_count;
        UINT size_of_sub_meshes;
    };

    struct geometry_data
    {
        UINT level_of_detail_count;
        struct geometry_header header;
    };

    struct geometry_sub_mesh_header
    {
        UINT element_size;
        UINT vertex_count;
        UINT index_count;
        UINT elements_type;
        UINT primitive_topology;
    };

    struct level_of_detail_offset_count
    {
        UINT16 offset;
        UINT16 count;
    };

    struct material_type {
        enum type : UINT {
            opaque,

            count
        };
    };

    struct material_surface
    {
        XMFLOAT4 base_color{ 1.f, 1.f, 1.f, 1.f };
        XMFLOAT3 emissive{ 0.f, 0.f, 0.f };
        float emissive_intensity{ 1.f };
        float ambient_occlusion{ 1.f };
        float metallic{ 0.f };
        float roughness{ 1.f };
    };

    struct material_init_info
    {
        UINT* texture_ids;
        material_surface surface;
        material_type::type type;
        UINT texture_count; // NOTE: textures are optional, so, texture count may be 0 and texture_ids may be nullptr.
        UINT shader_ids[shaders::shader_type::count]{ Invalid_Index, Invalid_Index, Invalid_Index, Invalid_Index, Invalid_Index, Invalid_Index, Invalid_Index, Invalid_Index };
    };

    namespace sub_mesh {
        //struct views_cache
        //{
        //    D3D12_GPU_VIRTUAL_ADDRESS* const position_buffers;
        //    D3D12_GPU_VIRTUAL_ADDRESS* const element_buffers;
        //    D3D12_INDEX_BUFFER_VIEW* const index_buffer_views;
        //    D3D_PRIMITIVE_TOPOLOGY* const primitive_topologies;
        //    UINT* const elements_types;
        //};

        UINT add(const UINT8*& data);
        void remove(UINT id);
        D3D_PRIMITIVE_TOPOLOGY get_primitive_topology(UINT id);
        UINT get_views_element_type(UINT id);
        //void get_views(const UINT* const gpu_ids, UINT id_count, const views_cache& cache);
        void get_views(UINT id_count, const graphic_pass::graphic_cache& cache);
    } // namespace sub_mesh

    namespace texture {
        UINT add(const UINT8* const data);
        void remove(UINT id);
        void get_descriptor_indices(const UINT *const texture_ids, UINT id_count, UINT* const indices);
    } // namespace texture

    namespace material {

        UINT add(material_init_info info);
        void remove(UINT id);
        void get_materials(UINT id_count, graphic_pass::graphic_cache& cache);
    } // namespace material

    namespace render_item {
        struct items_cache
        {
            UINT* const entity_ids;
            UINT* const sub_mesh_gpu_ids;
            UINT* const material_ids;
            ID3D12PipelineState** const graphic_pass_psos;
            ID3D12PipelineState** const depth_psos;
        };

        UINT add(UINT entity_id, UINT geometry_content_id, UINT material_count, const UINT* const material_ids);
        void remove(UINT id);
        void get_d3d12_render_item_ids(const core::frame_info& info, utl::vector<UINT>& d3d12_render_item_ids);
        //void get_items(const UINT* const d3d12_render_item_ids, UINT id_count, const items_cache& cache);
        void get_items(const UINT* const d3d12_render_item_ids, UINT id_count, const graphic_pass::graphic_cache& cache);

    }

    bool initialize();
    void shutdown();

    // ContentToEngine.cpp
    UINT create_resource(const void* const data, asset_type::type type);
    void destroy_resource(const UINT id, asset_type::type type);

    void get_sub_mesh_gpu_ids(UINT geometry_context_id, UINT id_count, UINT* const gpu_ids);
    void get_lod_offsets_counts(const UINT* const geometry_ids, const float* thresholds, UINT id_count, utl::vector<level_of_detail_offset_count>& offsets_counts);

}
