#include "inspector.h"
#include "IconsMaterialDesign.h"
#include "common.h"
#include <imgui/imgui.h>

void ash::ed_inspector_init()
{
    SCOPED_CPU_EVENT(L"ash::ed_inspector_init");
}

void ash::ed_inspector_render()
{
    SCOPED_CPU_EVENT(L"ash::ed_inspector_render");
    if (!ed_inspector_g_is_open)
        return;

    ImGui::Begin(ICON_MD_INFO_OUTLINE " Inspector ###Inspector", &ed_inspector_g_is_open);
    ImGui::End();
}
