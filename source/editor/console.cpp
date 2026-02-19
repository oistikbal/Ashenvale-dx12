#include "console.h"
#include "IconsMaterialSymbols.h"
#include "common.h"
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <imgui/imgui.h>
#include <mutex>
#include <sstream>
#include <string>

namespace
{
struct console_entry
{
    ash::ed_console_log_level level = ash::ed_console_log_level::info;
    std::string timestamp;
    std::string thread_name;
    std::string message;
};

std::mutex g_console_mutex;
std::deque<console_entry> g_console_entries;
bool g_auto_scroll = true;
constexpr size_t g_max_entries = 2000;

std::uint8_t g_level_filter_mask = 0b111; // info | warning | error

std::uint8_t level_to_mask(const ash::ed_console_log_level level)
{
    switch (level)
    {
    case ash::ed_console_log_level::info:
        return 1u << 0;
    case ash::ed_console_log_level::warning:
        return 1u << 1;
    case ash::ed_console_log_level::error:
        return 1u << 2;
    default:
        return 0;
    }
}

std::string make_timestamp()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto now_time_t = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm local_tm = {};
    localtime_s(&local_tm, &now_time_t);

    char buffer[32] = {};
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d", local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec,
             static_cast<int>(ms.count()));
    return std::string(buffer);
}

std::string get_current_thread_name()
{
    PWSTR thread_desc = nullptr;
    const HRESULT hr = GetThreadDescription(GetCurrentThread(), &thread_desc);
    if (SUCCEEDED(hr) && thread_desc != nullptr && thread_desc[0] != L'\0')
    {
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, thread_desc, -1, nullptr, 0, nullptr, nullptr);
        if (utf8_len > 1)
        {
            std::string utf8_name(static_cast<size_t>(utf8_len - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, thread_desc, -1, utf8_name.data(), utf8_len - 1, nullptr, nullptr);
            LocalFree(thread_desc);
            return utf8_name;
        }
        LocalFree(thread_desc);
    }

    std::ostringstream oss;
    oss << "Thread-" << GetCurrentThreadId();
    return oss.str();
}
} // namespace

void ash::ed_console_init()
{
    SCOPED_CPU_EVENT(L"ash::ed_console_init");
}

void ash::ed_console_log(ed_console_log_level level, std::string_view message)
{
    std::lock_guard<std::mutex> lock(g_console_mutex);

    g_console_entries.push_back({level, make_timestamp(), get_current_thread_name(), std::string(message)});
    if (g_console_entries.size() > g_max_entries)
    {
        g_console_entries.pop_front();
    }
}

void ash::ed_console_clear()
{
    std::lock_guard<std::mutex> lock(g_console_mutex);
    g_console_entries.clear();
}

void ash::ed_console_render()
{
    SCOPED_CPU_EVENT(L"ash::ed_console_render");
    if (!ed_console_g_is_open)
    {
        return;
    }

    ImGui::Begin(ICON_MS_TERMINAL " Console ###Console", &ed_console_g_is_open);

    if (ImGui::Button("Clear"))
    {
        ed_console_clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &g_auto_scroll);
    const float filters_width = 200.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - filters_width);
    bool info_enabled = (g_level_filter_mask & (1u << 0)) != 0;
    bool warn_enabled = (g_level_filter_mask & (1u << 1)) != 0;
    bool error_enabled = (g_level_filter_mask & (1u << 2)) != 0;

    if (ImGui::Checkbox("Info", &info_enabled))
    {
        if (info_enabled)
            g_level_filter_mask |= (1u << 0);
        else
            g_level_filter_mask &= ~(1u << 0);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Warning", &warn_enabled))
    {
        if (warn_enabled)
            g_level_filter_mask |= (1u << 1);
        else
            g_level_filter_mask &= ~(1u << 1);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Error", &error_enabled))
    {
        if (error_enabled)
            g_level_filter_mask |= (1u << 2);
        else
            g_level_filter_mask &= ~(1u << 2);
    }
    ImGui::Separator();

    if (ImGui::BeginChild("ConsoleLogRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
    {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        const ImVec4 row_even = ImVec4(0.15f, 0.15f, 0.15f, 0.55f);
        const ImVec4 row_odd = ImVec4(0.11f, 0.11f, 0.11f, 0.55f);
        const float row_height = ImGui::GetTextLineHeightWithSpacing() + 6.0f;

        for (size_t i = 0; i < g_console_entries.size(); ++i)
        {
            const auto &entry = g_console_entries[i];
            if ((g_level_filter_mask & level_to_mask(entry.level)) == 0)
            {
                continue;
            }

            const char *icon = ICON_MS_INFO;
            ImVec4 icon_color = ImVec4(0.35f, 0.70f, 0.95f, 1.0f);
            if (entry.level == ash::ed_console_log_level::warning)
            {
                icon = ICON_MS_WARNING;
                icon_color = ImVec4(0.93f, 0.76f, 0.28f, 1.0f);
            }
            else if (entry.level == ash::ed_console_log_level::error)
            {
                icon = ICON_MS_ERROR;
                icon_color = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
            }

            ImGui::PushID(static_cast<int>(i));
            ImVec2 row_start = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##console_row", ImVec2(-FLT_MIN, row_height));
            ImVec2 row_end = ImVec2(row_start.x + ImGui::GetItemRectSize().x, row_start.y + row_height);

            ImU32 bg = ImGui::ColorConvertFloat4ToU32((i % 2 == 0) ? row_even : row_odd);
            ImGui::GetWindowDrawList()->AddRectFilled(row_start, row_end, bg);

            ImGui::SetCursorScreenPos(ImVec2(row_start.x + 6.0f, row_start.y + 3.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, icon_color);
            ImGui::TextUnformatted(icon);
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::TextDisabled("[%s] [%s]", entry.timestamp.c_str(), entry.thread_name.c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(entry.message.c_str());
            ImGui::PopID();
        }

        if (g_auto_scroll && ImGui::GetScrollY() >= (ImGui::GetScrollMaxY() - 8.0f))
        {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}
