#include "stdafx.h"
#include "DXSampleHelper.h"
#include "Main.h"

namespace d3d12::d3dx {

    ID3D12RootSignature* create_root_signature(const D3D12_ROOT_SIGNATURE_DESC1& desc)
    {
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_desc{};
        versioned_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        versioned_desc.Desc_1_1 = desc;

        using namespace Microsoft::WRL;
        ComPtr<ID3DBlob> signature_blob{ nullptr };
        ComPtr<ID3DBlob> error_blob{ nullptr };
        HRESULT hr{ S_OK };
        if (FAILED(hr = D3D12SerializeVersionedRootSignature(&versioned_desc, &signature_blob, &error_blob)))
        {
            DEBUG_OP(const char* error_msg{ error_blob ? (const char*)error_blob->GetBufferPointer() : "" });
            DEBUG_OP(OutputDebugStringA(error_msg));
            return nullptr;
        }

        ID3D12RootSignature* signature{ nullptr };
        ThrowIfFailed(core::device()->CreateRootSignature(0, signature_blob->GetBufferPointer(),
            signature_blob->GetBufferSize(), IID_PPV_ARGS(&signature)));

        if (FAILED(hr))
        {
            core::release(signature);
        }

        return signature;
    }

    // IntermediateOffset = 0;
    // FirstSubresource = 0;
    // NumSubresources = 1;
    UINT64 Update_Subresource(id3d12_graphics_command_list* command_list, ID3D12Resource* p_destination_resource, ID3D12Resource* p_intermediate_resource, D3D12_SUBRESOURCE_DATA* p_src_data)
    {
        // Call 1
        const UINT Number_Subresources = 1;
        UINT64 required_size = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts;
        UINT number_of_rows;
        UINT64 row_sizes_in_bytes;

        D3D12_RESOURCE_DESC destination_desc = p_destination_resource->GetDesc();
        core::device()->GetCopyableFootprints(&destination_desc, 0, 1, 0, &layouts, &number_of_rows, &row_sizes_in_bytes, &required_size);
        D3D12_RESOURCE_DESC intermediate_desc = p_intermediate_resource->GetDesc();
        core::device()->GetCopyableFootprints(&intermediate_desc, 0, 1, 0, &layouts, &number_of_rows, &row_sizes_in_bytes, &required_size);

        // 	return UpdateSubresources(pCmdList, pDestinationResource, pIntermediate, FirstSubresource, NumSubresources, RequiredSize, Layouts, NumRows, RowSizesInBytes, pSrcData);
        //  Call 2                                                                          0                 1                
        UINT64 memory_to_allocate = static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64)) * Number_Subresources;
        if (memory_to_allocate > SIZE_MAX)
        {
            throw;
        }

        void* p_memory = HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(memory_to_allocate));
        if (p_memory == NULL)
        {
            throw;
        }

        if (intermediate_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
            intermediate_desc.Width < required_size + layouts.Offset ||
            required_size  >(SIZE_T) - 1 || row_sizes_in_bytes > (SIZE_T)-1 ||
            destination_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            throw;
        }

        BYTE* p_intermediate_data{ nullptr };
        ThrowIfFailed(p_intermediate_resource->Map(0, NULL, reinterpret_cast<void**>(&p_intermediate_data)));

        D3D12_MEMCPY_DEST dest_data = { p_intermediate_data + layouts.Offset, layouts.Footprint.RowPitch, layouts.Footprint.RowPitch * number_of_rows };
        // MemcpySubresource(&DestData, &pSrcData[i], (SIZE_T)pRowSizesInBytes[i], pNumRows[i], pLayouts[i].Footprint.Depth);
        // MemcpySubresource(&dest_data, &p_src_data[0], row_sizes_in_bytes, number_of_rows, layouts.Footprint.Depth);
        for (UINT z = 0; z < layouts.Footprint.Depth; ++z)
        {
            BYTE* p_desc_slice = reinterpret_cast<BYTE*>(dest_data.pData) + dest_data.SlicePitch * z;
            const BYTE* p_src_slice = reinterpret_cast<const BYTE*>(p_src_data->pData) + p_src_data->SlicePitch * z;
            for (UINT y = 0; y < number_of_rows; ++y)
            {
                memcpy(p_desc_slice + dest_data.RowPitch * y, p_src_slice + p_src_data->RowPitch * y, row_sizes_in_bytes);
            }
        }
        // MemcpySubresource -------------------

        p_intermediate_resource->Unmap(0, NULL);

        if (destination_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            throw;
        }

        command_list->CopyBufferRegion(p_destination_resource, 0, p_intermediate_resource, (UINT)layouts.Offset, layouts.Footprint.Width);

        return required_size;
    }
}
