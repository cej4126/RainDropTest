#pragma once
#include "stdafx.h"

namespace d3d12::d3dx {
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
        const D3D12_RASTERIZER_DESC no_cull{
            D3D12_FILL_MODE_SOLID,                          // FillMode
            D3D12_CULL_MODE_NONE,                           // CullMode
            TRUE,                                           // FrontCounterClockwise
            D3D12_DEFAULT_DEPTH_BIAS,                       // DepthBias
            D3D12_DEFAULT_DEPTH_BIAS_CLAMP,                 // DepthBiasClamp
            D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,          // SlopeScaledDepthBias
            TRUE,                                           // DepthClipEnable
            FALSE,                                          // MultisampleEnable
            FALSE,                                          // AntialiasedLineEnable
            0,                                              // ForcedSampleCount
            D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,      // ConservativeRaster
        };

        const D3D12_RASTERIZER_DESC face_cull{
            D3D12_FILL_MODE_SOLID,                          //  D3D12_FILL_MODE FillMode;
            D3D12_CULL_MODE_BACK,                           //  D3D12_CULL_MODE CullMode;
            FALSE,                                          //  BOOL FrontCounterClockwise;
            D3D12_DEFAULT_DEPTH_BIAS,                       //  INT DepthBias;
            D3D12_DEFAULT_DEPTH_BIAS_CLAMP,                 //  FLOAT DepthBiasClamp;
            D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,          //  FLOAT SlopeScaledDepthBias;
            TRUE,                                           //  BOOL DepthClipEnable;
            FALSE,                                          //  BOOL Multi sampleEnable;
            FALSE,                                          //  BOOL Ant aliasedLineEnable;
            0,                                              //  UINT ForcedSampleCount;
            D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF       //  D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
        };
    } rasterizer_state;

    constexpr struct {
        const D3D12_DEPTH_STENCIL_DESC enable {
            TRUE,                                           // BOOL DepthEnable;
            D3D12_DEPTH_WRITE_MASK_ALL,                     // D3D12_DEPTH_WRITE_MASK DepthWriteMask;
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

        const D3D12_DEPTH_STENCIL_DESC disable {
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

        const D3D12_DEPTH_STENCIL_DESC reversed {
            TRUE,                                           // BOOL DepthEnable;
            D3D12_DEPTH_WRITE_MASK_ALL,                     // D3D12_DEPTH_WRITE_MASK DepthWriteMask;
            D3D12_COMPARISON_FUNC_GREATER_EQUAL,            // D3D12_COMPARISON_FUNC DepthFunc;
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
        const D3D12_DEPTH_STENCIL_DESC1 disabled{
            0,                                              // DepthEnable
            D3D12_DEPTH_WRITE_MASK_ZERO,                    // DepthWriteMask
            D3D12_COMPARISON_FUNC_LESS_EQUAL,               // DepthFunc
            0,                                              // StencilEnable
            0,                                              // StencilReadMask
            0,                                              // StencilWriteMask
            {},                                             // FrontFace
            {},                                             // BackFace
            0                                               // DepthBoundsTestEnable
        };

        const D3D12_DEPTH_STENCIL_DESC1 enabled{
            1,                                              // DepthEnable
            D3D12_DEPTH_WRITE_MASK_ALL,                     // DepthWriteMask
            D3D12_COMPARISON_FUNC_LESS_EQUAL,               // DepthFunc
            0,                                              // StencilEnable
            0,                                              // StencilReadMask
            0,                                              // StencilWriteMask
            {},                                             // FrontFace
            {},                                             // BackFace
            0                                               // DepthBoundsTestEnable
        };

        const D3D12_DEPTH_STENCIL_DESC1 enabled_readonly{
            1,                                              // DepthEnable
            D3D12_DEPTH_WRITE_MASK_ZERO,                    // DepthWriteMask
            D3D12_COMPARISON_FUNC_LESS_EQUAL,               // DepthFunc
            0,                                              // StencilEnable
            0,                                              // StencilReadMask
            0,                                              // StencilWriteMask
            {},                                             // FrontFace
            {},                                             // BackFace
            0                                               // DepthBoundsTestEnable
        };

        const D3D12_DEPTH_STENCIL_DESC1 reversed{
            1,                                              // DepthEnable
            D3D12_DEPTH_WRITE_MASK_ALL,                     // DepthWriteMask
            D3D12_COMPARISON_FUNC_GREATER_EQUAL,            // DepthFunc
            0,                                              // StencilEnable
            0,                                              // StencilReadMask
            0,                                              // StencilWriteMask
            {},                                             // FrontFace
            {},                                             // BackFace
            0                                               // DepthBoundsTestEnable
        };

        const D3D12_DEPTH_STENCIL_DESC1 reversed_readonly{
            1,                                              // DepthEnable
            D3D12_DEPTH_WRITE_MASK_ZERO,                    // DepthWriteMask
            D3D12_COMPARISON_FUNC_GREATER_EQUAL,            // DepthFunc
            0,                                              // StencilEnable
            0,                                              // StencilReadMask
            0,                                              // StencilWriteMask
            {},                                             // FrontFace
            {},                                             // BackFace
            0                                               // DepthBoundsTestEnable
        };
    } depth_state;

    constexpr struct {
        const D3D12_BLEND_DESC disabled
        {
            FALSE,                                              // AlphaToCoverageEnable
            FALSE,                                              // IndependentBlendEnable
            {
               {
                   FALSE,                                       // BlendEnable
                   FALSE,                                       // LogicOpEnable
                   D3D12_BLEND_SRC_ALPHA,                   // SrcBlend
                   D3D12_BLEND_INV_SRC_ALPHA,               // DestBlend
                   D3D12_BLEND_OP_ADD,                      // BlendOp
                   D3D12_BLEND_ONE,                         // SrcBlendAlpha
                   D3D12_BLEND_ONE,                         // DestBlendAlpha
                   D3D12_BLEND_OP_ADD,                      // BlendOpAlpha
                   D3D12_LOGIC_OP_NOOP,                     // LogicOp
                   D3D12_COLOR_WRITE_ENABLE_ALL,            // RenderTargetWriteMask
               },
               {},{},{},{},{},{},{}
            }
        };

        const D3D12_BLEND_DESC blend_desc{
            FALSE,                                          // BOOL AlphaToCoverageEnable;
            FALSE,                                          // BOOL IndependentBlendEnable;
            {
                {
                    FALSE,                                  // BOOL BlendEnable;
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
            //D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            //D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS
            //D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS |
            //D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            //D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
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


#pragma warning(push)
#pragma warning(disable : 4324) // disable padding warining
    template<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type, typename T>
    class alignas(void*) d3d12_pipeline_state_subobject
    {
    public:
        d3d12_pipeline_state_subobject() = default;
        constexpr explicit d3d12_pipeline_state_subobject(T subobject) : _type{ type }, _subobject{ subobject } {}
        d3d12_pipeline_state_subobject& operator=(const T& subobject) { _subobject = subobject; return *this; }
    private:
        const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _type{ type };
        T _subobject{};
    };
#pragma warning(pop)

    // Pipeline State Subobject (PSS) macro
#define PSS(name, ...) using d3d12_pipeline_state_subobject_##name = d3d12_pipeline_state_subobject<__VA_ARGS__>;

    PSS(root_signature, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, ID3D12RootSignature*);
    PSS(vs, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, D3D12_SHADER_BYTECODE);
    PSS(ps, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, D3D12_SHADER_BYTECODE);
    PSS(ds, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS, D3D12_SHADER_BYTECODE);
    PSS(hs, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS, D3D12_SHADER_BYTECODE);
    PSS(gs, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS, D3D12_SHADER_BYTECODE);
    PSS(cs, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS, D3D12_SHADER_BYTECODE);
    PSS(stream_output, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT, D3D12_STREAM_OUTPUT_DESC);
    PSS(blend, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, D3D12_BLEND_DESC);
    PSS(sample_mask, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK, UINT);
    PSS(rasterizer, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, D3D12_RASTERIZER_DESC);
    PSS(depth_stencil, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL, D3D12_DEPTH_STENCIL_DESC);
    PSS(input_layout, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT, D3D12_INPUT_LAYOUT_DESC);
    PSS(ib_strip_cut_value, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE);
    PSS(primitive_topology, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY, D3D12_PRIMITIVE_TOPOLOGY_TYPE);
    PSS(render_target_formats, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, D3D12_RT_FORMAT_ARRAY);
    PSS(depth_stencil_format, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, DXGI_FORMAT);
    PSS(sample_desc, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC, DXGI_SAMPLE_DESC);
    PSS(node_mask, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK, UINT);
    PSS(cached_pso, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO, D3D12_CACHED_PIPELINE_STATE);
    PSS(flags, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS, D3D12_PIPELINE_STATE_FLAGS);
    PSS(depth_stencil1, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1, D3D12_DEPTH_STENCIL_DESC1);
    PSS(view_instancing, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING, D3D12_VIEW_INSTANCING_DESC);
    PSS(as, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, D3D12_SHADER_BYTECODE);
    PSS(ms, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, D3D12_SHADER_BYTECODE);

#undef PSS

    struct d3d12_pipeline_state_subobject_stream {
        d3d12_pipeline_state_subobject_root_signature           root_signature{ nullptr };
        d3d12_pipeline_state_subobject_vs                       vs{};
        d3d12_pipeline_state_subobject_ps                       ps{};
        d3d12_pipeline_state_subobject_ds                       ds{};
        d3d12_pipeline_state_subobject_hs                       hs{};
        d3d12_pipeline_state_subobject_gs                       gs{};
        d3d12_pipeline_state_subobject_cs                       cs{};
        d3d12_pipeline_state_subobject_stream_output            stream_output{};
        d3d12_pipeline_state_subobject_blend                    blend{ blend_state.disabled };
        d3d12_pipeline_state_subobject_sample_mask              sample_mask{ UINT_MAX };
        d3d12_pipeline_state_subobject_rasterizer               rasterizer{ rasterizer_state.no_cull };
        d3d12_pipeline_state_subobject_input_layout             input_layout{};
        d3d12_pipeline_state_subobject_ib_strip_cut_value       ib_strip_cut_value{};
        d3d12_pipeline_state_subobject_primitive_topology       primitive_topology{};
        d3d12_pipeline_state_subobject_render_target_formats    render_target_formats{};
        d3d12_pipeline_state_subobject_depth_stencil_format     depth_stencil_format{};
        d3d12_pipeline_state_subobject_sample_desc              sample_desc{ {1, 0} };
        d3d12_pipeline_state_subobject_node_mask                node_mask{};
        d3d12_pipeline_state_subobject_cached_pso               cached_pso{};
        d3d12_pipeline_state_subobject_flags                    flags{};
        d3d12_pipeline_state_subobject_depth_stencil1           depth_stencil1{ depth_state.disabled };
        d3d12_pipeline_state_subobject_view_instancing          view_instancing{};
        d3d12_pipeline_state_subobject_as                       as{};
        d3d12_pipeline_state_subobject_ms                       ms{};
    };

    ID3D12PipelineState* create_pipeline_state(D3D12_PIPELINE_STATE_STREAM_DESC desc);
    ID3D12PipelineState* create_pipeline_state(void* stream, UINT64 stream_size);


}