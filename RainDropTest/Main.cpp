#include "Main.h"
#include "RainDrop.h"
#include <filesystem>

d3d12::RainDrop m_rain(1280, 720, L"D3D12 rain");

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

    //    RainDrop rain(1280, 720, L"D3D12 rain");
    return Win32Application::Run(&m_rain, hInstance, nCmdShow);
}

namespace d3d12::core{

    id3d12_device* const device()
    {
        return m_rain.device();
    }

    UINT current_frame_index()
    {
        return m_rain.current_frame_index();
    }

    void set_deferred_release_flag()
    {
        m_rain.set_deferred_releases_flag();
    }

    void main_deferred_release(IUnknown* resource)
    {
        m_rain.deferred_release(resource);
    }

    Descriptor_Heap& rtv_heap() { return m_rain.rtv_heap(); }
    Descriptor_Heap& dsv_heap() { return m_rain.dsv_heap(); }
    Descriptor_Heap& srv_heap() { return m_rain.srv_heap(); }
    Descriptor_Heap& uav_heap() { return m_rain.uav_heap(); }
}