/*
    This file is part of Takoyaki, a custom screen share utility.

    Copyright (c) 2020-2023 Samuel Huang - All rights reserved.

    Takoyaki is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/


#include "resource.h"
#include "outputmanager.h"
#include "overlaymanager.h"

// Coredump for crashes
#include <dbghelp.h>
#include <minidumpapiset.h>
#pragma comment(lib, "dbghelp.lib")

#define NOTIFICATION_TRAY_ICON_MSG      (WM_USER + 0x100)
#define NOTIFICATION_TRAY_UID           1
#define IDM_EXIT                        100
#define IDM_LABEL                       101
#define IDM_STARTCAPTURE                102
#define IDM_STOPCAPTURE                 103

bool g_Enabled = false;
bool g_IsOverlayActive = false;
bool g_IsSelectingRegion = false;

POINT g_StartPoint, g_EndPoint;

Tako::TakoRect g_CaptureRect = {
    .m_X = 0,
    .m_Y = 0,
    .m_Width = 1,
    .m_Height = 1,
};

Tako::TakoRect g_SelectionRect = {
    .m_X = 0,
    .m_Y = 0,
    .m_Width = 1,
    .m_Height = 1,
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstaunce,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    // Prevent multiple instances of Takoyaki
    CreateMutexA(0, false, "Local\\Takoyaki");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBox(nullptr, L"An instance of Takoyaki is already running.", L"Takoyaki", MB_OK | MB_ICONERROR);
        return 0;
    }

#if _DEBUG
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    printf("Takoyaki debug console enabled\n");
#endif

    // Register the window class
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TakoyakiWindowClass";
    HRESULT hr = RegisterClass(&wc);

    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to register window class.", L"Takoyaki Error", MB_OK);
        return 0;
    }

    // Create the window
    HWND hwnd = CreateWindow(L"TakoyakiWindowClass", L"Takoyaki", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, hInstance, NULL);

    if (hwnd == nullptr)
    {
        MessageBox(nullptr, L"Failed to create win32 window.", L"Takoyaki Error", MB_OK);
        return 0;
    }

    HMODULE hModule = GetModuleHandle(NULL);
    HHOOK keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hModule, 0);

    Tako::TakoError err = Tako::Initialize();
    if (err != Tako::TakoError::OK)
    {
        MessageBox(nullptr, L"Failed to initialize Tako.dll.", L"Takoyaki Error", MB_OK);
        return 0;
    }

    Takoyaki::OutputManager outputManager(hwnd);
    Takoyaki::OverlayManager overlayManager;

    outputManager.Initialize();
    overlayManager.Initialize();

    // Add the icon to the system tray
    NOTIFYICONDATA nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    lstrcpy(nid.szTip, L"Takoyaki");
    Shell_NotifyIcon(NIM_ADD, &nid);

    // Run the message loop
    MSG msg = { 0 };

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        overlayManager.SetEnabled(g_IsOverlayActive);
        outputManager.SetEnabled(g_Enabled);

        if (g_IsOverlayActive)
        {
            SetCapture(hwnd); // Grab Mouse Events
            overlayManager.SetSelectionRect(g_SelectionRect);
            overlayManager.Update();
        }
        else
        {
            ReleaseCapture(); // Release Mouse Events
            outputManager.SetTargetRect(g_CaptureRect);

            if (!g_Enabled)
                continue;

            Tako::TakoError err = Tako::CaptureIntoBuffer(outputManager.GetSharedTextureHandle(), g_CaptureRect);
            if (err != Tako::TakoError::OK)
            {
                MessageBox(nullptr, L"Failed to capture display buffer. (Tako.dll)", L"Takoyaki Error", MB_OK);
                break;
            }

            outputManager.Render();
        }
    }

    // Remove the icon from the system tray
    Shell_NotifyIcon(NIM_DELETE, &nid);

    UnhookWindowsHookEx(keyboardHook);

    overlayManager.Shutdown();
    Tako::Shutdown();

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        if (!g_IsOverlayActive)
            break;
        if (g_IsSelectingRegion)
            break;

        GetPhysicalCursorPos(&g_StartPoint);
        g_IsSelectingRegion = true;
        break;

    case WM_MOUSEMOVE:
        if (!g_IsOverlayActive)
            break;

        if (!g_IsSelectingRegion)
            break;


        GetPhysicalCursorPos(&g_EndPoint);

        g_SelectionRect =
        {
            std::min(g_StartPoint.x, g_EndPoint.x),
            std::min(g_StartPoint.y, g_EndPoint.y),
            static_cast<uint32_t>(std::max(1l, std::abs(g_StartPoint.x - g_EndPoint.x))),
            static_cast<uint32_t>(std::max(1l, std::abs(g_StartPoint.y - g_EndPoint.y)))
        };

        break;

    case WM_LBUTTONUP:
        if (!g_IsOverlayActive)
            break;
        if (!g_IsSelectingRegion)
            break;

        g_IsSelectingRegion = false;
        g_IsOverlayActive = false;
        g_CaptureRect = g_SelectionRect;
        g_SelectionRect = { 0, 0, 1, 1 };
        g_Enabled = true;
        break;

    case WM_USER:
        // Handle system tray messages here
        if (LOWORD(lParam) == WM_RBUTTONUP)
        {
            // Show the context menu
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();

            if (g_Enabled)
                AppendMenu(hMenu, MF_GRAYED, IDM_LABEL, L"Status: Capturing");
            else
                AppendMenu(hMenu, MF_GRAYED, IDM_LABEL, L"Status: Stopped");

            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL); // Add separator

            if (g_Enabled)
                AppendMenu(hMenu, MF_STRING, IDM_STOPCAPTURE, L"Stop Capture");
            else
                AppendMenu(hMenu, MF_STRING, IDM_STARTCAPTURE, L"Start Capture");

            AppendMenu(hMenu, MF_STRING, IDM_EXIT, L"Exit");
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        }
        break;
    case WM_COMMAND:
        if (lParam == 0 && LOWORD(wParam) == IDM_EXIT)
        {
            PostQuitMessage(0);
            break;
        }
        else if (lParam == 0 && LOWORD(wParam) == IDM_STARTCAPTURE)
        {
            g_Enabled = true;
            break;
        }
        else if (lParam == 0 && LOWORD(wParam) == IDM_STOPCAPTURE)
        {
            g_Enabled = false;
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

#define X_KEY 0x58
#define ESC_KEY 0x1B

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
    {
        // A key has been pressed. You can process the key event here.
        KBDLLHOOKSTRUCT* pKeyData = (KBDLLHOOKSTRUCT*)lParam;

        // Check if the WIN+SHIFT+X key combination was pressed.
        if (pKeyData->vkCode == X_KEY && (GetKeyState(VK_LWIN) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000))
        {
            if (g_IsOverlayActive)
                return 1;

            g_IsOverlayActive = true;
            return 1;
        }
        else if (pKeyData->vkCode == ESC_KEY)
        {
            if (!g_IsOverlayActive)
                return 1;
            g_IsOverlayActive = false;
            return 1;
        }
    }

    // Pass the message on to the next hook in the chain.
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
