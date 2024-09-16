#pragma once
#include "stdafx.h"
#include "string"
#include "DXSampleHelper.h"
#include "Application.h"
#include <Windows.h>
#include "Entity.h"

class dx_app
{
public:
    bool initialize();
    void run();

    void shutdown();

    void OnKeyDown(UINT8) {}
    void OnKeyUp(UINT8) {}

    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }

    game_entity::entity create_entity_item(XMFLOAT3 position, XMFLOAT3 rotation, const char* script_name);
    void create_render_items();

    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

protected:
    std::wstring GetAssetFullPath(LPCWSTR assetName);
    void GetHardwareAdapter(_In_ IDXGIFactory2* pFactory, _Outptr_opt_result_maybenull_ IDXGIAdapter1** ppAdapter);
    void SetCustomWindowText(LPCWSTR text);




    UINT m_width;
    UINT m_height;
    float m_aspect_ratio;

    bool m_use_warp_device;

private:
    std::wstring m_assetsPath;
    std::wstring m_title;

    UINT m_cube_entity_id{ Invalid_Index };
    UINT m_cube_item_id{ Invalid_Index };

    UINT m_material_id{ Invalid_Index };
};

