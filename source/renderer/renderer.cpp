#include "renderer.h"
#include "renderer/core/command_queue.h"
#include "renderer/core/device.h"
#include "renderer/core/swapchain.h"
#include "pipeline/shader_compiler.h"
#include "pipeline/shader.h"
#include "window/window.h"
#include <dxgidebug.h>

using namespace winrt;

void ash::renderer::init()
{
    com_ptr<IDXGIFactory7> factory;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

    factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(core::g_adapter.put()));

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


    g_renderer_thread = std::thread(render);
    HANDLE hThread = static_cast<HANDLE>(g_renderer_thread.native_handle());
    SetThreadDescription(hThread, L"Renderer Thread");
    SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
}

void ash::renderer::render()
{
    while (g_running)
    {
        core::swapchain::g_current_backbuffer = core::swapchain::g_swapchain->GetCurrentBackBufferIndex();
        uint8_t &frameIndex = core::swapchain::g_current_backbuffer;

        core::command_queue::g_command_allocator->Reset();
        core::command_queue::g_command_list->Reset(core::command_queue::g_command_allocator.get(), nullptr);

        uint32_t rtvDescriptorSize = core::g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = core::swapchain::g_rtv_heap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += frameIndex * rtvDescriptorSize;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = core::swapchain::g_render_targets[frameIndex].get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        core::command_queue::g_command_list->ResourceBarrier(1, &barrier);

        core::command_queue::g_command_list->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        float clearColor[] = {0.1f, 0.1f, 0.3f, 1.0f};
        core::command_queue::g_command_list->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = core::swapchain::g_render_targets[frameIndex].get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        core::command_queue::g_command_list->ResourceBarrier(1, &barrier);

        core::command_queue::g_command_list->Close();

        ID3D12CommandList *cmdLists[] = {core::command_queue::g_command_list.get()};
        core::command_queue::g_direct->ExecuteCommandLists(1, cmdLists);
        core::swapchain::g_swapchain->Present(1, 0);

        core::swapchain::g_fence_value++;
        core::command_queue::g_direct->Signal(core::swapchain::g_fence.get(), core::swapchain::g_fence_value);
        if (core::swapchain::g_fence->GetCompletedValue() < core::swapchain::g_fence_value)
        {
            core::swapchain::g_fence->SetEventOnCompletion(core::swapchain::g_fence_value, core::swapchain::g_fence_event);
            WaitForSingleObject(core::swapchain::g_fence_event, INFINITE);
        }
    }

    ExitThread(0);
}

void ash::renderer::stop()
{
    g_running = false;
    g_renderer_thread.join();
}
