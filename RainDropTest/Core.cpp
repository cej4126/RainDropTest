#include "Core.h"
#include <functional>
#include <string>
#include "stdafx.h"
#include "Helpers.h"
#include "Command.h"
#include "Main.h"
#include "Barriers.h"
#include "Upload.h"
#include "Buffers.h"
#include "Utilities.h"
#include "ContentToEngine.h"
#include "Shaders.h"
#include <filesystem>
#include "Content.h"
#include "Transform.h"
#include "Entity.h"
#include "Shaders.h"
#include "Resources.h"
#include "GraphicPass.h"
#include "SharedTypes.h"
#include "FreeList.h"
#include "Input.h"
#include "Scripts.h"
#include "RainDrop.h"

//extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 614; }
//extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace d3d12 {

    namespace {
        Depth_Buffer m_depth_buffer{};

        UINT cube_model_id{ Invalid_Index };
        UINT cube_entity_id{ Invalid_Index };
        UINT cube_item_id{ Invalid_Index };
        UINT material_id{ Invalid_Index };

        game_entity::entity camera_entity{};

        barriers::resource_barrier resource_barriers{};

        utl::vector<UINT> surface_ids;

        d3d12_frame_info get_d3d12_frame_info(const frame_info& info, constant_buffer& cbuffer, const surface::Surface& surface)
        {
            camera::Camera& camera{ camera::get(info.camera_id) };
            camera.update();
            hlsl::GlobalShaderData global_shader_data{};

            XMStoreFloat4x4A(&global_shader_data.View, camera.view());
            XMStoreFloat4x4A(&global_shader_data.InverseView, camera.inverse_view());
            XMStoreFloat4x4A(&global_shader_data.Projection, camera.projection());
            XMStoreFloat4x4A(&global_shader_data.InverseProjection, camera.inverse_projection());
            XMStoreFloat4x4A(&global_shader_data.ViewProjection, camera.view_projection());
            XMStoreFloat4x4A(&global_shader_data.InverseViewProjection, camera.inverse_view_projection());
            XMStoreFloat3(&global_shader_data.CameraPosition, camera.position());
            XMStoreFloat3(&global_shader_data.CameraDirection, camera.direction());

            cbuffer.clear();
            hlsl::GlobalShaderData* const shader_data{ cbuffer.allocate<hlsl::GlobalShaderData>() };
            // TODO: handle the case when cbuffer is full.
            memcpy(shader_data, &global_shader_data, sizeof(hlsl::GlobalShaderData));

            d3d12_frame_info d3d12_info
            {
                &info,
                cbuffer.gpu_address(shader_data)
            };

            return d3d12_info;
        }
    } // anonymous namespace

    Core::Core(UINT width, UINT height, std::wstring name) :
        DXSample(width, height, name),
        m_frame_index(0),
        m_render_context_fence_value(0)
    {
        srand(0);

        m_srv_index = 0;
        ZeroMemory(m_frame_fence_values, sizeof(m_frame_fence_values));

        m_render_context_fence_value1 = 0;
        m_rain_drop.set_rain_state(true);

        struct
        {
            input::input_code::code code;
            float multipler;
            input::axis::type axis;
        } input_data[] =
        {
            { input::input_code::key_a,  1.f, input::axis::x },
            { input::input_code::key_d, -1.f, input::axis::x },
            { input::input_code::key_w,  1.f, input::axis::z },
            { input::input_code::key_s, -1.f, input::axis::z },
            { input::input_code::key_q, -1.f, input::axis::y },
            { input::input_code::key_e,  1.f, input::axis::y }
        };
        input::input_source source{};
        source.binding = std::hash<std::string>()("move");
        source.source_type = input::input_source::keyboard;

        for (int i{ 0 }; i < _countof(input_data); ++i)
        {
            source.code = input_data[i].code;
            source.multiplier = input_data[i].multipler;
            source.axis = input_data[i].axis;
            input::bind(source);
        }
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
        d3d12::content::material_init_info info{};
        info.type = d3d12::content::material_type::opaque;
        info.shader_ids[shaders::shader_type::vertex] = shaders::engine_shader::vertex_shader_vs;
        info.shader_ids[shaders::shader_type::pixel] = shaders::engine_shader::pixel_shader_ps;
        material_id = content::create_resource(&info, content::asset_type::material);
    }

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

        cube_item_id = content::render_item::add(cube_entity_id, cube_model_id, _countof(materials), &materials[0]);
    }

    void Core::OnInit(UINT width, UINT height)
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

        LoadPipeline();
        LoadAssets(width, height);
        m_rain_drop.CreateAsyncContexts();

        create_render_items();

        // x in(-) / out(+), y up(+) / down(-), z left(-) / right(+)
        camera_entity = create_entity_item({ 10.f, 0.f, 0.f }, { math::dtor(0.f), math::dtor(-90.f), math::dtor(0.f) }, "camera_script");
        m_camera_id = camera::create(camera::perspective_camera_init_info(camera_entity.get_id()));
        camera::aspect_ratio(m_camera_id, (float)width / height);
    }

    // Load the rendering pipeline dependencies.
    void Core::LoadPipeline()
    {
#if defined(_DEBUG)
        // Enable the D3D12 debug layer.
        {
            ComPtr<ID3D12Debug> debug_controller;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
            {
                debug_controller->EnableDebugLayer();
            }
        }
#endif

        ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)));

        if (m_use_warp_device)
        {
            // Wrap device
            ComPtr<IDXGIAdapter> warp_adapter;
            ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));
            ThrowIfFailed(D3D12CreateDevice(warp_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
        }
        else
        {
            ComPtr<IDXGIAdapter1> hardware_adapter;
            GetHardwareAdapter(m_factory.Get(), &hardware_adapter);

            ThrowIfFailed(D3D12CreateDevice(hardware_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
        }

        new (&m_command) command::Command(m_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
        if (!m_command.command_queue())
        {
            throw;
        }

        bool result{ true };
        result &= m_rtv_desc_heap.initialize(512, false);
        result &= m_dsv_desc_heap.initialize(512, false);
        result &= m_srv_desc_heap.initialize(4096, true);
        result &= m_uav_desc_heap.initialize(512, false);
        if (!result)
        {
            throw;
        }

        NAME_D3D12_OBJECT(m_rtv_desc_heap.heap(), L"RTV Descriptor Heap");
        NAME_D3D12_OBJECT(m_dsv_desc_heap.heap(), L"DSV Descriptor Heap");
        NAME_D3D12_OBJECT(m_srv_desc_heap.heap(), L"SRV Descriptor Heap");
        NAME_D3D12_OBJECT(m_uav_desc_heap.heap(), L"UAV Descriptor Heap");

        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            new (&m_constant_buffers[i]) constant_buffer{ 1024 * 1024, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT };
            NAME_D3D12_OBJECT_INDEXED(m_constant_buffers[i].buffer(), i, L"Global Constant Buffer");
        }

        m_rain_drop.create_descriptor_heap();

        if (!(graphic_pass::initialize() &&
            upload::initialize() &&
            content::initialize() &&
            shaders::initialize()))
        {
            throw;
        }
    }

    void Core::LoadAssets(UINT width, UINT height)
    {

        m_rain_drop.initialize(width, height);

        assert(width && height);
        m_depth_buffer.release();
        m_depth_buffer = Depth_Buffer{ width, height };

        // Close the command list and execute it to begin the initial GPU setup and uploads.
        ThrowIfFailed(m_command.command_list()->Close());
        ID3D12CommandList* pp_command_lists[] = { m_command.command_list() };
        m_command.command_queue()->ExecuteCommandLists(_countof(pp_command_lists), pp_command_lists);

        // Create synchronization objects and wait until assets have been uploaded to the GPU.
        {
            ThrowIfFailed(core::device()->CreateFence(m_render_context_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_render_context_fence)));
            ++m_render_context_fence_value;

            m_render_context_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (m_render_context_fence_event == nullptr)
            {
                ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
            }
        }
        WaitForRenderContext();
    }

    void Core::run()
    {
        OnUpdate(0.0165f);
    }

    // Update frame-based values.
    void Core::OnUpdate(float dt)
    {
        script::update(dt);

        surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
        // Wait for the previous Present to complete.
        WaitForSingleObjectEx(surface.swap_chain_event(), 100, FALSE);

        camera::Camera& camera{ camera::get(m_camera_id) };
        camera.update();

        m_rain_drop.update(m_camera_id, m_frame_index);

        OnRender();
    }

    void Core::render()
    {

        UINT render_items[1]{};
        render_items[0] = cube_item_id;

        frame_info info{};
        info.render_item_ids = &render_items[0];
        info.render_item_count = 1;
        info.camera_id = m_camera_id;

        surface::Surface& surface{ surface::get_surface(surface_ids[0]) };

        const d3d12_frame_info d3d12_info{ get_d3d12_frame_info(info, m_constant_buffers[m_frame_index], surface) };

        graphic_pass::render(m_command.command_list(), d3d12_info);
    }

    // Render the scene.
    void Core::OnRender()
    {
        m_rain_drop.sync_compute_tread(m_command.command_queue());

        m_command.BeginFrame();

        if (deferred_releases_flag[m_frame_index])
        {
            process_deferred_releases(m_frame_index);
        }

        // Record all the commands we need to render the scene into the command list.
        PopulateCommandList();

        MoveToNextFrame();
    }

    // Fill the command list with all the render commands and dependent state.
    void Core::PopulateCommandList()
    {
        m_rain_drop.populate_command_list(m_command.command_list(), m_frame_index);

        surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
        m_command.command_list()->RSSetScissorRects(1, &surface.scissor_rect());

        barriers::transition_resource(m_command.command_list(), surface.back_buffer(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        // Indicate that the back buffer will be used as a render target.
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{ surface.rtv().ptr };
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = m_depth_buffer.dsv();
        m_command.command_list()->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

        // Record commands.
        const float clearColor[] = { 0.3f, 0.3f, 0.3f, 0.0f };
        m_command.command_list()->ClearRenderTargetView(rtv_handle, clearColor, 0, nullptr);
        m_command.command_list()->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

        m_command.command_list()->RSSetViewports(1, &surface.viewport());

        m_rain_drop.render(m_command.command_list());

        render();

        // Indicate that the back buffer will now be used to present.
        barriers::transition_resource(m_command.command_list(), surface.back_buffer(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    }

    void Core::deferred_release(IUnknown* resource)
    {
        const UINT frame_idx{ current_frame_index() };
        std::lock_guard lock{ deferred_releases_mutex };
        deferred_releases[frame_idx].push_back(resource);
        set_deferred_releases_flag();
    }

    void __declspec(noinline) Core::process_deferred_releases(UINT frame_index)
    {
        std::lock_guard lock{ deferred_releases_mutex };
        // NOTE: we clear this flag in the beginning. If we'd clear it at the end
        //       then it might overwrite some other thread that was trying to set it.
        //       It's fine if overwriting happens before processing the items.
        deferred_releases_flag[m_frame_index] = false;

        m_rtv_desc_heap.process_deferred_free(frame_index);
        m_dsv_desc_heap.process_deferred_free(frame_index);
        m_srv_desc_heap.process_deferred_free(frame_index);
        m_uav_desc_heap.process_deferred_free(frame_index);

        std::vector<IUnknown*>& resources{ deferred_releases[m_frame_index] };
        if (!resources.empty())
        {
            for (auto& resource : resources)
            {
                core::release(resource);
            }
            resources.clear();
        }
    }

    void remove_item(UINT& entity_id, UINT& item_id, UINT& model_id)
    {
        if (entity_id != Invalid_Index)
        {
            game_entity::remove(entity_id);
            entity_id = Invalid_Index;
        }

        if (item_id != Invalid_Index)
        {
            content::render_item::remove(item_id);
            item_id = Invalid_Index;
        }

        if (model_id != Invalid_Index)
        {
            content::destroy_resource(model_id, content::asset_type::mesh);
            model_id = Invalid_Index;
        }
    }

    void Core::OnDestroy()
    {
        remove_item(cube_entity_id, cube_item_id, cube_model_id);

        if (material_id != Invalid_Index)
        {
            content::destroy_resource(material_id, content::asset_type::material);
        }

        camera::remove(m_camera_id);

        m_rain_drop.shutdown();

        m_command.Release();

        // NOTE: we don't call process_deferred_releases at the end because
        //       some resources (such as swap chains) can't be released before
        //       their depending resources are released.
        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            process_deferred_releases(i);
        }

        shaders::shutdown();
        content::shutdown();
        upload::shutdown();
        graphic_pass::shutdown();

        //m_render_target.release();

        m_depth_buffer.release();
        surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
        surface.release();

        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            m_constant_buffers[i].release();
        }

        // NOTE: some modules free their descriptors when they shutdown.
        //       We process those by calling process_deferred_free once more.
        m_rtv_desc_heap.process_deferred_free(0);
        m_dsv_desc_heap.process_deferred_free(0);
        m_srv_desc_heap.process_deferred_free(0);
        m_uav_desc_heap.process_deferred_free(0);

        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            process_deferred_releases(i);
        }

        m_rtv_desc_heap.release();
        m_dsv_desc_heap.release();
        m_srv_desc_heap.release();
        m_uav_desc_heap.release();

        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            process_deferred_releases(i);
        }

        // Ensure that the GPU is no longer referencing resources that are about to be
        // cleaned up by the destructor.
        WaitForRenderContext();

        remove_surface();

        // Close handles to fence events and threads.
        CloseHandle(m_render_context_fence_event);
    }

    void Core::create_surface(HWND hwnd, UINT width, UINT height)
    {
        UINT id{ surface::surface_create(hwnd, width, height) };
        surface_ids.emplace_back(id);
        surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
        surface.create_swap_chain(m_factory.Get(), m_command.command_queue());
    }

    void Core::remove_surface()
    {
        for (auto& id : surface_ids)
        {
            surface::surface_remove(id);
        }
        surface_ids.clear();
    }

    void Core::WaitForRenderContext()
    {
        // Add a signal command to the queue.
        ThrowIfFailed(m_command.command_queue()->Signal(m_render_context_fence.Get(), m_render_context_fence_value));

        // Instruct the fence to set the event object when the signal command completes.
        ThrowIfFailed(m_render_context_fence->SetEventOnCompletion(m_render_context_fence_value, m_render_context_fence_event));

        ++m_render_context_fence_value;

        // Wait until the signal command has been processed.
        WaitForSingleObject(m_render_context_fence_event, INFINITE);
    }

    // Cycle through the frame resources. This method blocks execution if the 
    // next frame resource in the queue has not yet had its previous contents 
    // processed by the GPU.
    void Core::MoveToNextFrame()
    {
        surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
        m_command.EndFrame(surface);

        // Assign the current fence value to the current frame.
        m_frame_fence_values[m_frame_index] = m_render_context_fence_value;

        // Signal and increment the fence value.
        ThrowIfFailed(m_command.command_queue()->Signal(m_render_context_fence.Get(), m_render_context_fence_value));
        m_render_context_fence_value++;

        // Update the frame index.
        m_frame_index = surface.set_current_bb_index();

        // If the next frame is not ready to be rendered yet, wait until it is ready.
        if (m_render_context_fence->GetCompletedValue() < m_frame_fence_values[m_frame_index])
        {
            ThrowIfFailed(m_render_context_fence->SetEventOnCompletion(m_frame_fence_values[m_frame_index], m_render_context_fence_event));
            WaitForSingleObject(m_render_context_fence_event, INFINITE);
        }
    }
}