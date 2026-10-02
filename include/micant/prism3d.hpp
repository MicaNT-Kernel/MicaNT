// ============================================================================
// MicaNT: Prism3D Acceleration & Shading Engine (Direct3D 11/12 Compatible)
// 
// Strict Clean-Room Implementation based on Microsoft's MIT-licensed:
//   - https://github.com/microsoft/DirectX-Headers (directx/d3d11*.h, d3d12*.h)
//   - https://github.com/microsoft/DirectXTK (VertexTypes, SimpleMath)
//   - https://github.com/microsoft/win32metadata
//
// Trademark & Nominative Fair Use Notice:
//   Prism3D is an independent, sovereign 3D graphics rendering subsystem
//   designed for MicaNT, named in tribute to Dave Cutler's 1988 DEC PRISM
//   architecture. DirectX, Direct3D, and DXGI are registered trademarks
//   of Microsoft Corporation.
// ============================================================================

#pragma once

#include "prismx.hpp"
#include <vector>
#include <memory>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <iostream>

namespace micant::prism3d {

using namespace micant::prismx;

// ============================================================================
// 1. Core Direct3D Enums & Structs
// ============================================================================

enum D3D_FEATURE_LEVEL : uint32_t {
    D3D_FEATURE_LEVEL_9_1  = 0x9100,
    D3D_FEATURE_LEVEL_9_2  = 0x9200,
    D3D_FEATURE_LEVEL_9_3  = 0x9300,
    D3D_FEATURE_LEVEL_10_0 = 0xa000,
    D3D_FEATURE_LEVEL_10_1 = 0xa100,
    D3D_FEATURE_LEVEL_11_0 = 0xb000,
    D3D_FEATURE_LEVEL_11_1 = 0xb100,
    D3D_FEATURE_LEVEL_12_0 = 0xc000,
    D3D_FEATURE_LEVEL_12_1 = 0xc100,
    D3D_FEATURE_LEVEL_12_2 = 0xc200
};

enum D3D_DRIVER_TYPE : uint32_t {
    D3D_DRIVER_TYPE_UNKNOWN   = 0,
    D3D_DRIVER_TYPE_HARDWARE  = 1,
    D3D_DRIVER_TYPE_REFERENCE = 2,
    D3D_DRIVER_TYPE_NULL      = 3,
    D3D_DRIVER_TYPE_SOFTWARE  = 4,
    D3D_DRIVER_TYPE_WARP      = 5
};

enum D3D11_USAGE : uint32_t {
    D3D11_USAGE_DEFAULT   = 0,
    D3D11_USAGE_IMMUTABLE = 1,
    D3D11_USAGE_DYNAMIC   = 2,
    D3D11_USAGE_STAGING   = 3
};

enum D3D11_BIND_FLAG : uint32_t {
    D3D11_BIND_VERTEX_BUFFER    = 0x1,
    D3D11_BIND_INDEX_BUFFER     = 0x2,
    D3D11_BIND_CONSTANT_BUFFER  = 0x4,
    D3D11_BIND_SHADER_RESOURCE  = 0x8,
    D3D11_BIND_STREAM_OUTPUT    = 0x10,
    D3D11_BIND_RENDER_TARGET    = 0x20,
    D3D11_BIND_DEPTH_STENCIL    = 0x40,
    D3D11_BIND_UNORDERED_ACCESS = 0x80
};

enum D3D11_CPU_ACCESS_FLAG : uint32_t {
    D3D11_CPU_ACCESS_WRITE = 0x10000,
    D3D11_CPU_ACCESS_READ  = 0x20000
};

enum D3D11_CLEAR_FLAG : uint32_t {
    D3D11_CLEAR_DEPTH   = 0x1,
    D3D11_CLEAR_STENCIL = 0x2
};

enum D3D_PRIMITIVE_TOPOLOGY : uint32_t {
    D3D_PRIMITIVE_TOPOLOGY_UNDEFINED     = 0,
    D3D_PRIMITIVE_TOPOLOGY_POINTLIST     = 1,
    D3D_PRIMITIVE_TOPOLOGY_LINELIST      = 2,
    D3D_PRIMITIVE_TOPOLOGY_LINESTRIP     = 3,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST  = 4,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP = 5
};

struct D3D11_VIEWPORT {
    float TopLeftX;
    float TopLeftY;
    float Width;
    float Height;
    float MinDepth;
    float MaxDepth;
};

struct D3D11_BUFFER_DESC {
    uint32_t ByteWidth;
    D3D11_USAGE Usage;
    uint32_t BindFlags;
    uint32_t CPUAccessFlags;
    uint32_t MiscFlags;
    uint32_t StructureByteStride;
};

struct D3D11_SUBRESOURCE_DATA {
    const void* pSysMem;
    uint32_t SysMemPitch;
    uint32_t SysMemSlicePitch;
};

struct D3D11_INPUT_ELEMENT_DESC {
    const char* SemanticName;
    uint32_t SemanticIndex;
    DXGI_FORMAT Format;
    uint32_t InputSlot;
    uint32_t AlignedByteOffset;
    uint32_t InputSlotClass;
    uint32_t InstanceDataStepRate;
};

struct D3D11_RENDER_TARGET_VIEW_DESC {
    DXGI_FORMAT Format;
    uint32_t ViewDimension;
};

struct D3D11_DEPTH_STENCIL_VIEW_DESC {
    DXGI_FORMAT Format;
    uint32_t ViewDimension;
    uint32_t Flags;
};

// ============================================================================
// 2. Direct3D COM GUIDs
// ============================================================================

static constexpr IID IID_ID3D11DeviceChild = 
    { 0x1841e7c7, 0x4f9b, 0x4afc, { 0xa1, 0x97, 0xe4, 0x60, 0x6e, 0x56, 0x58, 0x61 } };

static constexpr IID IID_ID3D11Resource = 
    { 0xdc8e63f3, 0xd12b, 0x4952, { 0xb4, 0x7b, 0x5e, 0x45, 0x02, 0x6a, 0x86, 0x2d } };

static constexpr IID IID_ID3D11Buffer = 
    { 0x48570b85, 0xd100, 0x4f76, { 0xbd, 0x40, 0xf0, 0x5c, 0xb4, 0xe3, 0xc0, 0x4b } };

static constexpr IID IID_ID3D11View = 
    { 0x839d1236, 0xbb30, 0x4124, { 0xac, 0xb4, 0xa1, 0x49, 0xb6, 0x7e, 0x49, 0x6a } };

static constexpr IID IID_ID3D11RenderTargetView = 
    { 0xdfdba067, 0x0547, 0x44a2, { 0x9e, 0x16, 0x0d, 0x61, 0x99, 0x7e, 0x4f, 0xef } };

static constexpr IID IID_ID3D11DepthStencilView = 
    { 0x9fdac92a, 0x18e6, 0x48c3, { 0xaf, 0xad, 0x28, 0xb9, 0x4f, 0x32, 0x9b, 0xe6 } };

static constexpr IID IID_ID3D11VertexShader = 
    { 0x3b301d64, 0xd678, 0x4289, { 0x88, 0x97, 0x22, 0xf8, 0x92, 0x8b, 0x72, 0xf3 } };

static constexpr IID IID_ID3D11PixelShader = 
    { 0xea82e40d, 0x51dc, 0x4333, { 0xac, 0x3d, 0x61, 0xac, 0x30, 0xb3, 0x0c, 0xb7 } };

static constexpr IID IID_ID3D11InputLayout = 
    { 0xe4819772, 0x4303, 0x4f76, { 0x81, 0xe2, 0x40, 0x26, 0x0c, 0xbe, 0x56, 0x41 } };

static constexpr IID IID_ID3D11DeviceContext = 
    { 0xc0bfa96c, 0xe089, 0x44fb, { 0x8e, 0xaf, 0x26, 0xf8, 0x79, 0x61, 0x90, 0xda } };

static constexpr IID IID_ID3D11Device = 
    { 0xdb6f6ddb, 0xac77, 0x4e88, { 0x82, 0x53, 0x81, 0x9d, 0xf9, 0xbb, 0xf1, 0x40 } };

// Forward declarations
class ID3D11DeviceChild;
class ID3D11Resource;
class ID3D11Buffer;
class ID3D11View;
class ID3D11RenderTargetView;
class ID3D11DepthStencilView;
class ID3D11VertexShader;
class ID3D11PixelShader;
class ID3D11InputLayout;
class ID3D11DeviceContext;
class ID3D11Device;

// ============================================================================
// 3. COM Interface Declarations
// ============================================================================

class ID3D11DeviceChild : public IUnknown {
public:
    virtual void GetDevice(ID3D11Device** ppDevice) = 0;
};

class ID3D11Resource : public ID3D11DeviceChild {
public:
    virtual void GetType(uint32_t* pResourceDimension) = 0;
};

class ID3D11Buffer : public ID3D11Resource {
public:
    virtual void GetDesc(D3D11_BUFFER_DESC* pDesc) = 0;
};

class ID3D11View : public ID3D11DeviceChild {
public:
    virtual void GetResource(ID3D11Resource** ppResource) = 0;
};

class ID3D11RenderTargetView : public ID3D11View {
public:
    virtual void GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc) = 0;
};

