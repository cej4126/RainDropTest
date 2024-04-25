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
#include <memory>
#include <mutex>

#include <string>

constexpr UINT Frame_Count{ 3 };
constexpr UINT Invalid_Index{ 0xffffffff };
using id3d12_device = ID3D12Device9;
using id3d12_graphics_command_list = ID3D12GraphicsCommandList6;

using namespace DirectX;
using Microsoft::WRL::ComPtr;

#ifdef _DEBUG
#define DEBUG_OP(x) x
#else
#define DEBUG_OP(x)
#endif
