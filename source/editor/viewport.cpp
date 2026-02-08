
#include "viewport.h"
#include "IconsMaterialDesign.h"
#include "editor.h"
#include "renderer/core/device.h"
#include "renderer/renderer.h"
#include <imgui/imgui.h>

#include "common.h"

void ash::editor::viewport::init()
{
    SCOPED_CPU_EVENT(L"ash::editor::viewport::init");
}

void ash::editor::viewport::render()
{
    SCOPED_CPU_EVENT(L"ash::editor::viewport::render");
    if (!g_is_open)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    bool is_expanded = ImGui::Begin(ICON_MD_LANDSCAPE " Viewport ###Viewport", &g_is_open);
    if (is_expanded)
    {
        ImVec2 size = ImGui::GetContentRegionAvail();
        int newWidth = size.x < 16 ? 16 : static_cast<int>(size.x);
        int newHeight = size.y < 16 ? 16 : static_cast<int>(size.y);

        static int lastW = 0, lastH = 0;
        if (newWidth != lastW || newHeight != lastH)
        {
            lastW = newWidth;
            lastH = newHeight;

            D3D12_VIEWPORT new_viewport = ash::renderer::g_viewport;
            new_viewport.Height = newHeight;
            new_viewport.Width = newWidth;
            ash::renderer::resize(new_viewport);
        }

        auto handle_size =
            ash::renderer::core::g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = ash::renderer::g_cbv_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
        gpu_handle.ptr += 1 * handle_size;

        ImGui::Image((ImTextureID)(intptr_t)gpu_handle.ptr, ImVec2(newWidth, newHeight));
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}
