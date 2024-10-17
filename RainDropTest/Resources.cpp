#include "Main.h"
#include "Helpers.h"
#include "Resources.h"
#include "Math.h"
#include "Buffers.h"
#include "Core.h"

namespace resource {

    bool Descriptor_Heap::initialize(UINT capacity, bool is_shader_visible)
    {
        std::lock_guard lock{ m_mutex };

        assert(capacity && capacity <= D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2);
        assert(!(m_type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && capacity > D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE));
        assert(!((m_type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV || m_type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) && is_shader_visible));

        release();

        id3d12_device* const device{ core::device() };
        assert(device);

        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Flags = is_shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 0;
        desc.NumDescriptors = capacity;
        desc.Type = m_type;

        ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));

        // Fill in the free indicies
        m_free_handles = std::move(std::make_unique<UINT[]>(capacity));
        for (UINT i{ 0 }; i < capacity; ++i)
        {
            m_free_handles[i] = i;
        }

        m_capacity = capacity;
        m_size = 0;

        m_descriptor_size = device->GetDescriptorHandleIncrementSize(m_type);
        m_cpu_start = m_heap->GetCPUDescriptorHandleForHeapStart();
        m_gpu_start = is_shader_visible ? m_heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };

        return true;
    }

    void Descriptor_Heap::release()
    {
        assert(!m_size);

        //deferred_release(_heap.Get());
    }

    void Descriptor_Heap::process_deferred_free(UINT frame_idx)
    {
        std::lock_guard lock{ m_mutex };
        assert(frame_idx < Frame_Count);

        std::vector<UINT>& indices{ m_deferred_free_indicies[frame_idx] };
        if (!indices.empty())
        {
            for (auto index : indices)
            {
                --m_size;
                m_free_handles[m_size] = index;
            }
            indices.clear();
        }
    }

    Descriptor_Handle Descriptor_Heap::allocate()
    {
        std::lock_guard lock{ m_mutex };
        assert(m_heap);
        assert(m_size < m_capacity);

        const UINT index{ m_free_handles[m_size] };
        const UINT offset{ index * m_descriptor_size };
        ++m_size;

        Descriptor_Handle descriptor_handle{};
        descriptor_handle.cpu.ptr = m_cpu_start.ptr + offset;
        if (is_shader_visible())
        {
            descriptor_handle.gpu.ptr = m_gpu_start.ptr + offset;
        }
        descriptor_handle.index = index;

        return descriptor_handle;
    }

    void Descriptor_Heap::free_handle(Descriptor_Handle& handle)
    {
        if (!handle.is_valid()) return;
        std::lock_guard lock{ m_mutex };

        assert(m_heap && m_size);
        assert(handle.cpu.ptr >= m_cpu_start.ptr);
        assert((handle.cpu.ptr - m_cpu_start.ptr) % m_descriptor_size == 0);
        assert(handle.index < m_capacity);
        const UINT index = { (UINT)(handle.cpu.ptr - m_cpu_start.ptr) / m_descriptor_size };
        assert(handle.index == index);

        const UINT frame_idx{ core::current_frame_index() };
        m_deferred_free_indicies[frame_idx].push_back(index);
        core::set_deferred_releases_flag();
        handle = {};
    }
    // --- buffer ---------------------------------------------------------------

    Buffer::Buffer(UINT size, UINT alignment, D3D12_RESOURCE_FLAGS flag, bool cpu_accessible)
    {
        assert(!m_buffer && size && alignment);
        m_size = (UINT)math::align_size_up(size, alignment);
        if (cpu_accessible)
        {
            m_buffer = buffers::create_buffer_default_without_upload(size);
        }
        else
        {
            m_buffer = buffers::create_buffer_default_with_upload(nullptr, size);
        }
        m_gpu_address = m_buffer->GetGPUVirtualAddress();
        NAME_D3D12_OBJECT_INDEXED(m_buffer, m_size, L"Buffer - size");
    }

    void Buffer::release()
    {
        core::deferred_release(m_buffer);
        m_gpu_address = 0;
        m_size = 0;
    }

    // --- constant buffer ------------------------------------------------------

    constant_buffer::constant_buffer(UINT size, UINT alignment)
    {
        // d3d12 buffer
        assert(!m_buffer && size && alignment);
        m_size = (UINT)math::align_size_up(size, alignment);
        m_buffer = buffers::create_buffer_default_without_upload(size);

        m_gpu_address = m_buffer->GetGPUVirtualAddress();
        NAME_D3D12_OBJECT_INDEXED(m_buffer, m_size, L"Constant Buffer - size");

        D3D12_RANGE range{};
        ThrowIfFailed(m_buffer->Map(0, &range, (void**)(&m_cpu_address)));
        assert(m_cpu_address);
    }

    void constant_buffer::release()
    {
        core::deferred_release(m_buffer);
        m_gpu_address = 0;
        m_size = 0;

        m_cpu_address = nullptr;
        m_cpu_offset = 0;
    }

    UINT8* const constant_buffer::allocate(UINT size)
    {
        std::lock_guard lock{ m_mutex };

        const UINT aligned_size{ (UINT)buffers::align_size_for_constant_buffer(size) };
        assert(m_cpu_offset + aligned_size <= m_size);
        if (m_cpu_offset + aligned_size <= m_size)
        {
            UINT8* const address{ m_cpu_address + m_cpu_offset };
            m_cpu_offset += aligned_size;
            return address;
        }

        return nullptr;
    }

    // --- uav_buffer --------------------------------------------
    uav_buffer::uav_buffer(UINT size, UINT alignment, D3D12_RESOURCE_FLAGS flag)
        : m_buffer{ size, alignment, flag, false }
    {
        assert(size && alignment);
        NAME_D3D12_OBJECT_INDEXED(buffer(), size, L"UAV buffer - size");

        assert(flag & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        m_uav = core::uav_heap().allocate();
        m_uav_shader_visible = core::srv_heap().allocate();
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_R32_UINT;
        desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

        desc.Buffer.FirstElement = 0;                    //  UINT64 FirstElement;
        desc.Buffer.NumElements = size / sizeof(UINT);   //  UINT NumElements;
        desc.Buffer.StructureByteStride = 0;             //  UINT StructureByteStride;
        desc.Buffer.CounterOffsetInBytes = 0;            //  UINT64 CounterOffsetInBytes;
        desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;  //  D3D12_BUFFER_UAV_FLAGS Flags;

        core::device()->CreateUnorderedAccessView(buffer(), nullptr, &desc, m_uav.cpu);
        core::device()->CopyDescriptorsSimple(1, m_uav_shader_visible.cpu, m_uav.cpu, core::srv_heap().type());
    }

    void uav_buffer::release()
    {
        core::srv_heap().free_handle(m_uav_shader_visible);
        core::uav_heap().free_handle(m_uav);
        m_buffer.release();
    }

    void uav_buffer::clear_uav(id3d12_graphics_command_list* cmd_list, const UINT* const values) const
    {
        assert(buffer());
        assert(m_uav.is_valid() && m_uav_shader_visible.is_valid() && m_uav_shader_visible.is_shader_visible());
        cmd_list->ClearUnorderedAccessViewUint(m_uav_shader_visible.gpu, m_uav.cpu, buffer(), values, 0, nullptr);
    }

    void uav_buffer::clear_uav(id3d12_graphics_command_list* cmd_list, const float* const values) const
    {
        assert(buffer());
        assert(m_uav.is_valid() && m_uav_shader_visible.is_valid() && m_uav_shader_visible.is_shader_visible());
        cmd_list->ClearUnorderedAccessViewFloat(m_uav_shader_visible.gpu, m_uav.cpu, buffer(), values, 0, nullptr);
    }

    // --- texture -----------------------------------------------------
    d3d12_texture::d3d12_texture(d3d12_texture_init_info info)
    {
        auto* const device{ core::device() };
        assert(device);

        D3D12_CLEAR_VALUE* const clear_value
        {
            (info.desc &&
            (info.desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ||
             info.desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
            ? &info.clear_value : nullptr
        };

        if (info.resource)
        {
            m_resource = info.resource;
        }
        else
        {
            ThrowIfFailed(device->CreateCommittedResource(&d3dx::heap_properties.default_heap,
                D3D12_HEAP_FLAG_NONE, info.desc, info.initial_state,
                clear_value, IID_PPV_ARGS(&m_resource)));
        }
        assert(m_resource);

        m_srv = core::srv_heap().allocate();
        device->CreateShaderResourceView(m_resource, info.srv_desc, m_srv.cpu);
    }

    void d3d12_texture::release()
    {
        core::deferred_release(m_resource);
    }


    // --- render texture ----------------------------------------------
    Render_Target::Render_Target(D3D12_RESOURCE_DESC desc)
    {
        //heap 0
        //resource 0
        //srv_desc 0
        // desc fill in
        // allocation_info fill in
        // D3D12_RESOURCE_STATES  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        // D3D12_CLEAR_VALUE

        //D3D12_RESOURCE_DESC desc{};
        //desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        //desc.Alignment = 0; // NOTE: 0 is the same as 64KB (or 4MB for MSAA)       
        //desc.Width = size.x;
        //desc.Height = size.y;
        //desc.DepthOrArraySize = 1;
        //desc.MipLevels = 0; // make space for all mip levels     
        //desc.Format = main_buffer_format;
        //desc.SampleDesc = { 1, 0 };
        //desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        //desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        //d3d12_texture_init_info info{};
        //info.desc = &desc;
        //info.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        //info.clear_value.Format = desc.Format;
        //memcpy(&info.clear_value.Color, &clear_value[0], sizeof(clear_value));

        constexpr float default_clear_value[4]{ 0.5f, 0.5f, 0.5f, 1.f };


        // d3d12_texture
        id3d12_device* const device{ core::device() };
        assert(device);

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = desc.Format;
        memcpy(&clear_value.Color, &default_clear_value[0], sizeof(default_clear_value));

        ThrowIfFailed(device->CreateCommittedResource(&d3dx::heap_properties.default_heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_value, IID_PPV_ARGS(&m_resource)));
        assert(m_resource);

        m_srv = core::srv_heap().allocate();
        device->CreateShaderResourceView(m_resource, nullptr, m_srv.cpu);


        // render texture
        m_mip_count = m_resource->GetDesc().MipLevels;
        assert(m_mip_count && m_mip_count <= max_mips);

        Descriptor_Heap& rtv_heap{ core::rtv_heap() };
        D3D12_RENDER_TARGET_VIEW_DESC render_desc{};
        render_desc.Format = desc.Format;                          // DXGI_FORMAT Format;
        render_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // D3D12_RTV_DIMENSION ViewDimension;
        render_desc.Texture2D.MipSlice = 0;                        // D3D12_TEX2D_RTV Texture2D;

        for (UINT i{ 0 }; i < m_mip_count; ++i)
        {
            m_rtv[i] = rtv_heap.allocate();
            device->CreateRenderTargetView(m_resource, &render_desc, m_rtv[i].cpu);
            ++render_desc.Texture2D.MipSlice;
        }
    }

    void Render_Target::release()
    {
        for (UINT i{ 0 }; i < m_mip_count; ++i)
        {
            core::rtv_heap().free_handle(m_rtv[i]);
        }
        core::srv_heap().free_handle(m_srv);
        core::deferred_release(m_resource);

    }

    // --- depth buffer ----------------------------------------
    Depth_Buffer::Depth_Buffer(UINT width, UINT height)
    {
        auto* const device{ core::device() };
        assert(device);

        D3D12_RESOURCE_DESC buffer_desc = {};
        buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // D3D12_RESOURCE_DIMENSION Dimension;
        buffer_desc.Alignment = 0;                                   // UINT64 Alignment;
        buffer_desc.Width = width;                                   // UINT64 Width;
        buffer_desc.Height = height;                                 // UINT Height;
        buffer_desc.DepthOrArraySize = 1;                            // UINT16 DepthOrArraySize;
        buffer_desc.MipLevels = 1;                                   // UINT16 MipLevels;
        buffer_desc.Format = DXGI_FORMAT_R32_TYPELESS;                  // DXGI_FORMAT Format;
        buffer_desc.SampleDesc = { 1, 0 };                           // DXGI_SAMPLE_DESC SampleDesc;
        buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;           // D3D12_TEXTURE_LAYOUT Layout;
        buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // D3D12_RESOURCE_FLAGS Flags;


        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DXGI_FORMAT_D32_FLOAT;
        // depth test - clear_value.DepthStencil.Depth = 1.0f;
        clear_value.DepthStencil.Depth = 0.0f;
        clear_value.DepthStencil.Stencil = 0;
        
        // d3d12_texture
        //d3d12_texture_init_info info{};
        //m_texture = d3d12_texture(buffer_desc, clear_value, dsv_desc);
        ThrowIfFailed(device->CreateCommittedResource(&d3dx::heap_properties.default_heap,
            D3D12_HEAP_FLAG_NONE, &buffer_desc, D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            &clear_value, IID_PPV_ARGS(&m_resource)));
        assert(m_resource);

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.PlaneSlice = 0;
        srv_desc.Texture2D.ResourceMinLODClamp = 0.f;
        m_srv = core::srv_heap().allocate();
        device->CreateShaderResourceView(m_resource, &srv_desc, m_srv.cpu);

        // d3d12_depth_buffer
        m_dsv = core::dsv_heap().allocate();

        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
        dsv_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        dsv_desc.Texture2D.MipSlice = 0;

        D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
        depth_stencil_desc.Format = DXGI_FORMAT_D32_FLOAT;                //   DXGI_FORMAT Format;
        depth_stencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; //   D3D12_DSV_DIMENSION ViewDimension;
        depth_stencil_desc.Flags = D3D12_DSV_FLAG_NONE;                   //   D3D12_DSV_FLAGS Flags;

        device->CreateDepthStencilView(resource(), &depth_stencil_desc, m_dsv.cpu);
    }

    void Depth_Buffer::release()
    {
        core::srv_heap().free_handle(m_srv);
        core::dsv_heap().free_handle(m_dsv);
        core::deferred_release(m_resource);
    }
}