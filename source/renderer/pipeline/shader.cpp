#include "shader.h"
#include "shader_compiler.h"
#include <d3dcompiler.h>

using namespace winrt;

void ash::renderer::pipeline::shader::init()
{
    com_ptr<IDxcBlobUtf8> error_blob;
    com_ptr<ID3D12ShaderReflection> reflector;

    shader_compiler::compile(L"triangle.hlsl", L"vs_main", L"vs_6_8", g_triangle_vs.blob.put(), reflector.put(),
                             error_blob.put());

    g_triangle_vs.input_layout = shader_compiler::get_input_layout(reflector.get());
    g_triangle_vs.bindings = shader_compiler::get_bindings(reflector.get());

    shader_compiler::compile(L"triangle.hlsl", L"ps_main", L"ps_6_8", g_triangle_ps.blob.put(), reflector.put(),
                             error_blob.put());

    g_triangle_ps.input_layout = shader_compiler::get_input_layout(reflector.get());
    g_triangle_ps.bindings = shader_compiler::get_bindings(reflector.get());
}