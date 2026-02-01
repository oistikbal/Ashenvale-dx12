#include "shader.h"
#include "renderer/core/device.h"
#include "shader_compiler.h"

using namespace winrt;

namespace
{
void init_shader(ash::renderer::pipeline::shader::shader &shader, const wchar_t *file, const wchar_t *entryPoint,
                 const wchar_t *target)
{
    com_ptr<IDxcBlobUtf8> error_blob;
    com_ptr<IDxcResult> result;

    com_ptr<ID3D12ShaderReflection> reflector;
    com_ptr<IDxcBlob> reflection_blob;

    ash::renderer::pipeline::shader_compiler::compile(file, entryPoint, target, result.put(), error_blob.put());

    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader.blob.put()), nullptr);
    result->GetOutput(DXC_OUT_ROOT_SIGNATURE, IID_PPV_ARGS(shader.root_blob.put()), nullptr);
    result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(reflection_blob.put()), nullptr);

    DxcBuffer reflectionBuffer{
        .Ptr = reflection_blob->GetBufferPointer(),
        .Size = reflection_blob->GetBufferSize(),
        .Encoding = 0,
    };

    ash::renderer::pipeline::shader_compiler::g_utils->CreateReflection(&reflectionBuffer,
                                                                        IID_PPV_ARGS(reflector.put()));

    shader.input_layout = ash::renderer::pipeline::shader_compiler::get_input_layout(reflector.get());
    shader.bindings = ash::renderer::pipeline::shader_compiler::get_bindings(reflector.get());
}
} // namespace

void ash::renderer::pipeline::shader::init()
{
    init_shader(g_triangle_vs, L"triangle.hlsl", L"vs_main", L"vs_6_8");
    init_shader(g_triangle_ps, L"triangle.hlsl", L"ps_main", L"ps_6_8");
}