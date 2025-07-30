#include "pipeline.h"
#include "renderer/core/device.h"
#include "renderer/core/swapchain.h"
#include "shader.h"

using namespace winrt;

void ash::renderer::pipeline::init()
{
    SCOPED_CPU_EVENT(L"ash::renderer::pipeline::init");
    core::g_device->CreateRootSignature(0, shader::g_triangle_vs.root_blob->GetBufferPointer(),
                                        shader::g_triangle_vs.root_blob->GetBufferSize(),
                                        IID_PPV_ARGS(g_triangle.root_signature.put()));

    SET_OBJECT_NAME(g_triangle.root_signature.get(), L"Triangle Root");

    D3D12_RASTERIZER_DESC raster_desc = {};
    raster_desc.FillMode = D3D12_FILL_MODE_SOLID;
    raster_desc.CullMode = D3D12_CULL_MODE_NONE;
    raster_desc.FrontCounterClockwise = false;

    D3D12_RENDER_TARGET_BLEND_DESC defaultBlend = {};
    defaultBlend.BlendEnable = FALSE;
    defaultBlend.LogicOpEnable = FALSE;
    defaultBlend.SrcBlend = D3D12_BLEND_ONE;
    defaultBlend.DestBlend = D3D12_BLEND_ZERO;
    defaultBlend.BlendOp = D3D12_BLEND_OP_ADD;
    defaultBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
    defaultBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
    defaultBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    defaultBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
    defaultBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_BLEND_DESC blend_desc = {};
    blend_desc.AlphaToCoverageEnable = FALSE;
    blend_desc.IndependentBlendEnable = FALSE;
    blend_desc.RenderTarget[0] = defaultBlend;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = g_triangle.root_signature.get();
    psoDesc.VS = {shader::g_triangle_vs.blob->GetBufferPointer(), shader::g_triangle_vs.blob->GetBufferSize()};
    psoDesc.PS = {shader::g_triangle_ps.blob->GetBufferPointer(), shader::g_triangle_ps.blob->GetBufferSize()};

    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = core::swapchain::g_format;
    psoDesc.RasterizerState = raster_desc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.BlendState = blend_desc;

    core::g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(g_triangle.pso.put()));
    SET_OBJECT_NAME(g_triangle.pso.get(), L"Triangle PSO");
}