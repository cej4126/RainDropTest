#include "Core.h"
#include <functional>
#include <string>
#include <filesystem>
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
#include "TimeProcess.h"
//#include "Surface.h"

// InterlockedCompareExchange returns the object's value if the 
// comparison fails.  If it is already 0, then its value won't 
// change and 0 will be returned.
#define InterlockedGetValue(object) InterlockedCompareExchange(object, 0, 0)

//extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 614; }
//extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace core {

    namespace {
        time_process timer{};

        resource::Depth_Buffer m_depth_buffer{};

        UINT cube_model_id{ Invalid_Index };
        UINT cube_entity_id{ Invalid_Index };
        UINT cube_item_id{ Invalid_Index };
        UINT material_id{ Invalid_Index };

        game_entity::entity camera_entity{};

        barriers::resource_barrier resource_barriers{};

        utl::vector<UINT> surface_ids;

        constexpr D3D_FEATURE_LEVEL minimum_feature_level{ D3D_FEATURE_LEVEL_12_0 };

        IDXGIFactory7* m_dxgi_factory{ nullptr };
        id3d12_device* m_device{ nullptr };
        command::Command m_command;
        resource::Descriptor_Heap m_rtv_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
        resource::Descriptor_Heap m_dsv_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
        resource::Descriptor_Heap m_srv_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
        resource::Descriptor_Heap m_uav_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

        resource::constant_buffer m_constant_buffers[Frame_Count];

        std::mutex m_deferred_releases_mutux{};
        UINT m_deferred_releasees_flags[Frame_Count]{};
        utl::vector<IUnknown*> m_deferred_releases[Frame_Count]{};

        rain_drop::RainDrop m_rain_drop;

        ComPtr<ID3D12Fence> m_render_context_fence;

        UINT64 m_render_context_fence_value{ 0 };
        HANDLE m_render_context_fence_event{ nullptr };

        d3d12_frame_info get_d3d12_frame_info(const frame_info& info, resource::constant_buffer& cbuffer, const surface::Surface& surface)
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

        IDXGIAdapter4* get_main_adapter()
        {
            IDXGIAdapter4* adapter{ nullptr };

            for (UINT i{ 0 }; m_dxgi_factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i)
            {
                if (SUCCEEDED(D3D12CreateDevice(adapter, minimum_feature_level, __uuidof(ID3D12Device), nullptr)))
                {
                    return adapter;
                }
                release(adapter);
            }

            return nullptr;
        }

        D3D_FEATURE_LEVEL get_max_feature_level(IDXGIAdapter4* adapter)
        {
            constexpr D3D_FEATURE_LEVEL feature_levels[]
            {
                D3D_FEATURE_LEVEL_12_0,
                D3D_FEATURE_LEVEL_12_1,
//                D3D_FEATURE_LEVEL_12_2,
            };

            D3D12_FEATURE_DATA_FEATURE_LEVELS feature_level_info{};
            feature_level_info.NumFeatureLevels = _countof(feature_levels);
            feature_level_info.pFeatureLevelsRequested = feature_levels;

            ComPtr<ID3D12Device> device;
            ThrowIfFailed(D3D12CreateDevice(adapter, minimum_feature_level, IID_PPV_ARGS(&device)));
            ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &feature_level_info, sizeof(feature_level_info)));
            return feature_level_info.MaxSupportedFeatureLevel;
        }


        bool failed_init()
        {
            shutdown();
            return false;
        }

        void __declspec(noinline) process_deferred_releases(UINT frame_index)
        {
            std::lock_guard lock{ m_deferred_releases_mutux };

            // NOTE: we clear this flag in the beginning. If we'd clear it at the end
            //       then it might overwrite some other thread that was trying to set it.
            //       It's fine if overwriting happens before processing the items.
            m_deferred_releasees_flags[frame_index] = 0;

            m_rtv_desc_heap.process_deferred_free(frame_index);
            m_dsv_desc_heap.process_deferred_free(frame_index);
            m_srv_desc_heap.process_deferred_free(frame_index);
            m_uav_desc_heap.process_deferred_free(frame_index);

            utl::vector<IUnknown*>& resources{ m_deferred_releases[frame_index] };
            if (resources.empty())
            {
                for (auto& resource : resources)
                {
                    release(resource);
                }
                resources.clear();
            }
        }

        void WaitForRenderContext()
        {
            // Add a signal command to the queue.
            ThrowIfFailed(m_command.command_queue()->Signal(m_render_context_fence.Get(), m_render_context_fence_value));

            // Instruct the fence to set the event object when the signal command completes.
            ThrowIfFailed(m_render_context_fence->SetEventOnCompletion(m_render_context_fence_value, m_render_context_fence_event));

            ++m_render_context_fence_value;

            // Wait until the signal command has been processed.
            WaitForSingleObject(m_render_context_fence_event, INFINITE);
        }

        void LoadAssets()
        {

            m_rain_drop.initialize();

            //// Close the command list and execute it to begin the initial GPU setup and uploads.
            //ThrowIfFailed(m_command.command_list()->Close());
            //ID3D12CommandList* pp_command_lists[] = { m_command.command_list() };
            //m_command.command_queue()->ExecuteCommandLists(_countof(pp_command_lists), pp_command_lists);

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


    } // anonymous namespace

    //Core::Core(UINT width, UINT height, std::wstring name) :
    //    dx_app(width, height, name),
    //    m_frame_index(0),
    //    m_render_context_fence_value(0)
    //{
    //    srand(0);

    //    m_srv_index = 0;
    //    ZeroMemory(m_frame_fence_values, sizeof(m_frame_fence_values));

    //    m_rain_drop.set_rain_state(true);

    //    struct
    //    {
    //        input::input_code::code code;
    //        float multipler;
    //        input::axis::type axis;
    //    } input_data[] =
    //    {
    //        { input::input_code::key_a,  1.f, input::axis::x },
    //        { input::input_code::key_d, -1.f, input::axis::x },
    //        { input::input_code::key_w,  1.f, input::axis::z },
    //        { input::input_code::key_s, -1.f, input::axis::z },
    //        { input::input_code::key_q, -1.f, input::axis::y },
    //        { input::input_code::key_e,  1.f, input::axis::y }
    //    };
    //    input::input_source source{};
    //    source.binding = std::hash<std::string>()("move");
    //    source.source_type = input::input_source::keyboard;

    //    for (int i{ 0 }; i < _countof(input_data); ++i)
    //    {
    //        source.code = input_data[i].code;
    //        source.multiplier = input_data[i].multipler;
    //        source.axis = input_data[i].axis;
    //        input::bind(source);
    //    }
    //}

    //void Core::OnInit(UINT width, UINT height)
    //{

    //    LoadPipeline();
    //    LoadAssets(width, height);
    //    m_rain_drop.CreateAsyncContexts();

    //    create_render_items();

    //    // x in(-) / out(+), y up(+) / down(-), z left(-) / right(+)
    //    camera_entity = create_entity_item({ 10.f, 0.f, 0.f }, { math::dtor(0.f), math::dtor(-90.f), math::dtor(0.f) }, "camera_script");
    //    m_camera_id = camera::create(camera::perspective_camera_init_info(camera_entity.get_id()));
    //    camera::aspect_ratio(m_camera_id, (float)width / height);
    //}

    //// Load the rendering pipeline dependencies.
    //void Core::LoadPipeline()
    //{
    //    //#if defined(_DEBUG)
    //    //        // Enable the D3D12 debug layer.
    //    //        {
    //    //            ComPtr<ID3D12Debug> debug_controller;
    //    //            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
    //    //            {
    //    //                debug_controller->EnableDebugLayer();
    //    //            }
    //    //        }
    //    //#endif

    //    //ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)));
    //    //
    //    //if (m_use_warp_device)
    //    //{
    //    //    // Wrap device
    //    //    ComPtr<IDXGIAdapter> warp_adapter;
    //    //    ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));
    //    //    ThrowIfFailed(D3D12CreateDevice(warp_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
    //    //}
    //    //else
    //    //{
    //    //    ComPtr<IDXGIAdapter1> hardware_adapter;
    //    //    GetHardwareAdapter(m_factory.Get(), &hardware_adapter);
    //    //
    //    //    ThrowIfFailed(D3D12CreateDevice(hardware_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
    //    //}
    //}

    //void Core::LoadAssets(UINT width, UINT height)
    //{

    //    m_rain_drop.initialize(width, height);

    //    assert(width && height);
    //    m_depth_buffer.release();
    //    m_depth_buffer = Depth_Buffer{ width, height };

    //    // Close the command list and execute it to begin the initial GPU setup and uploads.
    //    ThrowIfFailed(m_command.command_list()->Close());
    //    ID3D12CommandList* pp_command_lists[] = { m_command.command_list() };
    //    m_command.command_queue()->ExecuteCommandLists(_countof(pp_command_lists), pp_command_lists);

    //    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    //    {
    //        ThrowIfFailed(core::device()->CreateFence(m_render_context_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_render_context_fence)));
    //        ++m_render_context_fence_value;

    //        m_render_context_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    //        if (m_render_context_fence_event == nullptr)
    //        {
    //            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    //        }
    //    }
    //    WaitForRenderContext();
    //}

    //void Core::run()
    //{
    //    timer.begin();
    //    //std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //    const float dt{ timer.dt_avg() };

    //    OnUpdate(dt);
    //    timer.end();
    //}

    //// Update frame-based values.
    //void Core::OnUpdate(float dt)
    //{
    //    script::update(dt);

    //    surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
    //    // Wait for the previous Present to complete.
    //    WaitForSingleObjectEx(surface.swap_chain_event(), 100, FALSE);

    //    camera::Camera& camera{ camera::get(m_camera_id) };
    //    camera.update();

    //    m_rain_drop.update(m_camera_id, m_frame_index);

    //    OnRender();
    //}

    //void Core::render()
    //{

    //    UINT render_items[1]{};
    //    render_items[0] = cube_item_id;

    //    frame_info info{};
    //    info.render_item_ids = &render_items[0];
    //    info.render_item_count = 1;
    //    info.camera_id = m_camera_id;

    //    surface::Surface& surface{ surface::get_surface(surface_ids[0]) };

    //    const d3d12_frame_info d3d12_info{ get_d3d12_frame_info(info, m_constant_buffers[m_frame_index], surface) };

    //    graphic_pass::render(m_command.command_list(), d3d12_info);
    //}

    //// Render the scene.
    //void Core::OnRender()
    //{
    //    m_rain_drop.sync_compute_tread(m_command.command_queue());

    //    m_command.BeginFrame();

    //    if (deferred_releases_flag[m_frame_index])
    //    {
    //        process_deferred_releases(m_frame_index);
    //    }

    //    // Record all the commands we need to render the scene into the command list.
    //    PopulateCommandList();

    //    MoveToNextFrame();
    //}

    //// Fill the command list with all the render commands and dependent state.
    //void Core::PopulateCommandList()
    //{
    //    m_rain_drop.populate_command_list(m_command.command_list(), m_frame_index);

    //    surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
    //    m_command.command_list()->RSSetScissorRects(1, &surface.scissor_rect());

    //    barriers::transition_resource(m_command.command_list(), surface.back_buffer(),
    //        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    //    // Indicate that the back buffer will be used as a render target.
    //    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{ surface.rtv().ptr };
    //    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = m_depth_buffer.dsv();
    //    m_command.command_list()->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

    //    // Record commands.
    //    const float clearColor[] = { 0.3f, 0.3f, 0.3f, 0.0f };
    //    m_command.command_list()->ClearRenderTargetView(rtv_handle, clearColor, 0, nullptr);
    //    m_command.command_list()->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

    //    m_command.command_list()->RSSetViewports(1, &surface.viewport());

    //    m_rain_drop.render(m_command.command_list());

    //    render();

    //    // Indicate that the back buffer will now be used to present.
    //    barriers::transition_resource(m_command.command_list(), surface.back_buffer(),
    //        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    //}

    //void Core::deferred_release(IUnknown* resource)
    //{
    //    const UINT frame_idx{ current_frame_index() };
    //    std::lock_guard lock{ deferred_releases_mutex };
    //    deferred_releases[frame_idx].push_back(resource);
    //    set_deferred_releases_flag();
    //}

    //void __declspec(noinline) Core::process_deferred_releases(UINT frame_index)
    //{
    //    std::lock_guard lock{ deferred_releases_mutex };
    //    // NOTE: we clear this flag in the beginning. If we'd clear it at the end
    //    //       then it might overwrite some other thread that was trying to set it.
    //    //       It's fine if overwriting happens before processing the items.
    //    deferred_releases_flag[m_frame_index] = false;

    //    m_rtv_desc_heap.process_deferred_free(frame_index);
    //    m_dsv_desc_heap.process_deferred_free(frame_index);
    //    m_srv_desc_heap.process_deferred_free(frame_index);
    //    m_uav_desc_heap.process_deferred_free(frame_index);

    //    std::vector<IUnknown*>& resources{ deferred_releases[m_frame_index] };
    //    if (!resources.empty())
    //    {
    //        for (auto& resource : resources)
    //        {
    //            core::release(resource);
    //        }
    //        resources.clear();
    //    }
    //}

    namespace detail {
        void deferred_release(IUnknown* resource)
        {
            const UINT frame_index{ current_frame_index() };
            std::lock_guard lock{ m_deferred_releases_mutux };
            m_deferred_releases[frame_index].push_back(resource);
            set_deferred_releases_flag();
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

    //void Core::OnDestroy()
    //{
    //    remove_item(cube_entity_id, cube_item_id, cube_model_id);

    //    if (material_id != Invalid_Index)
    //    {
    //        content::destroy_resource(material_id, content::asset_type::material);
    //    }

    //    camera::remove(m_camera_id);

    //    m_rain_drop.shutdown();

    //    m_command.Release();

    //    // NOTE: we don't call process_deferred_releases at the end because
    //    //       some resources (such as swap chains) can't be released before
    //    //       their depending resources are released.
    //    for (UINT i{ 0 }; i < Frame_Count; ++i)
    //    {
    //        process_deferred_releases(i);
    //    }

    //    shaders::shutdown();
    //    content::shutdown();
    //    upload::shutdown();
    //    graphic_pass::shutdown();

    //    //m_render_target.release();

    //    m_depth_buffer.release();
    //    surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
    //    surface.release();

    //    for (UINT i{ 0 }; i < Frame_Count; ++i)
    //    {
    //        m_constant_buffers[i].release();
    //    }

    //    // NOTE: some modules free their descriptors when they shutdown.
    //    //       We process those by calling process_deferred_free once more.
    //    m_rtv_desc_heap.process_deferred_free(0);
    //    m_dsv_desc_heap.process_deferred_free(0);
    //    m_srv_desc_heap.process_deferred_free(0);
    //    m_uav_desc_heap.process_deferred_free(0);

    //    for (UINT i{ 0 }; i < Frame_Count; ++i)
    //    {
    //        process_deferred_releases(i);
    //    }

    //    m_rtv_desc_heap.release();
    //    m_dsv_desc_heap.release();
    //    m_srv_desc_heap.release();
    //    m_uav_desc_heap.release();

    //    for (UINT i{ 0 }; i < Frame_Count; ++i)
    //    {
    //        process_deferred_releases(i);
    //    }

    //    // Ensure that the GPU is no longer referencing resources that are about to be
    //    // cleaned up by the destructor.
    //    WaitForRenderContext();

    //    remove_surface();

    //    // Close handles to fence events and threads.
    //    CloseHandle(m_render_context_fence_event);
    //}

    //void Core::create_surface(HWND hwnd, UINT width, UINT height)
    //{
    //    UINT id{ surface::surface_create(hwnd, width, height) };
    //    surface_ids.emplace_back(id);
    //    surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
    //    surface.create_swap_chain(m_factory.Get(), m_command.command_queue());
    //}

    //void Core::remove_surface()
    //{
    //    for (auto& id : surface_ids)
    //    {
    //        surface::surface_remove(id);
    //    }
    //    surface_ids.clear();
    //}

    //void Core::WaitForRenderContext()
    //{
    //    // Add a signal command to the queue.
    //    ThrowIfFailed(m_command.command_queue()->Signal(m_render_context_fence.Get(), m_render_context_fence_value));

    //    // Instruct the fence to set the event object when the signal command completes.
    //    ThrowIfFailed(m_render_context_fence->SetEventOnCompletion(m_render_context_fence_value, m_render_context_fence_event));

    //    ++m_render_context_fence_value;

    //    // Wait until the signal command has been processed.
    //    WaitForSingleObject(m_render_context_fence_event, INFINITE);
    //}

    //// Cycle through the frame resources. This method blocks execution if the 
    //// next frame resource in the queue has not yet had its previous contents 
    //// processed by the GPU.
    //void Core::MoveToNextFrame()
    //{
    //    surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
    //    m_command.EndFrame(surface);

    //    // Assign the current fence value to the current frame.
    //    m_frame_fence_values[m_frame_index] = m_render_context_fence_value;

    //    // Signal and increment the fence value.
    //    ThrowIfFailed(m_command.command_queue()->Signal(m_render_context_fence.Get(), m_render_context_fence_value));
    //    m_render_context_fence_value++;

    //    // Update the frame index.
    //    m_frame_index = surface.set_current_bb_index();

    //    // If the next frame is not ready to be rendered yet, wait until it is ready.
    //    if (m_render_context_fence->GetCompletedValue() < m_frame_fence_values[m_frame_index])
    //    {
    //        ThrowIfFailed(m_render_context_fence->SetEventOnCompletion(m_frame_fence_values[m_frame_index], m_render_context_fence_event));
    //        WaitForSingleObject(m_render_context_fence_event, INFINITE);
    //    }
    //}

    bool initialize()
    {
        if (m_device)
        {
            shutdown();
        }

        UINT dxgi_factory_flags{ 0 };
#ifdef _DEBUG
        // Enable debugging layer. Requires "Graphics Tools" optional feature
        {
            ComPtr<ID3D12Debug3> debug_interface;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
            {
                debug_interface->EnableDebugLayer();
#if 0
#pragma message("WARNING: GPU-based validation is enabled. This will considerably slow down the renderer!")
                debug_interface->SetEnableGPUBasedValidation(1);
#endif
        }
            else
            {
                OutputDebugStringA("Warning: D3D12 Debug interface is not available. Verify that Graphics Tools optional feature is installed on this system.\n");
            }
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif // _DEBUG

        ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_dxgi_factory)));

        ComPtr<IDXGIAdapter4> main_adapter;
        main_adapter.Attach(get_main_adapter());
        if (!main_adapter)
        {
            return failed_init();
        }

        D3D_FEATURE_LEVEL max_feature_level{ get_max_feature_level(main_adapter.Get()) };
        assert(max_feature_level >= minimum_feature_level);
        if (max_feature_level < minimum_feature_level)
        {
            return failed_init();
        }

        ThrowIfFailed(D3D12CreateDevice(main_adapter.Get(), max_feature_level, IID_PPV_ARGS(&m_device)));

#ifdef _DEBUG
        {
            ComPtr<ID3D12InfoQueue> info_queue;
            ThrowIfFailed(m_device->QueryInterface(IID_PPV_ARGS(&info_queue)));

            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        }
#endif // _DEBUG

        new (&m_command) command::Command(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        if (!m_command.command_queue())
        {
            return failed_init();
        }

        bool result{ true };
        result &= m_rtv_desc_heap.initialize(512, false);
        result &= m_dsv_desc_heap.initialize(512, false);
        result &= m_srv_desc_heap.initialize(4096, true);
        result &= m_uav_desc_heap.initialize(512, false);
        if (!result)
        {
            return failed_init();
        }

        NAME_D3D12_OBJECT(m_rtv_desc_heap.heap(), L"RTV Descriptor Heap");
        NAME_D3D12_OBJECT(m_dsv_desc_heap.heap(), L"DSV Descriptor Heap");
        NAME_D3D12_OBJECT(m_srv_desc_heap.heap(), L"SRV Descriptor Heap");
        NAME_D3D12_OBJECT(m_uav_desc_heap.heap(), L"UAV Descriptor Heap");

        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            new (&m_constant_buffers[i]) resource::constant_buffer{ 1024 * 1024, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT };
            NAME_D3D12_OBJECT_INDEXED(m_constant_buffers[i].buffer(), i, L"Global Constant Buffer");
        }

        m_rain_drop.create_descriptor_heap();

        if (!(graphic_pass::initialize() &&
            upload::initialize() &&
            content::initialize() &&
            shaders::initialize()))
        {
            return failed_init();
        }

        LoadAssets();

        return true;
}

    void shutdown()
    {
        //remove_item(cube_entity_id, cube_item_id, cube_model_id);

        //if (material_id != Invalid_Index)
        //{
        //    content::destroy_resource(material_id, content::asset_type::material);
        //}

        //camera::remove(m_camera_id);

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
        //surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
        //surface.release();

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

        //remove_surface();

        // Close handles to fence events and threads.
        CloseHandle(m_render_context_fence_event);
    }

    //void Core::create_surface(HWND hwnd, UINT width, UINT height)
    //{
    //    UINT id{ surface::surface_create(hwnd, width, height) };
    //    surface_ids.emplace_back(id);
    //    surface::Surface& surface{ surface::get_surface(surface_ids[0]) };
    //    surface.create_swap_chain(m_factory.Get(), m_command.command_queue());
    //}

    //void Core::remove_surface()
    //{
    //    for (auto& id : surface_ids)
    //    {
    //        surface::surface_remove(id);
    //    }
    //    surface_ids.clear();

    //}

    id3d12_device* const device()
    {
        return m_device;
    }

    IDXGIFactory7* const factory()
    {
        return m_dxgi_factory;
    }

    ID3D12CommandQueue* const command_queue()
    {
        return m_command.command_queue();
    }

    resource::Descriptor_Heap& rtv_heap()
    {
        return m_rtv_desc_heap;
    }

    resource::Descriptor_Heap& dsv_heap()
    {
        return m_dsv_desc_heap;
    }

    resource::Descriptor_Heap& srv_heap()
    {
        return m_srv_desc_heap;
    }

    resource::Descriptor_Heap& uav_heap()
    {
        return m_uav_desc_heap;
    }

    resource::constant_buffer& cbuffers()
    {
        return m_constant_buffers[current_frame_index()];
    }

    UINT current_frame_index()
    {
        return m_command.frame_index();
    }

    void set_deferred_releases_flag()
    {
        m_deferred_releasees_flags[current_frame_index()] = 1;
    }

    ID3D12Fence* render_context_fence()
    {
        return m_render_context_fence.Get();
    }

}