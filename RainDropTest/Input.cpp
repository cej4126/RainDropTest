#include "Input.h"
#include <unordered_map>

namespace input {

    namespace {

        struct input_binding
        {
            utl::vector<input_source> sources;
            input_value value{};
            bool is_dirty{ true };
        };

        std::unordered_map<UINT64, input_value> input_values;
        std::unordered_map<UINT64, input_binding> input_bindings;
        std::unordered_map<UINT64, UINT64> source_binding_map;
        utl::vector<detail::input_system_base*> input_callbacks;

        UINT8 modifier_keys_state{ 0 };

        constexpr UINT64 get_key(input_source::type type, UINT code)
        {
            return ((UINT64)type << 32) | (UINT64)code;
        }

        void set(input_source::type type, input_code::code code, XMFLOAT3 value)
        {
            assert(type < input_source::count);
            const UINT64 key{ get_key(type, code) };
            input_value& input{ input_values[key] };
            input.previous = input.current;
            input.current = value;

            if (source_binding_map.count(key))
            {
                const UINT64 binding_key{ source_binding_map[key] };
                assert(input_bindings.count(binding_key));
                input_binding& binding{ input_bindings[binding_key] };
                binding.is_dirty = true;

                input_value binding_value;
                get(binding_key, binding_value);

                // TODO: these callbacks could cause data-races in scripts when not run on the same thread as game scripts
                for (const auto& c : input_callbacks)
                {
                    c->on_event(binding_key, binding_value);
                }

            }

            // TODO: these callbacks could cause data-races in scripts when not run on the same thread as game scripts
            for (const auto& c : input_callbacks)
            {
                c->on_event(type, code, input);
            }
        }

        void set_modifier_input(UINT8 virtual_key, input_code::code code, modifier_flags::flags flags)
        {
            // If virtual key is down
            if (GetKeyState(virtual_key) < 0)
            {
                set(input_source::keyboard, code, { 1.f, 0.f, 0.f });
                modifier_keys_state |= flags;
            }
            else if (modifier_keys_state & flags)
            {
                set(input_source::keyboard, code, { 1.f, 0.f, 0.f });
                modifier_keys_state &= ~flags;
            }
        }

        void set_modifier_inputs(input_code::code code)
        {
            if (code == input_code::key_shift)
            {
                set_modifier_input(VK_LSHIFT, input_code::key_left_shift, modifier_flags::left_shift);
                set_modifier_input(VK_RSHIFT, input_code::key_right_shift, modifier_flags::right_shift);
            }
            else if (code == input_code::key_control)
            {
                set_modifier_input(VK_LCONTROL, input_code::key_left_control, modifier_flags::left_control);
                set_modifier_input(VK_RCONTROL, input_code::key_right_control, modifier_flags::right_control);
            }
            else if (code == input_code::key_alt)
            {
                set_modifier_input(VK_LMENU, input_code::key_left_alt, modifier_flags::left_alt);
                set_modifier_input(VK_RMENU, input_code::key_right_alt, modifier_flags::right_alt);
            }
        }

        XMFLOAT2 get_mouse_position(LPARAM l_param)
        {
            return { (float)((INT16)(l_param & 0x0000ffff)), (float)((INT16)(l_param >> 16)) };
        }

    } // anonymous namespace

    detail::input_system_base::input_system_base()
    {
        input_callbacks.emplace_back(this);
    }

    detail::input_system_base::~input_system_base()
    {
        for (UINT i{ 0 }; i < input_callbacks.size(); ++i)
        {
            if (input_callbacks[i] == this)
            {
                input_callbacks.erase_unordered(i);
                break;
            }
        }
    }

