#include "swapchain.h"
#include "renderer/core/command_queue.h"
#include "renderer/core/device.h"
#include "window/window.h"

using namespace winrt;

void ash::renderer::core::swapchain::init(DXGI_FORMAT format)
{
    SCOPED_CPU_EVENT(L"ash::renderer::core::swapchain::init")
    assert(core::command_queue::g_direct.get());
    g_format = format;

    RECT clientRect;
    GetClientRect(window::g_hwnd, &clientRect);

    int renderWidth = clientRect.right - clientRect.left;
    int renderHeight = clientRect.bottom - clientRect.top;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = renderWidth;
    swapChainDesc.Height = renderHeight;
    swapChainDesc.Format = g_format;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = 0;

    com_ptr<IDXGIFactory7> factory;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

    com_ptr<IDXGISwapChain1> temp_swapchain;
    factory->CreateSwapChainForHwnd(core::command_queue::g_direct.get(), window::g_hwnd, &swapChainDesc, nullptr,
                                    nullptr, temp_swapchain.put());

    factory->MakeWindowAssociation(window::g_hwnd, DXGI_MWA_NO_ALT_ENTER);

    temp_swapchain.as(g_swapchain);

    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.NumDescriptors = 2;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    core::g_device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(g_rtv_heap.put()));
    SET_OBJECT_NAME(g_rtv_heap, L"Rtv Heap")


    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsv_heap_desc.NumDescriptors = 1;
    dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    core::g_device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(g_dsv_heap.put()));
    SET_OBJECT_NAME(g_dsv_heap, L"Dsv Heap")


    resize();
    g_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    core::g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(g_fence.put()));
    SET_OBJECT_NAME(g_fence.get(), L"Swapchain Fence")
}

void ash::renderer::core::swapchain::resize()
{
    SCOPED_CPU_EVENT(L"ash::renderer::core::swapchain::resize")
    assert(g_swapchain.get());

    RECT clientRect;
    GetClientRect(window::g_hwnd, &clientRect);

    int renderWidth = clientRect.right - clientRect.left;
    int renderHeight = clientRect.bottom - clientRect.top;

    if (renderWidth == 0 || renderHeight == 0)
        return;

    for (UINT i = 0; i < 2; ++i)
    {
        g_render_targets[i] = nullptr;
    }

    g_swapchain->ResizeBuffers(2, renderWidth, renderHeight, g_format, 0);

    g_viewport = {};
    g_viewport.Width = static_cast<float>(renderWidth);
    g_viewport.Height = static_cast<float>(renderHeight);
    g_viewport.MinDepth = 0.0f;
    g_viewport.MaxDepth = 1.0f;
    g_viewport.TopLeftX = 0.0f;
    g_viewport.TopLeftY = 0.0f;

    UINT backBufferIndex = g_swapchain->GetCurrentBackBufferIndex();

    UINT rtvDescriptorSize = core::g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtv_heap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < 2; ++i)
    {
        g_swapchain->GetBuffer(i, IID_PPV_ARGS(g_render_targets[i].put()));
        core::g_device->CreateRenderTargetView(g_render_targets[i].get(), nullptr, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize;
    }

    D3D12_CLEAR_VALUE depth_optimized_clear_value = {};
    depth_optimized_clear_value.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_optimized_clear_value.DepthStencil.Depth = 1.0f;
    depth_optimized_clear_value.DepthStencil.Stencil = 0;

    D3D12_RESOURCE_DESC depth_desc = {};
    depth_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depth_desc.Alignment = 0;
    depth_desc.Width = renderWidth;
    depth_desc.Height = renderHeight;
    depth_desc.DepthOrArraySize = 1;
    depth_desc.MipLevels = 1;
    depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_desc.SampleDesc.Count = 1;
    depth_desc.SampleDesc.Quality = 0;
    depth_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

    core::g_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE,
                                            &depth_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depth_optimized_clear_value,
                                            IID_PPV_ARGS(g_dsv_buffer.put()));

    SET_OBJECT_NAME(g_dsv_buffer.get(), L"Dsv Buffer");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_view_desc = {};
    dsv_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsv_view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_view_desc.Flags = D3D12_DSV_FLAG_NONE;

    core::g_device->CreateDepthStencilView(g_dsv_buffer.get(), &dsv_view_desc,
                                           g_dsv_heap->GetCPUDescriptorHandleForHeapStart());
}