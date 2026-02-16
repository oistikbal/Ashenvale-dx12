#include "renderer.h"
#include "common.h"
#include "configs/config.h"
#include "editor/editor.h"
#include "pipeline/pipeline.h"
#include "pipeline/shader.h"
#include "pipeline/shader_compiler.h"
#include "renderer/core/command_queue.h"
#include "renderer/core/swapchain.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "window/input.h"
#include "window/window.h"
#include <dxgidebug.h>
#include <filesystem>

using namespace winrt;

namespace
{
void init_device()
{
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

    factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                        IID_PPV_ARGS(ash::rhi_g_adapter.put()));

    D3D12CreateDevice(ash::rhi_g_adapter.get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(ash::rhi_g_device.put()));
    assert(ash::rhi_g_device.get());

    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    ash::rhi_g_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));

    switch (options.ResourceBindingTier)
    {
    case D3D12_RESOURCE_BINDING_TIER_1:
        MessageBox(ash::win_g_hwnd, "Tier 1 GPU is not supported.", 0, MB_ICONWARNING);
        ExitProcess(0);
        break;
    case D3D12_RESOURCE_BINDING_TIER_2:
        MessageBox(ash::win_g_hwnd, "Tier 2 GPU is not supported.", 0, MB_ICONWARNING);
        ExitProcess(0);
        break;
    case D3D12_RESOURCE_BINDING_TIER_3:
        break;
    }

    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = {D3D_SHADER_MODEL_6_6};
    if (SUCCEEDED(
            ash::rhi_g_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))))
    {
        if (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_6)
        {
            MessageBox(ash::win_g_hwnd, "Shader Model 6.6 is required.", 0, MB_ICONWARNING);
            ExitProcess(0);
        }
    }

    ash::rhi_g_adapter->EnumOutputs(0, ash::rhi_g_output.put());
    assert(ash::rhi_g_output.get());

    D3D12MA::ALLOCATOR_DESC allocator_desc = {};
    allocator_desc.pDevice = ash::rhi_g_device.get();
    allocator_desc.pAdapter = ash::rhi_g_adapter.get();
    D3D12MA::CreateAllocator(&allocator_desc, ash::rhi_g_allocator.put());

    assert(ash::rhi_g_allocator.get());
}

void handle_window_events()
{
    SCOPED_CPU_EVENT(L"ash::rhi_handle_window_events")

    ash::win_evt_swap_buffers(ash::win_g_queue);
    auto &q = ash::win_evt_get_back_buffer(ash::win_g_queue);

    if (q.empty())
        return;

    ash::win_evt_event e;

    bool resize = false;

    while (!q.empty())
    {
        e = std::move(q.front());
        q.pop();

        switch (e.type)
        {
        case ash::win_evt_windows_event::resize:
            resize = true;
            break;
        default:
            break;
        }
    }

    if (resize)
        ash::rhi_sw_resize();
}
} // namespace

void ash::rhi_init()
{
    SCOPED_CPU_EVENT(L"ash::rhi_init");

    init_device();

    rhi_cmd_init();
    rhi_sw_init();
    rhi_sc_init();
    rhi_sh_init();
    rhi_pl_init();

    rhi_g_viewport = {};
    rhi_g_viewport.Width = rhi_sw_g_viewport.Width;
    rhi_g_viewport.Height = rhi_sw_g_viewport.Height;
    rhi_g_viewport.MinDepth = 0.0f;
    rhi_g_viewport.MaxDepth = 1.0f;
    rhi_g_viewport.TopLeftX = 0.0f;
    rhi_g_viewport.TopLeftY = 0.0f;

    D3D12_DESCRIPTOR_HEAP_DESC viewport_rtv_heap_desc = {};
    viewport_rtv_heap_desc.NumDescriptors = 1;
    viewport_rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    viewport_rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rhi_g_device->CreateDescriptorHeap(&viewport_rtv_heap_desc, IID_PPV_ARGS(rhi_g_viewport_rtv_heap.put()));

    assert(rhi_g_viewport_rtv_heap.get());
    SET_OBJECT_NAME(rhi_g_viewport_rtv_heap.get(), L"Viewport Rtv Desc Heap");

    D3D12_DESCRIPTOR_HEAP_DESC cbv_srv_uav_heap_desc = {};
    cbv_srv_uav_heap_desc.NumDescriptors = 1000000;
    cbv_srv_uav_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbv_srv_uav_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    rhi_g_device->CreateDescriptorHeap(&cbv_srv_uav_heap_desc, IID_PPV_ARGS(rhi_g_cbv_srv_uav_heap.put()));

    assert(rhi_g_cbv_srv_uav_heap.get());
    SET_OBJECT_NAME(rhi_g_cbv_srv_uav_heap.get(), L"CBV SRV UAV Desc Heap");

    D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc = {};
    sampler_heap_desc.NumDescriptors = 2048;
    sampler_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    sampler_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    rhi_g_device->CreateDescriptorHeap(&sampler_heap_desc, IID_PPV_ARGS(rhi_g_sampler_heap.put()));

    assert(rhi_g_sampler_heap.get());
    SET_OBJECT_NAME(rhi_g_sampler_heap.get(), L"Sampler Desc Heap");

    rhi_resize(rhi_g_viewport);

    rhi_g_renderer_thread = std::thread(rhi_render);
    HANDLE hThread = static_cast<HANDLE>(rhi_g_renderer_thread.native_handle());
    SetThreadDescription(hThread, L"Renderer Thread");
    SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);

    g_camera.position = DirectX::XMFLOAT3(0, 0, -1.0f);
    g_camera.rotation = DirectX::XMFLOAT3(0, 0, 0.0f);
}

