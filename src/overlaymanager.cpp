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

#include "overlaymanager.h"

RECT g_SelectedRegion = { 0, 0, 500, 200 };

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void Takoyaki::OverlayManager::Initialize()
{
    InitializeWin32Window();
}

void Takoyaki::OverlayManager::Update(RECT selectionRect, bool isEnabled)
{
    if (isEnabled == m_IsEnabled)
        return;

    m_IsEnabled = isEnabled;
    g_SelectedRegion = selectionRect;

    for (uint32_t i = 0; i < m_NumMonitors; ++i)
    {
        if (m_IsEnabled)
            ShowWindow(m_OverlayHwnds[i], SW_SHOW);
        else
            ShowWindow(m_OverlayHwnds[i], SW_HIDE);

        UpdateWindow(m_OverlayHwnds[i]);
    }
}

void Takoyaki::OverlayManager::Shutdown()
{
    // Delete the bitmap
    DeleteObject(m_hBitmap);

    // Delete the memory DC
    DeleteDC(m_hMemDC);

    // Release the screen DC
    ReleaseDC(NULL, m_hScreenDC);
}

void Takoyaki::OverlayManager::InitializeWin32Window()
{
    HINSTANCE hInst = GetModuleHandle(NULL);
    m_NumMonitors = GetSystemMetrics(SM_CMONITORS);

    for (int i = 0; i < m_NumMonitors; i++)
    {
        m_MonitorInfos[i].cbSize = sizeof(MONITORINFOEX);
        POINT monitorTopLeft = {
            m_MonitorInfos[i].rcMonitor.left,
            m_MonitorInfos[i].rcMonitor.top
        };

        GetMonitorInfo(MonitorFromPoint(monitorTopLeft, MONITOR_DEFAULTTONEAREST), &m_MonitorInfos[i]);
    }

    // Register the window class.
    const wchar_t className[] = L"TakoyakiOverlayWindowClass";

    WNDCLASS wc = {};

    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = className;

    RegisterClass(&wc);

    for (int i = 0; i < m_NumMonitors; i++)
    {
        m_OverlayHwnds[i] = CreateWindowEx(
            WS_EX_TOOLWINDOW,
            className,
            L"Takoyaki Overlay",
            WS_POPUP | WS_VISIBLE,
            m_MonitorInfos[i].rcMonitor.left,
            m_MonitorInfos[i].rcMonitor.top,
            m_MonitorInfos[i].rcMonitor.right - m_MonitorInfos[i].rcMonitor.left,
            m_MonitorInfos[i].rcMonitor.bottom - m_MonitorInfos[i].rcMonitor.top,
            NULL,
            NULL,
            hInst,
            NULL
        );

        SetLayeredWindowAttributes(m_OverlayHwnds[i], RGB(0, 0, 0), 0, LWA_COLORKEY | LWA_ALPHA);
    }
}

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Draw white rectangle outline
        HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
        FrameRect(hdc, &g_SelectedRegion, brush);
        DeleteObject(brush);
        EndPaint(hWnd, &ps);
        break;
    }

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

