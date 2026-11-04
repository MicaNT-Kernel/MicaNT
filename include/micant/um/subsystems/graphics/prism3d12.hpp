// ============================================================================
// MicaNT: Prism3D12 Low-Level Graphics Subsystem (Direct3D 12 Parity)
// 
// Strict Clean-Room Implementation based on Microsoft's MIT-licensed:
//   - https://github.com/microsoft/DirectX-Headers (directx/d3d12*.h)
//   - https://github.com/microsoft/win32metadata
//
// Subsystem Overview:
//   Prism3D12 provides the explicit, low-level GPU control architecture
//   analogous to Direct3D 12 (d3d12.dll). It features asynchronous command
//   allocators, command lists, pipeline state objects (PSOs), explicit
//   resource barrier state transitions, GPU/CPU fence synchronization,
//   and descriptor heaps.
//
// Trademark & Nominative Fair Use Notice:
//   Direct3D, DirectX, and D3D12 are registered trademarks of Microsoft
//   Corporation. Prism3D12 is an independent sovereign clean-room implementation
//   engineered for the MicaNT operating system executive.
// ============================================================================

#pragma once
#include "micant/um/subsystems/graphics/prismx.hpp"
#include "micant/um/subsystems/graphics/prism3d.hpp"
#include "micant/km/drivers/video/dxgkrnl.hpp"
#include "micant/um/native/ldr.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cstring>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace micant::prism3d12 {

using namespace micant::prismx;
using namespace micant::prism3d;

using D3D12_PRIMITIVE_TOPOLOGY = D3D_PRIMITIVE_TOPOLOGY;

// ============================================================================
// 1. D3D12 Enumerations & Constants
// ============================================================================

enum D3D12_COMMAND_LIST_TYPE : uint32_t {
    D3D12_COMMAND_LIST_TYPE_DIRECT        = 0,
    D3D12_COMMAND_LIST_TYPE_BUNDLE        = 1,
    D3D12_COMMAND_LIST_TYPE_COMPUTE       = 2,
    D3D12_COMMAND_LIST_TYPE_COPY          = 3,
    D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE  = 4,
    D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS = 5,
    D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE  = 6
};

enum D3D12_COMMAND_QUEUE_FLAGS : uint32_t {
    D3D12_COMMAND_QUEUE_FLAG_NONE = 0
};

enum D3D12_COMMAND_QUEUE_PRIORITY : int32_t {
    D3D12_COMMAND_QUEUE_PRIORITY_NORMAL          = 0,
    D3D12_COMMAND_QUEUE_PRIORITY_HIGH            = 100,
    D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME = 10000
};

enum D3D12_RESOURCE_STATES : uint32_t {
    D3D12_RESOURCE_STATE_COMMON                     = 0,
    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
    D3D12_RESOURCE_STATE_INDEX_BUFFER               = 0x2,
    D3D12_RESOURCE_STATE_RENDER_TARGET              = 0x4,
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS           = 0x8,
    D3D12_RESOURCE_STATE_DEPTH_WRITE                = 0x10,
    D3D12_RESOURCE_STATE_DEPTH_READ                 = 0x20,
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE  = 0x40,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE      = 0x80,
    D3D12_RESOURCE_STATE_COPY_DEST                  = 0x400,
    D3D12_RESOURCE_STATE_COPY_SOURCE                = 0x800,
    D3D12_RESOURCE_STATE_RESOLVE_DEST               = 0x1000,
    D3D12_RESOURCE_STATE_RESOLVE_SOURCE             = 0x2000,
    D3D12_RESOURCE_STATE_PRESENT                    = 0,
    D3D12_RESOURCE_STATE_GENERIC_READ               = 0x1 | 0x2 | 0x40 | 0x80 | 0x400 | 0x800
};

enum D3D12_RESOURCE_BARRIER_TYPE : uint32_t {
    D3D12_RESOURCE_BARRIER_TYPE_TRANSITION  = 0,
    D3D12_RESOURCE_BARRIER_TYPE_ALIASING    = 1,
    D3D12_RESOURCE_BARRIER_TYPE_UAV         = 2
};

enum D3D12_RESOURCE_BARRIER_FLAGS : uint32_t {
    D3D12_RESOURCE_BARRIER_FLAG_NONE   = 0,
    D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY = 0x1,
    D3D12_RESOURCE_BARRIER_FLAG_END_ONLY   = 0x2
};

enum D3D12_DESCRIPTOR_HEAP_TYPE : uint32_t {
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV = 0,
    D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER     = 1,
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV         = 2,
    D3D12_DESCRIPTOR_HEAP_TYPE_DSV         = 3,
    D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES   = 4
};

enum D3D12_DESCRIPTOR_HEAP_FLAGS : uint32_t {
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE           = 0,
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE = 0x1
};

enum D3D12_FENCE_FLAGS : uint32_t {
    D3D12_FENCE_FLAG_NONE                   = 0,
    D3D12_FENCE_FLAG_SHARED                 = 0x1,
    D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER   = 0x2,
    D3D12_FENCE_FLAG_NON_MONITORED          = 0x4
};

enum D3D12_HEAP_TYPE : uint32_t {
    D3D12_HEAP_TYPE_DEFAULT  = 1,
    D3D12_HEAP_TYPE_UPLOAD   = 2,
    D3D12_HEAP_TYPE_READBACK = 3,
    D3D12_HEAP_TYPE_CUSTOM   = 4
};

enum D3D12_HEAP_FLAGS : uint32_t {
    D3D12_HEAP_FLAG_NONE                         = 0,
    D3D12_HEAP_FLAG_SHARED                       = 0x1,
    D3D12_HEAP_FLAG_DENY_BUFFERS                 = 0x4,
    D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES = 0,
    D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES          = 0x40,
    D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES      = 0x80,
    D3D12_HEAP_FLAG_HARDWARE_PROTECTED           = 0x100,
    D3D12_HEAP_FLAG_ALLOW_WRITE_WATCH            = 0x200,
    D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS         = 0x400,
    D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT          = 0x800,
    D3D12_HEAP_FLAG_CREATE_NOT_ZEROED            = 0x1000,
    D3D12_HEAP_FLAG_ALLOW_DISPLAY                = 0x8,
    D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER         = 0x20,
    D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS           = 0xc0,
    D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES = 0x44,
    D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES    = 0x84
};

enum D3D12_RESOURCE_DIMENSION : uint32_t {
    D3D12_RESOURCE_DIMENSION_UNKNOWN   = 0,
    D3D12_RESOURCE_DIMENSION_BUFFER    = 1,
    D3D12_RESOURCE_DIMENSION_TEXTURE1D = 2,
    D3D12_RESOURCE_DIMENSION_TEXTURE2D = 3,
    D3D12_RESOURCE_DIMENSION_TEXTURE3D = 4
};

enum D3D12_TEXTURE_LAYOUT : uint32_t {
    D3D12_TEXTURE_LAYOUT_UNKNOWN                = 0,
    D3D12_TEXTURE_LAYOUT_ROW_MAJOR              = 1,
    D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE = 2,
    D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE  = 3
};

enum D3D12_RESOURCE_FLAGS : uint32_t {
    D3D12_RESOURCE_FLAG_NONE                        = 0,
    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET         = 0x1,
    D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL         = 0x2,
    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS      = 0x4,
    D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE        = 0x8,
    D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER         = 0x10,
    D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS   = 0x20
};

// ============================================================================
// 2. D3D12 Core Structures
// ============================================================================

struct D3D12_COMMAND_QUEUE_DESC {
    D3D12_COMMAND_LIST_TYPE Type;
    int32_t Priority;
    D3D12_COMMAND_QUEUE_FLAGS Flags;
    uint32_t NodeMask;
};

struct D3D12_CPU_DESCRIPTOR_HANDLE {
    size_t ptr;
};

struct D3D12_GPU_DESCRIPTOR_HANDLE {
    uint64_t ptr;
};

struct D3D12_DESCRIPTOR_HEAP_DESC {
    D3D12_DESCRIPTOR_HEAP_TYPE Type;
    uint32_t NumDescriptors;
    D3D12_DESCRIPTOR_HEAP_FLAGS Flags;
    uint32_t NodeMask;
};

class ID3D12Resource;

struct D3D12_RESOURCE_TRANSITION_BARRIER {
    ID3D12Resource* pResource;
    uint32_t Subresource;
    D3D12_RESOURCE_STATES StateBefore;
    D3D12_RESOURCE_STATES StateAfter;
};

struct D3D12_RESOURCE_BARRIER {
    D3D12_RESOURCE_BARRIER_TYPE Type;
    D3D12_RESOURCE_BARRIER_FLAGS Flags;
    union {
        D3D12_RESOURCE_TRANSITION_BARRIER Transition;
    };
};

struct D3D12_HEAP_PROPERTIES {
    D3D12_HEAP_TYPE Type;
    uint32_t CPUPageProperty;
    uint32_t MemoryPoolPreference;
    uint32_t CreationNodeMask;
    uint32_t VisibleNodeMask;
};

struct D3D12_RESOURCE_DESC {
    D3D12_RESOURCE_DIMENSION Dimension;
    uint64_t Alignment;
    uint64_t Width;
    uint32_t Height;
    uint16_t DepthOrArraySize;
    uint16_t MipLevels;
    DXGI_FORMAT Format;
    DXGI_SAMPLE_DESC SampleDesc;
    D3D12_TEXTURE_LAYOUT Layout;
    D3D12_RESOURCE_FLAGS Flags;
};

struct D3D12_VERTEX_BUFFER_VIEW {
    uint64_t BufferLocation;
    uint32_t SizeInBytes;
    uint32_t StrideInBytes;
};

struct D3D12_INDEX_BUFFER_VIEW {
    uint64_t BufferLocation;
    uint32_t SizeInBytes;
    DXGI_FORMAT Format;
};

struct D3D12_VIEWPORT {
    float TopLeftX;
    float TopLeftY;
    float Width;
    float Height;
    float MinDepth;
    float MaxDepth;
};

struct D3D12_RECT {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};

struct D3D12_RENDER_TARGET_VIEW_DESC {
    DXGI_FORMAT Format;
    uint32_t ViewDimension;
};

struct D3D12_DEPTH_STENCIL_VIEW_DESC {
    DXGI_FORMAT Format;
    uint32_t ViewDimension;
    uint32_t Flags;
};

struct D3D12_ROOT_SIGNATURE_DESC {
    uint32_t NumParameters;
    const void* pParameters;
    uint32_t NumStaticSamplers;
    const void* pStaticSamplers;
    uint32_t Flags;
};

enum D3D12_INDIRECT_ARGUMENT_TYPE {
    D3D12_INDIRECT_ARGUMENT_TYPE_DRAW                   = 0,
    D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED           = 1,
    D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH               = 2,
    D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW     = 3,
    D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW      = 4,
    D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT               = 5,
    D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW   = 6,
    D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW   = 7,
    D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW  = 8,
    D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH          = 9,
    D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS          = 10
};

struct D3D12_INDIRECT_ARGUMENT_DESC {
    D3D12_INDIRECT_ARGUMENT_TYPE Type{ D3D12_INDIRECT_ARGUMENT_TYPE_DRAW };
    union {
        struct {
            uint32_t Slot;
        } VertexBuffer;
        struct {
            uint32_t RootParameterIndex;
            uint32_t DestOffsetIn32BitValues;
            uint32_t Num32BitValuesToSet;
        } Constant;
        struct {
            uint32_t RootParameterIndex;
        } ConstantBufferView;
        struct {
            uint32_t RootParameterIndex;
        } ShaderResourceView;
        struct {
            uint32_t RootParameterIndex;
        } UnorderedAccessView;
    };
};

struct D3D12_COMMAND_SIGNATURE_DESC {
    uint32_t ByteStride{ 0 };
    uint32_t NumArgumentDescs{ 0 };
    const D3D12_INDIRECT_ARGUMENT_DESC* pArgumentDescs{ nullptr };
    uint32_t NodeMask{ 0 };
};

struct D3D12_SHADER_BYTECODE {
    const void* pShaderBytecode{ nullptr };
    size_t BytecodeLength{ 0 };
};

enum D3D12_PRIMITIVE_TOPOLOGY_TYPE {
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED = 0,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT     = 1,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE      = 2,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE  = 3,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH     = 4
};

struct D3D12_GRAPHICS_PIPELINE_STATE_DESC {
    void* pRootSignature{ nullptr };
    D3D12_SHADER_BYTECODE VS{};
    D3D12_SHADER_BYTECODE PS{};
    D3D12_SHADER_BYTECODE DS{};
    D3D12_SHADER_BYTECODE HS{};
    D3D12_SHADER_BYTECODE GS{};
    D3D11_RASTERIZER_DESC RasterizerState{};
    D3D11_BLEND_DESC BlendState{};
    D3D11_DEPTH_STENCIL_DESC DepthStencilState{};
    uint32_t SampleMask{ 0xFFFFFFFF };
    D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED };
    uint32_t NumRenderTargets{ 0 };
    DXGI_FORMAT RTVFormats[8]{};
    DXGI_FORMAT DSVFormat{ DXGI_FORMAT_UNKNOWN };
    DXGI_SAMPLE_DESC SampleDesc{};
    uint32_t NodeMask{ 0 };
    uint32_t Flags{ 0 };
};

enum D3D12_META_COMMAND_PARAMETER_TYPE {
    D3D12_META_COMMAND_PARAMETER_TYPE_FLOAT = 0,
    D3D12_META_COMMAND_PARAMETER_TYPE_UINT64 = 1,
    D3D12_META_COMMAND_PARAMETER_TYPE_GPU_VIRTUAL_ADDRESS = 2,
    D3D12_META_COMMAND_PARAMETER_TYPE_RESOURCE = 3
};

enum D3D12_META_COMMAND_PARAMETER_FLAGS {
    D3D12_META_COMMAND_PARAMETER_FLAG_INPUT = 0x1,
    D3D12_META_COMMAND_PARAMETER_FLAG_OUTPUT = 0x2
};

enum D3D12_META_COMMAND_PARAMETER_STAGE {
    D3D12_META_COMMAND_PARAMETER_STAGE_CREATION = 0,
    D3D12_META_COMMAND_PARAMETER_STAGE_INITIALIZATION = 1,
    D3D12_META_COMMAND_PARAMETER_STAGE_EXECUTION = 2
};

enum D3D12_GRAPHICS_STATES : uint32_t {
    D3D12_GRAPHICS_STATE_NONE = 0,
    D3D12_GRAPHICS_STATE_IA_VERTEX_BUFFERS = (1 << 0),
    D3D12_GRAPHICS_STATE_IA_INDEX_BUFFER = (1 << 1),
    D3D12_GRAPHICS_STATE_IA_PRIMITIVE_TOPOLOGY = (1 << 2),
    D3D12_GRAPHICS_STATE_DESCRIPTOR_HEAPS = (1 << 3),
    D3D12_GRAPHICS_STATE_GRAPHICS_ROOT_SIGNATURE = (1 << 4),
    D3D12_GRAPHICS_STATE_COMPUTE_ROOT_SIGNATURE = (1 << 5),
    D3D12_GRAPHICS_STATE_RS_VIEWPORTS = (1 << 6),
    D3D12_GRAPHICS_STATE_RS_SCISSOR_RECTS = (1 << 7),
    D3D12_GRAPHICS_STATE_PREDICATION = (1 << 8),
    D3D12_GRAPHICS_STATE_OM_RENDER_TARGETS = (1 << 9),
    D3D12_GRAPHICS_STATE_OM_STENCIL_REF = (1 << 10),
    D3D12_GRAPHICS_STATE_OM_BLEND_FACTOR = (1 << 11),
    D3D12_GRAPHICS_STATE_PIPELINE_STATE = (1 << 12),
    D3D12_GRAPHICS_STATE_SO_TARGETS = (1 << 13),
    D3D12_GRAPHICS_STATE_OM_DEPTH_BOUNDS = (1 << 14),
    D3D12_GRAPHICS_STATE_SAMPLE_POSITIONS = (1 << 15),
    D3D12_GRAPHICS_STATE_VIEW_INSTANCE_MASK = (1 << 16)
};

