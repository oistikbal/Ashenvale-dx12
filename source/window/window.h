#pragma once

#include "common.h"
#include "event.h"

namespace ash::window
{
inline HWND g_hwnd = nullptr;
inline event::event_queue g_queue;
} // namespace ash::window

namespace ash::window
{
void init(HINSTANCE hInstance, PWSTR pCmdLine, int nCmdShow);
void run();
} // namespace ash::window