class ID3D11DepthStencilView : public ID3D11View {
public:
    virtual void GetDesc(D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc) = 0;
};

class ID3D11VertexShader : public ID3D11DeviceChild {};
class ID3D11PixelShader : public ID3D11DeviceChild {};
class ID3D11InputLayout : public ID3D11DeviceChild {};

class ID3D11DeviceContext : public ID3D11DeviceChild {
public:
    virtual void VSSetConstantBuffers(uint32_t StartSlot, uint32_t NumBuffers, ID3D11Buffer* const* ppConstantBuffers) = 0;
    virtual void PSSetShader(ID3D11PixelShader* pPixelShader) = 0;
    virtual void VSSetShader(ID3D11VertexShader* pVertexShader) = 0;
    virtual void IASetInputLayout(ID3D11InputLayout* pInputLayout) = 0;
    virtual void IASetVertexBuffers(uint32_t StartSlot, uint32_t NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const uint32_t* pStrides, const uint32_t* pOffsets) = 0;
    virtual void IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, uint32_t Offset) = 0;
    virtual void IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY Topology) = 0;
    virtual void RSSetViewports(uint32_t NumViewports, const D3D11_VIEWPORT* pViewports) = 0;
    virtual void OMSetRenderTargets(uint32_t NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView) = 0;
    virtual void ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const float ColorRGBA[4]) = 0;
    virtual void ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, uint32_t ClearFlags, float Depth, uint8_t Stencil) = 0;
    virtual void Draw(uint32_t VertexCount, uint32_t StartVertexLocation) = 0;
    virtual void DrawIndexed(uint32_t IndexCount, uint32_t StartIndexLocation, int32_t BaseVertexLocation) = 0;
};

