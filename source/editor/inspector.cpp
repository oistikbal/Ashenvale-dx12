#include "inspector.h"
#include "IconsMaterialSymbols.h"
#include "common.h"
#include "console.h"
#include "scene/component.h"
#include "scene/scene.h"
#include <cstring>
#include <imgui/imgui.h>
#include <cmath>
#include <string>

namespace
{
struct rotation_ui_state
{
    uint64_t entity_id = 0;
    bool initialized = false;
    DirectX::XMFLOAT3 euler_degrees = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 last_quaternion = {0.0f, 0.0f, 0.0f, 1.0f};
};

rotation_ui_state g_rotation_ui_state = {};

bool quaternion_nearly_equal(const DirectX::XMFLOAT4 &a, const DirectX::XMFLOAT4 &b)
{
    constexpr float epsilon = 1e-5f;
    return std::abs(a.x - b.x) <= epsilon && std::abs(a.y - b.y) <= epsilon && std::abs(a.z - b.z) <= epsilon &&
           std::abs(a.w - b.w) <= epsilon;
}

DirectX::XMFLOAT3 quaternion_to_euler_radians(const DirectX::XMFLOAT4 &q)
{
    // Convert quaternion to pitch (X), yaw (Y), roll (Z) in radians.
    const float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    const float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    const float pitch = std::atan2(sinr_cosp, cosr_cosp);

    const float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    const float yaw = (std::abs(sinp) >= 1.0f) ? std::copysign(DirectX::XM_PIDIV2, sinp) : std::asin(sinp);

    const float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    const float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    const float roll = std::atan2(siny_cosp, cosy_cosp);

    return {pitch, yaw, roll};
}

bool draw_axis_float(const char *axis_label, const ImVec4 &color, float &value, float speed, bool *is_active = nullptr)
{
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(axis_label);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    const bool changed = ImGui::DragFloat("##axis", &value, speed);
    if (is_active != nullptr)
    {
        *is_active |= ImGui::IsItemActive();
    }
    return changed;
}

bool draw_vec3_row(const char *row_label, float *values, float speed, bool *is_active = nullptr)
{
    bool changed = false;

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(row_label);

    ImGui::TableSetColumnIndex(1);
    ImGui::PushID(row_label);
    changed |= draw_axis_float("X", ImVec4(0.90f, 0.30f, 0.30f, 1.0f), values[0], speed, is_active);
    ImGui::SameLine();
    ImGui::PushID("y");
    ImGui::SameLine();

    changed |= draw_axis_float("Y", ImVec4(0.30f, 0.85f, 0.30f, 1.0f), values[1], speed, is_active);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::PushID("z");
    ImGui::SameLine();

    changed |= draw_axis_float("Z", ImVec4(0.35f, 0.60f, 0.95f, 1.0f), values[2], speed, is_active);
    ImGui::PopID();
    ImGui::PopID();

    return changed;
}

void draw_name_field(flecs::entity entity)
{
    static uint64_t buffered_entity_id = 0;
    static char name_buffer[256] = {};

    if (buffered_entity_id != entity.id())
    {
        std::string name = entity.name().c_str();

        memset(name_buffer, 0, sizeof(name_buffer));
        const size_t copy_count = (name.size() < (sizeof(name_buffer) - 1)) ? name.size() : (sizeof(name_buffer) - 1);
        memcpy(name_buffer, name.c_str(), copy_count);
        buffered_entity_id = entity.id();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(ICON_MS_DEPLOYED_CODE);
    ImGui::SameLine();

    ImGui::PushItemWidth(-1.0f);
    const bool submitted =
        ImGui::InputText("##InspectorName", name_buffer, sizeof(name_buffer), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();

    if (submitted || ImGui::IsItemDeactivatedAfterEdit())
    {
        std::string old_name = entity.name().c_str();
        ash::scene_set_entity_name_safe(entity, name_buffer);

        std::string resolved_name = entity.name().c_str();
        if (resolved_name != old_name)
        {
            ash::ed_console_log(ash::ed_console_log_level::info, "[Inspector] Entity renamed.");
        }
        memset(name_buffer, 0, sizeof(name_buffer));
        const size_t copy_count =
            (resolved_name.size() < (sizeof(name_buffer) - 1)) ? resolved_name.size() : (sizeof(name_buffer) - 1);
        memcpy(name_buffer, resolved_name.c_str(), copy_count);
    }
}

void draw_transform_component(flecs::entity entity)
{
    if (!entity.has<ash::transform>())
    {
        return;
    }

    ash::transform transform_value = entity.get<ash::transform>();
    bool changed = false;

    ImGuiTreeNodeFlags header_flags = ImGuiTreeNodeFlags_DefaultOpen;
    if (ImGui::CollapsingHeader(ICON_MS_OPEN_WITH " Transform", header_flags))
    {
        ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("TransformTable", 2, table_flags))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            changed |= draw_vec3_row("Position", &transform_value.position.x, 0.1f);

            const uint64_t entity_id = entity.id();
            if (!g_rotation_ui_state.initialized || g_rotation_ui_state.entity_id != entity_id ||
                !quaternion_nearly_equal(g_rotation_ui_state.last_quaternion, transform_value.rotation))
            {
                const DirectX::XMFLOAT3 rotation_euler_radians = quaternion_to_euler_radians(transform_value.rotation);
                g_rotation_ui_state.euler_degrees = {DirectX::XMConvertToDegrees(rotation_euler_radians.x),
                                                     DirectX::XMConvertToDegrees(rotation_euler_radians.y),
                                                     DirectX::XMConvertToDegrees(rotation_euler_radians.z)};
                g_rotation_ui_state.last_quaternion = transform_value.rotation;
                g_rotation_ui_state.entity_id = entity_id;
                g_rotation_ui_state.initialized = true;
            }

            const bool rotation_changed =
                draw_vec3_row("Rotation", &g_rotation_ui_state.euler_degrees.x, 0.5f, nullptr);
            if (rotation_changed)
            {
                const DirectX::XMFLOAT3 rotation_euler_radians = {
                    DirectX::XMConvertToRadians(g_rotation_ui_state.euler_degrees.x),
                    DirectX::XMConvertToRadians(g_rotation_ui_state.euler_degrees.y),
                    DirectX::XMConvertToRadians(g_rotation_ui_state.euler_degrees.z)};
                DirectX::XMVECTOR quat =
                    DirectX::XMQuaternionRotationRollPitchYaw(rotation_euler_radians.x, rotation_euler_radians.y,
                                                              rotation_euler_radians.z);
                quat = DirectX::XMQuaternionNormalize(quat);
                DirectX::XMStoreFloat4(&transform_value.rotation, quat);
                g_rotation_ui_state.last_quaternion = transform_value.rotation;
            }
            changed |= rotation_changed;
            changed |= draw_vec3_row("Scale", &transform_value.scale.x, 0.1f);

            ImGui::EndTable();
        }
    }

    if (changed)
    {
        entity.set<ash::transform>(transform_value);
    }
}
} // namespace

void ash::ed_inspector_init()
{
    SCOPED_CPU_EVENT(L"ash::ed_inspector_init");
}

void ash::ed_inspector_render()
{
    SCOPED_CPU_EVENT(L"ash::ed_inspector_render");
    if (!ed_inspector_g_is_open)
        return;

    ImGui::Begin(ICON_MS_INFO " Inspector ###Inspector", &ed_inspector_g_is_open);

    if (!scene_g_selected.is_valid())
    {
        ImGui::TextDisabled("No entity selected.");
        ImGui::End();
        return;
    }

    draw_name_field(scene_g_selected);
    draw_transform_component(scene_g_selected);

    ImGui::End();
}
