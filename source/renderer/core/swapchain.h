#pragma once

#include "common.h"
#include <D3D12MemAlloc.h>

namespace ash
{
inline winrt::com_ptr<IDXGISwapChain4> rhi_sw_g_swapchain = nullptr;
inline winrt::com_ptr<ID3D12DescriptorHeap> rhi_sw_g_swapchain_rtv_heap = nullptr;
inline winrt::com_ptr<ID3D12Resource> rhi_sw_g_render_targets[2];
inline winrt::com_ptr<ID3D12Fence> rhi_sw_g_fence;

inline uint8_t rhi_sw_g_current_backbuffer = 0;
inline uint64_t rhi_sw_g_fence_value = 0;
inline HANDLE rhi_sw_g_fence_event = 0;
inline D3D12_VIEWPORT rhi_sw_g_viewport;
inline DXGI_FORMAT rhi_sw_g_format;

} // namespace ash

namespace ash
{
void rhi_sw_init();
void rhi_sw_resize();
} // namespace ash
