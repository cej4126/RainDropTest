#pragma once
//
//#include "stdafx.h"
//#include "Shaders.h"
//
//namespace content
//{
//
//    struct primitive_topology {
//        enum type : UINT {
//            point_list = 1,
//            line_list,
//            line_strip,
//            triangle_list,
//            triangle_strip,
//
//            count
//        };
//    };
//
//    struct asset_type {
//        enum type : UINT {
//            unknown = 0,
//            material,
//            mesh,
//            texture,
//
//            count
//        };
//    };
//
//    struct geometry_header
//    {
//        float level_of_detail_threshold;
//        UINT sub_mesh_count;
//        UINT size_of_sub_meshes;
//    };
//
//    struct geometry_data
//    {
//        UINT level_of_detail_count;
//        struct geometry_header header;
//    };
//
//    struct geometry_sub_mesh_header
//    {
//        UINT element_size;
//        UINT vertex_count;
//        UINT index_count;
//        UINT elements_type;
//        UINT primitive_topology;
//    };
//
//    struct level_of_detail_offset_count
//    {
//        UINT16 offset;
//        UINT16 count;
//    };
//
//    struct material_type {
//        enum type : UINT {
//            opaque,
//
//            count
//        };
//    };
//
//    struct material_surface
//    {
//        XMFLOAT4 base_color{ 1.f, 1.f, 1.f, 1.f };
//        XMFLOAT3 emissive{ 0.f, 0.f, 0.f };
//        float emissive_intensity{ 1.f };
//        float ambient_occlusion{ 1.f };
//        float metallic{ 0.f };
//        float roughness{ 1.f };
//    };
//
//    struct material_init_info
//    {
//        UINT* texture_ids;
//        material_surface surface;
//        material_type::type type;
//        UINT texture_count{ 0 };
//        UINT shader_ids[shaders::shader_type::count]{ Invalid_Index, Invalid_Index, Invalid_Index, Invalid_Index, Invalid_Index, Invalid_Index, Invalid_Index, Invalid_Index };
//    };
//
//
//    UINT create_resource(const void* const data, asset_type::type type);
//    void destroy_resource(const UINT id, asset_type::type type);
//
//    void get_sub_mesh_gpu_ids(UINT geometry_context_id, UINT id_count, UINT* const gpu_ids);
//    void get_lod_offsets_counts(const UINT* const geometry_ids, const float* thresholds, UINT id_count, utl::vector<level_of_detail_offset_count>& offsets_counts);
//}
