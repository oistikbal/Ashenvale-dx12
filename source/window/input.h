#pragma once

#include "common.h"
#include <atomic>

namespace ash
{
struct win_input
{
    struct input_state
    {
        bool keyboard[256];
        bool mouse_button[3];
        int mouse_pos[2];
        float mouse_delta_pos[2];
    };

    static constexpr uint8_t invalid_index = 0xFF;

    input_state input_states[3]{};
    std::atomic<uint8_t> published_index = 0;
    std::atomic<uint8_t> reader_index = invalid_index;
    uint8_t writer_index = 1;
};

inline win_input g_win_input;
} // namespace ash

namespace ash
{
inline uint8_t win_input_find_next_writer_index(const win_input &input)
{
    const uint8_t published = input.published_index.load(std::memory_order_acquire);
    const uint8_t reading = input.reader_index.load(std::memory_order_acquire);

    for (uint8_t i = 0; i < 3; ++i)
    {
        if (i != published && i != reading)
        {
            return i;
        }
    }

    for (uint8_t i = 0; i < 3; ++i)
    {
        if (i != published)
        {
            return i;
        }
    }

    return published;
}

inline win_input::input_state &win_input_get_back_buffer(win_input &input)
{
    return input.input_states[input.writer_index];
}

inline void win_input_swap_buffers(win_input &input)
{
    input.published_index.store(input.writer_index, std::memory_order_release);
    input.writer_index = win_input_find_next_writer_index(input);
}

inline const win_input::input_state &win_input_acquire_front_buffer(win_input &input)
{
    const uint8_t published = input.published_index.load(std::memory_order_acquire);
    input.reader_index.store(published, std::memory_order_release);
    return input.input_states[published];
}

inline void win_input_release_front_buffer(win_input &input)
{
    input.reader_index.store(win_input::invalid_index, std::memory_order_release);
}
} // namespace ash