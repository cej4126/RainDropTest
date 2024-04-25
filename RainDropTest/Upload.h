#pragma once
#include "stdafx.h"

namespace upload {

    class Upload_Context
    {
    public:
        Upload_Context(UINT aligned_size);

        explicit Upload_Context(const Upload_Context&) = delete;
        Upload_Context& operator=(const Upload_Context&) = delete;
        explicit Upload_Context(Upload_Context&&) = delete;
        Upload_Context& operator=(Upload_Context&&) = delete;
        ~Upload_Context()
        {
            assert(m_frame_index == Invalid_Index);
        }

        void end_upload();

        [[nodiscard]] constexpr id3d12_graphics_command_list* command_list() const { return m_command_list; }
        [[nodiscard]] constexpr ID3D12Resource* upload_buffer() const { return m_upload_buffer; }
        [[nodiscard]] constexpr void* const cpu_address() const { return m_cpu_address; }

    private:
        id3d12_graphics_command_list* m_command_list{ nullptr };
        ID3D12Resource* m_upload_buffer{ nullptr };
        void* m_cpu_address{ nullptr };
        UINT m_frame_index{ Invalid_Index };
    };

    bool initialize();
    void shutdown();
}