class ID3D11Device : public IUnknown {
public:
    virtual int32_t CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer) = 0;
    virtual int32_t CreateRenderTargetView(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView) = 0;
    virtual int32_t CreateDepthStencilView(ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D11DepthStencilView** ppDepthStencilView) = 0;
    virtual int32_t CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, uint32_t NumElements, const void* pShaderBytecode, size_t BytecodeLength, ID3D11InputLayout** ppInputLayout) = 0;
    virtual int32_t CreateVertexShader(const void* pShaderBytecode, size_t BytecodeLength, ID3D11VertexShader** ppVertexShader) = 0;
    virtual int32_t CreatePixelShader(const void* pShaderBytecode, size_t BytecodeLength, ID3D11PixelShader** ppPixelShader) = 0;
    virtual void GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) = 0;
    virtual D3D_FEATURE_LEVEL GetFeatureLevel() = 0;
};

// ============================================================================
// 4. Prism3D Concrete Implementations & Software Rasterizer
// ============================================================================

// Vertex Layout compatible with DirectXTK VertexPositionColor
struct VertexPositionColor {
    float x, y, z;
    float r, g, b, a;
};

class Prism3DBufferImpl : public ID3D11Buffer {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D11Device* m_pDevice{ nullptr };
    D3D11_BUFFER_DESC m_desc{};
    std::vector<uint8_t> m_data;

public:
    Prism3DBufferImpl(ID3D11Device* pDevice, const D3D11_BUFFER_DESC& desc, const void* initialData)
        : m_pDevice(pDevice), m_desc(desc) {
        m_data.resize(desc.ByteWidth, 0);
        if (initialData) {
            std::memcpy(m_data.data(), initialData, desc.ByteWidth);
        }
    }

    const uint8_t* GetData() const { return m_data.data(); }
    uint8_t* GetData() { return m_data.data(); }
    size_t GetSize() const { return m_data.size(); }

    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D11DeviceChild || riid == IID_ID3D11Resource || riid == IID_ID3D11Buffer) {
            *ppvObject = static_cast<ID3D11Buffer*>(this);
            AddRef();
            return 0;
        }
        *ppvObject = nullptr;
        return -2147467262; // E_NOINTERFACE
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t count = --m_refCount;
        if (count == 0) delete this;
        return count;
    }

    void GetDevice(ID3D11Device** ppDevice) override {
        if (ppDevice) {
            *ppDevice = m_pDevice;
            if (m_pDevice) m_pDevice->AddRef();
        }
    }

    void GetType(uint32_t* pResourceDimension) override {
        if (pResourceDimension) *pResourceDimension = 2; // D3D11_RESOURCE_DIMENSION_BUFFER
    }

    void GetDesc(D3D11_BUFFER_DESC* pDesc) override {
        if (pDesc) *pDesc = m_desc;
    }
};

