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

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <unordered_map>
#include "Tako/includes/api.h"

namespace wrl = Microsoft::WRL;

namespace Takoyaki
{
    class OverlayManager
    {
    public:
        OverlayManager() = default;
        ~OverlayManager() = default;

        void Initialize();
        void SetEnabled(bool isEnabled);
        void SetSelectionRect(Tako::TakoRect rect);
        void Update();
        void Shutdown();

    public:
        inline bool IsEnabled() const { return m_IsEnabled; }
        inline const std::unordered_map<HWND, MONITORINFOEX>& GetMonitorInfos() const { return m_MonitorInfos; }
        inline const std::unordered_map<HWND, HBITMAP>& GetFullScreenCaptures() const { return m_FullScreenCaptures; }

    private:
        void InitializeWin32Window();
        HBITMAP GetDisplaySnapshot(HWND hWnd, RECT rect);

    private:
        std::unordered_map<HWND, MONITORINFOEX> m_MonitorInfos;
        std::unordered_map<HWND, HBITMAP> m_FullScreenCaptures;

        uint32_t m_NumMonitors = 0;
        bool m_IsEnabled;
    };
}