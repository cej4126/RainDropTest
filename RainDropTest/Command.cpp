#include "Command.h"
#include "Main.h"

namespace d3d12::command {
    Command::Command(id3d12_device* const device, D3D12_COMMAND_LIST_TYPE type)
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type = type;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;

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

    Command::~Command()
    {
    }

    void Command::BeginFrame()
    {
        command_frame& frame{ m_command_frame[m_frame_index] };
        frame.wait(m_fence_event, m_fence.Get());
        ThrowIfFailed(frame.command_allocator->Reset());
        m_command_list->Reset(frame.command_allocator.Get(), nullptr);
    }

    void Command::EndFrame(const surface::Surface& surface)
    {
        m_command_list->Close();
        ID3D12CommandList* const command_lists[]{ m_command_list.Get() };
        m_command_queue->ExecuteCommandLists(_countof(command_lists), &command_lists[0]);

        // Presenting swap chain buffers happens in lockstep with frame buffers.
        surface.present();
    }

    void Command::Flush()
    {
        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            m_command_frame[i].wait(m_fence_event, m_fence.Get());
        }
        m_frame_index = 0;
    }

    void Command::Release()
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

    void Command::command_frame::wait(HANDLE fence_event, ID3D12Fence1* fence) const
    {
        assert(fence_event && fence);
        if (fence->GetCompletedValue() < fence_value)
        {
            fence->SetEventOnCompletion(fence_value, fence_event);
            WaitForSingleObject(fence_event, INFINITE);
        }
    }

    void Command::command_frame::release()
    {
        fence_value = 0;
    }

}