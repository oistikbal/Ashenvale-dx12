#pragma once

#include "common.h"

namespace ash::renderer::core::swapchain
{
inline winrt::com_ptr<IDXGISwapChain4> g_swapchain = nullptr;
inline winrt::com_ptr<ID3D12DescriptorHeap> g_swapchain_rtv_heap = nullptr;
inline winrt::com_ptr<ID3D12DescriptorHeap> g_swapchain_dsv_heap = nullptr;
inline winrt::com_ptr<ID3D12Resource> g_render_targets[2];
inline winrt::com_ptr<ID3D12Resource> g_dsv_buffer;
inline winrt::com_ptr<ID3D12Fence> g_fence;

inline uint8_t g_current_backbuffer = 0;
inline uint64_t g_fence_value = 0;
inline HANDLE g_fence_event = 0;
inline D3D12_VIEWPORT g_viewport;
inline DXGI_FORMAT g_format;

} // namespace ash::renderer::core::swapchain

namespace ash::renderer::core::swapchain
{
void init(DXGI_FORMAT format);
void resize();
} // namespace ash::renderer::core::swapchain