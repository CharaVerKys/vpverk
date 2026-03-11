#pragma once


static_assert(false, "no client code");


#include <string_view>
#include <cstdint>
enum ContextTargets : int16_t{ // ! in order of moveToContext
    InvalidContext = -1,
    MainCtx,
    SettingsCtx,
    StatisticsCtx
};
#define NUM_OF_CONTEXTS 3
#define ContStore ContextsStore<3>

//****************************************
using namespace std::string_view_literals;
namespace method{
    //* signature promise<bool>(void), always set promise to true in order to invoke on main context
    static inline constexpr std::string_view move_to_main_context = "move_to_main_context"sv;
    //* signature void(ConnectionStats)
    static inline constexpr std::string_view write_session_stats = "write_session_data"sv;
    //* signature promise<SettingsSnapshot>(void)
    static inline constexpr std::string_view get_settings = "get_settings"sv;
}