struct D3D12_META_COMMAND_PARAMETER_DESC {
    const wchar_t* Name{ nullptr };
    D3D12_META_COMMAND_PARAMETER_TYPE Type{ D3D12_META_COMMAND_PARAMETER_TYPE_FLOAT };
    D3D12_META_COMMAND_PARAMETER_FLAGS Flags{ D3D12_META_COMMAND_PARAMETER_FLAG_INPUT };
    D3D12_RESOURCE_STATES RequiredResourceState{ D3D12_RESOURCE_STATE_COMMON };
    uint32_t StructureOffsetInBytes{ 0 };
};

struct D3D12_META_COMMAND_DESC {
    GUID Id{};
    const wchar_t* Name{ nullptr };
    D3D12_GRAPHICS_STATES InitializationDirtyState{ D3D12_GRAPHICS_STATE_NONE };
    D3D12_GRAPHICS_STATES ExecutionDirtyState{ D3D12_GRAPHICS_STATE_NONE };
};

struct D3D12_RESOURCE_ALLOCATION_INFO {
    uint64_t SizeInBytes{ 0 };
    uint64_t Alignment{ 0 };
};

struct D3D12_RESOURCE_ALLOCATION_INFO1 {
    uint64_t Offset{ 0 };
    uint64_t Alignment{ 0 };
    uint64_t SizeInBytes{ 0 };
};

// ============================================================================
// 3. COM Interface GUIDs
// ============================================================================

static constexpr IID IID_ID3D12Object = 
    { 0xc4fec28f, 0x796e, 0x4e23, { 0x9e, 0x18, 0x0d, 0x96, 0x5a, 0x00, 0xd9, 0x59 } };

static constexpr IID IID_ID3D12DeviceChild = 
    { 0x905db44e, 0xbb35, 0x4455, { 0xb1, 0x0b, 0xac, 0x13, 0x27, 0xf4, 0x73, 0xa0 } };

static constexpr IID IID_ID3D12Pageable = 
    { 0x63ee58fb, 0x1268, 0x4835, { 0x86, 0xda, 0xf0, 0x08, 0xce, 0x62, 0xf0, 0xd6 } };

static constexpr IID IID_ID3D12RootSignature = 
    { 0xc54a6b66, 0x72df, 0x4ee8, { 0x8b, 0xe5, 0xa9, 0x46, 0xa1, 0x42, 0x92, 0x14 } };

static constexpr IID IID_ID3D12Resource = 
    { 0x696442be, 0xa72e, 0x4059, { 0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad } };

static constexpr IID IID_ID3D12CommandAllocator = 
    { 0x6102dee4, 0xaf59, 0x4b09, { 0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24 } };

static constexpr IID IID_ID3D12Fence = 
    { 0x0a753dcf, 0xc4d8, 0x4b91, { 0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76 } };

static constexpr IID IID_ID3D12PipelineState = 
    { 0x765a30f3, 0xf624, 0x4c6f, { 0xa8, 0x28, 0xac, 0xe9, 0x48, 0x62, 0x24, 0x45 } };

static constexpr IID IID_ID3D12DescriptorHeap = 
    { 0x8efb471d, 0x616c, 0x4f49, { 0x90, 0xf7, 0x10, 0xbc, 0x16, 0x04, 0x4b, 0x82 } };

static constexpr IID IID_ID3D12CommandList = 
    { 0x7116d81c, 0xe7e4, 0x47ce, { 0xb8, 0xc6, 0xec, 0x81, 0x68, 0xf4, 0x37, 0xe5 } };

static constexpr IID IID_ID3D12GraphicsCommandList = 
    { 0x5b160d0f, 0xac1b, 0x4185, { 0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55 } };

static constexpr IID IID_ID3D12CommandQueue = 
    { 0x0ec870a6, 0x5d7e, 0x4c22, { 0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed } };

static constexpr IID IID_ID3D12CommandSignature = 
    { 0xc36a4777, 0xb798, 0x4eb5, { 0x8f, 0x50, 0x0f, 0x44, 0xcd, 0x78, 0x87, 0x03 } };

static constexpr IID IID_ID3D12MetaCommand = 
    { 0xdbb84c27, 0x36ce, 0x4fc9, { 0xb8, 0x01, 0xf0, 0x48, 0xc4, 0x6a, 0xc5, 0x70 } };

static constexpr IID IID_ID3D12Heap = 
    { 0x6b3ba88d, 0x4f52, 0x45e6, { 0x83, 0x23, 0xa5, 0xef, 0x46, 0x00, 0xd4, 0xce } };

static constexpr IID IID_ID3D12Device = 
    { 0x189819f1, 0x1db6, 0x4b57, { 0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7 } };

// Forward declarations
class ID3D12Object;
class ID3D12DeviceChild;
class ID3D12RootSignature;
class ID3D12Resource;
class ID3D12CommandAllocator;
class ID3D12Fence;
class ID3D12PipelineState;
class ID3D12DescriptorHeap;
class ID3D12CommandList;
class ID3D12GraphicsCommandList;
class ID3D12GraphicsCommandList10;
class ID3D12CommandQueue;
class ID3D12CommandSignature;
class ID3D12MetaCommand;
class ID3D12Heap;
class ID3D12Device;

// ============================================================================
// 4. Abstract COM Interfaces
// ============================================================================

class ID3D12Object : public IUnknown {
public:
    virtual int32_t GetPrivateData(const IID& guid, uint32_t* pDataSize, void* pData) = 0;
    virtual int32_t SetPrivateData(const IID& guid, uint32_t DataSize, const void* pData) = 0;
    virtual int32_t SetPrivateDataInterface(const IID& guid, const IUnknown* pData) = 0;
    virtual int32_t SetName(const wchar_t* Name) = 0;
};

class ID3D12DeviceChild : public ID3D12Object {
public:
    virtual int32_t GetDevice(const IID& riid, void** ppDevice) = 0;
};

class ID3D12RootSignature : public ID3D12DeviceChild {};

class ID3D12Pageable : public ID3D12DeviceChild {};

class ID3D12CommandSignature : public ID3D12Pageable {};

class ID3D12MetaCommand : public ID3D12Pageable {
public:
    virtual uint64_t GetRequiredParameterResourceSize(D3D12_META_COMMAND_PARAMETER_STAGE Stage, uint32_t ParameterIndex) = 0;
};

struct D3D12_HEAP_DESC {
    uint64_t SizeInBytes{0};
    D3D12_HEAP_PROPERTIES Properties{};
    uint64_t Alignment{0};
    uint32_t Flags{0};
};

class ID3D12Heap : public ID3D12Pageable {
public:
    virtual D3D12_HEAP_DESC GetDesc() = 0;
};

class Prism3D12HeapImpl : public ID3D12Heap {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    const void* m_pAddress{ nullptr };
    D3D12_HEAP_DESC m_desc{};
    std::vector<uint8_t> m_storage;

public:
    explicit Prism3D12HeapImpl(const void* pAddr, uint64_t size = 64 * 1024 * 1024ULL)
        : m_pAddress(pAddr) {
        m_desc.SizeInBytes = size;
        m_desc.Alignment = 64 * 1024;
    }

    Prism3D12HeapImpl(ID3D12Device* pDev, const D3D12_HEAP_DESC& desc)
        : m_pDevice(pDev), m_desc(desc) {
        if (m_desc.SizeInBytes > 0) {
            m_storage.resize(static_cast<size_t>(m_desc.SizeInBytes), 0);
            m_pAddress = m_storage.data();
        }
    }

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild ||
            riid == IID_ID3D12Pageable || riid == IID_ID3D12Heap) {
            *ppv = static_cast<ID3D12Heap*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }
    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }
    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }
    int32_t GetDevice(const IID&, void** ppDevice) override {
        if (!ppDevice) return -1;
        *ppDevice = m_pDevice;
        return 0;
    }
    D3D12_HEAP_DESC GetDesc() override { return m_desc; }
    const void* GetAddress() const noexcept { return m_pAddress; }
};

class ID3D12Resource : public ID3D12Pageable {
public:
    virtual int32_t Map(uint32_t Subresource, const void* pReadRange, void** ppData) = 0;
    virtual void Unmap(uint32_t Subresource, const void* pWrittenRange) = 0;
    virtual D3D12_RESOURCE_DESC GetDesc() = 0;
    virtual uint64_t GetGPUVirtualAddress() = 0;
};

class ID3D12CommandAllocator : public ID3D12DeviceChild {
public:
    virtual int32_t Reset() = 0;
};

class ID3D12Fence : public ID3D12DeviceChild {
public:
    virtual uint64_t GetCompletedValue() = 0;
    virtual int32_t SetEventOnCompletion(uint64_t Value, void* hEvent) = 0;
    virtual int32_t Signal(uint64_t Value) = 0;
};

class ID3D12PipelineState : public ID3D12DeviceChild {};

class ID3D12DescriptorHeap : public ID3D12DeviceChild {
public:
    virtual D3D12_DESCRIPTOR_HEAP_DESC GetDesc() = 0;
    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandleForHeapStart() = 0;
    virtual D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleForHeapStart() = 0;
};

class ID3D12CommandList : public ID3D12DeviceChild {
public:
    virtual D3D12_COMMAND_LIST_TYPE GetType() = 0;
};

class ID3D12GraphicsCommandList : public ID3D12CommandList {
public:
    virtual int32_t Close() = 0;
    virtual int32_t Reset(ID3D12CommandAllocator* pAllocator, ID3D12PipelineState* pInitialState) = 0;
    virtual void ClearState(ID3D12PipelineState* pPipelineState) = 0;
    virtual void ResourceBarrier(uint32_t NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers) = 0;
    virtual void RSSetViewports(uint32_t NumViewports, const D3D12_VIEWPORT* pViewports) = 0;
    virtual void RSSetScissorRects(uint32_t NumRects, const D3D12_RECT* pRects) = 0;
    virtual void OMSetRenderTargets(uint32_t NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors, int32_t RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) = 0;
    virtual void ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const float ColorRGBA[4], uint32_t NumRects, const D3D12_RECT* pRects) = 0;
    virtual void ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, uint32_t ClearFlags, float Depth, uint8_t Stencil, uint32_t NumRects, const D3D12_RECT* pRects) = 0;
    virtual void IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology) = 0;
    virtual void IASetVertexBuffers(uint32_t StartSlot, uint32_t NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews) = 0;
    virtual void IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView) = 0;
    virtual void SetGraphicsRootSignature(ID3D12RootSignature* pRootSignature) = 0;
    virtual void SetPipelineState(ID3D12PipelineState* pPipelineState) = 0;
    virtual void DrawInstanced(uint32_t VertexCountPerInstance, uint32_t InstanceCount, uint32_t StartVertexLocation, uint32_t StartInstanceLocation) = 0;
    virtual void DrawIndexedInstanced(uint32_t IndexCountPerInstance, uint32_t InstanceCount, uint32_t StartIndexLocation, int32_t BaseVertexLocation, uint32_t StartInstanceLocation) = 0;
    virtual void ExecuteIndirect(
        ID3D12CommandSignature* /*pCommandSignature*/,
        uint32_t /*MaxCommandCount*/,
        ID3D12Resource* /*pArgumentBuffer*/,
        uint64_t /*ArgumentBufferOffset*/,
        ID3D12Resource* /*pCountBuffer*/,
        uint64_t /*CountBufferOffset*/) {}
    virtual void InitializeMetaCommand(
        ID3D12MetaCommand* /*pMetaCommand*/,
        const void* /*pInitializationParametersData*/,
        size_t /*InitializationParametersDataSizeInBytes*/) {}
    virtual void ExecuteMetaCommand(
        ID3D12MetaCommand* /*pMetaCommand*/,
        const void* /*pExecutionParametersData*/,
        size_t /*ExecutionParametersDataSizeInBytes*/) {}
};

// ============================================================================
// DirectX 12 Agility SDK: Work Graphs & Command List 10
// ============================================================================

enum D3D12_PROGRAM_TYPE {
    D3D12_PROGRAM_TYPE_GENERIC_PIPELINE = 1,
    D3D12_PROGRAM_TYPE_RAYTRACING_PIPELINE = 2,
    D3D12_PROGRAM_TYPE_WORK_GRAPH = 3
};

enum D3D12_SET_WORK_GRAPH_FLAGS {
    D3D12_SET_WORK_GRAPH_FLAG_NONE = 0,
    D3D12_SET_WORK_GRAPH_FLAG_INITIALIZE = 1
};

struct D3D12_GPU_VIRTUAL_ADDRESS_RANGE {
    uint64_t StartAddress{0};
    uint64_t SizeInBytes{0};
};

struct D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE {
    uint64_t StartAddress{0};
    uint64_t SizeInBytes{0};
    uint64_t StrideInBytes{0};
};

