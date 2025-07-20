#include "command_queue.h"
#include "renderer/core/device.h"

namespace
{
void create(D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_QUEUE_PRIORITY priority, ID3D12CommandQueue **queue)
{
    D3D12_COMMAND_QUEUE_DESC qDesc = {};
    qDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    qDesc.NodeMask = 0;
    qDesc.Type = type;
    qDesc.Priority = priority;

    ash::renderer::core::g_device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(queue));

    assert(*queue);
}
} // namespace

void ash::renderer::core::command_queue::init()
{
    create(D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, g_direct.put());
    create(D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, g_compute.put());
    create(D3D12_COMMAND_LIST_TYPE_COPY, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, g_copy.put());

    SET_DX_NAME(g_direct.get(), L"Direct Queue")
    SET_DX_NAME(g_compute.get(), L"Compute Queue")
    SET_DX_NAME(g_copy.get(), L"Copy Queue")

    core::g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(g_command_allocator.put()));
    core::g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_command_allocator.get(), nullptr,
                                      IID_PPV_ARGS(g_command_list.put()));
}