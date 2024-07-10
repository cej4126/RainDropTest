#include "GraphicPass.h"
#include "Vector.h"
#include "Content.h"
#include "RainDrop.h"

namespace d3d12::graphic_pass
{
    namespace {

        graphic_cache frame_cache;

        void prepare_render_frame(const d3d12_frame_info& d3d12_info)
        {
            assert(d3d12_info.info);
            graphic_cache& cache{ frame_cache };
            cache.clear();

            content::render_item::get_d3d12_render_item_ids(*d3d12_info.info, cache.d3d12_render_item_ids);
            cache.resize();

            const UINT item_count{ cache.size() };
            //const content::render_item::items_cache items_cache{ cache.items_cache() };
            content::render_item::get_items(cache.d3d12_render_item_ids.data(), item_count, cache);

            content::sub_mesh::get_views(item_count, cache);

            content::material::get_materials(item_count, cache);
        }
    }

    bool initialize()
    {
        return true;
    }

    void shutdown()
    {}

    void set_size(DirectX::XMUINT2 size)
    {}

    void depth_prepass(const d3d12_frame_info& d3d12_info)
    {
        prepare_render_frame(d3d12_info);

        const graphic_cache& cache{ frame_cache };
        const UINT items_count{ cache.size() };



    }
}
