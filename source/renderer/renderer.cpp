#include "renderer.h"
#include "common.h"
#include "configs/config.h"
#include "editor/editor.h"
#include "pipeline/pipeline.h"
#include "pipeline/shader.h"
#include "pipeline/shader_compiler.h"
#include "renderer/core/command_queue.h"
#include "renderer/core/device.h"
#include "renderer/core/swapchain.h"
#include "window/window.h"
#include <dxgidebug.h>
#include <filesystem>

using namespace winrt;

namespace
{
void handle_window_events()
{
    SCOPED_CPU_EVENT(L"::handle_window_events")

    ash::window::event::swap_buffers(ash::window::g_queue);
    auto &q = ash::window::event::get_back_buffer(ash::window::g_queue);

    if (q.empty())
        return;

    ash::window::event::event e;

    bool resize = false;

    while (!q.empty())
    {
        e = std::move(q.front());
        q.pop();

        switch (e.type)
        {
        case ash::window::event::windows_event::resize:
            resize = true;
            break;
        default:
            break;
        }
    }

    if (resize)
        ash::renderer::core::swapchain::resize();
}
} // namespace

void ash::renderer::init()
{
    SCOPED_CPU_EVENT(L"ash::renderer::init");
#if _DEBUG
    com_ptr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }

    com_ptr<IDXGIInfoQueue> dxgiInfoQueue;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
    {
        dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
    }
#endif

    com_ptr<IDXGIFactory7> factory;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

    factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(core::g_adapter.put()));

    D3D12CreateDevice(core::g_adapter.get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(core::g_device.put()));
    assert(core::g_device.get());

    com_ptr<IDXGIOutput> adapterOutput;
    core::g_adapter->EnumOutputs(0, adapterOutput.put());
    assert(adapterOutput.get());

    com_ptr<IDXGIOutput6> adapterOutput6;
    adapterOutput.as(adapterOutput6);
    assert(adapterOutput6.get());

    DXGI_MODE_DESC1 targetMode = {};
    targetMode.Format = DXGI_FORMAT_R10G10B10A2_UNORM;

    DXGI_MODE_DESC1 closeMatch = {};

    adapterOutput6->FindClosestMatchingMode1(&targetMode, &closeMatch, nullptr);

    core::command_queue::init();
    core::swapchain::init(closeMatch.Format);
    pipeline::shader_compiler::init();
    pipeline::shader::init();
    pipeline::init();

    g_viewport = {};
    g_viewport.Width = 1024;
    g_viewport.Height = 1024;
    g_viewport.MinDepth = 0.0f;
    g_viewport.MaxDepth = 1.0f;
    g_viewport.TopLeftX = 0.0f;
    g_viewport.TopLeftY = 0.0f;

    renderer::resize(g_viewport);

    g_renderer_thread = std::thread(render);
    HANDLE hThread = static_cast<HANDLE>(g_renderer_thread.native_handle());
    SetThreadDescription(hThread, L"Renderer Thread");
    SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
}

