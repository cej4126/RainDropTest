#pragma once
#include "stdafx.h"
#include <filesystem>
#include <fstream>

namespace util {

    bool
        read_file(std::filesystem::path path, std::unique_ptr<UINT8[]>& data, UINT64& size)
    {
        if (!std::filesystem::exists(path)) return false;

        size = std::filesystem::file_size(path);
        assert(size);
        if (!size) return false;
        data = std::make_unique<UINT8[]>(size);
        std::ifstream file{ path, std::ios::in | std::ios::binary };
        if (!file || !file.read((char*)data.get(), size))
        {
            file.close();
            return false;
        }

        file.close();
        return true;
    }
}