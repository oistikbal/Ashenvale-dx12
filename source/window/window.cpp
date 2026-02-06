#include "window/window.h"
#include "editor/editor.h"
#include "renderer/core/swapchain.h"
#include "renderer/renderer.h"
#include <imgui/imgui_impl_win32.h>

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void ash::window::init(HINSTANCE hInstance, PWSTR pCmdLine, int nCmdShow)
{
    SCOPED_CPU_EVENT(L"ash::window::init");
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "Ashenvale";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    g_hwnd = CreateWindowEx(0, wc.lpszClassName, "Ashenvale", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            screenWidth, screenHeight, nullptr, nullptr, wc.hInstance, nullptr);

    assert(g_hwnd);

    ShowWindow(g_hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(g_hwnd);
    renderer::init();
    editor::init();
}

void ash::window::run()
{
    MSG msg = {};

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        SCOPED_CPU_EVENT(L"ash::window::run");
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

    switch (uMsg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
        break;

    case WM_DESTROY:
        ash::renderer::stop();
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        ash::window::event::push(ash::window::g_queue, {ash::window::event::windows_event::resize});
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
