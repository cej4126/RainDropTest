#include "Buffers.h"
#include "Helpers.h"
#include "DXSampleHelper.h"
#include "RainDrop.h"
#include "Upload.h"
#include "Main.h"

namespace d3d12::buffers {

    ID3D12Resource* create_buffer_default_with_upload(const void* data, UINT size, D3D12_RESOURCE_FLAGS flags /* = D3D12_RESOURCE_FLAG_NONE */)
    {
        assert(data && size);
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;  //  D3D12_RESOURCE_DIMENSION Dimension;
        desc.Alignment = 0;                                //  UINT64 Alignment;
        desc.Width = size;                                 //  UINT64 Width;
        desc.Height = 1;                                   //  UINT Height;
        desc.DepthOrArraySize = 1;                         //  UINT16 DepthOrArraySize;
        desc.MipLevels = 1;                                //  UINT16 MipLevels;
        desc.Format = DXGI_FORMAT_UNKNOWN;                 //  DXGI_FORMAT Format;
        desc.SampleDesc = { 1, 0 };                        //  DXGI_SAMPLE_DESC SampleDesc;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;      //  D3D12_TEXTURE_LAYOUT Layout;
        desc.Flags = flags;                                //  D3D12_RESOURCE_FLAGS Flags;

        ID3D12Resource* resource{ nullptr };
        ThrowIfFailed(core::device()->CreateCommittedResource(&d3dx::heap_properties.default_heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON,
            nullptr, IID_PPV_ARGS(&resource)));

        upload::Upload_Context context{ size };
        memcpy(context.cpu_address(), data, size);
        context.command_list()->CopyResource(resource, context.upload_buffer());
        context.end_upload();

        assert(resource);
        return resource;
    }
}
