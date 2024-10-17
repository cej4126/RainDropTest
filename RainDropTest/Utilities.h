#pragma once
#include "stdafx.h"
#include <filesystem>

namespace utl {

    bool read_file(std::filesystem::path path, std::unique_ptr<UINT8[]>& data, UINT64& size);

    std::wstring to_wstring(const char* c);

    class blob_stream_reader
    {
    public:
        DISABLE_COPY_AND_MOVE(blob_stream_reader);
        explicit blob_stream_reader(const UINT8* buffer)
            : m_buffer{ buffer }, m_position{ buffer }
        {
            assert(buffer);
        }

        // This template function is intended to read primitive types (e.g. int, float, bool)
        template<typename T>
        [[nodiscard]] constexpr T read()
        {
            static_assert(std::is_arithmetic_v<T>, "Template argument should be a primitive type.");
            T value{ *((T*)m_position) };
            m_position += sizeof(T);
            return value;
        }

        // reads 'length' bytes into 'buffer'. The caller is responsible to allocate enough memory in buffer.
        void read(UINT8* buffer, size_t length)
        {
            memcpy(buffer, m_position, length);
            m_position += length;
        }

        constexpr void skip(size_t offset)
        {
            m_position += offset;
        }

        [[nodiscard]] constexpr const UINT8* const buffer_start() const { return m_buffer; }
        [[nodiscard]] constexpr const UINT8* const position() const { return m_position; }
        [[nodiscard]] constexpr size_t offset() const { return m_position - m_buffer; }

    private:
        const UINT8* const m_buffer;
        const UINT8* m_position;
    };


    // NOTE: (Important) This utility class is intended for local use only (i.e. within one function).
    //       Do not keep instances around as member variables.
    class blob_stream_writer
    {
    public:
        DISABLE_COPY_AND_MOVE(blob_stream_writer);
        explicit blob_stream_writer(UINT8* buffer, size_t buffer_size)
            :m_buffer{ buffer }, m_position{ buffer }, m_buffer_size{ buffer_size }
        {
            assert(buffer && buffer_size);
        }

        // This template function is intended to write primitive types (e.g. int, float, bool)
        template<typename T>
        constexpr void write(T value)
        {
            static_assert(std::is_arithmetic_v<T>, "Template argument should be a primitive type.");
            assert(&m_position[sizeof(T)] <= &m_buffer[m_buffer_size]);
            *((T*)m_position) = value;
            m_position += sizeof(T);
        }

        // writes 'length' chars into 'buffer'.
        void write(const char* buffer, size_t length)
        {
            assert(&m_position[length] <= &m_buffer[m_buffer_size]);
            memcpy(m_position, buffer, length);
            m_position += length;
        }

        // writes 'length' bytes into 'buffer'.
        void write(const UINT8* buffer, size_t length)
        {
            assert(&m_position[length] <= &m_buffer[m_buffer_size]);
            memcpy(m_position, buffer, length);
            m_position += length;
        }

        constexpr void skip(size_t offset)
        {
            assert(&m_position[offset] <= &m_buffer[m_buffer_size]);
            m_position += offset;
        }

        [[nodiscard]] constexpr const UINT8* const buffer_start() const { return m_buffer; }
        [[nodiscard]] constexpr const UINT8* const buffer_end() const { return &m_buffer[m_buffer_size]; }
        [[nodiscard]] constexpr const UINT8* const position() const { return m_position; }
        [[nodiscard]] constexpr size_t offset() const { return m_position - m_buffer; }
    private:
        UINT8* const m_buffer;
        UINT8* m_position;
        size_t m_buffer_size;
    };
}