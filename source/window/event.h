#pragma once

#include "common.h"
#include <mutex>
#include <queue>

namespace ash::window::event
{
enum class windows_event : uint8_t
{
    resize,
};

struct event
{
    windows_event type;
};

struct event_queue
{
    std::queue<event> queue[2];
    std::atomic<uint8_t> index = 0;
};

inline void swap_buffers(event_queue &eq)
{
    eq.index.store(eq.index.load(std::memory_order_acquire) ^ 1, std::memory_order_release);
}

inline void push(event_queue &eq, const event e)
{;
    eq.queue[eq.index.load(std::memory_order_relaxed)].push(e);
}

inline std::queue<event>& get_back_buffer(event_queue &eq)
{
    return eq.queue[eq.index.load(std::memory_order_acquire) ^ 1];
}
} // namespace ash::window::event