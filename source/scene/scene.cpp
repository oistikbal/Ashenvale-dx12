#include "scene.h"
#include "editor/console.h"
#include "editor/editor.h"
#include "renderer/core/command_queue.h"
#include "renderer/core/swapchain.h"
#include "renderer/pipeline/pipeline.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include <common.h>
#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace winrt;
using namespace DirectX;

namespace
{
flecs::entity lookup_name_in_scope(const std::string &name, flecs::entity parent)
{
    if (parent.is_valid())
    {
        return parent.lookup(name.c_str(), false);
    }

    return ash::scene_g_world.lookup(name.c_str(), "::", "::", false);
}
} // namespace

std::string ash::scene_make_unique_name(std::string_view desired_name, flecs::entity parent, flecs::entity ignore)
{
    const std::string base_name = desired_name.empty() ? "GameObject" : std::string(desired_name);
    std::string candidate_name = base_name;

    uint32_t suffix = 2;
    while (true)
    {
        flecs::entity existing = lookup_name_in_scope(candidate_name, parent);
        const bool is_unique = !existing.is_valid() || (ignore.is_valid() && existing.id() == ignore.id());

        if (is_unique)
        {
            return candidate_name;
        }

        candidate_name = base_name + " (" + std::to_string(suffix++) + ")";
    }
}

void ash::scene_set_entity_name_safe(flecs::entity entity, std::string_view desired_name)
{
    if (!entity.is_valid())
    {
        return;
    }

    const flecs::entity parent = entity.parent();
    const std::string unique_name = scene_make_unique_name(desired_name, parent, entity);
    entity.set_name(unique_name.c_str());
}

flecs::entity ash::scene_create_empty(flecs::entity parent)
{
    static uint32_t game_object_counter = 1;
    const std::string name = "GameObject_" + std::to_string(game_object_counter++);

    flecs::entity game_object_entity = scene_g_world.entity().add<ash::game_object>().set<ash::transform>({});

    if (parent.is_valid())
    {
        game_object_entity.child_of(parent);
    }

    scene_set_entity_name_safe(game_object_entity, name);
    scene_g_selected = game_object_entity;
    ed_console_log(ed_console_log_level::info, "[Scene] Empty entity created.");
    return game_object_entity;
}

