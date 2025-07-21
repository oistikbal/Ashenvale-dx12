#pragma once

#define __STDC_LIMIT_MACROS

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <assert.h>
#include <cfloat>
#include <cstdint>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <winrt/base.h>

#if _DEBUG
#define SET_DX_NAME(object, name) object->SetName(name);
#else
#define SET_DX_NAME(object, name) ((void)0);
#endif

#define USE_PIX
#include <WinPixEventRuntime/pix3.h>

#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)

#define SCOPED_CPU_EVENT(label) pix_cpu_event CONCAT(pix_event_, __COUNTER__)(label);
#define SCOPED_GPU_EVENT(cmd, label) pix_gpu_event CONCAT(pix_event_, __COUNTER__)(cmd, label);

struct pix_cpu_event
{
    pix_cpu_event(const wchar_t *label)
    {
        PIXBeginEvent(0, label);
    }

    ~pix_cpu_event()
    {
        PIXEndEvent();
    }
};

struct pix_gpu_event
{
    pix_gpu_event(ID3D12GraphicsCommandList *cmd_list, const wchar_t *label) : cmd_list(cmd_list)
    {
        PIXBeginEvent(cmd_list, 0, label);
    }

    pix_gpu_event(ID3D12CommandQueue *cmd_queue, const wchar_t *label) : cmd_queue(cmd_queue)
    {
        PIXBeginEvent(cmd_queue, 0, label);
    }

    ~pix_gpu_event()
    {
        PIXEndEvent(cmd_queue);
    }

  private:
    union {
        ID3D12GraphicsCommandList *cmd_list;
        ID3D12CommandQueue *cmd_queue;
    };
};
