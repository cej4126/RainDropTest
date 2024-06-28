#pragma once
#include "stdafx.h"
#include <filesystem>

namespace utl {

    bool read_file(std::filesystem::path path, std::unique_ptr<UINT8[]>& data, UINT64& size);

    std::wstring to_wstring(const char* c);
}