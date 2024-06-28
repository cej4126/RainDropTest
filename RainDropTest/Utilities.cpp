#pragma once
#include "Utilities.h"
#include <fstream>

namespace utl {

    bool read_file(std::filesystem::path path, std::unique_ptr<UINT8[]>& data, UINT64& size)
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

    std::wstring
        to_wstring(const char* c)
    {
        std::string s{ c };
        return { s.begin(), s.end() };
    }

}