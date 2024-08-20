#pragma once
#include "stdafx.h"
#include "Vector.h"

namespace input {

    struct axis {
        enum type : UINT {
            x = 0,
            y = 1,
            z = 2,
        };
    };

    struct modifier_key {
        enum key : UINT {
            none = 0x00,
            left_shift = 0x01,
            right_shift = 0x02,
            shift = left_shift | right_shift,
            left_ctrl = 0x04,
            right_ctrl = 0x08,
            ctrl = left_ctrl | right_ctrl,
            left_alt = 0x10,
            right_alt = 0x20,
            alt = left_alt | right_alt,
        };
    };


    struct modifier_flags {
        enum flags : UINT8 {
            left_shift = 0x10,
            left_control = 0x20,
            left_alt = 0x40,

            right_shift = 0x01,
            right_control = 0x02,
            right_alt = 0x04,
        };
    };

    struct input_value
    {
        XMFLOAT3 previous;
        XMFLOAT3 current;
    };

    struct input_code {
        enum code : UINT {
            mouse_position,
            mouse_position_x,
            mouse_position_y,
            mouse_left,
            mouse_right,
            mouse_middle,
            mouse_wheel,

            key_backspace,
            key_tab,
            key_return,
            key_shift,
            key_left_shift,
            key_right_shift,
            key_control,
            key_left_control,
            key_right_control,
            key_alt,
            key_left_alt,
            key_right_alt,
            key_pause,
            key_capslock,
            key_escape,
            key_space,
            key_page_up,
            key_page_down,
            key_home,
            key_end,
            key_left,
            key_up,
            key_right,
            key_down,
            key_print_screen,
            key_insert,
            key_delete,

            key_0,
            key_1,
            key_2,
            key_3,
            key_4,
            key_5,
            key_6,
            key_7,
            key_8,
            key_9,

            key_a,
            key_b,
            key_c,
            key_d,
            key_e,
            key_f,
            key_g,
            key_h,
            key_i,
            key_j,
            key_k,
            key_l,
            key_m,
            key_n,
            key_o,
            key_p,
            key_q,
            key_r,
            key_s,
            key_t,
            key_u,
            key_v,
            key_w,
            key_x,
            key_y,
            key_z,

            key_numpad_0,
            key_numpad_1,
            key_numpad_2,
            key_numpad_3,
            key_numpad_4,
            key_numpad_5,
            key_numpad_6,
            key_numpad_7,
            key_numpad_8,
            key_numpad_9,

            key_multiply,
            key_add,
            key_subtract,
            key_decimal,
            key_divide,

            key_f1,
            key_f2,
            key_f3,
            key_f4,
            key_f5,
            key_f6,
            key_f7,
            key_f8,
            key_f9,
            key_f10,
            key_f11,
            key_f12,

            key_numlock,
            key_scrollock,
            key_colon,
            key_plus,
            key_comma,
            key_minus,
            key_period,
            key_question,
            key_bracket_open,
            key_pipe,
            key_bracket_close,
            key_quote,
            key_tilde,
        };
    };

    struct input_source {
        enum type : UINT {
            keyboard,
            mouse,
            controller,
            raw,

            count
        };

        UINT64 binding{ 0 };
        type source_type{};
        UINT code{ 0 };
        float multiplier{ 0 };
        bool is_discrete{ true };
        axis::type source_axis{};
        axis::type axis{};
        modifier_key::key modifier{};
    };

