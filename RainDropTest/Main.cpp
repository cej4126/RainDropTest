#include "Main.h"
#include "D3D12RainDrop.h"

D3D12RainDrop m_rain(1280, 720, L"D3D12 rain");

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    //    D3D12RainDrop rain(1280, 720, L"D3D12 rain");
    return Win32Application::Run(&m_rain, hInstance, nCmdShow);
}


namespace core {

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