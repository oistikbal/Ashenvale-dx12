#pragma once

#include "common.h"
#include "event.h"

namespace ash
{
inline HWND win_g_hwnd = nullptr;
inline win_evt_event_queue win_g_queue;
} // namespace ash

namespace ash
{
void win_init(HINSTANCE hInstance, PWSTR pCmdLine, int nCmdShow);
void win_run();
} // namespace ash
