#include "editor.h"
#include "common.h"
#include "configs/config.h"
#include "renderer/core/command_queue.h"
#include "renderer/core/device.h"
#include "renderer/core/swapchain.h"
#include "window/window.h"
#include <filesystem>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx12.h>
#include <imgui/imgui_impl_win32.h>
#include "renderer/core/command_queue.h"

using namespace winrt;
namespace
{
winrt::com_ptr<ID3D12DescriptorHeap> g_imgui_heap;
}

void ash::editor::init()
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    renderer::core::g_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(g_imgui_heap.put()));

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = g_imgui_heap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = g_imgui_heap->GetGPUDescriptorHandleForHeapStart();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = renderer::core::g_device.get();
    init_info.CommandQueue = renderer::core::command_queue::g_direct.get();
    init_info.NumFramesInFlight = 2;
    init_info.RTVFormat = renderer::core::swapchain::g_format;

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

void ash::editor::render()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();
    ImGui::Render();
}

void ash::editor::render_backend()
{
    ID3D12DescriptorHeap *heaps[] = {g_imgui_heap.get()};
    renderer::core::command_queue::g_command_list->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), renderer::core::command_queue::g_command_list.get());
}
