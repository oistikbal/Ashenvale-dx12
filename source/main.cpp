#include "window/window.h"
#include "editor/console.h"
#include <filesystem>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    ash::ed_console_log(ash::ed_console_log_level::info, "[App] Startup begin.");

    wchar_t buf[1024]{};
    GetModuleFileNameW(nullptr, buf, 1024);
    std::filesystem::path exe = buf;
    SetCurrentDirectoryW(exe.parent_path().c_str());

    SetThreadDescription(GetCurrentThread(), L"Main Thread");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    const HRESULT co_hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(co_hr))
    {
        ash::ed_console_log(ash::ed_console_log_level::error, "[App] COM initialization failed.");
        return 1;
    }
    ash::ed_console_log(ash::ed_console_log_level::info, "[App] COM initialized.");

    ash::win_init(hInstance, pCmdLine, nCmdShow);
    ash::win_run();

    ash::ed_console_log(ash::ed_console_log_level::info, "[App] Shutdown complete.");
    return 0;
}