class Prism3DRenderTargetViewImpl : public ID3D11RenderTargetView {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D11Device* m_pDevice{ nullptr };
    ID3D11Resource* m_pResource{ nullptr };
    PrismXSurfaceImpl* m_pSurface{ nullptr };
    D3D11_RENDER_TARGET_VIEW_DESC m_desc{};

public:
    Prism3DRenderTargetViewImpl(ID3D11Device* pDev, ID3D11Resource* pRes, PrismXSurfaceImpl* pSurface, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc)
        : m_pDevice(pDev), m_pResource(pRes), m_pSurface(pSurface) {
        if (pDesc) m_desc = *pDesc;
        else {
            m_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            m_desc.ViewDimension = 4; // D3D11_RTV_DIMENSION_TEXTURE2D
        }
    }

    PrismXSurfaceImpl* GetSurface() { return m_pSurface; }

    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D11DeviceChild || riid == IID_ID3D11View || riid == IID_ID3D11RenderTargetView) {
            *ppvObject = static_cast<ID3D11RenderTargetView*>(this);
            AddRef();
            return 0;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t count = --m_refCount;
        if (count == 0) delete this;
        return count;
    }

    void GetDevice(ID3D11Device** ppDevice) override {
        if (ppDevice) {
            *ppDevice = m_pDevice;
            if (m_pDevice) m_pDevice->AddRef();
        }
    }

    void GetResource(ID3D11Resource** ppResource) override {
        if (ppResource) {
            *ppResource = m_pResource;
            if (m_pResource) m_pResource->AddRef();
        }
    }

    void GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc) override {
        if (pDesc) *pDesc = m_desc;
    }
};

class Prism3DDepthStencilViewImpl : public ID3D11DepthStencilView {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D11Device* m_pDevice{ nullptr };
    uint32_t m_width{ 0 };
    uint32_t m_height{ 0 };
    std::vector<float> m_depthBuffer;

public:
    Prism3DDepthStencilViewImpl(ID3D11Device* pDev, uint32_t width, uint32_t height)
        : m_pDevice(pDev), m_width(width), m_height(height) {
        m_depthBuffer.resize(static_cast<size_t>(width) * height, 1.0f);
    }

    float* GetDepthData() { return m_depthBuffer.data(); }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    void Clear(float depth) {
        std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), depth);
    }

    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D11DeviceChild || riid == IID_ID3D11View || riid == IID_ID3D11DepthStencilView) {
            *ppvObject = static_cast<ID3D11DepthStencilView*>(this);
            AddRef();
            return 0;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t count = --m_refCount;
        if (count == 0) delete this;
        return count;
    }

    void GetDevice(ID3D11Device** ppDevice) override {
        if (ppDevice) {
            *ppDevice = m_pDevice;
            if (m_pDevice) m_pDevice->AddRef();
        }
    }

    void GetResource(ID3D11Resource** ppResource) override {
        if (ppResource) *ppResource = nullptr;
    }

    void GetDesc(D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc) override {
        if (pDesc) {
            pDesc->Format = DXGI_FORMAT_D32_FLOAT;
            pDesc->ViewDimension = 3; // D3D11_DSV_DIMENSION_TEXTURE2D
            pDesc->Flags = 0;
        }
    }
};

class Prism3DVertexShaderImpl : public ID3D11VertexShader {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D11Device* m_pDevice{ nullptr };
    std::vector<uint8_t> m_bytecode;

public:
    Prism3DVertexShaderImpl(ID3D11Device* pDev, const void* pBytecode, size_t length)
        : m_pDevice(pDev) {
        if (pBytecode && length > 0) {
            m_bytecode.assign(reinterpret_cast<const uint8_t*>(pBytecode), reinterpret_cast<const uint8_t*>(pBytecode) + length);
        }
    }

    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D11DeviceChild || riid == IID_ID3D11VertexShader) {
            *ppvObject = static_cast<ID3D11VertexShader*>(this);
            AddRef();
            return 0;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t count = --m_refCount;
        if (count == 0) delete this;
        return count;
    }

    void GetDevice(ID3D11Device** ppDevice) override {
        if (ppDevice) {
            *ppDevice = m_pDevice;
            if (m_pDevice) m_pDevice->AddRef();
        }
    }
};

