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

#include "outputmanager.h"
#include <process.h>
#include "data/vs.h"
#include "data/ps.h"

LRESULT CALLBACK OutputWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern bool g_Enabled;

Takoyaki::OutputManager::OutputManager()
    : m_OutputHwnd(nullptr)
{
    m_TargetRect = {
        .m_Width = 1920,
        .m_Height = 1080
    };
}

void Takoyaki::OutputManager::Initialize()
{
    InitializeWin32Window();
    InitializeGraphicsApi();
}

void Takoyaki::OutputManager::Render()
{
    while (true)
    {
        HRESULT hr = m_KeyMutex->AcquireSync(0, 100);
        if (hr == static_cast<HRESULT>(WAIT_TIMEOUT))
            continue;

        if (FAILED(hr))
        {
            MessageBox(nullptr, L"Failed to acquire sync on key mutex.", L"Takoyaki Error", MB_OK);
            exit(0);
        }

        break;
    }

    // Vertices for drawing whole texture
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT2 TexCoord;

    struct Vertex {
        DirectX::XMFLOAT3 Pos;
        DirectX::XMFLOAT2 TexCoord;
    };

    static constexpr uint32_t NumVertices = 6;
    Vertex vertices[NumVertices] =
    {
        { DirectX::XMFLOAT3(-1.0f, -1.0f, 0), DirectX::XMFLOAT2(0.0f, 1.0f) },
        { DirectX::XMFLOAT3(-1.0f, 1.0f, 0), DirectX::XMFLOAT2(0.0f, 0.0f) },
        { DirectX::XMFLOAT3(1.0f, -1.0f, 0), DirectX::XMFLOAT2(1.0f, 1.0f) },
        { DirectX::XMFLOAT3(1.0f, -1.0f, 0), DirectX::XMFLOAT2(1.0f, 1.0f) },
        { DirectX::XMFLOAT3(-1.0f, 1.0f, 0), DirectX::XMFLOAT2(0.0f, 0.0f) },
        { DirectX::XMFLOAT3(1.0f, 1.0f, 0), DirectX::XMFLOAT2(1.0f, 0.0f) },
    };

    D3D11_TEXTURE2D_DESC sharedTextureDesc;
    m_SharedTexture->GetDesc(&sharedTextureDesc);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = sharedTextureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = sharedTextureDesc.MipLevels - 1;
    srvDesc.Texture2D.MipLevels = sharedTextureDesc.MipLevels;

    // Create new shader resource view
    ID3D11ShaderResourceView* shaderResource = nullptr;
    HRESULT hr = m_GfxContext.GetDevice()->CreateShaderResourceView(m_SharedTexture.Get(), &srvDesc, &shaderResource);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create SRV while rendering frame.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    m_GfxContext.GetDeviceContext()->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    m_GfxContext.GetDeviceContext()->OMSetRenderTargets(1, m_BackbufferRtv.GetAddressOf(), nullptr);
    m_GfxContext.GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
    m_GfxContext.GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
    m_GfxContext.GetDeviceContext()->PSSetShaderResources(0, 1, &shaderResource);
    m_GfxContext.GetDeviceContext()->PSSetSamplers(0, 1, m_Sampler.GetAddressOf());
    m_GfxContext.GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_BUFFER_DESC bufferDesc;
    RtlZeroMemory(&bufferDesc, sizeof(bufferDesc));
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(Vertex) * NumVertices;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA initData;
    RtlZeroMemory(&initData, sizeof(initData));
    initData.pSysMem = vertices;

    // Create vertex buffer
    ID3D11Buffer* vertexBuffer = nullptr;
    hr = m_GfxContext.GetDevice()->CreateBuffer(&bufferDesc, &initData, &vertexBuffer);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create vertex buffer while rendering frame.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
    m_GfxContext.GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    // Draw textured quad onto render target
    m_GfxContext.GetDeviceContext()->Draw(NumVertices, 0);

    // Release keyed mutex
    hr = m_KeyMutex->ReleaseSync(0);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to release keyed mutex.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    vertexBuffer->Release();
    shaderResource->Release();

    hr = m_DxgiSwapChain->Present(1, 0);
}

