#pragma once

#include "common.h"
#include <d3d12shader.h>
#include <dxcapi.h>
#include "shader.h"

    namespace ash::renderer::pipeline::shader_compiler
    {
    void init();
    HRESULT compile(const wchar_t *file, const wchar_t *entryPoint, const wchar_t *target, IDxcBlob **shader_blob,
                    ID3D12ShaderReflection **reflector, IDxcBlobUtf8 **error_blob);
    std::vector<D3D12_INPUT_ELEMENT_DESC> get_input_layout(ID3D12ShaderReflection *reflection);
    std::vector<shader::resource_binding> get_bindings(ID3D12ShaderReflection *reflection);

    } // namespace ash::renderer::pipeline::shader_compiler