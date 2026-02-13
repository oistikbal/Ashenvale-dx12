#include "shader_compiler.h"
#include "configs/config.h"
#include <d3d12shader.h>
#include <dxcapi.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <windows.h>

using namespace winrt;

namespace
{
std::vector<uint8_t> load_file(const wchar_t *file_name)
{
    std::ifstream file(file_name, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

using DxcCreateInstanceProc = HRESULT(WINAPI *)(REFCLSID, REFIID, LPVOID *);

HRESULT dxc_create_instance_from_exe_dir(REFCLSID clsid, REFIID iid, void **out)
{
    static HMODULE dxc_module = nullptr;
    static DxcCreateInstanceProc dxc_create_instance = nullptr;

    if (!dxc_module)
    {
        wchar_t exe_path_buf[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe_path_buf, MAX_PATH);
        std::filesystem::path exe_path = exe_path_buf;
        std::filesystem::path exe_dir = exe_path.parent_path();

        std::filesystem::path dxc_path = exe_dir / L"dxcompiler.dll";
        dxc_module = LoadLibraryW(dxc_path.c_str());

        if (!dxc_module)
        {
            dxc_module = LoadLibraryW(L"dxcompiler.dll");
        }

        if (!dxc_module)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        std::filesystem::path dxil_path = exe_dir / L"dxil.dll";
        if (!GetModuleHandleW(L"dxil.dll"))
        {
            LoadLibraryW(dxil_path.c_str());
        }

        dxc_create_instance =
            reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(dxc_module, "DxcCreateInstance"));
        if (!dxc_create_instance)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }

    return dxc_create_instance(clsid, iid, out);
}
} // namespace

void ash::rhi_sc_init()
{
    dxc_create_instance_from_exe_dir(CLSID_DxcCompiler, IID_PPV_ARGS(rhi_sc_g_compiler.put()));
    dxc_create_instance_from_exe_dir(CLSID_DxcUtils, IID_PPV_ARGS(rhi_sc_g_utils.put()));

    assert(rhi_sc_g_compiler.get());
    assert(rhi_sc_g_utils.get());
}

HRESULT ash::rhi_sc_compile(const wchar_t *file, const wchar_t *entryPoint, const wchar_t *target, IDxcResult **result,
                            IDxcBlobUtf8 **error_blob)
{
    std::wstring full_path = std::wstring(cfg_SHADER_PATH) + file;
    std::vector<uint8_t> source_data = load_file(full_path.c_str());

    com_ptr<IDxcBlobEncoding> source_blob;
    rhi_sc_g_utils->CreateBlobFromPinned(source_data.data(), (UINT32)source_data.size(), CP_UTF8, source_blob.put());

    const wchar_t *args[] = {
        file, L"-E", entryPoint, L"-T", target, L"-Zi", L"-Qembed_debug", L"-Od", L"-Qstrip_reflect",
    };

    DxcBuffer source_buffer = {};
    source_buffer.Ptr = source_blob->GetBufferPointer();
    source_buffer.Size = source_blob->GetBufferSize();
    source_buffer.Encoding = DXC_CP_UTF8;

    rhi_sc_g_compiler->Compile(&source_buffer, args, _countof(args), nullptr, IID_PPV_ARGS(result));

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

std::vector<D3D12_INPUT_ELEMENT_DESC> ash::rhi_sc_get_input_layout(ID3D12ShaderReflection *reflection)
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

std::vector<ash::rhi_sh_resource_binding> ash::rhi_sc_get_bindings(ID3D12ShaderReflection *reflection)
{
    assert(reflection);

    std::vector<ash::rhi_sh_resource_binding> resource_bindings;

    D3D12_SHADER_DESC shaderDesc = {};
    reflection->GetDesc(&shaderDesc);

    for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC desc = {};
        if (SUCCEEDED(reflection->GetResourceBindingDesc(i, &desc)) && desc.Name)
        {
            ash::rhi_sh_resource_binding binding;
            binding.name = desc.Name;
            binding.slot = desc.BindPoint;
            binding.bind_index = desc.BindCount;
            binding.type = desc.Type;
            resource_bindings.push_back(binding);
        }
    }

    return resource_bindings;
}
