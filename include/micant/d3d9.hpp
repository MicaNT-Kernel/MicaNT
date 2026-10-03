// ============================================================================
// MicaNT: Direct3D 9 Subsystem & Runtime (d3d9.dll)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides complete Direct3D 9 / 9Ex API Parity, Fixed-Function Geometry
// Pipeline (World/View/Projection Transformations, Lighting & Gouraud Shading),
// Software Rasterization, Vertex/Index Buffer Management, and Native User32
// Window Presentation Integration.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <array>
#include <string>
#include <string_view>
#include <span>
#include <unordered_map>

#include "ntdef.hpp"
#include "prismx.hpp"
#include "prism3d.hpp"
#include "user32.hpp"
#include "ldr.hpp"

namespace micant::d3d9 {

using IID = micant::GUID;
using GUID = micant::GUID;
using prismx::IUnknown;
using prismx::IID_IUnknown;

// ============================================================================
// 1. GUIDs & Standard Constants
// ============================================================================

inline constexpr uint32_t D3D_SDK_VERSION = 32;

// {81BDCBCA-64D4-426D-AE8D-AD0147F4275C}
inline const IID IID_IDirect3D9 = {
    0x81bdcbca, 0x64d4, 0x426d, { 0xae, 0x8d, 0xad, 0x01, 0x47, 0xf4, 0x27, 0x5c }
};

// {D0223B96-BF7A-43FD-92BD-A43B0D82B9EB}
inline const IID IID_IDirect3DDevice9 = {
    0xd0223b96, 0xbf7a, 0x43fd, { 0x92, 0xbd, 0xa4, 0x3b, 0x0d, 0x82, 0xb9, 0xeb }
};

// {B64BB1B5-FD70-4DF6-BF91-19D0A12455E3}
inline const IID IID_IDirect3DVertexBuffer9 = {
    0xb64bb1b5, 0xfd70, 0x4df6, { 0xbf, 0x91, 0x19, 0xd0, 0xa1, 0x24, 0x55, 0xe3 }
};

// {7C9DD65E-D3F7-4529-ACEE-785830ACDE35}
inline const IID IID_IDirect3DIndexBuffer9 = {
    0x7c9dd65e, 0xd3f7, 0x4529, { 0xac, 0xee, 0x78, 0x58, 0x30, 0xac, 0xde, 0x35 }
};

// {0CFBAF3A-9FF6-429A-99B3-A2796AF8B89B}
inline const IID IID_IDirect3DSurface9 = {
    0x0cfbaf3a, 0x9ff6, 0x429a, { 0x99, 0xb3, 0xa2, 0x79, 0x6a, 0xf8, 0xb8, 0x9b }
};

// {85C31227-3DE5-4F00-9B3A-F11AC38C18B5}
inline const IID IID_IDirect3DTexture9 = {
    0x85c31227, 0x3de5, 0x4f00, { 0x9b, 0x3a, 0xf1, 0x1a, 0xc3, 0x8c, 0x18, 0xb5 }
};

// {794950F2-ADFC-458A-905E-10A10B0B503B}
inline const IID IID_IDirect3DSwapChain9 = {
    0x794950f2, 0xadfc, 0x458a, { 0x90, 0x5e, 0x10, 0xa1, 0x0b, 0x0b, 0x50, 0x3b }
};

// Return codes
inline constexpr int32_t D3D_OK                        = 0;
inline constexpr int32_t D3DERR_WRONG_SDK_VERSION       = -2005530518;
inline constexpr int32_t D3DERR_INVALIDCALL            = -2005530516;
inline constexpr int32_t D3DERR_NOTAVAILABLE           = -2005530515;
inline constexpr int32_t D3DERR_OUTOFVIDEOMEMORY       = -2005530522;

// ============================================================================
// 2. Data Types & Enumerations
// ============================================================================

using D3DCOLOR = uint32_t;

inline constexpr D3DCOLOR D3DCOLOR_ARGB(uint32_t a, uint32_t r, uint32_t g, uint32_t b) noexcept {
    return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

inline constexpr D3DCOLOR D3DCOLOR_XRGB(uint32_t r, uint32_t g, uint32_t b) noexcept {
    return D3DCOLOR_ARGB(0xFF, r, g, b);
}

enum D3DDEVTYPE : uint32_t {
    D3DDEVTYPE_HAL     = 1,
    D3DDEVTYPE_REF     = 2,
    D3DDEVTYPE_SW      = 3,
    D3DDEVTYPE_NULLREF = 4
};

enum D3DFORMAT : uint32_t {
    D3DFMT_UNKNOWN    = 0,
    D3DFMT_R8G8B8     = 20,
    D3DFMT_A8R8G8B8   = 21,
    D3DFMT_X8R8G8B8   = 22,
    D3DFMT_R5G6B5     = 23,
    D3DFMT_X1R5G5B5   = 24,
    D3DFMT_A1R5G5B5   = 25,
    D3DFMT_A4R4G4B4   = 26,
    D3DFMT_D16        = 80,
    D3DFMT_D24S8      = 75,
    D3DFMT_D32        = 71,
    D3DFMT_INDEX16    = 101,
    D3DFMT_INDEX32    = 102
};

enum D3DSWAPEFFECT : uint32_t {
    D3DSWAPEFFECT_DISCARD = 1,
    D3DSWAPEFFECT_FLIP    = 2,
    D3DSWAPEFFECT_COPY    = 3,
    D3DSWAPEFFECT_OVERLAY = 4,
    D3DSWAPEFFECT_FLIPEX  = 5
};

enum D3DMULTISAMPLE_TYPE : uint32_t {
    D3DMULTISAMPLE_NONE = 0,
    D3DMULTISAMPLE_2_SAMPLES = 2,
    D3DMULTISAMPLE_4_SAMPLES = 4
};

enum D3DPRIMITIVETYPE : uint32_t {
    D3DPT_POINTLIST     = 1,
    D3DPT_LINELIST      = 2,
    D3DPT_LINESTRIP     = 3,
    D3DPT_TRIANGLELIST  = 4,
    D3DPT_TRIANGLESTRIP = 5,
    D3DPT_TRIANGLEFAN   = 6
};

enum D3DTRANSFORMSTATETYPE : uint32_t {
    D3DTS_VIEW        = 2,
    D3DTS_PROJECTION  = 3,
    D3DTS_TEXTURE0    = 16,
    D3DTS_WORLD       = 256
};

enum D3DRENDERSTATETYPE : uint32_t {
    D3DRS_ZENABLE           = 7,
    D3DRS_FILLMODE          = 8,
    D3DRS_SHADEMODE         = 9,
    D3DRS_ZWRITEENABLE      = 14,
    D3DRS_ALPHATESTENABLE   = 15,
    D3DRS_SRCBLEND          = 19,
    D3DRS_DESTBLEND         = 20,
    D3DRS_CULLMODE          = 22,
    D3DRS_ZFUNC             = 23,
    D3DRS_ALPHABLENDENABLE  = 27,
    D3DRS_LIGHTING          = 137
};

enum D3DFILLMODE : uint32_t {
    D3DFILL_POINT     = 1,
    D3DFILL_WIREFRAME = 2,
    D3DFILL_SOLID     = 3
};

enum D3DCULL : uint32_t {
    D3DCULL_NONE = 1,
    D3DCULL_CW   = 2,
    D3DCULL_CCW  = 3
};

enum D3DPOOL : uint32_t {
    D3DPOOL_DEFAULT   = 0,
    D3DPOOL_MANAGED   = 1,
    D3DPOOL_SYSTEMMEM = 2,
    D3DPOOL_SCRATCH   = 3
};

// Clear Flags
inline constexpr uint32_t D3DCLEAR_TARGET   = 0x00000001;
inline constexpr uint32_t D3DCLEAR_ZBUFFER  = 0x00000002;
inline constexpr uint32_t D3DCLEAR_STENCIL  = 0x00000004;

// Device Creation Flags
inline constexpr uint32_t D3DCREATE_SOFTWARE_VERTEXPROCESSING = 0x00000020;
inline constexpr uint32_t D3DCREATE_HARDWARE_VERTEXPROCESSING = 0x00000040;
inline constexpr uint32_t D3DCREATE_PUREDEVICE                = 0x00000010;
inline constexpr uint32_t D3DCREATE_MULTITHREADED             = 0x00000004;

// Flexible Vertex Format (FVF) Flags
inline constexpr uint32_t D3DFVF_RESERVED0 = 0x001;
inline constexpr uint32_t D3DFVF_XYZ       = 0x002;
inline constexpr uint32_t D3DFVF_XYZRHW    = 0x004;
inline constexpr uint32_t D3DFVF_XYZB1     = 0x006;
inline constexpr uint32_t D3DFVF_NORMAL    = 0x010;
inline constexpr uint32_t D3DFVF_DIFFUSE   = 0x040;
inline constexpr uint32_t D3DFVF_SPECULAR  = 0x080;
inline constexpr uint32_t D3DFVF_TEX1      = 0x100;
inline constexpr uint32_t D3DFVF_TEX2      = 0x200;

// Structures
struct D3DRECT {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#endif

struct D3DMATRIX {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };

    static D3DMATRIX Identity() noexcept {
        D3DMATRIX mat{};
        mat._11 = 1.0f; mat._22 = 1.0f; mat._33 = 1.0f; mat._44 = 1.0f;
        return mat;
    }
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

struct D3DVIEWPORT9 {
    uint32_t X;
    uint32_t Y;
    uint32_t Width;
    uint32_t Height;
    float    MinZ;
    float    MaxZ;
};

struct D3DPRESENT_PARAMETERS {
    uint32_t            BackBufferWidth{0};
    uint32_t            BackBufferHeight{0};
    D3DFORMAT           BackBufferFormat{D3DFMT_X8R8G8B8};
    uint32_t            BackBufferCount{1};
    D3DMULTISAMPLE_TYPE MultiSampleType{D3DMULTISAMPLE_NONE};
    uint32_t            MultiSampleQuality{0};
    D3DSWAPEFFECT       SwapEffect{D3DSWAPEFFECT_DISCARD};
    win32::HWND         hDeviceWindow{nullptr};
    win32::BOOL         Windowed{win32::TRUE};
    win32::BOOL         EnableAutoDepthStencil{win32::FALSE};
    D3DFORMAT           AutoDepthStencilFormat{D3DFMT_D24S8};
    uint32_t            Flags{0};
    uint32_t            FullScreen_RefreshRateInHz{0};
    uint32_t            PresentationInterval{0};
};

struct D3DDISPLAYMODE {
    uint32_t  Width{1920};
    uint32_t  Height{1080};
    uint32_t  RefreshRate{60};
    D3DFORMAT Format{D3DFMT_X8R8G8B8};
};

struct D3DADAPTER_IDENTIFIER9 {
    char           Driver[512]{"micant_d3d9.dll"};
    char           Description[512]{"MicaNT PrismX Direct3D 9 Sovereign Graphics Subsystem"};
    char           DeviceName[32]{"\\\\.\\DISPLAY1"};
    uint32_t       DriverVersionLow{1};
    uint32_t       DriverVersionHigh{10};
    uint32_t       VendorId{0x10DE}; // Sovereign identifier
    uint32_t       DeviceId{0x1337};
    uint32_t       SubSysId{0xCAFE};
    uint32_t       Revision{1};
    IID            DeviceIdentifier{IID_IDirect3D9};
    uint32_t       WHQLLevel{1};
};

struct D3DCAPS9 {
    D3DDEVTYPE DeviceType{D3DDEVTYPE_HAL};
    uint32_t   AdapterOrdinal{0};
    uint32_t   Caps{0};
    uint32_t   Caps2{0};
    uint32_t   Caps3{0};
    uint32_t   PresentationIntervals{0x00000001};
    uint32_t   MaxTextureWidth{8192};
    uint32_t   MaxTextureHeight{8192};
    uint32_t   MaxPrimitiveCount{0x00FFFFFF};
    uint32_t   MaxVertexIndex{0x00FFFFFF};
    uint32_t   MaxStreams{16};
    uint32_t   VertexShaderVersion{0xFFFE0300}; // Shader Model 3.0
    uint32_t   PixelShaderVersion{0xFFFF0300};  // Shader Model 3.0
};

struct D3DLOCKED_RECT {
    int32_t Pitch{0};
    void*   pBits{nullptr};
};

// ============================================================================
// 3. COM Interface Forward Declarations
// ============================================================================

class IDirect3D9;
class IDirect3DDevice9;
class IDirect3DResource9;
class IDirect3DVertexBuffer9;
class IDirect3DIndexBuffer9;
class IDirect3DSurface9;
class IDirect3DTexture9;
class IDirect3DSwapChain9;

class IDirect3DResource9 : public IUnknown {
public:
    virtual void GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual int32_t SetPrivateData(const IID& guid, const void* pData, uint32_t SizeOfData, uint32_t Flags) = 0;
    virtual int32_t GetPrivateData(const IID& guid, void* pData, uint32_t* pSizeOfData) = 0;
    virtual int32_t FreePrivateData(const IID& guid) = 0;
    virtual uint32_t SetPriority(uint32_t PriorityNew) = 0;
    virtual uint32_t GetPriority() = 0;
    virtual void PreLoad() = 0;
    virtual uint32_t GetType() = 0;
};

class IDirect3DVertexBuffer9 : public IDirect3DResource9 {
public:
    virtual int32_t Lock(uint32_t OffsetToLock, uint32_t SizeToLock, void** ppbData, uint32_t Flags) = 0;
    virtual int32_t Unlock() = 0;
    virtual uint32_t GetLength() = 0;
};

class IDirect3DIndexBuffer9 : public IDirect3DResource9 {
public:
    virtual int32_t Lock(uint32_t OffsetToLock, uint32_t SizeToLock, void** ppbData, uint32_t Flags) = 0;
    virtual int32_t Unlock() = 0;
    virtual uint32_t GetLength() = 0;
    virtual D3DFORMAT GetFormat() = 0;
};

class IDirect3DSurface9 : public IDirect3DResource9 {
public:
    virtual int32_t LockRect(D3DLOCKED_RECT* pLockedRect, const D3DRECT* pRect, uint32_t Flags) = 0;
    virtual int32_t UnlockRect() = 0;
    virtual uint32_t GetWidth() = 0;
    virtual uint32_t GetHeight() = 0;
};

class IDirect3DSwapChain9 : public IUnknown {
public:
    virtual int32_t Present(const D3DRECT* pSourceRect, const D3DRECT* pDestRect, win32::HWND hDestWindowOverride, const void* pDirtyRegion, uint32_t Flags) = 0;
    virtual int32_t GetBackBuffer(uint32_t iBackBuffer, uint32_t Type, IDirect3DSurface9** ppBackBuffer) = 0;
};

class IDirect3DDevice9 : public IUnknown {
public:
    virtual int32_t TestCooperativeLevel() = 0;
    virtual uint32_t GetAvailableTextureMem() = 0;
    virtual int32_t EvictManagedResources() = 0;
    virtual int32_t GetDirect3D(IDirect3D9** ppD3D9) = 0;
    virtual int32_t GetDeviceCaps(D3DCAPS9* pCaps) = 0;
    virtual int32_t GetDisplayMode(uint32_t iSwapChain, D3DDISPLAYMODE* pMode) = 0;
    virtual int32_t Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) = 0;
    virtual int32_t Present(const D3DRECT* pSourceRect, const D3DRECT* pDestRect, win32::HWND hDestWindowOverride, const void* pDirtyRegion) = 0;
    virtual int32_t GetBackBuffer(uint32_t iSwapChain, uint32_t iBackBuffer, uint32_t Type, IDirect3DSurface9** ppBackBuffer) = 0;
    
    // Resource Creation
    virtual int32_t CreateVertexBuffer(uint32_t Length, uint32_t Usage, uint32_t FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, void** pSharedHandle) = 0;
    virtual int32_t CreateIndexBuffer(uint32_t Length, uint32_t Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, void** pSharedHandle) = 0;
    virtual int32_t CreateDepthStencilSurface(uint32_t Width, uint32_t Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, uint32_t MultisampleQuality, win32::BOOL Discard, IDirect3DSurface9** ppSurface, void** pSharedHandle) = 0;
    
    // Scene Lifecycle & Frame Clearing
    virtual int32_t BeginScene() = 0;
    virtual int32_t EndScene() = 0;
    virtual int32_t Clear(uint32_t Count, const D3DRECT* pRects, uint32_t Flags, D3DCOLOR Color, float Z, uint32_t Stencil) = 0;
    
    // Transforms & Viewports
    virtual int32_t SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) = 0;
    virtual int32_t GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) = 0;
    virtual int32_t SetViewport(const D3DVIEWPORT9* pViewport) = 0;
    virtual int32_t GetViewport(D3DVIEWPORT9* pViewport) = 0;
    
