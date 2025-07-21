#include "shader_compiler.h"
#include "configs/config.h"
#include <d3d12shader.h>
#include <dxcapi.h>
#include <fstream>
#include <vector>

using namespace winrt;

namespace
{
std::vector<uint8_t> load_file(const wchar_t *file_name)
{
    std::ifstream file(file_name, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}
} // namespace

void ash::renderer::pipeline::shader_compiler::init()
{
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(g_compiler.put()));
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(g_utils.put()));

    assert(g_compiler.get());
    assert(g_utils.get());
}

HRESULT ash::renderer::pipeline::shader_compiler::compile(const wchar_t *file, const wchar_t *entryPoint,
                                                          const wchar_t *target, IDxcResult **result,
                                                          IDxcBlobUtf8 **error_blob)
{
    std::wstring full_path = std::wstring(config::SHADER_PATH) + file;
    std::vector<uint8_t> source_data = load_file(full_path.c_str());

    com_ptr<IDxcBlobEncoding> source_blob;
    g_utils->CreateBlobFromPinned(source_data.data(), (UINT32)source_data.size(), CP_UTF8, source_blob.put());

    const wchar_t *args[] = {
        file, L"-E", entryPoint, L"-T", target, L"-Zi", L"-Qembed_debug", L"-Od", L"-Qstrip_reflect",
    };

    DxcBuffer source_buffer = {};
    source_buffer.Ptr = source_blob->GetBufferPointer();
    source_buffer.Size = source_blob->GetBufferSize();
    source_buffer.Encoding = DXC_CP_UTF8;

    g_compiler->Compile(&source_buffer, args, _countof(args), nullptr, IID_PPV_ARGS(result));

    (*result)->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(error_blob), nullptr);
    if (error_blob && (*error_blob)->GetStringLength() > 0)
    {
        OutputDebugStringA((*error_blob)->GetStringPointer());
    }

    HRESULT hrStatus;
    (*result)->GetStatus(&hrStatus);
    assert(SUCCEEDED(hrStatus));

    return hrStatus;
}

std::vector<D3D12_INPUT_ELEMENT_DESC> ash::renderer::pipeline::shader_compiler::get_input_layout(
    ID3D12ShaderReflection *reflection)
{
    assert(reflection);

    D3D12_SHADER_DESC shaderDesc;
    reflection->GetDesc(&shaderDesc);

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

    for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
    {
        D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
        reflection->GetInputParameterDesc(i, &paramDesc);

        D3D12_INPUT_ELEMENT_DESC element = {};
        element.SemanticName = paramDesc.SemanticName;
        element.SemanticIndex = paramDesc.SemanticIndex;
        element.InputSlot = 0;
        element.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        element.InstanceDataStepRate = 0;

        if (paramDesc.Mask == 1)
            element.Format = DXGI_FORMAT_R32_FLOAT;
        else if (paramDesc.Mask <= 3)
            element.Format = DXGI_FORMAT_R32G32_FLOAT;
        else if (paramDesc.Mask <= 7)
            element.Format = DXGI_FORMAT_R32G32B32_FLOAT;
        else if (paramDesc.Mask <= 15)
            element.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        else
            throw std::runtime_error("Unsupported input format");

        inputLayout.push_back(element);
    }

    return inputLayout;
}

std::vector<ash::renderer::pipeline::shader::resource_binding> ash::renderer::pipeline::shader_compiler::get_bindings(
    ID3D12ShaderReflection *reflection)
{
    assert(reflection);

    std::vector<ash::renderer::pipeline::shader::resource_binding> resource_bindings;

    D3D12_SHADER_DESC shaderDesc = {};
    reflection->GetDesc(&shaderDesc);

    for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC desc = {};
        if (SUCCEEDED(reflection->GetResourceBindingDesc(i, &desc)) && desc.Name)
        {
            ash::renderer::pipeline::shader::resource_binding binding;
            binding.name = desc.Name;
            binding.slot = desc.BindPoint;
            binding.bind_index = desc.BindCount;
            binding.type = desc.Type;
            resource_bindings.push_back(binding);
        }
    }

    return resource_bindings;
}