struct D3D12_SET_WORK_GRAPH_DESC {
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE ProgramIdentifier{};
    D3D12_SET_WORK_GRAPH_FLAGS Flags{D3D12_SET_WORK_GRAPH_FLAG_NONE};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE BackingMemory{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE NodeLocalRootArgumentsTable{};
};

struct D3D12_SET_PROGRAM_DESC {
    D3D12_PROGRAM_TYPE Type{D3D12_PROGRAM_TYPE_WORK_GRAPH};
    union {
        D3D12_SET_WORK_GRAPH_DESC WorkGraph;
    };
};

enum D3D12_DISPATCH_MODE {
    D3D12_DISPATCH_MODE_NODE_CPU_INPUT = 0,
    D3D12_DISPATCH_MODE_NODE_GPU_INPUT = 1
};

struct D3D12_NODE_CPU_INPUT {
    uint32_t EntrypointIndex{0};
    uint32_t NumRecords{0};
    const void* pRecords{nullptr};
    uint64_t RecordStrideInBytes{0};
};

struct D3D12_DISPATCH_GRAPH_DESC {
    D3D12_DISPATCH_MODE Mode{D3D12_DISPATCH_MODE_NODE_CPU_INPUT};
    union {
        D3D12_NODE_CPU_INPUT NodeCPUInput;
        D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE NodeGPUInput;
    };
};

enum D3D12_WORK_GRAPHS_TIER {
    D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED = 0,
    D3D12_WORK_GRAPHS_TIER_1_0 = 10,
    D3D12_WORK_GRAPHS_TIER_1_1 = 11
};

// Enhanced Barriers Enums & Structs
enum D3D12_BARRIER_LAYOUT {
    D3D12_BARRIER_LAYOUT_UNDEFINED               = 0,
    D3D12_BARRIER_LAYOUT_COMMON                  = 1,
    D3D12_BARRIER_LAYOUT_PRESENT                 = 2,
    D3D12_BARRIER_LAYOUT_GENERIC_READ            = 3,
    D3D12_BARRIER_LAYOUT_RENDER_TARGET           = 4,
    D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS        = 5,
    D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE     = 6,
    D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ      = 7,
    D3D12_BARRIER_LAYOUT_SHADER_RESOURCE         = 8,
    D3D12_BARRIER_LAYOUT_COPY_SOURCE             = 9,
    D3D12_BARRIER_LAYOUT_COPY_DEST               = 10,
    D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE          = 11,
    D3D12_BARRIER_LAYOUT_RESOLVE_DEST            = 12,
    D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE     = 13,
    D3D12_BARRIER_LAYOUT_DIRECT_INDEXING         = 20
};

enum D3D12_BARRIER_SYNC {
    D3D12_BARRIER_SYNC_NONE                       = 0,
    D3D12_BARRIER_SYNC_ALL                        = 0x1,
    D3D12_BARRIER_SYNC_DRAW                       = 0x2,
    D3D12_BARRIER_SYNC_INDEX_INPUT                = 0x4,
    D3D12_BARRIER_SYNC_VERTEX_SHADING             = 0x8,
    D3D12_BARRIER_SYNC_PIXEL_SHADING              = 0x10,
    D3D12_BARRIER_SYNC_DEPTH_STENCIL              = 0x20,
    D3D12_BARRIER_SYNC_RENDER_TARGET              = 0x40,
    D3D12_BARRIER_SYNC_COMPUTE_SHADING            = 0x80,
    D3D12_BARRIER_SYNC_RAYTRACING                 = 0x100,
    D3D12_BARRIER_SYNC_COPY                       = 0x200,
    D3D12_BARRIER_SYNC_RESOLVE                    = 0x400,
    D3D12_BARRIER_SYNC_EXECUTE_INDIRECT           = 0x800,
    D3D12_BARRIER_SYNC_ALL_SHADING                = 0x1000,
    D3D12_BARRIER_SYNC_NON_PIXEL_SHADING          = 0x2000,
    D3D12_BARRIER_SYNC_SPLIT                      = 0x80000000
};

enum D3D12_BARRIER_ACCESS {
    D3D12_BARRIER_ACCESS_COMMON                  = 0,
    D3D12_BARRIER_ACCESS_VERTEX_BUFFER           = 0x1,
    D3D12_BARRIER_ACCESS_CONSTANT_BUFFER         = 0x2,
    D3D12_BARRIER_ACCESS_INDEX_BUFFER            = 0x4,
    D3D12_BARRIER_ACCESS_RENDER_TARGET           = 0x8,
    D3D12_BARRIER_ACCESS_UNORDERED_ACCESS        = 0x10,
    D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE     = 0x20,
    D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ      = 0x40,
    D3D12_BARRIER_ACCESS_SHADER_RESOURCE         = 0x80,
    D3D12_BARRIER_ACCESS_STREAM_OUTPUT           = 0x100,
    D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT       = 0x200,
    D3D12_BARRIER_ACCESS_COPY_DEST               = 0x400,
    D3D12_BARRIER_ACCESS_COPY_SOURCE             = 0x800,
    D3D12_BARRIER_ACCESS_RESOLVE_DEST            = 0x1000,
    D3D12_BARRIER_ACCESS_RESOLVE_SOURCE          = 0x2000,
    D3D12_BARRIER_ACCESS_NO_ACCESS               = 0x80000000
};

enum D3D12_BARRIER_TYPE {
    D3D12_BARRIER_TYPE_GLOBAL  = 0,
    D3D12_BARRIER_TYPE_TEXTURE = 1,
    D3D12_BARRIER_TYPE_BUFFER  = 2
};

struct D3D12_GLOBAL_BARRIER {
    uint32_t SyncBefore{0};
    uint32_t SyncAfter{0};
    uint32_t AccessBefore{0};
    uint32_t AccessAfter{0};
};

struct D3D12_BUFFER_BARRIER {
    uint32_t SyncBefore{0};
    uint32_t SyncAfter{0};
    uint32_t AccessBefore{0};
    uint32_t AccessAfter{0};
    ID3D12Resource* pResource{nullptr};
    uint64_t Offset{0};
    uint64_t Size{0xFFFFFFFFFFFFFFFFULL};
};

struct D3D12_BARRIER_SUBRESOURCE_RANGE {
    uint32_t IndexOrFirstMipLevel{0};
    uint32_t NumMipLevels{1};
    uint32_t FirstArraySlice{0};
    uint32_t NumArraySlices{1};
    uint32_t FirstPlane{0};
    uint32_t NumPlanes{1};
};

struct D3D12_TEXTURE_BARRIER {
    uint32_t SyncBefore{0};
    uint32_t SyncAfter{0};
    uint32_t AccessBefore{0};
    uint32_t AccessAfter{0};
    D3D12_BARRIER_LAYOUT LayoutBefore{D3D12_BARRIER_LAYOUT_UNDEFINED};
    D3D12_BARRIER_LAYOUT LayoutAfter{D3D12_BARRIER_LAYOUT_UNDEFINED};
    ID3D12Resource* pResource{nullptr};
    D3D12_BARRIER_SUBRESOURCE_RANGE Subresources{};
    uint32_t Flags{0};
};

struct D3D12_BARRIER_GROUP {
    D3D12_BARRIER_TYPE Type{D3D12_BARRIER_TYPE_GLOBAL};
    uint32_t NumBarriers{0};
    union {
        const D3D12_GLOBAL_BARRIER*  pGlobalBarriers;
        const D3D12_TEXTURE_BARRIER* pTextureBarriers;
        const D3D12_BUFFER_BARRIER*  pBufferBarriers;
    };
};

struct D3D12_DEPTH_STENCIL_VALUE {
    float Depth{1.0f};
    uint8_t Stencil{0};
};

struct D3D12_CLEAR_VALUE {
    DXGI_FORMAT Format{DXGI_FORMAT_UNKNOWN};
    union {
        float Color[4];
        D3D12_DEPTH_STENCIL_VALUE DepthStencil;
    };
};

enum D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE {
    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD   = 0,
    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE  = 1,
    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR     = 2,
    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS = 3
};

enum D3D12_RENDER_PASS_ENDING_ACCESS_TYPE {
    D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD   = 0,
    D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE  = 1,
    D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE   = 2,
    D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS = 3
};

enum D3D12_RENDER_PASS_FLAGS : uint32_t {
    D3D12_RENDER_PASS_FLAG_NONE             = 0,
    D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES = 1,
    D3D12_RENDER_PASS_FLAG_SUSPENDING_PASS  = 2,
    D3D12_RENDER_PASS_FLAG_RESUMING_PASS    = 4
};

struct D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS {
    D3D12_CLEAR_VALUE ClearValue;
};

struct D3D12_RENDER_PASS_BEGINNING_ACCESS {
    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE Type{D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD};
    union {
        D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS Clear;
    };
};

struct D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS {
    uint32_t SrcSubresource{0};
    uint32_t DstSubresource{0};
    uint32_t DstX{0};
    uint32_t DstY{0};
    D3D12_RECT SrcRect{};
};

struct D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_PARAMETERS {
    ID3D12Resource* pSrcResource{nullptr};
    ID3D12Resource* pDstResource{nullptr};
    uint32_t SubresourceCount{0};
    const D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS* pSubresourceParameters{nullptr};
    DXGI_FORMAT Format{DXGI_FORMAT_UNKNOWN};
    uint32_t ResolveMode{0};
    BOOL PreserveResolveSource{FALSE};
};

struct D3D12_RENDER_PASS_ENDING_ACCESS {
    D3D12_RENDER_PASS_ENDING_ACCESS_TYPE Type{D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD};
    union {
        D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_PARAMETERS Resolve;
    };
};

struct D3D12_RENDER_PASS_RENDER_TARGET_DESC {
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor{};
    D3D12_RENDER_PASS_BEGINNING_ACCESS BeginningAccess{};
    D3D12_RENDER_PASS_ENDING_ACCESS EndingAccess{};
};

struct D3D12_RENDER_PASS_DEPTH_STENCIL_DESC {
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor{};
    D3D12_RENDER_PASS_BEGINNING_ACCESS DepthBeginningAccess{};
    D3D12_RENDER_PASS_BEGINNING_ACCESS StencilBeginningAccess{};
    D3D12_RENDER_PASS_ENDING_ACCESS DepthEndingAccess{};
    D3D12_RENDER_PASS_ENDING_ACCESS StencilEndingAccess{};
};

static constexpr IID IID_ID3D12GraphicsCommandList4 = 
    { 0x87e0e368, 0xe32b, 0x4d0b, { 0xbe, 0x4b, 0x4b, 0x78, 0xb8, 0x63, 0x00, 0xaf } };

struct D3D12_DISPATCH_RAYS_DESC {
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE RayGenerationShaderRecord{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE MissShaderTable{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE HitGroupTable{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE CallableShaderTable{};
    uint32_t Width{0};
    uint32_t Height{0};
    uint32_t Depth{0};
};

class ID3D12GraphicsCommandList4 : public ID3D12GraphicsCommandList {
public:
    virtual void DispatchRays(const D3D12_DISPATCH_RAYS_DESC* pDesc) = 0;
    virtual void BeginRenderPass(
        uint32_t NumRenderTargets,
        const D3D12_RENDER_PASS_RENDER_TARGET_DESC* pRenderTargets,
        const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* pDepthStencil,
        D3D12_RENDER_PASS_FLAGS Flags) = 0;
    virtual void EndRenderPass() = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList6 = 
    { 0xc3827890, 0xe548, 0x4cf6, { 0xbf, 0x5a, 0x4f, 0x97, 0x22, 0x3d, 0x1d, 0x28 } };

class ID3D12GraphicsCommandList6 : public ID3D12GraphicsCommandList4 {
public:
    virtual void DispatchMesh(uint32_t ThreadGroupCountX, uint32_t ThreadGroupCountY, uint32_t ThreadGroupCountZ) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList7 = 
    { 0xb0415a52, 0x8383, 0x4776, { 0x8b, 0x51, 0x73, 0x8d, 0x64, 0xdb, 0x8a, 0x91 } };

class ID3D12GraphicsCommandList7 : public ID3D12GraphicsCommandList6 {
public:
    virtual void Barrier(uint32_t NumBarrierGroups, const D3D12_BARRIER_GROUP* pBarrierGroups) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList10 = 
    { 0x217d12bc, 0x1f72, 0x4f72, { 0x87, 0x05, 0x76, 0x8a, 0x48, 0x49, 0x89, 0xb8 } };

class ID3D12GraphicsCommandList10 : public ID3D12GraphicsCommandList7 {
public:
    virtual void SetProgram(const D3D12_SET_PROGRAM_DESC* pDesc) = 0;
    virtual void DispatchGraph(const D3D12_DISPATCH_GRAPH_DESC* pDesc) = 0;
};

class ID3D12CommandQueue : public ID3D12DeviceChild {
public:
    virtual void ExecuteCommandLists(uint32_t NumCommandLists, ID3D12CommandList* const* ppCommandLists) = 0;
    virtual int32_t Signal(ID3D12Fence* pFence, uint64_t Value) = 0;
    virtual int32_t Wait(ID3D12Fence* pFence, uint64_t Value) = 0;
    virtual D3D12_COMMAND_QUEUE_DESC GetDesc() = 0;
};

// DirectX 12 Feature Options 5, 7, 12, 13, 14 & 15
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS5  = 21;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS7  = 23;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS8  = 24;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS9  = 25;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS10 = 26;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS11 = 27;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS12 = 28;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS13 = 29;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS14 = 30;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS15 = 31;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS16 = 32;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS17 = 33;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS18 = 34;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS19 = 35;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS20 = 36;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS21 = 37;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS22 = 38;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS23 = 39;

enum D3D12_SAMPLE_LOCATIONS_TIER {
    D3D12_SAMPLE_LOCATIONS_TIER_NOT_SUPPORTED = 0,
    D3D12_SAMPLE_LOCATIONS_TIER_1             = 1,
    D3D12_SAMPLE_LOCATIONS_TIER_2             = 2
};

enum D3D12_EXECUTE_INDIRECT_TIER {
    D3D12_EXECUTE_INDIRECT_TIER_1_0 = 10,
    D3D12_EXECUTE_INDIRECT_TIER_1_1 = 11
};


enum D3D12_RENDER_PASS_TIER {
    D3D12_RENDER_PASS_TIER_0 = 0,
    D3D12_RENDER_PASS_TIER_1 = 1,
    D3D12_RENDER_PASS_TIER_2 = 2
};

enum D3D12_RAYTRACING_TIER {
    D3D12_RAYTRACING_TIER_NOT_SUPPORTED = 0,
    D3D12_RAYTRACING_TIER_1_0           = 10,
    D3D12_RAYTRACING_TIER_1_1           = 11
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS5 {
    BOOL                   SRVOnlyTiledResourceTier3{0};
    D3D12_RENDER_PASS_TIER RenderPassesTier{D3D12_RENDER_PASS_TIER_0};
    D3D12_RAYTRACING_TIER  RaytracingTier{D3D12_RAYTRACING_TIER_1_1};
};

enum D3D12_SAMPLER_FEEDBACK_TIER {
    D3D12_SAMPLER_FEEDBACK_TIER_NOT_SUPPORTED = 0,
    D3D12_SAMPLER_FEEDBACK_TIER_09_0          = 90,
    D3D12_SAMPLER_FEEDBACK_TIER_1_0           = 100
};

enum D3D12_MESH_SHADER_TIER {
    D3D12_MESH_SHADER_TIER_NOT_SUPPORTED = 0,
    D3D12_MESH_SHADER_TIER_1             = 10
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS7 {
    D3D12_SAMPLER_FEEDBACK_TIER SamplerFeedbackTier{D3D12_SAMPLER_FEEDBACK_TIER_1_0};
    D3D12_MESH_SHADER_TIER      MeshShaderTier{D3D12_MESH_SHADER_TIER_1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS8 {
    BOOL UnalignedBlockTexturesSupported{1};
};

enum D3D12_WAVE_MMA_TIER {
    D3D12_WAVE_MMA_TIER_NOT_SUPPORTED = 0,
    D3D12_WAVE_MMA_TIER_1_0           = 10
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS9 {
    BOOL MeshShaderPipelineStatsSupported{1};
    BOOL MeshShaderSupportsFullRangeRenderTargetArrayIndex{1};
    BOOL AtomicInt64OnTypedResourceSupported{0};
    BOOL AtomicInt64OnGroupSharedSupported{0};
    BOOL DerivativesInMeshAndAmplificationShadersSupported{1};
    D3D12_WAVE_MMA_TIER WaveMMATier{D3D12_WAVE_MMA_TIER_1_0};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS10 {
    BOOL VariableRateShadingSumCombinerSupported{1};
    BOOL MeshShaderPerPrimitiveShadingRateSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS11 {
    BOOL AtomicInt64OnDescriptorHeapResourceSupported{1};
    D3D12_WORK_GRAPHS_TIER WorkGraphsTier{D3D12_WORK_GRAPHS_TIER_1_0};
    BOOL RelaxedFormatCastingSupported{1};
    BOOL EnhancedBarriersSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS12 {
    BOOL MSPrimitivesPipelineStatisticIncludesCulledPrimitives{0};
    BOOL EnhancedBarriersSupported{1};
    BOOL RelaxedFormatCastingSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS13 {
    BOOL UnrestrictedBufferTextureCopyPitchSupported{0};
    BOOL UnrestrictedVertexElementAlignmentSupported{0};
    BOOL InvertedViewportHeightFlipsYSupported{0};
    BOOL InvertedViewportDepthFlipsZSupported{0};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS14 {
    BOOL AdvancedTextureOpsSupported{0};
    BOOL WriteableMSAATexturesSupported{0};
    BOOL IndependentFrontAndBackStencilRefMaskSupported{0};
};

enum D3D12_DYNAMIC_INDEX_BUFFER_STRIP_CUT_SUPPORT {
    D3D12_DYNAMIC_INDEX_BUFFER_STRIP_CUT_SUPPORTED_NONE = 0,
    D3D12_DYNAMIC_INDEX_BUFFER_STRIP_CUT_SUPPORTED_STANDARD = 1,
    D3D12_DYNAMIC_INDEX_BUFFER_STRIP_CUT_SUPPORTED_ALL = 2
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS15 {
    BOOL TriangleFanSupported{0};
    D3D12_DYNAMIC_INDEX_BUFFER_STRIP_CUT_SUPPORT DynamicIndexBufferStripCut{D3D12_DYNAMIC_INDEX_BUFFER_STRIP_CUT_SUPPORTED_NONE};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS16 {
    BOOL DynamicDepthBiasSupported{1};
    BOOL GPUUploadHeapSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS17 {
    BOOL ManualWriteTrackingResourceSupported{1};
    BOOL InvertedDepthRangesSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS18 {
    D3D12_RENDER_PASS_TIER RenderPassesTier{D3D12_RENDER_PASS_TIER_1};
    BOOL AdvancedBlendOpsSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS19 {
    BOOL MipLevelOffsetsSupported{1};
    BOOL HalfFloatPrecisionSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS20 {
    BOOL                        ProgrammableSamplePositionsTier{1};
    D3D12_SAMPLE_LOCATIONS_TIER SampleLocationsTier{D3D12_SAMPLE_LOCATIONS_TIER_2};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS21 {
    D3D12_WORK_GRAPHS_TIER      WorkGraphsTier{D3D12_WORK_GRAPHS_TIER_1_0};
    D3D12_EXECUTE_INDIRECT_TIER ExecuteIndirectTier{D3D12_EXECUTE_INDIRECT_TIER_1_1};
    BOOL                        SampleCmpSimilarShaderInstructionsSupported{1};
};

enum D3D12_SHADER_EXECUTION_REORDERING_TIER {
    D3D12_SHADER_EXECUTION_REORDERING_TIER_NOT_SUPPORTED = 0,
    D3D12_SHADER_EXECUTION_REORDERING_TIER_1             = 1
};

enum D3D12_OPACITY_MICROMAP_TIER {
    D3D12_OPACITY_MICROMAP_TIER_NOT_SUPPORTED = 0,
    D3D12_OPACITY_MICROMAP_TIER_1_0           = 1
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS22 {
    D3D12_SHADER_EXECUTION_REORDERING_TIER ShaderExecutionReorderingTier{D3D12_SHADER_EXECUTION_REORDERING_TIER_1};
    D3D12_OPACITY_MICROMAP_TIER            OpacityMicromapTier{D3D12_OPACITY_MICROMAP_TIER_1_0};
    BOOL                                   PreviewFeaturesSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS23 {
    BOOL SubobjectCreationSupported{1};
    BOOL WorkGraphsTier1_1Supported{1};
    BOOL DynamicDepthBiasClampSupported{1};
};



enum D3D12_PIPELINE_STATE_SUBOBJECT_TYPE : uint32_t {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE        = 0,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS                    = 1,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS                    = 2,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS                    = 3,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS                    = 4,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS                    = 5,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT         = 6,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND                 = 7,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK           = 8,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER            = 9,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL         = 10,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT          = 11,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE    = 12,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY    = 13,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS = 14,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT  = 15,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC           = 16,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK             = 17,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO            = 18,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS                 = 19,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1        = 20,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING       = 21,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS                    = 22,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS                    = 23,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2        = 24,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MAX_VALID             = 25
};

struct D3D12_PIPELINE_STATE_STREAM_DESC {
    size_t SizeInBytes{0};
    void*  pPipelineStateSubobjectStream{nullptr};
};

struct CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type{ D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE };
    ID3D12RootSignature* pRootSignature{ nullptr };
};

struct CD3DX12_PIPELINE_STATE_STREAM_VS {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type{ D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS };
    D3D12_SHADER_BYTECODE Bytecode{};
};

struct CD3DX12_PIPELINE_STATE_STREAM_PS {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type{ D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS };
    D3D12_SHADER_BYTECODE Bytecode{};
};

struct CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type{ D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY };
    D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED };
};

struct CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type{ D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS };
    struct {
        uint32_t NumRenderTargets{0};
        DXGI_FORMAT RTFormats[8]{};
    } Formats{};
};

enum D3D12_RESIDENCY_PRIORITY : uint32_t {
    D3D12_RESIDENCY_PRIORITY_MINIMUM = 0x28000000,
    D3D12_RESIDENCY_PRIORITY_LOW     = 0x50000000,
    D3D12_RESIDENCY_PRIORITY_NORMAL  = 0x78000000,
    D3D12_RESIDENCY_PRIORITY_HIGH    = 0xa0000000,
    D3D12_RESIDENCY_PRIORITY_MAXIMUM = 0xc8000000
};

enum D3D12_RESIDENCY_FLAGS : uint32_t {
    D3D12_RESIDENCY_FLAG_NONE              = 0,
    D3D12_RESIDENCY_FLAG_DENY_OVERBUDGET   = 0x1
};

class ID3D12Device : public ID3D12Object {
public:
    virtual int32_t CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC* pDesc, const IID& riid, void** ppCommandQueue) = 0;
    virtual int32_t CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, const IID& riid, void** ppCommandAllocator) = 0;
    virtual int32_t CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) = 0;
    virtual int32_t CreateCommandList(uint32_t nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* pCommandAllocator, ID3D12PipelineState* pInitialState, const IID& riid, void** ppCommandList) = 0;
    virtual int32_t CreateFence(uint64_t InitialValue, D3D12_FENCE_FLAGS Flags, const IID& riid, void** ppFence) = 0;
    virtual int32_t CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC* pDescriptorHeapDesc, const IID& riid, void** ppvHeap) = 0;
    virtual int32_t CreateCommittedResource(const D3D12_HEAP_PROPERTIES* pHeapProperties, uint32_t HeapFlags, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialResourceState, const void* pOptimizedClearValue, const IID& riidResource, void** ppvResource) = 0;
    virtual int32_t CreateRootSignature(uint32_t nodeMask, const void* pBlobWithRootSignature, size_t blobLengthInBytes, const IID& riid, void** ppvRootSignature) = 0;
    virtual void CreateRenderTargetView(ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;
    virtual void CreateDepthStencilView(ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;
    virtual int32_t CheckFeatureSupport(uint32_t Feature, void* pFeatureSupportData, uint32_t FeatureSupportDataSize) = 0;
    virtual int32_t MakeResident(uint32_t /*NumObjects*/, ID3D12Pageable* const* /*ppObjects*/) { return 0; }
    virtual int32_t Evict(uint32_t /*NumObjects*/, ID3D12Pageable* const* /*ppObjects*/) { return 0; }
    virtual int32_t EnqueueMakeResident(D3D12_RESIDENCY_FLAGS /*Flags*/, uint32_t /*NumObjects*/, ID3D12Pageable* const* /*ppObjects*/, ID3D12Fence* /*pFenceToSignal*/, uint64_t /*FenceValueToSignal*/) { return 0; }
    virtual int32_t SetResidencyPriority(uint32_t /*NumObjects*/, ID3D12Pageable* const* /*ppObjects*/, const D3D12_RESIDENCY_PRIORITY* /*pPriorities*/) { return 0; }
    virtual int32_t CreateCommandSignature(
        const D3D12_COMMAND_SIGNATURE_DESC* /*pDesc*/,
        ID3D12RootSignature* /*pRootSignature*/,
        const IID& /*riid*/,
        void** /*ppvCommandSignature*/) { return 0; }
    virtual int32_t EnumerateMetaCommands(
        uint32_t* /*pNumMetaCommands*/,
        D3D12_META_COMMAND_DESC* /*pDescs*/) { return 0; }
    virtual int32_t EnumerateMetaCommandParameters(
        const GUID& /*CommandId*/,
        D3D12_META_COMMAND_PARAMETER_STAGE /*Stage*/,
        uint32_t* /*pTotalStructureSizeInBytes*/,
        uint32_t* /*pParameterCount*/,
        D3D12_META_COMMAND_PARAMETER_DESC* /*pParameterDescs*/) { return 0; }
    virtual int32_t CreateMetaCommand(
        const GUID& /*CommandId*/,
        uint32_t /*NodeMask*/,
        const void* /*pCreationParametersData*/,
        size_t /*CreationParametersDataSizeInBytes*/,
        const IID& /*riid*/,
        void** /*ppMetaCommand*/) { return 0; }
    virtual D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo(
        uint32_t /*visibleMask*/,
        uint32_t numResourceDescs,
        const D3D12_RESOURCE_DESC* pResourceDescs) {
        D3D12_RESOURCE_ALLOCATION_INFO info{};
        if (!pResourceDescs || numResourceDescs == 0) return info;
        for (uint32_t i = 0; i < numResourceDescs; ++i) {
            uint64_t itemSize = 0;
            uint64_t align = 64 * 1024;
            if (pResourceDescs[i].Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
                itemSize = pResourceDescs[i].Width;
            } else if (pResourceDescs[i].Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
                itemSize = pResourceDescs[i].Width * pResourceDescs[i].Height * 4;
                if (pResourceDescs[i].SampleDesc.Count > 1) {
                    align = 4 * 1024 * 1024;
                }
            }
            uint64_t alignedItem = (itemSize + align - 1) & ~(align - 1);
            info.SizeInBytes += alignedItem;
            if (align > info.Alignment) info.Alignment = align;
        }
        return info;
    }
    virtual int32_t CreateHeap(
        const D3D12_HEAP_DESC* /*pDesc*/,
        const IID& /*riid*/,
        void** /*ppvHeap*/) { return 0; }
    virtual int32_t CreatePlacedResource(
        ID3D12Heap* /*pHeap*/,
        uint64_t /*HeapOffset*/,
        const D3D12_RESOURCE_DESC* /*pDesc*/,
        D3D12_RESOURCE_STATES /*InitialState*/,
        const void* /*pOptimizedClearValue*/,
        const IID& /*riid*/,
        void** /*ppvResource*/) { return 0; }
};

static constexpr IID IID_ID3D12Device8 = 
    { 0x9218e66e, 0x19b4, 0x4ec2, { 0xa7, 0x76, 0xb2, 0xd4, 0x44, 0x53, 0x4f, 0x16 } };

class ID3D12Device8 : public ID3D12Device {
public:
    virtual void CreateSamplerFeedbackUnorderedAccessView(
        ID3D12Resource* pTargetedResource,
        ID3D12Resource* pFeedbackMinMipMap,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;
    virtual int32_t OpenExistingHeapFromAddress(const void* /*pAddress*/, const IID& /*riid*/, void** /*ppvHeap*/) { return 0; }
    virtual int32_t OpenExistingHeapFromFileMapping(void* /*hFileMapping*/, const IID& /*riid*/, void** /*ppvHeap*/) { return 0; }
    virtual int32_t CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC* /*pDesc*/, const IID& /*riid*/, void** /*ppPipelineState*/) { return 0; }
};

enum D3D12_SHADER_CACHE_MODE {
    D3D12_SHADER_CACHE_MODE_MEMORY = 0,
    D3D12_SHADER_CACHE_MODE_DISK   = 1
};

enum D3D12_SHADER_CACHE_FLAGS : uint32_t {
    D3D12_SHADER_CACHE_FLAG_NONE           = 0,
    D3D12_SHADER_CACHE_FLAG_DRIVER_MANAGED = 1
};

struct D3D12_SHADER_CACHE_SESSION_DESC {
    GUID Identifier;
    D3D12_SHADER_CACHE_MODE Mode;
    D3D12_SHADER_CACHE_FLAGS Flags;
    uint32_t MaximumInMemoryCacheSizeBytes;
    uint32_t MaximumInMemoryCacheEntries;
    uint32_t MaximumValueFileSizeBytes;
};

static constexpr IID IID_ID3D12ShaderCacheSession = 
    { 0x2804f332, 0xd0e2, 0x40aa, { 0xac, 0x2b, 0x8f, 0x72, 0x6b, 0x03, 0x34, 0x99 } };

class ID3D12ShaderCacheSession : public ID3D12DeviceChild {
public:
    virtual int32_t FindValue(const void* pKey, uint32_t KeySize, void* pValue, uint32_t* pValueSize) = 0;
    virtual int32_t StoreValue(const void* pKey, uint32_t KeySize, const void* pValue, uint32_t ValueSize) = 0;
    virtual void SetDeleteOnDestroy() = 0;
    virtual D3D12_SHADER_CACHE_SESSION_DESC GetDesc() = 0;
};

static constexpr IID IID_ID3D12Device9 = 
    { 0x56314f85, 0xef42, 0x4858, { 0xb9, 0x93, 0x76, 0x3d, 0x5a, 0x33, 0x22, 0x18 } };

class ID3D12Device9 : public ID3D12Device8 {
public:
    virtual int32_t CreateShaderCacheSession(
        const D3D12_SHADER_CACHE_SESSION_DESC* pDesc,
        const IID& riid,
        void** ppvSession) = 0;
    virtual int32_t ShaderCacheControl(uint32_t /*Kinds*/, uint32_t /*Control*/) { return 0; }
};

// ============================================================================
// 5. Concrete Implementation Classes
// ============================================================================

class Prism3D12FenceImpl : public ID3D12Fence {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    std::atomic<uint64_t> m_value{ 0 };

public:
    Prism3D12FenceImpl(ID3D12Device* pDev, uint64_t initialVal)
        : m_pDevice(pDev), m_value(initialVal) {}

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Fence) {
            *ppv = static_cast<ID3D12Fence*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }
    int32_t GetDevice(const IID&, void** ppDevice) override {
        if (!ppDevice) return -1;
        *ppDevice = m_pDevice;
        if (m_pDevice) m_pDevice->AddRef();
        return 0;
    }

    uint64_t GetCompletedValue() override {
        return m_value.load(std::memory_order_acquire);
    }

    int32_t SetEventOnCompletion(uint64_t, void*) override {
        return 0; // Immediate complete in synchronous reference model
    }

    int32_t Signal(uint64_t Value) override {
        m_value.store(Value, std::memory_order_release);
        return 0;
    }
};

class Prism3D12CommandAllocatorImpl : public ID3D12CommandAllocator {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    D3D12_COMMAND_LIST_TYPE m_type;
    uint32_t m_resetCount{ 0 };

public:
    Prism3D12CommandAllocatorImpl(ID3D12Device* pDev, D3D12_COMMAND_LIST_TYPE type)
        : m_pDevice(pDev), m_type(type) {}

    [[nodiscard]] D3D12_COMMAND_LIST_TYPE GetType() const noexcept { return m_type; }

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild || riid == IID_ID3D12CommandAllocator) {
            *ppv = static_cast<ID3D12CommandAllocator*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }
    int32_t GetDevice(const IID&, void** ppDevice) override {
        if (!ppDevice) return -1;
        *ppDevice = m_pDevice;
        if (m_pDevice) m_pDevice->AddRef();
        return 0;
    }

    int32_t Reset() override {
        m_resetCount++;
        return 0;
    }

    uint32_t GetResetCount() const { return m_resetCount; }
};

class Prism3D12ResourceImpl : public ID3D12Resource {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    D3D12_RESOURCE_DESC m_desc{};
    D3D12_RESOURCE_STATES m_currentState;
    std::vector<uint8_t> m_memory;
    uint64_t m_gpuVirtualAddress{ 0 };
    ID3D12Heap* m_pHeap{ nullptr };
    uint64_t m_heapOffset{ 0 };

public:
    Prism3D12ResourceImpl(ID3D12Device* pDev, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state, ID3D12Heap* pHeap = nullptr, uint64_t heapOffset = 0)
        : m_pDevice(pDev), m_desc(desc), m_currentState(state), m_pHeap(pHeap), m_heapOffset(heapOffset) {
        if (m_pHeap) m_pHeap->AddRef();
        size_t allocSize = 0;
        if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
            allocSize = static_cast<size_t>(desc.Width);
        } else if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
            allocSize = static_cast<size_t>(desc.Width) * desc.Height * 4; // 32-bpp BGRA/RGBA
        }
        if (allocSize > 0) {
            m_memory.resize(allocSize, 0);
            m_gpuVirtualAddress = reinterpret_cast<uint64_t>(m_memory.data());
        }
    }

    ~Prism3D12ResourceImpl() {
        if (m_pHeap) m_pHeap->Release();
    }

    [[nodiscard]] ID3D12Heap* GetHeap() const noexcept { return m_pHeap; }
    [[nodiscard]] uint64_t GetHeapOffset() const noexcept { return m_heapOffset; }

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Resource) {
            *ppv = static_cast<ID3D12Resource*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }
    int32_t GetDevice(const IID&, void** ppDevice) override {
        if (!ppDevice) return -1;
        *ppDevice = m_pDevice;
        if (m_pDevice) m_pDevice->AddRef();
        return 0;
    }

    int32_t Map(uint32_t, const void*, void** ppData) override {
        if (!ppData) return -1;
        *ppData = m_memory.data();
        return 0;
    }

    void Unmap(uint32_t, const void*) override {}

    D3D12_RESOURCE_DESC GetDesc() override { return m_desc; }
    uint64_t GetGPUVirtualAddress() override { return m_gpuVirtualAddress; }

    uint8_t* GetData() { return m_memory.data(); }
    size_t GetSize() const { return m_memory.size(); }
    D3D12_RESOURCE_STATES GetCurrentState() const { return m_currentState; }
    void SetCurrentState(D3D12_RESOURCE_STATES s) { m_currentState = s; }
};

class Prism3D12DescriptorHeapImpl : public ID3D12DescriptorHeap {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    D3D12_DESCRIPTOR_HEAP_DESC m_desc{};
    struct Entry {
        ID3D12Resource* pResource{ nullptr };
        bool isRTV{ false };
        bool isDSV{ false };
        DXGI_FORMAT format{ DXGI_FORMAT_UNKNOWN };
    };
    std::vector<Entry> m_entries;

public:
    Prism3D12DescriptorHeapImpl(ID3D12Device* pDev, const D3D12_DESCRIPTOR_HEAP_DESC& desc)
        : m_pDevice(pDev), m_desc(desc) {
        m_entries.resize(desc.NumDescriptors);
    }

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild || riid == IID_ID3D12DescriptorHeap) {
            *ppv = static_cast<ID3D12DescriptorHeap*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }
    int32_t GetDevice(const IID&, void** ppDevice) override {
        if (!ppDevice) return -1;
        *ppDevice = m_pDevice;
        if (m_pDevice) m_pDevice->AddRef();
        return 0;
    }

    D3D12_DESCRIPTOR_HEAP_DESC GetDesc() override { return m_desc; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandleForHeapStart() override {
        return D3D12_CPU_DESCRIPTOR_HANDLE{ reinterpret_cast<size_t>(this) };
    }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleForHeapStart() override {
        return D3D12_GPU_DESCRIPTOR_HANDLE{ reinterpret_cast<uint64_t>(this) };
    }

    void SetRTV(uint32_t index, ID3D12Resource* pRes, DXGI_FORMAT fmt) {
        if (index < m_entries.size()) {
            m_entries[index].pResource = pRes;
            m_entries[index].isRTV = true;
            m_entries[index].format = fmt;
        }
    }

    void SetDSV(uint32_t index, ID3D12Resource* pRes, DXGI_FORMAT fmt) {
        if (index < m_entries.size()) {
            m_entries[index].pResource = pRes;
            m_entries[index].isDSV = true;
            m_entries[index].format = fmt;
        }
    }

    ID3D12Resource* GetResource(uint32_t index) {
        if (index < m_entries.size()) return m_entries[index].pResource;
        return nullptr;
    }
};

class Prism3D12RootSignatureImpl : public ID3D12RootSignature {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    std::vector<uint8_t> m_blob;

public:
    Prism3D12RootSignatureImpl(ID3D12Device* pDev, const void* pData, size_t len)
        : m_pDevice(pDev) {
        if (pData && len > 0) {
            m_blob.assign(reinterpret_cast<const uint8_t*>(pData), reinterpret_cast<const uint8_t*>(pData) + len);
        }
    }

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild || riid == IID_ID3D12RootSignature) {
            *ppv = static_cast<ID3D12RootSignature*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }
    int32_t GetDevice(const IID&, void** ppDevice) override {
        if (!ppDevice) return -1;
        *ppDevice = m_pDevice;
        if (m_pDevice) m_pDevice->AddRef();
        return 0;
    }
};

class Prism3D12PipelineStateImpl : public ID3D12PipelineState {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_desc{};

public:
    Prism3D12PipelineStateImpl(ID3D12Device* pDev, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
        : m_pDevice(pDev), m_desc(desc) {}

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild || riid == IID_ID3D12PipelineState) {
            *ppv = static_cast<ID3D12PipelineState*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }
    int32_t GetDevice(const IID&, void** ppDevice) override {
        if (!ppDevice) return -1;
        *ppDevice = m_pDevice;
        if (m_pDevice) m_pDevice->AddRef();
        return 0;
    }

    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetDesc() const { return m_desc; }
};

class Prism3D12CommandSignatureImpl : public ID3D12CommandSignature {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    D3D12_COMMAND_SIGNATURE_DESC m_desc{};
    ID3D12RootSignature* m_pRootSignature{ nullptr };

public:
    Prism3D12CommandSignatureImpl(ID3D12Device* pDev, const D3D12_COMMAND_SIGNATURE_DESC& desc, ID3D12RootSignature* pRootSig)
        : m_pDevice(pDev), m_desc(desc), m_pRootSignature(pRootSig) {
        if (m_pRootSignature) m_pRootSignature->AddRef();
    }

    ~Prism3D12CommandSignatureImpl() {
        if (m_pRootSignature) m_pRootSignature->Release();
    }

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild ||
            riid == IID_ID3D12Pageable || riid == IID_ID3D12CommandSignature) {
            *ppv = static_cast<ID3D12CommandSignature*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }
    int32_t GetDevice(const IID& riid, void** ppDevice) override {
        if (!ppDevice || !m_pDevice) return -1;
        return m_pDevice->QueryInterface(riid, ppDevice);
    }

    [[nodiscard]] const D3D12_COMMAND_SIGNATURE_DESC& GetDesc() const noexcept { return m_desc; }
    [[nodiscard]] ID3D12RootSignature* GetRootSignature() const noexcept { return m_pRootSignature; }
};

class Prism3D12MetaCommandImpl : public ID3D12MetaCommand {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    GUID m_commandId{};
    uint32_t m_nodeMask{ 0 };
    std::vector<uint8_t> m_creationParams;
    std::wstring m_name;

public:
    Prism3D12MetaCommandImpl(ID3D12Device* pDev, const GUID& id, uint32_t nodeMask, const void* pCreationData, size_t dataSize, const wchar_t* name = L"DirectSR_NeuralUpscaling")
        : m_pDevice(pDev), m_commandId(id), m_nodeMask(nodeMask), m_name(name ? name : L"") {
        if (pCreationData && dataSize > 0) {
            const auto* ptr = reinterpret_cast<const uint8_t*>(pCreationData);
            m_creationParams.assign(ptr, ptr + dataSize);
        }
    }

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild ||
            riid == IID_ID3D12Pageable || riid == IID_ID3D12MetaCommand) {
            *ppv = static_cast<ID3D12MetaCommand*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t* Name) override {
        if (Name) m_name = Name;
        return 0;
    }
    int32_t GetDevice(const IID& riid, void** ppDevice) override {
        if (!ppDevice || !m_pDevice) return -1;
        return m_pDevice->QueryInterface(riid, ppDevice);
    }

    uint64_t GetRequiredParameterResourceSize(D3D12_META_COMMAND_PARAMETER_STAGE /*Stage*/, uint32_t /*ParameterIndex*/) override {
        return 1024 * 1024; // 1 MB scratch buffer
    }

    [[nodiscard]] const GUID& GetCommandId() const noexcept { return m_commandId; }
    [[nodiscard]] uint32_t GetNodeMask() const noexcept { return m_nodeMask; }
    [[nodiscard]] const std::wstring& GetCommandName() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<uint8_t>& GetCreationParams() const noexcept { return m_creationParams; }
};

// Command List implementation
class Prism3D12GraphicsCommandListImpl : public ID3D12GraphicsCommandList10 {
public:
    enum class CommandType {
        TransitionBarrier,
        SetViewport,
        SetScissor,
        SetRenderTargets,
        ClearRTV,
        ClearDSV,
        SetTopology,
        SetVertexBuffer,
        SetIndexBuffer,
        SetPipelineState,
        DrawInstanced,
        DrawIndexedInstanced
    };

    struct Command {
        CommandType type;
        D3D12_RESOURCE_TRANSITION_BARRIER barrier{};
        D3D12_VIEWPORT viewport{};
        D3D12_RECT scissor{};
        D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
        D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
        float clearColor[4]{};
        float clearDepth{ 1.0f };
        uint8_t clearStencil{ 0 };
        D3D_PRIMITIVE_TOPOLOGY topology{ D3D_PRIMITIVE_TOPOLOGY_UNDEFINED };
        D3D12_VERTEX_BUFFER_VIEW vbView{};
        D3D12_INDEX_BUFFER_VIEW ibView{};
        ID3D12PipelineState* pso{ nullptr };
        uint32_t vertexCount{ 0 };
        uint32_t indexCount{ 0 };
        uint32_t startIndex{ 0 };
        int32_t baseVertex{ 0 };
    };

private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    ID3D12CommandAllocator* m_pAllocator{ nullptr };
    D3D12_COMMAND_LIST_TYPE m_type{ D3D12_COMMAND_LIST_TYPE_DIRECT };
    bool m_isOpen{ true };
    std::vector<Command> m_commands;
    bool m_hasProgram{ false };
    D3D12_SET_PROGRAM_DESC m_currentProgram{};
    std::vector<D3D12_DISPATCH_GRAPH_DESC> m_dispatchedGraphs;
    uint32_t m_recordedEnhancedBarriersCount{ 0 };
    uint32_t m_dispatchedMeshCount{ 0 };
    uint32_t m_dispatchedRaysCount{ 0 };
    D3D12_DISPATCH_RAYS_DESC m_lastDispatchRaysDesc{};
    uint32_t m_executeIndirectCount{ 0 };
    uint32_t m_lastIndirectMaxCommands{ 0 };
    ID3D12CommandSignature* m_pLastCommandSignature{ nullptr };
    uint32_t m_metaCommandInitCount{ 0 };
    uint32_t m_metaCommandExecCount{ 0 };
    ID3D12MetaCommand* m_pLastMetaCommand{ nullptr };
    std::vector<uint8_t> m_lastMetaCommandInitData;
    std::vector<uint8_t> m_lastMetaCommandExecData;

public:
    Prism3D12GraphicsCommandListImpl(ID3D12Device* pDev, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* pAlloc)
        : m_pDevice(pDev), m_pAllocator(pAlloc), m_type(type) {}

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild ||
            riid == IID_ID3D12CommandList || riid == IID_ID3D12GraphicsCommandList ||
            riid == IID_ID3D12GraphicsCommandList4 ||
            riid == IID_ID3D12GraphicsCommandList6 || riid == IID_ID3D12GraphicsCommandList7 || riid == IID_ID3D12GraphicsCommandList10) {
            *ppv = static_cast<ID3D12GraphicsCommandList10*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }
    int32_t GetDevice(const IID&, void** ppDevice) override {
        if (!ppDevice) return -1;
        *ppDevice = m_pDevice;
        if (m_pDevice) m_pDevice->AddRef();
        return 0;
    }

    D3D12_COMMAND_LIST_TYPE GetType() override { return m_type; }

    int32_t Close() override {
        m_isOpen = false;
        return 0;
    }

    int32_t Reset(ID3D12CommandAllocator* pAllocator, ID3D12PipelineState*) override {
        m_pAllocator = pAllocator;
        m_commands.clear();
        m_dispatchedGraphs.clear();
        m_dispatchedMeshCount = 0;
        m_dispatchedRaysCount = 0;
        m_hasProgram = false;
        m_isOpen = true;
        return 0;
    }

    void ClearState(ID3D12PipelineState*) override {
        m_commands.clear();
        m_dispatchedGraphs.clear();
        m_hasProgram = false;
    }

    void ResourceBarrier(uint32_t NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers) override {
        if (!m_isOpen || !pBarriers) return;
        for (uint32_t i = 0; i < NumBarriers; ++i) {
            if (pBarriers[i].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
                Command cmd{};
                cmd.type = CommandType::TransitionBarrier;
                cmd.barrier = pBarriers[i].Transition;
                m_commands.push_back(cmd);
            }
        }
    }

    void RSSetViewports(uint32_t NumViewports, const D3D12_VIEWPORT* pViewports) override {
        if (!m_isOpen || NumViewports == 0 || !pViewports) return;
        Command cmd{};
        cmd.type = CommandType::SetViewport;
        cmd.viewport = pViewports[0];
        m_commands.push_back(cmd);
    }

    void RSSetScissorRects(uint32_t NumRects, const D3D12_RECT* pRects) override {
        if (!m_isOpen || NumRects == 0 || !pRects) return;
        Command cmd{};
        cmd.type = CommandType::SetScissor;
        cmd.scissor = pRects[0];
        m_commands.push_back(cmd);
    }

    void OMSetRenderTargets(uint32_t NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors, int32_t, const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) override {
        if (!m_isOpen) return;
        Command cmd{};
        cmd.type = CommandType::SetRenderTargets;
        if (NumRenderTargetDescriptors > 0 && pRenderTargetDescriptors) {
            cmd.rtv = pRenderTargetDescriptors[0];
        }
        if (pDepthStencilDescriptor) {
            cmd.dsv = *pDepthStencilDescriptor;
        }
        m_commands.push_back(cmd);
    }

    void ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const float ColorRGBA[4], uint32_t, const D3D12_RECT*) override {
        if (!m_isOpen) return;
        Command cmd{};
        cmd.type = CommandType::ClearRTV;
        cmd.rtv = RenderTargetView;
        if (ColorRGBA) std::memcpy(cmd.clearColor, ColorRGBA, sizeof(cmd.clearColor));
        m_commands.push_back(cmd);
    }

    void ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, uint32_t, float Depth, uint8_t Stencil, uint32_t, const D3D12_RECT*) override {
        if (!m_isOpen) return;
        Command cmd{};
        cmd.type = CommandType::ClearDSV;
        cmd.dsv = DepthStencilView;
        cmd.clearDepth = Depth;
        cmd.clearStencil = Stencil;
        m_commands.push_back(cmd);
    }

    void IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology) override {
        if (!m_isOpen) return;
        Command cmd{};
        cmd.type = CommandType::SetTopology;
        cmd.topology = PrimitiveTopology;
        m_commands.push_back(cmd);
    }

    void IASetVertexBuffers(uint32_t, uint32_t NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews) override {
        if (!m_isOpen || NumViews == 0 || !pViews) return;
        Command cmd{};
        cmd.type = CommandType::SetVertexBuffer;
        cmd.vbView = pViews[0];
        m_commands.push_back(cmd);
    }

    void IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView) override {
        if (!m_isOpen || !pView) return;
        Command cmd{};
        cmd.type = CommandType::SetIndexBuffer;
        cmd.ibView = *pView;
        m_commands.push_back(cmd);
    }

    void SetGraphicsRootSignature(ID3D12RootSignature*) override {}

    void SetPipelineState(ID3D12PipelineState* pPipelineState) override {
        if (!m_isOpen || !pPipelineState) return;
        Command cmd{};
        cmd.type = CommandType::SetPipelineState;
        cmd.pso = pPipelineState;
        m_commands.push_back(cmd);
    }

    void DrawInstanced(uint32_t VertexCountPerInstance, uint32_t, uint32_t StartVertexLocation, uint32_t) override {
        if (!m_isOpen) return;
        Command cmd{};
        cmd.type = CommandType::DrawInstanced;
        cmd.vertexCount = VertexCountPerInstance;
        cmd.startIndex = StartVertexLocation;
        m_commands.push_back(cmd);
    }

    void DrawIndexedInstanced(uint32_t IndexCountPerInstance, uint32_t, uint32_t StartIndexLocation, int32_t BaseVertexLocation, uint32_t) override {
        if (!m_isOpen) return;
        Command cmd{};
        cmd.type = CommandType::DrawIndexedInstanced;
        cmd.indexCount = IndexCountPerInstance;
        cmd.startIndex = StartIndexLocation;
        cmd.baseVertex = BaseVertexLocation;
        m_commands.push_back(cmd);
    }

    void SetProgram(const D3D12_SET_PROGRAM_DESC* pDesc) override {
        if (!m_isOpen || !pDesc) return;
        m_currentProgram = *pDesc;
        m_hasProgram = true;
    }

    void DispatchGraph(const D3D12_DISPATCH_GRAPH_DESC* pDesc) override {
        if (!m_isOpen || !pDesc) return;
        m_dispatchedGraphs.push_back(*pDesc);
    }

    void DispatchRays(const D3D12_DISPATCH_RAYS_DESC* pDesc) override {
        if (!m_isOpen) return;
        m_dispatchedRaysCount++;
        if (pDesc) {
            m_lastDispatchRaysDesc = *pDesc;
        }
    }

    [[nodiscard]] uint32_t GetDispatchedRaysCount() const noexcept { return m_dispatchedRaysCount; }
    [[nodiscard]] const D3D12_DISPATCH_RAYS_DESC& GetLastDispatchRaysDesc() const noexcept { return m_lastDispatchRaysDesc; }

    void ExecuteIndirect(
        ID3D12CommandSignature* pCommandSignature,
        uint32_t MaxCommandCount,
        ID3D12Resource* /*pArgumentBuffer*/,
        uint64_t /*ArgumentBufferOffset*/,
        ID3D12Resource* /*pCountBuffer*/,
        uint64_t /*CountBufferOffset*/) override {
        m_executeIndirectCount++;
        m_lastIndirectMaxCommands = MaxCommandCount;
        m_pLastCommandSignature = pCommandSignature;
    }

    [[nodiscard]] uint32_t GetExecuteIndirectCount() const noexcept { return m_executeIndirectCount; }
    [[nodiscard]] uint32_t GetLastIndirectMaxCommands() const noexcept { return m_lastIndirectMaxCommands; }
    [[nodiscard]] ID3D12CommandSignature* GetLastCommandSignature() const noexcept { return m_pLastCommandSignature; }

    void InitializeMetaCommand(ID3D12MetaCommand* pMetaCommand, const void* pInitializationParametersData, size_t InitializationParametersDataSizeInBytes) override {
        m_metaCommandInitCount++;
        m_pLastMetaCommand = pMetaCommand;
        m_lastMetaCommandInitData.clear();
        if (pInitializationParametersData && InitializationParametersDataSizeInBytes > 0) {
            auto* p = reinterpret_cast<const uint8_t*>(pInitializationParametersData);
            m_lastMetaCommandInitData.assign(p, p + InitializationParametersDataSizeInBytes);
        }
    }

    void ExecuteMetaCommand(ID3D12MetaCommand* pMetaCommand, const void* pExecutionParametersData, size_t ExecutionParametersDataSizeInBytes) override {
        m_metaCommandExecCount++;
        m_pLastMetaCommand = pMetaCommand;
        m_lastMetaCommandExecData.clear();
        if (pExecutionParametersData && ExecutionParametersDataSizeInBytes > 0) {
            auto* p = reinterpret_cast<const uint8_t*>(pExecutionParametersData);
            m_lastMetaCommandExecData.assign(p, p + ExecutionParametersDataSizeInBytes);
        }
    }

    [[nodiscard]] uint32_t GetMetaCommandInitCount() const noexcept { return m_metaCommandInitCount; }
    [[nodiscard]] uint32_t GetMetaCommandExecCount() const noexcept { return m_metaCommandExecCount; }
    [[nodiscard]] ID3D12MetaCommand* GetLastMetaCommand() const noexcept { return m_pLastMetaCommand; }
    [[nodiscard]] const std::vector<uint8_t>& GetLastMetaCommandInitData() const noexcept { return m_lastMetaCommandInitData; }
    [[nodiscard]] const std::vector<uint8_t>& GetLastMetaCommandExecData() const noexcept { return m_lastMetaCommandExecData; }

    void DispatchMesh(uint32_t ThreadGroupCountX, uint32_t ThreadGroupCountY, uint32_t ThreadGroupCountZ) override {
        if (!m_isOpen) return;
        (void)ThreadGroupCountX; (void)ThreadGroupCountY; (void)ThreadGroupCountZ;
        m_dispatchedMeshCount++;
    }

    [[nodiscard]] uint32_t GetDispatchedMeshCount() const noexcept { return m_dispatchedMeshCount; }

    void Barrier(uint32_t NumBarrierGroups, const D3D12_BARRIER_GROUP* pBarrierGroups) override {
        if (!m_isOpen || !pBarrierGroups) return;
        m_recordedEnhancedBarriersCount += NumBarrierGroups;
    }

    [[nodiscard]] uint32_t GetRecordedEnhancedBarriersCount() const noexcept { return m_recordedEnhancedBarriersCount; }

    bool m_inRenderPass{ false };
    uint32_t m_renderPassNumTargets{ 0 };
    D3D12_RENDER_PASS_FLAGS m_renderPassFlags{ D3D12_RENDER_PASS_FLAG_NONE };
    uint32_t m_renderPassTotalCount{ 0 };
    std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> m_activeRenderTargets;
    bool m_hasDepthStencil{ false };
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC m_activeDepthStencil{};

    void BeginRenderPass(
        uint32_t NumRenderTargets,
        const D3D12_RENDER_PASS_RENDER_TARGET_DESC* pRenderTargets,
        const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* pDepthStencil,
        D3D12_RENDER_PASS_FLAGS Flags) override {
        if (!m_isOpen) return;
        m_inRenderPass = true;
        m_renderPassNumTargets = NumRenderTargets;
        m_renderPassFlags = Flags;
        m_renderPassTotalCount++;
        m_activeRenderTargets.clear();
        if (pRenderTargets && NumRenderTargets > 0) {
            for (uint32_t i = 0; i < NumRenderTargets; ++i) {
                m_activeRenderTargets.push_back(pRenderTargets[i]);
            }
        }
        if (pDepthStencil) {
            m_hasDepthStencil = true;
            m_activeDepthStencil = *pDepthStencil;
        } else {
            m_hasDepthStencil = false;
        }
    }

    void EndRenderPass() override {
        if (!m_isOpen) return;
        m_inRenderPass = false;
    }

    [[nodiscard]] bool IsInRenderPass() const noexcept { return m_inRenderPass; }
    [[nodiscard]] uint32_t GetRenderPassTotalCount() const noexcept { return m_renderPassTotalCount; }
    [[nodiscard]] uint32_t GetActiveRenderPassNumTargets() const noexcept { return m_renderPassNumTargets; }
    [[nodiscard]] D3D12_RENDER_PASS_FLAGS GetActiveRenderPassFlags() const noexcept { return m_renderPassFlags; }
    [[nodiscard]] bool HasActiveDepthStencil() const noexcept { return m_hasDepthStencil; }
    [[nodiscard]] bool HasProgram() const noexcept { return m_hasProgram; }
    [[nodiscard]] const D3D12_SET_PROGRAM_DESC& GetCurrentProgram() const noexcept { return m_currentProgram; }
    [[nodiscard]] size_t GetDispatchedGraphCount() const noexcept { return m_dispatchedGraphs.size(); }
    [[nodiscard]] const std::vector<D3D12_DISPATCH_GRAPH_DESC>& GetDispatchedGraphs() const noexcept { return m_dispatchedGraphs; }

    const std::vector<Command>& GetRecordedCommands() const { return m_commands; }
    bool IsOpen() const { return m_isOpen; }
};

// Command Queue implementation
class Prism3D12CommandQueueImpl : public ID3D12CommandQueue {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    D3D12_COMMAND_QUEUE_DESC m_desc{};
    uint64_t m_lastExecutedCommandsCount{ 0 };

public:
    Prism3D12CommandQueueImpl(ID3D12Device* pDev, const D3D12_COMMAND_QUEUE_DESC& desc)
        : m_pDevice(pDev), m_desc(desc) {}

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild || riid == IID_ID3D12CommandQueue) {
            *ppv = static_cast<ID3D12CommandQueue*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }
    int32_t GetDevice(const IID&, void** ppDevice) override {
        if (!ppDevice) return -1;
        *ppDevice = m_pDevice;
        if (m_pDevice) m_pDevice->AddRef();
        return 0;
    }

    D3D12_COMMAND_QUEUE_DESC GetDesc() override { return m_desc; }

    void ExecuteCommandLists(uint32_t NumCommandLists, ID3D12CommandList* const* ppCommandLists) override {
        if (NumCommandLists == 0 || !ppCommandLists) return;

        // Process recorded command lists against the hardware/software pipeline
        for (uint32_t i = 0; i < NumCommandLists; ++i) {
            auto* cmdList = static_cast<Prism3D12GraphicsCommandListImpl*>(ppCommandLists[i]);
            if (!cmdList || cmdList->IsOpen()) continue; // Closed command lists only

            const auto& commands = cmdList->GetRecordedCommands();
            for (const auto& cmd : commands) {
                if (cmd.type == Prism3D12GraphicsCommandListImpl::CommandType::TransitionBarrier) {
                    if (cmd.barrier.pResource) {
                        auto* res = static_cast<Prism3D12ResourceImpl*>(cmd.barrier.pResource);
                        res->SetCurrentState(cmd.barrier.StateAfter);
                    }
                }
                m_lastExecutedCommandsCount++;
            }
        }

        // Interop with WDDM Ring 0 telemetry
        auto& dxg = dxgkrnl::DxgkrnlSubsystem::GetInstance();
        dxgkrnl::D3DKMT_SUBMITCOMMAND kmtCmd{};
        kmtCmd.Commands = 0x7FFD12000000ULL;
        kmtCmd.CommandLength = static_cast<uint32_t>(m_lastExecutedCommandsCount * sizeof(uint32_t));
        dxg.SubmitCommand(&kmtCmd);
    }

    int32_t Signal(ID3D12Fence* pFence, uint64_t Value) override {
        if (!pFence) return -1;
        return pFence->Signal(Value);
    }

    int32_t Wait(ID3D12Fence* pFence, uint64_t Value) override {
        if (!pFence) return -1;
        return (pFence->GetCompletedValue() >= Value) ? 0 : 0;
    }

    uint64_t GetLastExecutedCommandsCount() const { return m_lastExecutedCommandsCount; }
};

class Prism3D12ShaderCacheSessionImpl : public ID3D12ShaderCacheSession {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    D3D12_SHADER_CACHE_SESSION_DESC m_desc{};
    std::mutex m_mutex;
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> m_cache;
    bool m_deleteOnDestroy{ false };
    uint32_t m_findCount{ 0 };
    uint32_t m_storeCount{ 0 };

public:
    Prism3D12ShaderCacheSessionImpl(ID3D12Device* pDev, const D3D12_SHADER_CACHE_SESSION_DESC& desc)
        : m_pDevice(pDev), m_desc(desc) {}

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild || riid == IID_ID3D12ShaderCacheSession) {
            *ppv = static_cast<ID3D12ShaderCacheSession*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }

    int32_t GetDevice(const IID&, void** ppDevice) override {
        if (!ppDevice) return -1;
        *ppDevice = m_pDevice;
        return 0;
    }

    int32_t FindValue(const void* pKey, uint32_t KeySize, void* pValue, uint32_t* pValueSize) override {
        if (!pKey || KeySize == 0 || !pValueSize) return -1;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_findCount++;
        std::vector<uint8_t> k(reinterpret_cast<const uint8_t*>(pKey), reinterpret_cast<const uint8_t*>(pKey) + KeySize);
        auto it = m_cache.find(k);
        if (it == m_cache.end()) return 1; // S_FALSE (not found)
        if (!pValue) {
            *pValueSize = static_cast<uint32_t>(it->second.size());
            return 0;
        }
        if (*pValueSize < it->second.size()) {
            *pValueSize = static_cast<uint32_t>(it->second.size());
            return -1;
        }
        std::memcpy(pValue, it->second.data(), it->second.size());
        *pValueSize = static_cast<uint32_t>(it->second.size());
        return 0;
    }

    int32_t StoreValue(const void* pKey, uint32_t KeySize, const void* pValue, uint32_t ValueSize) override {
        if (!pKey || KeySize == 0 || !pValue || ValueSize == 0) return -1;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_storeCount++;
        std::vector<uint8_t> k(reinterpret_cast<const uint8_t*>(pKey), reinterpret_cast<const uint8_t*>(pKey) + KeySize);
        std::vector<uint8_t> v(reinterpret_cast<const uint8_t*>(pValue), reinterpret_cast<const uint8_t*>(pValue) + ValueSize);
        m_cache[k] = std::move(v);
        return 0;
    }

    void SetDeleteOnDestroy() override {
        m_deleteOnDestroy = true;
    }

    D3D12_SHADER_CACHE_SESSION_DESC GetDesc() override {
        return m_desc;
    }

    [[nodiscard]] uint32_t GetEntryCount() const noexcept { return static_cast<uint32_t>(m_cache.size()); }
    [[nodiscard]] uint32_t GetFindCount() const noexcept { return m_findCount; }
    [[nodiscard]] uint32_t GetStoreCount() const noexcept { return m_storeCount; }
};

// D3D12 Device Implementation
class Prism3D12DeviceImpl : public ID3D12Device9 {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    IUnknown* m_pAdapter{ nullptr };
    D3D_FEATURE_LEVEL m_featureLevel{ D3D_FEATURE_LEVEL_12_0 };
    uint32_t m_samplerFeedbackViewsCreated{ 0 };
    uint32_t m_metaCommandsCreated{ 0 };
    uint32_t m_heapsCreated{ 0 };
    uint32_t m_placedResourcesCreated{ 0 };

public:
    Prism3D12DeviceImpl(IUnknown* pAdapter = nullptr, D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_12_0)
        : m_pAdapter(pAdapter), m_featureLevel(fl) {
        if (m_pAdapter) m_pAdapter->AddRef();
    }

    ~Prism3D12DeviceImpl() override {
        if (m_pAdapter) m_pAdapter->Release();
    }

    [[nodiscard]] D3D_FEATURE_LEVEL GetFeatureLevel() const noexcept { return m_featureLevel; }

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12Device || riid == IID_ID3D12Device8 || riid == IID_ID3D12Device9) {
            *ppv = static_cast<ID3D12Device9*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262;
    }

    uint32_t AddRef() override { return ++m_refCount; }
    uint32_t Release() override {
        uint32_t c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    int32_t GetPrivateData(const IID&, uint32_t*, void*) override { return 0; }
    int32_t SetPrivateData(const IID&, uint32_t, const void*) override { return 0; }
    int32_t SetPrivateDataInterface(const IID&, const IUnknown*) override { return 0; }
    int32_t SetName(const wchar_t*) override { return 0; }

    int32_t CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC* pDesc, const IID&, void** ppCommandQueue) override {
        if (!pDesc || !ppCommandQueue) return -1;
        *ppCommandQueue = new Prism3D12CommandQueueImpl(this, *pDesc);
        return 0;
    }

    int32_t CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, const IID&, void** ppCommandAllocator) override {
        if (!ppCommandAllocator) return -1;
        *ppCommandAllocator = new Prism3D12CommandAllocatorImpl(this, type);
        return 0;
    }

    int32_t CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, const IID&, void** ppPipelineState) override {
        if (!pDesc || !ppPipelineState) return -1;
        *ppPipelineState = new Prism3D12PipelineStateImpl(this, *pDesc);
        m_pipelineStatesCreated++;
        return 0;
    }

    uint32_t m_pipelineStatesCreated{ 0 };
    uint32_t m_commandSignaturesCreated{ 0 };
    [[nodiscard]] uint32_t GetPipelineStatesCreated() const noexcept { return m_pipelineStatesCreated; }
    [[nodiscard]] uint32_t GetCommandSignaturesCreated() const noexcept { return m_commandSignaturesCreated; }
    [[nodiscard]] uint32_t GetMetaCommandsCreated() const noexcept { return m_metaCommandsCreated; }
    [[nodiscard]] uint32_t GetHeapsCreated() const noexcept { return m_heapsCreated; }
    [[nodiscard]] uint32_t GetPlacedResourcesCreated() const noexcept { return m_placedResourcesCreated; }

    D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo(
        uint32_t visibleMask,
        uint32_t numResourceDescs,
        const D3D12_RESOURCE_DESC* pResourceDescs) override {
        (void)visibleMask;
        D3D12_RESOURCE_ALLOCATION_INFO info{};
        if (!pResourceDescs || numResourceDescs == 0) return info;
        for (uint32_t i = 0; i < numResourceDescs; ++i) {
            uint64_t itemSize = 0;
            uint64_t align = 64 * 1024;
            if (pResourceDescs[i].Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
                itemSize = pResourceDescs[i].Width;
            } else if (pResourceDescs[i].Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
                itemSize = pResourceDescs[i].Width * pResourceDescs[i].Height * 4;
                if (pResourceDescs[i].SampleDesc.Count > 1) {
                    align = 4 * 1024 * 1024;
                }
            }
            uint64_t alignedItem = (itemSize + align - 1) & ~(align - 1);
            info.SizeInBytes += alignedItem;
            if (align > info.Alignment) info.Alignment = align;
        }
        return info;
    }

    int32_t CreateHeap(
        const D3D12_HEAP_DESC* pDesc,
        const IID& riid,
        void** ppvHeap) override {
        if (!pDesc || !ppvHeap) return -1;
        auto* heap = new Prism3D12HeapImpl(this, *pDesc);
        int32_t hr = heap->QueryInterface(riid, ppvHeap);
        heap->Release();
        if (hr == 0) m_heapsCreated++;
        return hr;
    }

    int32_t CreatePlacedResource(
        ID3D12Heap* pHeap,
        uint64_t HeapOffset,
        const D3D12_RESOURCE_DESC* pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const void* /*pOptimizedClearValue*/,
        const IID& riid,
        void** ppvResource) override {
        if (!pHeap || !pDesc || !ppvResource) return -1;
        auto* res = new Prism3D12ResourceImpl(this, *pDesc, InitialState, pHeap, HeapOffset);
        int32_t hr = res->QueryInterface(riid, ppvResource);
        res->Release();
        if (hr == 0) m_placedResourcesCreated++;
        return hr;
    }

    int32_t EnumerateMetaCommands(uint32_t* pNumMetaCommands, D3D12_META_COMMAND_DESC* pDescs) override {
        if (!pNumMetaCommands) return -1;
        if (!pDescs) {
            *pNumMetaCommands = 1;
            return 0;
        }
        if (*pNumMetaCommands >= 1) {
            static constexpr GUID DirectSRGuid = { 0xa2b3c4d5, 0x1111, 0x2222, { 0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66 } };
            pDescs[0].Id = DirectSRGuid;
            pDescs[0].Name = L"DirectSR_NeuralUpscaling";
            pDescs[0].InitializationDirtyState = D3D12_GRAPHICS_STATE_NONE;
            pDescs[0].ExecutionDirtyState = D3D12_GRAPHICS_STATE_COMPUTE_ROOT_SIGNATURE;
            *pNumMetaCommands = 1;
            return 0;
        }
        return -1;
    }

    int32_t EnumerateMetaCommandParameters(
        const GUID& /*CommandId*/,
        D3D12_META_COMMAND_PARAMETER_STAGE /*Stage*/,
        uint32_t* pTotalStructureSizeInBytes,
        uint32_t* pParameterCount,
        D3D12_META_COMMAND_PARAMETER_DESC* pParameterDescs) override {
        if (pTotalStructureSizeInBytes) *pTotalStructureSizeInBytes = 64;
        if (!pParameterCount) return -1;
        if (!pParameterDescs) {
            *pParameterCount = 2;
            return 0;
        }
        if (*pParameterCount >= 2) {
            pParameterDescs[0].Name = L"InputTexture";
            pParameterDescs[0].Type = D3D12_META_COMMAND_PARAMETER_TYPE_RESOURCE;
            pParameterDescs[0].Flags = D3D12_META_COMMAND_PARAMETER_FLAG_INPUT;
            pParameterDescs[0].RequiredResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            pParameterDescs[0].StructureOffsetInBytes = 0;

            pParameterDescs[1].Name = L"OutputTexture";
            pParameterDescs[1].Type = D3D12_META_COMMAND_PARAMETER_TYPE_RESOURCE;
            pParameterDescs[1].Flags = D3D12_META_COMMAND_PARAMETER_FLAG_OUTPUT;
            pParameterDescs[1].RequiredResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            pParameterDescs[1].StructureOffsetInBytes = 8;
            *pParameterCount = 2;
            return 0;
        }
        return -1;
    }

    int32_t CreateMetaCommand(
        const GUID& CommandId,
        uint32_t NodeMask,
        const void* pCreationParametersData,
        size_t CreationParametersDataSizeInBytes,
        const IID& riid,
        void** ppMetaCommand) override {
        if (!ppMetaCommand) return -1;
        auto* metaCmd = new Prism3D12MetaCommandImpl(this, CommandId, NodeMask, pCreationParametersData, CreationParametersDataSizeInBytes, L"DirectSR_NeuralUpscaling");
        int32_t hr = metaCmd->QueryInterface(riid, ppMetaCommand);
        metaCmd->Release();
        if (hr == 0) {
            m_metaCommandsCreated++;
        }
        return hr;
    }

    int32_t CreateCommandSignature(
        const D3D12_COMMAND_SIGNATURE_DESC* pDesc,
        ID3D12RootSignature* pRootSignature,
        const IID&,
        void** ppvCommandSignature) override {
        if (!pDesc || !ppvCommandSignature) return -1;
        *ppvCommandSignature = new Prism3D12CommandSignatureImpl(this, *pDesc, pRootSignature);
        m_commandSignaturesCreated++;
        return 0;
    }

    int32_t CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc, const IID&, void** ppPipelineState) override {
        if (!pDesc || !pDesc->pPipelineStateSubobjectStream || pDesc->SizeInBytes == 0 || !ppPipelineState) return -1;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc{};
        const uint8_t* pCur = reinterpret_cast<const uint8_t*>(pDesc->pPipelineStateSubobjectStream);
        const uint8_t* pEnd = pCur + pDesc->SizeInBytes;

        while (pCur + sizeof(uint32_t) <= pEnd) {
            uint32_t subType = *reinterpret_cast<const uint32_t*>(pCur);
            switch (subType) {
                case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE: {
                    auto* obj = reinterpret_cast<const CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE*>(pCur);
                    gpsDesc.pRootSignature = obj->pRootSignature;
                    pCur += sizeof(CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE);
                    break;
                }
                case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS: {
                    auto* obj = reinterpret_cast<const CD3DX12_PIPELINE_STATE_STREAM_VS*>(pCur);
                    gpsDesc.VS = obj->Bytecode;
                    pCur += sizeof(CD3DX12_PIPELINE_STATE_STREAM_VS);
                    break;
                }
                case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS: {
                    auto* obj = reinterpret_cast<const CD3DX12_PIPELINE_STATE_STREAM_PS*>(pCur);
                    gpsDesc.PS = obj->Bytecode;
                    pCur += sizeof(CD3DX12_PIPELINE_STATE_STREAM_PS);
                    break;
                }
                case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY: {
                    auto* obj = reinterpret_cast<const CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY*>(pCur);
                    gpsDesc.PrimitiveTopologyType = obj->PrimitiveTopologyType;
                    pCur += sizeof(CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY);
                    break;
                }
                case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS: {
                    auto* obj = reinterpret_cast<const CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS*>(pCur);
                    gpsDesc.NumRenderTargets = obj->Formats.NumRenderTargets;
                    for (uint32_t i = 0; i < 8; ++i) gpsDesc.RTVFormats[i] = obj->Formats.RTFormats[i];
                    pCur += sizeof(CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS);
                    break;
                }
                default: {
                    pCur += sizeof(void*) * 2;
                    break;
                }
            }
        }

        *ppPipelineState = new Prism3D12PipelineStateImpl(this, gpsDesc);
        m_pipelineStatesCreated++;
        return 0;
    }