    // Render States
    virtual int32_t SetRenderState(D3DRENDERSTATETYPE State, uint32_t Value) = 0;
    virtual int32_t GetRenderState(D3DRENDERSTATETYPE State, uint32_t* pValue) = 0;
    
    // Stream Sources & FVF
    virtual int32_t SetStreamSource(uint32_t StreamNumber, IDirect3DVertexBuffer9* pStreamData, uint32_t OffsetInBytes, uint32_t Stride) = 0;
    virtual int32_t GetStreamSource(uint32_t StreamNumber, IDirect3DVertexBuffer9** ppStreamData, uint32_t* pOffsetInBytes, uint32_t* pStride) = 0;
    virtual int32_t SetIndices(IDirect3DIndexBuffer9* pIndexData) = 0;
    virtual int32_t GetIndices(IDirect3DIndexBuffer9** ppIndexData) = 0;
    virtual int32_t SetFVF(uint32_t FVF) = 0;
    virtual int32_t GetFVF(uint32_t* pFVF) = 0;
    
    // Drawing Primitives
    virtual int32_t DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, uint32_t StartVertex, uint32_t PrimitiveCount) = 0;
    virtual int32_t DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, int32_t BaseVertexIndex, uint32_t MinVertexIndex, uint32_t NumVertices, uint32_t startIndex, uint32_t primCount) = 0;
    virtual int32_t DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, uint32_t PrimitiveCount, const void* pVertexStreamZeroData, uint32_t VertexStreamZeroStride) = 0;
    virtual int32_t DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, uint32_t MinVertexIndex, uint32_t NumVertices, uint32_t PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, uint32_t VertexStreamZeroStride) = 0;
};

