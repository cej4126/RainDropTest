#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <D3DCompiler.h>
#include <DirectXMath.h>
#include <stdio.h>
#include <pix.h>

#include <wrl.h>
#include <vector>
#include <shellapi.h>

#include <string>

constexpr UINT Frame_Count{ 3 };
using id3d12_device = ID3D12Device9;
using id3d12_graphics_command_list = ID3D12GraphicsCommandList6;
