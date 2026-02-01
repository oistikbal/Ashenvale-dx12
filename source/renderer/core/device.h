#pragma once

#include "common.h"

namespace ash::renderer::core
{
inline winrt::com_ptr<ID3D12Device> g_device;
inline winrt::com_ptr<IDXGIAdapter> g_adapter;
}