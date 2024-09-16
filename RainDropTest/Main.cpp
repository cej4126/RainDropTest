#include "Main.h"
#include "Core.h"
#include <filesystem>
#include "DXApp.h"


std::filesystem::path set_current_directoyr_to_executable_path()
{
    // set the working directory to the executable path
    wchar_t path[MAX_PATH];
    const UINT length{ GetModuleFileName(0, &path[0], MAX_PATH) };
    if (!length || GetLastError() == ERROR_INSUFFICIENT_BUFFER) return {};
    std::filesystem::path p{ path };
    std::filesystem::current_path(p.parent_path());
    return std::filesystem::current_path();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    set_current_directoyr_to_executable_path();

    dx_app app{};
    if (app.initialize())
    {
        MSG msg{};
        bool is_running{ true };
        while (is_running)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                is_running &= (msg.message != WM_QUIT);
            }

            app.run();
        }
    }

    app.shutdown();
    return 0;
}

//namespace d3d12::core{
//
//    id3d12_device* const device() { return g_core.device(); }
//
//    ID3D12Fence* render_context_fence() { return g_core.render_context_fence(); }
//
//    UINT current_frame_index()
//    {
//        return g_core.current_frame_index();
//    }
//
//    void set_deferred_release_flag()
//    {
//        g_core.set_deferred_releases_flag();
//    }
//
//    void main_deferred_release(IUnknown* resource)
//    {
//        g_core.deferred_release(resource);
//    }
//
//    Descriptor_Heap& rtv_heap() { return g_core.rtv_heap(); }
//    Descriptor_Heap& dsv_heap() { return g_core.dsv_heap(); }
//    Descriptor_Heap& srv_heap() { return g_core.srv_heap(); }
//    Descriptor_Heap& uav_heap() { return g_core.uav_heap(); }
//    constant_buffer& cbuffer() { return g_core.cbuffer(); }
//    UINT64 get_render_context_fence_value() { return g_core.get_render_context_fence_value(); }
//    void set_render_context_fence_value() { g_core.set_render_context_fence_value(); }
//}