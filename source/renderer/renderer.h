#pragma once

#include "common.h"
#include <thread>

using namespace winrt;

namespace ash
{
inline std::thread rhi_g_renderer_thread;
inline std::atomic<bool> rhi_g_running = true;

inline winrt::com_ptr<ID3D12DescriptorHeap> rhi_g_cbv_srv_uav_heap;
inline winrt::com_ptr<ID3D12DescriptorHeap> rhi_g_sampler_heap;
inline winrt::com_ptr<ID3D12DescriptorHeap> rhi_g_viewport_rtv_heap;
inline winrt::com_ptr<ID3D12DescriptorHeap> rhi_g_rtv_heap;
inline winrt::com_ptr<ID3D12Resource> rhi_g_viewport_texture;

inline winrt::com_ptr<ID3D12Device> rhi_g_device;
inline winrt::com_ptr<IDXGIAdapter> rhi_g_adapter;
inline winrt::com_ptr<IDXGIOutput> rhi_g_output;

inline D3D12_VIEWPORT rhi_g_viewport;
inline DXGI_FORMAT rhi_g_format;
} // namespace ash

namespace ash
{
void rhi_init();
void rhi_render();
void rhi_stop();
void rhi_resize(D3D12_VIEWPORT viewport);
} // namespace ash
