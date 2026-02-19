#include "hierarchy.h"
#include "IconsMaterialSymbols.h"
#include "common.h"
#include "console.h"
#include "scene/component.h"
#include "scene/scene.h"
#include <cstdint>
#include <imgui/imgui.h>

namespace
{
struct pending_create_request
{
    bool requested = false;
    flecs::entity parent = flecs::entity::null();
};

bool has_gameobject_child(flecs::entity entity)
{
    bool has_child = false;
    entity.children([&has_child](flecs::entity child) {
        if (!has_child && child.has<ash::game_object>())
        {
            has_child = true;
        }
    });
    return has_child;
}

void draw_entity_row(flecs::entity entity, pending_create_request &create_request)
{
    const bool has_children = has_gameobject_child(entity);
    const auto entity_name = entity.name();
    const char *label = entity_name.c_str()[0] != '\0' ? entity_name.c_str() : "<unnamed>";
    const char *entity_icon = has_children ? ICON_MS_DEPLOYED_CODE_UPDATE : ICON_MS_DEPLOYED_CODE;

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(static_cast<int>(entity.id()));

    ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                                    ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_FramePadding;
    if (ash::scene_g_selected.is_valid() && ash::scene_g_selected.id() == entity.id())
    {
        node_flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (!has_children)
    {
        node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    const bool is_open = ImGui::TreeNodeEx("node", node_flags, "%s %s", entity_icon, label);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        ash::scene_g_selected = entity;
    }

    if (ImGui::BeginPopupContextItem())
    {
        ImGui::PushID(entity.id());
        if (ImGui::MenuItem(ICON_MS_ADD " Create Empty"))
        {
            create_request.requested = true;
            create_request.parent = entity;
        }
        ImGui::PopID();
        ImGui::EndPopup();
    }

    if (ImGui::TableSetColumnIndex(1))
    {
        ImGui::TextDisabled(ICON_MS_DEPLOYED_CODE " GameObject");
    }

    if (ImGui::TableSetColumnIndex(2))
    {
        ImGui::TextDisabled("#%llu", static_cast<unsigned long long>(entity.id()));
    }

    if (has_children && is_open)
    {
        entity.children([&create_request](flecs::entity child) {
            if (child.has<ash::game_object>())
            {
                draw_entity_row(child, create_request);
            }
        });
        ImGui::TreePop();
    }

    ImGui::PopID();
}
} // namespace

void ash::ed_hierarchy_init()
{
    SCOPED_CPU_EVENT(L"ash::ed_hierarchy_init");
}

void ash::ed_hierarchy_render()
{
    SCOPED_CPU_EVENT(L"ash::ed_hierarchy_render");
    if (!ed_hierarchy_g_is_open)
        return;

    ImGui::Begin(ICON_MS_SORT " Hierarchy ###Hierarchy", &ed_hierarchy_g_is_open);

    pending_create_request create_request = {};


    if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem(ICON_MS_ADD " Create Empty"))
        {
            create_request.requested = true;
            create_request.parent = flecs::entity::null();
        }
        ImGui::EndPopup();
    }

    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Hideable;
    const float table_width = ImGui::GetContentRegionAvail().x;
    const bool compact = table_width < 320.0f;

    if (ImGui::BeginTable("HierarchyTable", 3, table_flags))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide, 1.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetColumnEnabled(2, !compact);
        ImGui::TableHeadersRow();

        scene_g_world.children([&create_request](flecs::entity entity) {
            if (entity.has<game_object>())
            {
                draw_entity_row(entity, create_request);
            }
        });

        ImGui::EndTable();
    }

    if (create_request.requested)
    {
        ed_console_log(ed_console_log_level::info, "[Hierarchy] Create Empty requested.");
        scene_create_empty(create_request.parent);
    }

    ImGui::End();
}