bool ash::scene_load_gltf(const std::filesystem::path &path)
{
    ed_console_log(ed_console_log_level::info, "[Scene] glTF import begin.");

    if (!std::filesystem::exists(path))
    {
        std::cerr << "glTF import failed. File does not exist: " << path << '\n';
        ed_console_log(ed_console_log_level::error, "glTF import failed: file does not exist.");
        return false;
    }

    fastgltf::Parser parser(fastgltf::Extensions::KHR_mesh_quantization);
    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::DecomposeNodeMatrices;
    constexpr auto categories =
        fastgltf::Category::Asset | fastgltf::Category::Scenes | fastgltf::Category::Nodes | fastgltf::Category::Meshes;

    auto gltf_file = fastgltf::MappedGltfFile::FromPath(path);
    if (!bool(gltf_file))
    {
        std::cerr << "glTF import failed to open file: " << fastgltf::getErrorMessage(gltf_file.error()) << '\n';
        ed_console_log(ed_console_log_level::error, "glTF import failed: unable to open file.");
        return false;
    }

    auto loaded_asset = parser.loadGltf(gltf_file.get(), path.parent_path(), options, categories);
    if (loaded_asset.error() != fastgltf::Error::None)
    {
        std::cerr << "glTF import failed to parse file: " << fastgltf::getErrorMessage(loaded_asset.error()) << '\n';
        ed_console_log(ed_console_log_level::error, "glTF import failed: parse error.");
        return false;
    }

    fastgltf::Asset asset = std::move(loaded_asset.get());
    if (asset.scenes.empty())
    {
        std::cerr << "glTF import failed: no scenes in file.\n";
        ed_console_log(ed_console_log_level::error, "glTF import failed: no scenes in file.");
        return false;
    }

    std::size_t scene_index = asset.defaultScene.value_or(0);
    if (scene_index >= asset.scenes.size())
    {
        scene_index = 0;
    }

    const auto &gltf_scene = asset.scenes[scene_index];
    std::string scene_name = gltf_scene.name.empty() ? path.stem().string() : std::string(gltf_scene.name.c_str());
    if (scene_name.empty())
    {
        scene_name = "Imported Scene";
    }

    flecs::entity import_root = scene_g_world.entity().add<ash::game_object>().set<ash::transform>({});
    scene_set_entity_name_safe(import_root, "glTF: " + scene_name);
    scene_g_selected = import_root;

    std::function<void(std::size_t, flecs::entity)> import_node = [&](std::size_t node_index, flecs::entity parent) {
        if (node_index >= asset.nodes.size())
        {
            return;
        }

        const auto &node = asset.nodes[node_index];
        std::string node_name = node.name.empty() ? ("Node_" + std::to_string(node_index)) : std::string(node.name.c_str());
        if (node.meshIndex.has_value())
        {
            node_name += " [Mesh " + std::to_string(node.meshIndex.value()) + "]";
        }

        ash::transform entity_transform = {};
        if (const auto *trs = std::get_if<fastgltf::TRS>(&node.transform))
        {
            entity_transform.position = {trs->translation[0], trs->translation[1], trs->translation[2]};
            entity_transform.rotation = {trs->rotation[0], trs->rotation[1], trs->rotation[2], trs->rotation[3]};
            entity_transform.scale = {trs->scale[0], trs->scale[1], trs->scale[2]};
        }
        else
        {
            fastgltf::math::fvec3 scale = {1.0f, 1.0f, 1.0f};
            fastgltf::math::fquat rotation(0.0f, 0.0f, 0.0f, 1.0f);
            fastgltf::math::fvec3 translation = {0.0f, 0.0f, 0.0f};

            auto matrix = std::get<fastgltf::math::fmat4x4>(node.transform);
            fastgltf::math::decomposeTransformMatrix(matrix, scale, rotation, translation);

            entity_transform.position = {translation[0], translation[1], translation[2]};
            entity_transform.rotation = {rotation[0], rotation[1], rotation[2], rotation[3]};
            entity_transform.scale = {scale[0], scale[1], scale[2]};
        }

        flecs::entity entity = scene_g_world.entity().add<ash::game_object>().set<ash::transform>(entity_transform);
        if (parent.is_valid())
        {
            entity.child_of(parent);
        }
        scene_set_entity_name_safe(entity, node_name);

        for (std::size_t child_node_index : node.children)
        {
            import_node(child_node_index, entity);
        }
    };

    for (std::size_t root_node_index : gltf_scene.nodeIndices)
    {
        import_node(root_node_index, import_root);
    }

    ed_console_log(ed_console_log_level::info, "[Scene] glTF import complete.");
    return true;
}

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
            barrier.Transition.pResource = rhi_g_viewport_texture->GetResource();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            command_list->ResourceBarrier(1, &barrier);

            D3D12_CPU_DESCRIPTOR_HANDLE viewport_rtv_handle =
                rhi_g_viewport_rtv_heap->GetCPUDescriptorHandleForHeapStart();
            D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = rhi_sw_g_swapchain_dsv_heap->GetCPUDescriptorHandleForHeapStart();

            command_list->OMSetRenderTargets(1, &viewport_rtv_handle, FALSE, &dsv_handle);

            command_list->ClearRenderTargetView(viewport_rtv_handle, clear_color, 0, nullptr);
            command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            command_list->SetPipelineState(rhi_pl_g_triangle.pso.get());
            command_list->RSSetViewports(1, &rhi_g_viewport);
            D3D12_RECT scissorRect = {0, 0, static_cast<UINT>(rhi_g_viewport.Width),
                                      static_cast<UINT>(rhi_g_viewport.Height)};
            command_list->RSSetScissorRects(1, &scissorRect);
            command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            command_list->SetGraphicsRoot32BitConstants(0, 16, &view_proj, 0);
            command_list->DrawInstanced(3, 1, 0, 0);

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = rhi_g_viewport_texture->GetResource();
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
