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
#include <uxtheme.h>
#pragma comment (lib, "uxtheme.lib")

Gdiplus::Rect g_SelectedRegion = { 100, 100, 300, 280 };
Takoyaki::OverlayManager* g_OverlayManager;

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void Takoyaki::OverlayManager::Initialize()
{
    InitializeWin32Window();
    InitializeGdiPlus();
    g_OverlayManager = this;
}

void Takoyaki::OverlayManager::SetEnabled(bool isEnabled)
{
    if (isEnabled == m_IsEnabled)
        return;

    m_IsEnabled = isEnabled;

    for (auto monitor : m_MonitorInfos)
    {
        if (m_IsEnabled)
            ShowWindow(monitor.first, SW_SHOW);
        else
            ShowWindow(monitor.first, SW_HIDE);
 
        Gdiplus::Rect fullScreenRect =
        {
            monitor.second.rcMonitor.left,
            monitor.second.rcMonitor.top,
            monitor.second.rcMonitor.right - monitor.second.rcMonitor.left,
            monitor.second.rcMonitor.bottom - monitor.second.rcMonitor.top
        };

        m_FullScreenCaptures[monitor.first] = g_OverlayManager->GetDisplaySnapshot(monitor.first, fullScreenRect);
    }
}

void Takoyaki::OverlayManager::SetSelectionRect(Tako::TakoRect rect)
{
    g_SelectedRegion = {
        (INT)rect.m_X,
        (INT)rect.m_Y,
        (INT)rect.m_Width,
        (INT)rect.m_Height,
    };
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
    GdiplusShutdown(m_GdiplusToken);
}

void Takoyaki::OverlayManager::InitializeGdiPlus()
{
    GdiplusStartup(&m_GdiplusToken, &m_GdiplusStartupInput, NULL);
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
            WS_POPUP | WS_VISIBLE,
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

HBITMAP Takoyaki::OverlayManager::GetDisplaySnapshot(HWND hWnd, Gdiplus::Rect rect)
{
    if (m_MonitorInfos.find(hWnd) == m_MonitorInfos.end())
        return NULL;

    Gdiplus::Size rectSize;
    rect.GetSize(&rectSize);

    HDC screenDc = GetDC(hWnd);
    HDC compatibleDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, rectSize.Width, rectSize.Height);
    HGDIOBJ oldBitmap = SelectObject(compatibleDc, bitmap);
    BitBlt(compatibleDc, 0, 0, rectSize.Width, rectSize.Height, screenDc, rect.GetLeft(), rect.GetTop(), SRCCOPY);
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

        printf("selection:: x: %i, y:%i, x2:%i y2:%i \n", g_SelectedRegion.GetLeft(), g_SelectedRegion.GetTop(), g_SelectedRegion.GetRight(), g_SelectedRegion.GetBottom());

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rc;
        GetClientRect(hWnd, &rc);

        // Start
        HDC memdc;
        auto hbuff = BeginBufferedPaint(hdc, &rc, BPBF_COMPATIBLEBITMAP, NULL, &memdc);
        Gdiplus::Graphics graphics(memdc);

        Gdiplus::Rect fullScreenRect =
        {
            monitorInfos.at(hWnd).rcMonitor.left,
            monitorInfos.at(hWnd).rcMonitor.top,
            monitorInfos.at(hWnd).rcMonitor.right - monitorInfos.at(hWnd).rcMonitor.left,
            monitorInfos.at(hWnd).rcMonitor.bottom - monitorInfos.at(hWnd).rcMonitor.top
        };

        // Get Screen captures
        Gdiplus::Bitmap fullScreenSnapshot(g_OverlayManager->GetFullScreenCaptures().at(hWnd), NULL);

        // Copy Screen
        graphics.DrawImage(&fullScreenSnapshot, fullScreenRect);

        // Darken screen
        SolidBrush semiTransBrush(Color(64, 0, 0, 0));
        Pen pen(Color(200, 0, 200, 255), 2);

        graphics.FillRectangle(&semiTransBrush, fullScreenRect);

        // Draw Selection Rectangle
        graphics.DrawRectangle(&pen, g_SelectedRegion);

        // Restore selected area's brightness
        graphics.DrawImage(&fullScreenSnapshot,
            g_SelectedRegion, 
            g_SelectedRegion.GetLeft(),
            g_SelectedRegion.GetTop(),
            g_SelectedRegion.GetRight() - g_SelectedRegion.GetLeft(),
            g_SelectedRegion.GetBottom() - g_SelectedRegion.GetTop(),
            Gdiplus::UnitPixel);

        // End
        EndBufferedPaint(hbuff, TRUE);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_ERASEBKGND:
        return TRUE; // tell Windows that we handled it. (but don't actually draw anything)
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

