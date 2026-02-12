#include "renderer.h"
#include "common.h"
#include "configs/config.h"
#include "editor/editor.h"
#include "pipeline/pipeline.h"
#include "pipeline/shader.h"
#include "pipeline/shader_compiler.h"
#include "renderer/core/command_queue.h"
#include "renderer/core/swapchain.h"
#include "scene/scene.h"
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

    com_ptr<IDXGIOutput6> adapterOutput6;
    ash::rhi_g_output.as(adapterOutput6);
    assert(adapterOutput6.get());

    DXGI_MODE_DESC1 targetMode = {};
    targetMode.Format = DXGI_FORMAT_R10G10B10A2_UNORM;

    DXGI_MODE_DESC1 closeMatch = {};

    adapterOutput6->FindClosestMatchingMode1(&targetMode, &closeMatch, nullptr);

    rhi_cmd_init();
    rhi_sw_init(closeMatch.Format);
    rhi_sc_init();
    rhi_sh_init();
    rhi_pl_init();

    rhi_g_viewport = {};
    rhi_g_viewport.Width = closeMatch.Width;
    rhi_g_viewport.Height = closeMatch.Height;
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
}

void ash::rhi_render()
{
    while (rhi_g_running)
    {
        SCOPED_CPU_EVENT(L"ash::rhi_render")

        handle_window_events();

        ed_render();

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

    ExitThread(0);
}

void ash::rhi_stop()
{
    SCOPED_CPU_EVENT(L"ash::rhi_stop")
    rhi_g_running = false;
    rhi_g_renderer_thread.join();
}

void ash::rhi_resize(D3D12_VIEWPORT viewport)
{
    SCOPED_CPU_EVENT(L"ash::rhi_resize")

    rhi_g_viewport = viewport;

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

    rhi_g_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &tex_desc,
                                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &optimized_clear_value,
                                          IID_PPV_ARGS(rhi_g_viewport_texture.put()));

    assert(rhi_g_viewport_texture.get());

    SET_OBJECT_NAME(rhi_g_viewport_texture.get(), L"Viewport Texture");

    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = tex_desc.Format;
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice = 0;

    UINT rtvDescriptorSize = rhi_g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rhi_g_viewport_rtv_heap->GetCPUDescriptorHandleForHeapStart();

    rhi_g_device->CreateRenderTargetView(rhi_g_viewport_texture.get(), &rtv_desc, rtv_handle);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    auto handle_size = rhi_g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = rhi_g_cbv_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
    cpu_handle.ptr += handle_size;

    rhi_g_device->CreateShaderResourceView(rhi_g_viewport_texture.get(), &srv_desc, cpu_handle);
}
