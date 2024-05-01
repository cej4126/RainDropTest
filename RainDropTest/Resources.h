#pragma once
#include "stdafx.h"
#include "DXSample.h"

namespace d3d12 {

    struct Descriptor_Handle {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        UINT index{ Invalid_Index };

        [[nodiscard]] constexpr bool is_valid() const { return cpu.ptr != 0; }
    };

    class Descriptor_Heap
    {
    public:
        explicit Descriptor_Heap(D3D12_DESCRIPTOR_HEAP_TYPE type) : m_type{ type } {}

        explicit Descriptor_Heap(const Descriptor_Heap&) = delete;
        Descriptor_Heap& operator=(const Descriptor_Heap&) = delete;
        explicit Descriptor_Heap(Descriptor_Heap&&) = delete;
        Descriptor_Heap& operator=(Descriptor_Heap&&) = delete;

        ~Descriptor_Heap() {}

        bool initialize(UINT capacity, bool is_shader_visible);
        void release();
        void process_deferred_free(UINT frame_idx);
        [[nodiscard]] Descriptor_Handle allocate();
        void free(Descriptor_Handle& handle);

        [[nodiscard]] constexpr bool is_shader_visible() const { return m_gpu_start.ptr != 0; }
        [[nodiscard]] ID3D12DescriptorHeap* const heap() const { return m_heap.Get(); }
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE cpu_start() const { return m_cpu_start; }
        [[nodiscard]] constexpr UINT descriptor_size() const { return m_descriptor_size; }

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


    // --- texture -----------------------------------------------------
    class d3d12_texture
    {
    public:
        d3d12_texture() = default;
        explicit d3d12_texture(D3D12_RESOURCE_DESC desc, D3D12_CLEAR_VALUE clear_value);
        DISABLE_COPY(d3d12_texture);
        constexpr d3d12_texture(d3d12_texture&& o) noexcept
            : m_resource{ o.m_resource }
        {
            o.reset();
        }

        constexpr d3d12_texture& operator=(d3d12_texture&& o) noexcept
        {
            assert(this != &o);
            if (this != &o)
            {
                release();
                move(o);
            }
            return *this;
        }

        ~d3d12_texture() { release(); }

        void release();
        [[nodiscard]] constexpr ID3D12Resource* const resource() const { return m_resource; }

    private:
        constexpr void move(d3d12_texture& o)
        {
            m_resource = o.m_resource;
            o.reset();
        }

        constexpr void reset()
        {
            m_resource = nullptr;
        }

        ID3D12Resource* m_resource{ nullptr };
    };

    // --- render texture ----------------------------------------------
    class Render_Target
    {
    public:
        constexpr static UINT max_mips{ 14 }; // support up to 16k resolutions.
        Render_Target() = default;
        explicit Render_Target(D3D12_RESOURCE_DESC desc);
        // disable copy
        explicit Render_Target(const Render_Target&) = delete;
        Render_Target& operator=(const Render_Target&) = delete;

        // move
        constexpr Render_Target(Render_Target&& o) noexcept
            : m_resource{ std::move(o.m_resource) }, m_srv{ o.m_srv }, m_mip_count{ o.m_mip_count }
        {
            for (UINT i{ 0 }; i < m_mip_count; ++i)
            {
                m_rtv[i] = o.m_rtv[i];
            }
            o.reset();
        }

        constexpr Render_Target& operator=(Render_Target&& o) noexcept
        {
            assert(this != &o);
            if (this != &o)
            {
                release();
                move(o);
            }
            return *this;
        }

        ~Render_Target() { release(); }

        void release();

        [[nodiscard]] constexpr UINT mip_count() const { return m_mip_count; }
        [[nodiscard]] constexpr D3D12_CPU_DESCRIPTOR_HANDLE rtv(UINT mip_index) const { assert(mip_index < m_mip_count); return m_rtv[mip_index].cpu; }
        [[nodiscard]] constexpr Descriptor_Handle srv() const { return m_srv; }
        [[nodiscard]] constexpr ID3D12Resource* const resource() const { return m_resource; }

    private:
        constexpr void move(Render_Target& o)
        {
            m_resource = std::move(o.m_resource);
            m_srv = o.m_srv;
            m_mip_count = o.m_mip_count;
            for (UINT i{ 0 }; i < m_mip_count; ++i)
            {
                m_rtv[i] = o.m_rtv[i];
            }
            o.reset();
        }

        constexpr void reset()
        {
            m_mip_count = 0;
            m_srv = {};
            for (UINT i{ 0 }; i < m_mip_count; ++i)
            {
                m_rtv[i] = {};
            }
        }

        ID3D12Resource* m_resource{ nullptr };
        Descriptor_Handle m_srv{};
        Descriptor_Handle m_rtv[max_mips]{};
        UINT m_mip_count{ 0 };
    };

    // --- depth buffer ----------------------------------------
    class Depth_Buffer
    {
    public:
        Depth_Buffer() = default;
        explicit Depth_Buffer(UINT width, UINT height);
        DISABLE_COPY(Depth_Buffer);
        constexpr Depth_Buffer(Depth_Buffer&& o) noexcept
            : m_texture{ std::move(o.m_texture) }, m_dsv{ o.m_dsv }
        {
            o.reset();
        }

        constexpr Depth_Buffer& operator=(Depth_Buffer&& o) noexcept
        {
            assert(this != &o);
            if (this != &o)
            {
                release();
                move(o);
            }
            return *this;
        }

        ~Depth_Buffer()
        {
            release();
        }

        void release();
        [[nodiscard]] constexpr D3D12_CPU_DESCRIPTOR_HANDLE dsv() const { return m_dsv.cpu; }
        [[nodiscard]] constexpr ID3D12Resource* const resource() const { return m_texture.resource(); }

    private:
        constexpr void move(Depth_Buffer& o)
        {
            m_texture = std::move(o.m_texture);
            m_dsv = o.m_dsv;
            o.reset();
        }

        constexpr void reset()
        {
            m_dsv = {};
        }

        d3d12_texture m_texture{};
        Descriptor_Handle m_dsv{};
    };
}