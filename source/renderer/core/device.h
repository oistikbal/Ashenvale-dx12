#pragma once

#include "common.h"

namespace ash
{
inline winrt::com_ptr<ID3D12Device> rhi_dev_g_device;
inline winrt::com_ptr<IDXGIAdapter> rhi_dev_g_adapter;
} // namespace ash
