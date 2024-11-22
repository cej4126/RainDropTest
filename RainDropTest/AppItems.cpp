#include <filesystem>
#include <functional>
#include <string>
#include "AppItems.h"
#include "Transform.h"
#include "Scripts.h"
#include "Entity.h"
#include "Content.h"
#include "Utilities.h"
#include "Core.h"
#include "Vector.h"


namespace app {

    namespace {
        struct material_type {
            enum type : UINT {
                default_material,
                dirty_material,

                count
            };
        };
        
        UINT cube_model_id{ Invalid_Index };
        UINT cube_entity_id{ Invalid_Index };
        UINT cube_item_id{ Invalid_Index };
        UINT material_ids[material_type::count]{};

        struct texture_usage {
            enum usage : UINT {
                //fem_bot_ambient_occlusion = 0,
                //fem_bot_base_color,
                //fem_bot_emissive,
                //fem_bot_metal_rough,
                //fem_bot_normal,

                dirty_metal_ambient_occlusion,
                dirty_metal_base_color,
                dirty_metal_emissive,
                dirty_metal_metal_rough,
                dirty_metal_normal,

                count
            };
        };

        UINT texture_ids[texture_usage::count];

        void remove_model(UINT model_id)
        {
            if (model_id != Invalid_Index)
            {
                content::destroy_resource(model_id, content::asset_type::mesh);
            }
        }

    } // anonymous namespace