    int32_t CreateCommandList(uint32_t, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* pAlloc, ID3D12PipelineState*, const IID&, void** ppCommandList) override {
        if (!pAlloc || !ppCommandList) return -1;
        *ppCommandList = new Prism3D12GraphicsCommandListImpl(this, type, pAlloc);
        return 0;
    }

    int32_t CreateFence(uint64_t InitialValue, D3D12_FENCE_FLAGS, const IID&, void** ppFence) override {
        if (!ppFence) return -1;
        *ppFence = new Prism3D12FenceImpl(this, InitialValue);
        return 0;
    }

    int32_t CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC* pDescriptorHeapDesc, const IID&, void** ppvHeap) override {
        if (!pDescriptorHeapDesc || !ppvHeap) return -1;
        *ppvHeap = new Prism3D12DescriptorHeapImpl(this, *pDescriptorHeapDesc);
        return 0;
    }

    int32_t CreateCommittedResource(const D3D12_HEAP_PROPERTIES*, uint32_t, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialResourceState, const void*, const IID&, void** ppvResource) override {
        if (!pDesc || !ppvResource) return -1;
        *ppvResource = new Prism3D12ResourceImpl(this, *pDesc, InitialResourceState);
        return 0;
    }

    int32_t CreateRootSignature(uint32_t, const void* pBlob, size_t len, const IID&, void** ppvRootSignature) override {
        if (!ppvRootSignature) return -1;
        *ppvRootSignature = new Prism3D12RootSignatureImpl(this, pBlob, len);
        return 0;
    }