    HRESULT process_input_message(HWND h_wnd, UINT msg, WPARAM w_param, LPARAM l_param)
    {
        switch (msg)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            assert(w_param <= 0xff);
            const input_code::code code{ vk_mapping[w_param & 0xff] };
            if (code != Invalid_Index)
            {
                set(input_source::keyboard, code, { 1.f, 0.f, 0.f });
                set_modifier_inputs(code);
            }
        }
        break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            assert(w_param <= 0xff);
            const input_code::code code{ vk_mapping[w_param & 0xff] };
            if (code != Invalid_Index)
            {
                set(input_source::keyboard, code, { 0.f, 0.f, 0.f });
                set_modifier_inputs(code);
            }
        }
        break;
        case WM_MOUSEMOVE:
        {
            const XMFLOAT2 pos{ get_mouse_position(l_param) };
            set(input_source::mouse, input_code::mouse_position_x, { pos.x, 0.f, 0.f });
            set(input_source::mouse, input_code::mouse_position_y, { pos.y, 0.f, 0.f });
            set(input_source::mouse, input_code::mouse_position, { pos.x, pos.y, 0.f });
        }
        break;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        {
            SetCapture(h_wnd);
            const input_code::code code{ msg == WM_LBUTTONDOWN ? input_code::mouse_left : msg == WM_RBUTTONDOWN ? input_code::mouse_right : input_code::mouse_middle };
            const XMFLOAT2 pos{ get_mouse_position(l_param) };
            set(input_source::mouse, code, { pos.x, pos.y, 1.f });
        }
        break;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        {
            ReleaseCapture();
            const input_code::code code{ msg == WM_LBUTTONUP ? input_code::mouse_left : msg == WM_RBUTTONUP ? input_code::mouse_right : input_code::mouse_middle };
            const XMFLOAT2 pos{ get_mouse_position(l_param) };
            set(input_source::mouse, code, { pos.x, pos.y, 0.f });
        }
        break;
        case WM_MOUSEHWHEEL:
        {
            set(input_source::mouse, input_code::mouse_wheel, { (float)(GET_WHEEL_DELTA_WPARAM(w_param)), 0.f, 0.f });
        }
        break;
        }

        return S_OK;
    }

    void get(input_source::type type, input_code::code code, input_value& value)
    {
        assert(type < input_source::count);
        const UINT64 key{ get_key(type, code) };
        value = input_values[key];
    }

    void get(UINT64 binding, input_value& value)
    {
        if (!input_bindings.count(binding))
        {
            return;
        }

        input_binding& input_binding{ input_bindings[binding] };

        if (!input_binding.is_dirty)
        {
            value = input_binding.value;
            return;
        }

        utl::vector<input_source>& sources{ input_binding.sources };
        input_value sub_value{};
        input_value result{};

        for (const auto& source : sources)
        {
            assert(source.binding == binding);
            get(source.source_type, (input_code::code)source.code, sub_value);
            assert(source.axis <= axis::z);
            if (source.source_type == input_source::mouse)
            {
                const float current{ (&sub_value.current.x)[source.source_axis] };
                const float previous{ (&sub_value.previous.x)[source.source_axis] };
                (&result.current.x)[source.axis] += (current - previous) * source.multiplier;
            }
            else
            {
                (&result.previous.x)[source.axis] += (&sub_value.previous.x)[source.source_axis] * source.multiplier;
                (&result.current.x)[source.axis] += (&sub_value.current.x)[source.source_axis] * source.multiplier;
            }
        }

        input_binding.value = result;
        input_binding.is_dirty = false;
        value = result;
    }

    void bind(input_source source)
    {
        assert(source.source_type < input_source::count);
        const UINT64 key{ get_key(source.source_type, source.code) };
        unbind(source.source_type, (input_code::code)source.code);

        input_bindings[source.binding].sources.emplace_back(source);
        source_binding_map[key] = source.binding;
    }

    void unbind(input_source::type type, input_code::code code)
    {
        assert(type < input_source::count);
        const UINT64 key{ get_key(type, code) };
        if (!source_binding_map.count(key))
        {
            return;
        }

        const UINT64 binding_key{ source_binding_map[key] };
        assert(input_bindings.count(binding_key));
        input_binding& binding{ input_bindings[binding_key] };
        utl::vector<input_source>& sources{ binding.sources };
        for (UINT i{ 0 }; i < sources.size(); ++i)
        {
            if (sources[i].source_type == type && sources[i].code == code)
            {
                assert(sources[i].binding == source_binding_map[key]);
                sources.erase_unordered(i);
                source_binding_map.erase(key);
                break;
            }
        }

        if (!sources.size())
        {
            assert(!source_binding_map.count(key));
            input_bindings.erase(binding_key);
        }
    }

    void
        unbind(UINT64 binding)
    {
        if (!input_bindings.count(binding))
        {
            return;
        }

        utl::vector<input_source>& sources{ input_bindings[binding].sources };
        for (const auto& source : sources)
        {
            assert(source.binding == binding);
            const UINT64 key{ get_key(source.source_type, source.code) };
            assert(source_binding_map.count(key) && source_binding_map[key] == binding);
            source_binding_map.erase(key);
        }

        input_bindings.erase(binding);
    }
}