#pragma once

#include <string_view>

namespace ash
{
enum class ed_console_log_level : uint8_t
{
    info,
    warning,
    error
};

inline bool ed_console_g_is_open = true;

void ed_console_init();
void ed_console_render();
void ed_console_log(ed_console_log_level level, std::string_view message);
void ed_console_clear();
} // namespace ash