HANDLE Takoyaki::OutputManager::GetSharedTextureHandle() const
{
    HANDLE handle = nullptr;

    IDXGIResource* resource = nullptr;
    HRESULT hr = m_SharedTexture->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void**>(&resource));
    if (SUCCEEDED(hr))
    {
        // Obtain handle to IDXGIResource object.
        resource->GetSharedHandle(&handle);
        resource->Release();
        resource = nullptr;
    }

    return handle;
}

Tako::TakoRect Takoyaki::OutputManager::GetTargetRect() const
{
    return m_TargetRect;
}

void Takoyaki::OutputManager::SetTargetRect(Tako::TakoRect rect)
{
    if (rect == m_TargetRect)
        return;

    m_TargetRect = rect;

    UpdateViewport();
    InitializeSharedTexture();
    ResizeSwapChain();
    UpdateWin32Window();
}

void Takoyaki::OutputManager::SetEnabled(bool isEnabled)
{
    if (isEnabled == m_IsEnabled)
        return;

    m_IsEnabled = isEnabled;
    
    if (m_IsEnabled)
        ShowWindow(m_OutputHwnd, SW_SHOW);
    else
        ShowWindow(m_OutputHwnd, SW_HIDE);
}

void Takoyaki::OutputManager::InitializeWin32Window()
{
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = OutputWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"TakoyakiOutputWindowClass";
    HRESULT hr = RegisterClass(&wc);

    HINSTANCE hInst = GetModuleHandle(NULL);

    m_OutputHwnd = CreateWindowExW(
        0, L"TakoyakiOutputWindowClass", L"Takoyaki",
        WS_POPUP,
        0, 0, 1920, 1080,
        nullptr, nullptr, hInst, nullptr);

    if (m_OutputHwnd == nullptr)
    {
        MessageBox(nullptr, L"Failed to create output window.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    UpdateWin32Window();
}

void Takoyaki::OutputManager::InitializeGraphicsApi()
{
    m_GfxContext.Initialize();

    InitializeSwapChain();
    InitializeSharedTexture();
    InitializeBackbufferRtv();
    InitializeSampler();
    InitializeBlendState();
    InitializeShaders();

    UpdateViewport();
}

void Takoyaki::OutputManager::InitializeSwapChain()
{
    // Get window size
    RECT windowRect;
    GetClientRect(m_OutputHwnd, &windowRect);
    UINT width = windowRect.right - windowRect.left;
    UINT height = windowRect.bottom - windowRect.top;

    // Create swapchain for window
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    RtlZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.BufferCount = 3;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    HRESULT hr = m_GfxContext.GetDxgiFactory()->CreateSwapChainForHwnd(
        m_GfxContext.GetDevice().Get(), m_OutputHwnd, &swapChainDesc, nullptr, nullptr, &m_DxgiSwapChain);

    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create window swapchain.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
}

void Takoyaki::OutputManager::InitializeSharedTexture()
{
    D3D11_TEXTURE2D_DESC desc;
    RtlZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
    desc.Width = m_TargetRect.m_Width;
    desc.Height = m_TargetRect.m_Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    HRESULT hr = m_GfxContext.GetDevice()->CreateTexture2D(&desc, nullptr, m_SharedTexture.GetAddressOf());
    static bool isInitialization = true;

    if (FAILED(hr))
    {
        // Texture creation can fail for many reasons, but one of them is that the requested size
        // is bigger than what the device supports. In the future, some sort of mandatory downsample
        // until creation success can be added, if too many people complains about this.

        if (isInitialization)
        {
            MessageBox(nullptr, L"Failed to create initial shared texture.", L"Takoyaki Error", MB_OK);
        }
        else
        {
            isInitialization = false;
            MessageBox(nullptr, L"Failed to create initial shared texture. The selected area may be too big.", L"Takoyaki Error", MB_OK);
        }

        exit(0);
    }

    // Get keyed mutex
    hr = m_SharedTexture->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(m_KeyMutex.GetAddressOf()));
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to obtain shared texture mutex.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
}

void Takoyaki::OutputManager::InitializeBackbufferRtv()
{
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = m_DxgiSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));

    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to obtain backbuffer from swapchain.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    hr = m_GfxContext.GetDevice()->CreateRenderTargetView(backBuffer, nullptr, m_BackbufferRtv.GetAddressOf());
    backBuffer->Release();
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create backbuffer RTV.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    // Set new render target
    m_GfxContext.GetDeviceContext()->OMSetRenderTargets(1, m_BackbufferRtv.GetAddressOf(), nullptr);
}