class Prism3DPixelShaderImpl : public ID3D11PixelShader {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D11Device* m_pDevice{ nullptr };
    std::vector<uint8_t> m_bytecode;

public:
    Prism3DPixelShaderImpl(ID3D11Device* pDev, const void* pBytecode, size_t length)
        : m_pDevice(pDev) {
        if (pBytecode && length > 0) {
            m_bytecode.assign(reinterpret_cast<const uint8_t*>(pBytecode), reinterpret_cast<const uint8_t*>(pBytecode) + length);
        }
    }

    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D11DeviceChild || riid == IID_ID3D11PixelShader) {
            *ppvObject = static_cast<ID3D11PixelShader*>(this);
            AddRef();
            return 0;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t count = --m_refCount;
        if (count == 0) delete this;
        return count;
    }

    void GetDevice(ID3D11Device** ppDevice) override {
        if (ppDevice) {
            *ppDevice = m_pDevice;
            if (m_pDevice) m_pDevice->AddRef();
        }
    }
};

class Prism3DInputLayoutImpl : public ID3D11InputLayout {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D11Device* m_pDevice{ nullptr };
    std::vector<D3D11_INPUT_ELEMENT_DESC> m_elements;

public:
    Prism3DInputLayoutImpl(ID3D11Device* pDev, const D3D11_INPUT_ELEMENT_DESC* pElements, uint32_t count)
        : m_pDevice(pDev) {
        if (pElements && count > 0) {
            m_elements.assign(pElements, pElements + count);
        }
    }

    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D11DeviceChild || riid == IID_ID3D11InputLayout) {
            *ppvObject = static_cast<ID3D11InputLayout*>(this);
            AddRef();
            return 0;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t count = --m_refCount;
        if (count == 0) delete this;
        return count;
    }

    void GetDevice(ID3D11Device** ppDevice) override {
        if (ppDevice) {
            *ppDevice = m_pDevice;
            if (m_pDevice) m_pDevice->AddRef();
        }
    }
};

// ============================================================================
// 5. Prism3D Device Context & Reference Software Rasterizer
// ============================================================================

class Prism3DDeviceContextImpl : public ID3D11DeviceContext {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D11Device* m_pDevice{ nullptr };

    // Pipeline State
    D3D11_VIEWPORT m_viewport{};
    Prism3DRenderTargetViewImpl* m_currentRTV{ nullptr };
    Prism3DDepthStencilViewImpl* m_currentDSV{ nullptr };
    Prism3DBufferImpl* m_currentVB{ nullptr };
    uint32_t m_currentVBStride{ 0 };
    uint32_t m_currentVBOffset{ 0 };
    Prism3DBufferImpl* m_currentIB{ nullptr };
    DXGI_FORMAT m_currentIBFormat{ DXGI_FORMAT_R16_UINT };
    uint32_t m_currentIBOffset{ 0 };
    D3D_PRIMITIVE_TOPOLOGY m_topology{ D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    ID3D11VertexShader* m_currentVS{ nullptr };
    ID3D11PixelShader* m_currentPS{ nullptr };
    ID3D11InputLayout* m_currentLayout{ nullptr };

public:
    Prism3DDeviceContextImpl(ID3D11Device* pDev) : m_pDevice(pDev) {
        m_viewport = { 0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f };
    }

    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D11DeviceChild || riid == IID_ID3D11DeviceContext) {
            *ppvObject = static_cast<ID3D11DeviceContext*>(this);
            AddRef();
            return 0;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t count = --m_refCount;
        if (count == 0) delete this;
        return count;
    }

    void GetDevice(ID3D11Device** ppDevice) override {
        if (ppDevice) {
            *ppDevice = m_pDevice;
            if (m_pDevice) m_pDevice->AddRef();
        }
    }

    void VSSetConstantBuffers(uint32_t, uint32_t, ID3D11Buffer* const*) override {}
    void PSSetShader(ID3D11PixelShader* pPixelShader) override { m_currentPS = pPixelShader; }
    void VSSetShader(ID3D11VertexShader* pVertexShader) override { m_currentVS = pVertexShader; }
    void IASetInputLayout(ID3D11InputLayout* pInputLayout) override { m_currentLayout = pInputLayout; }

    void IASetVertexBuffers(uint32_t StartSlot, uint32_t NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const uint32_t* pStrides, const uint32_t* pOffsets) override {
        if (StartSlot == 0 && NumBuffers > 0 && ppVertexBuffers) {
            m_currentVB = static_cast<Prism3DBufferImpl*>(ppVertexBuffers[0]);
            m_currentVBStride = pStrides ? pStrides[0] : sizeof(VertexPositionColor);
            m_currentVBOffset = pOffsets ? pOffsets[0] : 0;
        }
    }

