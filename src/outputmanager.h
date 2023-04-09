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

#include "Tako/includes/api.h"

namespace wrl = Microsoft::WRL;

namespace Takoyaki
{
    class OutputManager
    {
    public:
        OutputManager(HWND appHwnd);
        ~OutputManager() = default;

        void Initialize();
        void Render();

    public:
        HANDLE GetSharedTextureHandle() const;
        Tako::TakoRect GetTargetRect() const;
        void SetTargetRect(Tako::TakoRect rect);
        void SetEnabled(bool isEnabled);

    private:
        void InitializeWin32Window();
        void InitializeD3D11();

        void InitializeDevice();
        void InitializeDxgi();
        void InitializeSwapChain();
        void InitializeSharedTexture();
        void InitializeBackbufferRtv();
        void InitializeSampler();
        void InitializeBlendState();
        void InitializeShaders();

        void ResizeSwapChain();
        void UpdateViewport();
        void UpdateWin32Window();

    private:
        HWND m_AppHwnd;
        HWND m_OutputHwnd;

        wrl::ComPtr<IDXGIAdapter> m_DxgiAdapter;
        wrl::ComPtr<IDXGIFactory2> m_DxgiFactory;
        wrl::ComPtr<IDXGIDevice> m_DxgiDevice;
        wrl::ComPtr<IDXGISwapChain1> m_DxgiSwapChain;
        wrl::ComPtr<IDXGIKeyedMutex> m_KeyMutex;

        wrl::ComPtr<ID3D11Device> m_Device;
        wrl::ComPtr<ID3D11DeviceContext> m_DeviceContext;
        wrl::ComPtr<ID3D11RenderTargetView> m_BackbufferRtv;
        wrl::ComPtr<ID3D11SamplerState> m_Sampler;
        wrl::ComPtr<ID3D11BlendState> m_BlendState;
        wrl::ComPtr<ID3D11VertexShader> m_VertexShader;
        wrl::ComPtr<ID3D11PixelShader> m_PixelShader;
        wrl::ComPtr<ID3D11InputLayout> m_InputLayout;
        wrl::ComPtr<ID3D11Texture2D> m_SharedTexture;

        Tako::TakoRect m_TargetRect;

        bool m_IsEnabled;
    };
}