    constexpr UINT vk_mapping[256]{
        /* 0x00 */ Invalid_Index,
        /* 0x01 */ input_code::mouse_left,
        /* 0x02 */ input_code::mouse_right,
        /* 0x03 */ Invalid_Index,
        /* 0x04 */ input_code::mouse_middle,
        /* 0x05 */ Invalid_Index,
        /* 0x06 */ Invalid_Index,
        /* 0x07 */ Invalid_Index,
        /* 0x08 */ input_code::key_backspace,
        /* 0x09 */ input_code::key_tab,
        /* 0x0A */ Invalid_Index,
        /* 0x0B */ Invalid_Index,
        /* 0x0C */ Invalid_Index,
        /* 0x0D */ input_code::key_return,
        /* 0x0E */ Invalid_Index,
        /* 0x0F */ Invalid_Index,

        /* 0x10 */ input_code::key_shift,
        /* 0x11 */ input_code::key_control,
        /* 0x12 */ input_code::key_alt,
        /* 0x13 */ input_code::key_pause,
        /* 0x14 */ input_code::key_capslock,
        /* 0x15 */ Invalid_Index,
        /* 0x16 */ Invalid_Index,
        /* 0x17 */ Invalid_Index,
        /* 0x18 */ Invalid_Index,
        /* 0x19 */ Invalid_Index,
        /* 0x1A */ Invalid_Index,
        /* 0x1B */ input_code::key_escape,
        /* 0x1C */ Invalid_Index,
        /* 0x1D */ Invalid_Index,
        /* 0x1E */ Invalid_Index,
        /* 0x1F */ Invalid_Index,

        /* 0x20 */ input_code::key_space,
        /* 0x21 */ input_code::key_page_up,
        /* 0x22 */ input_code::key_page_down,
        /* 0x23 */ input_code::key_end,
        /* 0x24 */ input_code::key_home,
        /* 0x25 */ input_code::key_left,
        /* 0x26 */ input_code::key_up,
        /* 0x27 */ input_code::key_right,
        /* 0x28 */ input_code::key_down,
        /* 0x29 */ Invalid_Index,
        /* 0x2A */ Invalid_Index,
        /* 0x2B */ Invalid_Index,
        /* 0x2C */ input_code::key_print_screen,
        /* 0x2D */ input_code::key_insert,
        /* 0x2E */ input_code::key_delete,
        /* 0x2F */ Invalid_Index,

        /* 0x30 */ input_code::key_0,
        /* 0x31 */ input_code::key_1,
        /* 0x32 */ input_code::key_2,
        /* 0x33 */ input_code::key_3,
        /* 0x34 */ input_code::key_4,
        /* 0x35 */ input_code::key_5,
        /* 0x36 */ input_code::key_6,
        /* 0x37 */ input_code::key_7,
        /* 0x38 */ input_code::key_8,
        /* 0x39 */ input_code::key_9,
        /* 0x3A */ Invalid_Index,
        /* 0x3B */ Invalid_Index,
        /* 0x3C */ Invalid_Index,
        /* 0x3D */ Invalid_Index,
        /* 0x3E */ Invalid_Index,
        /* 0x3F */ Invalid_Index,

        /* 0x40 */ Invalid_Index,
        /* 0x41 */ input_code::key_a,
        /* 0x42 */ input_code::key_b,
        /* 0x43 */ input_code::key_c,
        /* 0x44 */ input_code::key_d,
        /* 0x45 */ input_code::key_e,
        /* 0x46 */ input_code::key_f,
        /* 0x47 */ input_code::key_g,
        /* 0x48 */ input_code::key_h,
        /* 0x49 */ input_code::key_i,
        /* 0x4A */ input_code::key_j,
        /* 0x4B */ input_code::key_k,
        /* 0x4C */ input_code::key_l,
        /* 0x4D */ input_code::key_m,
        /* 0x4E */ input_code::key_n,
        /* 0x4F */ input_code::key_o,

        /* 0x50 */ input_code::key_p,
        /* 0x51 */ input_code::key_q,
        /* 0x52 */ input_code::key_r,
        /* 0x53 */ input_code::key_s,
        /* 0x54 */ input_code::key_t,
        /* 0x55 */ input_code::key_u,
        /* 0x56 */ input_code::key_v,
        /* 0x57 */ input_code::key_w,
        /* 0x58 */ input_code::key_x,
        /* 0x59 */ input_code::key_y,
        /* 0x5A */ input_code::key_z,
        /* 0x5B */ Invalid_Index,
        /* 0x5C */ Invalid_Index,
        /* 0x5D */ Invalid_Index,
        /* 0x5E */ Invalid_Index,
        /* 0x5F */ Invalid_Index,

        /* 0x60 */ input_code::key_numpad_0,
        /* 0x61 */ input_code::key_numpad_1,
        /* 0x62 */ input_code::key_numpad_2,
        /* 0x63 */ input_code::key_numpad_3,
        /* 0x64 */ input_code::key_numpad_4,
        /* 0x65 */ input_code::key_numpad_5,
        /* 0x66 */ input_code::key_numpad_6,
        /* 0x67 */ input_code::key_numpad_7,
        /* 0x68 */ input_code::key_numpad_8,
        /* 0x69 */ input_code::key_numpad_9,
        /* 0x6A */ input_code::key_multiply,
        /* 0x6B */ input_code::key_add,
        /* 0x6C */ Invalid_Index,
        /* 0x6D */ input_code::key_subtract,
        /* 0x6E */ input_code::key_decimal,
        /* 0x6F */ input_code::key_divide,

        /* 0x70 */ input_code::key_f1,
        /* 0x71 */ input_code::key_f2,
        /* 0x72 */ input_code::key_f3,
        /* 0x73 */ input_code::key_f4,
        /* 0x74 */ input_code::key_f5,
        /* 0x75 */ input_code::key_f6,
        /* 0x76 */ input_code::key_f7,
        /* 0x77 */ input_code::key_f8,
        /* 0x78 */ input_code::key_f9,
        /* 0x79 */ input_code::key_f10,
        /* 0x7A */ input_code::key_f11,
        /* 0x7B */ input_code::key_f12,
        /* 0x7C */ Invalid_Index,
        /* 0x7D */ Invalid_Index,
        /* 0x7E */ Invalid_Index,
        /* 0x7F */ Invalid_Index,

        /* 0x80 */ Invalid_Index,
        /* 0x81 */ Invalid_Index,
        /* 0x82 */ Invalid_Index,
        /* 0x83 */ Invalid_Index,
        /* 0x84 */ Invalid_Index,
        /* 0x85 */ Invalid_Index,
        /* 0x86 */ Invalid_Index,
        /* 0x87 */ Invalid_Index,
        /* 0x88 */ Invalid_Index,
        /* 0x89 */ Invalid_Index,
        /* 0x8A */ Invalid_Index,
        /* 0x8B */ Invalid_Index,
        /* 0x8C */ Invalid_Index,
        /* 0x8D */ Invalid_Index,
        /* 0x8E */ Invalid_Index,
        /* 0x8F */ Invalid_Index,

        /* 0x90 */ input_code::key_numlock,
        /* 0x91 */ input_code::key_scrollock,
        /* 0x92 */ Invalid_Index,
        /* 0x93 */ Invalid_Index,
        /* 0x94 */ Invalid_Index,
        /* 0x95 */ Invalid_Index,
        /* 0x96 */ Invalid_Index,
        /* 0x97 */ Invalid_Index,
        /* 0x98 */ Invalid_Index,
        /* 0x99 */ Invalid_Index,
        /* 0x9A */ Invalid_Index,
        /* 0x9B */ Invalid_Index,
        /* 0x9C */ Invalid_Index,
        /* 0x9D */ Invalid_Index,
        /* 0x9E */ Invalid_Index,
        /* 0x9F */ Invalid_Index,

        /* 0xA0 */ Invalid_Index,
        /* 0xA1 */ Invalid_Index,
        /* 0xA2 */ Invalid_Index,
        /* 0xA3 */ Invalid_Index,
        /* 0xA4 */ Invalid_Index,
        /* 0xA5 */ Invalid_Index,
        /* 0xA6 */ Invalid_Index,
        /* 0xA7 */ Invalid_Index,
        /* 0xA8 */ Invalid_Index,
        /* 0xA9 */ Invalid_Index,
        /* 0xAA */ Invalid_Index,
        /* 0xAB */ Invalid_Index,
        /* 0xAC */ Invalid_Index,
        /* 0xAD */ Invalid_Index,
        /* 0xAE */ Invalid_Index,
        /* 0xAF */ Invalid_Index,

        /* 0xB0 */ Invalid_Index,
        /* 0xB1 */ Invalid_Index,
        /* 0xB2 */ Invalid_Index,
        /* 0xB3 */ Invalid_Index,
        /* 0xB4 */ Invalid_Index,
        /* 0xB5 */ Invalid_Index,
        /* 0xB6 */ Invalid_Index,
        /* 0xB7 */ Invalid_Index,
        /* 0xB8 */ Invalid_Index,
        /* 0xB9 */ Invalid_Index,
        /* 0xBA */ input_code::key_colon,
        /* 0xBB */ input_code::key_plus,
        /* 0xBC */ input_code::key_comma,
        /* 0xBD */ input_code::key_minus,
        /* 0xBE */ input_code::key_period,
        /* 0xBF */ input_code::key_question,

        /* 0xC0 */ input_code::key_tilde,
        /* 0xC1 */ Invalid_Index,
        /* 0xC2 */ Invalid_Index,
        /* 0xC3 */ Invalid_Index,
        /* 0xC4 */ Invalid_Index,
        /* 0xC5 */ Invalid_Index,
        /* 0xC6 */ Invalid_Index,
        /* 0xC7 */ Invalid_Index,
        /* 0xC8 */ Invalid_Index,
        /* 0xC9 */ Invalid_Index,
        /* 0xCA */ Invalid_Index,
        /* 0xCB */ Invalid_Index,
        /* 0xCC */ Invalid_Index,
        /* 0xCD */ Invalid_Index,
        /* 0xCE */ Invalid_Index,
        /* 0xCF */ Invalid_Index,

        /* 0xD0 */ Invalid_Index,
        /* 0xD1 */ Invalid_Index,
        /* 0xD2 */ Invalid_Index,
        /* 0xD3 */ Invalid_Index,
        /* 0xD4 */ Invalid_Index,
        /* 0xD5 */ Invalid_Index,
        /* 0xD6 */ Invalid_Index,
        /* 0xD7 */ Invalid_Index,
        /* 0xD8 */ Invalid_Index,
        /* 0xD9 */ Invalid_Index,
        /* 0xDA */ Invalid_Index,
        /* 0xDB */ input_code::key_bracket_open,
        /* 0xDC */ input_code::key_pipe,
        /* 0xDD */ input_code::key_bracket_close,
        /* 0xDE */ input_code::key_quote,
        /* 0xDF */ Invalid_Index,

        /* 0xE0 */ Invalid_Index,
        /* 0xE1 */ Invalid_Index,
        /* 0xE2 */ Invalid_Index,
        /* 0xE3 */ Invalid_Index,
        /* 0xE4 */ Invalid_Index,
        /* 0xE5 */ Invalid_Index,
        /* 0xE6 */ Invalid_Index,
        /* 0xE7 */ Invalid_Index,
        /* 0xE8 */ Invalid_Index,
        /* 0xE9 */ Invalid_Index,
        /* 0xEA */ Invalid_Index,
        /* 0xEB */ Invalid_Index,
        /* 0xEC */ Invalid_Index,
        /* 0xED */ Invalid_Index,
        /* 0xEE */ Invalid_Index,
        /* 0xEF */ Invalid_Index,

        /* 0xF0 */ Invalid_Index,
        /* 0xF1 */ Invalid_Index,
        /* 0xF2 */ Invalid_Index,
        /* 0xF3 */ Invalid_Index,
        /* 0xF4 */ Invalid_Index,
        /* 0xF5 */ Invalid_Index,
        /* 0xF6 */ Invalid_Index,
        /* 0xF7 */ Invalid_Index,
        /* 0xF8 */ Invalid_Index,
        /* 0xF9 */ Invalid_Index,
        /* 0xFA */ Invalid_Index,
        /* 0xFB */ Invalid_Index,
        /* 0xFC */ Invalid_Index,
        /* 0xFD */ Invalid_Index,
        /* 0xFE */ Invalid_Index,
        /* 0xFF */ Invalid_Index,
    };

