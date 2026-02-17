#pragma once

namespace ash
{
inline bool ed_vp_g_is_open = true;
inline bool ed_vp_g_is_focused = false;
}

namespace ash
{
void ed_vp_init();
void ed_vp_render();
} // namespace ash
