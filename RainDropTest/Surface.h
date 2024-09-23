#pragma once
#include "stdafx.h"
#include "Resources.h"
#include "Window.h"
#include "Core.h"

namespace surface {

    //class surface_item  
    //{
    //public:
    //    constexpr explicit surface_item(UINT id) : m_id{ id } {}
    //    constexpr surface_item() = default;
    //    constexpr UINT get_id() const { return m_id; }
    //    constexpr bool is_valid() const { return m_id != Invalid_Index; }

    //    void resize(UINT width, UINT height) const;
    //    UINT width() const;
    //    UINT height() const;
    //    void render(core::frame_info info) const;
    //private:
    //    UINT m_id{ Invalid_Index };
    //};

    class Surface
    {
    public:
        constexpr explicit Surface(UINT id) : m_id{ id } {}
        constexpr Surface() = default;
        constexpr UINT get_id() const { return m_id; }
        constexpr bool is_valid() const { return m_id != Invalid_Index; }
            
        //constexpr static DXGI_FORMAT default_back_buffer_format{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB };
        constexpr static DXGI_FORMAT default_back_buffer_format{ DXGI_FORMAT_R8G8B8A8_UNORM };
        constexpr static UINT buffer_count{ 3 };

        explicit Surface(windows::window window)
            : m_window{ window }
        {
            assert(m_window.handle());
        }

        //DISABLE_COPY_AND_MOVE(Surface);
        ~Surface() { release(); }

        void create_swap_chain(IDXGIFactory7* factory, ID3D12CommandQueue* command_queue);
        void present() const;
        void resize();

        //UINT set_current_bb_index();

        [[nodiscard]] constexpr UINT width() const { return (UINT)m_viewport.Width; }
        [[nodiscard]] constexpr UINT height() const { return (UINT)m_viewport.Height; }
        [[nodiscard]] constexpr ID3D12Resource* const back_buffer() const { return m_render_targets[m_current_bb_index].resource; }
        [[nodiscard]] constexpr D3D12_CPU_DESCRIPTOR_HANDLE rtv() const { return m_render_targets[m_current_bb_index].rtv.cpu; }
        [[nodiscard]] constexpr const D3D12_VIEWPORT& viewport() const { return m_viewport; }
        [[nodiscard]] constexpr const D3D12_RECT& scissor_rect() const { return m_scissor_rectangle; }
        [[nodiscard]] constexpr HANDLE swap_chain_event() const { return m_swap_chain_event; }

    private:
        void finalize();
        void release();

        struct render_target
        {
            ID3D12Resource* resource{ nullptr };
            resource::Descriptor_Handle rtv{};
        };

        IDXGISwapChain4* m_swap_chain{ nullptr };
        render_target m_render_targets[buffer_count]{};
        windows::window m_window{};
        mutable UINT m_current_bb_index{ 0 };
        UINT m_allow_tearing{ 0 };
        UINT m_present_flag{ 0 };
        D3D12_VIEWPORT m_viewport{};
        D3D12_RECT m_scissor_rectangle{};
        HANDLE m_swap_chain_event{};

        UINT m_id{ Invalid_Index };
    };

    Surface create(windows::window window);
    void remove(UINT id);
    surface::Surface& get_surface(UINT id);
}
