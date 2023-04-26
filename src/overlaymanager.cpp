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
#include <wingdi.h>
#pragma comment(lib, "Msimg32.lib")

Tako::TakoRect g_SelectedRegion = { 100, 100, 300, 280 };
Takoyaki::OverlayManager* g_OverlayManager;

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void Takoyaki::OverlayManager::Initialize()
{
    InitializeWin32Window();
    g_OverlayManager = this;
}

void Takoyaki::OverlayManager::SetEnabled(bool isEnabled)
{
    if (isEnabled == m_IsEnabled)
        return;

    m_IsEnabled = isEnabled;

    SetCursor(LoadCursor(NULL, IDC_CROSS));

    for (auto monitor : m_MonitorInfos)
    {
        if (m_IsEnabled)
            ShowWindow(monitor.first, SW_SHOW);
        else
            ShowWindow(monitor.first, SW_HIDE);

        if (m_IsEnabled)
            m_FullScreenCaptures[monitor.first] = g_OverlayManager->GetDisplaySnapshot(monitor.first, monitor.second.rcMonitor);
    }
}

void Takoyaki::OverlayManager::SetSelectionRect(Tako::TakoRect rect)
{
    g_SelectedRegion = rect;
}

void Takoyaki::OverlayManager::Update()
{
    for (auto monitor : m_MonitorInfos)
    {
        InvalidateRect(monitor.first, NULL, TRUE);
    }
}

void Takoyaki::OverlayManager::Shutdown()
{
}

void Takoyaki::OverlayManager::InitializeWin32Window()
{
    HINSTANCE hInst = GetModuleHandle(NULL);
    m_NumMonitors = GetSystemMetrics(SM_CMONITORS);

    // Register the window class.
    const wchar_t className[] = L"TakoyakiOverlayWindowClass";

    WNDCLASS wc = {};

    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = className;
    wc.hbrBackground = NULL;

    RegisterClass(&wc);

    for (int i = 0; i < m_NumMonitors; i++)
    {
        MONITORINFOEX monitorInfo;

        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        POINT monitorTopLeft = {
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top
        };

        GetMonitorInfo(MonitorFromPoint(monitorTopLeft, MONITOR_DEFAULTTONEAREST), &monitorInfo);

        HWND hwnd = CreateWindowEx(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            className,
            L"Takoyaki Overlay",
            WS_POPUP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            NULL,
            NULL,
            hInst,
            NULL
        );

        m_MonitorInfos.insert({ hwnd, monitorInfo });
    }
}

HBITMAP Takoyaki::OverlayManager::GetDisplaySnapshot(HWND hWnd, RECT rect)
{
    if (m_MonitorInfos.find(hWnd) == m_MonitorInfos.end())
        return NULL;

    HDC screenDc = GetDC(hWnd);
    HDC compatibleDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, rect.right - rect.left, rect.bottom - rect.top);
    HGDIOBJ oldBitmap = SelectObject(compatibleDc, bitmap);
    BitBlt(compatibleDc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, screenDc, rect.left, rect.top, SRCCOPY);
    SelectObject(compatibleDc, oldBitmap);
    DeleteDC(compatibleDc);
    ReleaseDC(NULL, screenDc);
    return bitmap;

}

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_OverlayManager == nullptr)
        return DefWindowProc(hWnd, message, wParam, lParam);

    switch (message)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    case WM_PAINT:
    {
        if (!g_OverlayManager->IsEnabled())
            break;

        auto monitorInfos = g_OverlayManager->GetMonitorInfos();
        HWND hWnd;

        for (auto monitor : monitorInfos)
        {
            hWnd = monitor.first;
            break;
        }

        RECT fullScreenRect = monitorInfos.at(hWnd).rcMonitor;
        int screenWidth = fullScreenRect.right - fullScreenRect.left;
        int screenHeight = fullScreenRect.bottom - fullScreenRect.top;

        HRGN selectionRegion = CreateRectRgn(g_SelectedRegion.m_X, g_SelectedRegion.m_Y, g_SelectedRegion.m_X + g_SelectedRegion.m_Width - 1, g_SelectedRegion.m_Y + g_SelectedRegion.m_Height - 1);
        HRGN fullScreenRegion = CreateRectRgn(fullScreenRect.left, fullScreenRect.top, fullScreenRect.right, fullScreenRect.bottom);

        HDC hdc = GetDC(hWnd);

        HDC rtHdc = CreateCompatibleDC(hdc);
        HBITMAP renderTarget = CreateCompatibleBitmap(hdc, screenWidth, screenHeight);
        SelectObject(rtHdc, renderTarget);
        BitBlt(rtHdc, 0, 0, screenWidth, screenHeight, NULL, 0, 0, WHITENESS);

        // Bitblt current snapshot to window
        HDC miscHdc = CreateCompatibleDC(hdc);
        HBITMAP fullScreenSnapshot = g_OverlayManager->GetFullScreenCaptures().at(hWnd);
        SelectObject(miscHdc, fullScreenSnapshot);
        BitBlt(rtHdc, 0, 0, screenWidth, screenHeight, miscHdc, fullScreenRect.left, fullScreenRect.top, SRCCOPY);

        // Darken screen
        HBITMAP darkenTarget = CreateCompatibleBitmap(hdc, screenWidth, screenHeight);
        SelectObject(miscHdc, darkenTarget);
        SelectClipRgn(rtHdc, fullScreenRegion);
        ExtSelectClipRgn(rtHdc, selectionRegion, RGN_DIFF);
        FillRect(miscHdc, &fullScreenRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        DeleteObject(selectionRegion);
        DeleteObject(fullScreenRegion);

        BLENDFUNCTION blend;
        blend.AlphaFormat = 0;
        blend.SourceConstantAlpha = 180;
        blend.BlendFlags = 0;
        blend.BlendOp = AC_SRC_OVER;
        AlphaBlend(rtHdc, 0, 0, screenWidth, screenHeight,
            miscHdc, 0, 0, screenWidth, screenHeight, blend);

        // Draw selection rectangle
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 200, 255));
        SelectObject(rtHdc, hPen);
        SelectObject(rtHdc, GetStockObject(NULL_BRUSH));
        Rectangle(rtHdc, g_SelectedRegion.m_X, g_SelectedRegion.m_Y, g_SelectedRegion.m_X + g_SelectedRegion.m_Width, g_SelectedRegion.m_Y + g_SelectedRegion.m_Height);
        DeleteObject(hPen);

        // copy the off-screen bitmap to the screen in a single operation
        BitBlt(hdc, 0, 0, screenWidth, screenHeight, rtHdc, 0, 0, SRCCOPY);
        
        DeleteObject(renderTarget);
        DeleteObject(darkenTarget);

        DeleteObject(selectionRegion);
        DeleteObject(fullScreenRegion);
        DeleteDC(miscHdc);
        DeleteDC(rtHdc);
        ReleaseDC(hWnd, hdc);
        break;
    }
    case WM_ERASEBKGND:
        return TRUE; // tell Windows that we handled it. (but don't actually draw anything)
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

