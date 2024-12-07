#include "Core.h"
#include <functional>
#include <string>
#include <filesystem>
#include "stdafx.h"
#include "Command.h"
#include "Barriers.h"
#include "Upload.h"
#include "Resources.h"
#include "GraphicPass.h"
#include "SharedTypes.h"
#include "FreeList.h"
#include "RainDrop.h"
#include "Lights.h"
#include "PostProcess.h"

// InterlockedCompareExchange returns the object's value if the 
// comparison fails.  If it is already 0, then its value won't 
// change and 0 will be returned.
#define InterlockedGetValue(object) InterlockedCompareExchange(object, 0, 0)

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 614; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace core {

    namespace {

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

        // rain_drop::RainDrop m_rain_drop;

        ComPtr<ID3D12Fence> m_render_context_fence;

        UINT64 m_render_context_fence_value{ 0 };
        HANDLE m_render_context_fence_event{ nullptr };

        d3d12_frame_info get_d3d12_frame_info(const frame_info& info, resource::constant_buffer& cbuffer, const surface::Surface& surface, UINT frame_index, float delta_time)
        {
            camera::Camera& camera{ camera::get(info.camera_id) };
            camera.update();
            hlsl::GlobalShaderData global_shader_data{};

            XMStoreFloat4x4A(&global_shader_data.View, camera.view());
            //XMStoreFloat4x4A(&global_shader_data.InverseView, camera.inverse_view());
            XMStoreFloat4x4A(&global_shader_data.Projection, camera.projection());
            XMStoreFloat4x4A(&global_shader_data.InvProjection, camera.inverse_projection());
            XMStoreFloat4x4A(&global_shader_data.ViewProjection, camera.view_projection());
            XMStoreFloat4x4A(&global_shader_data.InvViewProjection, camera.inverse_view_projection());
            XMStoreFloat3(&global_shader_data.CameraPosition, camera.position());
            XMStoreFloat3(&global_shader_data.CameraDirection, camera.direction());
            global_shader_data.ViewWidth = surface.viewport().Width;
            global_shader_data.ViewHeight = surface.viewport().Height;
            global_shader_data.NumDirectionalLights = lights::non_cullable_light_count(info.light_set_key);
            global_shader_data.DeltaTime = delta_time;

            hlsl::GlobalShaderData* const shader_data{ cbuffer.allocate<hlsl::GlobalShaderData>() };
            // TODO: handle the case when cbuffer is full.
            memcpy(shader_data, &global_shader_data, sizeof(hlsl::GlobalShaderData));

            d3d12_frame_info d3d12_info
            {
                &info,
                &camera,
                cbuffer.gpu_address(shader_data),
                surface.width(),
                surface.height(),
                surface.light_id(),
                frame_index,
                delta_time
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
            if (!resources.empty())
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

            // m_rain_drop.initialize();

            ////// Close the command list and execute it to begin the initial GPU setup and uploads.
            ////ThrowIfFailed(m_command.command_list()->Close());
            ////ID3D12CommandList* pp_command_lists[] = { m_command.command_list() };
            ////m_command.command_queue()->ExecuteCommandLists(_countof(pp_command_lists), pp_command_lists);

            // Create synchronization objects and wait until assets have been uploaded to the GPU.
            //{
            //    ThrowIfFailed(core::device()->CreateFence(m_render_context_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_render_context_fence)));
            //    ++m_render_context_fence_value;

            //    m_render_context_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            //    if (m_render_context_fence_event == nullptr)
            //    {
            //        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
            //    }
            //}
            //WaitForRenderContext();
        }


    } // anonymous namespace

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
        NAME_D3D12_OBJECT(m_device, L"Main D3D12 Device");

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

        resource::buffer_init_info info{};
        info.size = 1024 * 1024;
        info.alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        info.cpu_accessible = true;
        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            new (&m_constant_buffers[i]) resource::constant_buffer{ info };
            NAME_D3D12_OBJECT_INDEXED(m_constant_buffers[i].buffer(), i, L"Global Constant Buffer");
        }

        // m_rain_drop.create_descriptor_heap();

        if (!(shaders::initialize() &&
            graphic_pass::initialize() &&
            post_process::initialize() &&
            upload::initialize() &&
            content::initialize() &&
            lights::initialize()))
        {
            return failed_init();
        }

        //LoadAssets();

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

        //m_rain_drop.shutdown();

        m_command.release();

        // NOTE: we don't call process_deferred_releases at the end because
        //       some resources (such as swap chains) can't be released before
        //       their depending resources are released.
        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            process_deferred_releases(i);
        }

        lights::shutdown();
        content::shutdown();
        upload::shutdown();
        post_process::shutdown();
        graphic_pass::shutdown();
        shaders::shutdown();

        release(m_dxgi_factory);
        //m_render_target.release();

        //m_depth_buffer.release();
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

        //for (UINT i{ 0 }; i < Frame_Count; ++i)
        //{
        //    process_deferred_releases(i);
        //}

        m_rtv_desc_heap.release();
        m_dsv_desc_heap.release();
        m_srv_desc_heap.release();
        m_uav_desc_heap.release();

        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            process_deferred_releases(i);
        }

        if (m_device)
        {
            {
                ComPtr<ID3D12InfoQueue> info_queue;
                ThrowIfFailed(m_device->QueryInterface(IID_PPV_ARGS(&info_queue)));
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, false);
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
            }

            ComPtr<ID3D12DebugDevice2> debug_device;
            ThrowIfFailed(m_device->QueryInterface(IID_PPV_ARGS(&debug_device)));
            release(m_device);
            ThrowIfFailed(debug_device->ReportLiveDeviceObjects(
                D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL));
        }

        release(m_device);
    }

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

    resource::constant_buffer& cbuffer()
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

    void render(UINT id, frame_info info)
    {
        // Wait for GPU
        m_command.begin_frame();
        id3d12_graphics_command_list* const cmd_list{ m_command.command_list() };

        const UINT frame_index{ current_frame_index() };

        // Clear constant buffers
        resource::constant_buffer& cbuffer{ m_constant_buffers[frame_index] };
        cbuffer.clear();

        if (m_deferred_releasees_flags[frame_index])
        {
            process_deferred_releases(frame_index);
        }

        const surface::Surface& surface{ surface::get_surface(id) };
        ID3D12Resource* const current_back_buffer{ surface.back_buffer() };
        const d3d12_frame_info d3d12_info{ get_d3d12_frame_info(info, cbuffer, surface, frame_index, 16.7f) };

        graphic_pass::set_size({ d3d12_info.surface_width, d3d12_info.surface_height });
        barriers::resource_barrier& barriers{ resource_barriers };

        ID3D12DescriptorHeap* const heaps[]{ m_srv_desc_heap.heap() };
        cmd_list->SetDescriptorHeaps(1, &heaps[0]);

        cmd_list->RSSetViewports(1, &surface.viewport());
        cmd_list->RSSetScissorRects(1, &surface.scissor_rect());

        barriers.add(current_back_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY);

        graphic_pass::depth_process(cmd_list, d3d12_info, barriers);

        lights::update_light_buffers(d3d12_info);
        lights::cull_lights(cmd_list, d3d12_info, barriers);

        graphic_pass::render_targets(cmd_list, d3d12_info, barriers);

        // Post-process
        barriers.add(current_back_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY);
        barriers.apply(cmd_list);

        // Post process
        post_process::post_process(cmd_list, d3d12_info, surface.rtv());
        // after post process
        barriers::transition_resource(cmd_list, current_back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        m_command.end_frame(surface);
    }

    void flush()
    {
        m_command.flush();
    }
}