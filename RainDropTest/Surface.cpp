#include "Surface.h"
#include "Main.h"
#include "FreeList.h"
#include "Resources.h"
#include "Window.h"
#include "Core.h"
#include "Lights.h"

namespace surface {
    
    namespace {

        utl::free_list<Surface> surfaces{ 40 };

    } // anonymous namespace

    void Surface::create_swap_chain(IDXGIFactory7* factory, ID3D12CommandQueue* command_queue)
    {
        assert(factory && command_queue);
        release();

        if (SUCCEEDED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &m_allow_tearing, sizeof(UINT))) && m_allow_tearing)
        {
            m_allow_tearing = DXGI_PRESENT_ALLOW_TEARING;
        }

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.BufferCount = buffer_count;                                       // UINT BufferCount;
        desc.Width = m_window.width();                                                  // UINT Width;
        desc.Height = m_window.height();                                                // UINT Height;
        desc.Format = default_back_buffer_format;                              // DXGI_FORMAT Format;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;                    // DXGI_USAGE BufferUsage;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;                       // DXGI_SWAP_EFFECT SwapEffect;
        desc.SampleDesc = { 1, 0 };                                            // DXGI_SAMPLE_DESC SampleDesc;
        desc.Flags = m_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        // maybe need to the compute raindrop?
        //desc.Flags = m_allow_tearing
        //    ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
        //    : DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT; // UINT Flags;
        desc.Scaling = DXGI_SCALING_STRETCH;                                   // DXGI_SCALING Scaling;
        desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;                          // DXGI_ALPHA_MODE AlphaMode;
        desc.Stereo = false;                                                   // BOOL Stereo;

        IDXGISwapChain1* swap_chain;
        HWND hwnd{ (HWND)m_window.handle() };
        ThrowIfFailed(factory->CreateSwapChainForHwnd(command_queue, hwnd, &desc, nullptr, nullptr, &swap_chain));
        ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
        swap_chain->QueryInterface(IID_PPV_ARGS(&m_swap_chain));
        //m_swap_chain_event = m_swap_chain->GetFrameLatencyWaitableObject();

        core::release(swap_chain);

        m_current_bb_index = m_swap_chain->GetCurrentBackBufferIndex();

        for (UINT i{ 0 }; i < buffer_count; ++i)
        {
            m_render_targets[i].rtv = core::rtv_heap().allocate();
        }

        finalize();

        assert(m_light_id == Invalid_Index);
        m_light_id = lights::add_cull_light();
    }

    void Surface::present() const
    {
        assert(m_swap_chain);
        m_swap_chain->Present(1, m_present_flag);
        m_current_bb_index = m_swap_chain->GetCurrentBackBufferIndex();
    }

    void Surface::resize()
    {
        assert(m_swap_chain);
        for (UINT i{ 0 }; i < buffer_count; ++i)
        {
            core::release(m_render_targets[i].resource);
        }

        const UINT flags{ m_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0ul };
        ThrowIfFailed(m_swap_chain->ResizeBuffers(buffer_count, 0, 0, DXGI_FORMAT_UNKNOWN, flags));
        m_current_bb_index = m_swap_chain->GetCurrentBackBufferIndex();

        finalize();
    }

    void Surface::render(core::frame_info info) const
    {
        core::render(m_id, info);
    }

    void Surface::finalize()
    {
        // create RTVs for back-buffers

        for (UINT i{ 0 }; i < buffer_count; ++i)
        {
            render_target& render_target_items{ m_render_targets[i] };
            assert(!render_target_items.resource);
            ThrowIfFailed(m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_target_items.resource)));

            D3D12_RENDER_TARGET_VIEW_DESC render_target_desc{};
            render_target_desc.Format = default_back_buffer_format;
            render_target_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            core::device()->CreateRenderTargetView(render_target_items.resource, &render_target_desc, render_target_items.rtv.cpu);
        }

        DXGI_SWAP_CHAIN_DESC desc{};
        ThrowIfFailed(m_swap_chain->GetDesc(&desc));
        UINT width{ m_window.width() };
        UINT height{ m_window.height() };
        assert(width == desc.BufferDesc.Width && height == desc.BufferDesc.Height);

        m_viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
        m_scissor_rectangle = { 0, 0, (INT)width, (INT)height };
    }

    void Surface::release()
    {
        if (m_light_id != Invalid_Index)
        {
            lights::remove_cull_light(m_light_id);
        }

        for (UINT i{ 0 }; i < buffer_count; ++i)
        {
            render_target& render_target_iterm{ m_render_targets[i] };
            core::release(render_target_iterm.resource);
            core::rtv_heap().free_handle(render_target_iterm.rtv);
        }

        core::release(m_swap_chain);
    }
    
    UINT create(windows::window window)
    {
        UINT id{ surfaces.add(window) };
        surfaces[id].create_swap_chain(core::factory(), core::command_queue());
        surfaces[id].set_id(id);
        return id;
    }

    void remove(UINT id)
    {
        core::flush();
        surfaces.remove(id);
    }

    Surface& get_surface(UINT id)
    {
        assert(id != Invalid_Index);
        return surfaces[id];
    }
}