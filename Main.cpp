#include <iostream>

#include "pch.h"

#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

// ImGui
#include "imgui.h"

using Microsoft::WRL::ComPtr;

extern "C"
{
    // Used to enable the "Agility SDK" components
    __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

//把 Windows 的 event 傳給 imgui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    constexpr UINT g_frameCount = 2;

    HWND g_hwnd = nullptr;
    UINT g_width = 1280;
    UINT g_height = 720;

    ComPtr<ID3D12Device> g_device;
    ComPtr<ID3D12CommandQueue> g_commandQueue;
    ComPtr<IDXGISwapChain3> g_swapChain;
    ComPtr<ID3D12DescriptorHeap> g_descriptorHeap;
    UINT g_rtvDescriptorSize = 0;

    ComPtr<ID3D12Resource> g_renderTargets[g_frameCount];
    ComPtr<ID3D12CommandAllocator> g_commandAllocators[g_frameCount];
    ComPtr<ID3D12GraphicsCommandList> g_commandList;

    ComPtr<ID3D12Fence> g_fence;
    UINT g_frameIndex = 0;
    UINT g_fenceValues[g_frameCount] = {};
    HANDLE g_fenceEvent = nullptr;

    ComPtr<ID3D12RootSignature> g_rootSignature;
    ComPtr<ID3D12PipelineState> g_pipelineState;

    ComPtr<ID3D12DescriptorHeap> g_imguiSrvHeap;

    float g_clearColor[4] = {0.08f, 0.1f, 0.14f, 1.0f};
    bool g_showDemo = false;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            default:
            break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}
void WaitForGpu() {
    const UINT64 fenceToWaitFor = g_fenceValues[g_frameCount];
    DX::ThrowIfFailed(g_commandQueue->Signal(g_fence.Get(), fenceToWaitFor));
    DX::ThrowIfFailed(g_fence->SetEventOnCompletion(fenceToWaitFor, g_fenceEvent));
    WaitForSingleObject(g_fenceEvent, INFINITE);
    ++g_fenceValues[g_frameIndex];
}

void MoveToNextFrame() {
    const UINT64 currentFenceValue = g_fenceValues[g_frameIndex];
    DX::ThrowIfFailed(g_commandQueue->Signal(g_fence.Get(), currentFenceValue));

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    if (g_fence->GetCompletedValue() < g_fenceValues[g_frameIndex]) {
        DX::ThrowIfFailed(g_fence->SetEventOnCompletion(g_fenceValues[g_frameIndex], g_fenceEvent));
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }

    g_fenceValues[g_frameIndex] = currentFenceValue + 1;
}

void InitWindow(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ImGui";

    if (!RegisterClassExW(&wc))
        throw std::runtime_error("RegisterClassExW failed");

    RECT rc = {0, 0, static_cast<LONG>(g_width), static_cast<LONG>(g_height)};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, 0);

    g_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"DX12 + Imgui",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
        );

    if (!g_hwnd)
        throw std::runtime_error("CreateWindow failed");

    ShowWindow(g_hwnd, nCmdShow);
}