void Takoyaki::OutputManager::InitializeSampler()
{
    D3D11_SAMPLER_DESC sampleDesc;
    RtlZeroMemory(&sampleDesc, sizeof(sampleDesc));
    sampleDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampleDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampleDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampleDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampleDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampleDesc.MinLOD = 0;
    sampleDesc.MaxLOD = D3D11_FLOAT32_MAX;
    HRESULT hr = m_GfxContext.GetDevice()->CreateSamplerState(&sampleDesc, &m_Sampler);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create sampler state.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
}

void Takoyaki::OutputManager::InitializeBlendState()
{
    // Create the blend state
    D3D11_BLEND_DESC blendStateDesc;
    blendStateDesc.AlphaToCoverageEnable = FALSE;
    blendStateDesc.IndependentBlendEnable = FALSE;
    blendStateDesc.RenderTarget[0].BlendEnable = TRUE;
    blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    HRESULT hr = m_GfxContext.GetDevice()->CreateBlendState(&blendStateDesc, &m_BlendState);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create blend state.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
}

void Takoyaki::OutputManager::InitializeShaders()
{
    HRESULT hr;

    UINT size = ARRAYSIZE(g_VS_Main);
    hr = m_GfxContext.GetDevice()->CreateVertexShader(g_VS_Main, size, nullptr, &m_VertexShader);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create vertex shader.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    D3D11_INPUT_ELEMENT_DESC inputLayout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    UINT numElements = ARRAYSIZE(inputLayout);
    hr = m_GfxContext.GetDevice()->CreateInputLayout(inputLayout, numElements, g_VS_Main, size, m_InputLayout.GetAddressOf());
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create input layout for vertex shader.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
    m_GfxContext.GetDeviceContext()->IASetInputLayout(m_InputLayout.Get());

    size = ARRAYSIZE(g_PS_Main);
    hr = m_GfxContext.GetDevice()->CreatePixelShader(g_PS_Main, size, nullptr, &m_PixelShader);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create pixel shader.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
}

void Takoyaki::OutputManager::ResizeSwapChain()
{
    m_BackbufferRtv->Release();

    DXGI_SWAP_CHAIN_DESC desc;
    m_DxgiSwapChain->GetDesc(&desc);

    HRESULT hr = m_DxgiSwapChain->ResizeBuffers(
        desc.BufferCount,
        m_TargetRect.m_Width,
        m_TargetRect.m_Height,
        desc.BufferDesc.Format,
        desc.Flags);

    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to resize swapchain buffers.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    InitializeBackbufferRtv();
}

void Takoyaki::OutputManager::UpdateViewport()
{
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<FLOAT>(m_TargetRect.m_Width);
    vp.Height = static_cast<FLOAT>(m_TargetRect.m_Height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_GfxContext.GetDeviceContext()->RSSetViewports(1, &vp);
}

void Takoyaki::OutputManager::UpdateWin32Window()
{
    MoveWindow(m_OutputHwnd, -32000, -32000, m_TargetRect.m_Width, m_TargetRect.m_Height, false);
}

LRESULT CALLBACK OutputWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CLOSE:
    {
        g_Enabled = false;
        break;
    }
    case WM_SIZE:
    {
        // TODO: resize swap chain buffers
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

