#pragma once

#include "common.h"
#include "renderer/pipeline/shader.h"

namespace ash::renderer::pipeline
{
struct pipeline_state
{
    winrt::com_ptr<ID3D12PipelineState> pso;
    winrt::com_ptr<ID3D12RootSignature> root_signature;
};
inline pipeline_state g_triangle;
} // namespace ash::renderer::pipeline::pipeline_stage

namespace ash::renderer::pipeline
{
void init();
}