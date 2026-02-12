#include "command_queue.h"
#include <renderer/renderer.h>

namespace
{
void create(D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_QUEUE_PRIORITY priority, ID3D12CommandQueue **queue)
{
    D3D12_COMMAND_QUEUE_DESC qDesc = {};
    qDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    qDesc.NodeMask = 0;
    qDesc.Type = type;
    qDesc.Priority = priority;

    ash::rhi_g_device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(queue));

    assert(*queue);
}
} // namespace

void ash::rhi_cmd_init()
{
    create(D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, rhi_cmd_g_direct.put());
    create(D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, rhi_cmd_g_compute.put());
    create(D3D12_COMMAND_LIST_TYPE_COPY, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, rhi_cmd_g_copy.put());

    SET_OBJECT_NAME(rhi_cmd_g_direct.get(), L"Direct Queue")
    SET_OBJECT_NAME(rhi_cmd_g_compute.get(), L"Compute Queue")
    SET_OBJECT_NAME(rhi_cmd_g_copy.get(), L"Copy Queue")

    rhi_g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             IID_PPV_ARGS(rhi_cmd_g_command_allocator.put()));
    rhi_g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, rhi_cmd_g_command_allocator.get(), nullptr,
                                        IID_PPV_ARGS(rhi_cmd_g_command_list.put()));

    SET_OBJECT_NAME(rhi_cmd_g_command_list.get(), L"Triangle Command List");
}
