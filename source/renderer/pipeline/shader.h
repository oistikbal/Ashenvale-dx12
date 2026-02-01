#pragma once

#include "common.h"
#include <dxcapi.h>

namespace ash::renderer::pipeline::shader
{
struct resource_binding
{
    std::string name;
    D3D_SHADER_INPUT_TYPE type;
    uint32_t slot;
    uint32_t bind_index;
};

struct shader
{
    winrt::com_ptr<IDxcBlob> blob;
    winrt::com_ptr<IDxcBlob> root_blob;
    std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout;
    std::vector<resource_binding> bindings;
};

inline shader g_triangle_vs;
inline shader g_triangle_ps;
} // namespace ash::renderer::pipeline::shader

namespace ash::renderer::pipeline::shader
{
void init();
}