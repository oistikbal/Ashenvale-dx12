#pragma once

#include "common.h"

namespace ash::renderer::core::command_queue
{
inline winrt::com_ptr<ID3D12CommandQueue> g_direct = nullptr;
inline winrt::com_ptr<ID3D12CommandQueue> g_compute = nullptr;
inline winrt::com_ptr<ID3D12CommandQueue> g_copy = nullptr;

inline winrt::com_ptr<ID3D12CommandAllocator> g_command_allocator;
inline winrt::com_ptr<ID3D12GraphicsCommandList> g_command_list;
} // namespace ash::renderer::core::command_queue

namespace ash::renderer::core::command_queue
{
void init();
} // namespace ash::renderer::core::swapchain