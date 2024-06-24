#pragma once

#include "stdafx.h"

namespace d3d12::content
{

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

    UINT create_resource(const void* const data, asset_type::type type);
    void destroy_resource(const UINT id, asset_type::type type);
}
