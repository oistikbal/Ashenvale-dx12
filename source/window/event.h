#pragma once

#include "common.h"
#include <mutex>
#include <queue>

namespace ash
{
enum class win_evt_windows_event : uint8_t
{
    resize,
};

struct win_evt_event
{
    win_evt_windows_event type;
};

struct win_evt_event_queue
{
    std::queue<win_evt_event> queue[2];
    std::atomic<uint8_t> index = 0;
};

inline void win_evt_swap_buffers(win_evt_event_queue &eq)
{
    eq.index.store(eq.index.load(std::memory_order_acquire) ^ 1, std::memory_order_release);
}

inline void win_evt_push(win_evt_event_queue &eq, const win_evt_event e)
{
    eq.queue[eq.index.load(std::memory_order_relaxed)].push(e);
}

inline std::queue<win_evt_event> &win_evt_get_back_buffer(win_evt_event_queue &eq)
{
    return eq.queue[eq.index.load(std::memory_order_acquire) ^ 1];
}
} // namespace ash
