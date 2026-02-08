#pragma once

#include "common.h"
#include <thread>

using namespace winrt;

namespace ash::renderer
{
inline std::thread g_renderer_thread;
inline std::atomic<bool> g_running = true;

inline winrt::com_ptr<ID3D12DescriptorHeap> g_cbv_srv_uav_heap;
inline winrt::com_ptr<ID3D12DescriptorHeap> g_sampler_heap;
inline winrt::com_ptr<ID3D12DescriptorHeap> g_viewport_rtv_heap;
inline winrt::com_ptr<ID3D12DescriptorHeap> g_rtv_heap;
inline winrt::com_ptr<ID3D12Resource> g_viewport_texture;

inline D3D12_VIEWPORT g_viewport;
inline DXGI_FORMAT g_format;
} // namespace ash::renderer

namespace ash::renderer
{
void init();
void render();
void stop();
void resize(D3D12_VIEWPORT viewport);
} // namespace ash::renderer