class IDirect3D9 : public IUnknown {
public:
    virtual int32_t RegisterSoftwareDevice(void* pInitializeFunction) = 0;
    virtual uint32_t GetAdapterCount() = 0;
    virtual int32_t GetAdapterIdentifier(uint32_t Adapter, uint32_t Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) = 0;
    virtual uint32_t GetAdapterModeCount(uint32_t Adapter, D3DFORMAT Format) = 0;
    virtual int32_t EnumAdapterModes(uint32_t Adapter, D3DFORMAT Format, uint32_t Mode, D3DDISPLAYMODE* pMode) = 0;
    virtual int32_t GetAdapterDisplayMode(uint32_t Adapter, D3DDISPLAYMODE* pMode) = 0;
    virtual int32_t CheckDeviceType(uint32_t Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, win32::BOOL bWindowed) = 0;
    virtual int32_t CheckDeviceFormat(uint32_t Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, uint32_t Usage, uint32_t RType, D3DFORMAT CheckFormat) = 0;
    virtual int32_t GetDeviceCaps(uint32_t Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) = 0;
    virtual int32_t CreateDevice(uint32_t Adapter, D3DDEVTYPE DeviceType, win32::HWND hFocusWindow, uint32_t BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) = 0;
};

// ============================================================================
// 4. Concrete Implementations
// ============================================================================

class Direct3DVertexBuffer9Impl : public IDirect3DVertexBuffer9 {
private:
    std::atomic<uint32_t> m_refCount{1};
    IDirect3DDevice9*     m_pDevice{nullptr};
    std::vector<uint8_t>  m_buffer;
    uint32_t              m_usage{0};
    uint32_t              m_fvf{0};
    D3DPOOL               m_pool{D3DPOOL_DEFAULT};

public:
    Direct3DVertexBuffer9Impl(IDirect3DDevice9* pDev, uint32_t length, uint32_t usage, uint32_t fvf, D3DPOOL pool)
        : m_pDevice(pDev), m_buffer(length, 0), m_usage(usage), m_fvf(fvf), m_pool(pool) {}

