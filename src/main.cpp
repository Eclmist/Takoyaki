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
#include "outputwindow.h"

#define NOTIFICATION_TRAY_ICON_MSG      (WM_USER + 0x100)
#define NOTIFICATION_TRAY_UID           1
#define IDM_EXIT                        100

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
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

    Tako::Initialize();
    Takoyaki::OutputWindow outputWindow(hwnd, hInstance);
    outputWindow.Initialize();

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

        Tako::UpdateBufferRegion(outputWindow.GetSharedTextureHandle(), 1920, 1080);

        outputWindow.Render();
    }

    // Remove the icon from the system tray
    Shell_NotifyIcon(NIM_DELETE, &nid);

    Tako::Shutdown();
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_USER:
        // Handle system tray messages here
        if (LOWORD(lParam) == WM_RBUTTONUP)
        {
            // Show the context menu
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, IDM_EXIT, L"Exit");
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        }
        break;
    case WM_COMMAND:
        if (lParam == 0 && LOWORD(wParam) == IDM_EXIT)
            PostQuitMessage(0);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}
