#include "D3D12Resources.h"

bool D3D12DescriptorHeap::initialize(UINT capacity, bool is_shader_visible)
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

void D3D12DescriptorHeap::release()
{
    assert(!m_size);

    //deferred_release(_heap.Get());
}

void D3D12DescriptorHeap::process_deferred_free(UINT frame_idx)
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

Descriptor_Handle D3D12DescriptorHeap::allocator()
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

void D3D12DescriptorHeap::free(Descriptor_Handle& handle)
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
    core::set_deferred_release_flag();
    handle = {};
}