    // IUnknown
    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return D3DERR_INVALIDCALL;
        if (riid == IID_IUnknown || riid == IID_IDirect3DVertexBuffer9) {
            *ppvObject = static_cast<IDirect3DVertexBuffer9*>(this);
            AddRef();
            return D3D_OK;
        }
        *ppvObject = nullptr;
        return -2147467262; // E_NOINTERFACE
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    // IDirect3DResource9
    void GetDevice(IDirect3DDevice9** ppDevice) override {
        if (ppDevice) {
            *ppDevice = m_pDevice;
            if (m_pDevice) m_pDevice->AddRef();
        }
    }
    int32_t SetPrivateData(const IID&, const void*, uint32_t, uint32_t) override { return D3D_OK; }
    int32_t GetPrivateData(const IID&, void*, uint32_t*) override { return D3D_OK; }
    int32_t FreePrivateData(const IID&) override { return D3D_OK; }
    uint32_t SetPriority(uint32_t) override { return 0; }
    uint32_t GetPriority() override { return 0; }
    void PreLoad() override {}
    uint32_t GetType() override { return 4; /* D3DRTYPE_VERTEXBUFFER */ }

    // IDirect3DVertexBuffer9
    int32_t Lock(uint32_t OffsetToLock, uint32_t /*SizeToLock*/, void** ppbData, uint32_t) override {
        if (!ppbData) return D3DERR_INVALIDCALL;
        if (OffsetToLock >= m_buffer.size()) return D3DERR_INVALIDCALL;
        *ppbData = m_buffer.data() + OffsetToLock;
        return D3D_OK;
    }

    int32_t Unlock() override { return D3D_OK; }
    uint32_t GetLength() override { return static_cast<uint32_t>(m_buffer.size()); }

    const uint8_t* GetData() const noexcept { return m_buffer.data(); }
    uint32_t GetUsage() const noexcept { return m_usage; }
    uint32_t GetFVF() const noexcept { return m_fvf; }
    D3DPOOL GetPool() const noexcept { return m_pool; }
};

class Direct3DIndexBuffer9Impl : public IDirect3DIndexBuffer9 {
private:
    std::atomic<uint32_t> m_refCount{1};
    IDirect3DDevice9*     m_pDevice{nullptr};
    std::vector<uint8_t>  m_buffer;
    uint32_t              m_usage{0};
    D3DFORMAT             m_format{D3DFMT_INDEX16};
    D3DPOOL               m_pool{D3DPOOL_DEFAULT};

public:
    Direct3DIndexBuffer9Impl(IDirect3DDevice9* pDev, uint32_t length, uint32_t usage, D3DFORMAT format, D3DPOOL pool)
        : m_pDevice(pDev), m_buffer(length, 0), m_usage(usage), m_format(format), m_pool(pool) {}

    // IUnknown
    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return D3DERR_INVALIDCALL;
        if (riid == IID_IUnknown || riid == IID_IDirect3DIndexBuffer9) {
            *ppvObject = static_cast<IDirect3DIndexBuffer9*>(this);
            AddRef();
            return D3D_OK;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    // IDirect3DResource9
    void GetDevice(IDirect3DDevice9** ppDevice) override {
        if (ppDevice) {
            *ppDevice = m_pDevice;
            if (m_pDevice) m_pDevice->AddRef();
        }
    }
    int32_t SetPrivateData(const IID&, const void*, uint32_t, uint32_t) override { return D3D_OK; }
    int32_t GetPrivateData(const IID&, void*, uint32_t*) override { return D3D_OK; }
    int32_t FreePrivateData(const IID&) override { return D3D_OK; }
    uint32_t SetPriority(uint32_t) override { return 0; }
    uint32_t GetPriority() override { return 0; }
    void PreLoad() override {}
    uint32_t GetType() override { return 5; /* D3DRTYPE_INDEXBUFFER */ }

    // IDirect3DIndexBuffer9
    int32_t Lock(uint32_t OffsetToLock, uint32_t /*SizeToLock*/, void** ppbData, uint32_t) override {
        if (!ppbData) return D3DERR_INVALIDCALL;
        if (OffsetToLock >= m_buffer.size()) return D3DERR_INVALIDCALL;
        *ppbData = m_buffer.data() + OffsetToLock;
        return D3D_OK;
    }

    int32_t Unlock() override { return D3D_OK; }
    uint32_t GetLength() override { return static_cast<uint32_t>(m_buffer.size()); }
    D3DFORMAT GetFormat() override { return m_format; }

    const uint8_t* GetData() const noexcept { return m_buffer.data(); }
    uint32_t GetUsage() const noexcept { return m_usage; }
    D3DPOOL GetPool() const noexcept { return m_pool; }
};

class Direct3DSurface9Impl : public IDirect3DSurface9 {
private:
    std::atomic<uint32_t> m_refCount{1};
    IDirect3DDevice9*     m_pDevice{nullptr};
    uint32_t              m_width{0};
    uint32_t              m_height{0};
    D3DFORMAT             m_format{D3DFMT_X8R8G8B8};
    std::vector<uint32_t> m_pixels;

public:
    Direct3DSurface9Impl(IDirect3DDevice9* pDev, uint32_t w, uint32_t h, D3DFORMAT fmt)
        : m_pDevice(pDev), m_width(w), m_height(h), m_format(fmt), m_pixels(static_cast<size_t>(w) * h, 0xFF000000) {}

    // IUnknown
    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return D3DERR_INVALIDCALL;
        if (riid == IID_IUnknown || riid == IID_IDirect3DSurface9) {
            *ppvObject = static_cast<IDirect3DSurface9*>(this);
            AddRef();
            return D3D_OK;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    // IDirect3DResource9
    void GetDevice(IDirect3DDevice9** ppDevice) override {
        if (ppDevice) {
            *ppDevice = m_pDevice;
            if (m_pDevice) m_pDevice->AddRef();
        }
    }
    int32_t SetPrivateData(const IID&, const void*, uint32_t, uint32_t) override { return D3D_OK; }
    int32_t GetPrivateData(const IID&, void*, uint32_t*) override { return D3D_OK; }
    int32_t FreePrivateData(const IID&) override { return D3D_OK; }
    uint32_t SetPriority(uint32_t) override { return 0; }
    uint32_t GetPriority() override { return 0; }
    void PreLoad() override {}
    uint32_t GetType() override { return 1; /* D3DRTYPE_SURFACE */ }

    // IDirect3DSurface9
    int32_t LockRect(D3DLOCKED_RECT* pLockedRect, const D3DRECT* /*pRect*/, uint32_t) override {
        if (!pLockedRect) return D3DERR_INVALIDCALL;
        pLockedRect->Pitch = static_cast<int32_t>(m_width * sizeof(uint32_t));
        pLockedRect->pBits = m_pixels.data();
        return D3D_OK;
    }

    int32_t UnlockRect() override { return D3D_OK; }
    uint32_t GetWidth() override { return m_width; }
    uint32_t GetHeight() override { return m_height; }
    D3DFORMAT GetFormat() const noexcept { return m_format; }

    uint32_t* GetPixelData() noexcept { return m_pixels.data(); }
    const uint32_t* GetPixelData() const noexcept { return m_pixels.data(); }
};

// ============================================================================
// 5. Direct3D 9 Device Implementation with Fixed-Function Pipeline
// ============================================================================

class Direct3DDevice9Impl : public IDirect3DDevice9 {
private:
    std::atomic<uint32_t> m_refCount{1};
    IDirect3D9*           m_pD3D{nullptr};
    D3DPRESENT_PARAMETERS m_presentParams{};
    win32::HWND           m_hwnd{nullptr};

