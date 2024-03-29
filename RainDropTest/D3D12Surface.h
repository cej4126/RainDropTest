#pragma once
#include "stdafx.h"
#include "D3D12Resources.h"

class D3D12Surface
{
public:
    D3D12Surface() {}
    constexpr static DXGI_FORMAT default_back_buffer_format{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB };
    constexpr static UINT buffer_count{ 3 };

    constexpr explicit D3D12Surface(HWND handle, UINT width, UINT height)
        : m_handle{ handle }, m_width{ width }, m_height{ height }
    {
        assert(handle);
    }

    // disable copy
    explicit D3D12Surface(const D3D12Surface&) = delete;
    D3D12Surface& operator=(const D3D12Surface&) = delete;

    // set the move
    constexpr D3D12Surface(D3D12Surface&& o) noexcept :
        m_swap_chain{ o.m_swap_chain }, m_width{ o.m_width }, m_height{ o.m_height }, m_handle{ o.m_handle },
        m_current_bb_index{ o.m_current_bb_index },
        m_allow_tearing{ o.m_allow_tearing }, m_present_flag{ o.m_present_flag },
        m_viewport{ o.m_viewport }, m_scissor_rectangle{ o.m_scissor_rectangle }
    {
        for (UINT i{ 0 }; i < buffer_count; ++i)
        {
            m_render_targets[i].resource = o.m_render_targets[i].resource;
            m_render_targets[i].rtv = o.m_render_targets[i].rtv;
        }

        o.reset();
    }

    constexpr D3D12Surface& operator=(D3D12Surface&& o) noexcept
    {
        assert(this != &o);
        if (this != &o)
        {
            release();
            move(o);
        }

        return *this;
    }

    ~D3D12Surface() { release(); }

    void create_swap_chain(IDXGIFactory7* factory, ID3D12CommandQueue* command_queue);
    void present() const;
    void resize();

    [[nodiscard]] constexpr UINT width() const { return m_width; }
    [[nodiscard]] constexpr UINT height() const { return m_height; }
    [[nodiscard]] constexpr ID3D12Resource* const back_buffer() const { return m_render_targets[m_current_bb_index].resource; }
    [[nodiscard]] constexpr D3D12_CPU_DESCRIPTOR_HANDLE rtv() const { return m_render_targets[m_current_bb_index].rtv.cpu; }
    [[nodiscard]] constexpr D3D12_VIEWPORT viewport() const { return m_viewport; }
    [[nodiscard]] constexpr D3D12_RECT scissor_rect() const { return m_scissor_rectangle; }

private:
    void finalize();
    void release();

    constexpr void move(D3D12Surface& o)
    {
        m_swap_chain = o.m_swap_chain;
        for (UINT i{ 0 }; i < buffer_count; ++i)
        {
            m_render_targets[i].resource = o.m_render_targets[i].resource;
            m_render_targets[i].rtv = o.m_render_targets[i].rtv;
        }
        m_width = o.m_width;
        m_height = o.m_height;
        m_handle = o.m_handle;
        m_current_bb_index = 0;
        m_allow_tearing = o.m_allow_tearing;
        m_present_flag = o.m_present_flag;
        m_viewport = o.m_viewport;
        m_scissor_rectangle = o.m_scissor_rectangle;

        o.reset();
    }

    constexpr void reset()
    {
        m_swap_chain = nullptr;
        for (UINT i{ 0 }; i < buffer_count; ++i)
        {
            m_render_targets[i] = {};
        }
        m_width = 0;
        m_height = 0;
        m_handle = nullptr;
        m_current_bb_index = 0;
        m_allow_tearing = 0;
        m_present_flag = 0;
        m_viewport = {};
        m_scissor_rectangle = {};
    }

    struct render_target
    {
        ID3D12Resource* resource{ nullptr };
        Descriptor_Handle rtv{};
    };

    IDXGISwapChain4* m_swap_chain{ nullptr };
    render_target m_render_targets[buffer_count]{};
    UINT m_width{ 0 };
    UINT m_height{ 0 };
    HWND m_handle{ nullptr };
    mutable UINT m_current_bb_index{ 0 };

    UINT m_allow_tearing{ 0 };
    UINT m_present_flag{ 0 };
    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT m_scissor_rectangle{};
};

