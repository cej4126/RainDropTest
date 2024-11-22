#include "stdafx.h"
#include "DXApp.h"
#include "Shaders.h"
#include "Core.h"
#include "Window.h"
#include "Camera.h"
#include "Surface.h"
#include "Math.h"
#include "Transform.h"
#include "Scripts.h"
#include "Geometry.h"
#include "Utilities.h"
#include "Content.h"
#include "AppItems.h"
#include "TimeProcess.h"
#include "Lights.h"

using namespace Microsoft::WRL;
namespace app {
    namespace {
        struct Scene {
            game_entity::entity entity{};
            UINT camera_id{ Invalid_Index };
            UINT surface_id{ Invalid_Index };
            windows::window window{};
        };

        struct input_info
        {
            input::input_code::code code;
            float multipler;
            input::axis::type axis;
        };

        Scene m_scenes[1];
        time_process timer{};

        utl::vector<UINT> render_item_id_cache;

        [[nodiscard]] UINT load_model(const char* path)
        {
            std::unique_ptr<UINT8[]> model;
            UINT64 size{ 0 };

            utl::read_file(path, model, size);

            const UINT model_id{ content::create_resource(model.get(), content::asset_type::mesh) };
            assert(model_id != Invalid_Index);
            return model_id;
        }

        UINT m_cube_model_id;
        UINT m_cube_entity_id{ Invalid_Index };
        UINT m_cube_item_id{ Invalid_Index };

        UINT m_material_id{ Invalid_Index };
        struct input_info m_input_data[] =
        {
            { input::input_code::key_a,  1.f, input::axis::x },
            { input::input_code::key_d, -1.f, input::axis::x },
            { input::input_code::key_w,  1.f, input::axis::z },
            { input::input_code::key_s, -1.f, input::axis::z },
            { input::input_code::key_q, -1.f, input::axis::y },
            { input::input_code::key_e,  1.f, input::axis::y }
        };

    } // anonymous namespace

    LRESULT win_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        switch (msg)
        {
        case WM_DESTROY:
        {}
        break;
        }
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    void create_scene(Scene& scene, windows::window_init_info info)
    {
        scene.window = windows::create(&info);
        scene.surface_id = surface::create(scene.window);

        // x in(-) / out(+), y up(+) / down(-), z left(-) / right(+)
        scene.entity = create_entity_item({ 5.f, 0.f, 0.f }, { 0.f, 1.5f * math::pi, 0.f }, { 1.f, 1.f, 1.f }, nullptr, "camera_script");
        scene.camera_id = camera::create(camera::perspective_camera_init_info(scene.entity.get_id()));
        const surface::Surface& surface{ surface::get_surface(scene.surface_id) };

        camera::Camera& camera{ camera::get(scene.camera_id) };
        camera.aspect_ratio((float)(surface.width() / surface.height()));
    }

    void destroy_scene(Scene& scene)
    {
        Scene temp{ scene };
        scene = {};

        if (scene.surface_id != Invalid_Index)
        {
            surface::remove(scene.surface_id);
        }
        if (scene.window.is_valid())
        {
            windows::remove(scene.window.get_id());
        }
    }

    bool app_initialize()
    {
#ifdef _DEBUG
        while (!shaders::compile_shaders())
        {
            // Pop up a message box allowing the user to retry compilation.
            if (MessageBox(nullptr, L"Failed to compile engine shaders.", L"Shader Compilation Error", MB_RETRYCANCEL) != IDRETRY)
                assert(false);
        }
#else
        assert(!shaders::compile_shaders());
#endif

        if (!core::initialize())
        {
            return false;
        }

        windows::window_init_info info[]
        {
            {&win_proc, nullptr, L"Main", 100, 100, 400, 400 },
        };
        static_assert(_countof(info) == _countof(m_scenes));

        for (UINT i{ 0 }; i < _countof(m_scenes); ++i)
        {
            create_scene(m_scenes[i], info[i]);
        }

        app::create_render_items();

        lights::generate_lights();

        render_item_id_cache.resize(1);
        geometry::get_geometry_item_ids(render_item_id_cache.data(), (UINT)render_item_id_cache.size());

        input::input_source source{};
        source.binding = std::hash<std::string>()("move");
        source.source_type = input::input_source::keyboard;

        for (int i{ 0 }; i < _countof(m_input_data); ++i)
        {
            source.code = m_input_data[i].code;
            source.multiplier = m_input_data[i].multipler;
            source.axis = m_input_data[i].axis;
            input::bind(source);
        }

        return true;
    }

    void app_shutdown()
    {
        input::unbind(std::hash<std::string>()("move"));
        lights::remove_lights();
        app::destroy_render_items();

        for (UINT i{ 0 }; i < _countof(m_scenes); ++i)
        {
            destroy_scene(m_scenes[i]);
        }

        core::shutdown();
    }

    bool dx_app::initialize()
    {
        return app_initialize();
    }

    void dx_app::run()
    {
        timer.begin();
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const float dt{ timer.dt_avg() };

        script::update(dt);

        for (UINT i{ 0 }; i < _countof(m_scenes); ++i)
        {
            if (m_scenes[i].surface_id != Invalid_Index)
            {
                float thresholds[1]{};

                core::frame_info info{};
                info.render_item_ids = render_item_id_cache.data();
                info.render_item_count = 1;
                info.thresholds = &thresholds[0];
                info.camera_id = m_scenes[i].camera_id;

                assert(_countof(thresholds) >= info.render_item_count);
                const surface::Surface& surface{ surface::get_surface(m_scenes[i].surface_id) };
                surface.render(info);
            }
        }

        timer.end();
    }

    void dx_app::shutdown()
    {
        app_shutdown();
    }
}