    void CreateRenderTargetView(ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override {
        if (!DestDescriptor.ptr) return;
        auto* heap = reinterpret_cast<Prism3D12DescriptorHeapImpl*>(DestDescriptor.ptr);
        DXGI_FORMAT fmt = pDesc ? pDesc->Format : (pResource ? pResource->GetDesc().Format : DXGI_FORMAT_B8G8R8A8_UNORM);
        heap->SetRTV(0, pResource, fmt);
    }

    void CreateDepthStencilView(ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override {
        if (!DestDescriptor.ptr) return;
        auto* heap = reinterpret_cast<Prism3D12DescriptorHeapImpl*>(DestDescriptor.ptr);
        DXGI_FORMAT fmt = pDesc ? pDesc->Format : (pResource ? pResource->GetDesc().Format : DXGI_FORMAT_D32_FLOAT);
        heap->SetDSV(0, pResource, fmt);
    }

    void CreateSamplerFeedbackUnorderedAccessView(
        ID3D12Resource* /*pTargetedResource*/,
        ID3D12Resource* /*pFeedbackMinMipMap*/,
        D3D12_CPU_DESCRIPTOR_HANDLE /*DestDescriptor*/
    ) override {
        m_samplerFeedbackViewsCreated++;
    }

    int32_t CreateShaderCacheSession(
        const D3D12_SHADER_CACHE_SESSION_DESC* pDesc,
        const IID& riid,
        void** ppvSession
    ) override {
        if (!pDesc || !ppvSession) return -1;
        auto* pSession = new Prism3D12ShaderCacheSessionImpl(this, *pDesc);
        int32_t hr = pSession->QueryInterface(riid, ppvSession);
        pSession->Release();
        return hr;
    }

    std::unordered_set<ID3D12Pageable*> m_residentObjects;
    std::unordered_map<ID3D12Pageable*, D3D12_RESIDENCY_PRIORITY> m_residencyPriorities;

    int32_t MakeResident(uint32_t NumObjects, ID3D12Pageable* const* ppObjects) override {
        if (!ppObjects && NumObjects > 0) return -1;
        for (uint32_t i = 0; i < NumObjects; ++i) {
            if (ppObjects[i]) {
                m_residentObjects.insert(ppObjects[i]);
            }
        }
        return 0;
    }

    int32_t Evict(uint32_t NumObjects, ID3D12Pageable* const* ppObjects) override {
        if (!ppObjects && NumObjects > 0) return -1;
        for (uint32_t i = 0; i < NumObjects; ++i) {
            if (ppObjects[i]) {
                m_residentObjects.erase(ppObjects[i]);
            }
        }
        return 0;
    }

    int32_t EnqueueMakeResident(D3D12_RESIDENCY_FLAGS /*Flags*/, uint32_t NumObjects, ID3D12Pageable* const* ppObjects, ID3D12Fence* pFenceToSignal, uint64_t FenceValueToSignal) override {
        MakeResident(NumObjects, ppObjects);
        if (pFenceToSignal) {
            pFenceToSignal->Signal(FenceValueToSignal);
        }
        return 0;
    }

    int32_t SetResidencyPriority(uint32_t NumObjects, ID3D12Pageable* const* ppObjects, const D3D12_RESIDENCY_PRIORITY* pPriorities) override {
        if (!ppObjects || !pPriorities) return -1;
        for (uint32_t i = 0; i < NumObjects; ++i) {
            if (ppObjects[i]) {
                m_residencyPriorities[ppObjects[i]] = pPriorities[i];
            }
        }
        return 0;
    }

    int32_t OpenExistingHeapFromAddress(const void* pAddress, const IID&, void** ppvHeap) override {
        if (!pAddress || !ppvHeap) return -1;
        *ppvHeap = new Prism3D12HeapImpl(pAddress);
        return 0;
    }

    int32_t OpenExistingHeapFromFileMapping(void* hFileMapping, const IID&, void** ppvHeap) override {
        if (!hFileMapping || !ppvHeap) return -1;
        *ppvHeap = new Prism3D12HeapImpl(reinterpret_cast<const void*>(0x00007FF620000000ULL));
        return 0;
    }

    bool IsResident(ID3D12Pageable* pObj) const noexcept {
        return m_residentObjects.find(pObj) != m_residentObjects.end();
    }

    D3D12_RESIDENCY_PRIORITY GetResidencyPriority(ID3D12Pageable* pObj) const noexcept {
        auto it = m_residencyPriorities.find(pObj);
        return (it != m_residencyPriorities.end()) ? it->second : D3D12_RESIDENCY_PRIORITY_NORMAL;
    }

    [[nodiscard]] uint32_t GetSamplerFeedbackViewsCreated() const noexcept { return m_samplerFeedbackViewsCreated; }

    int32_t CheckFeatureSupport(uint32_t Feature, void* pFeatureSupportData, uint32_t FeatureSupportDataSize) override {
        if (!pFeatureSupportData) return -1;
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS5) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS5*>(pFeatureSupportData);
            pData->SRVOnlyTiledResourceTier3 = FALSE;
            pData->RenderPassesTier = D3D12_RENDER_PASS_TIER_0;
            pData->RaytracingTier = D3D12_RAYTRACING_TIER_1_1;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS7) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS7)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS7*>(pFeatureSupportData);
            pData->SamplerFeedbackTier = D3D12_SAMPLER_FEEDBACK_TIER_1_0;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS8) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS8)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS8*>(pFeatureSupportData);
            pData->UnalignedBlockTexturesSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS9) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS9)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS9*>(pFeatureSupportData);
            pData->MeshShaderPipelineStatsSupported = TRUE;
            pData->MeshShaderSupportsFullRangeRenderTargetArrayIndex = TRUE;
            pData->AtomicInt64OnTypedResourceSupported = FALSE;
            pData->AtomicInt64OnGroupSharedSupported = FALSE;
            pData->DerivativesInMeshAndAmplificationShadersSupported = TRUE;
            pData->WaveMMATier = D3D12_WAVE_MMA_TIER_1_0;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS10) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS10)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS10*>(pFeatureSupportData);
            pData->VariableRateShadingSumCombinerSupported = TRUE;
            pData->MeshShaderPerPrimitiveShadingRateSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS11) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS11)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS11*>(pFeatureSupportData);
            pData->AtomicInt64OnDescriptorHeapResourceSupported = TRUE;
            pData->WorkGraphsTier = D3D12_WORK_GRAPHS_TIER_1_0;
            pData->RelaxedFormatCastingSupported = TRUE;
            pData->EnhancedBarriersSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS12) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS12)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS12*>(pFeatureSupportData);
            pData->MSPrimitivesPipelineStatisticIncludesCulledPrimitives = FALSE;
            pData->EnhancedBarriersSupported = TRUE;
            pData->RelaxedFormatCastingSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS13) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS13)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS13*>(pFeatureSupportData);
            pData->UnrestrictedBufferTextureCopyPitchSupported = TRUE;
            pData->UnrestrictedVertexElementAlignmentSupported = TRUE;
            pData->InvertedViewportHeightFlipsYSupported = TRUE;
            pData->InvertedViewportDepthFlipsZSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS14) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS14)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS14*>(pFeatureSupportData);
            pData->AdvancedTextureOpsSupported = TRUE;
            pData->WriteableMSAATexturesSupported = TRUE;
            pData->IndependentFrontAndBackStencilRefMaskSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS15) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS15)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS15*>(pFeatureSupportData);
            pData->TriangleFanSupported = TRUE;
            pData->DynamicIndexBufferStripCut = D3D12_DYNAMIC_INDEX_BUFFER_STRIP_CUT_SUPPORTED_ALL;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS16) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS16)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS16*>(pFeatureSupportData);
            pData->DynamicDepthBiasSupported = TRUE;
            pData->GPUUploadHeapSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS17) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS17)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS17*>(pFeatureSupportData);
            pData->ManualWriteTrackingResourceSupported = TRUE;
            pData->InvertedDepthRangesSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS18) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS18)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS18*>(pFeatureSupportData);
            pData->RenderPassesTier = D3D12_RENDER_PASS_TIER_1;
            pData->AdvancedBlendOpsSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS19) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS19)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS19*>(pFeatureSupportData);
            pData->MipLevelOffsetsSupported = TRUE;
            pData->HalfFloatPrecisionSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS20) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS20)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS20*>(pFeatureSupportData);
            pData->ProgrammableSamplePositionsTier = TRUE;
            pData->SampleLocationsTier = D3D12_SAMPLE_LOCATIONS_TIER_2;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS21) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS21)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS21*>(pFeatureSupportData);
            pData->WorkGraphsTier = D3D12_WORK_GRAPHS_TIER_1_0;
            pData->ExecuteIndirectTier = D3D12_EXECUTE_INDIRECT_TIER_1_1;
            pData->SampleCmpSimilarShaderInstructionsSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS22) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS22)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS22*>(pFeatureSupportData);
            pData->ShaderExecutionReorderingTier = D3D12_SHADER_EXECUTION_REORDERING_TIER_1;
            pData->OpacityMicromapTier = D3D12_OPACITY_MICROMAP_TIER_1_0;
            pData->PreviewFeaturesSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS23) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS23)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS23*>(pFeatureSupportData);
            pData->SubobjectCreationSupported = TRUE;
            pData->WorkGraphsTier1_1Supported = TRUE;
            pData->DynamicDepthBiasClampSupported = TRUE;
            return 0;
        }

        return 0;
    }
};