    game_entity::entity create_entity_item(XMFLOAT3 position, XMFLOAT3 rotation, XMFLOAT3 scale, geometry::init_info* geometry_info, const char* script_name)
    {
        transform::init_info transform_info{};
        XMVECTOR quat{ XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&rotation)) };
        XMFLOAT4A rot_quat;
        XMStoreFloat4A(&rot_quat, quat);
        memcpy(&transform_info.rotation[0], &rot_quat.x, sizeof(transform_info.rotation));
        memcpy(&transform_info.position[0], &position.x, sizeof(transform_info.position));
        memcpy(&transform_info.scale[0], &scale.x, sizeof(transform_info.scale));

        script::init_info script_info{};
        if (script_name)
        {
            script_info.script_creator = script::detail::get_script_creator(std::hash<std::string>()(script_name));
            assert(script_info.script_creator);
        }

        game_entity::entity_info entity_info{};
        entity_info.transform = &transform_info;
        entity_info.script = &script_info;
        entity_info.geometry = geometry_info;
        game_entity::entity entity = game_entity::create(entity_info);
        assert(entity.is_valid());
        return entity;
    }

    void remove_game_entity(UINT id)
    {
        game_entity::remove(id);
    }

    [[nodiscard]] UINT load_asset(const char* path, content::asset_type::type type)
    {
        std::unique_ptr<UINT8[]> buffer;
        UINT64 size{ 0 };

        utl::read_file(path, buffer, size);

        const UINT model_id{ content::create_resource(buffer.get(), type) };
        assert(model_id != Invalid_Index);
        return model_id;
    }

    void create_material()
    {
        content::material_init_info info{};
        info.type = content::material_type::opaque;
        info.shader_ids[shaders::shader_type::vertex] = shaders::engine_shader::normal_shader_vs;
        info.shader_ids[shaders::shader_type::pixel] = shaders::engine_shader::pixel_shader_ps;
        material_ids[material_type::default_material] = content::create_resource(&info, content::asset_type::material);

        info.shader_ids[shaders::shader_type::pixel] = shaders::engine_shader::texture_shader_ps;
        info.shader_ids[shaders::shader_type::vertex] = shaders::engine_shader::normal_texture_shader_vs;
        info.texture_count = texture_usage::dirty_metal_normal - texture_usage::dirty_metal_ambient_occlusion + 1;
        info.texture_ids = &texture_ids[texture_usage::dirty_metal_ambient_occlusion];
        material_ids[material_type::dirty_material] = content::create_resource(&info, content::asset_type::material);
    }

    void create_render_items()
    {
        memset(&texture_ids[0], 0xff, sizeof(UINT) * _countof(texture_ids));
//#define NO_THREAD
#ifdef NO_THREAD
        //texture_ids[texture_usage::fem_bot_ambient_occlusion] =     load_asset("../fem_bot_ambient_occlusion.texture", content::asset_type::texture); }},
        //texture_ids[texture_usage::fem_bot_base_color] =            load_asset("../fem_bot_base_color.texture", content::asset_type::texture); }},
        //texture_ids[texture_usage::fem_bot_emissive] =              load_asset("../fem_bot_emissive.texture", content::asset_type::texture); }},
        //texture_ids[texture_usage::fem_bot_metal_rough] =           load_asset("../fem_bot_roughness.texture", content::asset_type::texture); }},
        //texture_ids[texture_usage::fem_bot_normal] =                load_asset("../fem_bot_normal.texture", content::asset_type::texture); }},

        texture_ids[texture_usage::dirty_metal_ambient_occlusion] = load_asset("../dirty_metal_ambient_occlusion.texture", content::asset_type::texture);
        texture_ids[texture_usage::dirty_metal_base_color] = load_asset("../dirty_metal_base_color.texture", content::asset_type::texture);
        texture_ids[texture_usage::dirty_metal_emissive] = load_asset("../dirty_metal_emissive.texture", content::asset_type::texture);
        texture_ids[texture_usage::dirty_metal_metal_rough] = load_asset("../dirty_metal_roughness.texture", content::asset_type::texture);
        texture_ids[texture_usage::dirty_metal_normal] = load_asset("../dirty_metal_normal.texture", content::asset_type::texture);

        cube_model_id = load_asset("../cube.model", content::asset_type::mesh);

#else
        std::thread threads[]
        {
            //std::thread{ [] { texture_ids[texture_usage::fem_bot_ambient_occlusion] =     load_asset("../fem_bot_ambient_occlusion.texture", content::asset_type::texture); }},
            //std::thread{ [] { texture_ids[texture_usage::fem_bot_base_color] =            load_asset("../fem_bot_base_color.texture", content::asset_type::texture); }},
            //std::thread{ [] { texture_ids[texture_usage::fem_bot_emissive] =              load_asset("../fem_bot_emissive.texture", content::asset_type::texture); }},
            //std::thread{ [] { texture_ids[texture_usage::fem_bot_metal_rough] =           load_asset("../fem_bot_roughness.texture", content::asset_type::texture); }},
            //std::thread{ [] { texture_ids[texture_usage::fem_bot_normal] =                load_asset("../fem_bot_normal.texture", content::asset_type::texture); }},
        
            std::thread{ [] { texture_ids[texture_usage::dirty_metal_ambient_occlusion] = load_asset("../dirty_metal_ambient_occlusion.texture", content::asset_type::texture); }},
            std::thread{ [] { texture_ids[texture_usage::dirty_metal_base_color] =        load_asset("../dirty_metal_base_color.texture", content::asset_type::texture); }},
            std::thread{ [] { texture_ids[texture_usage::dirty_metal_emissive] =          load_asset("../dirty_metal_emissive.texture", content::asset_type::texture); }},
            std::thread{ [] { texture_ids[texture_usage::dirty_metal_metal_rough] =       load_asset("../dirty_metal_roughness.texture", content::asset_type::texture); }},
            std::thread{ [] { texture_ids[texture_usage::dirty_metal_normal] =            load_asset("../dirty_metal_normal.texture", content::asset_type::texture); }},

            std::thread{ [] { cube_model_id =                                             load_asset("../cube.model", content::asset_type::mesh); }},
        };

        for (auto& t : threads)
        {
            t.join();
        }
#endif
        create_material();

        geometry::init_info geometry_info{};
        geometry_info.material_count = 1;
        geometry_info.material_ids = &material_ids[material_type::dirty_material];
        geometry_info.geometry_content_id = cube_model_id;
        cube_entity_id = create_entity_item({ 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f }, { 0.4f, 0.4f, 0.4f }, &geometry_info, nullptr).get_id();

        cube_item_id =  content::render_item::add(cube_entity_id, cube_model_id, 1, &material_ids[0]);
    }

    void destroy_render_items()
    {
        remove_game_entity(cube_entity_id);

        remove_model(cube_model_id);

        for (UINT id : material_ids)
        {
            content::destroy_resource(id, content::asset_type::material);
        }

        for (UINT id : texture_ids)
        {
            content::destroy_resource(id, content::asset_type::texture);
        }
    }

    //void get_render_item_ids(UINT* const item_ids, UINT count)
    //{
    //    assert(render_item_ids.size() >= count);
    //    memcpy(item_ids, render_item_ids.data(), count * sizeof(UINT));
    //}
}