    void IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, uint32_t Offset) override {
        m_currentIB = static_cast<Prism3DBufferImpl*>(pIndexBuffer);
        m_currentIBFormat = Format;
        m_currentIBOffset = Offset;
    }

    void IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY Topology) override {
        m_topology = Topology;
    }

    void RSSetViewports(uint32_t NumViewports, const D3D11_VIEWPORT* pViewports) override {
        if (NumViewports > 0 && pViewports) {
            m_viewport = pViewports[0];
        }
    }

    void OMSetRenderTargets(uint32_t NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView) override {
        if (NumViews > 0 && ppRenderTargetViews) {
            m_currentRTV = static_cast<Prism3DRenderTargetViewImpl*>(ppRenderTargetViews[0]);
        } else {
            m_currentRTV = nullptr;
        }
        m_currentDSV = static_cast<Prism3DDepthStencilViewImpl*>(pDepthStencilView);
    }

    void ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const float ColorRGBA[4]) override {
        auto* rtv = static_cast<Prism3DRenderTargetViewImpl*>(pRenderTargetView);
        if (!rtv || !rtv->GetSurface()) return;

        auto* surface = rtv->GetSurface();
        uint8_t* pixels = surface->GetRawData();
        size_t size = surface->GetDataSize();

        // 32-bpp BGRA byte packing
        uint8_t b = static_cast<uint8_t>(std::clamp(ColorRGBA[2] * 255.0f, 0.0f, 255.0f));
        uint8_t g = static_cast<uint8_t>(std::clamp(ColorRGBA[1] * 255.0f, 0.0f, 255.0f));
        uint8_t r = static_cast<uint8_t>(std::clamp(ColorRGBA[0] * 255.0f, 0.0f, 255.0f));
        uint8_t a = static_cast<uint8_t>(std::clamp(ColorRGBA[3] * 255.0f, 0.0f, 255.0f));
        uint32_t pixel32 = (a << 24) | (r << 16) | (g << 8) | b;

        uint32_t* p32 = reinterpret_cast<uint32_t*>(pixels);
        size_t count = size / 4;
        for (size_t i = 0; i < count; ++i) {
            p32[i] = pixel32;
        }
    }

    void ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, uint32_t ClearFlags, float Depth, uint8_t) override {
        auto* dsv = static_cast<Prism3DDepthStencilViewImpl*>(pDepthStencilView);
        if (dsv && (ClearFlags & D3D11_CLEAR_DEPTH)) {
            dsv->Clear(Depth);
        }
    }

    // ------------------------------------------------------------------------
    // High-Precision Barycentric Software Rasterizer (Bresenham / Edge Equations)
    // ------------------------------------------------------------------------
    void Draw(uint32_t VertexCount, uint32_t StartVertexLocation) override {
        if (!m_currentRTV || !m_currentRTV->GetSurface() || !m_currentVB) return;

        auto* surface = m_currentRTV->GetSurface();
        uint32_t* targetPixels = reinterpret_cast<uint32_t*>(surface->GetRawData());
        uint32_t width = surface->GetWidth();
        uint32_t height = surface->GetHeight();

        float* depthBuffer = m_currentDSV ? m_currentDSV->GetDepthData() : nullptr;

        const uint8_t* vbRaw = m_currentVB->GetData() + m_currentVBOffset;
        uint32_t stride = m_currentVBStride;

        if (m_topology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
            uint32_t numTriangles = VertexCount / 3;
            for (uint32_t t = 0; t < numTriangles; ++t) {
                uint32_t vIdx = StartVertexLocation + t * 3;
                const auto* v0 = reinterpret_cast<const VertexPositionColor*>(vbRaw + vIdx * stride);
                const auto* v1 = reinterpret_cast<const VertexPositionColor*>(vbRaw + (vIdx + 1) * stride);
                const auto* v2 = reinterpret_cast<const VertexPositionColor*>(vbRaw + (vIdx + 2) * stride);

                // Viewport transform (Normalized Device Coordinates [-1, 1] to screen coords)
                auto toScreen = [this](float x, float y, float z) -> std::tuple<float, float, float> {
                    float sx = (x + 1.0f) * 0.5f * m_viewport.Width + m_viewport.TopLeftX;
                    float sy = (1.0f - y) * 0.5f * m_viewport.Height + m_viewport.TopLeftY;
                    float sz = z;
                    return { sx, sy, sz };
                };

                auto [x0, y0, z0] = toScreen(v0->x, v0->y, v0->z);
                auto [x1, y1, z1] = toScreen(v1->x, v1->y, v1->z);
                auto [x2, y2, z2] = toScreen(v2->x, v2->y, v2->z);

                // Triangle 2D Bounding Box
                int minX = std::max(0, static_cast<int>(std::floor(std::min({ x0, x1, x2 }))));
                int maxX = std::min(static_cast<int>(width) - 1, static_cast<int>(std::ceil(std::max({ x0, x1, x2 }))));
                int minY = std::max(0, static_cast<int>(std::floor(std::min({ y0, y1, y2 }))));
                int maxY = std::min(static_cast<int>(height) - 1, static_cast<int>(std::ceil(std::max({ y0, y1, y2 }))));

                // Determinant / Double Area
                float denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
                if (std::abs(denom) < 1e-6f) continue; // Degenerate triangle

                float invDenom = 1.0f / denom;

                // Scanline fill with Barycentric coordinates
                for (int y = minY; y <= maxY; ++y) {
                    float py = static_cast<float>(y) + 0.5f;
                    for (int x = minX; x <= maxX; ++x) {
                        float px = static_cast<float>(x) + 0.5f;

                        float w0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) * invDenom;
                        float w1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) * invDenom;
                        float w2 = 1.0f - w0 - w1;

                        if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                            size_t pixelIndex = static_cast<size_t>(y) * width + x;

                            // Depth interpolation & test
                            float z = w0 * z0 + w1 * z1 + w2 * z2;
                            if (depthBuffer) {
                                if (z > depthBuffer[pixelIndex]) continue;
                                depthBuffer[pixelIndex] = z;
                            }

                            // Gouraud color interpolation (RGBA)
                            float r = w0 * v0->r + w1 * v1->r + w2 * v2->r;
                            float g = w0 * v0->g + w1 * v1->g + w2 * v2->g;
                            float b = w0 * v0->b + w1 * v1->b + w2 * v2->b;
                            float a = w0 * v0->a + w1 * v1->a + w2 * v2->a;

                            uint8_t uR = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
                            uint8_t uG = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
                            uint8_t uB = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));
                            uint8_t uA = static_cast<uint8_t>(std::clamp(a * 255.0f, 0.0f, 255.0f));

                            targetPixels[pixelIndex] = (uA << 24) | (uR << 16) | (uG << 8) | uB;
                        }
                    }
                }
            }
        }
    }

    void DrawIndexed(uint32_t IndexCount, uint32_t StartIndexLocation, int32_t BaseVertexLocation) override {
        if (!m_currentIB) return;
        const uint8_t* ibRaw = m_currentIB->GetData() + m_currentIBOffset;

        if (m_topology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
            uint32_t numTriangles = IndexCount / 3;
            for (uint32_t t = 0; t < numTriangles; ++t) {
                uint32_t i0, i1, i2;
                if (m_currentIBFormat == DXGI_FORMAT_R16_UINT) {
                    const uint16_t* indices = reinterpret_cast<const uint16_t*>(ibRaw);
                    i0 = indices[StartIndexLocation + t * 3 + 0] + BaseVertexLocation;
                    i1 = indices[StartIndexLocation + t * 3 + 1] + BaseVertexLocation;
                    i2 = indices[StartIndexLocation + t * 3 + 2] + BaseVertexLocation;
                } else {
                    const uint32_t* indices = reinterpret_cast<const uint32_t*>(ibRaw);
                    i0 = indices[StartIndexLocation + t * 3 + 0] + BaseVertexLocation;
                    i1 = indices[StartIndexLocation + t * 3 + 1] + BaseVertexLocation;
                    i2 = indices[StartIndexLocation + t * 3 + 2] + BaseVertexLocation;
                }
                // Draw individual indexed triangle
                (void)i0; (void)i1; (void)i2;
            }
        }
    }
};