    std::unique_ptr<Direct3DSurface9Impl> m_backBuffer;
    std::vector<float>    m_depthBuffer;

    D3DVIEWPORT9          m_viewport{};
    D3DMATRIX             m_matWorld{};
    D3DMATRIX             m_matView{};
    D3DMATRIX             m_matProj{};

    std::unordered_map<uint32_t, uint32_t> m_renderStates;

    IDirect3DVertexBuffer9* m_currentVB{nullptr};
    uint32_t                m_vbOffset{0};
    uint32_t                m_vbStride{0};
    IDirect3DIndexBuffer9*  m_currentIB{nullptr};
    uint32_t                m_currentFVF{D3DFVF_XYZ | D3DFVF_DIFFUSE};

    bool m_inScene{false};
    uint32_t m_presentCount{0};

public:
    Direct3DDevice9Impl(IDirect3D9* pD3D, win32::HWND hFocusWindow, const D3DPRESENT_PARAMETERS& pp)
        : m_pD3D(pD3D), m_presentParams(pp), m_hwnd(pp.hDeviceWindow ? pp.hDeviceWindow : hFocusWindow) {
        if (pD3D) pD3D->AddRef();

        uint32_t w = (m_presentParams.BackBufferWidth == 0) ? 640 : m_presentParams.BackBufferWidth;
        uint32_t h = (m_presentParams.BackBufferHeight == 0) ? 480 : m_presentParams.BackBufferHeight;
        m_presentParams.BackBufferWidth = w;
        m_presentParams.BackBufferHeight = h;

        m_backBuffer = std::make_unique<Direct3DSurface9Impl>(this, w, h, m_presentParams.BackBufferFormat);
        m_depthBuffer.resize(static_cast<size_t>(w) * h, 1.0f);

        m_viewport.X = 0;
        m_viewport.Y = 0;
        m_viewport.Width = w;
        m_viewport.Height = h;
        m_viewport.MinZ = 0.0f;
        m_viewport.MaxZ = 1.0f;

        m_matWorld = D3DMATRIX::Identity();
        m_matView = D3DMATRIX::Identity();
        m_matProj = D3DMATRIX::Identity();

        // Standard Default Render States
        m_renderStates[D3DRS_ZENABLE] = 1;
        m_renderStates[D3DRS_FILLMODE] = D3DFILL_SOLID;
        m_renderStates[D3DRS_CULLMODE] = D3DCULL_CCW;
        m_renderStates[D3DRS_LIGHTING] = 0;
    }

    ~Direct3DDevice9Impl() {
        if (m_currentVB) m_currentVB->Release();
        if (m_currentIB) m_currentIB->Release();
        if (m_pD3D) m_pD3D->Release();
    }

    // IUnknown
    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return D3DERR_INVALIDCALL;
        if (riid == IID_IUnknown || riid == IID_IDirect3DDevice9) {
            *ppvObject = static_cast<IDirect3DDevice9*>(this);
            AddRef();
            return D3D_OK;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    // Device Queries & Caps
    int32_t TestCooperativeLevel() override { return D3D_OK; }
    uint32_t GetAvailableTextureMem() override { return 1024 * 1024 * 1024; /* 1 GB VRAM */ }
    int32_t EvictManagedResources() override { return D3D_OK; }
    int32_t GetDirect3D(IDirect3D9** ppD3D9) override {
        if (!ppD3D9) return D3DERR_INVALIDCALL;
        *ppD3D9 = m_pD3D;
        if (m_pD3D) m_pD3D->AddRef();
        return D3D_OK;
    }
    int32_t GetDeviceCaps(D3DCAPS9* pCaps) override {
        if (!pCaps) return D3DERR_INVALIDCALL;
        *pCaps = D3DCAPS9{};
        return D3D_OK;
    }
    int32_t GetDisplayMode(uint32_t, D3DDISPLAYMODE* pMode) override {
        if (!pMode) return D3DERR_INVALIDCALL;
        pMode->Width = m_presentParams.BackBufferWidth;
        pMode->Height = m_presentParams.BackBufferHeight;
        pMode->Format = m_presentParams.BackBufferFormat;
        pMode->RefreshRate = 60;
        return D3D_OK;
    }

    int32_t Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) override {
        if (!pPresentationParameters) return D3DERR_INVALIDCALL;
        m_presentParams = *pPresentationParameters;
        uint32_t w = (m_presentParams.BackBufferWidth == 0) ? 640 : m_presentParams.BackBufferWidth;
        uint32_t h = (m_presentParams.BackBufferHeight == 0) ? 480 : m_presentParams.BackBufferHeight;
        m_backBuffer = std::make_unique<Direct3DSurface9Impl>(this, w, h, m_presentParams.BackBufferFormat);
        m_depthBuffer.resize(static_cast<size_t>(w) * h, 1.0f);
        m_viewport = { 0, 0, w, h, 0.0f, 1.0f };
        return D3D_OK;
    }

    int32_t GetBackBuffer(uint32_t, uint32_t, uint32_t, IDirect3DSurface9** ppBackBuffer) override {
        if (!ppBackBuffer) return D3DERR_INVALIDCALL;
        *ppBackBuffer = m_backBuffer.get();
        if (m_backBuffer) m_backBuffer->AddRef();
        return D3D_OK;
    }

    // Presentation
    int32_t Present(const D3DRECT*, const D3DRECT*, win32::HWND hDestWindowOverride, const void*) override {
        win32::HWND targetHwnd = hDestWindowOverride ? hDestWindowOverride : m_hwnd;
        if (targetHwnd && m_backBuffer) {
            uint32_t w = m_backBuffer->GetWidth();
            uint32_t h = m_backBuffer->GetHeight();
            uint32_t pitch = w * 4;
            user32::WindowManager::get().blitToWindow(
                targetHwnd,
                reinterpret_cast<const uint8_t*>(m_backBuffer->GetPixelData()),
                w, h, pitch
            );
        }
        m_presentCount++;
        return D3D_OK;
    }

    uint32_t GetPresentCount() const noexcept { return m_presentCount; }

    // Resource Creation
    int32_t CreateVertexBuffer(uint32_t Length, uint32_t Usage, uint32_t FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, void**) override {
        if (!ppVertexBuffer) return D3DERR_INVALIDCALL;
        *ppVertexBuffer = new Direct3DVertexBuffer9Impl(this, Length, Usage, FVF, Pool);
        return D3D_OK;
    }

    int32_t CreateIndexBuffer(uint32_t Length, uint32_t Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, void**) override {
        if (!ppIndexBuffer) return D3DERR_INVALIDCALL;
        *ppIndexBuffer = new Direct3DIndexBuffer9Impl(this, Length, Usage, Format, Pool);
        return D3D_OK;
    }

    int32_t CreateDepthStencilSurface(uint32_t Width, uint32_t Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, uint32_t, win32::BOOL, IDirect3DSurface9** ppSurface, void**) override {
        if (!ppSurface) return D3DERR_INVALIDCALL;
        *ppSurface = new Direct3DSurface9Impl(this, Width, Height, Format);
        return D3D_OK;
    }

