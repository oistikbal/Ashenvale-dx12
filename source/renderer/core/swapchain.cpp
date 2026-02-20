#include "swapchain.h"
#include "editor/console.h"
#include "renderer/core/command_queue.h"
#include "window/window.h"
#include <renderer/renderer.h>

using namespace winrt;

void ash::rhi_sw_init()
{
    SCOPED_CPU_EVENT(L"ash::rhi_sw_init")
    ed_console_log(ed_console_log_level::info, "[Swapchain] Initialization begin.");
    assert(rhi_cmd_g_direct.get());

    com_ptr<IDXGIOutput6> adapterOutput6;
    ash::rhi_g_output.as(adapterOutput6);
    assert(adapterOutput6.get());

    DXGI_MODE_DESC1 targetMode = {};
    targetMode.Format = DXGI_FORMAT_R10G10B10A2_UNORM;

    DXGI_MODE_DESC1 closeMatch = {};

    adapterOutput6->FindClosestMatchingMode1(&targetMode, &closeMatch, nullptr);

    rhi_sw_g_format = closeMatch.Format;

    RECT clientRect;
    GetClientRect(win_g_hwnd, &clientRect);

    int renderWidth = clientRect.right - clientRect.left;
    int renderHeight = clientRect.bottom - clientRect.top;
    ed_console_log(ed_console_log_level::info, "[Swapchain] Creating swapchain.");

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = renderWidth;
    swapChainDesc.Height = renderHeight;
    swapChainDesc.Format = rhi_sw_g_format;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = 0;

    com_ptr<IDXGIFactory7> factory;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

    com_ptr<IDXGISwapChain1> temp_swapchain;
    factory->CreateSwapChainForHwnd(rhi_cmd_g_direct.get(), win_g_hwnd, &swapChainDesc, nullptr, nullptr,
                                    temp_swapchain.put());

    factory->MakeWindowAssociation(win_g_hwnd, DXGI_MWA_NO_ALT_ENTER);

    temp_swapchain.as(rhi_sw_g_swapchain);

    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.NumDescriptors = 2;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rhi_g_device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(rhi_sw_g_swapchain_rtv_heap.put()));
    SET_OBJECT_NAME(rhi_sw_g_swapchain_rtv_heap, L"Rtv Heap")

    rhi_sw_resize();
    rhi_sw_g_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    rhi_g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(rhi_sw_g_fence.put()));
    SET_OBJECT_NAME(rhi_sw_g_fence.get(), L"Swapchain Fence")
    ed_console_log(ed_console_log_level::info, "[Swapchain] Initialization complete.");
}

void ash::rhi_sw_resize()
{
    SCOPED_CPU_EVENT(L"ash::rhi_sw_resize")
    assert(rhi_sw_g_swapchain.get());

    RECT clientRect;
    GetClientRect(win_g_hwnd, &clientRect);

    int renderWidth = clientRect.right - clientRect.left;
    int renderHeight = clientRect.bottom - clientRect.top;

    if (renderWidth == 0 || renderHeight == 0)
    {
        ed_console_log(ed_console_log_level::warning, "[Swapchain] Resize skipped (minimized).");
        return;
    }

    for (UINT i = 0; i < 2; ++i)
    {
        rhi_sw_g_render_targets[i] = nullptr;
    }

    const HRESULT resize_hr = rhi_sw_g_swapchain->ResizeBuffers(2, renderWidth, renderHeight, rhi_sw_g_format, 0);
    if (FAILED(resize_hr))
    {
        ed_console_log(ed_console_log_level::error, "[Swapchain] ResizeBuffers failed.");
        return;
    }
    ed_console_log(ed_console_log_level::info, "[Swapchain] Resized successfully.");

    rhi_sw_g_viewport = {};
    rhi_sw_g_viewport.Width = static_cast<float>(renderWidth);
    rhi_sw_g_viewport.Height = static_cast<float>(renderHeight);
    rhi_sw_g_viewport.MinDepth = 0.0f;
    rhi_sw_g_viewport.MaxDepth = 1.0f;
    rhi_sw_g_viewport.TopLeftX = 0.0f;
    rhi_sw_g_viewport.TopLeftY = 0.0f;

    UINT backBufferIndex = rhi_sw_g_swapchain->GetCurrentBackBufferIndex();

    UINT rtvDescriptorSize = rhi_g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rhi_sw_g_swapchain_rtv_heap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < 2; ++i)
    {
        rhi_sw_g_swapchain->GetBuffer(i, IID_PPV_ARGS(rhi_sw_g_render_targets[i].put()));
        rhi_g_device->CreateRenderTargetView(rhi_sw_g_render_targets[i].get(), nullptr, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize;
    }

}
