#include "scene.h"
#include "editor/editor.h"
#include "renderer/core/command_queue.h"
#include "renderer/core/swapchain.h"
#include "renderer/pipeline/pipeline.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include <common.h>

using namespace winrt;
using namespace DirectX;

void ash::scene_render()
{
    {
        cam_update_view_mat(g_camera);
        cam_update_proj_mat(g_camera, XM_PI / 3, rhi_sw_g_viewport.Width / rhi_sw_g_viewport.Height, 0.1f, 1000.0f);

        XMFLOAT4X4 view_proj = {};
        DirectX::XMStoreFloat4x4(&view_proj, cam_get_view_proj_mat(g_camera));

        auto &command_list = rhi_cmd_g_command_list;
        {
            SCOPED_GPU_EVENT(rhi_cmd_g_command_list.get(), L"ash::scene_render")

            constexpr float clear_color[] = {0.0f, 0.0f, 0.0f, 1.0f};

            ID3D12DescriptorHeap *heap[] = {rhi_g_cbv_srv_uav_heap.get(), rhi_g_sampler_heap.get()};
            command_list->SetDescriptorHeaps(2, heap);

            command_list->SetGraphicsRootSignature(rhi_pl_g_triangle.root_signature.get());

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = rhi_g_viewport_texture.get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            command_list->ResourceBarrier(1, &barrier);

            D3D12_CPU_DESCRIPTOR_HANDLE viewport_rtv_handle = rhi_g_viewport_rtv_heap->GetCPUDescriptorHandleForHeapStart();
            D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = rhi_sw_g_swapchain_dsv_heap->GetCPUDescriptorHandleForHeapStart();

            command_list->OMSetRenderTargets(1, &viewport_rtv_handle, FALSE, &dsv_handle);

            command_list->ClearRenderTargetView(viewport_rtv_handle, clear_color, 0, nullptr);
            command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            command_list->SetPipelineState(rhi_pl_g_triangle.pso.get());
            command_list->RSSetViewports(1, &rhi_g_viewport);
            D3D12_RECT scissorRect = {0, 0, rhi_g_viewport.Width, rhi_g_viewport.Height};
            command_list->RSSetScissorRects(1, &scissorRect);
            command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            command_list->SetGraphicsRoot32BitConstants(0, 16, &view_proj, 0);
            command_list->DrawInstanced(3, 1, 0, 0);

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = rhi_g_viewport_texture.get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            command_list->ResourceBarrier(1, &barrier);

            //// Swapchain

            rhi_sw_g_current_backbuffer = rhi_sw_g_swapchain->GetCurrentBackBufferIndex();
            uint8_t &frame_index = rhi_sw_g_current_backbuffer;

            const uint32_t rtv_descriptor_size =
                rhi_g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE swapchain_rtv_Handle =
                rhi_sw_g_swapchain_rtv_heap->GetCPUDescriptorHandleForHeapStart();
            swapchain_rtv_Handle.ptr += frame_index * rtv_descriptor_size;

            barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = rhi_sw_g_render_targets[frame_index].get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            command_list->ResourceBarrier(1, &barrier);

            command_list->OMSetRenderTargets(1, &swapchain_rtv_Handle, FALSE, nullptr);
            command_list->ClearRenderTargetView(swapchain_rtv_Handle, clear_color, 0, nullptr);

            ed_render_backend();

            barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = rhi_sw_g_render_targets[frame_index].get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            command_list->ResourceBarrier(1, &barrier);
        }
        command_list->Close();
    }
}
