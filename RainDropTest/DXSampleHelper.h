#pragma once

constexpr struct {
    const D3D12_HEAP_PROPERTIES default_heap{
        D3D12_HEAP_TYPE_DEFAULT,                        // Type
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,                // CPUPageProperty
        D3D12_MEMORY_POOL_UNKNOWN,                      // MemoryPoolPreference
        0,                                              // CreationNodeMask
        0                                               // VisibleNodeMask
    };

    const D3D12_HEAP_PROPERTIES upload_heap{
        D3D12_HEAP_TYPE_UPLOAD,                         // Type
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,                // CPUPageProperty
        D3D12_MEMORY_POOL_UNKNOWN,                      // MemoryPoolPreference
        0,                                              // CreationNodeMask
        0                                               // VisibleNodeMask
    };
} heap_properties;

constexpr struct {
    const D3D12_RASTERIZER_DESC face_cull{
        D3D12_FILL_MODE_SOLID,                          //  D3D12_FILL_MODE FillMode;
        D3D12_CULL_MODE_BACK,                           //  D3D12_CULL_MODE CullMode;
        FALSE,                                          //  BOOL FrontCounterClockwise;
        D3D12_DEFAULT_DEPTH_BIAS,                       //  INT DepthBias;
        D3D12_DEFAULT_DEPTH_BIAS_CLAMP,                 //  FLOAT DepthBiasClamp;
        D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,          //  FLOAT SlopeScaledDepthBias;
        TRUE,                                           //  BOOL DepthClipEnable;
        FALSE,                                          //  BOOL MultisampleEnable;
        FALSE,                                          //  BOOL AntialiasedLineEnable;
        0,                                              //  UINT ForcedSampleCount;
        D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF       //  D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
    };
} rasterizer_state;

constexpr struct {
    const D3D12_DEPTH_STENCIL_DESC enable{
        TRUE,                                          // BOOL DepthEnable;
        D3D12_DEPTH_WRITE_MASK_ZERO,                    // D3D12_DEPTH_WRITE_MASK DepthWriteMask;
        D3D12_COMPARISON_FUNC_LESS,                     // D3D12_COMPARISON_FUNC DepthFunc;
        FALSE,                                          // BOOL StencilEnable;
        D3D12_DEFAULT_STENCIL_READ_MASK,                // UINT8 StencilReadMask;
        D3D12_DEFAULT_STENCIL_WRITE_MASK,               // UINT8 StencilWriteMask;
        {                                               // FrontFace
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilFailOp;
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilDepthFailOp;
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilPassOp;
            D3D12_COMPARISON_FUNC_ALWAYS,                   // D3D12_COMPARISON_FUNC StencilFunc;
        },
        {                                               // BackFace
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilFailOp;
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilDepthFailOp;
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilPassOp;
            D3D12_COMPARISON_FUNC_ALWAYS,                   // D3D12_COMPARISON_FUNC StencilFunc;
        }
    };

    const D3D12_DEPTH_STENCIL_DESC disable{
        FALSE,                                          // BOOL DepthEnable;
        D3D12_DEPTH_WRITE_MASK_ZERO,                    // D3D12_DEPTH_WRITE_MASK DepthWriteMask;
        D3D12_COMPARISON_FUNC_LESS,                     // D3D12_COMPARISON_FUNC DepthFunc;
        FALSE,                                          // BOOL StencilEnable;
        D3D12_DEFAULT_STENCIL_READ_MASK,                // UINT8 StencilReadMask;
        D3D12_DEFAULT_STENCIL_WRITE_MASK,               // UINT8 StencilWriteMask;
        {                                               // FrontFace
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilFailOp;
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilDepthFailOp;
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilPassOp;
            D3D12_COMPARISON_FUNC_ALWAYS,                   // D3D12_COMPARISON_FUNC StencilFunc;
        },
        {                                               // BackFace
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilFailOp;
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilDepthFailOp;
            D3D12_STENCIL_OP_KEEP,                          // D3D12_STENCIL_OP StencilPassOp;
            D3D12_COMPARISON_FUNC_ALWAYS,                   // D3D12_COMPARISON_FUNC StencilFunc;
        }
    };
} depth_desc_state;


constexpr struct {
    const D3D12_BLEND_DESC blend_desc{
        FALSE,                                          // BOOL AlphaToCoverageEnable;
        FALSE,                                          // BOOL IndependentBlendEnable;
        {
            {
                TRUE,                                   // BOOL BlendEnable;
                FALSE,                                  // BOOL LogicOpEnable;
                D3D12_BLEND_SRC_ALPHA,                  // D3D12_BLEND SrcBlend;
                D3D12_BLEND_ONE,                        // D3D12_BLEND DestBlend;
                D3D12_BLEND_OP_ADD,                     // D3D12_BLEND_OP BlendOp;
                D3D12_BLEND_ZERO,                       // D3D12_BLEND SrcBlendAlpha;
                D3D12_BLEND_ZERO,                       // D3D12_BLEND DestBlendAlpha;
                D3D12_BLEND_OP_ADD,                     // D3D12_BLEND_OP BlendOpAlpha;
                D3D12_LOGIC_OP_NOOP,                    // D3D12_LOGIC_OP LogicOp;
                D3D12_COLOR_WRITE_ENABLE_ALL,           // UINT8 RenderTargetWriteMask;
            },
            {},{},{},{},{},{},{}
        }
    };
} blend_state;

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
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

// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x) SetName(x.Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed(x[n].Get(), L#x, n)
