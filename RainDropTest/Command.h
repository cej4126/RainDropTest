#pragma once
#include "stdafx.h"
#include "DXApp.h"
//#include "Surface.h"

namespace surface {
    class Surface;
}

namespace command {
    class Command
    {
    public:
        Command() = default;
        explicit Command(id3d12_device* const device, D3D12_COMMAND_LIST_TYPE type);

        explicit Command(const Command&) = delete;
        Command& operator=(const Command&) = delete;
        explicit Command(Command&&) = delete;

        Command& operator=(Command&&) = delete;

        ~Command();

        void begin_frame();
        void end_frame(const surface::Surface& surface);
        void flush();
        void release();

        [[nodiscard]] ID3D12CommandQueue* const command_queue() const { return m_command_queue; }
        [[nodiscard]] ID3D12GraphicsCommandList6* const command_list() const { return m_command_list; }
        [[nodiscard]] constexpr UINT frame_index() const { return m_frame_index; }

    private:
        struct command_frame
        {
            ID3D12CommandAllocator* command_allocator;
            UINT64 fence_value{ 0 };
            void wait(HANDLE fence_event, ID3D12Fence1* fence) const;
            void release();
        };

        ID3D12CommandQueue* m_command_queue;
        id3d12_graphics_command_list* m_command_list;
        ID3D12Fence1* m_fence;
        UINT64 m_fence_value{ 0 };
        command_frame m_command_frame[Frame_Count]{};
        HANDLE m_fence_event{ nullptr };
        UINT m_frame_index{ 0 };
    };

}