// ============================================================================
// 6. Exported C APIs (d3d12.dll parity)
// ============================================================================

// ============================================================================
// 6. Direct3D 12 Agility SDK & Debug Layer Interfaces
// ============================================================================

inline constexpr GUID CLSID_D3D12SDKConfiguration = { 0x7cda4236, 0x14f4, 0x48f2, { 0x9f, 0x36, 0xd5, 0x6e, 0x15, 0x56, 0x05, 0x01 } };
inline constexpr GUID IID_ID3D12SDKConfiguration  = { 0xb951e43a, 0x61a4, 0x41f3, { 0x82, 0x6e, 0x2e, 0x3d, 0x17, 0x02, 0x5a, 0xeb } };
inline constexpr GUID IID_ID3D12SDKConfiguration1 = { 0x8ba5fb08, 0x5195, 0x40e2, { 0xac, 0x58, 0x0d, 0x98, 0x9c, 0x3a, 0x01, 0x02 } };
inline constexpr GUID CLSID_D3D12Debug            = { 0x3fda6c3c, 0x5926, 0x4a7a, { 0x9a, 0x67, 0x7c, 0xd8, 0x3f, 0x23, 0x65, 0x71 } };
inline constexpr GUID IID_ID3D12Debug             = { 0x344488b7, 0x6846, 0x474b, { 0xb9, 0x89, 0xf0, 0x27, 0x44, 0x82, 0x45, 0xe0 } };
inline constexpr GUID IID_ID3D12Debug1            = { 0xaffaa442, 0x3042, 0x4e5c, { 0x80, 0xc9, 0xc8, 0x0f, 0xac, 0x66, 0x07, 0xa8 } };

