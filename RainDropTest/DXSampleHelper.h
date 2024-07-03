#pragma once
#include "stdafx.h"


namespace d3d12::d3dx {

    // IntermediateOffset = 0;
    // FirstSubresource = 0;
    // NumSubresources = 1;
    UINT64 Update_Subresource(id3d12_graphics_command_list* command_list, ID3D12Resource* p_destination_resource, ID3D12Resource* p_intermediate_resource, D3D12_SUBRESOURCE_DATA* p_src_data);

    ID3D12RootSignature* create_root_signature(const D3D12_ROOT_SIGNATURE_DESC1& desc);

    struct d3d12_descriptor_range : public D3D12_DESCRIPTOR_RANGE1
    {
        constexpr explicit d3d12_descriptor_range(D3D12_DESCRIPTOR_RANGE_TYPE range_type,
            UINT descriptor_count, UINT shader_register, UINT space = 0,
            D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
            UINT offset_from_table_start = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
            : D3D12_DESCRIPTOR_RANGE1{ range_type, descriptor_count, shader_register, space, flags, offset_from_table_start }
        {}
    };

    struct d3d12_root_parameter : public D3D12_ROOT_PARAMETER1
    {
        constexpr void as_constants(UINT number_of_constants, D3D12_SHADER_VISIBILITY visibility, UINT shader_register, UINT space = 0)
        {
            ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            Constants.ShaderRegister = shader_register;
            Constants.RegisterSpace = space;
            Constants.Num32BitValues = number_of_constants;
            ShaderVisibility = visibility;
        }

        constexpr void as_cbv(D3D12_SHADER_VISIBILITY visibility,
            UINT shader_register, UINT space = 0,
            D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE)
        {
            as_descriptor(D3D12_ROOT_PARAMETER_TYPE_CBV, visibility, shader_register, space, flags);
        }

        constexpr void as_srv(D3D12_SHADER_VISIBILITY visibility,
            UINT shader_register, UINT space = 0,
            D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE)
        {
            as_descriptor(D3D12_ROOT_PARAMETER_TYPE_SRV, visibility, shader_register, space, flags);
        }

        constexpr void as_uav(D3D12_SHADER_VISIBILITY visibility,
            UINT shader_register, UINT space = 0,
            D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE)
        {
            as_descriptor(D3D12_ROOT_PARAMETER_TYPE_UAV, visibility, shader_register, space, flags);
        }

        constexpr void as_descriptor_table(D3D12_SHADER_VISIBILITY visibility,
            const d3d12_descriptor_range* ranges, UINT range_count)
        {
            ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            ShaderVisibility = visibility;
            DescriptorTable.NumDescriptorRanges = range_count;
            DescriptorTable.pDescriptorRanges = ranges;
        }

    private:
        constexpr void as_descriptor(D3D12_ROOT_PARAMETER_TYPE type, D3D12_SHADER_VISIBILITY visibility,
            UINT shader_register, UINT space, D3D12_ROOT_DESCRIPTOR_FLAGS flags)
        {
            ParameterType = type;
            ShaderVisibility = visibility;
            Descriptor.ShaderRegister = shader_register;
            Descriptor.RegisterSpace = space;
            Descriptor.Flags = flags;
        }
    };

    // Maximum 64 DWORDs (u32's) divided up amongst all root parameters.
    // Root constants = 1 DWORD per 32-bit constant
    // Root descriptor  (CBV, SRV or UAV) = 2 DWORDs each
    // Descriptor table pointer = 1 DWORD
    // Static samplers = 0 DWORDs (compiled into shader)
    struct d3d12_root_signature_desc : public D3D12_ROOT_SIGNATURE_DESC1
    {
        constexpr static D3D12_ROOT_SIGNATURE_FLAGS default_flags{
            D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
        };

        constexpr explicit d3d12_root_signature_desc(const d3d12_root_parameter* parameters,
            UINT parameter_count,
            D3D12_ROOT_SIGNATURE_FLAGS flags = default_flags,
            const D3D12_STATIC_SAMPLER_DESC* static_samplers = nullptr,
            UINT sampler_count = 0)
            : D3D12_ROOT_SIGNATURE_DESC1{ parameter_count, parameters, sampler_count, static_samplers, flags }
        {}

        ID3D12RootSignature* create() const
        {
            return create_root_signature(*this);
        }
    };

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