    namespace detail {
        class input_system_base
        {
        public:
            input_system_base();
            ~input_system_base();
            virtual void on_event(input_source::type, input_code::code, const input_value&) = 0;
            virtual void on_event(UINT64 binding, const input_value&) = 0;
        private:
            //~input_system_base();
        };
    } // namespace detail

    template<typename T>
    class input_system final : public detail::input_system_base
    {
    public:
        ~input_system() {};
        using input_callback_t = void(T::*)(input_source::type, input_code::code, const input_value&);
        using binding_callback_t = void(T::*)(UINT64, const input_value&);

        void add_handler(input_source::type type, T* instance, input_callback_t callback)
        {
            assert(instance && callback && type < input_source::count);
            auto& collection = m_input_callbacks[type];
            for (const auto& func : collection)
            {
                // If handler was already added then don't add it again.
                if (func.instance == instance && func.callback == callback) return;
            }

            collection.emplace_back(input_callback{ instance, callback });
        }

        void add_handler(UINT64 binding, T* instance, binding_callback_t callback)
        {
            assert(instance && callback);
            for (const auto& func : m_binding_callbacks)
            {
                // If handler was already added then don't add it again.
                if (func.binding == binding && func.instance == instance && func.callback == callback) return;
            }

            m_binding_callbacks.emplace_back(binding_callback{ binding, instance, callback });
        }

        void on_event(input_source::type type, input_code::code code, const input_value& value) override
        {
            assert(type < input_source::count);
            if (type < input_source::keyboard || type >= input_source::count) return;
            
            for (const auto& item : m_input_callbacks[type])
            {
                (item.instance->*item.callback)(type, code, value);
            }
        }

        void on_event(UINT64 binding, const input_value& value) override
        {
            for (const auto& item : m_binding_callbacks)
            {
                if (item.binding == binding)
                {
                    (item.instance->*item.callback)(binding, value);
                }
            }
        }

    private:
        struct input_callback
        {
            T* instance;
            input_callback_t callback;
        };

        struct binding_callback
        {
            UINT64 binding;
            T* instance;
            binding_callback_t callback;
        };

        utl::vector<input_callback> m_input_callbacks[input_source::count];
        utl::vector<binding_callback> m_binding_callbacks;
    };

    HRESULT process_input_message(HWND h_wnd, UINT msg, WPARAM w_param, LPARAM l_param);
    void get(input_source::type type, input_code::code code, input_value& value);
    void get(UINT64 binding, input_value& value);
    void bind(input_source source);
    void unbind(input_source::type type, input_code::code code);
    void unbind(UINT64 binding);

}