class ID3D12SDKConfiguration : public IUnknown {
public:
    virtual int32_t WINAPI SetSDKVersion(uint32_t SDKVersion, const char* SDKPath) = 0;
};

class ID3D12SDKConfiguration1 : public ID3D12SDKConfiguration {
public:
    virtual int32_t WINAPI CreateDeviceFactory(uint32_t SDKVersion, const char* SDKPath, const IID& riid, void** ppFactory) = 0;
    virtual void WINAPI FreeUnusedSDKs() = 0;
};

class ID3D12Debug : public IUnknown {
public:
    virtual void WINAPI EnableDebugLayer() = 0;
};

class Prism3D12SDKConfigurationImpl : public ID3D12SDKConfiguration1 {
private:
    std::atomic<uint32_t> m_refCount{1};
    uint32_t m_sdkVersion{614}; // Default to Agility SDK 1.614.0
    std::string m_sdkPath{".\\D3D12\\"};

public:
    int32_t WINAPI QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12SDKConfiguration || riid == IID_ID3D12SDKConfiguration1) {
            *ppvObject = static_cast<ID3D12SDKConfiguration1*>(this);
            AddRef();
            return 0; // S_OK
        }
        *ppvObject = nullptr;
        return -2147467262; // E_NOINTERFACE
    }
    uint32_t WINAPI AddRef() override { return ++m_refCount; }
    uint32_t WINAPI Release() override {
        uint32_t r = --m_refCount;
        if (r == 0) delete this;
        return r;
    }
    int32_t WINAPI SetSDKVersion(uint32_t SDKVersion, const char* SDKPath) override {
        m_sdkVersion = SDKVersion;
        if (SDKPath) m_sdkPath = SDKPath;
        return 0; // S_OK
    }
    int32_t WINAPI CreateDeviceFactory(uint32_t SDKVersion, const char* SDKPath, const IID& riid, void** ppFactory) override {
        (void)SDKVersion; (void)SDKPath; (void)riid;
        if (ppFactory) *ppFactory = reinterpret_cast<void*>(0x9001);
        return 0; // S_OK
    }
    void WINAPI FreeUnusedSDKs() override {}

    uint32_t GetConfiguredSDKVersion() const noexcept { return m_sdkVersion; }
    const std::string& GetConfiguredSDKPath() const noexcept { return m_sdkPath; }
};