void ash::renderer::render()
{
    while (g_running)
    {
        SCOPED_CPU_EVENT(L"ash::renderer::render")

        handle_window_events();

        editor::render();

        {
            constexpr float clear_color[] = {0.0f, 0.0f, 0.0f, 1.0f};

            // Viewport

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = g_viewport_texture.get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            core::command_queue::g_command_list->ResourceBarrier(1, &barrier);
            D3D12_CPU_DESCRIPTOR_HANDLE viewport_rtv_handle = g_viewport_rtv_heap->GetCPUDescriptorHandleForHeapStart();

            core::command_queue::g_command_list->OMSetRenderTargets(1, &viewport_rtv_handle, FALSE, nullptr);
            core::command_queue::g_command_list->ClearRenderTargetView(viewport_rtv_handle, clear_color, 0, nullptr);

            core::command_queue::g_command_list->SetPipelineState(pipeline::g_triangle.pso.get());
            core::command_queue::g_command_list->SetGraphicsRootSignature(pipeline::g_triangle.root_signature.get());
            D3D12_RECT scissorRect = {0, 0, g_viewport.Width, g_viewport.Height};
            core::command_queue::g_command_list->RSSetViewports(1, &renderer::g_viewport);
            core::command_queue::g_command_list->RSSetScissorRects(1, &scissorRect);
            core::command_queue::g_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            core::command_queue::g_command_list->DrawInstanced(3, 1, 0, 0);

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = g_viewport_texture.get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            core::command_queue::g_command_list->ResourceBarrier(1, &barrier);

            //// Swapchain

            core::swapchain::g_current_backbuffer = core::swapchain::g_swapchain->GetCurrentBackBufferIndex();
            uint8_t &frame_index = core::swapchain::g_current_backbuffer;

            const uint32_t rtv_descriptor_size =
                core::g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE swapchain_rtv_Handle =
                core::swapchain::g_rtv_heap->GetCPUDescriptorHandleForHeapStart();
            swapchain_rtv_Handle.ptr += frame_index * rtv_descriptor_size;

            barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = core::swapchain::g_render_targets[frame_index].get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            core::command_queue::g_command_list->ResourceBarrier(1, &barrier);

            core::command_queue::g_command_list->OMSetRenderTargets(1, &swapchain_rtv_Handle, FALSE, nullptr);

            core::command_queue::g_command_list->ClearRenderTargetView(swapchain_rtv_Handle, clear_color, 0, nullptr);

            editor::render_backend();

            barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = core::swapchain::g_render_targets[frame_index].get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            core::command_queue::g_command_list->ResourceBarrier(1, &barrier);
            core::command_queue::g_command_list->Close();
        }

        ID3D12CommandList *cmdLists[] = {core::command_queue::g_command_list.get()};
        core::command_queue::g_direct->ExecuteCommandLists(1, cmdLists);
        core::swapchain::g_swapchain->Present(1, 0);

        core::swapchain::g_fence_value++;
        core::command_queue::g_direct->Signal(core::swapchain::g_fence.get(), core::swapchain::g_fence_value);

        if (core::swapchain::g_fence->GetCompletedValue() < core::swapchain::g_fence_value)
        {
            core::swapchain::g_fence->SetEventOnCompletion(core::swapchain::g_fence_value,
                                                           core::swapchain::g_fence_event);
            WaitForSingleObject(core::swapchain::g_fence_event, INFINITE);
        }

        core::command_queue::g_command_allocator->Reset();
        core::command_queue::g_command_list->Reset(core::command_queue::g_command_allocator.get(), nullptr);
    }

    ExitThread(0);
}

void ash::renderer::stop()
{
    SCOPED_CPU_EVENT(L"ash::renderer::stop")
    g_running = false;
    g_renderer_thread.join();
}

void ash::renderer::resize(D3D12_VIEWPORT viewport)
{
    g_viewport = viewport;

    D3D12_RESOURCE_DESC tex_desc = {};
    tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex_desc.Alignment = 0;
    tex_desc.Width = viewport.Width;
    tex_desc.Height = viewport.Height;
    tex_desc.DepthOrArraySize = 1;
    tex_desc.MipLevels = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    tex_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE optimized_clear_value = {};
    optimized_clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    optimized_clear_value.Color[0] = 0.0f;
    optimized_clear_value.Color[1] = 0.0f;
    optimized_clear_value.Color[2] = 0.0f;
    optimized_clear_value.Color[3] = 1.0f;

    D3D12_HEAP_PROPERTIES heap_properties = {};
    heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;

    core::g_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &tex_desc,
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &optimized_clear_value,
                                            IID_PPV_ARGS(g_viewport_texture.put()));

    assert(g_viewport_texture.get());

    SET_OBJECT_NAME(g_viewport_texture.get(), L"Viewport Texture");

    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.NumDescriptors = 1;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    core::g_device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(g_viewport_rtv_heap.put()));

    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = tex_desc.Format;
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice = 0;

    UINT rtvDescriptorSize = core::g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = g_viewport_rtv_heap->GetCPUDescriptorHandleForHeapStart();

    core::g_device->CreateRenderTargetView(g_viewport_texture.get(), &rtv_desc, rtv_handle);

    SET_OBJECT_NAME(g_viewport_rtv_heap.get(), L"Viewport Rtv Heap");
}
