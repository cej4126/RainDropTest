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

using namespace Microsoft::WRL;
namespace app {
    namespace {
        struct Scene {
            game_entity::entity entity{};
            camera::Camera camera{};
            surface::Surface surface{};
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

        UINT create_material()
        {
            content::material_init_info info{};
            info.type = content::material_type::opaque;
            info.shader_ids[shaders::shader_type::vertex] = shaders::engine_shader::vertex_shader_vs;
            info.shader_ids[shaders::shader_type::pixel] = shaders::engine_shader::pixel_shader_ps;
            return content::create_resource(&info, content::asset_type::material);
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

    //bool dx_app::initialize()
    //{
    //    WCHAR assetsPath[512];
    //    GetAssetsPath(assetsPath, _countof(assetsPath));
    //    //m_assetsPath = assetsPath;
    //
    //    return true;
    //}
    //
    //void dx_app::run()
    //{
    //
    //}
    //
    //void dx_app::shutdown()
    //{
    //}

    //game_entity::entity create_entity_item(XMFLOAT3 position, XMFLOAT3 rotation, const char* script_name)
    //{
    //    transform::init_info transform_info{};
    //    XMVECTOR quat{ XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&rotation)) };
    //    XMFLOAT4A rot_quat;
    //    XMStoreFloat4A(&rot_quat, quat);
    //    memcpy(&transform_info.rotation[0], &rot_quat.x, sizeof(transform_info.rotation));
    //    memcpy(&transform_info.position[0], &position.x, sizeof(transform_info.position));

    //    script::init_info script_info{};
    //    if (strlen(script_name) > 0)
    //    {
    //        script_info.script_creator = script::detail::get_script_creator(std::hash<std::string>()(script_name));
    //        assert(script_info.script_creator);
    //    }

    //    game_entity::entity_info entity_info{};
    //    entity_info.transform = &transform_info;
    //    entity_info.script = &script_info;
    //    game_entity::entity entity = game_entity::create(entity_info).get_id();
    //    assert(entity.is_valid());
    //    return entity;
    //}
    //
    //void create_render_items()
    //{
    //    m_cube_entity_id = create_entity_item({ 1.0f, 0.f, 0.f }, { 0.f, 0.f, 0.f }, "").get_id();
    //
    //    assert(std::filesystem::exists("../cube.model"));
    //    std::thread threads[]{
    //        std::thread{ [] { m_cube_model_id = load_model("../cube.model"); }},
    //    };
    //
    //    for (auto& t : threads)
    //    {
    //        t.join();
    //    }
    //
    //    m_material_id = create_material();
    //    UINT materials[]{ m_material_id };
    //
    //    geometry::init_info geometry_info{};
    //
    //
    //    m_cube_item_id = content::render_item::add(m_cube_entity_id, m_ cube_model_id, _countof(materials), &materials[0]);
    //}
    //
    //
    //std::wstring dx_app::GetAssetFullPath(LPCWSTR assetName)
    //{
    //    return m_assetsPath + assetName;
    //}

    // Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
    // If no such adapter can be found, *ppAdapter will be set to nullptr.
    //_Use_decl_annotations_
    //void dx_app::GetHardwareAdapter(_In_ IDXGIFactory2* pFactory, _Outptr_opt_result_maybenull_ IDXGIAdapter1** ppAdapter)
    //{
    //    ComPtr<IDXGIAdapter1> adapter;
    //    *ppAdapter = nullptr;
    //
    //    for (UINT adapter_index = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapter_index, &adapter); ++adapter_index)
    //    {
    //        DXGI_ADAPTER_DESC1 desc;
    //        adapter->GetDesc1(&desc);
    //
    //        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
    //        {
    //            // Don't select the Basic Render Driver adapter.
    //            // If you want a software adapter, pass in "/warp" on the command line.
    //            continue;
    //        }
    //
    //        // Check to see if the adapter supports Direct3D 12, but don't create the
    //        // actual device yet.
    //        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
    //        {
    //            break;
    //        }
    //    }
    //
    //    *ppAdapter = adapter.Detach();
    //}
    //
    //void dx_app::SetCustomWindowText(LPCWSTR text)
    //{
    //
    //}
    //
    //void dx_app::ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc)
    //{
    //    for (int i = 1; i < argc; ++i)
    //    {
    //        if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 ||
    //            _wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
    //        {
    //            m_use_warp_device = true;
    //            m_title = m_title + L" (WARP)";
    //        }
    //    }
    //}

    void create_scene(Scene& scene, windows::window_init_info info)
    {
        scene.window = windows::create(&info);
        scene.surface = surface::create(scene.window);

        // x in(-) / out(+), y up(+) / down(-), z left(-) / right(+)
        scene.entity = create_entity_item({ 10.f, 0.f, 0.f }, { math::dtor(0.f), math::dtor(-90.f), math::dtor(0.f) }, "camera_script");
        scene.camera = camera::create(camera::perspective_camera_init_info(scene.entity.get_id()));
        scene.camera.aspect_ratio((float)(scene.surface.width() / scene.surface.height()));
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
            {&win_proc, nullptr, L"Main", 100, 100, 800, 400 },
        };
        static_assert(_countof(info) == _countof(m_scenes));

        for (UINT i{ 0 }; i < _countof(m_scenes); ++i)
        {
            create_scene(m_scenes[i], info[i]);
        }

        app::create_render_items();

        //generate_lights();

        render_item_id_cache.resize(1);
        geometry::get_render_item_ids(render_item_id_cache.data(), (UINT)render_item_id_cache.size());

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
    }

    void app_shutdown()
    {

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
            if (m_scenes[i].surface.is_valid())
            {
                float thresholds[1];

                core::frame_info info{};
                info.render_item_ids = render_item_id_cache.data();
                info.render_item_count = 1;
                info.thresholds = &thresholds[0];
                info.camera_id = m_scenes[i].camera.get_id();

                assert(_countof(thresholds) >= info.render_item_count);
                m_scenes[i].surface.render(info);
            }
        }

        //surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
        //// Wait for the previous Present to complete.
        //WaitForSingleObjectEx(surface.swap_chain_event(), 100, FALSE);

        //camera::Camera& camera{ camera::get(m_camera_id) };
        //camera.update();

        //m_rain_drop.update(m_camera_id, m_frame_index);

        //OnRender();
        timer.end();
    }

    void dx_app::shutdown()
    {
        app_shutdown();
    }
}