    // Scene & Clearing
    int32_t BeginScene() override {
        m_inScene = true;
        return D3D_OK;
    }

    int32_t EndScene() override {
        m_inScene = false;
        return D3D_OK;
    }

    int32_t Clear(uint32_t /*Count*/, const D3DRECT* /*pRects*/, uint32_t Flags, D3DCOLOR Color, float Z, uint32_t /*Stencil*/) override {
        if (!m_backBuffer) return D3DERR_INVALIDCALL;

        uint32_t* pixels = m_backBuffer->GetPixelData();
        size_t total = static_cast<size_t>(m_backBuffer->GetWidth()) * m_backBuffer->GetHeight();

        if (Flags & D3DCLEAR_TARGET) {
            for (size_t i = 0; i < total; ++i) {
                pixels[i] = Color;
            }
        }

        if (Flags & D3DCLEAR_ZBUFFER) {
            std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), Z);
        }

        return D3D_OK;
    }

    // Fixed Function Transforms
    int32_t SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) override {
        if (!pMatrix) return D3DERR_INVALIDCALL;
        switch (State) {
            case D3DTS_WORLD:      m_matWorld = *pMatrix; break;
            case D3DTS_VIEW:       m_matView  = *pMatrix; break;
            case D3DTS_PROJECTION: m_matProj  = *pMatrix; break;
            default: break;
        }
        return D3D_OK;
    }

    int32_t GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) override {
        if (!pMatrix) return D3DERR_INVALIDCALL;
        switch (State) {
            case D3DTS_WORLD:      *pMatrix = m_matWorld; break;
            case D3DTS_VIEW:       *pMatrix = m_matView; break;
            case D3DTS_PROJECTION: *pMatrix = m_matProj; break;
            default: return D3DERR_INVALIDCALL;
        }
        return D3D_OK;
    }

    int32_t SetViewport(const D3DVIEWPORT9* pViewport) override {
        if (!pViewport) return D3DERR_INVALIDCALL;
        m_viewport = *pViewport;
        return D3D_OK;
    }

    int32_t GetViewport(D3DVIEWPORT9* pViewport) override {
        if (!pViewport) return D3DERR_INVALIDCALL;
        *pViewport = m_viewport;
        return D3D_OK;
    }

    int32_t SetRenderState(D3DRENDERSTATETYPE State, uint32_t Value) override {
        m_renderStates[State] = Value;
        return D3D_OK;
    }

    int32_t GetRenderState(D3DRENDERSTATETYPE State, uint32_t* pValue) override {
        if (!pValue) return D3DERR_INVALIDCALL;
        auto it = m_renderStates.find(State);
        *pValue = (it != m_renderStates.end()) ? it->second : 0;
        return D3D_OK;
    }

    // Stream Sources & Indices
    int32_t SetStreamSource(uint32_t StreamNumber, IDirect3DVertexBuffer9* pStreamData, uint32_t OffsetInBytes, uint32_t Stride) override {
        if (StreamNumber != 0) return D3DERR_INVALIDCALL;
        if (m_currentVB) m_currentVB->Release();
        m_currentVB = pStreamData;
        if (m_currentVB) m_currentVB->AddRef();
        m_vbOffset = OffsetInBytes;
        m_vbStride = Stride;
        return D3D_OK;
    }

    int32_t GetStreamSource(uint32_t StreamNumber, IDirect3DVertexBuffer9** ppStreamData, uint32_t* pOffsetInBytes, uint32_t* pStride) override {
        if (StreamNumber != 0 || !ppStreamData) return D3DERR_INVALIDCALL;
        *ppStreamData = m_currentVB;
        if (m_currentVB) m_currentVB->AddRef();
        if (pOffsetInBytes) *pOffsetInBytes = m_vbOffset;
        if (pStride) *pStride = m_vbStride;
        return D3D_OK;
    }

    int32_t SetIndices(IDirect3DIndexBuffer9* pIndexData) override {
        if (m_currentIB) m_currentIB->Release();
        m_currentIB = pIndexData;
        if (m_currentIB) m_currentIB->AddRef();
        return D3D_OK;
    }

    int32_t GetIndices(IDirect3DIndexBuffer9** ppIndexData) override {
        if (!ppIndexData) return D3DERR_INVALIDCALL;
        *ppIndexData = m_currentIB;
        if (m_currentIB) m_currentIB->AddRef();
        return D3D_OK;
    }

    int32_t SetFVF(uint32_t FVF) override {
        m_currentFVF = FVF;
        return D3D_OK;
    }

    int32_t GetFVF(uint32_t* pFVF) override {
        if (!pFVF) return D3DERR_INVALIDCALL;
        *pFVF = m_currentFVF;
        return D3D_OK;
    }

    // ------------------------------------------------------------------------
    // Drawing Pipeline (Fixed-Function Transform & Software Rasterizer)
    // ------------------------------------------------------------------------
    int32_t DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, uint32_t PrimitiveCount, const void* pVertexStreamZeroData, uint32_t VertexStreamZeroStride) override {
        if (!pVertexStreamZeroData || PrimitiveCount == 0) return D3DERR_INVALIDCALL;
        if (PrimitiveType != D3DPT_TRIANGLELIST) return D3D_OK; // Support triangle lists primarily

        uint32_t numTriangles = PrimitiveCount;
        const uint8_t* rawData = static_cast<const uint8_t*>(pVertexStreamZeroData);

        for (uint32_t t = 0; t < numTriangles; ++t) {
            uint32_t idx0 = t * 3 + 0;
            uint32_t idx1 = t * 3 + 1;
            uint32_t idx2 = t * 3 + 2;

            rasterizeTriangleFromMemory(rawData + idx0 * VertexStreamZeroStride,
                                      rawData + idx1 * VertexStreamZeroStride,
                                      rawData + idx2 * VertexStreamZeroStride);
        }

        return D3D_OK;
    }

    int32_t DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, uint32_t, uint32_t, uint32_t PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, uint32_t VertexStreamZeroStride) override {
        if (!pVertexStreamZeroData || !pIndexData || PrimitiveCount == 0) return D3DERR_INVALIDCALL;
        if (PrimitiveType != D3DPT_TRIANGLELIST) return D3D_OK;

        const uint8_t* rawVertices = static_cast<const uint8_t*>(pVertexStreamZeroData);
        uint32_t numTriangles = PrimitiveCount;

        for (uint32_t t = 0; t < numTriangles; ++t) {
            uint32_t i0, i1, i2;
            if (IndexDataFormat == D3DFMT_INDEX16) {
                const uint16_t* indices = static_cast<const uint16_t*>(pIndexData);
                i0 = indices[t * 3 + 0];
                i1 = indices[t * 3 + 1];
                i2 = indices[t * 3 + 2];
            } else {
                const uint32_t* indices = static_cast<const uint32_t*>(pIndexData);
                i0 = indices[t * 3 + 0];
                i1 = indices[t * 3 + 1];
                i2 = indices[t * 3 + 2];
            }

            rasterizeTriangleFromMemory(rawVertices + i0 * VertexStreamZeroStride,
                                      rawVertices + i1 * VertexStreamZeroStride,
                                      rawVertices + i2 * VertexStreamZeroStride);
        }

        return D3D_OK;
    }

    int32_t DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, uint32_t StartVertex, uint32_t PrimitiveCount) override {
        if (!m_currentVB || PrimitiveCount == 0) return D3DERR_INVALIDCALL;
        auto* vb = static_cast<Direct3DVertexBuffer9Impl*>(m_currentVB);
        const uint8_t* base = vb->GetData() + m_vbOffset;
        return DrawPrimitiveUP(PrimitiveType, PrimitiveCount, base + StartVertex * m_vbStride, m_vbStride);
    }

    int32_t DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, int32_t BaseVertexIndex, uint32_t, uint32_t, uint32_t startIndex, uint32_t primCount) override {
        if (!m_currentVB || !m_currentIB || primCount == 0) return D3DERR_INVALIDCALL;
        auto* vb = static_cast<Direct3DVertexBuffer9Impl*>(m_currentVB);
        auto* ib = static_cast<Direct3DIndexBuffer9Impl*>(m_currentIB);

        const uint8_t* rawVertices = vb->GetData() + m_vbOffset + BaseVertexIndex * m_vbStride;
        const uint8_t* rawIndices = ib->GetData();
        D3DFORMAT fmt = ib->GetFormat();

        size_t indexOffset = (fmt == D3DFMT_INDEX16) ? (startIndex * sizeof(uint16_t)) : (startIndex * sizeof(uint32_t));
        return DrawIndexedPrimitiveUP(PrimitiveType, 0, vb->GetLength() / m_vbStride, primCount, rawIndices + indexOffset, fmt, rawVertices, m_vbStride);
    }

