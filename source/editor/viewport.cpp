
#include "viewport.h"
#include "IconsMaterialDesign.h"
#include "editor.h"
#include "renderer/core/device.h"
#include "renderer/renderer.h"
#include <imgui/imgui.h>

#include "common.h"

namespace
{
void create_srv()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    auto handle_size =
        ash::renderer::core::g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = ash::editor::g_imgui_heap->GetCPUDescriptorHandleForHeapStart();
    cpu_handle.ptr += handle_size;

    ash::renderer::core::g_device->CreateShaderResourceView(ash::renderer::g_viewport_texture.get(), &srv_desc,
                                                            cpu_handle);
}
} // namespace

void ash::editor::viewport::init()
{
    create_srv();
}

void ash::editor::viewport::render()
{
    SCOPED_CPU_EVENT(L"ash::editor::viewport::render");
    if (!g_is_open)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin(ICON_MD_LANDSCAPE " Viewport ###Viewport", &g_is_open))
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
            create_srv();
        }

        auto handle_size =
            ash::renderer::core::g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = g_imgui_heap->GetGPUDescriptorHandleForHeapStart();
        gpu_handle.ptr += handle_size;

        ImGui::Image((ImTextureID)(intptr_t)gpu_handle.ptr, ImVec2(newWidth, newHeight));
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}
