#pragma once

#include "common.h"
#include <thread>

namespace ash::renderer
{
inline std::thread g_renderer_thread;
inline std::atomic<bool> g_running = true;
}

namespace ash::renderer
{
void init();
void render();
void stop();
}