#include "window/window.h"
#include <filesystem>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    wchar_t buf[1024]{};
    GetModuleFileNameW(nullptr, buf, 1024);
    std::filesystem::path exe = buf;
    SetCurrentDirectoryW(exe.parent_path().c_str());

    SetThreadDescription(GetCurrentThread(), L"Main Thread");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    ash::window::init(hInstance, pCmdLine, nCmdShow);
    ash::window::run();

    return 0;
}
