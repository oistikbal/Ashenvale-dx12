#pragma once

#include "common.h"

namespace ash
{
inline winrt::com_ptr<ID3D12CommandQueue> rhi_cmd_g_direct = nullptr;
inline winrt::com_ptr<ID3D12CommandQueue> rhi_cmd_g_compute = nullptr;
inline winrt::com_ptr<ID3D12CommandQueue> rhi_cmd_g_copy = nullptr;

inline winrt::com_ptr<ID3D12CommandAllocator> rhi_cmd_g_command_allocator;
inline winrt::com_ptr<ID3D12GraphicsCommandList> rhi_cmd_g_command_list;
} // namespace ash

namespace ash
{
void rhi_cmd_init();
} // namespace ash
