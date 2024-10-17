#pragma once
#include "stdafx.h"

namespace resource {

    struct Descriptor_Handle {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        UINT index{ Invalid_Index };

        [[nodiscard]] constexpr bool is_valid() const { return cpu.ptr != 0; }
        [[nodiscard]] constexpr bool is_shader_visible() const { return gpu.ptr != 0; }
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
        void free_handle(Descriptor_Handle& handle);

        [[nodiscard]] constexpr D3D12_DESCRIPTOR_HEAP_TYPE type() const { return m_type; }
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

    // --- buffer ---------------------------------------------------------------

    class Buffer
    {
    public:
        Buffer() = default;
        explicit Buffer(UINT size, UINT alignment, D3D12_RESOURCE_FLAGS flag, bool cpu_accessible);
        DISABLE_COPY(Buffer);
        constexpr Buffer(Buffer&& o)
            : m_buffer{ o.m_buffer }, m_gpu_address{ o.m_gpu_address }, m_size{ o.m_size }

        {
            o.reset();
        }

        constexpr Buffer& operator=(Buffer&& o)
        {
            assert(this != &o);
            if (this != &o)
            {
                release();
                move(o);
            }

            return *this;
        }

        ~Buffer() { release(); }

        void release();

        [[nodiscard]] constexpr ID3D12Resource* const buffer() const { return m_buffer; }
        [[nodiscard]] constexpr D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const { return m_gpu_address; }
        [[nodiscard]] constexpr UINT size() const { return m_size; }

    private:
        constexpr void move(Buffer& o)
        {
            m_buffer = o.m_buffer;
            m_gpu_address = o.m_gpu_address;
            m_size = o.m_size;
            o.reset();
        }

        constexpr void reset()
        {
            m_buffer = nullptr;
            m_gpu_address = 0;
            m_size = 0;
        }

        ID3D12Resource* m_buffer{ nullptr };
        D3D12_GPU_VIRTUAL_ADDRESS m_gpu_address{ 0 };
        UINT m_size{ 0 };
    };

    // --- constant buffer ------------------------------------------------------

    class constant_buffer
    {
    public:
        constant_buffer() = default;
        explicit constant_buffer(UINT size, UINT alignment);
        DISABLE_COPY(constant_buffer);
        constexpr constant_buffer(constant_buffer&& o) noexcept
            : m_buffer{o.m_buffer }, m_gpu_address{ o.m_gpu_address }, m_size{ o.m_size },
            m_cpu_address{ o.m_cpu_address }, m_cpu_offset{ o.m_cpu_offset }

        {
            o.reset();
        }

        constexpr constant_buffer& operator=(constant_buffer&& o) noexcept
        {
            assert(this != &o);
            if (this != &o)
            {
                release();
                move(o);
            }

            return *this;
        }

        ~constant_buffer() { release(); }

        void release();

        constexpr void clear()
        {
            m_cpu_offset = 0;
        }
        [[nodiscard]] UINT8* const allocate(UINT size);

        template<typename T>
        [[nodiscard]] T* const allocate()
        {
            return (T* const)allocate(sizeof(T));
        }

        [[nodiscard]] constexpr ID3D12Resource* const buffer() const { return m_buffer; }
        [[nodiscard]] constexpr D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const { return m_gpu_address; }
        [[nodiscard]] constexpr UINT size() const { return m_size; }
        [[nodiscard]] constexpr UINT8* const cpu_address() const { return m_cpu_address; }

        template<typename T>
        [[nodiscard]] constexpr D3D12_GPU_VIRTUAL_ADDRESS gpu_address(T* const allocation)
        {
            std::lock_guard lock{ m_mutex };
            if (!m_cpu_address)
            {
                assert(m_cpu_address);
                return {};
            }

            const UINT8* const address{ (const UINT8* const)allocation };
            assert(address <= m_cpu_address + m_cpu_offset);
            assert(address >= m_cpu_address);

            const UINT64 offset{ (UINT64)(address - m_cpu_address) };
            return m_gpu_address + offset;
        }
    private:

        constexpr void move(constant_buffer& o)
        {
            m_buffer = o.m_buffer;
            m_gpu_address = o.m_gpu_address;
            m_size = o.m_size;

            m_cpu_address = o.m_cpu_address;
            m_cpu_offset = o.m_cpu_offset;
        }

        constexpr void reset()
        {
            m_buffer = nullptr;
            m_gpu_address = 0;
            m_size = 0;

            m_cpu_address = 0;
            m_cpu_offset = 0;
        }

