#include "Lights.h"
#include <unordered_map>

namespace lights
{
    namespace {

        void create_light(XMFLOAT3 position)
        {}

        std::unordered_map<UINT64, light_set> light_set_keys;

        struct light_set_states {
            enum state {
                left_set,
                right_set
            };
        };

    } // anonymous namespace


    void create_light_set(UINT64 set_key)
    {
        // key not used check
        assert(!light_set_keys.count(set_key));
        light_set_keys[set_key] = {};
    }

    void generate_lights()
    {
        create_light_set(light_set_states::left_set);
        create_light_set(light_set_states::right_set);

        light_init_info info{};

    }
}