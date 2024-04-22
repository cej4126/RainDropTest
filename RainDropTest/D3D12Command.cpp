#include "D3D12Command.h"

D3D12Command::D3D12Command(id3d12_device* device, D3D12_COMMAND_LIST_TYPE type)
{
    D3D12_COMMAND_QUEUE_DESC desc{};
    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_command_queue)));
    NAME_D3D12_COMPTR_OBJECT(m_command_queue);

    for (UINT i{ 0 }; i < Frame_Count; ++i)
    {
        command_frame& frame{ m_command_frame[i] };
        ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&frame.command_allocator)));
        NAME_D3D12_COMPTR_OBJECT(frame.command_allocator);
    }

    ThrowIfFailed(device->CreateCommandList(0, type, m_command_frame[0].command_allocator.Get(), nullptr, IID_PPV_ARGS(&m_command_list)));
    NAME_D3D12_COMPTR_OBJECT(m_command_list);
    //m_command_list->Close();

    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    NAME_D3D12_COMPTR_OBJECT(m_fence);

    m_fence_event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    if (!m_fence_event)
    {
        throw;
    }
}

D3D12Command::~D3D12Command()
{
}

void D3D12Command::BeginFrame()
{
    command_frame& frame{ m_command_frame[m_frame_index] };
    frame.wait(m_fence_event, m_fence.Get());
    ThrowIfFailed(frame.command_allocator->Reset());
    m_command_list->Reset(frame.command_allocator.Get(), nullptr);
}

#ifdef USE_RENDER
void D3D12Command::EndFrame(const D3D12Surface& surface)
#else
void D3D12Command::EndFrame(IDXGISwapChain4* swap_chain)
#endif
{
    m_command_list->Close();
    ID3D12CommandList* const command_lists[]{ m_command_list.Get() };
    m_command_queue->ExecuteCommandLists(_countof(command_lists), &command_lists[0]);

    // Presenting swap chain buffers happens in lockstep with frame buffers.
#ifdef USE_RENDER
    surface.present();
#else
    swap_chain->Present(1, 0);
#endif

    //UINT64& fence_value{ m_fence_value };
    //++fence_value;
    //command_frame& frame{ m_command_frame[m_frame_index] };
    //frame.fence_value = fence_value;
    //m_command_queue->Signal(m_fence.Get(), fence_value);
    
   // m_frame_index = (m_frame_index + 1) % Frame_Count;
}


void D3D12Command::Flush()
{
    for (UINT i{ 0 }; i < Frame_Count; ++i)
    {
        m_command_frame[i].wait(m_fence_event, m_fence.Get());
    }
    m_frame_index = 0;
}

void D3D12Command::Release()
{
    Flush();
    m_fence_value = 0;

    CloseHandle(m_fence_event);
    m_fence_event = nullptr;

    for (UINT i{ 0 }; i < Frame_Count; ++i)
    {
        m_command_frame[i].release();
    }

}

void D3D12Command::command_frame::wait(HANDLE fence_event, ID3D12Fence1* fence) const
{
    assert(fence_event && fence);
    if (fence->GetCompletedValue() < fence_value)
    {
        fence->SetEventOnCompletion(fence_value, fence_event);
        WaitForSingleObject(fence_event, INFINITE);
    }
}

void D3D12Command::command_frame::release()
{
    fence_value = 0;
}

