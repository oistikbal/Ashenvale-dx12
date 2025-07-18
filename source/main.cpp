#include "window/window.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    SetThreadDescription(GetCurrentThread(), L"Main Thread");

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    ash::window::init(hInstance, pCmdLine, nCmdShow);
    ash::window::run();

    return 0;
}