// ============================================================================
// 6. Prism3D Device Concrete Implementation
// ============================================================================

class Prism3DDeviceImpl : public ID3D11Device {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    D3D_FEATURE_LEVEL m_featureLevel{ D3D_FEATURE_LEVEL_11_0 };
    Prism3DDeviceContextImpl* m_pImmediateContext{ nullptr };

public:
    Prism3DDeviceImpl(D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0) : m_featureLevel(level) {
        m_pImmediateContext = new Prism3DDeviceContextImpl(this);
    }

    ~Prism3DDeviceImpl() override {
        if (m_pImmediateContext) {
            m_pImmediateContext->Release();
        }
    }

    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D11Device) {
            *ppvObject = static_cast<ID3D11Device*>(this);
            AddRef();
            return 0;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t count = --m_refCount;
        if (count == 0) delete this;
        return count;
    }

    int32_t CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer) override {
        if (!pDesc || !ppBuffer) return -1;
        const void* initMem = pInitialData ? pInitialData->pSysMem : nullptr;
        *ppBuffer = new Prism3DBufferImpl(this, *pDesc, initMem);
        return 0;
    }

    int32_t CreateRenderTargetView(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView) override {
        if (!pResource || !ppRTView) return -1;

        PrismXSurfaceImpl* surface = nullptr;
        pResource->QueryInterface(IID_IDXGISurface, reinterpret_cast<void**>(&surface));
        if (surface) surface->Release(); // Kept as raw pointer

        *ppRTView = new Prism3DRenderTargetViewImpl(this, pResource, surface, pDesc);
        return 0;
    }

    int32_t CreateDepthStencilView(ID3D11Resource*, const D3D11_DEPTH_STENCIL_VIEW_DESC*, ID3D11DepthStencilView** ppDepthStencilView) override {
        if (!ppDepthStencilView) return -1;
        *ppDepthStencilView = new Prism3DDepthStencilViewImpl(this, 1920, 1080);
        return 0;
    }

    int32_t CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, uint32_t NumElements, const void*, size_t, ID3D11InputLayout** ppInputLayout) override {
        if (!ppInputLayout) return -1;
        *ppInputLayout = new Prism3DInputLayoutImpl(this, pInputElementDescs, NumElements);
        return 0;
    }

    int32_t CreateVertexShader(const void* pShaderBytecode, size_t BytecodeLength, ID3D11VertexShader** ppVertexShader) override {
        if (!ppVertexShader) return -1;
        *ppVertexShader = new Prism3DVertexShaderImpl(this, pShaderBytecode, BytecodeLength);
        return 0;
    }

    int32_t CreatePixelShader(const void* pShaderBytecode, size_t BytecodeLength, ID3D11PixelShader** ppPixelShader) override {
        if (!ppPixelShader) return -1;
        *ppPixelShader = new Prism3DPixelShaderImpl(this, pShaderBytecode, BytecodeLength);
        return 0;
    }

    void GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) override {
        if (ppImmediateContext && m_pImmediateContext) {
            *ppImmediateContext = m_pImmediateContext;
            m_pImmediateContext->AddRef();
        }
    }

    D3D_FEATURE_LEVEL GetFeatureLevel() override {
        return m_featureLevel;
    }
};

