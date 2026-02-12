#pragma once

#include "common.h"
#include "shader.h"
#include <d3d12shader.h>
#include <dxcapi.h>

namespace ash
{
inline winrt::com_ptr<IDxcCompiler3> rhi_sc_g_compiler;
inline winrt::com_ptr<IDxcUtils> rhi_sc_g_utils;
} // namespace ash

namespace ash
{
void rhi_sc_init();
HRESULT rhi_sc_compile(const wchar_t *file, const wchar_t *entryPoint, const wchar_t *target, IDxcResult **result,
                       IDxcBlobUtf8 **error_blob);
std::vector<D3D12_INPUT_ELEMENT_DESC> rhi_sc_get_input_layout(ID3D12ShaderReflection *reflection);
std::vector<rhi_sh_resource_binding> rhi_sc_get_bindings(ID3D12ShaderReflection *reflection);
} // namespace ash
