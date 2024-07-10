#pragma once
#include "stdafx.h"


namespace d3d12::d3dx {

    // IntermediateOffset = 0;
    // FirstSubresource = 0;
    // NumSubresources = 1;
    UINT64 Update_Subresource(id3d12_graphics_command_list* command_list, ID3D12Resource* p_destination_resource, ID3D12Resource* p_intermediate_resource, D3D12_SUBRESOURCE_DATA* p_src_data);
}

inline void GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, UINT pathSize)
{
    if (path == nullptr)
    {
        throw std::exception();
    }

    DWORD size = GetModuleFileName(nullptr, path, pathSize);
    if (size == 0 || size == pathSize)
    {
        throw std::exception();
    }

    WCHAR* last_slash = wcsrchr(path, L'\\');
    if (last_slash == NULL)
    {
        last_slash = wcsrchr(path, L'/');
    }

    if (last_slash)
    {
        *(last_slash + 1) = L'\0';
    }
}

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR full_name[50];
    if (swprintf_s(full_name, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(full_name);
    }
}
#else
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
}
#endif

#ifdef _DEBUG
// Sets the name of the COM object and outputs a debug string int Visual Studio's output panel.
#define NAME_D3D12_OBJECT(obj, name) obj->SetName(name); OutputDebugString(L"::D3D12 Object Created: "); OutputDebugString(name); OutputDebugString(L"\n");
// The indexed variant will include the index in the name of the object
#define NAME_D3D12_OBJECT_INDEXED(obj, n, name)     \
{                                                   \
wchar_t full_name[128];                             \
if (swprintf_s(full_name, L"%s[%llu]", name, (UINT64)n) >0 ){ \
    obj->SetName(full_name);                        \
    OutputDebugString(L"::D3D12 Object Created: "); \
    OutputDebugString(full_name);                   \
    OutputDebugString(L"\n");                       \
}}
#else
#define NAME_D3D12_OBJECT(x, name)
#define NAME_D3D12_OBJECT_INDEXED(x, n, name)
#endif // _DEBUG

#define NAME_D3D12_COMPTR_OBJECT(x) SetName(x.Get(), L#x)
#define NAME_D3D12_COMPTR_OBJECT_INDEXED(x, n) SetNameIndexed(x[n].Get(), L#x, n)