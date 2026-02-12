#pragma once

#include "common.h"
#include <dxcapi.h>

namespace ash
{
struct rhi_sh_resource_binding
{
    std::string name;
    D3D_SHADER_INPUT_TYPE type;
    uint32_t slot;
    uint32_t bind_index;
};

struct rhi_sh_shader
{
    winrt::com_ptr<IDxcBlob> blob;
    winrt::com_ptr<IDxcBlob> root_blob;
    std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout;
    std::vector<rhi_sh_resource_binding> bindings;
};

inline rhi_sh_shader rhi_sh_g_triangle_vs;
inline rhi_sh_shader rhi_sh_g_triangle_ps;
} // namespace ash

namespace ash
{
void rhi_sh_init();
}


