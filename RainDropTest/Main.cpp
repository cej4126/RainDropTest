#include "Main.h"
#include "Core.h"
#include <filesystem>

d3d12::Core g_core(1280, 720, L"D3D12 rain");

std::filesystem::path set_current_directoyr_to_executable_path()
{
    // set the working directory to the executable path
    wchar_t path[MAX_PATH];
    const uint32_t length{ GetModuleFileName(0, &path[0], MAX_PATH) };
    if (!length || GetLastError() == ERROR_INSUFFICIENT_BUFFER) return {};
    std::filesystem::path p{ path };
    std::filesystem::current_path(p.parent_path());
    return std::filesystem::current_path();
}

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    set_current_directoyr_to_executable_path();

    return Win32Application::Run(&g_core, hInstance, nCmdShow);
}

namespace d3d12::core{

    id3d12_device* const device() { return g_core.device(); }

    ID3D12Fence* render_context_fence() { return g_core.render_context_fence(); }

    UINT current_frame_index()
    {
        return g_core.current_frame_index();
    }

    void set_deferred_release_flag()
    {
        g_core.set_deferred_releases_flag();
    }

    void main_deferred_release(IUnknown* resource)
    {
        g_core.deferred_release(resource);
    }

    Descriptor_Heap& rtv_heap() { return g_core.rtv_heap(); }
    Descriptor_Heap& dsv_heap() { return g_core.dsv_heap(); }
    Descriptor_Heap& srv_heap() { return g_core.srv_heap(); }
    Descriptor_Heap& uav_heap() { return g_core.uav_heap(); }
    constant_buffer& cbuffer() { return g_core.cbuffer(); }
    UINT64 get_render_context_fence_value() { return g_core.get_render_context_fence_value(); }
    void set_render_context_fence_value() { g_core.set_render_context_fence_value(); }
}