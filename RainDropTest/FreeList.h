// Copyright (c) Arash Khatami
// Distributed under the MIT license. See the LICENSE file in the project root for more information.
#pragma once
#include "stdafx.h"
#include "Vector.h"

namespace utl {

#if USE_STL_VECTOR
#pragma message("WARNING: using utl::free_list with std::vector result in duplicate calls to class destructor!")
#endif

    template<typename T>
    class free_list
    {
        static_assert(sizeof(T) >= sizeof(UINT));
    public:
        free_list() = default;
        explicit free_list(UINT count)
        {
            _array.reserve(count);
        }

        ~free_list()
        {
            assert(!_size);
#if USE_STL_VECTOR
            memset(_array.data(), 0, _array.size() * sizeof(T));
#endif
        }

        template<class... params>
        constexpr UINT add(params&&... p)
        {
            UINT id{ Invalid_Index };
            if (_next_free_index == Invalid_Index)
            {
                id = (UINT)_array.size();
                _array.emplace_back(std::forward<params>(p)...);
            }
            else
            {
                id = _next_free_index;
                assert(id < _array.size() && already_removed(id, true));
                _next_free_index = *(const UINT* const)std::addressof(_array[id]);
                new (std::addressof(_array[id])) T(std::forward<params>(p)...);
            }
            ++_size;
            return id;
        }

        constexpr void remove(UINT id)
        {
            assert(id < _array.size() && !already_removed(id, false));
            T& item{ _array[id] };
            item.~T();
            DEBUG_OP(memset(std::addressof(_array[id]), 0xcc, sizeof(T)));
            *(UINT* const)std::addressof(_array[id]) = _next_free_index;
            _next_free_index = id;
            --_size;
        }

        constexpr UINT size() const
        {
            return _size;
        }

        constexpr UINT capacity() const
        {
            return (UINT)_array.size();
        }

        constexpr bool empty() const
        {
            return _size == 0;
        }

        [[nodiscard]] constexpr T& operator[](UINT id)
        {
            assert(id < _array.size() && !already_removed(id, false));
            return _array[id];
        }

        [[nodiscard]] constexpr const T& operator[](UINT id) const
        {
            assert(id < _array.size() && !already_removed(id, false));
            return _array[id];
        }

    private:
        constexpr bool already_removed(UINT id, bool return_value_when_sizeof_t_equals_4) const
        {
            // NOTE: when sizeof(T) == sizeof(UINT) we can't test if the item was already removed!
            if constexpr (sizeof(T) > sizeof(UINT))
            {
                UINT i{ sizeof(UINT) }; //skip the first 4 bytes.
                const UINT8* const p{ (const UINT8* const)std::addressof(_array[id]) };
                while ((p[i] == 0xcc) && (i < sizeof(T))) ++i;
                return i == sizeof(T);
            }
            else
            {
                return return_value_when_sizeof_t_equals_4;
            }
        }
#if USE_STL_VECTOR
        utl::vector<T>          _array;
#else
        utl::vector<T, false>   _array;
#endif
        UINT                     _next_free_index{ Invalid_Index };
        UINT                     _size{ 0 };
    };
}
