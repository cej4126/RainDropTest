#include "D3D12Surface.h"
#include "Main.h"

void D3D12Surface::create_swap_chain(IDXGIFactory7* factory, ID3D12CommandQueue* command_queue)
{
    assert(factory && command_queue);

    if (SUCCEEDED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &m_allow_tearing, sizeof(UINT))) && m_allow_tearing)
    {
        m_allow_tearing = DXGI_PRESENT_ALLOW_TEARING;
    }
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = m_width;                                                  // UINT Width;
    desc.Height = m_height;                                                // UINT Height;
    desc.Format = default_back_buffer_format;                              // DXGI_FORMAT Format;
    desc.Stereo = false;                                                   // BOOL Stereo;
    desc.SampleDesc = { 1, 0 };                                            // DXGI_SAMPLE_DESC SampleDesc;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;                    // DXGI_USAGE BufferUsage;
    desc.BufferCount = buffer_count;                                       // UINT BufferCount;
    desc.Scaling = DXGI_SCALING_STRETCH;                                   // DXGI_SCALING Scaling;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;                       // DXGI_SWAP_EFFECT SwapEffect;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;                          // DXGI_ALPHA_MODE AlphaMode;
    desc.Flags = m_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0; // UINT Flags;

    IDXGISwapChain1* swap_chain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(command_queue, m_handle, &desc, nullptr, nullptr, &swap_chain));
    ThrowIfFailed(factory->MakeWindowAssociation(m_handle, DXGI_MWA_NO_ALT_ENTER));
    swap_chain->QueryInterface(IID_PPV_ARGS(&m_swap_chain));
    core::release(swap_chain);

    m_current_bb_index = m_swap_chain->GetCurrentBackBufferIndex();

    for (UINT i{ 0 }; i < buffer_count; ++i)
    {
        m_render_targets[i].rtv = core::rtv_heap().allocator();
    }
}

void D3D12Surface::present() const
{
    assert(m_swap_chain);
    m_swap_chain->Present(0, m_present_flag);
    m_current_bb_index = m_swap_chain->GetCurrentBackBufferIndex();
}

void D3D12Surface::resize()
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

void D3D12Surface::finalize()
{
    // create RTVs for back-buffers
    D3D12_RENDER_TARGET_VIEW_DESC render_target_desc{};
    render_target_desc.Format = default_back_buffer_format;
    render_target_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    for (UINT i{ 0 }; i < buffer_count; ++i)
    {
        render_target& render_target_iterm{ m_render_targets[i] };
        assert(!render_target_iterm.resource);
        ThrowIfFailed(m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_target_iterm.resource)));

        core::device()->CreateRenderTargetView(render_target_iterm.resource, &render_target_desc, render_target_iterm.rtv.cpu);
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    ThrowIfFailed(m_swap_chain->GetDesc(&desc));
    assert(m_width == desc.BufferDesc.Width && m_height == desc.BufferDesc.Height);

    m_viewport = { 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f };
    m_scissor_rectangle = { 0, 0, (INT)m_width, (INT)m_height };
}

void D3D12Surface::release()
{
    for (UINT i{ 0 }; i < buffer_count; ++i)
    {
        render_target& render_target_iterm{ m_render_targets[i] };
        core::release(render_target_iterm.resource);
        core::rtv_heap().free(render_target_iterm.rtv);
    }

    core::release(m_swap_chain);
}