void ash::rhi_render()
{
    auto last_time = std::chrono::high_resolution_clock::now();

    while (rhi_g_running)
    {
        SCOPED_CPU_EVENT(L"ash::rhi_render")

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> delta_time = now - last_time;
        last_time = now;

        handle_window_events();

        ed_render();

        cam_handle_input(g_camera, delta_time.count(), win_input_acquire_front_buffer(ash::g_win_input));
        win_input_release_front_buffer(ash::g_win_input);

        scene_render();

        ID3D12CommandList *cmdLists[] = {rhi_cmd_g_command_list.get()};
        rhi_cmd_g_direct->ExecuteCommandLists(1, cmdLists);
        rhi_sw_g_swapchain->Present(1, 0);

        rhi_sw_g_fence_value++;
        rhi_cmd_g_direct->Signal(rhi_sw_g_fence.get(), rhi_sw_g_fence_value);

        if (rhi_sw_g_fence->GetCompletedValue() < rhi_sw_g_fence_value)
        {
            rhi_sw_g_fence->SetEventOnCompletion(rhi_sw_g_fence_value, rhi_sw_g_fence_event);
            WaitForSingleObject(rhi_sw_g_fence_event, INFINITE);
        }

        rhi_cmd_g_command_allocator->Reset();
        rhi_cmd_g_command_list->Reset(rhi_cmd_g_command_allocator.get(), nullptr);
    }
}

void ash::rhi_stop()
{
    SCOPED_CPU_EVENT(L"ash::rhi_stop")
    rhi_g_running = false;
    if (rhi_g_renderer_thread.joinable())
    {
        rhi_g_renderer_thread.join();
    }
}

void ash::rhi_shutdown()
{
    SCOPED_CPU_EVENT(L"ash::rhi_shutdown")

    rhi_g_viewport_texture = nullptr;
    rhi_sw_g_dsv_buffer = nullptr;

    for (UINT i = 0; i < 2; ++i)
    {
        rhi_sw_g_render_targets[i] = nullptr;
    }

    rhi_pl_g_triangle.pso = nullptr;
    rhi_pl_g_triangle.root_signature = nullptr;

    rhi_sh_g_triangle_vs.blob = nullptr;
    rhi_sh_g_triangle_vs.root_blob = nullptr;
    rhi_sh_g_triangle_vs.input_layout.clear();
    rhi_sh_g_triangle_vs.bindings.clear();

    rhi_sh_g_triangle_ps.blob = nullptr;
    rhi_sh_g_triangle_ps.root_blob = nullptr;
    rhi_sh_g_triangle_ps.input_layout.clear();
    rhi_sh_g_triangle_ps.bindings.clear();

    rhi_cmd_g_command_list = nullptr;
    rhi_cmd_g_command_allocator = nullptr;
    rhi_cmd_g_copy = nullptr;
    rhi_cmd_g_compute = nullptr;
    rhi_cmd_g_direct = nullptr;

    rhi_g_cbv_srv_uav_heap = nullptr;
    rhi_g_sampler_heap = nullptr;
    rhi_g_viewport_rtv_heap = nullptr;
    rhi_g_rtv_heap = nullptr;

    rhi_sw_g_swapchain_rtv_heap = nullptr;
    rhi_sw_g_swapchain_dsv_heap = nullptr;
    rhi_sw_g_swapchain = nullptr;
    rhi_sw_g_fence = nullptr;

    if (rhi_sw_g_fence_event)
    {
        CloseHandle(rhi_sw_g_fence_event);
        rhi_sw_g_fence_event = nullptr;
    }

    rhi_sc_g_compiler = nullptr;
    rhi_sc_g_utils = nullptr;

    rhi_g_allocator = nullptr;
    rhi_g_output = nullptr;
    rhi_g_adapter = nullptr;
    rhi_g_device = nullptr;
}

void ash::rhi_resize(D3D12_VIEWPORT viewport)
{
    SCOPED_CPU_EVENT(L"ash::rhi_resize")

    rhi_g_viewport = viewport;

    D3D12_RESOURCE_DESC tex_desc = {};
    tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex_desc.Alignment = 0;
    tex_desc.Width = static_cast<UINT64>(viewport.Width);
    tex_desc.Height = static_cast<UINT>(viewport.Height);
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

    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    rhi_g_allocator->CreateResource(&alloc_desc, &tex_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    &optimized_clear_value, rhi_g_viewport_texture.put(), IID_NULL, nullptr);
    assert(rhi_g_viewport_texture.get());

    SET_OBJECT_NAME(rhi_g_viewport_texture.get(), L"Viewport Texture");

    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = tex_desc.Format;
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rhi_g_viewport_rtv_heap->GetCPUDescriptorHandleForHeapStart();

    rhi_g_device->CreateRenderTargetView(rhi_g_viewport_texture->GetResource(), &rtv_desc, rtv_handle);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    auto handle_size = rhi_g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = rhi_g_cbv_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
    cpu_handle.ptr += handle_size;

    rhi_g_device->CreateShaderResourceView(rhi_g_viewport_texture->GetResource(), &srv_desc, cpu_handle);
}
