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

Takoyaki::OutputManager::OutputManager(HWND appHwnd, HINSTANCE hInstance)
    : m_hInstance(hInstance)
    , m_AppHwnd(appHwnd)
    , m_OutputHwnd(nullptr)
{
    m_TargetRect = {
        .m_Width = 1920,
        .m_Height = 1080
    };
}

void Takoyaki::OutputManager::Initialize()
{
    InitializeWin32Window();
    InitializeD3D11();
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
    HRESULT hr = m_Device->CreateShaderResourceView(m_SharedTexture.Get(), &srvDesc, &shaderResource);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create SRV while rendering frame.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    m_DeviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    m_DeviceContext->OMSetRenderTargets(1, m_BackbufferRtv.GetAddressOf(), nullptr);
    m_DeviceContext->VSSetShader(m_VertexShader.Get(), nullptr, 0);
    m_DeviceContext->PSSetShader(m_PixelShader.Get(), nullptr, 0);
    m_DeviceContext->PSSetShaderResources(0, 1, &shaderResource);
    m_DeviceContext->PSSetSamplers(0, 1, m_Sampler.GetAddressOf());
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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
    hr = m_Device->CreateBuffer(&bufferDesc, &initData, &vertexBuffer);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create vertex buffer while rendering frame.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
    m_DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    // Draw textured quad onto render target
    m_DeviceContext->Draw(NumVertices, 0);

    // Release keyed mutex
    hr = m_KeyMutex->ReleaseSync(0);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to release keyed mutex.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    hr = m_DxgiSwapChain->Present(1, 0);

    vertexBuffer->Release();
    shaderResource->Release();
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
    m_TargetRect = rect;

    UpdateViewport();
    InitializeSharedTexture();
    ResizeSwapChain();
}

void Takoyaki::OutputManager::InitializeWin32Window()
{
    m_OutputHwnd = CreateWindowExW(
        0, L"TakoyakiWindowClass", L"Takoyaki",
        WS_POPUP | WS_VISIBLE,
        0, 0, 1920, 1080,
        nullptr, nullptr, m_hInstance, nullptr);

    if (m_OutputHwnd == nullptr)
    {
        MessageBox(nullptr, L"Failed to create output window.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    MoveWindow(m_OutputHwnd, -32000, -32000, 1920, 1080, false);
}

void Takoyaki::OutputManager::InitializeD3D11()
{
    InitializeDevice();
    InitializeDxgi();
    InitializeSwapChain();
    InitializeSharedTexture();
    InitializeBackbufferRtv();
    InitializeSampler();
    InitializeBlendState();
    InitializeShaders();

    UpdateViewport();
}

void Takoyaki::OutputManager::InitializeDevice()
{
    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    D3D_FEATURE_LEVEL featureLevels[] =
    {
    // Release old buffers
    // On DX12, this would be done manually with signal/fence
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);
    D3D_FEATURE_LEVEL currentFeatureLevel;

    HRESULT hr;

    // Create device
    for (UINT i = 0; i < numDriverTypes; ++i)
    {
        UINT createDeviceFlags = 0;
#ifdef _DEBUG 
        createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif
        
        hr = D3D11CreateDevice(nullptr, driverTypes[i], nullptr, D3D11_CREATE_DEVICE_DEBUG, featureLevels, numFeatureLevels,
            D3D11_SDK_VERSION, &m_Device, &currentFeatureLevel, &m_DeviceContext);

        if (SUCCEEDED(hr))
            break;
    }

    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to find a compatible graphics device.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
}

void Takoyaki::OutputManager::InitializeDxgi()
{
    // DXGI Factory
    HRESULT hr = m_Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(m_DxgiDevice.GetAddressOf()));
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to query interface for DXGI.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    hr = m_DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(m_DxgiAdapter.GetAddressOf()));
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to get parent DXGI Adapter.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    hr = m_DxgiAdapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(m_DxgiFactory.GetAddressOf()));
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to get parent DXGI Factory.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
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

    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    HRESULT hr = m_DxgiFactory->CreateSwapChainForHwnd(m_Device.Get(), m_OutputHwnd, &swapChainDesc, nullptr, nullptr, &m_DxgiSwapChain);

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

    HRESULT hr = m_Device->CreateTexture2D(&desc, nullptr, m_SharedTexture.GetAddressOf());
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

    hr = m_Device->CreateRenderTargetView(backBuffer, nullptr, m_BackbufferRtv.GetAddressOf());
    backBuffer->Release();
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create backbuffer RTV.", L"Takoyaki Error", MB_OK);
        exit(0);
    }

    // Set new render target
    m_DeviceContext->OMSetRenderTargets(1, m_BackbufferRtv.GetAddressOf(), nullptr);
}

void Takoyaki::OutputManager::InitializeSampler()
{
    D3D11_SAMPLER_DESC sampleDesc;
    RtlZeroMemory(&sampleDesc, sizeof(sampleDesc));
    sampleDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampleDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampleDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampleDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampleDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampleDesc.MinLOD = 0;
    sampleDesc.MaxLOD = D3D11_FLOAT32_MAX;
    HRESULT hr = m_Device->CreateSamplerState(&sampleDesc, &m_Sampler);
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
    HRESULT hr = m_Device->CreateBlendState(&blendStateDesc, &m_BlendState);
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
    hr = m_Device->CreateVertexShader(g_VS_Main, size, nullptr, &m_VertexShader);
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
    hr = m_Device->CreateInputLayout(inputLayout, numElements, g_VS_Main, size, m_InputLayout.GetAddressOf());
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create input layout for vertex shader.", L"Takoyaki Error", MB_OK);
        exit(0);
    }
    m_DeviceContext->IASetInputLayout(m_InputLayout.Get());

    size = ARRAYSIZE(g_PS_Main);
    hr = m_Device->CreatePixelShader(g_PS_Main, size, nullptr, &m_PixelShader);
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
    m_DeviceContext->RSSetViewports(1, &vp);
}

LRESULT CALLBACK OutputWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
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

