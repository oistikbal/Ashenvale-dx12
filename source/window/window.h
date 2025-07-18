#pragma once

#include "common.h"

namespace ash::window
{
inline HWND g_hwnd = nullptr;
}

namespace ash::window
{
void init(HINSTANCE hInstance, PWSTR pCmdLine, int nCmdShow);
void run();
} // namespace ash::window