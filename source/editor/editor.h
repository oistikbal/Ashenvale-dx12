#pragma once

#include "common.h"
#include <winrt/base.h>

namespace ash::editor
{
inline winrt::com_ptr<ID3D12DescriptorHeap> g_imgui_heap;
}

namespace ash::editor
{
void init();
void render();
void render_backend();
} // namespace ash::editor