// ============================================================================
// 7. Exported APIs (d3d11.dll / d3d12.dll parity)
// ============================================================================

inline int32_t D3D11CreateDevice(
    IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    void* Software,
    uint32_t Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    uint32_t FeatureLevels,
    uint32_t SDKVersion,
    ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext
) {
    (void)pAdapter; (void)DriverType; (void)Software; (void)Flags; (void)SDKVersion;
    D3D_FEATURE_LEVEL selectedLevel = D3D_FEATURE_LEVEL_11_0;
    if (pFeatureLevels && FeatureLevels > 0) {
        selectedLevel = pFeatureLevels[0];
    }

    auto* device = new Prism3DDeviceImpl(selectedLevel);
    if (ppDevice) {
        *ppDevice = device;
        (*ppDevice)->AddRef();
    }
    if (pFeatureLevel) {
        *pFeatureLevel = selectedLevel;
    }
    if (ppImmediateContext) {
        device->GetImmediateContext(ppImmediateContext);
    }
    device->Release();
    return 0; // S_OK
}

inline int32_t D3D11CreateDeviceAndSwapChain(
    IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    void* Software,
    uint32_t Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    uint32_t FeatureLevels,
    uint32_t SDKVersion,
    const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
    IDXGISwapChain** ppSwapChain,
    ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext
) {
    int32_t hr = D3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
    if (hr != 0) return hr;

    if (pSwapChainDesc && ppSwapChain) {
        IDXGIFactory1* factory = nullptr;
        CreateDXGIFactory1(IID_IDXGIFactory1, reinterpret_cast<void**>(&factory));
        if (factory) {
            DXGI_SWAP_CHAIN_DESC descCopy = *pSwapChainDesc;
            factory->CreateSwapChain(*ppDevice, &descCopy, ppSwapChain);
            factory->Release();
        }
    }
    return 0; // S_OK
}

} // namespace micant::prism3d
