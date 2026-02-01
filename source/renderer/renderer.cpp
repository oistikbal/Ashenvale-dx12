#include "renderer.h"
#include "configs/config.h"
#include "pipeline/pipeline.h"
#include "pipeline/shader.h"
#include "pipeline/shader_compiler.h"
#include "renderer/core/command_queue.h"
#include "renderer/core/device.h"
#include "renderer/core/swapchain.h"
#include "window/window.h"
#include <dxgidebug.h>
#include <filesystem>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx12.h>
#include <imgui/imgui_impl_win32.h>

using namespace winrt;

static com_ptr<ID3D12DescriptorHeap> g_imgui_heap;

namespace
{
void handle_window_events()
{
    SCOPED_CPU_EVENT(L"::handle_window_events");

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

    g_renderer_thread = std::thread(render);
    HANDLE hThread = static_cast<HANDLE>(g_renderer_thread.native_handle());
    SetThreadDescription(hThread, L"Renderer Thread");
    SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    core::g_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(g_imgui_heap.put()));

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = g_imgui_heap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = g_imgui_heap->GetGPUDescriptorHandleForHeapStart();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = core::g_device.get();
    init_info.CommandQueue = core::command_queue::g_direct.get();
    init_info.NumFramesInFlight = 2;
    init_info.RTVFormat = core::swapchain::g_format;

    init_info.SrvDescriptorHeap = g_imgui_heap.get();
    init_info.SrvDescriptorAllocFn = nullptr;
    init_info.SrvDescriptorFreeFn = nullptr;
    init_info.LegacySingleSrvCpuDescriptor = cpu;
    init_info.LegacySingleSrvGpuDescriptor = gpu;

    ImGui_ImplWin32_Init(window::g_hwnd);
    ImGui_ImplDX12_Init(&init_info);

    ImFontConfig config{};
    config.MergeMode = false;
    config.PixelSnapH = true;

    std::string full_path = (std::filesystem::path(config::RESOURCES_PATH) / "Roboto-Regular.ttf").string();
    io.Fonts->AddFontFromFileTTF((full_path.c_str()), 16.0f, &config);
}

void ash::renderer::render()
{
    while (g_running)
    {
        SCOPED_CPU_EVENT(L"ash::renderer::render")

        handle_window_events();

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();
        ImGui::Render();

        {
            core::swapchain::g_current_backbuffer = core::swapchain::g_swapchain->GetCurrentBackBufferIndex();
            uint8_t &frameIndex = core::swapchain::g_current_backbuffer;

            uint32_t rtvDescriptorSize =
                core::g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
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

            core::command_queue::g_command_list->SetPipelineState(pipeline::g_triangle.pso.get());
            core::command_queue::g_command_list->SetGraphicsRootSignature(pipeline::g_triangle.root_signature.get());
            D3D12_RECT scissorRect = {0, 0, core::swapchain::g_viewport.Width, core::swapchain::g_viewport.Height};
            core::command_queue::g_command_list->RSSetViewports(1, &core::swapchain::g_viewport);
            core::command_queue::g_command_list->RSSetScissorRects(1, &scissorRect);
            core::command_queue::g_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            core::command_queue::g_command_list->DrawInstanced(3, 1, 0, 0);

            ID3D12DescriptorHeap *heaps[] = {g_imgui_heap.get()};
            core::command_queue::g_command_list->SetDescriptorHeaps(1, heaps);
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), core::command_queue::g_command_list.get());

            barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = core::swapchain::g_render_targets[frameIndex].get();
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

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    ExitThread(0);
}

void ash::renderer::stop()
{
    SCOPED_CPU_EVENT(L"ash::renderer::stop")
    g_running = false;
    g_renderer_thread.join();
}
