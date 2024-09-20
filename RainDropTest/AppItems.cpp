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

namespace app {

    namespace {

        UINT cube_model_id{ Invalid_Index };
        UINT cube_entity_id{ Invalid_Index };
        UINT cube_item_id{ Invalid_Index };
        UINT material_id{ Invalid_Index };


    } // anonymous namespace

    game_entity::entity create_entity_item(XMFLOAT3 position, XMFLOAT3 rotation, const char* script_name)
    {
        transform::init_info transform_info{};
        XMVECTOR quat{ XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&rotation)) };
        XMFLOAT4A rot_quat;
        XMStoreFloat4A(&rot_quat, quat);
        memcpy(&transform_info.rotation[0], &rot_quat.x, sizeof(transform_info.rotation));
        memcpy(&transform_info.position[0], &position.x, sizeof(transform_info.position));

        script::init_info script_info{};
        if (strlen(script_name) > 0)
        {
            script_info.script_creator = script::detail::get_script_creator(std::hash<std::string>()(script_name));
            assert(script_info.script_creator);
        }

        game_entity::entity_info entity_info{};
        entity_info.transform = &transform_info;
        entity_info.script = &script_info;
        game_entity::entity entity = game_entity::create(entity_info).get_id();
        assert(entity.is_valid());
        return entity;
    }

    [[nodiscard]] UINT load_model(const char* path)
    {
        std::unique_ptr<UINT8[]> model;
        UINT64 size{ 0 };

        utl::read_file(path, model, size);

        const UINT model_id{ content::create_resource(model.get(), content::asset_type::mesh) };
        assert(model_id != Invalid_Index);
        return model_id;
    }

    void create_material()
    {
        content::material_init_info info{};
        info.type = content::material_type::opaque;
        info.shader_ids[shaders::shader_type::vertex] = shaders::engine_shader::vertex_shader_vs;
        info.shader_ids[shaders::shader_type::pixel] = shaders::engine_shader::pixel_shader_ps;
        material_id = content::create_resource(&info, content::asset_type::material);
    }

    void create_render_items()
    {
        cube_entity_id = create_entity_item({ 1.0f, 0.f, 0.f }, { 0.f, 0.f, 0.f }, "").get_id();

        assert(std::filesystem::exists("../cube.model"));

        std::thread threads[]{
            std::thread{ [] { cube_model_id = load_model("../cube.model"); }},
        };

        for (auto& t : threads)
        {
            t.join();
        }

        create_material();
        UINT materials[]{ material_id };

        cube_item_id =  content::render_item::add(cube_entity_id, cube_model_id, _countof(materials), &materials[0]);
    }
}