        // d3d12_buffer
        ID3D12Resource* m_buffer{ nullptr };
        D3D12_GPU_VIRTUAL_ADDRESS m_gpu_address{ 0 };
        UINT m_size{ 0 };

        UINT8* m_cpu_address{ 0 };
        UINT m_cpu_offset{ 0 };
        std::mutex m_mutex{};
    };

    // --- uav_buffer --------------------------------------------
    class uav_buffer
    {
    public:
        uav_buffer() = default;
        explicit uav_buffer(UINT size, UINT alignment, D3D12_RESOURCE_FLAGS flag);
        DISABLE_COPY(uav_buffer);
        constexpr uav_buffer(uav_buffer&& o)
            : m_buffer{ std::move(o.m_buffer) }, m_uav{ o.m_uav }, m_uav_shader_visible{ o.m_uav_shader_visible }
        {
            o.reset();
        }

        constexpr uav_buffer& operator=(uav_buffer&& o)
        {
            assert(this != &o);
            if (this != &o)
            {
                release();
                move(o);
            }
            return *this;
        }

        ~uav_buffer() { release(); }

        void release();

        void clear_uav(id3d12_graphics_command_list* cmd_list, const UINT* const values) const;
        void clear_uav(id3d12_graphics_command_list* cmd_list, const float* const values) const;

        [[nodiscard]] constexpr ID3D12Resource* buffer() const { return m_buffer.buffer(); }
        [[nodiscard]] constexpr D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const { return m_buffer.gpu_address(); }
        [[nodiscard]] constexpr UINT size() const { return m_buffer.size(); }
        [[nodiscard]] constexpr Descriptor_Handle uav() const { return m_uav; }
        [[nodiscard]] constexpr Descriptor_Handle uav_shader_visible() const { return m_uav_shader_visible; }

    private:

        constexpr void move(uav_buffer& o)
        {
            m_buffer = std::move(o.m_buffer);
            m_uav = o.m_uav;
            m_uav_shader_visible = o.m_uav_shader_visible;
            o.reset();
        }

        constexpr void reset()
        {
            m_uav = {};
            m_uav_shader_visible = {};
        }

        Buffer m_buffer{};
        Descriptor_Handle m_uav{};
        Descriptor_Handle m_uav_shader_visible{};
    };

    // --- texture -----------------------------------------------------

    struct d3d12_texture_init_info
    {
        ID3D12Resource* resource{ nullptr };
        D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc{ nullptr };
        D3D12_RESOURCE_DESC* desc{ nullptr };
        D3D12_RESOURCE_STATES initial_state{};
        D3D12_CLEAR_VALUE clear_value{};
    };

    class d3d12_texture
    {
    public:
        constexpr static UINT max_mips{ 14 };
        d3d12_texture() = default;
        explicit d3d12_texture(d3d12_texture_init_info info);
        DISABLE_COPY(d3d12_texture);
        constexpr d3d12_texture(d3d12_texture&& o) noexcept
            : m_resource{ o.m_resource }, m_srv{ o.m_srv }
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
        [[nodiscard]] constexpr Descriptor_Handle srv() const { return m_srv; }

    private:
        constexpr void move(d3d12_texture& o)
        {
            m_resource = o.m_resource;
            m_srv = o.m_srv;
            o.reset();
        }

        constexpr void reset()
        {
            m_resource = nullptr;
            m_srv = {};
        }

        ID3D12Resource* m_resource{ nullptr };
        Descriptor_Handle m_srv;
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

        explicit Depth_Buffer(const Depth_Buffer&) = delete;
        Depth_Buffer& operator=(const Depth_Buffer&) = delete;
        //DISABLE_COPY(Depth_Buffer);

        constexpr Depth_Buffer(Depth_Buffer&& o) noexcept
            : m_dsv{o.m_dsv}, m_resource{ o.m_resource }, m_srv{ o.m_srv }
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
        [[nodiscard]] constexpr Descriptor_Handle srv() const { return m_srv; }
        [[nodiscard]] constexpr ID3D12Resource* const resource() const { return m_resource; }

    private:
        constexpr void move(Depth_Buffer& o)
        {
            m_dsv = o.m_dsv;
            m_srv = o.m_srv;
            m_resource = std::move(o.m_resource);
            o.reset();
        }

        constexpr void reset()
        {
            m_dsv = {};
            m_srv = {};
            m_resource = nullptr;
        }

        //d3d12_texture m_texture{};
        Descriptor_Handle m_dsv{};
        ID3D12Resource* m_resource{ nullptr };
        Descriptor_Handle m_srv;




    };
}