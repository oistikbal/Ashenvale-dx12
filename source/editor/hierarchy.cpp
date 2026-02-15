#include "hierarchy.h"
#include "IconsMaterialDesign.h"
#include "common.h"
#include <imgui/imgui.h>

void ash::ed_hierarchy_init()
{
    SCOPED_CPU_EVENT(L"ash::ed_hierarchy_init");
}

void ash::ed_hierarchy_render()
{
    SCOPED_CPU_EVENT(L"ash::ed_hierarchy_render");
    if (!ed_hierarchy_g_is_open)
        return;

    ImGui::Begin(ICON_MD_VIEW_LIST " Hierarchy ###Hierarchy", &ed_hierarchy_g_is_open);
    ImGui::End();
}
