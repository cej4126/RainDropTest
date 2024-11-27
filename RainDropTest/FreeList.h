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
        free_list(UINT list_index) : m_list_index{ list_index } {}
        explicit free_list(UINT count, UINT list_index)
        {
            m_list_index = list_index;
            m_array.reserve(count);
        }

        ~free_list()
        {
            if (m_size)
            {
                assert(!m_size);
            }
#if USE_STL_VECTOR
            memset(_array.data(), 0, m_array.size() * sizeof(T));
#endif
        }

        template<class... params>
        constexpr UINT add(params&&... p)
        {
            UINT id{ Invalid_Index };
            if (m_next_free_index == Invalid_Index)
            {
                id = (UINT)m_array.size();
                m_array.emplace_back(std::forward<params>(p)...);
            }
            else
            {
                id = m_next_free_index;
                assert(id < m_array.size() && already_removed(id, true));
                m_next_free_index = *(const UINT* const)std::addressof(m_array[id]);
                new (std::addressof(m_array[id])) T(std::forward<params>(p)...);
            }
            ++m_size;
            return id;
        }

        constexpr void remove(UINT id)
        {
            assert(id < m_array.size() && !already_removed(id, false));
            T& item{ m_array[id] };
            item.~T();
            DEBUG_OP(memset(std::addressof(m_array[id]), 0xcc, sizeof(T)));
            *(UINT* const)std::addressof(m_array[id]) = m_next_free_index;
            m_next_free_index = id;
            --m_size;
        }

        constexpr UINT size() const
        {
            return m_size;
        }

        constexpr UINT capacity() const
        {
            return (UINT)m_array.size();
        }

        constexpr bool empty() const
        {
            return m_size == 0;
        }

        [[nodiscard]] constexpr T& operator[](UINT id)
        {
            if (id >= m_array.size() || already_removed(id, false))
            {
                assert(id < m_array.size() && !already_removed(id, false));
            }
            return m_array[id];
        }

        [[nodiscard]] constexpr const T& operator[](UINT id) const
        {
            assert(id < m_array.size() && !already_removed(id, false));
            return m_array[id];
        }

    private:
        constexpr bool already_removed(UINT id, bool return_value_when_sizeof_t_equals_4) const
        {
            // NOTE: when sizeof(T) == sizeof(UINT) we can't test if the item was already removed!
            if constexpr (sizeof(T) > sizeof(UINT))
            {
                UINT i{ sizeof(UINT) }; //skip the first 4 bytes.
                const UINT8* const p{ (const UINT8* const)std::addressof(m_array[id]) };
                while ((p[i] == 0xcc) && (i < sizeof(T))) ++i;
                return i == sizeof(T);
            }
            else
            {
                return return_value_when_sizeof_t_equals_4;
            }
        }
#if USE_STL_VECTOR
        utl::vector<T>          m_array;
#else
        utl::vector<T, false>   m_array;
#endif
        UINT                     m_next_free_index{ Invalid_Index };
        UINT                     m_size{ 0 };
        UINT                     m_list_index;
    };
}