// //
// // Main.cpp
// //
//
// #include "pch.h"
// #include "Game.h"
//
// using namespace DirectX;
//
// #ifdef __clang__
// #pragma clang diagnostic ignored "-Wcovered-switch-default"
// #pragma clang diagnostic ignored "-Wswitch-enum"
// #endif
//
// #pragma warning(disable : 4061)
//
// extern "C"
// {
//     // Used to enable the "Agility SDK" components
//     __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
//     __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
// }
//
// namespace
// {
//     std::unique_ptr<Game> g_game;
// }
//
// LPCWSTR g_szAppName = L"DirectXTest";
//
// LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
// void ExitGame() noexcept;
//
// // Entry point
// int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
// {
//     UNREFERENCED_PARAMETER(hPrevInstance);
//     UNREFERENCED_PARAMETER(lpCmdLine);
//
//     if (!XMVerifyCPUSupport())
//         return 1;
//
// #ifdef __MINGW32__
//     if (FAILED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED)))
//         return 1;
// #else
//     if (FAILED(RoInitialize(RO_INIT_MULTITHREADED)))
//         return 1;
// #endif
//
//     g_game = std::make_unique<Game>();
//
//     // Register class and create window
//     {
//         // Register class
//         WNDCLASSEXW wcex = {};
//         wcex.cbSize = sizeof(WNDCLASSEXW);
//         wcex.style = CS_HREDRAW | CS_VREDRAW;
//         wcex.lpfnWndProc = WndProc;
//         wcex.hInstance = hInstance;
//         wcex.hIcon = LoadIconW(hInstance, L"IDI_ICON");
//         wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
//         wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
//         wcex.lpszClassName = L"DirectXTestWindowClass";
//         wcex.hIconSm = LoadIconW(wcex.hInstance, L"IDI_ICON");
//         if (!RegisterClassExW(&wcex))
//             return 1;
//
//         // Create window
//         int w, h;
//         g_game->GetDefaultSize(w, h);
//
//         RECT rc = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };
//
//         AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
//
//         HWND hwnd = CreateWindowExW(0, L"DirectXTestWindowClass", g_szAppName, WS_OVERLAPPEDWINDOW,
//             CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
//             nullptr, nullptr, hInstance,
//             g_game.get());
//         // TODO: Change to CreateWindowExW(WS_EX_TOPMOST, L"DirectXTestWindowClass", g_szAppName, WS_POPUP,
//         // to default to fullscreen.
//
//         if (!hwnd)
//             return 1;
//
//         ShowWindow(hwnd, nCmdShow);
//         // TODO: Change nCmdShow to SW_SHOWMAXIMIZED to default to fullscreen.
//
//         GetClientRect(hwnd, &rc);
//
//         g_game->Initialize(hwnd, rc.right - rc.left, rc.bottom - rc.top);
//     }
//
//     // Main message loop
//     MSG msg = {};
//     while (WM_QUIT != msg.message)
//     {
//         if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
//         {
//             TranslateMessage(&msg);
//             DispatchMessage(&msg);
//         }
//         else
//         {
//             g_game->Tick();
//         }
//     }
//
//     g_game.reset();
//
//     return static_cast<int>(msg.wParam);
// }
//
// // Windows procedure
// LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
// {
//     static bool s_in_sizemove = false;
//     static bool s_in_suspend = false;
//     static bool s_minimized = false;
//     static bool s_fullscreen = false;
//     // TODO: Set s_fullscreen to true if defaulting to fullscreen.
//
//     auto game = reinterpret_cast<Game*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
//
//     switch (message)
//     {
//     case WM_CREATE:
//         if (lParam)
//         {
//             auto params = reinterpret_cast<LPCREATESTRUCTW>(lParam);
//             SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(params->lpCreateParams));
//         }
//         break;
//
//     case WM_PAINT:
//         if (s_in_sizemove && game)
//         {
//             game->Tick();
//         }
//         else
//         {
//             PAINTSTRUCT ps;
//             std::ignore = BeginPaint(hWnd, &ps);
//             EndPaint(hWnd, &ps);
//         }
//         break;
//
//     case WM_DISPLAYCHANGE:
//         if (game)
//         {
//             game->OnDisplayChange();
//         }
//         break;
//
//     case WM_MOVE:
//         if (game)
//         {
//             game->OnWindowMoved();
//         }
//         break;
//
//     case WM_SIZE:
//         if (wParam == SIZE_MINIMIZED)
//         {
//             if (!s_minimized)
//             {
//                 s_minimized = true;
//                 if (!s_in_suspend && game)
//                     game->OnSuspending();
//                 s_in_suspend = true;
//             }
//         }
//         else if (s_minimized)
//         {
//             s_minimized = false;
//             if (s_in_suspend && game)
//                 game->OnResuming();
//             s_in_suspend = false;
//         }
//         else if (!s_in_sizemove && game)
//         {
//             game->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam));
//         }
//         break;
//
//     case WM_ENTERSIZEMOVE:
//         s_in_sizemove = true;
//         break;
//
//     case WM_EXITSIZEMOVE:
//         s_in_sizemove = false;
//         if (game)
//         {
//             RECT rc;
//             GetClientRect(hWnd, &rc);
//
//             game->OnWindowSizeChanged(rc.right - rc.left, rc.bottom - rc.top);
//         }
//         break;
//
//     case WM_GETMINMAXINFO:
//         if (lParam)
//         {
//             auto info = reinterpret_cast<MINMAXINFO*>(lParam);
//             info->ptMinTrackSize.x = 320;
//             info->ptMinTrackSize.y = 200;
//         }
//         break;
//
//     case WM_ACTIVATEAPP:
//         if (game)
//         {
//             if (wParam)
//             {
//                 game->OnActivated();
//             }
//             else
//             {
//                 game->OnDeactivated();
//             }
//         }
//         break;
//
//     case WM_POWERBROADCAST:
//         switch (wParam)
//         {
//         case PBT_APMQUERYSUSPEND:
//             if (!s_in_suspend && game)
//                 game->OnSuspending();
//             s_in_suspend = true;
//             return TRUE;
//
//         case PBT_APMRESUMESUSPEND:
//             if (!s_minimized)
//             {
//                 if (s_in_suspend && game)
//                     game->OnResuming();
//                 s_in_suspend = false;
//             }
//             return TRUE;
//
//         default:
//             break;
//         }
//         break;
//
//     case WM_DESTROY:
//         PostQuitMessage(0);
//         break;
//
//     case WM_SYSKEYDOWN:
//         if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
//         {
//             // Implements the classic ALT+ENTER fullscreen toggle
//             if (s_fullscreen)
//             {
//                 SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
//                 SetWindowLongPtr(hWnd, GWL_EXSTYLE, 0);
//
//                 int width = 800;
//                 int height = 600;
//                 if (game)
//                     game->GetDefaultSize(width, height);
//
//                 ShowWindow(hWnd, SW_SHOWNORMAL);
//
//                 SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
//             }
//             else
//             {
//                 SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP);
//                 SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);
//
//                 SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
//
//                 ShowWindow(hWnd, SW_SHOWMAXIMIZED);
//             }
//
//             s_fullscreen = !s_fullscreen;
//         }
//         break;
//
//     case WM_MENUCHAR:
//         // A menu is active and the user presses a key that does not correspond
//         // to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
//         return MAKELRESULT(0, MNC_CLOSE);
//
//     default:
//         break;
//     }
//
//     return DefWindowProc(hWnd, message, wParam, lParam);
// }
//
// // Exit helper
// void ExitGame() noexcept
// {
//     PostQuitMessage(0);
// }
