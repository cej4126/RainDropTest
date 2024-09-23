#include "Upload.h"
#include "DXSampleHelper.h"
#include "Main.h"
#include "Helpers.h"
#include "Core.h"

namespace upload {

    namespace {
        struct upload_frame
        {
            ID3D12CommandAllocator* command_allocator{ nullptr };
            id3d12_graphics_command_list* command_list{ nullptr };
            ID3D12Resource* upload_buffer{ nullptr };
            void* cpu_address{ nullptr };
            UINT64 fence_value{ 0 };

            void wait_and_reset();

            void release();

            constexpr bool is_ready() const { return upload_buffer == nullptr; }
        };

        constexpr UINT upload_frame_count{ 4 };
        upload_frame upload_frames[upload_frame_count]{};
        ID3D12CommandQueue* upload_command_queue{ nullptr };
        ID3D12Fence1* upload_fence{ nullptr };
        UINT64 upload_fence_value{ 0 };
        HANDLE fence_event{};
        std::mutex frame_mutex{};
        std::mutex queue_mutex{};

        void upload_frame::wait_and_reset()
        {
            assert(upload_fence && fence_event);
            if (upload_fence->GetCompletedValue() < fence_value)
            {
                ThrowIfFailed(upload_fence->SetEventOnCompletion(fence_value, fence_event));
                WaitForSingleObject(fence_event, INFINITE);
            }

            core::release(upload_buffer);
            cpu_address = nullptr;
        }

        void upload_frame::release()
        {
            wait_and_reset();
            core::release(command_allocator);
            core::release(command_list);
        }

        // NOTE: frames should be locked before this function is called.
        UINT get_available_upload_frame()
        {
            UINT index{ Invalid_Index };
            while (index == Invalid_Index)
            {
                // check for open upload frames
                for (UINT i{ 0 }; i < upload_frame_count; ++i)
                {
                    if (upload_frames[i].is_ready())
                    {
                        index = i;
                        return index;
                    }
                }

                // All upload frames are busy, yield the cpu
                std::this_thread::yield();
            }
            return index;
        }

        bool init_failed()
        {
            shutdown();
            return false;
        }

    } // anonymous namespace


    Upload_Context::Upload_Context(UINT aligned_size)
    {
        assert(upload_command_queue);

        {
            // We don't want to lock this function for longer than necessary. So, we scope this lock.
            std::lock_guard lock{ frame_mutex };
            m_frame_index = get_available_upload_frame();
            assert(m_frame_index != Invalid_Index);
            // Before unlocking, we prevent other threads from picking
            // this frame by making is_ready return false.
            upload_frames[m_frame_index].upload_buffer = (ID3D12Resource*)1;
        }

        upload_frame& frame{ upload_frames[m_frame_index] };
        assert(aligned_size);

        // Create upload buffer
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;  //  D3D12_RESOURCE_DIMENSION Dimension;
        desc.Alignment = 0;                                //  UINT64 Alignment;
        desc.Width = aligned_size;                         //  UINT64 Width;
        desc.Height = 1;                                   //  UINT Height;
        desc.DepthOrArraySize = 1;                         //  UINT16 DepthOrArraySize;
        desc.MipLevels = 1;                                //  UINT16 MipLevels;
        desc.Format = DXGI_FORMAT_UNKNOWN;                 //  DXGI_FORMAT Format;
        desc.SampleDesc = { 1, 0 };                        //  DXGI_SAMPLE_DESC SampleDesc;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;      //  D3D12_TEXTURE_LAYOUT Layout;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;             //  D3D12_RESOURCE_FLAGS Flags;

        ThrowIfFailed(core::device()->CreateCommittedResource(&d3dx::heap_properties.upload_heap,
            D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&frame.upload_buffer)));
        assert(frame.upload_buffer);

        NAME_D3D12_OBJECT_INDEXED(frame.upload_buffer, aligned_size, L"Upload Buffer - size");

        const D3D12_RANGE range{};
        ThrowIfFailed(frame.upload_buffer->Map(0, &range, reinterpret_cast<void**>(&frame.cpu_address)));

        m_command_list = frame.command_list;
        m_upload_buffer = frame.upload_buffer;
        m_cpu_address = frame.cpu_address;
        assert(m_command_list && m_upload_buffer && m_cpu_address);

        ThrowIfFailed(frame.command_allocator->Reset());
        ThrowIfFailed(frame.command_list->Reset(frame.command_allocator, nullptr));
    }
 
    void Upload_Context::end_upload()
    {
        assert(m_frame_index != Invalid_Index);
        upload_frame& frame{ upload_frames[m_frame_index] };
        id3d12_graphics_command_list* const cmd_list{ frame.command_list };
        ThrowIfFailed(cmd_list->Close()); 

        std::lock_guard lock{ queue_mutex };

        ID3D12CommandList* const cmd_lists[]{ cmd_list };
        ID3D12CommandQueue* const cmd_queue{ upload_command_queue };
        cmd_queue->ExecuteCommandLists(_countof(cmd_lists), cmd_lists);

        ++upload_fence_value;
        frame.fence_value = upload_fence_value;
        ThrowIfFailed(cmd_queue->Signal(upload_fence, frame.fence_value));

        // Wait for copy queue to finish. Then release the upload buffer.
        frame.wait_and_reset();
        m_frame_index = Invalid_Index;
    }

    bool initialize()
    {
        id3d12_device* const device{ core::device() };
        assert(device && !upload_command_queue);

        for (UINT i{ 0 }; i < upload_frame_count; ++i)
        {
            upload_frame& frame{ upload_frames[i] };
            ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&frame.command_allocator)));
            NAME_D3D12_OBJECT_INDEXED(frame.command_allocator, i, L"Upload Command Allocator");

            ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, frame.command_allocator, nullptr, IID_PPV_ARGS(&frame.command_list)));
            NAME_D3D12_OBJECT_INDEXED(frame.command_list, i, L"Upload Command List");
            frame.command_list->Close();
        }

        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

        ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&upload_command_queue)));
        NAME_D3D12_OBJECT(upload_command_queue, L"Upload Copy Queue");

        ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&upload_fence)));
        NAME_D3D12_OBJECT(upload_fence, L"Upload Fence");

        fence_event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        assert(fence_event);
        if (!fence_event) return init_failed();

        return true;
    }

    void shutdown()
    {
        for (UINT i{ 0 }; i < upload_frame_count; ++i)
        {
            upload_frames[i].release();
        }

        if (fence_event)
        {
            CloseHandle(fence_event);
            fence_event = nullptr;
        }

        core::release(upload_command_queue);
        core::release(upload_fence);
        upload_fence_value = 0;
    }
}
