#pragma once
#include "stdafx.h"
#include "DXSample.h"
#include "Main.h"

struct Descriptor_Handle {
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
    UINT index{ Invalid_Index };

    [[nodiscard]] constexpr bool is_valid() const { return cpu.ptr != 0; }
};

class D3D12DescriptorHeap
{
public:
    explicit D3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) : m_type{ type } {}

    explicit D3D12DescriptorHeap(const D3D12DescriptorHeap&) = delete;
    D3D12DescriptorHeap& operator=(const D3D12DescriptorHeap&) = delete;
    explicit D3D12DescriptorHeap(D3D12DescriptorHeap&&) = delete;
    D3D12DescriptorHeap& operator=(D3D12DescriptorHeap&&) = delete;

    ~D3D12DescriptorHeap() {}

    bool initialize(UINT capacity, bool is_shader_visible);
    void release();
    void process_deferred_free(UINT frame_idx);
    [[nodiscard]] Descriptor_Handle allocator();
    void free(Descriptor_Handle& handle);

    [[nodiscard]] constexpr bool is_shader_visible() const { return m_gpu_start.ptr != 0; }
    [[nodiscard]] ID3D12DescriptorHeap* const heap() const { return m_heap.Get(); }

private:
    ComPtr<ID3D12DescriptorHeap> m_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpu_start{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpu_start{};
    std::unique_ptr<UINT[]> m_free_handles{};
    std::vector<UINT> m_deferred_free_indicies[Frame_Count]{};
    std::mutex m_mutex{};
    UINT m_capacity{ 0 };
    UINT m_size{ 0 };
    UINT m_descriptor_size{};

    const D3D12_DESCRIPTOR_HEAP_TYPE m_type{};
};