class Prism3D12DebugImpl : public ID3D12Debug {
private:
    std::atomic<uint32_t> m_refCount{1};
    bool m_debugLayerEnabled{false};

public:
    int32_t WINAPI QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Debug || riid == IID_ID3D12Debug1) {
            *ppvObject = static_cast<ID3D12Debug*>(this);
            AddRef();
            return 0; // S_OK
        }
        *ppvObject = nullptr;
        return -2147467262; // E_NOINTERFACE
    }
    uint32_t WINAPI AddRef() override { return ++m_refCount; }
    uint32_t WINAPI Release() override {
        uint32_t r = --m_refCount;
        if (r == 0) delete this;
        return r;
    }
    void WINAPI EnableDebugLayer() override { m_debugLayerEnabled = true; }
    bool IsDebugLayerEnabled() const noexcept { return m_debugLayerEnabled; }
};

// ============================================================================
// 7. Exported C APIs (d3d12.dll parity)
// ============================================================================

inline int32_t D3D12CreateDevice(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, const IID& riid, void** ppDevice) {
    if (!ppDevice) return -1;
    auto* dev = new Prism3D12DeviceImpl(pAdapter, MinimumFeatureLevel);
    int32_t hr = dev->QueryInterface(riid, ppDevice);
    dev->Release();
    return hr;
}

inline int32_t WINAPI D3D12GetInterface(const GUID& rclsid, const IID& riid, void** ppvDebug) {
    if (!ppvDebug) return -1;
    if (rclsid == CLSID_D3D12SDKConfiguration) {
        auto* cfg = new Prism3D12SDKConfigurationImpl();
        int32_t hr = cfg->QueryInterface(riid, ppvDebug);
        cfg->Release();
        return hr;
    }
    if (rclsid == CLSID_D3D12Debug) {
        auto* dbg = new Prism3D12DebugImpl();
        int32_t hr = dbg->QueryInterface(riid, ppvDebug);
        dbg->Release();
        return hr;
    }
    *ppvDebug = nullptr;
    return -2147467262; // E_NOINTERFACE
}

inline int32_t WINAPI D3D12GetDebugInterface(const IID& riid, void** ppvDebug) {
    return D3D12GetInterface(CLSID_D3D12Debug, riid, ppvDebug);
}

inline int32_t WINAPI D3D12EnableExperimentalFeatures(uint32_t NumFeatures, const IID* /*pIIDs*/, void* /*pConfigurationStructs*/, uint32_t* /*pConfigurationStructSizes*/) {
    (void)NumFeatures;
    return 0; // S_OK
}

inline int32_t WINAPI D3D12_Ordinal101() noexcept { return 0; }
inline int32_t WINAPI D3D12_Ordinal102() noexcept { return 0; }

inline int32_t WINAPI D3D12SerializeVersionedRootSignature(const void* /*pRootSignatureDesc*/, void** ppBlob, void** ppErrorBlob) noexcept {
    if (ppBlob) *ppBlob = reinterpret_cast<void*>(0x8001);
    if (ppErrorBlob) *ppErrorBlob = nullptr;
    return 0; // S_OK
}

inline void InitializePrism3D12SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("d3d12.dll", "D3D12CreateDevice", reinterpret_cast<void*>(D3D12CreateDevice));
    ldr.registerExport("d3d12.dll", "D3D12GetInterface", reinterpret_cast<void*>(D3D12GetInterface));
    ldr.registerExport("d3d12.dll", "D3D12GetDebugInterface", reinterpret_cast<void*>(D3D12GetDebugInterface));
    ldr.registerExport("d3d12.dll", "D3D12EnableExperimentalFeatures", reinterpret_cast<void*>(D3D12EnableExperimentalFeatures));
    ldr.registerExport("d3d12.dll", "D3D12SerializeVersionedRootSignature", reinterpret_cast<void*>(D3D12SerializeVersionedRootSignature));
    ldr.registerExportOrdinal("d3d12.dll", 101, reinterpret_cast<void*>(D3D12_Ordinal101));
    ldr.registerExportOrdinal("d3d12.dll", 102, reinterpret_cast<void*>(D3D12_Ordinal102));
}

} // namespace micant::prism3d12