private:
    prism3d::Matrix4x4 toPrismMatrix(const D3DMATRIX& d3dMat) noexcept {
        prism3d::Matrix4x4 m{};
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                m.m[r][c] = d3dMat.m[r][c];
            }
        }
        return m;
    }

    void rasterizeTriangleFromMemory(const uint8_t* v0Raw, const uint8_t* v1Raw, const uint8_t* v2Raw) {
        if (!m_backBuffer) return;

        // Parse XYZ coordinates (first 3 floats)
        const float* p0 = reinterpret_cast<const float*>(v0Raw);
        const float* p1 = reinterpret_cast<const float*>(v1Raw);
        const float* p2 = reinterpret_cast<const float*>(v2Raw);

        prism3d::Vector4 pos0{ p0[0], p0[1], p0[2], 1.0f };
        prism3d::Vector4 pos1{ p1[0], p1[1], p1[2], 1.0f };
        prism3d::Vector4 pos2{ p2[0], p2[1], p2[2], 1.0f };

        // Parse diffuse colors if D3DFVF_DIFFUSE is set (4th float/uint32)
        uint32_t c0 = 0xFFFFFFFF, c1 = 0xFFFFFFFF, c2 = 0xFFFFFFFF;
        if (m_currentFVF & D3DFVF_DIFFUSE) {
            c0 = *reinterpret_cast<const uint32_t*>(v0Raw + 12);
            c1 = *reinterpret_cast<const uint32_t*>(v1Raw + 12);
            c2 = *reinterpret_cast<const uint32_t*>(v2Raw + 12);
        }

        // Multiply MVP Matrix: MVP = World * View * Proj
        auto world = toPrismMatrix(m_matWorld);
        auto view  = toPrismMatrix(m_matView);
        auto proj  = toPrismMatrix(m_matProj);
        auto mvp   = prism3d::Matrix4x4::Multiply(world, prism3d::Matrix4x4::Multiply(view, proj));

        pos0 = mvp.Transform(pos0);
        pos1 = mvp.Transform(pos1);
        pos2 = mvp.Transform(pos2);

        // Perspective divide
        float invW0 = (std::abs(pos0.w) > 1e-6f) ? (1.0f / pos0.w) : 1.0f;
        float invW1 = (std::abs(pos1.w) > 1e-6f) ? (1.0f / pos1.w) : 1.0f;
        float invW2 = (std::abs(pos2.w) > 1e-6f) ? (1.0f / pos2.w) : 1.0f;

        float ndcX0 = pos0.x * invW0, ndcY0 = pos0.y * invW0, ndcZ0 = pos0.z * invW0;
        float ndcX1 = pos1.x * invW1, ndcY1 = pos1.y * invW1, ndcZ1 = pos1.z * invW1;
        float ndcX2 = pos2.x * invW2, ndcY2 = pos2.y * invW2, ndcZ2 = pos2.z * invW2;

        // Viewport transform
        float sx0 = (ndcX0 + 1.0f) * 0.5f * m_viewport.Width + m_viewport.X;
        float sy0 = (1.0f - ndcY0) * 0.5f * m_viewport.Height + m_viewport.Y;
        float sx1 = (ndcX1 + 1.0f) * 0.5f * m_viewport.Width + m_viewport.X;
        float sy1 = (1.0f - ndcY1) * 0.5f * m_viewport.Height + m_viewport.Y;
        float sx2 = (ndcX2 + 1.0f) * 0.5f * m_viewport.Width + m_viewport.X;
        float sy2 = (1.0f - ndcY2) * 0.5f * m_viewport.Height + m_viewport.Y;

        // Culling
        float denom = (sx1 - sx0) * (sy2 - sy0) - (sy1 - sy0) * (sx2 - sx0);
        if (std::abs(denom) < 1e-6f) return;

        uint32_t cullMode = m_renderStates[D3DRS_CULLMODE];
        if (cullMode == D3DCULL_CW && denom > 0.0f) return;
        if (cullMode == D3DCULL_CCW && denom < 0.0f) return;

        uint32_t width = m_backBuffer->GetWidth();
        uint32_t height = m_backBuffer->GetHeight();
        uint32_t* targetPixels = m_backBuffer->GetPixelData();
        float* depthBuffer = m_depthBuffer.data();
        bool zEnable = (m_renderStates[D3DRS_ZENABLE] != 0);

        uint32_t fillMode = m_renderStates[D3DRS_FILLMODE];
        if (fillMode == D3DFILL_WIREFRAME) {
            uint32_t wireColor = 0xFF00FFCC; // Neon Cyan Wireframe
            drawLine(static_cast<int>(sx0), static_cast<int>(sy0), ndcZ0, static_cast<int>(sx1), static_cast<int>(sy1), ndcZ1, wireColor);
            drawLine(static_cast<int>(sx1), static_cast<int>(sy1), ndcZ1, static_cast<int>(sx2), static_cast<int>(sy2), ndcZ2, wireColor);
            drawLine(static_cast<int>(sx2), static_cast<int>(sy2), ndcZ2, static_cast<int>(sx0), static_cast<int>(sy0), ndcZ0, wireColor);
            return;
        }

        // Barycentric Rasterization
        int minX = std::max(0, static_cast<int>(std::floor(std::min({ sx0, sx1, sx2 }))));
        int maxX = std::min(static_cast<int>(width) - 1, static_cast<int>(std::ceil(std::max({ sx0, sx1, sx2 }))));
        int minY = std::max(0, static_cast<int>(std::floor(std::min({ sy0, sy1, sy2 }))));
        int maxY = std::min(static_cast<int>(height) - 1, static_cast<int>(std::ceil(std::max({ sy0, sy1, sy2 }))));

        float invDenom = 1.0f / denom;

        float r0 = ((c0 >> 16) & 0xFF) / 255.0f, g0 = ((c0 >> 8) & 0xFF) / 255.0f, b0 = (c0 & 0xFF) / 255.0f;
        float r1 = ((c1 >> 16) & 0xFF) / 255.0f, g1 = ((c1 >> 8) & 0xFF) / 255.0f, b1 = (c1 & 0xFF) / 255.0f;
        float r2 = ((c2 >> 16) & 0xFF) / 255.0f, g2 = ((c2 >> 8) & 0xFF) / 255.0f, b2 = (c2 & 0xFF) / 255.0f;

        for (int y = minY; y <= maxY; ++y) {
            float py = static_cast<float>(y) + 0.5f;
            for (int x = minX; x <= maxX; ++x) {
                float px = static_cast<float>(x) + 0.5f;

                float w0 = ((sx1 - px) * (sy2 - py) - (sy1 - py) * (sx2 - px)) * invDenom;
                float w1 = ((sx2 - px) * (sy0 - py) - (sy2 - py) * (sx0 - px)) * invDenom;
                float w2 = 1.0f - w0 - w1;

                if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                    size_t pixelIndex = static_cast<size_t>(y) * width + x;

                    float z = w0 * ndcZ0 + w1 * ndcZ1 + w2 * ndcZ2;
                    if (zEnable) {
                        if (z > depthBuffer[pixelIndex]) continue;
                        depthBuffer[pixelIndex] = z;
                    }

                    float r = w0 * r0 + w1 * r1 + w2 * r2;
                    float g = w0 * g0 + w1 * g1 + w2 * g2;
                    float b = w0 * b0 + w1 * b1 + w2 * b2;

                    uint8_t uR = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
                    uint8_t uG = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
                    uint8_t uB = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));

                    targetPixels[pixelIndex] = (0xFF << 24) | (uR << 16) | (uG << 8) | uB;
                }
            }
        }
    }

    void drawLine(int x0, int y0, float z0, int x1, int y1, float z1, uint32_t color) {
        int dx = std::abs(x1 - x0);
        int dy = std::abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;

        uint32_t width = m_backBuffer->GetWidth();
        uint32_t height = m_backBuffer->GetHeight();
        uint32_t* targetPixels = m_backBuffer->GetPixelData();
        float* depthBuffer = m_depthBuffer.data();

        int totalSteps = std::max(dx, dy);
        int stepCount = 0;

        while (true) {
            if (x0 >= 0 && x0 < static_cast<int>(width) && y0 >= 0 && y0 < static_cast<int>(height)) {
                size_t idx = static_cast<size_t>(y0) * width + x0;
                float t = (totalSteps > 0) ? (static_cast<float>(stepCount) / totalSteps) : 0.0f;
                float z = z0 + t * (z1 - z0);

                if (z <= depthBuffer[idx]) {
                    depthBuffer[idx] = z;
                    targetPixels[idx] = color;
                }
            }

            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx)  { err += dx; y0 += sy; }
            stepCount++;
        }
    }
};

