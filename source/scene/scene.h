#pragma once
#include <flecs.h>
#include <scene/component.h>

namespace ash
{
inline flecs::world scene_g_world;
inline flecs::entity scene_g_selected;
}

namespace ash
{
flecs::entity scene_create_empty(flecs::entity parent = {});
void scene_render();
}