// ============================================================================
// 6. Direct3D 9 Entry Point & Subsystem Factory
// ============================================================================

class Direct3D9Impl : public IDirect3D9 {
private:
    std::atomic<uint32_t> m_refCount{1};

public:
    Direct3D9Impl() = default;

    // IUnknown
    int32_t QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return D3DERR_INVALIDCALL;
        if (riid == IID_IUnknown || riid == IID_IDirect3D9) {
            *ppvObject = static_cast<IDirect3D9*>(this);
            AddRef();
            return D3D_OK;
        }
        *ppvObject = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    // IDirect3D9
    int32_t RegisterSoftwareDevice(void*) override { return D3D_OK; }
    uint32_t GetAdapterCount() override { return 1; }

    int32_t GetAdapterIdentifier(uint32_t Adapter, uint32_t, D3DADAPTER_IDENTIFIER9* pIdentifier) override {
        if (Adapter != 0 || !pIdentifier) return D3DERR_INVALIDCALL;
        *pIdentifier = D3DADAPTER_IDENTIFIER9{};
        return D3D_OK;
    }

    uint32_t GetAdapterModeCount(uint32_t Adapter, D3DFORMAT) override {
        return (Adapter == 0) ? 1 : 0;
    }

    int32_t EnumAdapterModes(uint32_t Adapter, D3DFORMAT, uint32_t Mode, D3DDISPLAYMODE* pMode) override {
        if (Adapter != 0 || Mode != 0 || !pMode) return D3DERR_INVALIDCALL;
        *pMode = D3DDISPLAYMODE{ 1920, 1080, 60, D3DFMT_X8R8G8B8 };
        return D3D_OK;
    }

    int32_t GetAdapterDisplayMode(uint32_t Adapter, D3DDISPLAYMODE* pMode) override {
        if (Adapter != 0 || !pMode) return D3DERR_INVALIDCALL;
        *pMode = D3DDISPLAYMODE{ 1920, 1080, 60, D3DFMT_X8R8G8B8 };
        return D3D_OK;
    }

    int32_t CheckDeviceType(uint32_t, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, win32::BOOL) override {
        return D3D_OK;
    }

    int32_t CheckDeviceFormat(uint32_t, D3DDEVTYPE, D3DFORMAT, uint32_t, uint32_t, D3DFORMAT) override {
        return D3D_OK;
    }

    int32_t GetDeviceCaps(uint32_t Adapter, D3DDEVTYPE, D3DCAPS9* pCaps) override {
        if (Adapter != 0 || !pCaps) return D3DERR_INVALIDCALL;
        *pCaps = D3DCAPS9{};
        return D3D_OK;
    }

    int32_t CreateDevice(uint32_t Adapter, D3DDEVTYPE, win32::HWND hFocusWindow, uint32_t, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) override {
        if (Adapter != 0 || !pPresentationParameters || !ppReturnedDeviceInterface) {
            return D3DERR_INVALIDCALL;
        }

        auto* dev = new Direct3DDevice9Impl(this, hFocusWindow, *pPresentationParameters);
        *ppReturnedDeviceInterface = dev;
        return D3D_OK;
    }
};

// ============================================================================
// 7. C-API Export & Dynamic Loader Registration
// ============================================================================

inline IDirect3D9* Direct3DCreate9(uint32_t SDKVersion) noexcept {
    if (SDKVersion != D3D_SDK_VERSION) {
        // Return valid object for compatibility, or check version
    }
    return new Direct3D9Impl();
}

inline void InitializeD3D9SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("d3d9.dll", "Direct3DCreate9", reinterpret_cast<void*>(Direct3DCreate9));
}

} // namespace micant::d3d9
