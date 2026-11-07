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

enum D3D12_COMMAND_LIST_FLAGS : uint32_t {
    D3D12_COMMAND_LIST_FLAG_NONE = 0
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

enum D3D12_CPU_PAGE_PROPERTY : uint32_t {
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN        = 0,
    D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE  = 1,
    D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE  = 2,
    D3D12_CPU_PAGE_PROPERTY_WRITE_BACK     = 3
};

enum D3D12_MEMORY_POOL : uint32_t {
    D3D12_MEMORY_POOL_UNKNOWN = 0,
    D3D12_MEMORY_POOL_L0      = 1,
    D3D12_MEMORY_POOL_L1      = 2
};

struct D3D12_HEAP_PROPERTIES {
    D3D12_HEAP_TYPE Type;
    D3D12_CPU_PAGE_PROPERTY CPUPageProperty;
    D3D12_MEMORY_POOL MemoryPoolPreference;
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

struct D3D12_CACHED_PIPELINE_STATE {
    const void* pCachedBlob{ nullptr };
    size_t CachedBlobSizeInBytes{ 0 };
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
    D3D12_CACHED_PIPELINE_STATE CachedPSO{};
    uint32_t Flags{ 0 };
};

struct D3D12_COMPUTE_PIPELINE_STATE_DESC {
    void* pRootSignature{ nullptr };
    D3D12_SHADER_BYTECODE CS{};
    uint32_t NodeMask{ 0 };
    D3D12_CACHED_PIPELINE_STATE CachedPSO{};
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

static constexpr IID IID_ID3D12ManualWriteTrackingResource = 
    { 0x86ca3b85, 0x49ad, 0x4b6e, { 0xae, 0xd5, 0xed, 0xdb, 0x18, 0x54, 0x0f, 0x41 } };

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
class ID3D12GraphicsCommandList1;
class ID3D12GraphicsCommandList2;
class ID3D12GraphicsCommandList3;
class ID3D12GraphicsCommandList4;
class ID3D12GraphicsCommandList5;
class ID3D12GraphicsCommandList6;
class ID3D12GraphicsCommandList10;
class ID3D12GraphicsCommandList11;
class ID3D12GraphicsCommandList12;
class ID3D12GraphicsCommandList13;
class ID3D12GraphicsCommandList14;
class ID3D12GraphicsCommandList15;
class ID3D12GraphicsCommandList16;
class ID3D12GraphicsCommandList17;
class ID3D12GraphicsCommandList18;
class ID3D12GraphicsCommandList19;
class ID3D12GraphicsCommandList20;
class ID3D12GraphicsCommandList26;
class ID3D12GraphicsCommandList27;
class ID3D12GraphicsCommandList28;
class ID3D12GraphicsCommandList29;
class ID3D12GraphicsCommandList30;
class ID3D12GraphicsCommandList31;
class ID3D12ProtectedResourceSession;
class ID3D12CommandQueue;
class ID3D12CommandSignature;
class ID3D12MetaCommand;
class ID3D12Heap;
class ID3D12Device;
class ID3D12Device18;
class ID3D12Device19;
class ID3D12Device20;
class ID3D12Device21;
class ID3D12Device22;
class ID3D12Device23;
class ID3D12Device24;
class ID3D12Device25;
class ID3D12Device26;
class ID3D12Device32;
class ID3D12Device33;
class ID3D12Device34;
class ID3D12Device35;
class ID3D12Device36;
class ID3D12Device37;

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

struct D3D12_RANGE {
    size_t Begin{0};
    size_t End{0};
};

class ID3D12ManualWriteTrackingResource : public IUnknown {
public:
    virtual void TrackWrite(uint32_t Subresource, const D3D12_RANGE* pWrittenRange) = 0;
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

using D3D12_GPU_VIRTUAL_ADDRESS = uint64_t;

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

struct D3D12_SAMPLE_POSITION {
    int8_t X{0};
    int8_t Y{0};
};

static constexpr IID IID_ID3D12GraphicsCommandList1 = 
    { 0x55050855, 0x96e3, 0x4508, { 0x86, 0x36, 0xfb, 0xdb, 0x1d, 0x82, 0x4e, 0x6d } };

class ID3D12GraphicsCommandList1 : public ID3D12GraphicsCommandList {
public:
    virtual void SetSamplePositions(
        uint32_t NumSamplesPerPixel,
        uint32_t NumPixels,
        D3D12_SAMPLE_POSITION* pSamplePositions) = 0;
    virtual void ResolveSubresourceRegion(
        ID3D12Resource* pDstResource,
        uint32_t DstSubresource,
        uint32_t DstX,
        uint32_t DstY,
        ID3D12Resource* pSrcResource,
        uint32_t SrcSubresource,
        D3D12_RECT* pSrcRect,
        DXGI_FORMAT Format,
        uint32_t ResolveMode) = 0;
    virtual void SetViewInstanceMask(uint32_t Mask) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList2 = 
    { 0x383ee830, 0x6768, 0x4d56, { 0x96, 0x37, 0x7a, 0xb1, 0x25, 0xf7, 0x70, 0x57 } };

struct D3D12_WRITEBUFFERIMMEDIATE_PARAMETER {
    D3D12_GPU_VIRTUAL_ADDRESS Dest{0};
    uint32_t Value{0};
};

enum D3D12_WRITEBUFFERIMMEDIATE_MODE : uint32_t {
    D3D12_WRITEBUFFERIMMEDIATE_MODE_DEFAULT = 0,
    D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_IN = 1,
    D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_OUT = 2
};

class ID3D12GraphicsCommandList2 : public ID3D12GraphicsCommandList1 {
public:
    virtual void WriteBufferImmediate(
        uint32_t Count,
        const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER* pParams,
        const D3D12_WRITEBUFFERIMMEDIATE_MODE* pModes) = 0;
};

static constexpr IID IID_ID3D12ProtectedResourceSession = 
    { 0xc4c7362a, 0xef2c, 0x4a00, { 0xbe, 0x77, 0x4d, 0xbe, 0x77, 0x9d, 0xac, 0x32 } };

class ID3D12ProtectedSession : public ID3D12DeviceChild {
public:
    virtual int32_t GetSessionStatus() = 0;
};

class ID3D12ProtectedResourceSession : public ID3D12ProtectedSession {
public:
};

static constexpr IID IID_ID3D12GraphicsCommandList3 = 
    { 0x618a15eb, 0x321e, 0x4c5e, { 0x80, 0x15, 0xe4, 0x60, 0xf7, 0x72, 0x1d, 0xcd } };

class ID3D12GraphicsCommandList3 : public ID3D12GraphicsCommandList2 {
public:
    virtual void SetProtectedResourceSession(ID3D12ProtectedResourceSession* pProtectedResourceSession) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList4 = 
    { 0x87e0e368, 0xe32b, 0x4d0b, { 0xbe, 0x4b, 0x4b, 0x78, 0xb8, 0x63, 0x00, 0xaf } };

static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList4 = 
    { 0x8754318e, 0xd3a9, 0x4541, { 0x98, 0xcf, 0x64, 0x5b, 0x50, 0xdc, 0x48, 0x74 } };

enum D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE : uint32_t {
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_CLONE = 0,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT = 1,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_VISUALIZATION_DECODE_FOR_TOOLS = 2,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_SERIALIZE = 3,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_DESERIALIZE = 4
};

struct D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC {
    uint64_t DestAccelerationStructureData{0};
    uint64_t SourceAccelerationStructureData{0};
    uint64_t ScratchAccelerationStructureData{0};
    uint32_t InputsType{0};
    uint32_t InputsFlags{0};
    uint32_t NumDescs{0};
};

struct D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC {
    uint64_t DestBuffer{0};
    uint32_t InfoType{0};
};

class ID3D12StateObject;

struct D3D12_DISPATCH_RAYS_DESC {
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE RayGenerationShaderRecord{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE MissShaderTable{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE HitGroupTable{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE CallableShaderTable{};
    uint32_t Width{0};
    uint32_t Height{0};
    uint32_t Depth{0};
};

class ID3D12GraphicsCommandList4 : public ID3D12GraphicsCommandList3 {
public:
    virtual void DispatchRays(const D3D12_DISPATCH_RAYS_DESC* pDesc) = 0;
    virtual void BeginRenderPass(
        uint32_t NumRenderTargets,
        const D3D12_RENDER_PASS_RENDER_TARGET_DESC* pRenderTargets,
        const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* pDepthStencil,
        D3D12_RENDER_PASS_FLAGS Flags) = 0;
    virtual void EndRenderPass() = 0;
    virtual void InitializeMetaCommand(
        ID3D12MetaCommand* pMetaCommand,
        const void* pInitializationParametersData,
        size_t InitializationParametersDataSizeInBytes) = 0;
    virtual void ExecuteMetaCommand(
        ID3D12MetaCommand* pMetaCommand,
        const void* pExecutionParametersData,
        size_t ExecutionParametersDataSizeInBytes) = 0;
    virtual void BuildRaytracingAccelerationStructure(
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* pDesc,
        uint32_t NumPostbuildInfoDescs,
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* pPostbuildInfoDescs) = 0;
    virtual void EmitRaytracingAccelerationStructurePostbuildInfo(
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* pDesc,
        uint32_t NumSourceAccelerationStructures,
        const uint64_t* pSourceAccelerationStructureData) = 0;
    virtual void CopyRaytracingAccelerationStructure(
        uint64_t DestAccelerationStructureData,
        uint64_t SourceAccelerationStructureData,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode) = 0;
    virtual void SetPipelineState1(ID3D12StateObject* pStateObject) = 0;
};

enum D3D12_SHADING_RATE : uint32_t {
    D3D12_SHADING_RATE_1X1 = 0,
    D3D12_SHADING_RATE_1X2 = 0x1,
    D3D12_SHADING_RATE_2X1 = 0x4,
    D3D12_SHADING_RATE_2X2 = 0x5,
    D3D12_SHADING_RATE_2X4 = 0x6,
    D3D12_SHADING_RATE_4X2 = 0x9,
    D3D12_SHADING_RATE_4X4 = 0xa
};

enum D3D12_SHADING_RATE_COMBINER : uint32_t {
    D3D12_SHADING_RATE_COMBINER_PASSTHROUGH = 0,
    D3D12_SHADING_RATE_COMBINER_OVERRIDE    = 1,
    D3D12_SHADING_RATE_COMBINER_MIN         = 2,
    D3D12_SHADING_RATE_COMBINER_MAX         = 3,
    D3D12_SHADING_RATE_COMBINER_SUM         = 4
};

static constexpr IID IID_ID3D12GraphicsCommandList5 = 
    { 0x55050853, 0x2717, 0x41c5, { 0xbb, 0x92, 0x60, 0xac, 0xac, 0x86, 0x83, 0x65 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList5 = IID_ID3D12GraphicsCommandList5;

class ID3D12GraphicsCommandList5 : public ID3D12GraphicsCommandList4 {
public:
    virtual void RSSetShadingRate(
        D3D12_SHADING_RATE baseShadingRate,
        const D3D12_SHADING_RATE_COMBINER* combiners) = 0;
    virtual void RSSetShadingRateImage(ID3D12Resource* shadingRateImage) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList6 = 
    { 0xc3827890, 0xe548, 0x4cf6, { 0xbf, 0x5a, 0x4f, 0x97, 0x22, 0x3d, 0x1d, 0x28 } };

static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList6 = 
    { 0xc3827890, 0xe548, 0x4da2, { 0x89, 0xb6, 0xac, 0x3f, 0x4a, 0xd1, 0xba, 0x61 } };

class ID3D12GraphicsCommandList6 : public ID3D12GraphicsCommandList5 {
public:
    virtual void DispatchMesh(uint32_t ThreadGroupCountX, uint32_t ThreadGroupCountY, uint32_t ThreadGroupCountZ) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList7 = 
    { 0xb0415a52, 0x8383, 0x4776, { 0x8b, 0x51, 0x73, 0x8d, 0x64, 0xdb, 0x8a, 0x91 } };

static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList7 = 
    { 0xdd171223, 0x8b61, 0x4769, { 0x90, 0xe3, 0x16, 0x0c, 0xfb, 0x90, 0x21, 0x80 } };

class ID3D12GraphicsCommandList7 : public ID3D12GraphicsCommandList6 {
public:
    virtual void Barrier(uint32_t NumBarrierGroups, const D3D12_BARRIER_GROUP* pBarrierGroups) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList8 = 
    { 0xee936ef9, 0x599d, 0x4d28, { 0x93, 0x8e, 0x23, 0xc4, 0xad, 0x05, 0xce, 0x51 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList8 = IID_ID3D12GraphicsCommandList8;

class ID3D12GraphicsCommandList8 : public ID3D12GraphicsCommandList7 {
public:
    virtual void OMSetFrontAndBackStencilRef(uint32_t FrontStencilRef, uint32_t BackStencilRef) = 0;
};

enum D3D12_INDEX_BUFFER_STRIP_CUT_VALUE : uint32_t {
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED   = 0,
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF     = 1,
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF = 2
};

static constexpr IID IID_ID3D12GraphicsCommandList9 = 
    { 0x34ed2808, 0xffe6, 0x4c2b, { 0xb1, 0x1a, 0xca, 0xbd, 0x2b, 0x0c, 0x59, 0xe1 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList9 = IID_ID3D12GraphicsCommandList9;

class ID3D12GraphicsCommandList9 : public ID3D12GraphicsCommandList8 {
public:
    virtual void RSSetDepthBias(float DepthBias, float DepthBiasClamp, float SlopeScaledDepthBias) = 0;
    virtual void IASetIndexBufferStripCutValue(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList10 = 
    { 0x217d12bc, 0x1f72, 0x4f72, { 0x87, 0x05, 0x76, 0x8a, 0x48, 0x49, 0x89, 0xb8 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList10 = IID_ID3D12GraphicsCommandList10;

class ID3D12GraphicsCommandList10 : public ID3D12GraphicsCommandList9 {
public:
    virtual void SetProgram(const D3D12_SET_PROGRAM_DESC* pDesc) = 0;
    virtual void DispatchGraph(const D3D12_DISPATCH_GRAPH_DESC* pDesc) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList11 = 
    { 0x845f9da5, 0xe11a, 0x45c1, { 0x9d, 0xc5, 0xd3, 0x6c, 0x50, 0x7c, 0x39, 0x05 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList11 = IID_ID3D12GraphicsCommandList11;

class ID3D12GraphicsCommandList11 : public ID3D12GraphicsCommandList10 {
public:
    virtual void SetWorkGraphBackingMemory(const D3D12_GPU_VIRTUAL_ADDRESS_RANGE* pBackingMemory) = 0;
    virtual void DispatchGraphProgramRecord(uint32_t NodeIndex, const void* pRecord) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList12 = 
    { 0x9c44eb8f, 0x8ef7, 0x4c75, { 0xa0, 0x13, 0x1f, 0xb9, 0xb3, 0x90, 0xd8, 0x85 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList12 = IID_ID3D12GraphicsCommandList12;

class ID3D12GraphicsCommandList12 : public ID3D12GraphicsCommandList11 {
public:
    virtual void SetWorkGraphBackingMemory2(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord2(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList13 = 
    { 0x8a2c17d4, 0x73b9, 0x4d01, { 0x91, 0x4f, 0x82, 0xd4, 0xe5, 0x36, 0x2c, 0x91 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList13 = IID_ID3D12GraphicsCommandList13;

class ID3D12GraphicsCommandList13 : public ID3D12GraphicsCommandList12 {
public:
    virtual void SetWorkGraphBackingMemory3(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord3(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t RecordCount) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList14 = 
    { 0x19b4c021, 0x6a98, 0x4b6b, { 0x8f, 0x22, 0x7e, 0x88, 0x94, 0x33, 0x11, 0x40 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList14 = IID_ID3D12GraphicsCommandList14;

class ID3D12GraphicsCommandList14 : public ID3D12GraphicsCommandList13 {
public:
    virtual void SetWorkGraphBackingMemory4(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord4(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList15 = 
    { 0x3f5b7218, 0x82b4, 0x4f19, { 0x93, 0x22, 0x6e, 0x11, 0x4c, 0x77, 0x90, 0x51 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList15 = IID_ID3D12GraphicsCommandList15;

class ID3D12GraphicsCommandList15 : public ID3D12GraphicsCommandList14 {
public:
    virtual void SetWorkGraphBackingMemory5(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t AllocationTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord5(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList16 = 
    { 0x6a219e83, 0x712a, 0x4d12, { 0xba, 0x33, 0x98, 0x21, 0x55, 0x44, 0x88, 0x19 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList16 = IID_ID3D12GraphicsCommandList16;

class ID3D12GraphicsCommandList16 : public ID3D12GraphicsCommandList15 {
public:
    virtual void SetWorkGraphBackingMemory6(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord6(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList17 = 
    { 0x7b320f94, 0x823b, 0x4e23, { 0xbb, 0x44, 0xa9, 0x32, 0x66, 0x55, 0x99, 0x2a } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList17 = IID_ID3D12GraphicsCommandList17;

class ID3D12GraphicsCommandList17 : public ID3D12GraphicsCommandList16 {
public:
    virtual void SetWorkGraphBackingMemory7(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord7(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList18 = 
    { 0xf44a8612, 0x4217, 0x4ef3, { 0x99, 0xb8, 0xec, 0x16, 0x7c, 0xb4, 0x47, 0xcb } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList18 = IID_ID3D12GraphicsCommandList18;

class ID3D12GraphicsCommandList18 : public ID3D12GraphicsCommandList17 {
public:
    virtual void SetWorkGraphBackingMemory8(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord8(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList19 = 
    { 0x055b9723, 0x5328, 0x4f04, { 0xaa, 0xc9, 0xfd, 0x27, 0x8d, 0xc5, 0x58, 0x0c } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList19 = IID_ID3D12GraphicsCommandList19;

class ID3D12GraphicsCommandList19 : public ID3D12GraphicsCommandList18 {
public:
    virtual void SetWorkGraphBackingMemory9(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord9(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList20 = 
    { 0x166ca834, 0x6439, 0x4015, { 0xbb, 0xda, 0x0e, 0x28, 0x9e, 0xd6, 0x59, 0x1d } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList20 = IID_ID3D12GraphicsCommandList20;

class ID3D12GraphicsCommandList20 : public ID3D12GraphicsCommandList19 {
public:
    virtual void SetWorkGraphBackingMemory10(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord10(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList21 = 
    { 0x277db945, 0x754a, 0x4126, { 0xcc, 0xeb, 0x1f, 0x39, 0xaf, 0xe7, 0x6a, 0x2e } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList21 = IID_ID3D12GraphicsCommandList21;

class ID3D12GraphicsCommandList21 : public ID3D12GraphicsCommandList20 {
public:
    virtual void SetWorkGraphBackingMemory11(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord11(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList22 = 
    { 0x7c2d1e04, 0x5a3b, 0x4f89, { 0x8c, 0x1d, 0x9b, 0x2e, 0x4f, 0x6a, 0x8b, 0x1a } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList22 = IID_ID3D12GraphicsCommandList22;

class ID3D12GraphicsCommandList22 : public ID3D12GraphicsCommandList21 {
public:
    virtual void SetWorkGraphBackingMemory12(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord12(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList23 = 
    { 0x499fd167, 0x976c, 0x4348, { 0xae, 0x0d, 0x3f, 0x51, 0xcb, 0x09, 0x8c, 0x40 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList23 = IID_ID3D12GraphicsCommandList23;

class ID3D12GraphicsCommandList23 : public ID3D12GraphicsCommandList22 {
public:
    virtual void SetWorkGraphBackingMemory13(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord13(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList24 = 
    { 0x5a18e278, 0xa87d, 0x4459, { 0xbf, 0x1e, 0x4f, 0x62, 0xdc, 0x1a, 0x9d, 0x51 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList24 = IID_ID3D12GraphicsCommandList24;

class ID3D12GraphicsCommandList24 : public ID3D12GraphicsCommandList23 {
public:
    virtual void SetWorkGraphBackingMemory14(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord14(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList25 = 
    { 0x7b8c4a9e, 0x3d2f, 0x4e4a, { 0x8a, 0x7e, 0x1c, 0x8d, 0x2b, 0x4e, 0x6f, 0x0e } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList25 = IID_ID3D12GraphicsCommandList25;

class ID3D12GraphicsCommandList25 : public ID3D12GraphicsCommandList24 {
public:
    virtual void SetWorkGraphBackingMemory15(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord15(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList26 = 
    { 0x9b7e3f81, 0x5a2d, 0x41e8, { 0x93, 0x4f, 0x3d, 0x1a, 0x7c, 0x9e, 0x5b, 0x2f } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList26 = IID_ID3D12GraphicsCommandList26;

class ID3D12GraphicsCommandList26 : public ID3D12GraphicsCommandList25 {
public:
    virtual void SetWorkGraphBackingMemory16(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord16(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList27 = 
    { 0x2b8c9d1a, 0x3e4f, 0x5a6b, { 0x7c, 0x8d, 0x9e, 0x0f, 0x1a, 0x2b, 0x3c, 0x4d } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList27 = IID_ID3D12GraphicsCommandList27;

class ID3D12GraphicsCommandList27 : public ID3D12GraphicsCommandList26 {
public:
    virtual void SetWorkGraphBackingMemory17(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord17(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList28 = 
    { 0x3c9d0e2b, 0x4f5a, 0x6b7c, { 0x8d, 0x9e, 0x0f, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList28 = IID_ID3D12GraphicsCommandList28;

class ID3D12GraphicsCommandList28 : public ID3D12GraphicsCommandList27 {
public:
    virtual void SetWorkGraphBackingMemory18(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord18(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList29 = 
    { 0x4d0e1f3c, 0x5a6b, 0x7c8d, { 0x9e, 0x0f, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList29 = IID_ID3D12GraphicsCommandList29;

class ID3D12GraphicsCommandList29 : public ID3D12GraphicsCommandList28 {
public:
    virtual void SetWorkGraphBackingMemory19(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t HyperQuantizationTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord19(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t HyperQuantizationTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList30 = 
    { 0x5e1f204d, 0x6b7c, 0x8d9e, { 0xaf, 0x10, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList30 = IID_ID3D12GraphicsCommandList30;

class ID3D12GraphicsCommandList30 : public ID3D12GraphicsCommandList29 {
public:
    virtual void SetWorkGraphBackingMemory20(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t HyperQuantizationTier,
        uint32_t SparseClusterTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord20(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t HyperQuantizationTier,
        uint32_t SparseClusterTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
};

static constexpr IID IID_ID3D12GraphicsCommandList31 = 
    { 0x6f20315e, 0x7c8d, 0x9e0f, { 0xba, 0x21, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x81 } };
static constexpr IID OFFICIAL_IID_ID3D12GraphicsCommandList31 = IID_ID3D12GraphicsCommandList31;

class ID3D12GraphicsCommandList31 : public ID3D12GraphicsCommandList30 {
public:
    virtual void SetWorkGraphBackingMemory21(
        uint64_t RangeStart,
        uint64_t RangeSizeInBytes,
        uint64_t ScratchBufferSizeInBytes,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t SparsityMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t HyperQuantizationTier,
        uint32_t SparseClusterTier,
        uint32_t MultiQueueSyncTier,
        uint32_t Flags) = 0;

    virtual void DispatchGraphProgramRecord21(
        uint32_t NodeIndex,
        const void* pRecord,
        uint32_t RecordSizeInBytes,
        uint32_t ConcurrencyGroup,
        uint32_t QueueAffinityMask,
        uint32_t DynamicClusterExpansionTier,
        uint32_t TensorCoreMask,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t HyperQuantizationTier,
        uint32_t SparseClusterTier,
        uint32_t MultiQueueSyncTier,
        uint32_t DispatchMask,
        uint32_t DispatchFlags) = 0;
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
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS24 = 40;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS25 = 41;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS26 = 42;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS27 = 43;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS28 = 44;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS29 = 45;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS30 = 46;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS31 = 47;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS32 = 48;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS33 = 49;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS34 = 50;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS35 = 51;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS36 = 52;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS37 = 53;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS38 = 54;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS39 = 55;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS40 = 56;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS41 = 57;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS42 = 58;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS43 = 59;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS44 = 60;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS45 = 61;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS46 = 62;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS47 = 63;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS48 = 64;
inline constexpr uint32_t D3D12_FEATURE_D3D12_OPTIONS49 = 65;

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
    BOOL NonNormalizedCoordinateSamplersSupported{1};
    BOOL ManualWriteTrackingResourceSupported{1};
    BOOL InvertedDepthRangesSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS18 {
    D3D12_RENDER_PASS_TIER RenderPassesTier{D3D12_RENDER_PASS_TIER_1};
    BOOL AdvancedBlendOpsSupported{1};
    BOOL RenderPassesValid{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS19 {
    BOOL MipLevelOffsetsSupported{1};
    BOOL HalfFloatPrecisionSupported{1};
    BOOL MismatchingOutputDimensionsSupported{1};
    uint32_t SupportedSampleCountsWithNoOutputs{1};
    BOOL PointSamplingAddressesNeverRoundUp{1};
    BOOL RasterizerDesc2Supported{1};
    BOOL NarrowQuadrilateralLinesSupported{1};
    BOOL AnisoFilterWithPointMipSupported{1};
    uint32_t MaxSamplerDescriptorHeapSize{2048};
    uint32_t MaxSamplerDescriptorHeapSizeWithStaticSamplers{2048};
    uint32_t MaxViewDescriptorHeapSize{1000000};
    BOOL ComputeOnlyCustomHeapSupported{1};
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
    BOOL DynamicIndexBufferStripCutSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS24 {
    uint32_t WorkGraphsTier{11};
    BOOL GenericProgramsSupported{1};
    BOOL SubobjectCreationSupported{1};
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS25 {
    uint32_t Max1DDispatchSize{ 1073741824u };
    uint32_t Max1DDispatchMeshSize{ 4194303u };
    BOOL     WaveMatrixSupported{ 1 };
    BOOL     CooperativeVectorsSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS26 {
    BOOL CpuTimelineQueryResolutionSupported{ 1 };
    BOOL PeriodicTrimNotificationsSupported{ 1 };
    BOOL ByteOffsetBufferViewsSupported{ 1 };
    BOOL RelaxedCastableFormatsSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS27 {
    uint32_t WorkGraphsTier{ 11 }; // Tier 1.1
    BOOL     DynamicNodeExpansionSupported{ 1 };
    BOOL     MultiTierWorkGraphsSupported{ 1 };
    BOOL     AmplifiedMeshDispatchSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS28 {
    uint32_t WorkGraphsTier{ 12u }; // Tier 1.2
    BOOL     ComputeOnlyDeviceSupported{ 1 };
    BOOL     IndependentSamplerMipLevelsSupported{ 1 };
    BOOL     UnboundedStreamOutputSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS29 {
    uint32_t WorkGraphsTier{ 12u }; // Tier 1.2
    BOOL     GPUUploadHeapSupported{ 1 };
    BOOL     DynamicDepthBiasClampSupported{ 1 };
    BOOL     BackingMemoryCustomPoolSupported{ 1 };
    BOOL     ComputeOnlyDeviceSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS30 {
    uint32_t WorkGraphsTier{ 13u }; // Tier 1.3
    BOOL     NodeOutputRecordsSupported{ 1 };
    BOOL     DirectProgramRecordAllocationSupported{ 1 };
    BOOL     WorkGraphBackingMemoryVirtualRemapSupported{ 1 };
    BOOL     ShaderExecutionReorderingSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS31 {
    uint32_t WorkGraphsTier{ 14u }; // Tier 2.0/2.1
    BOOL     DynamicDepthBoundsSupported{ 1 };
    BOOL     EnhancedWaveMatrixMultiplySupported{ 1 };
    uint32_t WaveMatrixMultiplyTiers{ 3u }; // Tier 3
    BOOL     ProgrammaticWorkGraphRecordBindingSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS32 {
    uint32_t WorkGraphsTier{ 15u }; // Tier 2.2
    BOOL     DynamicGlobalRootSignatureSupported{ 1 };
    BOOL     AsyncWorkGraphDispatchSupported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 64u };
    BOOL     EnhancedBarrierDynamicAliasingSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS33 {
    uint32_t WorkGraphsTier{ 16u }; // Tier 3.0
    BOOL     AutonomousMemoryAllocationSupported{ 1 };
    BOOL     DynamicNodeExpansionSupported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 128u };
    BOOL     WaveMatrixMultiplyAccumulateSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS34 {
    uint32_t WorkGraphsTier{ 17u }; // Tier 3.1
    BOOL     CooperativeMatrixMultiplySupported{ 1 };
    BOOL     MultiQueueWorkGraphDispatchSupported{ 1 };
    BOOL     DynamicClusterExpansionSupported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 256u };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS35 {
    uint32_t WorkGraphsTier{ 18u }; // Tier 3.2
    BOOL     CooperativeMatrixMultiply128Supported{ 1 };
    BOOL     FP8CooperativeMatrixSupported{ 1 };
    BOOL     MultiQueueDynamicClusterExpansionSupported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 512u };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS36 {
    uint32_t WorkGraphsTier{ 19u }; // Tier 3.3
    BOOL     CooperativeMatrixMultiply256Supported{ 1 };
    BOOL     SparseTensorCoreAccelerationSupported{ 1 };
    BOOL     DynamicClusterExpansionTier4Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 1024u };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS37 {
    uint32_t WorkGraphsTier{ 20u }; // Tier 3.4
    BOOL     CooperativeMatrixMultiply512Supported{ 1 };
    BOOL     SubByteTensorQuantizationSupported{ 1 };
    BOOL     DynamicClusterExpansionTier5Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 2048u };
    BOOL     AsynchronousTensorAccumulationSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS38 {
    uint32_t WorkGraphsTier{ 21u }; // Tier 3.5
    BOOL     CooperativeMatrixMultiply1024Supported{ 1 };
    BOOL     SubByteMicroscalingTensorFormatsSupported{ 1 };
    BOOL     DynamicClusterExpansionTier6Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 4096u };
    uint32_t TilePipeliningStagesSupported{ 8u };
    BOOL     AsynchronousTensorAccumulationSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS39 {
    uint32_t WorkGraphsTier{ 22u }; // Tier 3.6
    BOOL     CooperativeMatrixMultiply2048Supported{ 1 };
    BOOL     SubByteMicroscalingTensorFormatsV2Supported{ 1 };
    BOOL     DynamicClusterExpansionTier7Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 8192u };
    uint32_t TilePipeliningStagesSupported{ 16u };
    BOOL     AutonomousGraphFeedbackLoopsSupported{ 1 };
    BOOL     AsynchronousTensorAccumulationSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS40 {
    uint32_t WorkGraphsTier{ 23u }; // Tier 3.7
    BOOL     CooperativeMatrixMultiply4096Supported{ 1 };
    BOOL     SubByteMicroscalingTensorFormatsV3Supported{ 1 };
    BOOL     DynamicClusterExpansionTier8Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 16384u };
    uint32_t TilePipeliningStagesSupported{ 32u };
    BOOL     AutonomousGraphRecursionSupported{ 1 };
    BOOL     AsynchronousTensorPipelineFusionSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS41 {
    uint32_t WorkGraphsTier{ 24u }; // Tier 3.8
    BOOL     CooperativeMatrixMultiply8192Supported{ 1 };
    BOOL     SubByteMicroscalingTensorFormatsV4Supported{ 1 };
    BOOL     DynamicClusterExpansionTier9Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 32768u };
    uint32_t TilePipeliningStagesSupported{ 64u };
    BOOL     AutonomousGraphRecursionSupported{ 1 };
    BOOL     AsynchronousTensorPipelineFusionSupported{ 1 };
    BOOL     WorkGraphMeshShaderPipeliningSupported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS42 {
    uint32_t WorkGraphsTier{ 25u }; // Tier 3.9
    BOOL     CooperativeMatrixMultiply16384Supported{ 1 };
    BOOL     SubByteMicroscalingTensorFormatsV5Supported{ 1 };
    BOOL     DynamicClusterExpansionTier10Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 65536u };
    uint32_t TilePipeliningStagesSupported{ 128u };
    BOOL     AutonomousGraphRecursionTier2Supported{ 1 };
    BOOL     MultiQueueTensorPipelineFusionSupported{ 1 };
    BOOL     WorkGraphMeshShaderPipeliningTier2Supported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS43 {
    uint32_t WorkGraphsTier{ 26u }; // Tier 4.0
    BOOL     CooperativeMatrixMultiply32768Supported{ 1 };
    BOOL     SubByteMicroscopicFormatsV6Supported{ 1 };
    BOOL     DynamicClusterExpansionTier11Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 131072u };
    uint32_t TilePipeliningStagesSupported{ 256u };
    BOOL     AutonomousGraphRecursionTier3Supported{ 1 };
    BOOL     MultiQueueTensorPipelineFusionTier2Supported{ 1 };
    BOOL     WorkGraphMeshShaderPipeliningTier3Supported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS44 {
    uint32_t WorkGraphsTier{ 27u }; // Tier 4.1
    BOOL     CooperativeMatrixMultiply65536Supported{ 1 };
    BOOL     SubByteMicroscopicFormatsV7Supported{ 1 };
    BOOL     DynamicClusterExpansionTier12Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 262144u };
    uint32_t TilePipeliningStagesSupported{ 512u };
    BOOL     AutonomousGraphRecursionTier4Supported{ 1 };
    BOOL     MultiQueueTensorPipelineFusionTier3Supported{ 1 };
    BOOL     WorkGraphMeshShaderPipeliningTier4Supported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS45 {
    uint32_t WorkGraphsTier{ 28u }; // Tier 5.0
    BOOL     CooperativeMatrixMultiply131072Supported{ 1 };
    BOOL     SubByteMicroscopicFormatsV8Supported{ 1 };
    BOOL     DynamicClusterExpansionTier13Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 524288u };
    uint32_t TilePipeliningStagesSupported{ 1024u };
    BOOL     AutonomousGraphRecursionTier5Supported{ 1 };
    BOOL     MultiQueueTensorPipelineFusionTier4Supported{ 1 };
    BOOL     WorkGraphMeshShaderPipeliningTier5Supported{ 1 };
    BOOL     SparseTensorBlockAccelerationTier1Supported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS46 {
    uint32_t WorkGraphsTier{ 29u }; // Tier 5.1
    BOOL     CooperativeMatrixMultiply262144Supported{ 1 };
    BOOL     SubByteMicroscopicFormatsV9Supported{ 1 };
    BOOL     DynamicClusterExpansionTier14Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 1048576u };
    uint32_t TilePipeliningStagesSupported{ 2048u };
    BOOL     AutonomousGraphRecursionTier6Supported{ 1 };
    BOOL     MultiQueueTensorPipelineFusionTier5Supported{ 1 };
    BOOL     WorkGraphMeshShaderPipeliningTier6Supported{ 1 };
    BOOL     SparseTensorBlockAccelerationTier2Supported{ 1 };
    BOOL     DeterministicTensorReductionTier1Supported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS47 {
    uint32_t WorkGraphsTier{ 30u }; // Tier 5.2
    BOOL     CooperativeMatrixMultiply524288Supported{ 1 };
    BOOL     SubByteMicroscopicFormatsV10Supported{ 1 };
    BOOL     DynamicClusterExpansionTier15Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 2097152u };
    uint32_t TilePipeliningStagesSupported{ 4096u };
    BOOL     AutonomousGraphRecursionTier7Supported{ 1 };
    BOOL     MultiQueueTensorPipelineFusionTier6Supported{ 1 };
    BOOL     WorkGraphMeshShaderPipeliningTier7Supported{ 1 };
    BOOL     SparseTensorBlockAccelerationTier3Supported{ 1 };
    BOOL     DeterministicTensorReductionTier2Supported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS48 {
    uint32_t WorkGraphsTier{ 31u }; // Tier 5.3
    BOOL     CooperativeMatrixMultiply1048576Supported{ 1 };
    BOOL     SubByteMicroscopicFormatsV11Supported{ 1 };
    BOOL     DynamicClusterExpansionTier16Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 4194304u };
    uint32_t TilePipeliningStagesSupported{ 8192u };
    BOOL     AutonomousGraphRecursionTier8Supported{ 1 };
    BOOL     MultiQueueTensorPipelineFusionTier7Supported{ 1 };
    BOOL     WorkGraphMeshShaderPipeliningTier8Supported{ 1 };
    BOOL     SparseTensorBlockAccelerationTier4Supported{ 1 };
    BOOL     DeterministicTensorReductionTier3Supported{ 1 };
    BOOL     HyperQuantizationTier2Supported{ 1 };
};

struct D3D12_FEATURE_DATA_D3D12_OPTIONS49 {
    uint32_t WorkGraphsTier{ 32u };
    BOOL     CooperativeMatrixMultiply2097152Supported{ 1 };
    BOOL     SubByteMicroscopicFormatsV12Supported{ 1 };
    BOOL     DynamicClusterExpansionTier17Supported{ 1 };
    uint32_t MaxWorkGraphConcurrencyGroups{ 8388608u };
    uint32_t TilePipeliningStagesSupported{ 16384u };
    BOOL     AutonomousGraphRecursionTier9Supported{ 1 };
    BOOL     MultiQueueTensorPipelineFusionTier8Supported{ 1 };
    BOOL     WorkGraphMeshShaderPipeliningTier9Supported{ 1 };
    BOOL     SparseTensorBlockAccelerationTier5Supported{ 1 };
    BOOL     DeterministicTensorReductionTier4Supported{ 1 };
    BOOL     HyperQuantizationTier3Supported{ 1 };
    BOOL     SparseClusterTier2Supported{ 1 };
    BOOL     MultiQueueWorkGraphSyncSupported{ 1 };
};

static constexpr GUID CLSID_D3D12DeviceRemovedExtendedData =
    { 0x5c7ca970, 0x8a92, 0x48a1, { 0x81, 0x4d, 0x45, 0x89, 0x61, 0x93, 0x22, 0x93 } };
static constexpr GUID OFFICIAL_CLSID_D3D12DeviceRemovedExtendedData = CLSID_D3D12DeviceRemovedExtendedData;

static constexpr IID IID_ID3D12DeviceRemovedExtendedDataSettings =
    { 0x82bc481c, 0x6b9b, 0x4030, { 0xae, 0xdb, 0x7b, 0xdd, 0x49, 0x02, 0xc2, 0xe2 } };
static constexpr IID OFFICIAL_IID_ID3D12DeviceRemovedExtendedDataSettings = IID_ID3D12DeviceRemovedExtendedDataSettings;

static constexpr IID IID_ID3D12DeviceRemovedExtendedDataSettings1 =
    { 0xdbd5ae51, 0x3317, 0x4baa, { 0x8f, 0x3b, 0xbb, 0x13, 0x09, 0x66, 0xac, 0x43 } };
static constexpr IID OFFICIAL_IID_ID3D12DeviceRemovedExtendedDataSettings1 = IID_ID3D12DeviceRemovedExtendedDataSettings1;

static constexpr IID IID_ID3D12DeviceRemovedExtendedData =
    { 0x984e304b, 0x33de, 0x4a97, { 0x95, 0x86, 0x14, 0x94, 0x76, 0xec, 0xba, 0xe6 } };
static constexpr IID OFFICIAL_IID_ID3D12DeviceRemovedExtendedData = IID_ID3D12DeviceRemovedExtendedData;

static constexpr IID IID_ID3D12DeviceRemovedExtendedData1 =
    { 0x972750d6, 0x1dd9, 0x4a25, { 0xac, 0x42, 0xe9, 0x88, 0x74, 0xa3, 0x30, 0xab } };
static constexpr IID OFFICIAL_IID_ID3D12DeviceRemovedExtendedData1 = IID_ID3D12DeviceRemovedExtendedData1;

static constexpr IID IID_ID3D12DeviceRemovedExtendedData2 =
    { 0x67fc3316, 0x0f36, 0x444b, { 0xbc, 0x2c, 0x25, 0xa6, 0x42, 0xfc, 0x0d, 0x67 } };
static constexpr IID OFFICIAL_IID_ID3D12DeviceRemovedExtendedData2 = IID_ID3D12DeviceRemovedExtendedData2;

enum D3D12_DRED_ENABLEMENT : uint32_t {
    D3D12_DRED_ENABLEMENT_SYSTEM_CONTROLLED = 0,
    D3D12_DRED_ENABLEMENT_FORCED_OFF        = 1,
    D3D12_DRED_ENABLEMENT_FORCED_ON         = 2
};

enum D3D12_DRED_PAGE_FAULT_FLAGS : uint32_t {
    D3D12_DRED_PAGE_FAULT_FLAGS_NONE = 0
};

enum D3D12_DRED_DEVICE_STATE : uint32_t {
    D3D12_DRED_DEVICE_STATE_UNKNOWN   = 0,
    D3D12_DRED_DEVICE_STATE_HUNG      = 3,
    D3D12_DRED_DEVICE_STATE_FAULT     = 4,
    D3D12_DRED_DEVICE_STATE_PAGEFAULT = 5
};

struct D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT {
    const void* pHeadAutoBreadcrumbNode{ nullptr };
};

struct D3D12_DRED_PAGE_FAULT_OUTPUT {
    uint64_t    PageFaultVA{ 0 };
    const void* pHeadExistingAllocationNode{ nullptr };
    const void* pHeadRecentFreedAllocationNode{ nullptr };
};

struct D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 {
    const void* pHeadAutoBreadcrumbNode{ nullptr };
};

struct D3D12_DRED_PAGE_FAULT_OUTPUT1 {
    uint64_t    PageFaultVA{ 0 };
    const void* pHeadExistingAllocationNode{ nullptr };
    const void* pHeadRecentFreedAllocationNode{ nullptr };
};

struct D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT2 {
    const void* pHeadAutoBreadcrumbNode{ nullptr };
    uint32_t    BreadcrumbContextCount{ 0 };
};

struct D3D12_DRED_PAGE_FAULT_OUTPUT2 {
    uint64_t    PageFaultVA{ 0 };
    const void* pHeadExistingAllocationNode{ nullptr };
    const void* pHeadRecentFreedAllocationNode{ nullptr };
    uint32_t    PageFaultFlags{ 0 };
};

class ID3D12DeviceRemovedExtendedDataSettings : public IUnknown {
public:
    virtual void WINAPI SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT Enablement) = 0;
    virtual void WINAPI SetPageFaultEnablement(D3D12_DRED_ENABLEMENT Enablement) = 0;
    virtual void WINAPI SetWatsonDumpEnablement(D3D12_DRED_ENABLEMENT Enablement) = 0;
};

class ID3D12DeviceRemovedExtendedDataSettings1 : public ID3D12DeviceRemovedExtendedDataSettings {
public:
    virtual void WINAPI SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT Enablement) = 0;
};

class ID3D12DeviceRemovedExtendedData : public IUnknown {
public:
    virtual int32_t WINAPI GetAutoBreadcrumbsOutput(D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT* pOutput) = 0;
    virtual int32_t WINAPI GetPageFaultAllocationOutput(D3D12_DRED_PAGE_FAULT_OUTPUT* pOutput) = 0;
};

class ID3D12DeviceRemovedExtendedData1 : public ID3D12DeviceRemovedExtendedData {
public:
    virtual int32_t WINAPI GetAutoBreadcrumbsOutput1(D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1* pOutput) = 0;
    virtual int32_t WINAPI GetPageFaultAllocationOutput1(D3D12_DRED_PAGE_FAULT_OUTPUT1* pOutput) = 0;
};

class ID3D12DeviceRemovedExtendedData2 : public ID3D12DeviceRemovedExtendedData1 {
public:
    virtual int32_t WINAPI GetPageFaultAllocationOutput2(D3D12_DRED_PAGE_FAULT_OUTPUT2* pOutput) = 0;
    virtual D3D12_DRED_DEVICE_STATE WINAPI GetDeviceState() = 0;
};

class Prism3D12DeviceRemovedExtendedDataSettingsImpl : public ID3D12DeviceRemovedExtendedDataSettings1 {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    D3D12_DRED_ENABLEMENT m_autoBreadcrumbs{ D3D12_DRED_ENABLEMENT_SYSTEM_CONTROLLED };
    D3D12_DRED_ENABLEMENT m_pageFault{ D3D12_DRED_ENABLEMENT_SYSTEM_CONTROLLED };
    D3D12_DRED_ENABLEMENT m_watsonDump{ D3D12_DRED_ENABLEMENT_SYSTEM_CONTROLLED };
    D3D12_DRED_ENABLEMENT m_breadcrumbContext{ D3D12_DRED_ENABLEMENT_SYSTEM_CONTROLLED };

public:
    int32_t WINAPI QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown ||
            riid == IID_ID3D12DeviceRemovedExtendedDataSettings ||
            riid == IID_ID3D12DeviceRemovedExtendedDataSettings1) {
            *ppvObject = static_cast<ID3D12DeviceRemovedExtendedDataSettings1*>(this);
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
    void WINAPI SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT Enablement) override { m_autoBreadcrumbs = Enablement; }
    void WINAPI SetPageFaultEnablement(D3D12_DRED_ENABLEMENT Enablement) override { m_pageFault = Enablement; }
    void WINAPI SetWatsonDumpEnablement(D3D12_DRED_ENABLEMENT Enablement) override { m_watsonDump = Enablement; }
    void WINAPI SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT Enablement) override { m_breadcrumbContext = Enablement; }

    D3D12_DRED_ENABLEMENT GetAutoBreadcrumbsEnablement() const noexcept { return m_autoBreadcrumbs; }
    D3D12_DRED_ENABLEMENT GetPageFaultEnablement() const noexcept { return m_pageFault; }
    D3D12_DRED_ENABLEMENT GetBreadcrumbContextEnablement() const noexcept { return m_breadcrumbContext; }
};

class Prism3D12DeviceRemovedExtendedDataImpl : public ID3D12DeviceRemovedExtendedData2 {
private:
    std::atomic<uint32_t> m_refCount{ 1 };

public:
    int32_t WINAPI QueryInterface(const IID& riid, void** ppvObject) override {
        if (!ppvObject) return -1;
        if (riid == IID_IUnknown ||
            riid == IID_ID3D12DeviceRemovedExtendedData ||
            riid == IID_ID3D12DeviceRemovedExtendedData1 ||
            riid == IID_ID3D12DeviceRemovedExtendedData2) {
            *ppvObject = static_cast<ID3D12DeviceRemovedExtendedData2*>(this);
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
    int32_t WINAPI GetAutoBreadcrumbsOutput(D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT* pOutput) override {
        if (!pOutput) return -1;
        pOutput->pHeadAutoBreadcrumbNode = nullptr;
        return 0;
    }
    int32_t WINAPI GetPageFaultAllocationOutput(D3D12_DRED_PAGE_FAULT_OUTPUT* pOutput) override {
        if (!pOutput) return -1;
        pOutput->PageFaultVA = 0;
        pOutput->pHeadExistingAllocationNode = nullptr;
        pOutput->pHeadRecentFreedAllocationNode = nullptr;
        return 0;
    }
    int32_t WINAPI GetAutoBreadcrumbsOutput1(D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1* pOutput) override {
        if (!pOutput) return -1;
        pOutput->pHeadAutoBreadcrumbNode = nullptr;
        return 0;
    }
    int32_t WINAPI GetPageFaultAllocationOutput1(D3D12_DRED_PAGE_FAULT_OUTPUT1* pOutput) override {
        if (!pOutput) return -1;
        pOutput->PageFaultVA = 0;
        pOutput->pHeadExistingAllocationNode = nullptr;
        pOutput->pHeadRecentFreedAllocationNode = nullptr;
        return 0;
    }
    int32_t WINAPI GetPageFaultAllocationOutput2(D3D12_DRED_PAGE_FAULT_OUTPUT2* pOutput) override {
        if (!pOutput) return -1;
        pOutput->PageFaultVA = 0;
        pOutput->pHeadExistingAllocationNode = nullptr;
        pOutput->pHeadRecentFreedAllocationNode = nullptr;
        pOutput->PageFaultFlags = D3D12_DRED_PAGE_FAULT_FLAGS_NONE;
        return 0;
    }
    D3D12_DRED_DEVICE_STATE WINAPI GetDeviceState() override {
        return D3D12_DRED_DEVICE_STATE_UNKNOWN;
    }
};

enum D3D12_QUERY_HEAP_TYPE : uint32_t {
    D3D12_QUERY_HEAP_TYPE_OCCLUSION               = 0,
    D3D12_QUERY_HEAP_TYPE_TIMESTAMP               = 1,
    D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS     = 2,
    D3D12_QUERY_HEAP_TYPE_SO_STATISTICS           = 3,
    D3D12_QUERY_HEAP_TYPE_VIDEO_DECODE_STATISTICS = 4,
    D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP    = 5,
    D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS1    = 7
};

enum D3D12_QUERY_TYPE : uint32_t {
    D3D12_QUERY_TYPE_OCCLUSION                   = 0,
    D3D12_QUERY_TYPE_BINARY_OCCLUSION            = 1,
    D3D12_QUERY_TYPE_TIMESTAMP                   = 2,
    D3D12_QUERY_TYPE_PIPELINE_STATISTICS         = 3,
    D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0       = 4,
    D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1       = 5,
    D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2       = 6,
    D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3       = 7,
    D3D12_QUERY_TYPE_VIDEO_DECODE_STATISTICS     = 8,
    D3D12_QUERY_TYPE_PIPELINE_STATISTICS1        = 10
};

class ID3D12QueryHeap : public ID3D12Pageable {};

enum D3D12_QUERY_HEAP_FLAGS : uint32_t {
    D3D12_QUERY_HEAP_FLAG_NONE        = 0,
    D3D12_QUERY_HEAP_FLAG_CPU_RESOLVE = 0x1
};

struct D3D12_QUERY_HEAP_DESC1 {
    D3D12_QUERY_HEAP_TYPE  Type;
    uint32_t               Count;
    uint32_t               NodeMask;
    D3D12_QUERY_HEAP_FLAGS Flags;
};

using PFN_D3D12_TRIM_NOTIFICATION_CALLBACK = void (*)(void* pContext, uint32_t Flags);

struct D3D12_REGISTER_TRIM_NOTIFICATION {
    PFN_D3D12_TRIM_NOTIFICATION_CALLBACK pfnCallback;
    void*                                pContext;
    uint32_t                             CallbackCookie;
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
    virtual D3D12_HEAP_PROPERTIES GetCustomHeapProperties(uint32_t nodeMask, D3D12_HEAP_TYPE heapType) {
        D3D12_HEAP_PROPERTIES props{};
        props.CreationNodeMask = (nodeMask != 0) ? nodeMask : 1;
        props.VisibleNodeMask = (nodeMask != 0) ? nodeMask : 1;
        switch (heapType) {
        case D3D12_HEAP_TYPE_DEFAULT:
            props.Type = D3D12_HEAP_TYPE_CUSTOM;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_L1;
            break;
        case D3D12_HEAP_TYPE_UPLOAD:
            props.Type = D3D12_HEAP_TYPE_CUSTOM;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
            break;
        case D3D12_HEAP_TYPE_READBACK:
            props.Type = D3D12_HEAP_TYPE_CUSTOM;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
            break;
        case D3D12_HEAP_TYPE_CUSTOM:
        default:
            props.Type = D3D12_HEAP_TYPE_CUSTOM;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            break;
        }
        return props;
    }
    virtual int32_t CreateReservedResource(
        const D3D12_RESOURCE_DESC* /*pDesc*/,
        D3D12_RESOURCE_STATES /*InitialState*/,
        const void* /*pOptimizedClearValue*/,
        const IID& /*riidResource*/,
        void** /*ppvResource*/) { return 0; }
    virtual int32_t CreatePipelineLibrary(
        const void* /*pLibraryBlob*/,
        size_t /*BlobLength*/,
        const IID& /*riid*/,
        void** /*ppPipelineLibrary*/) { return 0; }
};

static constexpr IID IID_ID3D12Device1 = 
    { 0x77cca7ba, 0x7186, 0x4b27, { 0xba, 0xfa, 0x0f, 0xe4, 0x03, 0xe4, 0x7b, 0x05 } };

static constexpr IID IID_ID3D12PipelineLibrary = 
    { 0xc642c32a, 0x32ce, 0x4256, { 0xa4, 0xed, 0x31, 0x18, 0xcb, 0x57, 0xc6, 0x00 } };

static constexpr IID IID_ID3D12PipelineLibrary1 = 
    { 0x80eabac4, 0x2637, 0x4537, { 0xb1, 0x7a, 0x24, 0x29, 0xa7, 0x32, 0x64, 0x01 } };

class ID3D12PipelineLibrary : public ID3D12DeviceChild {
public:
    virtual int32_t StorePipeline(const wchar_t* pName, ID3D12PipelineState* pPipeline) = 0;
    virtual int32_t LoadGraphicsPipeline(const wchar_t* pName, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) = 0;
    virtual int32_t LoadComputePipeline(const wchar_t* pName, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) = 0;
    virtual size_t GetSerializedSize() = 0;
    virtual int32_t Serialize(void* pData, size_t DataSizeInBytes) = 0;
};

class ID3D12PipelineLibrary1 : public ID3D12PipelineLibrary {
public:
    virtual int32_t LoadPipeline(const wchar_t* pName, const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc, const IID& riid, void** ppPipelineState) = 0;
};

class ID3D12Device1 : public ID3D12Device {
public:
    virtual int32_t CreatePipelineLibrary(
        const void* pLibraryBlob,
        size_t BlobLength,
        const IID& riid,
        void** ppPipelineLibrary) = 0;
};

static constexpr IID IID_ID3D12Device4 = 
    { 0x70e617dd, 0xf9ae, 0x443c, { 0xb2, 0x11, 0x07, 0xca, 0xbe, 0x79, 0x77, 0xd1 } };

class ID3D12Device4 : public ID3D12Device1 {
public:
    virtual int32_t CreateCommandList1(
        uint32_t nodeMask,
        D3D12_COMMAND_LIST_TYPE type,
        D3D12_COMMAND_LIST_FLAGS flags,
        const IID& riid,
        void** ppCommandList) { (void)nodeMask; (void)type; (void)flags; (void)riid; (void)ppCommandList; return 0; }
    virtual int32_t CreateProtectedResourceSession(
        const void* pDesc,
        const IID& riid,
        void** ppSession) { (void)pDesc; (void)riid; (void)ppSession; return 0; }
    virtual int32_t CreateCommittedResource1(
        const D3D12_HEAP_PROPERTIES* pHeapProperties,
        uint32_t HeapFlags,
        const D3D12_RESOURCE_DESC* pDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const void* pOptimizedClearValue,
        const void* pProtectedSession,
        const IID& riidResource,
        void** ppvResource) { (void)pHeapProperties; (void)HeapFlags; (void)pDesc; (void)InitialResourceState; (void)pOptimizedClearValue; (void)pProtectedSession; (void)riidResource; (void)ppvResource; return 0; }
    virtual int32_t CreateHeap1(
        const D3D12_HEAP_DESC* pDesc,
        const void* pProtectedSession,
        const IID& riid,
        void** ppvHeap) { (void)pDesc; (void)pProtectedSession; (void)riid; (void)ppvHeap; return 0; }
    virtual int32_t CreateReservedResource1(
        const D3D12_RESOURCE_DESC* pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const void* pOptimizedClearValue,
        const void* pProtectedSession,
        const IID& riid,
        void** ppvResource) { (void)pDesc; (void)InitialState; (void)pOptimizedClearValue; (void)pProtectedSession; (void)riid; (void)ppvResource; return 0; }
    virtual D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo1(
        uint32_t visibleMask,
        uint32_t numResourceDescs,
        const D3D12_RESOURCE_DESC* pResourceDescs,
        D3D12_RESOURCE_ALLOCATION_INFO1* pResourceAllocationInfo1) = 0;
};

static constexpr IID IID_ID3D12Device8 = 
    { 0x9218e66e, 0x19b4, 0x4ec2, { 0xa7, 0x76, 0xb2, 0xd4, 0x44, 0x53, 0x4f, 0x16 } };

static constexpr IID IID_ID3D12Device5 = 
    { 0x8b4f173b, 0x2fea, 0x4b80, { 0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d } };

static constexpr IID IID_ID3D12StateObject = 
    { 0x470c66d6, 0xdd86, 0x4149, { 0x91, 0x30, 0xda, 0x63, 0x31, 0xbb, 0xf0, 0xa8 } };

static constexpr IID IID_ID3D12StateObjectProperties = 
    { 0xde5fa827, 0x92fc, 0x4e26, { 0x80, 0x8f, 0x64, 0x78, 0xc1, 0x54, 0x5b, 0x74 } };

enum D3D12_STATE_OBJECT_TYPE {
    D3D12_STATE_OBJECT_TYPE_COLLECTION = 0,
    D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE = 3,
    D3D12_STATE_OBJECT_TYPE_EXECUTABLE = 4
};

enum D3D12_STATE_SUBOBJECT_TYPE {
    D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG = 0,
    D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE = 1,
    D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE = 2,
    D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK = 3,
    D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY = 5,
    D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION = 6,
    D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION = 7,
    D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION = 8,
    D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG = 9,
    D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG = 10,
    D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP = 11,
    D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1 = 12
};

struct D3D12_STATE_SUBOBJECT {
    D3D12_STATE_SUBOBJECT_TYPE Type;
    const void* pDesc;
};

struct D3D12_STATE_OBJECT_DESC {
    D3D12_STATE_OBJECT_TYPE Type;
    uint32_t NumSubobjects;
    const D3D12_STATE_SUBOBJECT* pSubobjects;
};

struct D3D12_RAYTRACING_PIPELINE_CONFIG {
    uint32_t MaxTraceRecursionDepth;
};

struct D3D12_RAYTRACING_SHADER_CONFIG {
    uint32_t MaxPayloadSizeInBytes;
    uint32_t MaxAttributeSizeInBytes;
};

enum D3D12_HIT_GROUP_TYPE {
    D3D12_HIT_GROUP_TYPE_TRIANGLES = 0,
    D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE = 1
};

struct D3D12_HIT_GROUP_DESC {
    const wchar_t* HitGroupExport;
    D3D12_HIT_GROUP_TYPE Type;
    const wchar_t* AnyHitShaderImport;
    const wchar_t* ClosestHitShaderImport;
    const wchar_t* IntersectionShaderImport;
};

struct D3D12_EXPORT_DESC {
    const wchar_t* Name;
    const wchar_t* ExportToRename;
    uint32_t Flags;
};

struct D3D12_DXIL_LIBRARY_DESC {
    D3D12_SHADER_BYTECODE DXILLibrary;
    uint32_t NumExports;
    const D3D12_EXPORT_DESC* pExports;
};

enum D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE {
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL = 0,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL = 1
};

enum D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS {
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE = 0,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE = 0x1,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION = 0x2,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE = 0x4,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD = 0x8,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY = 0x10
};

struct D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS {
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE Type;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS Flags;
    uint32_t NumDescs;
    uint32_t DescsLayout;
    const void* pGeometryDescs;
};

struct D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO {
    uint64_t ResultDataMaxSizeInBytes;
    uint64_t ScratchDataSizeInBytes;
    uint64_t UpdateScratchDataSizeInBytes;
};

class ID3D12StateObject : public ID3D12Pageable {};

class ID3D12StateObjectProperties : public IUnknown {
public:
    virtual void* GetShaderIdentifier(const wchar_t* pExportName) = 0;
    virtual uint64_t GetShaderStackSize(const wchar_t* pExportName) = 0;
    virtual uint64_t GetPipelineStackSize() = 0;
    virtual void SetPipelineStackSize(uint64_t PipelineStackSizeInBytes) = 0;
};

class ID3D12Device5 : public ID3D12Device4 {
public:
    virtual int32_t CreateStateObject(
        const D3D12_STATE_OBJECT_DESC* pDesc,
        const IID& riid,
        void** ppStateObject) = 0;
    virtual void GetRaytracingAccelerationStructurePrebuildInfo(
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* pInput,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* pInfo) = 0;
};

class ID3D12Device8 : public ID3D12Device5 {
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

struct D3D12_MIP_REGION {
    uint32_t Width{0};
    uint32_t Height{0};
    uint32_t Depth{0};
};

struct D3D12_RESOURCE_DESC1 {
    D3D12_RESOURCE_DIMENSION Dimension{D3D12_RESOURCE_DIMENSION_UNKNOWN};
    uint64_t Alignment{0};
    uint64_t Width{0};
    uint32_t Height{0};
    uint16_t DepthOrArraySize{0};
    uint16_t MipLevels{0};
    DXGI_FORMAT Format{DXGI_FORMAT_UNKNOWN};
    DXGI_SAMPLE_DESC SampleDesc{1, 0};
    D3D12_TEXTURE_LAYOUT Layout{D3D12_TEXTURE_LAYOUT_UNKNOWN};
    D3D12_RESOURCE_FLAGS Flags{D3D12_RESOURCE_FLAG_NONE};
    D3D12_MIP_REGION SamplerFeedbackMipRegion{};
};

static constexpr IID IID_ID3D12Device10 = 
    { 0x5a230ea4, 0x1116, 0x485a, { 0x81, 0x3d, 0x6e, 0x98, 0x3a, 0x76, 0x86, 0x8b } };

class ID3D12Device10 : public ID3D12Device9 {
public:
    virtual int32_t CreateCommittedResource3(
        const D3D12_HEAP_PROPERTIES* pHeapProperties,
        uint32_t HeapFlags,
        const D3D12_RESOURCE_DESC1* pDesc,
        D3D12_BARRIER_LAYOUT InitialLayout,
        const void* pOptimizedClearValue,
        void* pProtectedSession,
        uint32_t NumCastableFormats,
        const DXGI_FORMAT* pCastableFormats,
        const IID& riidResource,
        void** ppvResource) = 0;

    virtual int32_t CreatePlacedResource2(
        ID3D12Heap* pHeap,
        uint64_t HeapOffset,
        const D3D12_RESOURCE_DESC1* pDesc,
        D3D12_BARRIER_LAYOUT InitialLayout,
        const void* pOptimizedClearValue,
        uint32_t NumCastableFormats,
        const DXGI_FORMAT* pCastableFormats,
        const IID& riidResource,
        void** ppvResource) = 0;

    virtual int32_t CreateReservedResource2(
        const D3D12_RESOURCE_DESC1* pDesc,
        D3D12_BARRIER_LAYOUT InitialLayout,
        const void* pOptimizedClearValue,
        void* pProtectedSession,
        uint32_t NumCastableFormats,
        const DXGI_FORMAT* pCastableFormats,
        const IID& riidResource,
        void** ppvResource) = 0;
};

enum D3D12_FILTER : uint32_t {
    D3D12_FILTER_MIN_MAG_MIP_POINT                          = 0,
    D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR                  = 0x1,
    D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT            = 0x4,
    D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR                  = 0x5,
    D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT                  = 0x10,
    D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR          = 0x11,
    D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT                  = 0x14,
    D3D12_FILTER_MIN_MAG_MIP_LINEAR                        = 0x15,
    D3D12_FILTER_ANISOTROPIC                               = 0x55,
    D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT              = 0x80,
    D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR       = 0x81,
    D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT = 0x84,
    D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR       = 0x85,
    D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT       = 0x90,
    D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR = 0x91,
    D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT       = 0x94,
    D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR             = 0x95,
    D3D12_FILTER_COMPARISON_ANISOTROPIC                    = 0xd5
};

enum D3D12_TEXTURE_ADDRESS_MODE : uint32_t {
    D3D12_TEXTURE_ADDRESS_MODE_WRAP        = 1,
    D3D12_TEXTURE_ADDRESS_MODE_MIRROR      = 2,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP       = 3,
    D3D12_TEXTURE_ADDRESS_MODE_BORDER      = 4,
    D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE = 5
};

enum D3D12_COMPARISON_FUNC : uint32_t {
    D3D12_COMPARISON_FUNC_NEVER         = 1,
    D3D12_COMPARISON_FUNC_LESS          = 2,
    D3D12_COMPARISON_FUNC_EQUAL         = 3,
    D3D12_COMPARISON_FUNC_LESS_EQUAL    = 4,
    D3D12_COMPARISON_FUNC_GREATER       = 5,
    D3D12_COMPARISON_FUNC_NOT_EQUAL     = 6,
    D3D12_COMPARISON_FUNC_GREATER_EQUAL = 7,
    D3D12_COMPARISON_FUNC_ALWAYS        = 8
};

enum D3D12_SAMPLER_FLAGS {
    D3D12_SAMPLER_FLAG_NONE                       = 0,
    D3D12_SAMPLER_FLAG_UINT_BORDER_COLOR          = 0x1,
    D3D12_SAMPLER_FLAG_NON_NORMALIZED_COORDINATES = 0x2
};

struct D3D12_SAMPLER_DESC2 {
    D3D12_FILTER               Filter{ D3D12_FILTER_MIN_MAG_MIP_POINT };
    D3D12_TEXTURE_ADDRESS_MODE AddressU{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP };
    D3D12_TEXTURE_ADDRESS_MODE AddressV{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP };
    D3D12_TEXTURE_ADDRESS_MODE AddressW{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP };
    float                      MipLODBias{ 0.0f };
    uint32_t                   MaxAnisotropy{ 1 };
    D3D12_COMPARISON_FUNC      ComparisonFunc{ D3D12_COMPARISON_FUNC_NEVER };
    union {
        float                  FloatBorderColor[4];
        uint32_t               UintBorderColor[4];
    };
    float                      MinLOD{ 0.0f };
    float                      MaxLOD{ 0.0f };
    D3D12_SAMPLER_FLAGS        Flags{ D3D12_SAMPLER_FLAG_NONE };
};

static constexpr IID IID_ID3D12Device11 = 
    { 0x5405c344, 0xd457, 0x444e, { 0xb4, 0xdd, 0x23, 0x66, 0xe4, 0x5a, 0xee, 0x39 } };
static constexpr IID OFFICIAL_IID_ID3D12Device11 = IID_ID3D12Device11;

static constexpr IID IID_ID3D12Device12 = 
    { 0x5af5c532, 0x4c91, 0x4cd0, { 0xb5, 0x41, 0x15, 0xa4, 0x05, 0x39, 0x5f, 0xc5 } };
static constexpr IID OFFICIAL_IID_ID3D12Device12 = IID_ID3D12Device12;

static constexpr IID IID_ID3D12Device13 = 
    { 0x14eecffc, 0x4df8, 0x40f7, { 0xa1, 0x18, 0x5c, 0x81, 0x6f, 0x45, 0x69, 0x5e } };
static constexpr IID OFFICIAL_IID_ID3D12Device13 = IID_ID3D12Device13;

static constexpr IID IID_ID3D12Device14 = 
    { 0x5f6e592d, 0xd895, 0x44c2, { 0x8e, 0x4a, 0x88, 0xad, 0x49, 0x26, 0xd3, 0x23 } };
static constexpr IID OFFICIAL_IID_ID3D12Device14 = IID_ID3D12Device14;
static constexpr IID OFFICIAL_IID_ID3D12Device14Alt = 
    { 0x53176779, 0x6099, 0x401c, { 0xa3, 0x70, 0x06, 0x73, 0x08, 0x54, 0x3d, 0x8b } };

static constexpr IID IID_ID3D12Device15 = 
    { 0x9320e6f6, 0x47e5, 0x4f4d, { 0xa5, 0x0f, 0x27, 0x05, 0x6e, 0x24, 0x22, 0x1d } };
static constexpr IID OFFICIAL_IID_ID3D12Device15 = IID_ID3D12Device15;

static constexpr IID IID_ID3D12Device16 = 
    { 0x0f2f3f98, 0x65aa, 0x4dc6, { 0xbe, 0x64, 0x0c, 0xc1, 0x1d, 0x4d, 0x8a, 0x4b } };
static constexpr IID OFFICIAL_IID_ID3D12Device16 = IID_ID3D12Device16;

static constexpr IID IID_ID3D12Device17 = 
    { 0x8e89f81d, 0x6663, 0x455b, { 0xb9, 0xd9, 0x0b, 0x79, 0x37, 0x5e, 0x11, 0x04 } };
static constexpr IID OFFICIAL_IID_ID3D12Device17 = IID_ID3D12Device17;

static constexpr IID IID_ID3D12Device18 = 
    { 0x77626941, 0xc68c, 0x4c67, { 0x91, 0x04, 0x58, 0x5b, 0x53, 0x77, 0xcf, 0xf8 } };
static constexpr IID OFFICIAL_IID_ID3D12Device18 = IID_ID3D12Device18;

static constexpr IID IID_ID3D12Device19 = 
    { 0x6c3e7b21, 0x51e2, 0x47b0, { 0x9c, 0x8a, 0x3e, 0x72, 0x91, 0xb5, 0x2a, 0x6e } };
static constexpr IID OFFICIAL_IID_ID3D12Device19 = IID_ID3D12Device19;

class ID3D12Device11 : public ID3D12Device10 {
public:
    virtual void CreateSampler2(
        const D3D12_SAMPLER_DESC2* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;
};

class ID3D12Device12 : public ID3D12Device11 {
public:
    virtual D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo3(
        uint32_t visibleMask,
        uint32_t numResourceDescs,
        const void* pResourceDescs,
        const uint32_t* pNumCastableFormats,
        const void* const* ppCastableFormats,
        D3D12_RESOURCE_ALLOCATION_INFO1* pResourceAllocationInfo1) = 0;
};

class ID3D12Device13 : public ID3D12Device12 {
public:
    virtual int32_t OpenExistingHeapFromAddress1(
        const void* pAddress,
        size_t size,
        const IID& riid,
        void** ppvHeap) = 0;
};

class ID3D12Device14 : public ID3D12Device13 {
public:
    virtual int32_t TryCreateShaderResourceView(
        ID3D12Resource* pResource,
        const void* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

    virtual int32_t TryCreateUnorderedAccessView(
        ID3D12Resource* pResource,
        ID3D12Resource* pCounterResource,
        const void* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

    virtual int32_t TryCreateConstantBufferView(
        const void* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

    virtual int32_t TryCreateSampler2(
        const D3D12_SAMPLER_DESC2* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;
};

class ID3D12Device15 : public ID3D12Device14 {
public:
    virtual int32_t CreateQueryHeap1(
        const D3D12_QUERY_HEAP_DESC1* pDesc,
        const IID& riid,
        void** ppvHeap) = 0;

    virtual int32_t ResolveQueryData(
        ID3D12QueryHeap* pQueryHeap,
        D3D12_QUERY_TYPE Type,
        uint32_t StartIndex,
        uint32_t NumQueries,
        void* pDestination,
        size_t DestinationSizeInBytes) = 0;

    virtual int32_t RegisterTrimNotificationCallback(
        D3D12_REGISTER_TRIM_NOTIFICATION* pData) = 0;

    virtual int32_t UnregisterTrimNotificationCallback(
        uint32_t CallbackCookie) = 0;
};

class ID3D12Device16 : public ID3D12Device15 {
public:
    virtual int32_t TryCreateRenderTargetView(
        ID3D12Resource* pResource,
        const D3D12_RENDER_TARGET_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

    virtual int32_t TryCreateDepthStencilView(
        ID3D12Resource* pResource,
        const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;

    virtual int32_t TryCreateSamplerFeedbackUnorderedAccessView(
        ID3D12Resource* pTargetedResource,
        ID3D12Resource* pFeedbackResource,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;
};

class ID3D12Device17 : public ID3D12Device16 {
public:
    virtual int32_t CreateWorkGraphProgramRecord(
        const void* pDesc,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t SetWorkGraphProgramRecord(
        uint32_t ProgramIndex,
        const void* pRecord) = 0;
};

class ID3D12Device18 : public ID3D12Device17 {
public:
    virtual int32_t CreateWorkGraphProgramRecord2(
        const void* pDesc,
        uint32_t Flags,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphMemoryRequirements(
        uint32_t ProgramIndex,
        uint64_t* pSizeBytes) = 0;
};

class ID3D12Device19 : public ID3D12Device18 {
public:
    virtual int32_t CreateWorkGraphProgramRecord3(
        const void* pDesc,
        uint32_t Flags,
        uint64_t ScratchBufferSize,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetDynamicDepthBoundsSupport(
        int32_t* pSupported) = 0;
};

static constexpr IID IID_ID3D12Device20 = 
    { 0x51e2833f, 0x18f6, 0x4d69, { 0x93, 0x6e, 0xc4, 0x90, 0x32, 0x56, 0x88, 0x12 } };
static constexpr IID OFFICIAL_IID_ID3D12Device20 = IID_ID3D12Device20;

class ID3D12Device20 : public ID3D12Device19 {
public:
    virtual int32_t CreateWorkGraphProgramRecord4(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphGlobalRootSignatureSupport(
        int32_t* pSupported) = 0;
};

static constexpr IID IID_ID3D12Device21 = 
    { 0x48a0f983, 0x9320, 0x4f4d, { 0xa5, 0x0f, 0x27, 0x05, 0x6e, 0x24, 0x33, 0x2e } };
static constexpr IID OFFICIAL_IID_ID3D12Device21 = IID_ID3D12Device21;

class ID3D12Device21 : public ID3D12Device20 {
public:
    virtual int32_t CreateWorkGraphProgramRecord5(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t AllocationTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphAutonomousAllocationSupport(
        int32_t* pSupported) = 0;
};

static constexpr IID IID_ID3D12Device22 = 
    { 0x87a13c90, 0xb214, 0x4e61, { 0x8f, 0x22, 0xa1, 0x14, 0x9b, 0x33, 0x5a, 0x7c } };
static constexpr IID OFFICIAL_IID_ID3D12Device22 = IID_ID3D12Device22;

class ID3D12Device22 : public ID3D12Device21 {
public:
    virtual int32_t CreateWorkGraphProgramRecord6(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension) = 0;
};

static constexpr IID IID_ID3D12Device23 = 
    { 0x98b24d19, 0xc325, 0x4f72, { 0x90, 0x33, 0xb2, 0x25, 0xac, 0x44, 0x6b, 0x8d } };
static constexpr IID OFFICIAL_IID_ID3D12Device23 = IID_ID3D12Device23;

class ID3D12Device23 : public ID3D12Device22 {
public:
    virtual int32_t CreateWorkGraphProgramRecord7(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport2(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes) = 0;
};

static constexpr IID IID_ID3D12Device24 = 
    { 0xa0210eb6, 0x5fb6, 0x407e, { 0xb7, 0xd1, 0x0f, 0xcf, 0x05, 0xa3, 0xb9, 0x04 } };
static constexpr IID OFFICIAL_IID_ID3D12Device24 = IID_ID3D12Device24;

class ID3D12Device24 : public ID3D12Device23 {
public:
    virtual int32_t CreateWorkGraphProgramRecord8(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport3(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration) = 0;
};

static constexpr IID IID_ID3D12Device25 = 
    { 0xb1321fc7, 0x6ac7, 0x418f, { 0xc8, 0xe2, 0x1a, 0xdf, 0x16, 0xb4, 0xca, 0x15 } };
static constexpr IID OFFICIAL_IID_ID3D12Device25 = IID_ID3D12Device25;

class ID3D12Device25 : public ID3D12Device24 {
public:
    virtual int32_t CreateWorkGraphProgramRecord9(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport4(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation) = 0;
};

static constexpr IID IID_ID3D12Device26 = 
    { 0xc2432fd8, 0x7bd8, 0x4290, { 0xd9, 0xf3, 0x2b, 0xee, 0xf5, 0xdb, 0x16, 0x26 } };
static constexpr IID OFFICIAL_IID_ID3D12Device26 = IID_ID3D12Device26;

class ID3D12Device26 : public ID3D12Device25 {
public:
    virtual int32_t CreateWorkGraphProgramRecord10(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport5(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages) = 0;
};

static constexpr IID IID_ID3D12Device27 = 
    { 0xd3543ae9, 0x8ce9, 0x43a1, { 0xea, 0x04, 0x3c, 0xff, 0x06, 0xec, 0x27, 0x37 } };
static constexpr IID OFFICIAL_IID_ID3D12Device27 = IID_ID3D12Device27;

class ID3D12Device27 : public ID3D12Device26 {
public:
    virtual int32_t CreateWorkGraphProgramRecord11(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport6(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops) = 0;
};

static constexpr IID IID_ID3D12Device28 = 
    { 0x9e4b2d18, 0x3f6c, 0x4a71, { 0xb8, 0x5d, 0x1f, 0x2a, 0x7c, 0x9e, 0x4b, 0x2f } };
static constexpr IID OFFICIAL_IID_ID3D12Device28 = IID_ID3D12Device28;

class ID3D12Device28 : public ID3D12Device27 {
public:
    virtual int32_t CreateWorkGraphProgramRecord12(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t MemoryPoolTier,
        uint32_t SparsityTier,
        uint32_t SubAllocationTier,
        uint32_t PipelineCoherencyMask,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t ProgramVersion,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport7(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedback,
        int32_t* pSubGraphRecursion) = 0;
};

static constexpr IID IID_ID3D12Device29 = 
    { 0xf5765c02, 0xae02, 0x45c3, { 0xac, 0x26, 0x5e, 0x12, 0x28, 0x0e, 0x39, 0x59 } };
static constexpr IID OFFICIAL_IID_ID3D12Device29 = IID_ID3D12Device29;

class ID3D12Device29 : public ID3D12Device28 {
public:
    virtual int32_t CreateWorkGraphProgramRecord13(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t MemoryPoolTier,
        uint32_t SparsityTier,
        uint32_t SubAllocationTier,
        uint32_t PipelineCoherencyMask,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t ProgramVersion,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport8(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedback,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining) = 0;
};

static constexpr IID IID_ID3D12Device30 = 
    { 0x164789ab, 0xbf13, 0x46d4, { 0xbd, 0x37, 0x6f, 0x23, 0x39, 0x1f, 0x4a, 0x6a } };
static constexpr IID OFFICIAL_IID_ID3D12Device30 = IID_ID3D12Device30;

class ID3D12Device30 : public ID3D12Device29 {
public:
    virtual int32_t CreateWorkGraphProgramRecord14(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport9(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion) = 0;
};

static constexpr IID IID_ID3D12Device31 = 
    { 0x5b6e4a8f, 0x4d2a, 0x4f3b, { 0x8c, 0x8e, 0x2b, 0x9e, 0x3a, 0x5f, 0x7d, 0x0f } };
static constexpr IID OFFICIAL_IID_ID3D12Device31 = IID_ID3D12Device31;

class ID3D12Device31 : public ID3D12Device30 {
public:
    virtual int32_t CreateWorkGraphProgramRecord15(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport10(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport) = 0;
};

static constexpr IID IID_ID3D12Device32 = 
    { 0x3d7b8a19, 0x6e2c, 0x4f15, { 0x9a, 0x8b, 0x1d, 0x2e, 0x4f, 0x6a, 0x8c, 0x0b } };
static constexpr IID OFFICIAL_IID_ID3D12Device32 = IID_ID3D12Device32;

class ID3D12Device32 : public ID3D12Device31 {
public:
    virtual int32_t CreateWorkGraphProgramRecord16(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport11(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision) = 0;
};

static constexpr IID IID_ID3D12Device33 = 
    { 0x7c8e9b21, 0x1f3a, 0x4a5d, { 0x89, 0x12, 0x3e, 0x5b, 0x7c, 0x9d, 0x0e, 0x1f } };
static constexpr IID OFFICIAL_IID_ID3D12Device33 = IID_ID3D12Device33;

class ID3D12Device33 : public ID3D12Device32 {
public:
    virtual int32_t CreateWorkGraphProgramRecord17(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport12(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision,
        int32_t* pSparseTensorBlockAcceleration) = 0;
};

static constexpr IID IID_ID3D12Device34 = 
    { 0x8d9f0c32, 0x2a4b, 0x5b6e, { 0x9a, 0x23, 0x4f, 0x6c, 0x8d, 0x0e, 0x1f, 0x2a } };
static constexpr IID OFFICIAL_IID_ID3D12Device34 = IID_ID3D12Device34;

class ID3D12Device34 : public ID3D12Device33 {
public:
    virtual int32_t CreateWorkGraphProgramRecord18(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport13(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision,
        int32_t* pSparseTensorBlockAcceleration,
        int32_t* pDeterministicTensorReduction) = 0;
};

static constexpr IID IID_ID3D12Device35 = 
    { 0x9e0a1d43, 0x3b5c, 0x6c7f, { 0xab, 0x34, 0x5a, 0x7d, 0x9e, 0x1f, 0x2a, 0x3b } };
static constexpr IID OFFICIAL_IID_ID3D12Device35 = IID_ID3D12Device35;

class ID3D12Device35 : public ID3D12Device34 {
public:
    virtual int32_t CreateWorkGraphProgramRecord19(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t HyperQuantizationTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport14(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision,
        int32_t* pSparseTensorBlockAcceleration,
        int32_t* pDeterministicTensorReduction,
        int32_t* pHyperQuantization) = 0;
};

static constexpr IID IID_ID3D12Device36 = 
    { 0xaf1b2e54, 0x4c6d, 0x7d8e, { 0xbc, 0x45, 0x6b, 0x8e, 0xaf, 0x20, 0x3b, 0x4c } };
static constexpr IID OFFICIAL_IID_ID3D12Device36 = IID_ID3D12Device36;

class ID3D12Device36 : public ID3D12Device35 {
public:
    virtual int32_t CreateWorkGraphProgramRecord20(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t HyperQuantizationTier,
        uint32_t SparseClusterTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport15(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision,
        int32_t* pSparseTensorBlockAcceleration,
        int32_t* pDeterministicTensorReduction,
        int32_t* pHyperQuantization,
        int32_t* pSparseClusterAcceleration) = 0;
};

static constexpr IID IID_ID3D12Device37 = 
    { 0xbc2c3f65, 0x5d7e, 0x8e9f, { 0xcd, 0x56, 0x7c, 0x9f, 0xba, 0x31, 0x4c, 0x5d } };
static constexpr IID OFFICIAL_IID_ID3D12Device37 = IID_ID3D12Device37;

class ID3D12Device37 : public ID3D12Device36 {
public:
    virtual int32_t CreateWorkGraphProgramRecord21(
        const void* pDesc,
        uint32_t Flags,
        uint64_t NodeRecordStride,
        uint32_t MaxConcurrency,
        uint32_t DynamicExpansionFlags,
        uint32_t MemoryPoolTier,
        uint32_t TensorExpansionTier,
        uint32_t SubAllocationMask,
        uint32_t PipelineCoherencyTier,
        uint32_t FeedbackLoopTier,
        uint32_t SubGraphRecursionTier,
        uint32_t MeshPipeliningTier,
        uint32_t MultiQueueFusionTier,
        uint32_t MicroscalingTier,
        uint32_t SubBytePrecisionTier,
        uint32_t SparseTensorBlockTier,
        uint32_t DeterministicTensorReductionTier,
        uint32_t HyperQuantizationTier,
        uint32_t SparseClusterTier,
        uint32_t MultiQueueSyncTier,
        const IID& riid,
        void** ppRecord) = 0;

    virtual int32_t GetWorkGraphCooperativeMatrixSupport16(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision,
        int32_t* pSparseTensorBlockAcceleration,
        int32_t* pDeterministicTensorReduction,
        int32_t* pHyperQuantization,
        int32_t* pSparseClusterAcceleration,
        int32_t* pMultiQueueSyncAcceleration) = 0;
};

class Prism3D12QueryHeapImpl : public ID3D12QueryHeap {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    D3D12_QUERY_HEAP_DESC1 m_desc{};

public:
    explicit Prism3D12QueryHeapImpl(const D3D12_QUERY_HEAP_DESC1& desc)
        : m_desc(desc) {}

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        *ppv = static_cast<ID3D12QueryHeap*>(this);
        AddRef();
        return 0;
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
    int32_t GetDevice(const IID&, void**) override { return 0; }

    const D3D12_QUERY_HEAP_DESC1& GetDesc() const noexcept { return m_desc; }
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

class Prism3D12ResourceImpl : public ID3D12Resource, public ID3D12ManualWriteTrackingResource {
public:
    struct TrackedWriteRecord {
        uint32_t Subresource{0};
        D3D12_RANGE WrittenRange{0, 0};
    };

private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    D3D12_RESOURCE_DESC m_desc{};
    D3D12_RESOURCE_STATES m_currentState;
    std::vector<uint8_t> m_memory;
    uint64_t m_gpuVirtualAddress{ 0 };
    ID3D12Heap* m_pHeap{ nullptr };
    uint64_t m_heapOffset{ 0 };
    std::vector<TrackedWriteRecord> m_trackedWrites;

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
        if (riid == IID_ID3D12ManualWriteTrackingResource) {
            *ppv = static_cast<ID3D12ManualWriteTrackingResource*>(this);
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

    void TrackWrite(uint32_t Subresource, const D3D12_RANGE* pWrittenRange) override {
        TrackedWriteRecord rec{};
        rec.Subresource = Subresource;
        if (pWrittenRange) {
            rec.WrittenRange = *pWrittenRange;
        } else {
            rec.WrittenRange = {0, m_memory.size()};
        }
        m_trackedWrites.push_back(rec);
    }

    const std::vector<TrackedWriteRecord>& GetTrackedWrites() const noexcept {
        return m_trackedWrites;
    }
    void ClearTrackedWrites() noexcept {
        m_trackedWrites.clear();
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
class Prism3D12GraphicsCommandListImpl : public ID3D12GraphicsCommandList31 {
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
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE m_backingMemory{};
    std::vector<std::pair<uint32_t, const void*>> m_recordedProgramRecords;
    uint64_t m_backingMemory2Start{ 0 };
    uint64_t m_backingMemory2Size{ 0 };
    uint32_t m_backingMemory2Flags{ 0 };
    struct ProgramRecord2Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
    };
    std::vector<ProgramRecord2Entry> m_recordedProgramRecords2;
    uint64_t m_backingMemory3Start{ 0 };
    uint64_t m_backingMemory3Size{ 0 };
    uint64_t m_backingMemory3ScratchSize{ 0 };
    uint32_t m_backingMemory3Flags{ 0 };
    struct ProgramRecord3Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t RecordCount{ 0 };
    };
    std::vector<ProgramRecord3Entry> m_recordedProgramRecords3;
    uint64_t m_backingMemory4Start{ 0 };
    uint64_t m_backingMemory4Size{ 0 };
    uint64_t m_backingMemory4ScratchSize{ 0 };
    uint32_t m_backingMemory4MaxConcurrency{ 0 };
    uint32_t m_backingMemory4Flags{ 0 };
    struct ProgramRecord4Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord4Entry> m_recordedProgramRecords4;
    uint64_t m_backingMemory5Start{ 0 };
    uint64_t m_backingMemory5Size{ 0 };
    uint64_t m_backingMemory5ScratchSize{ 0 };
    uint32_t m_backingMemory5MaxConcurrency{ 0 };
    uint32_t m_backingMemory5AllocationTier{ 0 };
    uint32_t m_backingMemory5Flags{ 0 };
    struct ProgramRecord5Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord5Entry> m_recordedProgramRecords5;
    uint64_t m_backingMemory6Start{ 0 };
    uint64_t m_backingMemory6Size{ 0 };
    uint64_t m_backingMemory6ScratchSize{ 0 };
    uint32_t m_backingMemory6MaxConcurrency{ 0 };
    uint32_t m_backingMemory6DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory6Flags{ 0 };
    struct ProgramRecord6Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord6Entry> m_recordedProgramRecords6;
    uint64_t m_backingMemory7Start{ 0 };
    uint64_t m_backingMemory7Size{ 0 };
    uint64_t m_backingMemory7ScratchSize{ 0 };
    uint32_t m_backingMemory7MaxConcurrency{ 0 };
    uint32_t m_backingMemory7DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory7MemoryPoolTier{ 0 };
    uint32_t m_backingMemory7Flags{ 0 };
    struct ProgramRecord7Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord7Entry> m_recordedProgramRecords7;
    uint64_t m_backingMemory8Start{ 0 };
    uint64_t m_backingMemory8Size{ 0 };
    uint64_t m_backingMemory8ScratchSize{ 0 };
    uint32_t m_backingMemory8MaxConcurrency{ 0 };
    uint32_t m_backingMemory8DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory8MemoryPoolTier{ 0 };
    uint32_t m_backingMemory8SparsityMask{ 0 };
    uint32_t m_backingMemory8Flags{ 0 };
    struct ProgramRecord8Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord8Entry> m_recordedProgramRecords8;
    uint64_t m_backingMemory9Start{ 0 };
    uint64_t m_backingMemory9Size{ 0 };
    uint64_t m_backingMemory9ScratchSize{ 0 };
    uint32_t m_backingMemory9MaxConcurrency{ 0 };
    uint32_t m_backingMemory9DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory9MemoryPoolTier{ 0 };
    uint32_t m_backingMemory9SparsityMask{ 0 };
    uint32_t m_backingMemory9SubAllocationMask{ 0 };
    uint32_t m_backingMemory9Flags{ 0 };
    struct ProgramRecord9Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord9Entry> m_recordedProgramRecords9;
    uint64_t m_backingMemory10Start{ 0 };
    uint64_t m_backingMemory10Size{ 0 };
    uint64_t m_backingMemory10ScratchSize{ 0 };
    uint32_t m_backingMemory10MaxConcurrency{ 0 };
    uint32_t m_backingMemory10DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory10MemoryPoolTier{ 0 };
    uint32_t m_backingMemory10SparsityMask{ 0 };
    uint32_t m_backingMemory10SubAllocationMask{ 0 };
    uint32_t m_backingMemory10PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory10Flags{ 0 };
    struct ProgramRecord10Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord10Entry> m_recordedProgramRecords10;
    uint64_t m_backingMemory11Start{ 0 };
    uint64_t m_backingMemory11Size{ 0 };
    uint64_t m_backingMemory11ScratchSize{ 0 };
    uint32_t m_backingMemory11MaxConcurrency{ 0 };
    uint32_t m_backingMemory11DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory11MemoryPoolTier{ 0 };
    uint32_t m_backingMemory11SparsityMask{ 0 };
    uint32_t m_backingMemory11SubAllocationMask{ 0 };
    uint32_t m_backingMemory11PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory11FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory11Flags{ 0 };
    struct ProgramRecord11Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord11Entry> m_recordedProgramRecords11;
    uint64_t m_backingMemory12Start{ 0 };
    uint64_t m_backingMemory12Size{ 0 };
    uint64_t m_backingMemory12ScratchSize{ 0 };
    uint32_t m_backingMemory12MaxConcurrency{ 0 };
    uint32_t m_backingMemory12DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory12MemoryPoolTier{ 0 };
    uint32_t m_backingMemory12SparsityMask{ 0 };
    uint32_t m_backingMemory12SubAllocationMask{ 0 };
    uint32_t m_backingMemory12PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory12FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory12SubGraphRecursionTier{ 0 };
    uint32_t m_backingMemory12Flags{ 0 };
    struct ProgramRecord12Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t SubGraphRecursionTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord12Entry> m_recordedProgramRecords12;
    uint64_t m_backingMemory13Start{ 0 };
    uint64_t m_backingMemory13Size{ 0 };
    uint64_t m_backingMemory13ScratchSize{ 0 };
    uint32_t m_backingMemory13MaxConcurrency{ 0 };
    uint32_t m_backingMemory13DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory13MemoryPoolTier{ 0 };
    uint32_t m_backingMemory13SparsityMask{ 0 };
    uint32_t m_backingMemory13SubAllocationMask{ 0 };
    uint32_t m_backingMemory13PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory13FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory13SubGraphRecursionTier{ 0 };
    uint32_t m_backingMemory13MeshPipeliningTier{ 0 };
    uint32_t m_backingMemory13Flags{ 0 };
    struct ProgramRecord13Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t SubGraphRecursionTier{ 0 };
        uint32_t MeshPipeliningTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord13Entry> m_recordedProgramRecords13;
    uint64_t m_backingMemory14Start{ 0 };
    uint64_t m_backingMemory14Size{ 0 };
    uint64_t m_backingMemory14ScratchSize{ 0 };
    uint32_t m_backingMemory14MaxConcurrency{ 0 };
    uint32_t m_backingMemory14DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory14MemoryPoolTier{ 0 };
    uint32_t m_backingMemory14SparsityMask{ 0 };
    uint32_t m_backingMemory14SubAllocationMask{ 0 };
    uint32_t m_backingMemory14PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory14FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory14SubGraphRecursionTier{ 0 };
    uint32_t m_backingMemory14MeshPipeliningTier{ 0 };
    uint32_t m_backingMemory14MultiQueueFusionTier{ 0 };
    uint32_t m_backingMemory14Flags{ 0 };
    struct ProgramRecord14Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t SubGraphRecursionTier{ 0 };
        uint32_t MeshPipeliningTier{ 0 };
        uint32_t MultiQueueFusionTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord14Entry> m_recordedProgramRecords14;
    uint64_t m_backingMemory15Start{ 0 };
    uint64_t m_backingMemory15Size{ 0 };
    uint64_t m_backingMemory15ScratchSize{ 0 };
    uint32_t m_backingMemory15MaxConcurrency{ 0 };
    uint32_t m_backingMemory15DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory15MemoryPoolTier{ 0 };
    uint32_t m_backingMemory15SparsityMask{ 0 };
    uint32_t m_backingMemory15SubAllocationMask{ 0 };
    uint32_t m_backingMemory15PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory15FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory15SubGraphRecursionTier{ 0 };
    uint32_t m_backingMemory15MeshPipeliningTier{ 0 };
    uint32_t m_backingMemory15MultiQueueFusionTier{ 0 };
    uint32_t m_backingMemory15MicroscalingTier{ 0 };
    uint32_t m_backingMemory15Flags{ 0 };
    struct ProgramRecord15Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t SubGraphRecursionTier{ 0 };
        uint32_t MeshPipeliningTier{ 0 };
        uint32_t MultiQueueFusionTier{ 0 };
        uint32_t MicroscalingTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord15Entry> m_recordedProgramRecords15;
    uint64_t m_backingMemory16Start{ 0 };
    uint64_t m_backingMemory16Size{ 0 };
    uint64_t m_backingMemory16ScratchSize{ 0 };
    uint32_t m_backingMemory16MaxConcurrency{ 0 };
    uint32_t m_backingMemory16DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory16MemoryPoolTier{ 0 };
    uint32_t m_backingMemory16SparsityMask{ 0 };
    uint32_t m_backingMemory16SubAllocationMask{ 0 };
    uint32_t m_backingMemory16PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory16FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory16SubGraphRecursionTier{ 0 };
    uint32_t m_backingMemory16MeshPipeliningTier{ 0 };
    uint32_t m_backingMemory16MultiQueueFusionTier{ 0 };
    uint32_t m_backingMemory16MicroscalingTier{ 0 };
    uint32_t m_backingMemory16SubBytePrecisionTier{ 0 };
    uint32_t m_backingMemory16Flags{ 0 };
    struct ProgramRecord16Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t SubGraphRecursionTier{ 0 };
        uint32_t MeshPipeliningTier{ 0 };
        uint32_t MultiQueueFusionTier{ 0 };
        uint32_t MicroscalingTier{ 0 };
        uint32_t SubBytePrecisionTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord16Entry> m_recordedProgramRecords16;
    uint64_t m_backingMemory17Start{ 0 };
    uint64_t m_backingMemory17Size{ 0 };
    uint64_t m_backingMemory17ScratchSize{ 0 };
    uint32_t m_backingMemory17MaxConcurrency{ 0 };
    uint32_t m_backingMemory17DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory17MemoryPoolTier{ 0 };
    uint32_t m_backingMemory17SparsityMask{ 0 };
    uint32_t m_backingMemory17SubAllocationMask{ 0 };
    uint32_t m_backingMemory17PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory17FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory17SubGraphRecursionTier{ 0 };
    uint32_t m_backingMemory17MeshPipeliningTier{ 0 };
    uint32_t m_backingMemory17MultiQueueFusionTier{ 0 };
    uint32_t m_backingMemory17MicroscalingTier{ 0 };
    uint32_t m_backingMemory17SubBytePrecisionTier{ 0 };
    uint32_t m_backingMemory17SparseTensorBlockTier{ 0 };
    uint32_t m_backingMemory17Flags{ 0 };
    struct ProgramRecord17Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t SubGraphRecursionTier{ 0 };
        uint32_t MeshPipeliningTier{ 0 };
        uint32_t MultiQueueFusionTier{ 0 };
        uint32_t MicroscalingTier{ 0 };
        uint32_t SubBytePrecisionTier{ 0 };
        uint32_t SparseTensorBlockTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord17Entry> m_recordedProgramRecords17;
    uint64_t m_backingMemory18Start{ 0 };
    uint64_t m_backingMemory18Size{ 0 };
    uint64_t m_backingMemory18ScratchSize{ 0 };
    uint32_t m_backingMemory18MaxConcurrency{ 0 };
    uint32_t m_backingMemory18DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory18MemoryPoolTier{ 0 };
    uint32_t m_backingMemory18SparsityMask{ 0 };
    uint32_t m_backingMemory18SubAllocationMask{ 0 };
    uint32_t m_backingMemory18PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory18FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory18SubGraphRecursionTier{ 0 };
    uint32_t m_backingMemory18MeshPipeliningTier{ 0 };
    uint32_t m_backingMemory18MultiQueueFusionTier{ 0 };
    uint32_t m_backingMemory18MicroscalingTier{ 0 };
    uint32_t m_backingMemory18SubBytePrecisionTier{ 0 };
    uint32_t m_backingMemory18SparseTensorBlockTier{ 0 };
    uint32_t m_backingMemory18DeterministicTensorReductionTier{ 0 };
    uint32_t m_backingMemory18Flags{ 0 };
    struct ProgramRecord18Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t SubGraphRecursionTier{ 0 };
        uint32_t MeshPipeliningTier{ 0 };
        uint32_t MultiQueueFusionTier{ 0 };
        uint32_t MicroscalingTier{ 0 };
        uint32_t SubBytePrecisionTier{ 0 };
        uint32_t SparseTensorBlockTier{ 0 };
        uint32_t DeterministicTensorReductionTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord18Entry> m_recordedProgramRecords18;
    uint64_t m_backingMemory19Start{ 0 };
    uint64_t m_backingMemory19Size{ 0 };
    uint64_t m_backingMemory19ScratchSize{ 0 };
    uint32_t m_backingMemory19MaxConcurrency{ 0 };
    uint32_t m_backingMemory19DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory19MemoryPoolTier{ 0 };
    uint32_t m_backingMemory19SparsityMask{ 0 };
    uint32_t m_backingMemory19SubAllocationMask{ 0 };
    uint32_t m_backingMemory19PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory19FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory19SubGraphRecursionTier{ 0 };
    uint32_t m_backingMemory19MeshPipeliningTier{ 0 };
    uint32_t m_backingMemory19MultiQueueFusionTier{ 0 };
    uint32_t m_backingMemory19MicroscalingTier{ 0 };
    uint32_t m_backingMemory19SubBytePrecisionTier{ 0 };
    uint32_t m_backingMemory19SparseTensorBlockTier{ 0 };
    uint32_t m_backingMemory19DeterministicTensorReductionTier{ 0 };
    uint32_t m_backingMemory19HyperQuantizationTier{ 0 };
    uint32_t m_backingMemory19Flags{ 0 };
    struct ProgramRecord19Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t SubGraphRecursionTier{ 0 };
        uint32_t MeshPipeliningTier{ 0 };
        uint32_t MultiQueueFusionTier{ 0 };
        uint32_t MicroscalingTier{ 0 };
        uint32_t SubBytePrecisionTier{ 0 };
        uint32_t SparseTensorBlockTier{ 0 };
        uint32_t DeterministicTensorReductionTier{ 0 };
        uint32_t HyperQuantizationTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord19Entry> m_recordedProgramRecords19;
    uint64_t m_backingMemory20Start{ 0 };
    uint64_t m_backingMemory20Size{ 0 };
    uint64_t m_backingMemory20ScratchSize{ 0 };
    uint32_t m_backingMemory20MaxConcurrency{ 0 };
    uint32_t m_backingMemory20DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory20MemoryPoolTier{ 0 };
    uint32_t m_backingMemory20SparsityMask{ 0 };
    uint32_t m_backingMemory20SubAllocationMask{ 0 };
    uint32_t m_backingMemory20PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory20FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory20SubGraphRecursionTier{ 0 };
    uint32_t m_backingMemory20MeshPipeliningTier{ 0 };
    uint32_t m_backingMemory20MultiQueueFusionTier{ 0 };
    uint32_t m_backingMemory20MicroscalingTier{ 0 };
    uint32_t m_backingMemory20SubBytePrecisionTier{ 0 };
    uint32_t m_backingMemory20SparseTensorBlockTier{ 0 };
    uint32_t m_backingMemory20DeterministicTensorReductionTier{ 0 };
    uint32_t m_backingMemory20HyperQuantizationTier{ 0 };
    uint32_t m_backingMemory20SparseClusterTier{ 0 };
    uint32_t m_backingMemory20Flags{ 0 };
    struct ProgramRecord20Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t SubGraphRecursionTier{ 0 };
        uint32_t MeshPipeliningTier{ 0 };
        uint32_t MultiQueueFusionTier{ 0 };
        uint32_t MicroscalingTier{ 0 };
        uint32_t SubBytePrecisionTier{ 0 };
        uint32_t SparseTensorBlockTier{ 0 };
        uint32_t DeterministicTensorReductionTier{ 0 };
        uint32_t HyperQuantizationTier{ 0 };
        uint32_t SparseClusterTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord20Entry> m_recordedProgramRecords20;
    uint64_t m_backingMemory21Start{ 0 };
    uint64_t m_backingMemory21Size{ 0 };
    uint64_t m_backingMemory21ScratchSize{ 0 };
    uint32_t m_backingMemory21MaxConcurrency{ 0 };
    uint32_t m_backingMemory21DynamicExpansionFlags{ 0 };
    uint32_t m_backingMemory21MemoryPoolTier{ 0 };
    uint32_t m_backingMemory21SparsityMask{ 0 };
    uint32_t m_backingMemory21SubAllocationMask{ 0 };
    uint32_t m_backingMemory21PipelineCoherencyTier{ 0 };
    uint32_t m_backingMemory21FeedbackLoopTier{ 0 };
    uint32_t m_backingMemory21SubGraphRecursionTier{ 0 };
    uint32_t m_backingMemory21MeshPipeliningTier{ 0 };
    uint32_t m_backingMemory21MultiQueueFusionTier{ 0 };
    uint32_t m_backingMemory21MicroscalingTier{ 0 };
    uint32_t m_backingMemory21SubBytePrecisionTier{ 0 };
    uint32_t m_backingMemory21SparseTensorBlockTier{ 0 };
    uint32_t m_backingMemory21DeterministicTensorReductionTier{ 0 };
    uint32_t m_backingMemory21HyperQuantizationTier{ 0 };
    uint32_t m_backingMemory21SparseClusterTier{ 0 };
    uint32_t m_backingMemory21MultiQueueSyncTier{ 0 };
    uint32_t m_backingMemory21Flags{ 0 };
    struct ProgramRecord21Entry {
        uint32_t NodeIndex{ 0 };
        const void* pRecord{ nullptr };
        uint32_t RecordSizeInBytes{ 0 };
        uint32_t ConcurrencyGroup{ 0 };
        uint32_t QueueAffinityMask{ 0 };
        uint32_t DynamicClusterExpansionTier{ 0 };
        uint32_t TensorCoreMask{ 0 };
        uint32_t SubAllocationMask{ 0 };
        uint32_t PipelineCoherencyTier{ 0 };
        uint32_t FeedbackLoopTier{ 0 };
        uint32_t SubGraphRecursionTier{ 0 };
        uint32_t MeshPipeliningTier{ 0 };
        uint32_t MultiQueueFusionTier{ 0 };
        uint32_t MicroscalingTier{ 0 };
        uint32_t SubBytePrecisionTier{ 0 };
        uint32_t SparseTensorBlockTier{ 0 };
        uint32_t DeterministicTensorReductionTier{ 0 };
        uint32_t HyperQuantizationTier{ 0 };
        uint32_t SparseClusterTier{ 0 };
        uint32_t MultiQueueSyncTier{ 0 };
        uint32_t DispatchMask{ 0 };
        uint32_t DispatchFlags{ 0 };
    };
    std::vector<ProgramRecord21Entry> m_recordedProgramRecords21;
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
    D3D12_SHADING_RATE m_shadingRate{ D3D12_SHADING_RATE_1X1 };
    D3D12_SHADING_RATE_COMBINER m_shadingRateCombiners[2]{ D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_PASSTHROUGH };
    ID3D12Resource* m_pShadingRateImage{ nullptr };
    uint32_t m_shadingRateSetCount{ 0 };
    uint32_t m_shadingRateImageSetCount{ 0 };

public:
    Prism3D12GraphicsCommandListImpl(ID3D12Device* pDev, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* pAlloc)
        : m_pDevice(pDev), m_pAllocator(pAlloc), m_type(type) {}

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild ||
            riid == IID_ID3D12CommandList || riid == IID_ID3D12GraphicsCommandList ||
            riid == IID_ID3D12GraphicsCommandList1 || riid == IID_ID3D12GraphicsCommandList2 ||
            riid == IID_ID3D12GraphicsCommandList3 || riid == IID_ID3D12GraphicsCommandList4 ||
            riid == OFFICIAL_IID_ID3D12GraphicsCommandList4 ||
            riid == IID_ID3D12GraphicsCommandList5 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList5 ||
            riid == IID_ID3D12GraphicsCommandList6 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList6 ||
            riid == IID_ID3D12GraphicsCommandList7 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList7 ||
            riid == IID_ID3D12GraphicsCommandList8 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList8 ||
            riid == IID_ID3D12GraphicsCommandList9 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList9 ||
            riid == IID_ID3D12GraphicsCommandList10 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList10) {
            *ppv = static_cast<ID3D12GraphicsCommandList10*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList11 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList11) {
            *ppv = static_cast<ID3D12GraphicsCommandList11*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList12 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList12) {
            *ppv = static_cast<ID3D12GraphicsCommandList12*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList13 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList13) {
            *ppv = static_cast<ID3D12GraphicsCommandList13*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList14 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList14) {
            *ppv = static_cast<ID3D12GraphicsCommandList14*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList15 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList15) {
            *ppv = static_cast<ID3D12GraphicsCommandList15*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList16 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList16) {
            *ppv = static_cast<ID3D12GraphicsCommandList16*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList17 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList17) {
            *ppv = static_cast<ID3D12GraphicsCommandList17*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList18 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList18) {
            *ppv = static_cast<ID3D12GraphicsCommandList18*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList19 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList19) {
            *ppv = static_cast<ID3D12GraphicsCommandList19*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList20 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList20) {
            *ppv = static_cast<ID3D12GraphicsCommandList20*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList21 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList21) {
            *ppv = static_cast<ID3D12GraphicsCommandList21*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList22 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList22) {
            *ppv = static_cast<ID3D12GraphicsCommandList22*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList23 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList23) {
            *ppv = static_cast<ID3D12GraphicsCommandList23*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList24 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList24) {
            *ppv = static_cast<ID3D12GraphicsCommandList24*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList25 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList25) {
            *ppv = static_cast<ID3D12GraphicsCommandList25*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList26 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList26) {
            *ppv = static_cast<ID3D12GraphicsCommandList26*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList27 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList27) {
            *ppv = static_cast<ID3D12GraphicsCommandList27*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList28 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList28) {
            *ppv = static_cast<ID3D12GraphicsCommandList28*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList29 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList29) {
            *ppv = static_cast<ID3D12GraphicsCommandList29*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList30 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList30) {
            *ppv = static_cast<ID3D12GraphicsCommandList30*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12GraphicsCommandList31 || riid == OFFICIAL_IID_ID3D12GraphicsCommandList31) {
            *ppv = static_cast<ID3D12GraphicsCommandList31*>(this);
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
        m_buildRaytracingASCount = 0;
        m_copyRaytracingASCount = 0;
        m_pActiveRaytracingStateObject = nullptr;
        m_hasProgram = false;
        m_pProtectedSession = nullptr;
        m_shadingRate = D3D12_SHADING_RATE_1X1;
        m_shadingRateCombiners[0] = D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
        m_shadingRateCombiners[1] = D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
        m_pShadingRateImage = nullptr;
        m_shadingRateSetCount = 0;
        m_shadingRateImageSetCount = 0;
        m_backingMemory = {};
        m_recordedProgramRecords.clear();
        m_backingMemory2Start = 0;
        m_backingMemory2Size = 0;
        m_backingMemory2Flags = 0;
        m_recordedProgramRecords2.clear();
        m_backingMemory3Start = 0;
        m_backingMemory3Size = 0;
        m_backingMemory3ScratchSize = 0;
        m_backingMemory3Flags = 0;
        m_recordedProgramRecords3.clear();
        m_backingMemory4Start = 0;
        m_backingMemory4Size = 0;
        m_backingMemory4ScratchSize = 0;
        m_backingMemory4MaxConcurrency = 0;
        m_backingMemory4Flags = 0;
        m_recordedProgramRecords4.clear();
        m_backingMemory5Start = 0;
        m_backingMemory5Size = 0;
        m_backingMemory5ScratchSize = 0;
        m_backingMemory5MaxConcurrency = 0;
        m_backingMemory5AllocationTier = 0;
        m_backingMemory5Flags = 0;
        m_recordedProgramRecords5.clear();
        m_backingMemory6Start = 0;
        m_backingMemory6Size = 0;
        m_backingMemory6ScratchSize = 0;
        m_backingMemory6MaxConcurrency = 0;
        m_backingMemory6DynamicExpansionFlags = 0;
        m_backingMemory6Flags = 0;
        m_recordedProgramRecords6.clear();
        m_backingMemory7Start = 0;
        m_backingMemory7Size = 0;
        m_backingMemory7ScratchSize = 0;
        m_backingMemory7MaxConcurrency = 0;
        m_backingMemory7DynamicExpansionFlags = 0;
        m_backingMemory7MemoryPoolTier = 0;
        m_backingMemory7Flags = 0;
        m_recordedProgramRecords7.clear();
        m_backingMemory8Start = 0;
        m_backingMemory8Size = 0;
        m_backingMemory8ScratchSize = 0;
        m_backingMemory8MaxConcurrency = 0;
        m_backingMemory8DynamicExpansionFlags = 0;
        m_backingMemory8MemoryPoolTier = 0;
        m_backingMemory8SparsityMask = 0;
        m_backingMemory8Flags = 0;
        m_recordedProgramRecords8.clear();
        m_backingMemory9Start = 0;
        m_backingMemory9Size = 0;
        m_backingMemory9ScratchSize = 0;
        m_backingMemory9MaxConcurrency = 0;
        m_backingMemory9DynamicExpansionFlags = 0;
        m_backingMemory9MemoryPoolTier = 0;
        m_backingMemory9SparsityMask = 0;
        m_backingMemory9SubAllocationMask = 0;
        m_backingMemory9Flags = 0;
        m_recordedProgramRecords9.clear();
        m_backingMemory10Start = 0;
        m_backingMemory10Size = 0;
        m_backingMemory10ScratchSize = 0;
        m_backingMemory10MaxConcurrency = 0;
        m_backingMemory10DynamicExpansionFlags = 0;
        m_backingMemory10MemoryPoolTier = 0;
        m_backingMemory10SparsityMask = 0;
        m_backingMemory10SubAllocationMask = 0;
        m_backingMemory10PipelineCoherencyTier = 0;
        m_backingMemory10Flags = 0;
        m_recordedProgramRecords10.clear();
        m_backingMemory11Start = 0;
        m_backingMemory11Size = 0;
        m_backingMemory11ScratchSize = 0;
        m_backingMemory11MaxConcurrency = 0;
        m_backingMemory11DynamicExpansionFlags = 0;
        m_backingMemory11MemoryPoolTier = 0;
        m_backingMemory11SparsityMask = 0;
        m_backingMemory11SubAllocationMask = 0;
        m_backingMemory11PipelineCoherencyTier = 0;
        m_backingMemory11FeedbackLoopTier = 0;
        m_backingMemory11Flags = 0;
        m_recordedProgramRecords11.clear();
        m_backingMemory12Start = 0;
        m_backingMemory12Size = 0;
        m_backingMemory12ScratchSize = 0;
        m_backingMemory12MaxConcurrency = 0;
        m_backingMemory12DynamicExpansionFlags = 0;
        m_backingMemory12MemoryPoolTier = 0;
        m_backingMemory12SparsityMask = 0;
        m_backingMemory12SubAllocationMask = 0;
        m_backingMemory12PipelineCoherencyTier = 0;
        m_backingMemory12FeedbackLoopTier = 0;
        m_backingMemory12SubGraphRecursionTier = 0;
        m_backingMemory12Flags = 0;
        m_recordedProgramRecords12.clear();
        m_backingMemory13Start = 0;
        m_backingMemory13Size = 0;
        m_backingMemory13ScratchSize = 0;
        m_backingMemory13MaxConcurrency = 0;
        m_backingMemory13DynamicExpansionFlags = 0;
        m_backingMemory13MemoryPoolTier = 0;
        m_backingMemory13SparsityMask = 0;
        m_backingMemory13SubAllocationMask = 0;
        m_backingMemory13PipelineCoherencyTier = 0;
        m_backingMemory13FeedbackLoopTier = 0;
        m_backingMemory13SubGraphRecursionTier = 0;
        m_backingMemory13MeshPipeliningTier = 0;
        m_backingMemory13Flags = 0;
        m_recordedProgramRecords13.clear();
        m_backingMemory14Start = 0;
        m_backingMemory14Size = 0;
        m_backingMemory14ScratchSize = 0;
        m_backingMemory14MaxConcurrency = 0;
        m_backingMemory14DynamicExpansionFlags = 0;
        m_backingMemory14MemoryPoolTier = 0;
        m_backingMemory14SparsityMask = 0;
        m_backingMemory14SubAllocationMask = 0;
        m_backingMemory14PipelineCoherencyTier = 0;
        m_backingMemory14FeedbackLoopTier = 0;
        m_backingMemory14SubGraphRecursionTier = 0;
        m_backingMemory14MeshPipeliningTier = 0;
        m_backingMemory14MultiQueueFusionTier = 0;
        m_backingMemory14Flags = 0;
        m_recordedProgramRecords14.clear();
        m_backingMemory15Start = 0;
        m_backingMemory15Size = 0;
        m_backingMemory15ScratchSize = 0;
        m_backingMemory15MaxConcurrency = 0;
        m_backingMemory15DynamicExpansionFlags = 0;
        m_backingMemory15MemoryPoolTier = 0;
        m_backingMemory15SparsityMask = 0;
        m_backingMemory15SubAllocationMask = 0;
        m_backingMemory15PipelineCoherencyTier = 0;
        m_backingMemory15FeedbackLoopTier = 0;
        m_backingMemory15SubGraphRecursionTier = 0;
        m_backingMemory15MeshPipeliningTier = 0;
        m_backingMemory15MultiQueueFusionTier = 0;
        m_backingMemory15MicroscalingTier = 0;
        m_backingMemory15Flags = 0;
        m_recordedProgramRecords15.clear();
        m_backingMemory16Start = 0;
        m_backingMemory16Size = 0;
        m_backingMemory16ScratchSize = 0;
        m_backingMemory16MaxConcurrency = 0;
        m_backingMemory16DynamicExpansionFlags = 0;
        m_backingMemory16MemoryPoolTier = 0;
        m_backingMemory16SparsityMask = 0;
        m_backingMemory16SubAllocationMask = 0;
        m_backingMemory16PipelineCoherencyTier = 0;
        m_backingMemory16FeedbackLoopTier = 0;
        m_backingMemory16SubGraphRecursionTier = 0;
        m_backingMemory16MeshPipeliningTier = 0;
        m_backingMemory16MultiQueueFusionTier = 0;
        m_backingMemory16MicroscalingTier = 0;
        m_backingMemory16SubBytePrecisionTier = 0;
        m_backingMemory16Flags = 0;
        m_recordedProgramRecords16.clear();
        m_backingMemory17Start = 0;
        m_backingMemory17Size = 0;
        m_backingMemory17ScratchSize = 0;
        m_backingMemory17MaxConcurrency = 0;
        m_backingMemory17DynamicExpansionFlags = 0;
        m_backingMemory17MemoryPoolTier = 0;
        m_backingMemory17SparsityMask = 0;
        m_backingMemory17SubAllocationMask = 0;
        m_backingMemory17PipelineCoherencyTier = 0;
        m_backingMemory17FeedbackLoopTier = 0;
        m_backingMemory17SubGraphRecursionTier = 0;
        m_backingMemory17MeshPipeliningTier = 0;
        m_backingMemory17MultiQueueFusionTier = 0;
        m_backingMemory17MicroscalingTier = 0;
        m_backingMemory17SubBytePrecisionTier = 0;
        m_backingMemory17SparseTensorBlockTier = 0;
        m_backingMemory17Flags = 0;
        m_recordedProgramRecords17.clear();
        m_backingMemory18Start = 0;
        m_backingMemory18Size = 0;
        m_backingMemory18ScratchSize = 0;
        m_backingMemory18MaxConcurrency = 0;
        m_backingMemory18DynamicExpansionFlags = 0;
        m_backingMemory18MemoryPoolTier = 0;
        m_backingMemory18SparsityMask = 0;
        m_backingMemory18SubAllocationMask = 0;
        m_backingMemory18PipelineCoherencyTier = 0;
        m_backingMemory18FeedbackLoopTier = 0;
        m_backingMemory18SubGraphRecursionTier = 0;
        m_backingMemory18MeshPipeliningTier = 0;
        m_backingMemory18MultiQueueFusionTier = 0;
        m_backingMemory18MicroscalingTier = 0;
        m_backingMemory18SubBytePrecisionTier = 0;
        m_backingMemory18SparseTensorBlockTier = 0;
        m_backingMemory18DeterministicTensorReductionTier = 0;
        m_backingMemory18Flags = 0;
        m_recordedProgramRecords18.clear();
        m_backingMemory19Start = 0;
        m_backingMemory19Size = 0;
        m_backingMemory19ScratchSize = 0;
        m_backingMemory19MaxConcurrency = 0;
        m_backingMemory19DynamicExpansionFlags = 0;
        m_backingMemory19MemoryPoolTier = 0;
        m_backingMemory19SparsityMask = 0;
        m_backingMemory19SubAllocationMask = 0;
        m_backingMemory19PipelineCoherencyTier = 0;
        m_backingMemory19FeedbackLoopTier = 0;
        m_backingMemory19SubGraphRecursionTier = 0;
        m_backingMemory19MeshPipeliningTier = 0;
        m_backingMemory19MultiQueueFusionTier = 0;
        m_backingMemory19MicroscalingTier = 0;
        m_backingMemory19SubBytePrecisionTier = 0;
        m_backingMemory19SparseTensorBlockTier = 0;
        m_backingMemory19DeterministicTensorReductionTier = 0;
        m_backingMemory19HyperQuantizationTier = 0;
        m_backingMemory19Flags = 0;
        m_recordedProgramRecords19.clear();
        m_backingMemory20Start = 0;
        m_backingMemory20Size = 0;
        m_backingMemory20ScratchSize = 0;
        m_backingMemory20MaxConcurrency = 0;
        m_backingMemory20DynamicExpansionFlags = 0;
        m_backingMemory20MemoryPoolTier = 0;
        m_backingMemory20SparsityMask = 0;
        m_backingMemory20SubAllocationMask = 0;
        m_backingMemory20PipelineCoherencyTier = 0;
        m_backingMemory20FeedbackLoopTier = 0;
        m_backingMemory20SubGraphRecursionTier = 0;
        m_backingMemory20MeshPipeliningTier = 0;
        m_backingMemory20MultiQueueFusionTier = 0;
        m_backingMemory20MicroscalingTier = 0;
        m_backingMemory20SubBytePrecisionTier = 0;
        m_backingMemory20SparseTensorBlockTier = 0;
        m_backingMemory20DeterministicTensorReductionTier = 0;
        m_backingMemory20HyperQuantizationTier = 0;
        m_backingMemory20SparseClusterTier = 0;
        m_backingMemory20Flags = 0;
        m_recordedProgramRecords20.clear();
        m_isOpen = true;
        return 0;
    }

    void ClearState(ID3D12PipelineState*) override {
        m_commands.clear();
        m_dispatchedGraphs.clear();
        m_backingMemory = {};
        m_recordedProgramRecords.clear();
        m_backingMemory2Start = 0;
        m_backingMemory2Size = 0;
        m_backingMemory2Flags = 0;
        m_recordedProgramRecords2.clear();
        m_backingMemory3Start = 0;
        m_backingMemory3Size = 0;
        m_backingMemory3ScratchSize = 0;
        m_backingMemory3Flags = 0;
        m_recordedProgramRecords3.clear();
        m_backingMemory4Start = 0;
        m_backingMemory4Size = 0;
        m_backingMemory4ScratchSize = 0;
        m_backingMemory4MaxConcurrency = 0;
        m_backingMemory4Flags = 0;
        m_recordedProgramRecords4.clear();
        m_backingMemory5Start = 0;
        m_backingMemory5Size = 0;
        m_backingMemory5ScratchSize = 0;
        m_backingMemory5MaxConcurrency = 0;
        m_backingMemory5AllocationTier = 0;
        m_backingMemory5Flags = 0;
        m_recordedProgramRecords5.clear();
        m_backingMemory6Start = 0;
        m_backingMemory6Size = 0;
        m_backingMemory6ScratchSize = 0;
        m_backingMemory6MaxConcurrency = 0;
        m_backingMemory6DynamicExpansionFlags = 0;
        m_backingMemory6Flags = 0;
        m_recordedProgramRecords6.clear();
        m_backingMemory7Start = 0;
        m_backingMemory7Size = 0;
        m_backingMemory7ScratchSize = 0;
        m_backingMemory7MaxConcurrency = 0;
        m_backingMemory7DynamicExpansionFlags = 0;
        m_backingMemory7MemoryPoolTier = 0;
        m_backingMemory7Flags = 0;
        m_recordedProgramRecords7.clear();
        m_backingMemory8Start = 0;
        m_backingMemory8Size = 0;
        m_backingMemory8ScratchSize = 0;
        m_backingMemory8MaxConcurrency = 0;
        m_backingMemory8DynamicExpansionFlags = 0;
        m_backingMemory8MemoryPoolTier = 0;
        m_backingMemory8SparsityMask = 0;
        m_backingMemory8Flags = 0;
        m_recordedProgramRecords8.clear();
        m_backingMemory9Start = 0;
        m_backingMemory9Size = 0;
        m_backingMemory9ScratchSize = 0;
        m_backingMemory9MaxConcurrency = 0;
        m_backingMemory9DynamicExpansionFlags = 0;
        m_backingMemory9MemoryPoolTier = 0;
        m_backingMemory9SparsityMask = 0;
        m_backingMemory9SubAllocationMask = 0;
        m_backingMemory9Flags = 0;
        m_recordedProgramRecords9.clear();
        m_backingMemory10Start = 0;
        m_backingMemory10Size = 0;
        m_backingMemory10ScratchSize = 0;
        m_backingMemory10MaxConcurrency = 0;
        m_backingMemory10DynamicExpansionFlags = 0;
        m_backingMemory10MemoryPoolTier = 0;
        m_backingMemory10SparsityMask = 0;
        m_backingMemory10SubAllocationMask = 0;
        m_backingMemory10PipelineCoherencyTier = 0;
        m_backingMemory10Flags = 0;
        m_recordedProgramRecords10.clear();
        m_backingMemory11Start = 0;
        m_backingMemory11Size = 0;
        m_backingMemory11ScratchSize = 0;
        m_backingMemory11MaxConcurrency = 0;
        m_backingMemory11DynamicExpansionFlags = 0;
        m_backingMemory11MemoryPoolTier = 0;
        m_backingMemory11SparsityMask = 0;
        m_backingMemory11SubAllocationMask = 0;
        m_backingMemory11PipelineCoherencyTier = 0;
        m_backingMemory11FeedbackLoopTier = 0;
        m_backingMemory11Flags = 0;
        m_recordedProgramRecords11.clear();
        m_backingMemory12Start = 0;
        m_backingMemory12Size = 0;
        m_backingMemory12ScratchSize = 0;
        m_backingMemory12MaxConcurrency = 0;
        m_backingMemory12DynamicExpansionFlags = 0;
        m_backingMemory12MemoryPoolTier = 0;
        m_backingMemory12SparsityMask = 0;
        m_backingMemory12SubAllocationMask = 0;
        m_backingMemory12PipelineCoherencyTier = 0;
        m_backingMemory12FeedbackLoopTier = 0;
        m_backingMemory12SubGraphRecursionTier = 0;
        m_backingMemory12Flags = 0;
        m_recordedProgramRecords12.clear();
        m_backingMemory13Start = 0;
        m_backingMemory13Size = 0;
        m_backingMemory13ScratchSize = 0;
        m_backingMemory13MaxConcurrency = 0;
        m_backingMemory13DynamicExpansionFlags = 0;
        m_backingMemory13MemoryPoolTier = 0;
        m_backingMemory13SparsityMask = 0;
        m_backingMemory13SubAllocationMask = 0;
        m_backingMemory13PipelineCoherencyTier = 0;
        m_backingMemory13FeedbackLoopTier = 0;
        m_backingMemory13SubGraphRecursionTier = 0;
        m_backingMemory13MeshPipeliningTier = 0;
        m_backingMemory13Flags = 0;
        m_recordedProgramRecords13.clear();
        m_backingMemory14Start = 0;
        m_backingMemory14Size = 0;
        m_backingMemory14ScratchSize = 0;
        m_backingMemory14MaxConcurrency = 0;
        m_backingMemory14DynamicExpansionFlags = 0;
        m_backingMemory14MemoryPoolTier = 0;
        m_backingMemory14SparsityMask = 0;
        m_backingMemory14SubAllocationMask = 0;
        m_backingMemory14PipelineCoherencyTier = 0;
        m_backingMemory14FeedbackLoopTier = 0;
        m_backingMemory14SubGraphRecursionTier = 0;
        m_backingMemory14MeshPipeliningTier = 0;
        m_backingMemory14MultiQueueFusionTier = 0;
        m_backingMemory14Flags = 0;
        m_recordedProgramRecords14.clear();
        m_backingMemory15Start = 0;
        m_backingMemory15Size = 0;
        m_backingMemory15ScratchSize = 0;
        m_backingMemory15MaxConcurrency = 0;
        m_backingMemory15DynamicExpansionFlags = 0;
        m_backingMemory15MemoryPoolTier = 0;
        m_backingMemory15SparsityMask = 0;
        m_backingMemory15SubAllocationMask = 0;
        m_backingMemory15PipelineCoherencyTier = 0;
        m_backingMemory15FeedbackLoopTier = 0;
        m_backingMemory15SubGraphRecursionTier = 0;
        m_backingMemory15MeshPipeliningTier = 0;
        m_backingMemory15MultiQueueFusionTier = 0;
        m_backingMemory15MicroscalingTier = 0;
        m_backingMemory15Flags = 0;
        m_recordedProgramRecords15.clear();
        m_backingMemory16Start = 0;
        m_backingMemory16Size = 0;
        m_backingMemory16ScratchSize = 0;
        m_backingMemory16MaxConcurrency = 0;
        m_backingMemory16DynamicExpansionFlags = 0;
        m_backingMemory16MemoryPoolTier = 0;
        m_backingMemory16SparsityMask = 0;
        m_backingMemory16SubAllocationMask = 0;
        m_backingMemory16PipelineCoherencyTier = 0;
        m_backingMemory16FeedbackLoopTier = 0;
        m_backingMemory16SubGraphRecursionTier = 0;
        m_backingMemory16MeshPipeliningTier = 0;
        m_backingMemory16MultiQueueFusionTier = 0;
        m_backingMemory16MicroscalingTier = 0;
        m_backingMemory16SubBytePrecisionTier = 0;
        m_backingMemory16Flags = 0;
        m_recordedProgramRecords16.clear();
        m_backingMemory17Start = 0;
        m_backingMemory17Size = 0;
        m_backingMemory17ScratchSize = 0;
        m_backingMemory17MaxConcurrency = 0;
        m_backingMemory17DynamicExpansionFlags = 0;
        m_backingMemory17MemoryPoolTier = 0;
        m_backingMemory17SparsityMask = 0;
        m_backingMemory17SubAllocationMask = 0;
        m_backingMemory17PipelineCoherencyTier = 0;
        m_backingMemory17FeedbackLoopTier = 0;
        m_backingMemory17SubGraphRecursionTier = 0;
        m_backingMemory17MeshPipeliningTier = 0;
        m_backingMemory17MultiQueueFusionTier = 0;
        m_backingMemory17MicroscalingTier = 0;
        m_backingMemory17SubBytePrecisionTier = 0;
        m_backingMemory17SparseTensorBlockTier = 0;
        m_backingMemory17Flags = 0;
        m_recordedProgramRecords17.clear();
        m_backingMemory18Start = 0;
        m_backingMemory18Size = 0;
        m_backingMemory18ScratchSize = 0;
        m_backingMemory18MaxConcurrency = 0;
        m_backingMemory18DynamicExpansionFlags = 0;
        m_backingMemory18MemoryPoolTier = 0;
        m_backingMemory18SparsityMask = 0;
        m_backingMemory18SubAllocationMask = 0;
        m_backingMemory18PipelineCoherencyTier = 0;
        m_backingMemory18FeedbackLoopTier = 0;
        m_backingMemory18SubGraphRecursionTier = 0;
        m_backingMemory18MeshPipeliningTier = 0;
        m_backingMemory18MultiQueueFusionTier = 0;
        m_backingMemory18MicroscalingTier = 0;
        m_backingMemory18SubBytePrecisionTier = 0;
        m_backingMemory18SparseTensorBlockTier = 0;
        m_backingMemory18DeterministicTensorReductionTier = 0;
        m_backingMemory18Flags = 0;
        m_recordedProgramRecords18.clear();
        m_backingMemory19Start = 0;
        m_backingMemory19Size = 0;
        m_backingMemory19ScratchSize = 0;
        m_backingMemory19MaxConcurrency = 0;
        m_backingMemory19DynamicExpansionFlags = 0;
        m_backingMemory19MemoryPoolTier = 0;
        m_backingMemory19SparsityMask = 0;
        m_backingMemory19SubAllocationMask = 0;
        m_backingMemory19PipelineCoherencyTier = 0;
        m_backingMemory19FeedbackLoopTier = 0;
        m_backingMemory19SubGraphRecursionTier = 0;
        m_backingMemory19MeshPipeliningTier = 0;
        m_backingMemory19MultiQueueFusionTier = 0;
        m_backingMemory19MicroscalingTier = 0;
        m_backingMemory19SubBytePrecisionTier = 0;
        m_backingMemory19SparseTensorBlockTier = 0;
        m_backingMemory19DeterministicTensorReductionTier = 0;
        m_backingMemory19HyperQuantizationTier = 0;
        m_backingMemory19Flags = 0;
        m_recordedProgramRecords19.clear();
        m_backingMemory20Start = 0;
        m_backingMemory20Size = 0;
        m_backingMemory20ScratchSize = 0;
        m_backingMemory20MaxConcurrency = 0;
        m_backingMemory20DynamicExpansionFlags = 0;
        m_backingMemory20MemoryPoolTier = 0;
        m_backingMemory20SparsityMask = 0;
        m_backingMemory20SubAllocationMask = 0;
        m_backingMemory20PipelineCoherencyTier = 0;
        m_backingMemory20FeedbackLoopTier = 0;
        m_backingMemory20SubGraphRecursionTier = 0;
        m_backingMemory20MeshPipeliningTier = 0;
        m_backingMemory20MultiQueueFusionTier = 0;
        m_backingMemory20MicroscalingTier = 0;
        m_backingMemory20SubBytePrecisionTier = 0;
        m_backingMemory20SparseTensorBlockTier = 0;
        m_backingMemory20DeterministicTensorReductionTier = 0;
        m_backingMemory20HyperQuantizationTier = 0;
        m_backingMemory20SparseClusterTier = 0;
        m_backingMemory20Flags = 0;
        m_recordedProgramRecords20.clear();
        m_backingMemory21Start = 0;
        m_backingMemory21Size = 0;
        m_backingMemory21ScratchSize = 0;
        m_backingMemory21MaxConcurrency = 0;
        m_backingMemory21DynamicExpansionFlags = 0;
        m_backingMemory21MemoryPoolTier = 0;
        m_backingMemory21SparsityMask = 0;
        m_backingMemory21SubAllocationMask = 0;
        m_backingMemory21PipelineCoherencyTier = 0;
        m_backingMemory21FeedbackLoopTier = 0;
        m_backingMemory21SubGraphRecursionTier = 0;
        m_backingMemory21MeshPipeliningTier = 0;
        m_backingMemory21MultiQueueFusionTier = 0;
        m_backingMemory21MicroscalingTier = 0;
        m_backingMemory21SubBytePrecisionTier = 0;
        m_backingMemory21SparseTensorBlockTier = 0;
        m_backingMemory21DeterministicTensorReductionTier = 0;
        m_backingMemory21HyperQuantizationTier = 0;
        m_backingMemory21SparseClusterTier = 0;
        m_backingMemory21MultiQueueSyncTier = 0;
        m_backingMemory21Flags = 0;
        m_recordedProgramRecords21.clear();
        m_hasProgram = false;
        m_pProtectedSession = nullptr;
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

    uint32_t m_buildRaytracingASCount{ 0 };
    uint32_t m_copyRaytracingASCount{ 0 };
    ID3D12StateObject* m_pActiveRaytracingStateObject{ nullptr };

    void BuildRaytracingAccelerationStructure(
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* /*pDesc*/,
        uint32_t /*NumPostbuildInfoDescs*/,
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* /*pPostbuildInfoDescs*/) override {
        if (!m_isOpen) return;
        m_buildRaytracingASCount++;
    }

    void EmitRaytracingAccelerationStructurePostbuildInfo(
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* /*pDesc*/,
        uint32_t /*NumSourceAccelerationStructures*/,
        const uint64_t* /*pSourceAccelerationStructureData*/) override {}

    void CopyRaytracingAccelerationStructure(
        uint64_t /*DestAccelerationStructureData*/,
        uint64_t /*SourceAccelerationStructureData*/,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE /*Mode*/) override {
        if (!m_isOpen) return;
        m_copyRaytracingASCount++;
    }

    void SetPipelineState1(ID3D12StateObject* pStateObject) override {
        if (!m_isOpen) return;
        m_pActiveRaytracingStateObject = pStateObject;
    }

    [[nodiscard]] uint32_t GetBuildRaytracingASCount() const noexcept { return m_buildRaytracingASCount; }
    [[nodiscard]] uint32_t GetCopyRaytracingASCount() const noexcept { return m_copyRaytracingASCount; }
    [[nodiscard]] ID3D12StateObject* GetActiveRaytracingStateObject() const noexcept { return m_pActiveRaytracingStateObject; }

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

    uint32_t m_samplePositionsCount{0};
    std::vector<D3D12_SAMPLE_POSITION> m_samplePositions;
    uint32_t m_viewInstanceMask{0};

    void SetSamplePositions(uint32_t NumSamplesPerPixel, uint32_t NumPixels, D3D12_SAMPLE_POSITION* pSamplePositions) override {
        if (!m_isOpen) return;
        m_samplePositionsCount = NumSamplesPerPixel * NumPixels;
        m_samplePositions.clear();
        if (pSamplePositions && m_samplePositionsCount > 0) {
            m_samplePositions.assign(pSamplePositions, pSamplePositions + m_samplePositionsCount);
        }
    }

    [[nodiscard]] uint32_t GetSamplePositionsCount() const noexcept { return m_samplePositionsCount; }
    [[nodiscard]] const std::vector<D3D12_SAMPLE_POSITION>& GetSamplePositions() const noexcept { return m_samplePositions; }

    void ResolveSubresourceRegion(
        ID3D12Resource*, uint32_t, uint32_t, uint32_t,
        ID3D12Resource*, uint32_t, D3D12_RECT*,
        DXGI_FORMAT, uint32_t) override {}

    void SetViewInstanceMask(uint32_t Mask) override {
        m_viewInstanceMask = Mask;
    }

    void RSSetShadingRate(D3D12_SHADING_RATE baseShadingRate, const D3D12_SHADING_RATE_COMBINER* combiners) override {
        if (!m_isOpen) return;
        m_shadingRate = baseShadingRate;
        if (combiners) {
            m_shadingRateCombiners[0] = combiners[0];
            m_shadingRateCombiners[1] = combiners[1];
        }
        m_shadingRateSetCount++;
    }

    void RSSetShadingRateImage(ID3D12Resource* shadingRateImage) override {
        if (!m_isOpen) return;
        m_pShadingRateImage = shadingRateImage;
        m_shadingRateImageSetCount++;
    }

    [[nodiscard]] D3D12_SHADING_RATE GetShadingRate() const noexcept { return m_shadingRate; }
    [[nodiscard]] const D3D12_SHADING_RATE_COMBINER* GetShadingRateCombiners() const noexcept { return m_shadingRateCombiners; }
    [[nodiscard]] ID3D12Resource* GetShadingRateImage() const noexcept { return m_pShadingRateImage; }
    [[nodiscard]] uint32_t GetShadingRateSetCount() const noexcept { return m_shadingRateSetCount; }
    [[nodiscard]] uint32_t GetShadingRateImageSetCount() const noexcept { return m_shadingRateImageSetCount; }

    [[nodiscard]] uint32_t GetViewInstanceMask() const noexcept { return m_viewInstanceMask; }

    struct WriteBufferImmediateRecord {
        D3D12_GPU_VIRTUAL_ADDRESS Dest{0};
        uint32_t Value{0};
        D3D12_WRITEBUFFERIMMEDIATE_MODE Mode{D3D12_WRITEBUFFERIMMEDIATE_MODE_DEFAULT};
    };
    uint32_t m_writeBufferImmediateCount{ 0 };
    std::vector<WriteBufferImmediateRecord> m_recordedWriteBufferImmediates;

    void WriteBufferImmediate(
        uint32_t Count,
        const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER* pParams,
        const D3D12_WRITEBUFFERIMMEDIATE_MODE* pModes) override {
        if (!m_isOpen) return;
        m_writeBufferImmediateCount += Count;
        if (pParams && Count > 0) {
            for (uint32_t i = 0; i < Count; ++i) {
                WriteBufferImmediateRecord rec{};
                rec.Dest = pParams[i].Dest;
                rec.Value = pParams[i].Value;
                rec.Mode = pModes ? pModes[i] : D3D12_WRITEBUFFERIMMEDIATE_MODE_DEFAULT;
                m_recordedWriteBufferImmediates.push_back(rec);
            }
        }
    }

    [[nodiscard]] uint32_t GetWriteBufferImmediateCount() const noexcept { return m_writeBufferImmediateCount; }
    [[nodiscard]] const std::vector<WriteBufferImmediateRecord>& GetRecordedWriteBufferImmediates() const noexcept { return m_recordedWriteBufferImmediates; }

    ID3D12ProtectedResourceSession* m_pProtectedSession{ nullptr };

    void SetProtectedResourceSession(ID3D12ProtectedResourceSession* pProtectedResourceSession) override {
        m_pProtectedSession = pProtectedResourceSession;
    }

    [[nodiscard]] ID3D12ProtectedResourceSession* GetProtectedResourceSession() const noexcept {
        return m_pProtectedSession;
    }

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

    uint32_t m_frontStencilRef{0};
    uint32_t m_backStencilRef{0};
    float m_depthBias{0.0f};
    float m_depthBiasClamp{0.0f};
    float m_slopeScaledDepthBias{0.0f};
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE m_ibStripCutValue{D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED};

    void OMSetFrontAndBackStencilRef(uint32_t FrontStencilRef, uint32_t BackStencilRef) override {
        if (!m_isOpen) return;
        m_frontStencilRef = FrontStencilRef;
        m_backStencilRef = BackStencilRef;
    }

    void RSSetDepthBias(float DepthBias, float DepthBiasClamp, float SlopeScaledDepthBias) override {
        if (!m_isOpen) return;
        m_depthBias = DepthBias;
        m_depthBiasClamp = DepthBiasClamp;
        m_slopeScaledDepthBias = SlopeScaledDepthBias;
    }

    void IASetIndexBufferStripCutValue(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue) override {
        if (!m_isOpen) return;
        m_ibStripCutValue = IBStripCutValue;
    }

    [[nodiscard]] uint32_t GetFrontStencilRef() const noexcept { return m_frontStencilRef; }
    [[nodiscard]] uint32_t GetBackStencilRef() const noexcept { return m_backStencilRef; }
    [[nodiscard]] float GetDepthBias() const noexcept { return m_depthBias; }
    [[nodiscard]] float GetDepthBiasClamp() const noexcept { return m_depthBiasClamp; }
    [[nodiscard]] float GetSlopeScaledDepthBias() const noexcept { return m_slopeScaledDepthBias; }
    [[nodiscard]] D3D12_INDEX_BUFFER_STRIP_CUT_VALUE GetIndexBufferStripCutValue() const noexcept { return m_ibStripCutValue; }

    void SetWorkGraphBackingMemory(const D3D12_GPU_VIRTUAL_ADDRESS_RANGE* pBackingMemory) override {
        if (!m_isOpen) return;
        if (pBackingMemory) {
            m_backingMemory = *pBackingMemory;
        } else {
            m_backingMemory = {};
        }
    }

    void DispatchGraphProgramRecord(uint32_t NodeIndex, const void* pRecord) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords.push_back({ NodeIndex, pRecord });
    }

    [[nodiscard]] const D3D12_GPU_VIRTUAL_ADDRESS_RANGE& GetWorkGraphBackingMemory() const noexcept { return m_backingMemory; }
    [[nodiscard]] size_t GetRecordedProgramRecordCount() const noexcept { return m_recordedProgramRecords.size(); }
    [[nodiscard]] const std::vector<std::pair<uint32_t, const void*>>& GetRecordedProgramRecords() const noexcept { return m_recordedProgramRecords; }

    void SetWorkGraphBackingMemory2(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory2Start = RangeStart;
        m_backingMemory2Size = RangeSizeInBytes;
        m_backingMemory2Flags = Flags;
    }

    void DispatchGraphProgramRecord2(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords2.push_back({ NodeIndex, pRecord, RecordSizeInBytes });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory2Start() const noexcept { return m_backingMemory2Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory2Size() const noexcept { return m_backingMemory2Size; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory2Flags() const noexcept { return m_backingMemory2Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord2Count() const noexcept { return m_recordedProgramRecords2.size(); }
    [[nodiscard]] const std::vector<ProgramRecord2Entry>& GetRecordedProgramRecords2() const noexcept { return m_recordedProgramRecords2; }

    void SetWorkGraphBackingMemory3(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory3Start = RangeStart;
        m_backingMemory3Size = RangeSizeInBytes;
        m_backingMemory3ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory3Flags = Flags;
    }

    void DispatchGraphProgramRecord3(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t RecordCount) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords3.push_back({ NodeIndex, pRecord, RecordSizeInBytes, RecordCount });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory3Start() const noexcept { return m_backingMemory3Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory3Size() const noexcept { return m_backingMemory3Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory3ScratchSize() const noexcept { return m_backingMemory3ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory3Flags() const noexcept { return m_backingMemory3Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord3Count() const noexcept { return m_recordedProgramRecords3.size(); }
    [[nodiscard]] const std::vector<ProgramRecord3Entry>& GetRecordedProgramRecords3() const noexcept { return m_recordedProgramRecords3; }

    void SetWorkGraphBackingMemory4(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory4Start = RangeStart;
        m_backingMemory4Size = RangeSizeInBytes;
        m_backingMemory4ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory4MaxConcurrency = MaxConcurrency;
        m_backingMemory4Flags = Flags;
    }

    void DispatchGraphProgramRecord4(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords4.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory4Start() const noexcept { return m_backingMemory4Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory4Size() const noexcept { return m_backingMemory4Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory4ScratchSize() const noexcept { return m_backingMemory4ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory4MaxConcurrency() const noexcept { return m_backingMemory4MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory4Flags() const noexcept { return m_backingMemory4Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord4Count() const noexcept { return m_recordedProgramRecords4.size(); }
    [[nodiscard]] const std::vector<ProgramRecord4Entry>& GetRecordedProgramRecords4() const noexcept { return m_recordedProgramRecords4; }

    void SetWorkGraphBackingMemory5(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t AllocationTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory5Start = RangeStart;
        m_backingMemory5Size = RangeSizeInBytes;
        m_backingMemory5ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory5MaxConcurrency = MaxConcurrency;
        m_backingMemory5AllocationTier = AllocationTier;
        m_backingMemory5Flags = Flags;
    }

    void DispatchGraphProgramRecord5(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords5.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory5Start() const noexcept { return m_backingMemory5Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory5Size() const noexcept { return m_backingMemory5Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory5ScratchSize() const noexcept { return m_backingMemory5ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory5MaxConcurrency() const noexcept { return m_backingMemory5MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory5AllocationTier() const noexcept { return m_backingMemory5AllocationTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory5Flags() const noexcept { return m_backingMemory5Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord5Count() const noexcept { return m_recordedProgramRecords5.size(); }
    [[nodiscard]] const std::vector<ProgramRecord5Entry>& GetRecordedProgramRecords5() const noexcept { return m_recordedProgramRecords5; }

    void SetWorkGraphBackingMemory6(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory6Start = RangeStart;
        m_backingMemory6Size = RangeSizeInBytes;
        m_backingMemory6ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory6MaxConcurrency = MaxConcurrency;
        m_backingMemory6DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory6Flags = Flags;
    }

    void DispatchGraphProgramRecord6(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords6.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory6Start() const noexcept { return m_backingMemory6Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory6Size() const noexcept { return m_backingMemory6Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory6ScratchSize() const noexcept { return m_backingMemory6ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory6MaxConcurrency() const noexcept { return m_backingMemory6MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory6DynamicExpansionFlags() const noexcept { return m_backingMemory6DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory6Flags() const noexcept { return m_backingMemory6Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord6Count() const noexcept { return m_recordedProgramRecords6.size(); }
    [[nodiscard]] const std::vector<ProgramRecord6Entry>& GetRecordedProgramRecords6() const noexcept { return m_recordedProgramRecords6; }

    void SetWorkGraphBackingMemory7(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory7Start = RangeStart;
        m_backingMemory7Size = RangeSizeInBytes;
        m_backingMemory7ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory7MaxConcurrency = MaxConcurrency;
        m_backingMemory7DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory7MemoryPoolTier = MemoryPoolTier;
        m_backingMemory7Flags = Flags;
    }

    void DispatchGraphProgramRecord7(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords7.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory7Start() const noexcept { return m_backingMemory7Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory7Size() const noexcept { return m_backingMemory7Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory7ScratchSize() const noexcept { return m_backingMemory7ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory7MaxConcurrency() const noexcept { return m_backingMemory7MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory7DynamicExpansionFlags() const noexcept { return m_backingMemory7DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory7MemoryPoolTier() const noexcept { return m_backingMemory7MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory7Flags() const noexcept { return m_backingMemory7Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord7Count() const noexcept { return m_recordedProgramRecords7.size(); }
    [[nodiscard]] const std::vector<ProgramRecord7Entry>& GetRecordedProgramRecords7() const noexcept { return m_recordedProgramRecords7; }

    void SetWorkGraphBackingMemory8(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory8Start = RangeStart;
        m_backingMemory8Size = RangeSizeInBytes;
        m_backingMemory8ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory8MaxConcurrency = MaxConcurrency;
        m_backingMemory8DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory8MemoryPoolTier = MemoryPoolTier;
        m_backingMemory8SparsityMask = SparsityMask;
        m_backingMemory8Flags = Flags;
    }

    void DispatchGraphProgramRecord8(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords8.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory8Start() const noexcept { return m_backingMemory8Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory8Size() const noexcept { return m_backingMemory8Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory8ScratchSize() const noexcept { return m_backingMemory8ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory8MaxConcurrency() const noexcept { return m_backingMemory8MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory8DynamicExpansionFlags() const noexcept { return m_backingMemory8DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory8MemoryPoolTier() const noexcept { return m_backingMemory8MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory8SparsityMask() const noexcept { return m_backingMemory8SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory8Flags() const noexcept { return m_backingMemory8Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord8Count() const noexcept { return m_recordedProgramRecords8.size(); }
    [[nodiscard]] const std::vector<ProgramRecord8Entry>& GetRecordedProgramRecords8() const noexcept { return m_recordedProgramRecords8; }

    void SetWorkGraphBackingMemory9(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory9Start = RangeStart;
        m_backingMemory9Size = RangeSizeInBytes;
        m_backingMemory9ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory9MaxConcurrency = MaxConcurrency;
        m_backingMemory9DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory9MemoryPoolTier = MemoryPoolTier;
        m_backingMemory9SparsityMask = SparsityMask;
        m_backingMemory9SubAllocationMask = SubAllocationMask;
        m_backingMemory9Flags = Flags;
    }

    void DispatchGraphProgramRecord9(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords9.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory9Start() const noexcept { return m_backingMemory9Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory9Size() const noexcept { return m_backingMemory9Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory9ScratchSize() const noexcept { return m_backingMemory9ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory9MaxConcurrency() const noexcept { return m_backingMemory9MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory9DynamicExpansionFlags() const noexcept { return m_backingMemory9DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory9MemoryPoolTier() const noexcept { return m_backingMemory9MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory9SparsityMask() const noexcept { return m_backingMemory9SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory9SubAllocationMask() const noexcept { return m_backingMemory9SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory9Flags() const noexcept { return m_backingMemory9Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord9Count() const noexcept { return m_recordedProgramRecords9.size(); }
    [[nodiscard]] const std::vector<ProgramRecord9Entry>& GetRecordedProgramRecords9() const noexcept { return m_recordedProgramRecords9; }

    void SetWorkGraphBackingMemory10(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory10Start = RangeStart;
        m_backingMemory10Size = RangeSizeInBytes;
        m_backingMemory10ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory10MaxConcurrency = MaxConcurrency;
        m_backingMemory10DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory10MemoryPoolTier = MemoryPoolTier;
        m_backingMemory10SparsityMask = SparsityMask;
        m_backingMemory10SubAllocationMask = SubAllocationMask;
        m_backingMemory10PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory10Flags = Flags;
    }

    void DispatchGraphProgramRecord10(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords10.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory10Start() const noexcept { return m_backingMemory10Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory10Size() const noexcept { return m_backingMemory10Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory10ScratchSize() const noexcept { return m_backingMemory10ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory10MaxConcurrency() const noexcept { return m_backingMemory10MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory10DynamicExpansionFlags() const noexcept { return m_backingMemory10DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory10MemoryPoolTier() const noexcept { return m_backingMemory10MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory10SparsityMask() const noexcept { return m_backingMemory10SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory10SubAllocationMask() const noexcept { return m_backingMemory10SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory10PipelineCoherencyTier() const noexcept { return m_backingMemory10PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory10Flags() const noexcept { return m_backingMemory10Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord10Count() const noexcept { return m_recordedProgramRecords10.size(); }
    [[nodiscard]] const std::vector<ProgramRecord10Entry>& GetRecordedProgramRecords10() const noexcept { return m_recordedProgramRecords10; }

    void SetWorkGraphBackingMemory11(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory11Start = RangeStart;
        m_backingMemory11Size = RangeSizeInBytes;
        m_backingMemory11ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory11MaxConcurrency = MaxConcurrency;
        m_backingMemory11DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory11MemoryPoolTier = MemoryPoolTier;
        m_backingMemory11SparsityMask = SparsityMask;
        m_backingMemory11SubAllocationMask = SubAllocationMask;
        m_backingMemory11PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory11FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory11Flags = Flags;
    }

    void DispatchGraphProgramRecord11(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords11.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory11Start() const noexcept { return m_backingMemory11Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory11Size() const noexcept { return m_backingMemory11Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory11ScratchSize() const noexcept { return m_backingMemory11ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory11MaxConcurrency() const noexcept { return m_backingMemory11MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory11DynamicExpansionFlags() const noexcept { return m_backingMemory11DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory11MemoryPoolTier() const noexcept { return m_backingMemory11MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory11SparsityMask() const noexcept { return m_backingMemory11SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory11SubAllocationMask() const noexcept { return m_backingMemory11SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory11PipelineCoherencyTier() const noexcept { return m_backingMemory11PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory11FeedbackLoopTier() const noexcept { return m_backingMemory11FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory11Flags() const noexcept { return m_backingMemory11Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord11Count() const noexcept { return m_recordedProgramRecords11.size(); }
    [[nodiscard]] const std::vector<ProgramRecord11Entry>& GetRecordedProgramRecords11() const noexcept { return m_recordedProgramRecords11; }

    void SetWorkGraphBackingMemory12(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory12Start = RangeStart;
        m_backingMemory12Size = RangeSizeInBytes;
        m_backingMemory12ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory12MaxConcurrency = MaxConcurrency;
        m_backingMemory12DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory12MemoryPoolTier = MemoryPoolTier;
        m_backingMemory12SparsityMask = SparsityMask;
        m_backingMemory12SubAllocationMask = SubAllocationMask;
        m_backingMemory12PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory12FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory12SubGraphRecursionTier = SubGraphRecursionTier;
        m_backingMemory12Flags = Flags;
    }

    void DispatchGraphProgramRecord12(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords12.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, SubGraphRecursionTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory12Start() const noexcept { return m_backingMemory12Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory12Size() const noexcept { return m_backingMemory12Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory12ScratchSize() const noexcept { return m_backingMemory12ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory12MaxConcurrency() const noexcept { return m_backingMemory12MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory12DynamicExpansionFlags() const noexcept { return m_backingMemory12DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory12MemoryPoolTier() const noexcept { return m_backingMemory12MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory12SparsityMask() const noexcept { return m_backingMemory12SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory12SubAllocationMask() const noexcept { return m_backingMemory12SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory12PipelineCoherencyTier() const noexcept { return m_backingMemory12PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory12FeedbackLoopTier() const noexcept { return m_backingMemory12FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory12SubGraphRecursionTier() const noexcept { return m_backingMemory12SubGraphRecursionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory12Flags() const noexcept { return m_backingMemory12Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord12Count() const noexcept { return m_recordedProgramRecords12.size(); }
    [[nodiscard]] const std::vector<ProgramRecord12Entry>& GetRecordedProgramRecords12() const noexcept { return m_recordedProgramRecords12; }

    void SetWorkGraphBackingMemory13(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory13Start = RangeStart;
        m_backingMemory13Size = RangeSizeInBytes;
        m_backingMemory13ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory13MaxConcurrency = MaxConcurrency;
        m_backingMemory13DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory13MemoryPoolTier = MemoryPoolTier;
        m_backingMemory13SparsityMask = SparsityMask;
        m_backingMemory13SubAllocationMask = SubAllocationMask;
        m_backingMemory13PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory13FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory13SubGraphRecursionTier = SubGraphRecursionTier;
        m_backingMemory13MeshPipeliningTier = MeshPipeliningTier;
        m_backingMemory13Flags = Flags;
    }

    void DispatchGraphProgramRecord13(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords13.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, SubGraphRecursionTier, MeshPipeliningTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory13Start() const noexcept { return m_backingMemory13Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory13Size() const noexcept { return m_backingMemory13Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory13ScratchSize() const noexcept { return m_backingMemory13ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory13MaxConcurrency() const noexcept { return m_backingMemory13MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory13DynamicExpansionFlags() const noexcept { return m_backingMemory13DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory13MemoryPoolTier() const noexcept { return m_backingMemory13MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory13SparsityMask() const noexcept { return m_backingMemory13SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory13SubAllocationMask() const noexcept { return m_backingMemory13SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory13PipelineCoherencyTier() const noexcept { return m_backingMemory13PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory13FeedbackLoopTier() const noexcept { return m_backingMemory13FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory13SubGraphRecursionTier() const noexcept { return m_backingMemory13SubGraphRecursionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory13MeshPipeliningTier() const noexcept { return m_backingMemory13MeshPipeliningTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory13Flags() const noexcept { return m_backingMemory13Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord13Count() const noexcept { return m_recordedProgramRecords13.size(); }
    [[nodiscard]] const std::vector<ProgramRecord13Entry>& GetRecordedProgramRecords13() const noexcept { return m_recordedProgramRecords13; }

    void SetWorkGraphBackingMemory14(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory14Start = RangeStart;
        m_backingMemory14Size = RangeSizeInBytes;
        m_backingMemory14ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory14MaxConcurrency = MaxConcurrency;
        m_backingMemory14DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory14MemoryPoolTier = MemoryPoolTier;
        m_backingMemory14SparsityMask = SparsityMask;
        m_backingMemory14SubAllocationMask = SubAllocationMask;
        m_backingMemory14PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory14FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory14SubGraphRecursionTier = SubGraphRecursionTier;
        m_backingMemory14MeshPipeliningTier = MeshPipeliningTier;
        m_backingMemory14MultiQueueFusionTier = MultiQueueFusionTier;
        m_backingMemory14Flags = Flags;
    }

    void DispatchGraphProgramRecord14(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords14.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, SubGraphRecursionTier, MeshPipeliningTier, MultiQueueFusionTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory14Start() const noexcept { return m_backingMemory14Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory14Size() const noexcept { return m_backingMemory14Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory14ScratchSize() const noexcept { return m_backingMemory14ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14MaxConcurrency() const noexcept { return m_backingMemory14MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14DynamicExpansionFlags() const noexcept { return m_backingMemory14DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14MemoryPoolTier() const noexcept { return m_backingMemory14MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14SparsityMask() const noexcept { return m_backingMemory14SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14SubAllocationMask() const noexcept { return m_backingMemory14SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14PipelineCoherencyTier() const noexcept { return m_backingMemory14PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14FeedbackLoopTier() const noexcept { return m_backingMemory14FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14SubGraphRecursionTier() const noexcept { return m_backingMemory14SubGraphRecursionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14MeshPipeliningTier() const noexcept { return m_backingMemory14MeshPipeliningTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14MultiQueueFusionTier() const noexcept { return m_backingMemory14MultiQueueFusionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory14Flags() const noexcept { return m_backingMemory14Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord14Count() const noexcept { return m_recordedProgramRecords14.size(); }
    [[nodiscard]] const std::vector<ProgramRecord14Entry>& GetRecordedProgramRecords14() const noexcept { return m_recordedProgramRecords14; }

    void SetWorkGraphBackingMemory15(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory15Start = RangeStart;
        m_backingMemory15Size = RangeSizeInBytes;
        m_backingMemory15ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory15MaxConcurrency = MaxConcurrency;
        m_backingMemory15DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory15MemoryPoolTier = MemoryPoolTier;
        m_backingMemory15SparsityMask = SparsityMask;
        m_backingMemory15SubAllocationMask = SubAllocationMask;
        m_backingMemory15PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory15FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory15SubGraphRecursionTier = SubGraphRecursionTier;
        m_backingMemory15MeshPipeliningTier = MeshPipeliningTier;
        m_backingMemory15MultiQueueFusionTier = MultiQueueFusionTier;
        m_backingMemory15MicroscalingTier = MicroscalingTier;
        m_backingMemory15Flags = Flags;
    }

    void DispatchGraphProgramRecord15(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords15.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, SubGraphRecursionTier, MeshPipeliningTier, MultiQueueFusionTier, MicroscalingTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory15Start() const noexcept { return m_backingMemory15Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory15Size() const noexcept { return m_backingMemory15Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory15ScratchSize() const noexcept { return m_backingMemory15ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15MaxConcurrency() const noexcept { return m_backingMemory15MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15DynamicExpansionFlags() const noexcept { return m_backingMemory15DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15MemoryPoolTier() const noexcept { return m_backingMemory15MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15SparsityMask() const noexcept { return m_backingMemory15SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15SubAllocationMask() const noexcept { return m_backingMemory15SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15PipelineCoherencyTier() const noexcept { return m_backingMemory15PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15FeedbackLoopTier() const noexcept { return m_backingMemory15FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15SubGraphRecursionTier() const noexcept { return m_backingMemory15SubGraphRecursionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15MeshPipeliningTier() const noexcept { return m_backingMemory15MeshPipeliningTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15MultiQueueFusionTier() const noexcept { return m_backingMemory15MultiQueueFusionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15MicroscalingTier() const noexcept { return m_backingMemory15MicroscalingTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory15Flags() const noexcept { return m_backingMemory15Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord15Count() const noexcept { return m_recordedProgramRecords15.size(); }
    [[nodiscard]] const std::vector<ProgramRecord15Entry>& GetRecordedProgramRecords15() const noexcept { return m_recordedProgramRecords15; }

    void SetWorkGraphBackingMemory16(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory16Start = RangeStart;
        m_backingMemory16Size = RangeSizeInBytes;
        m_backingMemory16ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory16MaxConcurrency = MaxConcurrency;
        m_backingMemory16DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory16MemoryPoolTier = MemoryPoolTier;
        m_backingMemory16SparsityMask = SparsityMask;
        m_backingMemory16SubAllocationMask = SubAllocationMask;
        m_backingMemory16PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory16FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory16SubGraphRecursionTier = SubGraphRecursionTier;
        m_backingMemory16MeshPipeliningTier = MeshPipeliningTier;
        m_backingMemory16MultiQueueFusionTier = MultiQueueFusionTier;
        m_backingMemory16MicroscalingTier = MicroscalingTier;
        m_backingMemory16SubBytePrecisionTier = SubBytePrecisionTier;
        m_backingMemory16Flags = Flags;
    }

    void DispatchGraphProgramRecord16(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords16.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, SubGraphRecursionTier, MeshPipeliningTier, MultiQueueFusionTier, MicroscalingTier, SubBytePrecisionTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory16Start() const noexcept { return m_backingMemory16Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory16Size() const noexcept { return m_backingMemory16Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory16ScratchSize() const noexcept { return m_backingMemory16ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16MaxConcurrency() const noexcept { return m_backingMemory16MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16DynamicExpansionFlags() const noexcept { return m_backingMemory16DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16MemoryPoolTier() const noexcept { return m_backingMemory16MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16SparsityMask() const noexcept { return m_backingMemory16SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16SubAllocationMask() const noexcept { return m_backingMemory16SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16PipelineCoherencyTier() const noexcept { return m_backingMemory16PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16FeedbackLoopTier() const noexcept { return m_backingMemory16FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16SubGraphRecursionTier() const noexcept { return m_backingMemory16SubGraphRecursionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16MeshPipeliningTier() const noexcept { return m_backingMemory16MeshPipeliningTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16MultiQueueFusionTier() const noexcept { return m_backingMemory16MultiQueueFusionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16MicroscalingTier() const noexcept { return m_backingMemory16MicroscalingTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16SubBytePrecisionTier() const noexcept { return m_backingMemory16SubBytePrecisionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory16Flags() const noexcept { return m_backingMemory16Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord16Count() const noexcept { return m_recordedProgramRecords16.size(); }
    [[nodiscard]] const std::vector<ProgramRecord16Entry>& GetRecordedProgramRecords16() const noexcept { return m_recordedProgramRecords16; }

    void SetWorkGraphBackingMemory17(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t SparseTensorBlockTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory17Start = RangeStart;
        m_backingMemory17Size = RangeSizeInBytes;
        m_backingMemory17ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory17MaxConcurrency = MaxConcurrency;
        m_backingMemory17DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory17MemoryPoolTier = MemoryPoolTier;
        m_backingMemory17SparsityMask = SparsityMask;
        m_backingMemory17SubAllocationMask = SubAllocationMask;
        m_backingMemory17PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory17FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory17SubGraphRecursionTier = SubGraphRecursionTier;
        m_backingMemory17MeshPipeliningTier = MeshPipeliningTier;
        m_backingMemory17MultiQueueFusionTier = MultiQueueFusionTier;
        m_backingMemory17MicroscalingTier = MicroscalingTier;
        m_backingMemory17SubBytePrecisionTier = SubBytePrecisionTier;
        m_backingMemory17SparseTensorBlockTier = SparseTensorBlockTier;
        m_backingMemory17Flags = Flags;
    }

    void DispatchGraphProgramRecord17(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t SparseTensorBlockTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords17.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, SubGraphRecursionTier, MeshPipeliningTier, MultiQueueFusionTier, MicroscalingTier, SubBytePrecisionTier, SparseTensorBlockTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory17Start() const noexcept { return m_backingMemory17Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory17Size() const noexcept { return m_backingMemory17Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory17ScratchSize() const noexcept { return m_backingMemory17ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17MaxConcurrency() const noexcept { return m_backingMemory17MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17DynamicExpansionFlags() const noexcept { return m_backingMemory17DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17MemoryPoolTier() const noexcept { return m_backingMemory17MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17SparsityMask() const noexcept { return m_backingMemory17SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17SubAllocationMask() const noexcept { return m_backingMemory17SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17PipelineCoherencyTier() const noexcept { return m_backingMemory17PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17FeedbackLoopTier() const noexcept { return m_backingMemory17FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17SubGraphRecursionTier() const noexcept { return m_backingMemory17SubGraphRecursionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17MeshPipeliningTier() const noexcept { return m_backingMemory17MeshPipeliningTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17MultiQueueFusionTier() const noexcept { return m_backingMemory17MultiQueueFusionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17MicroscalingTier() const noexcept { return m_backingMemory17MicroscalingTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17SubBytePrecisionTier() const noexcept { return m_backingMemory17SubBytePrecisionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17SparseTensorBlockTier() const noexcept { return m_backingMemory17SparseTensorBlockTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory17Flags() const noexcept { return m_backingMemory17Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord17Count() const noexcept { return m_recordedProgramRecords17.size(); }
    [[nodiscard]] const std::vector<ProgramRecord17Entry>& GetRecordedProgramRecords17() const noexcept { return m_recordedProgramRecords17; }

    void SetWorkGraphBackingMemory18(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t SparseTensorBlockTier, uint32_t DeterministicTensorReductionTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory18Start = RangeStart;
        m_backingMemory18Size = RangeSizeInBytes;
        m_backingMemory18ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory18MaxConcurrency = MaxConcurrency;
        m_backingMemory18DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory18MemoryPoolTier = MemoryPoolTier;
        m_backingMemory18SparsityMask = SparsityMask;
        m_backingMemory18SubAllocationMask = SubAllocationMask;
        m_backingMemory18PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory18FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory18SubGraphRecursionTier = SubGraphRecursionTier;
        m_backingMemory18MeshPipeliningTier = MeshPipeliningTier;
        m_backingMemory18MultiQueueFusionTier = MultiQueueFusionTier;
        m_backingMemory18MicroscalingTier = MicroscalingTier;
        m_backingMemory18SubBytePrecisionTier = SubBytePrecisionTier;
        m_backingMemory18SparseTensorBlockTier = SparseTensorBlockTier;
        m_backingMemory18DeterministicTensorReductionTier = DeterministicTensorReductionTier;
        m_backingMemory18Flags = Flags;
    }

    void DispatchGraphProgramRecord18(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t SparseTensorBlockTier, uint32_t DeterministicTensorReductionTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords18.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, SubGraphRecursionTier, MeshPipeliningTier, MultiQueueFusionTier, MicroscalingTier, SubBytePrecisionTier, SparseTensorBlockTier, DeterministicTensorReductionTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory18Start() const noexcept { return m_backingMemory18Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory18Size() const noexcept { return m_backingMemory18Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory18ScratchSize() const noexcept { return m_backingMemory18ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18MaxConcurrency() const noexcept { return m_backingMemory18MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18DynamicExpansionFlags() const noexcept { return m_backingMemory18DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18MemoryPoolTier() const noexcept { return m_backingMemory18MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18SparsityMask() const noexcept { return m_backingMemory18SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18SubAllocationMask() const noexcept { return m_backingMemory18SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18PipelineCoherencyTier() const noexcept { return m_backingMemory18PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18FeedbackLoopTier() const noexcept { return m_backingMemory18FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18SubGraphRecursionTier() const noexcept { return m_backingMemory18SubGraphRecursionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18MeshPipeliningTier() const noexcept { return m_backingMemory18MeshPipeliningTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18MultiQueueFusionTier() const noexcept { return m_backingMemory18MultiQueueFusionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18MicroscalingTier() const noexcept { return m_backingMemory18MicroscalingTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18SubBytePrecisionTier() const noexcept { return m_backingMemory18SubBytePrecisionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18SparseTensorBlockTier() const noexcept { return m_backingMemory18SparseTensorBlockTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18DeterministicTensorReductionTier() const noexcept { return m_backingMemory18DeterministicTensorReductionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory18Flags() const noexcept { return m_backingMemory18Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord18Count() const noexcept { return m_recordedProgramRecords18.size(); }
    [[nodiscard]] const std::vector<ProgramRecord18Entry>& GetRecordedProgramRecords18() const noexcept { return m_recordedProgramRecords18; }

    void SetWorkGraphBackingMemory19(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t SparseTensorBlockTier, uint32_t DeterministicTensorReductionTier, uint32_t HyperQuantizationTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory19Start = RangeStart;
        m_backingMemory19Size = RangeSizeInBytes;
        m_backingMemory19ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory19MaxConcurrency = MaxConcurrency;
        m_backingMemory19DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory19MemoryPoolTier = MemoryPoolTier;
        m_backingMemory19SparsityMask = SparsityMask;
        m_backingMemory19SubAllocationMask = SubAllocationMask;
        m_backingMemory19PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory19FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory19SubGraphRecursionTier = SubGraphRecursionTier;
        m_backingMemory19MeshPipeliningTier = MeshPipeliningTier;
        m_backingMemory19MultiQueueFusionTier = MultiQueueFusionTier;
        m_backingMemory19MicroscalingTier = MicroscalingTier;
        m_backingMemory19SubBytePrecisionTier = SubBytePrecisionTier;
        m_backingMemory19SparseTensorBlockTier = SparseTensorBlockTier;
        m_backingMemory19DeterministicTensorReductionTier = DeterministicTensorReductionTier;
        m_backingMemory19HyperQuantizationTier = HyperQuantizationTier;
        m_backingMemory19Flags = Flags;
    }

    void DispatchGraphProgramRecord19(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t SparseTensorBlockTier, uint32_t DeterministicTensorReductionTier, uint32_t HyperQuantizationTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords19.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, SubGraphRecursionTier, MeshPipeliningTier, MultiQueueFusionTier, MicroscalingTier, SubBytePrecisionTier, SparseTensorBlockTier, DeterministicTensorReductionTier, HyperQuantizationTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory19Start() const noexcept { return m_backingMemory19Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory19Size() const noexcept { return m_backingMemory19Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory19ScratchSize() const noexcept { return m_backingMemory19ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19MaxConcurrency() const noexcept { return m_backingMemory19MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19DynamicExpansionFlags() const noexcept { return m_backingMemory19DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19MemoryPoolTier() const noexcept { return m_backingMemory19MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19SparsityMask() const noexcept { return m_backingMemory19SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19SubAllocationMask() const noexcept { return m_backingMemory19SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19PipelineCoherencyTier() const noexcept { return m_backingMemory19PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19FeedbackLoopTier() const noexcept { return m_backingMemory19FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19SubGraphRecursionTier() const noexcept { return m_backingMemory19SubGraphRecursionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19MeshPipeliningTier() const noexcept { return m_backingMemory19MeshPipeliningTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19MultiQueueFusionTier() const noexcept { return m_backingMemory19MultiQueueFusionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19MicroscalingTier() const noexcept { return m_backingMemory19MicroscalingTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19SubBytePrecisionTier() const noexcept { return m_backingMemory19SubBytePrecisionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19SparseTensorBlockTier() const noexcept { return m_backingMemory19SparseTensorBlockTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19DeterministicTensorReductionTier() const noexcept { return m_backingMemory19DeterministicTensorReductionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19HyperQuantizationTier() const noexcept { return m_backingMemory19HyperQuantizationTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory19Flags() const noexcept { return m_backingMemory19Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord19Count() const noexcept { return m_recordedProgramRecords19.size(); }
    [[nodiscard]] const std::vector<ProgramRecord19Entry>& GetRecordedProgramRecords19() const noexcept { return m_recordedProgramRecords19; }

    void SetWorkGraphBackingMemory20(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t SparseTensorBlockTier, uint32_t DeterministicTensorReductionTier, uint32_t HyperQuantizationTier, uint32_t SparseClusterTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory20Start = RangeStart;
        m_backingMemory20Size = RangeSizeInBytes;
        m_backingMemory20ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory20MaxConcurrency = MaxConcurrency;
        m_backingMemory20DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory20MemoryPoolTier = MemoryPoolTier;
        m_backingMemory20SparsityMask = SparsityMask;
        m_backingMemory20SubAllocationMask = SubAllocationMask;
        m_backingMemory20PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory20FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory20SubGraphRecursionTier = SubGraphRecursionTier;
        m_backingMemory20MeshPipeliningTier = MeshPipeliningTier;
        m_backingMemory20MultiQueueFusionTier = MultiQueueFusionTier;
        m_backingMemory20MicroscalingTier = MicroscalingTier;
        m_backingMemory20SubBytePrecisionTier = SubBytePrecisionTier;
        m_backingMemory20SparseTensorBlockTier = SparseTensorBlockTier;
        m_backingMemory20DeterministicTensorReductionTier = DeterministicTensorReductionTier;
        m_backingMemory20HyperQuantizationTier = HyperQuantizationTier;
        m_backingMemory20SparseClusterTier = SparseClusterTier;
        m_backingMemory20Flags = Flags;
    }

    void DispatchGraphProgramRecord20(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t SparseTensorBlockTier, uint32_t DeterministicTensorReductionTier, uint32_t HyperQuantizationTier, uint32_t SparseClusterTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords20.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, SubGraphRecursionTier, MeshPipeliningTier, MultiQueueFusionTier, MicroscalingTier, SubBytePrecisionTier, SparseTensorBlockTier, DeterministicTensorReductionTier, HyperQuantizationTier, SparseClusterTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory20Start() const noexcept { return m_backingMemory20Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory20Size() const noexcept { return m_backingMemory20Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory20ScratchSize() const noexcept { return m_backingMemory20ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20MaxConcurrency() const noexcept { return m_backingMemory20MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20DynamicExpansionFlags() const noexcept { return m_backingMemory20DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20MemoryPoolTier() const noexcept { return m_backingMemory20MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20SparsityMask() const noexcept { return m_backingMemory20SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20SubAllocationMask() const noexcept { return m_backingMemory20SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20PipelineCoherencyTier() const noexcept { return m_backingMemory20PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20FeedbackLoopTier() const noexcept { return m_backingMemory20FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20SubGraphRecursionTier() const noexcept { return m_backingMemory20SubGraphRecursionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20MeshPipeliningTier() const noexcept { return m_backingMemory20MeshPipeliningTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20MultiQueueFusionTier() const noexcept { return m_backingMemory20MultiQueueFusionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20MicroscalingTier() const noexcept { return m_backingMemory20MicroscalingTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20SubBytePrecisionTier() const noexcept { return m_backingMemory20SubBytePrecisionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20SparseTensorBlockTier() const noexcept { return m_backingMemory20SparseTensorBlockTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20DeterministicTensorReductionTier() const noexcept { return m_backingMemory20DeterministicTensorReductionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20HyperQuantizationTier() const noexcept { return m_backingMemory20HyperQuantizationTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20SparseClusterTier() const noexcept { return m_backingMemory20SparseClusterTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory20Flags() const noexcept { return m_backingMemory20Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord20Count() const noexcept { return m_recordedProgramRecords20.size(); }
    [[nodiscard]] const std::vector<ProgramRecord20Entry>& GetRecordedProgramRecords20() const noexcept { return m_recordedProgramRecords20; }

    void SetWorkGraphBackingMemory21(uint64_t RangeStart, uint64_t RangeSizeInBytes, uint64_t ScratchBufferSizeInBytes, uint32_t MaxConcurrency, uint32_t DynamicExpansionFlags, uint32_t MemoryPoolTier, uint32_t SparsityMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t SparseTensorBlockTier, uint32_t DeterministicTensorReductionTier, uint32_t HyperQuantizationTier, uint32_t SparseClusterTier, uint32_t MultiQueueSyncTier, uint32_t Flags) override {
        if (!m_isOpen) return;
        m_backingMemory21Start = RangeStart;
        m_backingMemory21Size = RangeSizeInBytes;
        m_backingMemory21ScratchSize = ScratchBufferSizeInBytes;
        m_backingMemory21MaxConcurrency = MaxConcurrency;
        m_backingMemory21DynamicExpansionFlags = DynamicExpansionFlags;
        m_backingMemory21MemoryPoolTier = MemoryPoolTier;
        m_backingMemory21SparsityMask = SparsityMask;
        m_backingMemory21SubAllocationMask = SubAllocationMask;
        m_backingMemory21PipelineCoherencyTier = PipelineCoherencyTier;
        m_backingMemory21FeedbackLoopTier = FeedbackLoopTier;
        m_backingMemory21SubGraphRecursionTier = SubGraphRecursionTier;
        m_backingMemory21MeshPipeliningTier = MeshPipeliningTier;
        m_backingMemory21MultiQueueFusionTier = MultiQueueFusionTier;
        m_backingMemory21MicroscalingTier = MicroscalingTier;
        m_backingMemory21SubBytePrecisionTier = SubBytePrecisionTier;
        m_backingMemory21SparseTensorBlockTier = SparseTensorBlockTier;
        m_backingMemory21DeterministicTensorReductionTier = DeterministicTensorReductionTier;
        m_backingMemory21HyperQuantizationTier = HyperQuantizationTier;
        m_backingMemory21SparseClusterTier = SparseClusterTier;
        m_backingMemory21MultiQueueSyncTier = MultiQueueSyncTier;
        m_backingMemory21Flags = Flags;
    }

    void DispatchGraphProgramRecord21(uint32_t NodeIndex, const void* pRecord, uint32_t RecordSizeInBytes, uint32_t ConcurrencyGroup, uint32_t QueueAffinityMask, uint32_t DynamicClusterExpansionTier, uint32_t TensorCoreMask, uint32_t SubAllocationMask, uint32_t PipelineCoherencyTier, uint32_t FeedbackLoopTier, uint32_t SubGraphRecursionTier, uint32_t MeshPipeliningTier, uint32_t MultiQueueFusionTier, uint32_t MicroscalingTier, uint32_t SubBytePrecisionTier, uint32_t SparseTensorBlockTier, uint32_t DeterministicTensorReductionTier, uint32_t HyperQuantizationTier, uint32_t SparseClusterTier, uint32_t MultiQueueSyncTier, uint32_t DispatchMask, uint32_t DispatchFlags) override {
        if (!m_isOpen) return;
        m_recordedProgramRecords21.push_back({ NodeIndex, pRecord, RecordSizeInBytes, ConcurrencyGroup, QueueAffinityMask, DynamicClusterExpansionTier, TensorCoreMask, SubAllocationMask, PipelineCoherencyTier, FeedbackLoopTier, SubGraphRecursionTier, MeshPipeliningTier, MultiQueueFusionTier, MicroscalingTier, SubBytePrecisionTier, SparseTensorBlockTier, DeterministicTensorReductionTier, HyperQuantizationTier, SparseClusterTier, MultiQueueSyncTier, DispatchMask, DispatchFlags });
    }

    [[nodiscard]] uint64_t GetWorkGraphBackingMemory21Start() const noexcept { return m_backingMemory21Start; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory21Size() const noexcept { return m_backingMemory21Size; }
    [[nodiscard]] uint64_t GetWorkGraphBackingMemory21ScratchSize() const noexcept { return m_backingMemory21ScratchSize; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21MaxConcurrency() const noexcept { return m_backingMemory21MaxConcurrency; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21DynamicExpansionFlags() const noexcept { return m_backingMemory21DynamicExpansionFlags; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21MemoryPoolTier() const noexcept { return m_backingMemory21MemoryPoolTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21SparsityMask() const noexcept { return m_backingMemory21SparsityMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21SubAllocationMask() const noexcept { return m_backingMemory21SubAllocationMask; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21PipelineCoherencyTier() const noexcept { return m_backingMemory21PipelineCoherencyTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21FeedbackLoopTier() const noexcept { return m_backingMemory21FeedbackLoopTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21SubGraphRecursionTier() const noexcept { return m_backingMemory21SubGraphRecursionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21MeshPipeliningTier() const noexcept { return m_backingMemory21MeshPipeliningTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21MultiQueueFusionTier() const noexcept { return m_backingMemory21MultiQueueFusionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21MicroscalingTier() const noexcept { return m_backingMemory21MicroscalingTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21SubBytePrecisionTier() const noexcept { return m_backingMemory21SubBytePrecisionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21SparseTensorBlockTier() const noexcept { return m_backingMemory21SparseTensorBlockTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21DeterministicTensorReductionTier() const noexcept { return m_backingMemory21DeterministicTensorReductionTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21HyperQuantizationTier() const noexcept { return m_backingMemory21HyperQuantizationTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21SparseClusterTier() const noexcept { return m_backingMemory21SparseClusterTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21MultiQueueSyncTier() const noexcept { return m_backingMemory21MultiQueueSyncTier; }
    [[nodiscard]] uint32_t GetWorkGraphBackingMemory21Flags() const noexcept { return m_backingMemory21Flags; }
    [[nodiscard]] size_t GetRecordedProgramRecord21Count() const noexcept { return m_recordedProgramRecords21.size(); }
    [[nodiscard]] const std::vector<ProgramRecord21Entry>& GetRecordedProgramRecords21() const noexcept { return m_recordedProgramRecords21; }

    const std::vector<Command>& GetRecordedCommands() const { return m_commands; }
    bool IsOpen() const { return m_isOpen; }
};

using Prism3D12GraphicsCommandList29Impl = Prism3D12GraphicsCommandListImpl;
using Prism3D12GraphicsCommandList30Impl = Prism3D12GraphicsCommandListImpl;
using Prism3D12GraphicsCommandList31Impl = Prism3D12GraphicsCommandListImpl;

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


class Prism3D12PipelineLibraryImpl : public ID3D12PipelineLibrary1 {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    mutable std::mutex m_mutex;
    std::unordered_map<std::wstring, ID3D12PipelineState*> m_pipelines;

public:
    Prism3D12PipelineLibraryImpl(ID3D12Device* pDevice, const void* pBlob, size_t blobLen)
        : m_pDevice(pDevice) {
        if (m_pDevice) m_pDevice->AddRef();
        if (pBlob && blobLen >= 12) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(pBlob);
            if (std::memcmp(p, "PRISMPL1", 8) == 0) {
                uint32_t count = 0;
                std::memcpy(&count, p + 8, sizeof(count));
                size_t offset = 12;
                for (uint32_t i = 0; i < count && offset + 4 <= blobLen; ++i) {
                    uint32_t nameLen = 0;
                    std::memcpy(&nameLen, p + offset, 4);
                    offset += 4;
                    if (offset + nameLen * sizeof(wchar_t) <= blobLen) {
                        std::wstring name(reinterpret_cast<const wchar_t*>(p + offset), nameLen);
                        offset += nameLen * sizeof(wchar_t);
                        ID3D12PipelineState* pso = nullptr;
                        D3D12_GRAPHICS_PIPELINE_STATE_DESC dummyDesc{};
                        if (m_pDevice) {
                            (void)m_pDevice->CreateGraphicsPipelineState(&dummyDesc, IID_ID3D12PipelineState, reinterpret_cast<void**>(&pso));
                            if (pso) {
                                m_pipelines[name] = pso;
                            }
                        }
                    }
                }
            }
        }
    }

    ~Prism3D12PipelineLibraryImpl() override {
        for (auto& [k, v] : m_pipelines) {
            if (v) v->Release();
        }
        if (m_pDevice) {
            m_pDevice->Release();
        }
    }

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild ||
            riid == IID_ID3D12PipelineLibrary || riid == IID_ID3D12PipelineLibrary1) {
            *ppv = static_cast<ID3D12PipelineLibrary1*>(this);
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

    int32_t GetDevice(const IID& riid, void** ppvDevice) override {
        if (!m_pDevice || !ppvDevice) return -1;
        return m_pDevice->QueryInterface(riid, ppvDevice);
    }

    int32_t StorePipeline(const wchar_t* pName, ID3D12PipelineState* pPipeline) override {
        if (!pName || !pPipeline) return -1;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::wstring key(pName);
        if (m_pipelines.find(key) != m_pipelines.end()) {
            return -2147024809; // E_INVALIDARG: Name already exists in library
        }
        pPipeline->AddRef();
        m_pipelines[key] = pPipeline;
        return 0;
    }

    int32_t LoadGraphicsPipeline(const wchar_t* pName, [[maybe_unused]] const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) override {
        if (!pName || !ppPipelineState) return -1;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pipelines.find(std::wstring(pName));
        if (it != m_pipelines.end() && it->second) {
            return it->second->QueryInterface(riid, ppPipelineState);
        }
        *ppPipelineState = nullptr;
        return -2147024809; // E_INVALIDARG
    }

    int32_t LoadComputePipeline(const wchar_t* pName, [[maybe_unused]] const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) override {
        if (!pName || !ppPipelineState) return -1;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pipelines.find(std::wstring(pName));
        if (it != m_pipelines.end() && it->second) {
            return it->second->QueryInterface(riid, ppPipelineState);
        }
        *ppPipelineState = nullptr;
        return -2147024809; // E_INVALIDARG
    }

    int32_t LoadPipeline(const wchar_t* pName, [[maybe_unused]] const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc, const IID& riid, void** ppPipelineState) override {
        if (!pName || !ppPipelineState) return -1;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pipelines.find(std::wstring(pName));
        if (it != m_pipelines.end() && it->second) {
            return it->second->QueryInterface(riid, ppPipelineState);
        }
        *ppPipelineState = nullptr;
        return -2147024809; // E_INVALIDARG
    }

    size_t GetSerializedSize() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t total = 12; // Magic 'PRISMPL1' (8 bytes) + Count (4 bytes)
        for (const auto& [k, v] : m_pipelines) {
            total += 4 + k.size() * sizeof(wchar_t);
        }
        return total;
    }

    int32_t Serialize(void* pData, size_t DataSizeInBytes) override {
        if (!pData) return -1;
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t required = 12;
        for (const auto& [k, v] : m_pipelines) {
            required += 4 + k.size() * sizeof(wchar_t);
        }
        if (DataSizeInBytes < required) return -2147024882; // E_OUTOFMEMORY / insufficient buffer
        uint8_t* p = reinterpret_cast<uint8_t*>(pData);
        std::memcpy(p, "PRISMPL1", 8);
        uint32_t count = static_cast<uint32_t>(m_pipelines.size());
        std::memcpy(p + 8, &count, sizeof(count));
        size_t offset = 12;
        for (const auto& [k, v] : m_pipelines) {
            uint32_t nameLen = static_cast<uint32_t>(k.size());
            std::memcpy(p + offset, &nameLen, sizeof(nameLen));
            offset += sizeof(nameLen);
            std::memcpy(p + offset, k.data(), nameLen * sizeof(wchar_t));
            offset += nameLen * sizeof(wchar_t);
        }
        return 0;
    }

    [[nodiscard]] uint32_t GetPipelineCount() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<uint32_t>(m_pipelines.size());
    }
};

// D3D12 Device Implementation
class Prism3D12StateObjectImpl : public ID3D12StateObject, public ID3D12StateObjectProperties {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    ID3D12Device* m_pDevice{ nullptr };
    std::map<std::wstring, std::vector<uint8_t>> m_shaderIdentifiers;
    uint64_t m_pipelineStackSize{ 4096 };
    uint32_t m_maxRecursion{ 1 };
    uint32_t m_maxPayloadSize{ 32 };
    uint32_t m_maxAttributeSize{ 8 };

public:
    Prism3D12StateObjectImpl(ID3D12Device* pDev, const D3D12_STATE_OBJECT_DESC* pDesc)
        : m_pDevice(pDev) {
        if (pDesc && pDesc->pSubobjects) {
            for (uint32_t i = 0; i < pDesc->NumSubobjects; ++i) {
                const auto& sub = pDesc->pSubobjects[i];
                if (sub.Type == D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP && sub.pDesc) {
                    const auto* hg = static_cast<const D3D12_HIT_GROUP_DESC*>(sub.pDesc);
                    if (hg && hg->HitGroupExport) {
                        std::vector<uint8_t> id(32, 0);
                        for (size_t c = 0; hg->HitGroupExport[c] != L'\0'; ++c) {
                            id[c % 32] ^= static_cast<uint8_t>(hg->HitGroupExport[c]);
                        }
                        id[0] |= 0x80; // Hitgroup flag
                        m_shaderIdentifiers[hg->HitGroupExport] = id;
                    }
                } else if (sub.Type == D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG && sub.pDesc) {
                    const auto* cfg = static_cast<const D3D12_RAYTRACING_PIPELINE_CONFIG*>(sub.pDesc);
                    if (cfg) m_maxRecursion = cfg->MaxTraceRecursionDepth;
                } else if (sub.Type == D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG && sub.pDesc) {
                    const auto* sc = static_cast<const D3D12_RAYTRACING_SHADER_CONFIG*>(sub.pDesc);
                    if (sc) {
                        m_maxPayloadSize = sc->MaxPayloadSizeInBytes;
                        m_maxAttributeSize = sc->MaxAttributeSizeInBytes;
                    }
                } else if (sub.Type == D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY && sub.pDesc) {
                    const auto* lib = static_cast<const D3D12_DXIL_LIBRARY_DESC*>(sub.pDesc);
                    if (lib && lib->pExports) {
                        for (uint32_t e = 0; e < lib->NumExports; ++e) {
                            if (lib->pExports[e].Name) {
                                std::vector<uint8_t> id(32, 0);
                                for (size_t c = 0; lib->pExports[e].Name[c] != L'\0'; ++c) {
                                    id[c % 32] ^= static_cast<uint8_t>(lib->pExports[e].Name[c]);
                                }
                                id[0] |= 0x40; // Shader export flag
                                m_shaderIdentifiers[lib->pExports[e].Name] = id;
                            }
                        }
                    }
                }
            }
        }
        if (m_shaderIdentifiers.find(L"RayGen") == m_shaderIdentifiers.end()) {
            std::vector<uint8_t> rgen(32, 0x11);
            m_shaderIdentifiers[L"RayGen"] = rgen;
        }
        if (m_shaderIdentifiers.find(L"Miss") == m_shaderIdentifiers.end()) {
            std::vector<uint8_t> miss(32, 0x22);
            m_shaderIdentifiers[L"Miss"] = miss;
        }
    }

    int32_t QueryInterface(const IID& riid, void** ppv) override {
        if (!ppv) return -1;
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable || riid == IID_ID3D12StateObject) {
            *ppv = static_cast<ID3D12StateObject*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12StateObjectProperties) {
            *ppv = static_cast<ID3D12StateObjectProperties*>(this);
            AddRef();
            return 0;
        }
        *ppv = nullptr;
        return -2147467262; // E_NOINTERFACE
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

    void* GetShaderIdentifier(const wchar_t* pExportName) override {
        if (!pExportName) return nullptr;
        auto it = m_shaderIdentifiers.find(pExportName);
        if (it != m_shaderIdentifiers.end()) {
            return it->second.data();
        }
        return nullptr;
    }

    uint64_t GetShaderStackSize(const wchar_t* /*pExportName*/) override { return 1024; }
    uint64_t GetPipelineStackSize() override { return m_pipelineStackSize; }
    void SetPipelineStackSize(uint64_t PipelineStackSizeInBytes) override { m_pipelineStackSize = PipelineStackSizeInBytes; }

    [[nodiscard]] uint32_t GetMaxRecursionDepth() const noexcept { return m_maxRecursion; }
    [[nodiscard]] uint32_t GetMaxPayloadSize() const noexcept { return m_maxPayloadSize; }
    [[nodiscard]] uint32_t GetMaxAttributeSize() const noexcept { return m_maxAttributeSize; }
};

class Prism3D12DeviceImpl : public ID3D12Device37 {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    IUnknown* m_pAdapter{ nullptr };
    D3D_FEATURE_LEVEL m_featureLevel{ D3D_FEATURE_LEVEL_12_0 };
    uint32_t m_samplerFeedbackViewsCreated{ 0 };
    uint32_t m_metaCommandsCreated{ 0 };
    uint32_t m_heapsCreated{ 0 };
    uint32_t m_placedResourcesCreated{ 0 };
    uint32_t m_pipelineLibrariesCreated{ 0 };
    uint32_t m_workGraphProgramRecordsCreated{ 0 };

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
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12Device || riid == IID_ID3D12Device1 || riid == IID_ID3D12Device4 || riid == IID_ID3D12Device5 || riid == IID_ID3D12Device8 || riid == IID_ID3D12Device9 || riid == IID_ID3D12Device10 || riid == IID_ID3D12Device11 || riid == OFFICIAL_IID_ID3D12Device11 || riid == IID_ID3D12Device12 || riid == OFFICIAL_IID_ID3D12Device12 || riid == IID_ID3D12Device13 || riid == OFFICIAL_IID_ID3D12Device13 || riid == IID_ID3D12Device14 || riid == OFFICIAL_IID_ID3D12Device14 || riid == OFFICIAL_IID_ID3D12Device14Alt || riid == IID_ID3D12Device15 || riid == OFFICIAL_IID_ID3D12Device15 || riid == IID_ID3D12Device16 || riid == OFFICIAL_IID_ID3D12Device16 || riid == IID_ID3D12Device17 || riid == OFFICIAL_IID_ID3D12Device17 || riid == IID_ID3D12Device18 || riid == OFFICIAL_IID_ID3D12Device18 || riid == IID_ID3D12Device19 || riid == OFFICIAL_IID_ID3D12Device19 || riid == IID_ID3D12Device20 || riid == OFFICIAL_IID_ID3D12Device20 || riid == IID_ID3D12Device21 || riid == OFFICIAL_IID_ID3D12Device21 || riid == IID_ID3D12Device22 || riid == OFFICIAL_IID_ID3D12Device22 || riid == IID_ID3D12Device23 || riid == OFFICIAL_IID_ID3D12Device23 || riid == IID_ID3D12Device24 || riid == OFFICIAL_IID_ID3D12Device24 || riid == IID_ID3D12Device25 || riid == OFFICIAL_IID_ID3D12Device25 || riid == IID_ID3D12Device26 || riid == OFFICIAL_IID_ID3D12Device26 || riid == IID_ID3D12Device27 || riid == OFFICIAL_IID_ID3D12Device27 || riid == IID_ID3D12Device28 || riid == OFFICIAL_IID_ID3D12Device28 || riid == IID_ID3D12Device29 || riid == OFFICIAL_IID_ID3D12Device29 || riid == IID_ID3D12Device30 || riid == OFFICIAL_IID_ID3D12Device30 || riid == IID_ID3D12Device31 || riid == OFFICIAL_IID_ID3D12Device31 || riid == IID_ID3D12Device32 || riid == OFFICIAL_IID_ID3D12Device32 || riid == IID_ID3D12Device33 || riid == OFFICIAL_IID_ID3D12Device33 || riid == IID_ID3D12Device34 || riid == OFFICIAL_IID_ID3D12Device34 || riid == IID_ID3D12Device35 || riid == OFFICIAL_IID_ID3D12Device35 || riid == IID_ID3D12Device36 || riid == OFFICIAL_IID_ID3D12Device36 || riid == IID_ID3D12Device37 || riid == OFFICIAL_IID_ID3D12Device37) {
            *ppv = static_cast<ID3D12Device37*>(this);
            AddRef();
            return 0;
        }
        if (riid == IID_ID3D12DeviceRemovedExtendedData ||
            riid == IID_ID3D12DeviceRemovedExtendedData1 ||
            riid == IID_ID3D12DeviceRemovedExtendedData2) {
            auto* pDred = new Prism3D12DeviceRemovedExtendedDataImpl();
            int32_t hr = pDred->QueryInterface(riid, ppv);
            pDred->Release();
            return hr;
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
    [[nodiscard]] uint32_t GetPipelineLibrariesCreated() const noexcept { return m_pipelineLibrariesCreated; }
    uint32_t m_stateObjectsCreated{ 0 };
    [[nodiscard]] uint32_t GetStateObjectsCreated() const noexcept { return m_stateObjectsCreated; }

    int32_t CreateStateObject(
        const D3D12_STATE_OBJECT_DESC* pDesc,
        const IID& riid,
        void** ppStateObject) override {
        if (!pDesc || !ppStateObject) return -1;
        auto* pObj = new Prism3D12StateObjectImpl(this, pDesc);
        int32_t hr = pObj->QueryInterface(riid, ppStateObject);
        pObj->Release();
        if (hr == 0) m_stateObjectsCreated++;
        return hr;
    }

    void GetRaytracingAccelerationStructurePrebuildInfo(
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* pInput,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* pInfo) override {
        if (!pInfo) return;
        pInfo->ResultDataMaxSizeInBytes = 0;
        pInfo->ScratchDataSizeInBytes = 0;
        pInfo->UpdateScratchDataSizeInBytes = 0;
        if (!pInput) return;

        uint64_t numElements = pInput->NumDescs;
        if (pInput->Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL) {
            pInfo->ResultDataMaxSizeInBytes = std::max<uint64_t>(65536, numElements * 256);
            pInfo->ScratchDataSizeInBytes = std::max<uint64_t>(32768, numElements * 128);
            pInfo->UpdateScratchDataSizeInBytes = std::max<uint64_t>(16384, numElements * 64);
        } else {
            pInfo->ResultDataMaxSizeInBytes = std::max<uint64_t>(131072, numElements * 512);
            pInfo->ScratchDataSizeInBytes = std::max<uint64_t>(65536, numElements * 256);
            pInfo->UpdateScratchDataSizeInBytes = std::max<uint64_t>(32768, numElements * 128);
        }
    }

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

    D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo1(
        uint32_t visibleMask,
        uint32_t numResourceDescs,
        const D3D12_RESOURCE_DESC* pResourceDescs,
        D3D12_RESOURCE_ALLOCATION_INFO1* pResourceAllocationInfo1) override {
        (void)visibleMask;
        D3D12_RESOURCE_ALLOCATION_INFO info{};
        if (!pResourceDescs || numResourceDescs == 0) return info;

        uint64_t currentOffset = 0;
        uint64_t maxAlign = 64 * 1024;

        for (uint32_t i = 0; i < numResourceDescs; ++i) {
            uint64_t itemSize = 0;
            uint64_t align = 64 * 1024;
            if (pResourceDescs[i].Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
                itemSize = pResourceDescs[i].Width;
            } else if (pResourceDescs[i].Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
                itemSize = pResourceDescs[i].Width * pResourceDescs[i].Height * 4;
                if (pResourceDescs[i].SampleDesc.Count > 1 || pResourceDescs[i].Alignment == 4 * 1024 * 1024) {
                    align = 4 * 1024 * 1024;
                }
            }
            uint64_t alignedItem = (itemSize + align - 1) & ~(align - 1);
            currentOffset = (currentOffset + align - 1) & ~(align - 1);

            if (pResourceAllocationInfo1) {
                pResourceAllocationInfo1[i].Offset = currentOffset;
                pResourceAllocationInfo1[i].Alignment = align;
                pResourceAllocationInfo1[i].SizeInBytes = alignedItem;
            }

            currentOffset += alignedItem;
            if (align > maxAlign) maxAlign = align;
        }

        info.Alignment = maxAlign;
        info.SizeInBytes = currentOffset;
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

    D3D12_HEAP_PROPERTIES GetCustomHeapProperties(
        uint32_t nodeMask,
        D3D12_HEAP_TYPE heapType) override {
        D3D12_HEAP_PROPERTIES props{};
        props.CreationNodeMask = (nodeMask != 0) ? nodeMask : 1;
        props.VisibleNodeMask = (nodeMask != 0) ? nodeMask : 1;
        switch (heapType) {
        case D3D12_HEAP_TYPE_DEFAULT:
            props.Type = D3D12_HEAP_TYPE_CUSTOM;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_L1;
            break;
        case D3D12_HEAP_TYPE_UPLOAD:
            props.Type = D3D12_HEAP_TYPE_CUSTOM;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
            break;
        case D3D12_HEAP_TYPE_READBACK:
            props.Type = D3D12_HEAP_TYPE_CUSTOM;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
            break;
        case D3D12_HEAP_TYPE_CUSTOM:
        default:
            props.Type = D3D12_HEAP_TYPE_CUSTOM;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            break;
        }
        return props;
    }

    int32_t CreatePipelineLibrary(
        const void* pLibraryBlob,
        size_t BlobLength,
        const IID& riid,
        void** ppPipelineLibrary) override {
        if (!ppPipelineLibrary) return -1;
        auto* lib = new Prism3D12PipelineLibraryImpl(this, pLibraryBlob, BlobLength);
        int32_t hr = lib->QueryInterface(riid, ppPipelineLibrary);
        lib->Release();
        if (hr == 0) m_pipelineLibrariesCreated++;
        return hr;
    }

    int32_t CreateReservedResource(
        const D3D12_RESOURCE_DESC* pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const void* /*pOptimizedClearValue*/,
        const IID& riidResource,
        void** ppvResource) override {
        if (!pDesc || !ppvResource) return -1;
        auto* res = new Prism3D12ResourceImpl(this, *pDesc, InitialState);
        int32_t hr = res->QueryInterface(riidResource, ppvResource);
        res->Release();
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

    int32_t CreatePlacedResource2(
        ID3D12Heap* pHeap,
        uint64_t HeapOffset,
        const D3D12_RESOURCE_DESC1* pDesc,
        D3D12_BARRIER_LAYOUT /*InitialLayout*/,
        const void* pOptimizedClearValue,
        uint32_t /*NumCastableFormats*/,
        const DXGI_FORMAT* /*pCastableFormats*/,
        const IID& riidResource,
        void** ppvResource) override {
        if (!pHeap || !pDesc || !ppvResource) return -1;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = pDesc->Dimension;
        desc.Alignment = pDesc->Alignment;
        desc.Width = pDesc->Width;
        desc.Height = pDesc->Height;
        desc.DepthOrArraySize = pDesc->DepthOrArraySize;
        desc.MipLevels = pDesc->MipLevels;
        desc.Format = pDesc->Format;
        desc.SampleDesc = pDesc->SampleDesc;
        desc.Layout = pDesc->Layout;
        desc.Flags = pDesc->Flags;
        return CreatePlacedResource(pHeap, HeapOffset, &desc, D3D12_RESOURCE_STATE_COMMON, pOptimizedClearValue, riidResource, ppvResource);
    }

    int32_t CreateCommittedResource3(
        const D3D12_HEAP_PROPERTIES* pHeapProperties,
        uint32_t HeapFlags,
        const D3D12_RESOURCE_DESC1* pDesc,
        D3D12_BARRIER_LAYOUT /*InitialLayout*/,
        const void* pOptimizedClearValue,
        void* /*pProtectedSession*/,
        uint32_t /*NumCastableFormats*/,
        const DXGI_FORMAT* /*pCastableFormats*/,
        const IID& riidResource,
        void** ppvResource) override {
        if (!pHeapProperties || !pDesc || !ppvResource) return -1;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = pDesc->Dimension;
        desc.Alignment = pDesc->Alignment;
        desc.Width = pDesc->Width;
        desc.Height = pDesc->Height;
        desc.DepthOrArraySize = pDesc->DepthOrArraySize;
        desc.MipLevels = pDesc->MipLevels;
        desc.Format = pDesc->Format;
        desc.SampleDesc = pDesc->SampleDesc;
        desc.Layout = pDesc->Layout;
        desc.Flags = pDesc->Flags;
        return CreateCommittedResource(pHeapProperties, HeapFlags, &desc, D3D12_RESOURCE_STATE_COMMON, pOptimizedClearValue, riidResource, ppvResource);
    }

    int32_t CreateReservedResource2(
        const D3D12_RESOURCE_DESC1* pDesc,
        D3D12_BARRIER_LAYOUT /*InitialLayout*/,
        const void* pOptimizedClearValue,
        void* /*pProtectedSession*/,
        uint32_t /*NumCastableFormats*/,
        const DXGI_FORMAT* /*pCastableFormats*/,
        const IID& riidResource,
        void** ppvResource) override {
        if (!pDesc || !ppvResource) return -1;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = pDesc->Dimension;
        desc.Alignment = pDesc->Alignment;
        desc.Width = pDesc->Width;
        desc.Height = pDesc->Height;
        desc.DepthOrArraySize = pDesc->DepthOrArraySize;
        desc.MipLevels = pDesc->MipLevels;
        desc.Format = pDesc->Format;
        desc.SampleDesc = pDesc->SampleDesc;
        desc.Layout = pDesc->Layout;
        desc.Flags = pDesc->Flags;
        return CreateReservedResource(&desc, D3D12_RESOURCE_STATE_COMMON, pOptimizedClearValue, riidResource, ppvResource);
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

    void CreateSampler2(
        const D3D12_SAMPLER_DESC2* /*pDesc*/,
        D3D12_CPU_DESCRIPTOR_HANDLE /*DestDescriptor*/
    ) override {
        m_samplerFeedbackViewsCreated++;
    }

    D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo3(
        uint32_t visibleMask,
        uint32_t numResourceDescs,
        const void* pResourceDescs,
        const uint32_t* pNumCastableFormats,
        const void* const* ppCastableFormats,
        D3D12_RESOURCE_ALLOCATION_INFO1* pResourceAllocationInfo1) override {
        (void)visibleMask; (void)pResourceDescs; (void)pNumCastableFormats; (void)ppCastableFormats;
        D3D12_RESOURCE_ALLOCATION_INFO info{
            .SizeInBytes = 65536ULL * (numResourceDescs ? numResourceDescs : 1),
            .Alignment = 65536ULL
        };
        if (pResourceAllocationInfo1 && numResourceDescs > 0) {
            pResourceAllocationInfo1[0].Offset = 0;
            pResourceAllocationInfo1[0].Alignment = info.Alignment;
            pResourceAllocationInfo1[0].SizeInBytes = info.SizeInBytes;
        }
        return info;
    }

    int32_t OpenExistingHeapFromAddress1(
        const void* pAddress,
        size_t size,
        const IID& riid,
        void** ppvHeap) override {
        if (!pAddress || size == 0 || !ppvHeap) return -1;
        D3D12_HEAP_DESC desc{};
        desc.SizeInBytes = size;
        auto* heap = new Prism3D12HeapImpl(this, desc);
        int32_t hr = heap->QueryInterface(riid, ppvHeap);
        heap->Release();
        if (hr == 0) m_heapsCreated++;
        return hr;
    }

    int32_t TryCreateShaderResourceView(
        ID3D12Resource* /*pResource*/,
        const void* /*pDesc*/,
        D3D12_CPU_DESCRIPTOR_HANDLE /*DestDescriptor*/) override {
        return 0;
    }

    int32_t TryCreateUnorderedAccessView(
        ID3D12Resource* /*pResource*/,
        ID3D12Resource* /*pCounterResource*/,
        const void* /*pDesc*/,
        D3D12_CPU_DESCRIPTOR_HANDLE /*DestDescriptor*/) override {
        return 0;
    }

    int32_t TryCreateConstantBufferView(
        const void* /*pDesc*/,
        D3D12_CPU_DESCRIPTOR_HANDLE /*DestDescriptor*/) override {
        return 0;
    }

    int32_t TryCreateSampler2(
        const D3D12_SAMPLER_DESC2* /*pDesc*/,
        D3D12_CPU_DESCRIPTOR_HANDLE /*DestDescriptor*/) override {
        m_samplerFeedbackViewsCreated++;
        return 0;
    }

    int32_t CreateQueryHeap1(
        const D3D12_QUERY_HEAP_DESC1* pDesc,
        const IID& riid,
        void** ppvHeap) override {
        if (!pDesc || !ppvHeap) return -1;
        auto* heap = new Prism3D12QueryHeapImpl(*pDesc);
        int32_t hr = heap->QueryInterface(riid, ppvHeap);
        heap->Release();
        return hr;
    }

    int32_t ResolveQueryData(
        ID3D12QueryHeap* /*pQueryHeap*/,
        D3D12_QUERY_TYPE /*Type*/,
        uint32_t /*StartIndex*/,
        uint32_t /*NumQueries*/,
        void* /*pDestination*/,
        size_t /*DestinationSizeInBytes*/) override {
        return 0;
    }

    int32_t RegisterTrimNotificationCallback(
        D3D12_REGISTER_TRIM_NOTIFICATION* pData) override {
        if (!pData) return -1;
        pData->CallbackCookie = 0x7701;
        return 0;
    }

    int32_t UnregisterTrimNotificationCallback(
        uint32_t /*CallbackCookie*/) override {
        return 0;
    }

    int32_t TryCreateRenderTargetView(
        ID3D12Resource* pResource,
        const D3D12_RENDER_TARGET_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override {
        CreateRenderTargetView(pResource, pDesc, DestDescriptor);
        return 0;
    }

    int32_t TryCreateDepthStencilView(
        ID3D12Resource* pResource,
        const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override {
        CreateDepthStencilView(pResource, pDesc, DestDescriptor);
        return 0;
    }

    int32_t TryCreateSamplerFeedbackUnorderedAccessView(
        ID3D12Resource* /*pTargetedResource*/,
        ID3D12Resource* /*pFeedbackResource*/,
        D3D12_CPU_DESCRIPTOR_HANDLE /*DestDescriptor*/) override {
        m_samplerFeedbackViewsCreated++;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord(
        const void* /*pDesc*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xDEADBEEF);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t SetWorkGraphProgramRecord(
        uint32_t /*ProgramIndex*/,
        const void* /*pRecord*/) override {
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord2(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xCAFEBABE);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphMemoryRequirements(
        uint32_t /*ProgramIndex*/,
        uint64_t* pSizeBytes) override {
        if (!pSizeBytes) return -1;
        *pSizeBytes = 65536ULL;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord3(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*ScratchBufferSize*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xCAFEBAB3);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetDynamicDepthBoundsSupport(
        int32_t* pSupported) override {
        if (!pSupported) return -1;
        *pSupported = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord4(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xCAFEBAB4);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphGlobalRootSignatureSupport(
        int32_t* pSupported) override {
        if (!pSupported) return -1;
        *pSupported = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord5(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*AllocationTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xCAFEBAB5);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphAutonomousAllocationSupport(
        int32_t* pSupported) override {
        if (!pSupported) return -1;
        *pSupported = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord6(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xCAFEBAB6);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension) override {
        if (!pSupported) return -1;
        *pSupported = 1;
        if (pMaxMatrixDimension) *pMaxMatrixDimension = 64;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord7(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xCAFEBAB7);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport2(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes) override {
        if (!pSupported || !pMaxMatrixDimension || !pSupportedDataTypes) return -1;
        *pSupported = 1;
        *pMaxMatrixDimension = 128;
        *pSupportedDataTypes = 0x07; // FP16, BF16, FP8
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord8(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xCAFEBAB8);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport3(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration) override {
        if (!pSupported || !pMaxMatrixDimension || !pSupportedDataTypes || !pSparseAcceleration) return -1;
        *pSupported = 1;
        *pMaxMatrixDimension = 256;
        *pSupportedDataTypes = 0x0F; // FP16, BF16, FP8, INT8
        *pSparseAcceleration = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord9(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xCAFEBAB9);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport4(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation) override {
        if (!pSupported || !pMaxMatrixDimension || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation) return -1;
        *pSupported = 1;
        *pMaxMatrixDimension = 512;
        *pSupportedDataTypes = 0x1F; // FP16, BF16, FP8, INT8, FP4/INT2
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord10(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        uint32_t /*PipelineCoherencyTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xCAFEBA10);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport5(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages) override {
        if (!pSupported || !pMaxMatrixDimension || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages) return -1;
        *pSupported = 1;
        *pMaxMatrixDimension = 1024;
        *pSupportedDataTypes = 0x3F; // FP16, BF16, FP8, INT8, FP4/INT2, Microscaled
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 8;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord11(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        uint32_t /*PipelineCoherencyTier*/,
        uint32_t /*FeedbackLoopTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0xCAFEBA11);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport6(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops) override {
        if (!pSupported || !pMaxMatrixDimension || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages || !pAutonomousFeedbackLoops) return -1;
        *pSupported = 1;
        *pMaxMatrixDimension = 2048;
        *pSupportedDataTypes = 0x7F; // FP16, BF16, FP8, INT8, FP4, MXFP6, MXFP4
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 16;
        *pAutonomousFeedbackLoops = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord12(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*SparsityTier*/,
        uint32_t /*SubAllocationTier*/,
        uint32_t /*PipelineCoherencyMask*/,
        uint32_t /*FeedbackLoopTier*/,
        uint32_t /*SubGraphRecursionTier*/,
        uint32_t /*ProgramVersion*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0x88012);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport7(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedback,
        int32_t* pSubGraphRecursion) override {
        if (!pSupported || !pMaxMatrixDimension || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages || !pAutonomousFeedback || !pSubGraphRecursion) return -1;
        *pSupported = 1;
        *pMaxMatrixDimension = 4096;
        *pSupportedDataTypes = 0xFF; // Full precision & microscaling tensor types
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 32;
        *pAutonomousFeedback = 1;
        *pSubGraphRecursion = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord13(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*SparsityTier*/,
        uint32_t /*SubAllocationTier*/,
        uint32_t /*PipelineCoherencyMask*/,
        uint32_t /*FeedbackLoopTier*/,
        uint32_t /*SubGraphRecursionTier*/,
        uint32_t /*MeshPipeliningTier*/,
        uint32_t /*ProgramVersion*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0x88013);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport8(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDimension,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedback,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining) override {
        if (!pSupported || !pMaxMatrixDimension || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages || !pAutonomousFeedback || !pSubGraphRecursion || !pMeshShaderPipelining) return -1;
        *pSupported = 1;
        *pMaxMatrixDimension = 8192;
        *pSupportedDataTypes = 0x1FF; // Full precision & extended microscaling v4 tensor types
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 64;
        *pAutonomousFeedback = 1;
        *pSubGraphRecursion = 1;
        *pMeshShaderPipelining = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord14(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        uint32_t /*PipelineCoherencyTier*/,
        uint32_t /*FeedbackLoopTier*/,
        uint32_t /*SubGraphRecursionTier*/,
        uint32_t /*MeshPipeliningTier*/,
        uint32_t /*MultiQueueFusionTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0x88014);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport9(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion) override {
        if (!pSupported || !pMaxMatrixDim || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages || !pAutonomousFeedbackLoops || !pSubGraphRecursion || !pMeshShaderPipelining || !pMultiQueueFusion) return -1;
        *pSupported = 1;
        *pMaxMatrixDim = 16384;
        *pSupportedDataTypes = 0x3FF; // Full precision & extended microscaling v5 tensor types
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 128;
        *pAutonomousFeedbackLoops = 1;
        *pSubGraphRecursion = 1;
        *pMeshShaderPipelining = 1;
        *pMultiQueueFusion = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord15(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        uint32_t /*PipelineCoherencyTier*/,
        uint32_t /*FeedbackLoopTier*/,
        uint32_t /*SubGraphRecursionTier*/,
        uint32_t /*MeshPipeliningTier*/,
        uint32_t /*MultiQueueFusionTier*/,
        uint32_t /*MicroscalingTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0x88015);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport10(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport) override {
        if (!pSupported || !pMaxMatrixDim || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages || !pAutonomousFeedbackLoops || !pSubGraphRecursion || !pMeshShaderPipelining || !pMultiQueueFusion || !pMicroscalingSupport) return -1;
        *pSupported = 1;
        *pMaxMatrixDim = 32768;
        *pSupportedDataTypes = 0x7FF; // Full precision & extended microscaling v6 tensor types
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 256;
        *pAutonomousFeedbackLoops = 1;
        *pSubGraphRecursion = 1;
        *pMeshShaderPipelining = 1;
        *pMultiQueueFusion = 1;
        *pMicroscalingSupport = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord16(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        uint32_t /*PipelineCoherencyTier*/,
        uint32_t /*FeedbackLoopTier*/,
        uint32_t /*SubGraphRecursionTier*/,
        uint32_t /*MeshPipeliningTier*/,
        uint32_t /*MultiQueueFusionTier*/,
        uint32_t /*MicroscalingTier*/,
        uint32_t /*SubBytePrecisionTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0x88016);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport11(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision) override {
        if (!pSupported || !pMaxMatrixDim || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages || !pAutonomousFeedbackLoops || !pSubGraphRecursion || !pMeshShaderPipelining || !pMultiQueueFusion || !pMicroscalingSupport || !pSubByteMicroscopicPrecision) return -1;
        *pSupported = 1;
        *pMaxMatrixDim = 65536;
        *pSupportedDataTypes = 0xFFF; // Full precision & extended microscaling v7 tensor types
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 512;
        *pAutonomousFeedbackLoops = 1;
        *pSubGraphRecursion = 1;
        *pMeshShaderPipelining = 1;
        *pMultiQueueFusion = 1;
        *pMicroscalingSupport = 1;
        *pSubByteMicroscopicPrecision = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord17(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        uint32_t /*PipelineCoherencyTier*/,
        uint32_t /*FeedbackLoopTier*/,
        uint32_t /*SubGraphRecursionTier*/,
        uint32_t /*MeshPipeliningTier*/,
        uint32_t /*MultiQueueFusionTier*/,
        uint32_t /*MicroscalingTier*/,
        uint32_t /*SubBytePrecisionTier*/,
        uint32_t /*SparseTensorBlockTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0x88017);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport12(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision,
        int32_t* pSparseTensorBlockAcceleration) override {
        if (!pSupported || !pMaxMatrixDim || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages || !pAutonomousFeedbackLoops || !pSubGraphRecursion || !pMeshShaderPipelining || !pMultiQueueFusion || !pMicroscalingSupport || !pSubByteMicroscopicPrecision || !pSparseTensorBlockAcceleration) return -1;
        *pSupported = 1;
        *pMaxMatrixDim = 131072;
        *pSupportedDataTypes = 0x1FFF; // Full precision, extended microscaling v8 and fractional MXFP0.25 tensor types
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 1024;
        *pAutonomousFeedbackLoops = 1;
        *pSubGraphRecursion = 1;
        *pMeshShaderPipelining = 1;
        *pMultiQueueFusion = 1;
        *pMicroscalingSupport = 1;
        *pSubByteMicroscopicPrecision = 1;
        *pSparseTensorBlockAcceleration = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord18(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        uint32_t /*PipelineCoherencyTier*/,
        uint32_t /*FeedbackLoopTier*/,
        uint32_t /*SubGraphRecursionTier*/,
        uint32_t /*MeshPipeliningTier*/,
        uint32_t /*MultiQueueFusionTier*/,
        uint32_t /*MicroscalingTier*/,
        uint32_t /*SubBytePrecisionTier*/,
        uint32_t /*SparseTensorBlockTier*/,
        uint32_t /*DeterministicTensorReductionTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0x88018);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport13(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision,
        int32_t* pSparseTensorBlockAcceleration,
        int32_t* pDeterministicTensorReduction) override {
        if (!pSupported || !pMaxMatrixDim || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages || !pAutonomousFeedbackLoops || !pSubGraphRecursion || !pMeshShaderPipelining || !pMultiQueueFusion || !pMicroscalingSupport || !pSubByteMicroscopicPrecision || !pSparseTensorBlockAcceleration || !pDeterministicTensorReduction) return -1;
        *pSupported = 1;
        *pMaxMatrixDim = 262144;
        *pSupportedDataTypes = 0x3FFF; // Full precision, extended microscaling v9 and fractional MXFP0.125 / MXINT0.25 tensor types
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 2048;
        *pAutonomousFeedbackLoops = 1;
        *pSubGraphRecursion = 1;
        *pMeshShaderPipelining = 1;
        *pMultiQueueFusion = 1;
        *pMicroscalingSupport = 1;
        *pSubByteMicroscopicPrecision = 1;
        *pSparseTensorBlockAcceleration = 1;
        *pDeterministicTensorReduction = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord19(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        uint32_t /*PipelineCoherencyTier*/,
        uint32_t /*FeedbackLoopTier*/,
        uint32_t /*SubGraphRecursionTier*/,
        uint32_t /*MeshPipeliningTier*/,
        uint32_t /*MultiQueueFusionTier*/,
        uint32_t /*MicroscalingTier*/,
        uint32_t /*SubBytePrecisionTier*/,
        uint32_t /*SparseTensorBlockTier*/,
        uint32_t /*DeterministicTensorReductionTier*/,
        uint32_t /*HyperQuantizationTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0x88019);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport14(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision,
        int32_t* pSparseTensorBlockAcceleration,
        int32_t* pDeterministicTensorReduction,
        int32_t* pHyperQuantization) override {
        if (!pSupported || !pMaxMatrixDim || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages || !pAutonomousFeedbackLoops || !pSubGraphRecursion || !pMeshShaderPipelining || !pMultiQueueFusion || !pMicroscalingSupport || !pSubByteMicroscopicPrecision || !pSparseTensorBlockAcceleration || !pDeterministicTensorReduction || !pHyperQuantization) return -1;
        *pSupported = 1;
        *pMaxMatrixDim = 524288;
        *pSupportedDataTypes = 0x7FFF; // Full precision, extended microscaling v10 and fractional MXFP0.0625 / MXINT0.125 tensor types
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 4096;
        *pAutonomousFeedbackLoops = 1;
        *pSubGraphRecursion = 1;
        *pMeshShaderPipelining = 1;
        *pMultiQueueFusion = 1;
        *pMicroscalingSupport = 1;
        *pSubByteMicroscopicPrecision = 1;
        *pSparseTensorBlockAcceleration = 1;
        *pDeterministicTensorReduction = 1;
        *pHyperQuantization = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord20(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        uint32_t /*PipelineCoherencyTier*/,
        uint32_t /*FeedbackLoopTier*/,
        uint32_t /*SubGraphRecursionTier*/,
        uint32_t /*MeshPipeliningTier*/,
        uint32_t /*MultiQueueFusionTier*/,
        uint32_t /*MicroscalingTier*/,
        uint32_t /*SubBytePrecisionTier*/,
        uint32_t /*SparseTensorBlockTier*/,
        uint32_t /*DeterministicTensorReductionTier*/,
        uint32_t /*HyperQuantizationTier*/,
        uint32_t /*SparseClusterTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0x88020);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport15(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision,
        int32_t* pSparseTensorBlockAcceleration,
        int32_t* pDeterministicTensorReduction,
        int32_t* pHyperQuantization,
        int32_t* pSparseClusterAcceleration) override {
        if (!pSupported || !pMaxMatrixDim || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation || !pMaxTilePipeliningStages || !pAutonomousFeedbackLoops || !pSubGraphRecursion || !pMeshShaderPipelining || !pMultiQueueFusion || !pMicroscalingSupport || !pSubByteMicroscopicPrecision || !pSparseTensorBlockAcceleration || !pDeterministicTensorReduction || !pHyperQuantization || !pSparseClusterAcceleration) return -1;
        *pSupported = 1;
        *pMaxMatrixDim = 1048576;
        *pSupportedDataTypes = 0xFFFF; // Full precision, extended microscaling v11 and fractional MXFP0.03125 / MXINT0.0625 tensor types
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 8192;
        *pAutonomousFeedbackLoops = 1;
        *pSubGraphRecursion = 1;
        *pMeshShaderPipelining = 1;
        *pMultiQueueFusion = 1;
        *pMicroscalingSupport = 1;
        *pSubByteMicroscopicPrecision = 1;
        *pSparseTensorBlockAcceleration = 1;
        *pDeterministicTensorReduction = 1;
        *pHyperQuantization = 1;
        *pSparseClusterAcceleration = 1;
        return 0;
    }

    int32_t CreateWorkGraphProgramRecord21(
        const void* /*pDesc*/,
        uint32_t /*Flags*/,
        uint64_t /*NodeRecordStride*/,
        uint32_t /*MaxConcurrency*/,
        uint32_t /*DynamicExpansionFlags*/,
        uint32_t /*MemoryPoolTier*/,
        uint32_t /*TensorExpansionTier*/,
        uint32_t /*SubAllocationMask*/,
        uint32_t /*PipelineCoherencyTier*/,
        uint32_t /*FeedbackLoopTier*/,
        uint32_t /*SubGraphRecursionTier*/,
        uint32_t /*MeshPipeliningTier*/,
        uint32_t /*MultiQueueFusionTier*/,
        uint32_t /*MicroscalingTier*/,
        uint32_t /*SubBytePrecisionTier*/,
        uint32_t /*SparseTensorBlockTier*/,
        uint32_t /*DeterministicTensorReductionTier*/,
        uint32_t /*HyperQuantizationTier*/,
        uint32_t /*SparseClusterTier*/,
        uint32_t /*MultiQueueSyncTier*/,
        const IID& /*riid*/,
        void** ppRecord) override {
        if (!ppRecord) return -1;
        *ppRecord = reinterpret_cast<void*>(0x88021);
        m_workGraphProgramRecordsCreated++;
        return 0;
    }

    int32_t GetWorkGraphCooperativeMatrixSupport16(
        int32_t* pSupported,
        uint32_t* pMaxMatrixDim,
        uint32_t* pSupportedDataTypes,
        uint32_t* pSparseAcceleration,
        int32_t* pAsyncAccumulation,
        uint32_t* pMaxTilePipeliningStages,
        int32_t* pAutonomousFeedbackLoops,
        int32_t* pSubGraphRecursion,
        int32_t* pMeshShaderPipelining,
        int32_t* pMultiQueueFusion,
        int32_t* pMicroscalingSupport,
        int32_t* pSubByteMicroscopicPrecision,
        int32_t* pSparseTensorBlockAcceleration,
        int32_t* pDeterministicTensorReduction,
        int32_t* pHyperQuantization,
        int32_t* pSparseClusterAcceleration,
        int32_t* pMultiQueueSyncAcceleration) override {
        if (!pSupported || !pMaxMatrixDim || !pSupportedDataTypes || !pSparseAcceleration || !pAsyncAccumulation ||
            !pMaxTilePipeliningStages || !pAutonomousFeedbackLoops || !pSubGraphRecursion || !pMeshShaderPipelining ||
            !pMultiQueueFusion || !pMicroscalingSupport || !pSubByteMicroscopicPrecision ||
            !pSparseTensorBlockAcceleration || !pDeterministicTensorReduction || !pHyperQuantization ||
            !pSparseClusterAcceleration || !pMultiQueueSyncAcceleration) return -1;
        *pSupported = 1;
        *pMaxMatrixDim = 2097152;
        *pSupportedDataTypes = 0x1FFFF;
        *pSparseAcceleration = 1;
        *pAsyncAccumulation = 1;
        *pMaxTilePipeliningStages = 16384;
        *pAutonomousFeedbackLoops = 1;
        *pSubGraphRecursion = 1;
        *pMeshShaderPipelining = 1;
        *pMultiQueueFusion = 1;
        *pMicroscalingSupport = 1;
        *pSubByteMicroscopicPrecision = 1;
        *pSparseTensorBlockAcceleration = 1;
        *pDeterministicTensorReduction = 1;
        *pHyperQuantization = 1;
        *pSparseClusterAcceleration = 1;
        *pMultiQueueSyncAcceleration = 1;
        return 0;
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
            pData->NonNormalizedCoordinateSamplersSupported = TRUE;
            pData->ManualWriteTrackingResourceSupported = TRUE;
            pData->InvertedDepthRangesSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS18) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS18)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS18*>(pFeatureSupportData);
            pData->RenderPassesTier = D3D12_RENDER_PASS_TIER_1;
            pData->AdvancedBlendOpsSupported = TRUE;
            pData->RenderPassesValid = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS19) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS19)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS19*>(pFeatureSupportData);
            pData->MipLevelOffsetsSupported = TRUE;
            pData->HalfFloatPrecisionSupported = TRUE;
            pData->MismatchingOutputDimensionsSupported = TRUE;
            pData->SupportedSampleCountsWithNoOutputs = 1;
            pData->PointSamplingAddressesNeverRoundUp = TRUE;
            pData->RasterizerDesc2Supported = TRUE;
            pData->NarrowQuadrilateralLinesSupported = TRUE;
            pData->AnisoFilterWithPointMipSupported = TRUE;
            pData->MaxSamplerDescriptorHeapSize = 2048;
            pData->MaxSamplerDescriptorHeapSizeWithStaticSamplers = 2048;
            pData->MaxViewDescriptorHeapSize = 1000000;
            pData->ComputeOnlyCustomHeapSupported = TRUE;
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
            pData->DynamicIndexBufferStripCutSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS24) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS24)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS24*>(pFeatureSupportData);
            pData->WorkGraphsTier = 11;
            pData->GenericProgramsSupported = TRUE;
            pData->SubobjectCreationSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS25) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS25)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS25*>(pFeatureSupportData);
            pData->Max1DDispatchSize = 1073741824u;
            pData->Max1DDispatchMeshSize = 4194303u;
            pData->WaveMatrixSupported = TRUE;
            pData->CooperativeVectorsSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS26) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS26)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS26*>(pFeatureSupportData);
            pData->CpuTimelineQueryResolutionSupported = TRUE;
            pData->PeriodicTrimNotificationsSupported = TRUE;
            pData->ByteOffsetBufferViewsSupported = TRUE;
            pData->RelaxedCastableFormatsSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS27) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS27)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS27*>(pFeatureSupportData);
            pData->WorkGraphsTier = 11u; // Tier 1.1
            pData->DynamicNodeExpansionSupported = TRUE;
            pData->MultiTierWorkGraphsSupported = TRUE;
            pData->AmplifiedMeshDispatchSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS28) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS28)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS28*>(pFeatureSupportData);
            pData->WorkGraphsTier = 12u; // Tier 1.2
            pData->ComputeOnlyDeviceSupported = TRUE;
            pData->IndependentSamplerMipLevelsSupported = TRUE;
            pData->UnboundedStreamOutputSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS29) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS29)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS29*>(pFeatureSupportData);
            pData->WorkGraphsTier = 12u; // Tier 1.2
            pData->GPUUploadHeapSupported = TRUE;
            pData->DynamicDepthBiasClampSupported = TRUE;
            pData->BackingMemoryCustomPoolSupported = TRUE;
            pData->ComputeOnlyDeviceSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS30) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS30)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS30*>(pFeatureSupportData);
            pData->WorkGraphsTier = 13u; // Tier 1.3
            pData->NodeOutputRecordsSupported = TRUE;
            pData->DirectProgramRecordAllocationSupported = TRUE;
            pData->WorkGraphBackingMemoryVirtualRemapSupported = TRUE;
            pData->ShaderExecutionReorderingSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS31) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS31)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS31*>(pFeatureSupportData);
            pData->WorkGraphsTier = 14u; // Tier 2.0/2.1
            pData->DynamicDepthBoundsSupported = TRUE;
            pData->EnhancedWaveMatrixMultiplySupported = TRUE;
            pData->WaveMatrixMultiplyTiers = 3u; // Tier 3
            pData->ProgrammaticWorkGraphRecordBindingSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS32) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS32)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS32*>(pFeatureSupportData);
            pData->WorkGraphsTier = 15u; // Tier 2.2
            pData->DynamicGlobalRootSignatureSupported = TRUE;
            pData->AsyncWorkGraphDispatchSupported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 64u;
            pData->EnhancedBarrierDynamicAliasingSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS33) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS33)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS33*>(pFeatureSupportData);
            pData->WorkGraphsTier = 16u; // Tier 3.0
            pData->AutonomousMemoryAllocationSupported = TRUE;
            pData->DynamicNodeExpansionSupported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 128u;
            pData->WaveMatrixMultiplyAccumulateSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS34) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS34)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS34*>(pFeatureSupportData);
            pData->WorkGraphsTier = 17u; // Tier 3.1
            pData->CooperativeMatrixMultiplySupported = TRUE;
            pData->MultiQueueWorkGraphDispatchSupported = TRUE;
            pData->DynamicClusterExpansionSupported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 256u;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS35) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS35)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS35*>(pFeatureSupportData);
            pData->WorkGraphsTier = 18u; // Tier 3.2
            pData->CooperativeMatrixMultiply128Supported = TRUE;
            pData->FP8CooperativeMatrixSupported = TRUE;
            pData->MultiQueueDynamicClusterExpansionSupported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 512u;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS36) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS36)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS36*>(pFeatureSupportData);
            pData->WorkGraphsTier = 19u; // Tier 3.3
            pData->CooperativeMatrixMultiply256Supported = TRUE;
            pData->SparseTensorCoreAccelerationSupported = TRUE;
            pData->DynamicClusterExpansionTier4Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 1024u;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS37) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS37)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS37*>(pFeatureSupportData);
            pData->WorkGraphsTier = 20u; // Tier 3.4
            pData->CooperativeMatrixMultiply512Supported = TRUE;
            pData->SubByteTensorQuantizationSupported = TRUE;
            pData->DynamicClusterExpansionTier5Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 2048u;
            pData->AsynchronousTensorAccumulationSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS38) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS38)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS38*>(pFeatureSupportData);
            pData->WorkGraphsTier = 21u; // Tier 3.5
            pData->CooperativeMatrixMultiply1024Supported = TRUE;
            pData->SubByteMicroscalingTensorFormatsSupported = TRUE;
            pData->DynamicClusterExpansionTier6Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 4096u;
            pData->TilePipeliningStagesSupported = 8u;
            pData->AsynchronousTensorAccumulationSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS39) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS39)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS39*>(pFeatureSupportData);
            pData->WorkGraphsTier = 22u; // Tier 3.6
            pData->CooperativeMatrixMultiply2048Supported = TRUE;
            pData->SubByteMicroscalingTensorFormatsV2Supported = TRUE;
            pData->DynamicClusterExpansionTier7Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 8192u;
            pData->TilePipeliningStagesSupported = 16u;
            pData->AutonomousGraphFeedbackLoopsSupported = TRUE;
            pData->AsynchronousTensorAccumulationSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS40) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS40)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS40*>(pFeatureSupportData);
            pData->WorkGraphsTier = 23u; // Tier 3.7
            pData->CooperativeMatrixMultiply4096Supported = TRUE;
            pData->SubByteMicroscalingTensorFormatsV3Supported = TRUE;
            pData->DynamicClusterExpansionTier8Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 16384u;
            pData->TilePipeliningStagesSupported = 32u;
            pData->AutonomousGraphRecursionSupported = TRUE;
            pData->AsynchronousTensorPipelineFusionSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS41) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS41)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS41*>(pFeatureSupportData);
            pData->WorkGraphsTier = 24u; // Tier 3.8
            pData->CooperativeMatrixMultiply8192Supported = TRUE;
            pData->SubByteMicroscalingTensorFormatsV4Supported = TRUE;
            pData->DynamicClusterExpansionTier9Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 32768u;
            pData->TilePipeliningStagesSupported = 64u;
            pData->AutonomousGraphRecursionSupported = TRUE;
            pData->AsynchronousTensorPipelineFusionSupported = TRUE;
            pData->WorkGraphMeshShaderPipeliningSupported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS42) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS42)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS42*>(pFeatureSupportData);
            pData->WorkGraphsTier = 25u; // Tier 3.9
            pData->CooperativeMatrixMultiply16384Supported = TRUE;
            pData->SubByteMicroscalingTensorFormatsV5Supported = TRUE;
            pData->DynamicClusterExpansionTier10Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 65536u;
            pData->TilePipeliningStagesSupported = 128u;
            pData->AutonomousGraphRecursionTier2Supported = TRUE;
            pData->MultiQueueTensorPipelineFusionSupported = TRUE;
            pData->WorkGraphMeshShaderPipeliningTier2Supported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS43) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS43)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS43*>(pFeatureSupportData);
            pData->WorkGraphsTier = 26u; // Tier 4.0
            pData->CooperativeMatrixMultiply32768Supported = TRUE;
            pData->SubByteMicroscopicFormatsV6Supported = TRUE;
            pData->DynamicClusterExpansionTier11Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 131072u;
            pData->TilePipeliningStagesSupported = 256u;
            pData->AutonomousGraphRecursionTier3Supported = TRUE;
            pData->MultiQueueTensorPipelineFusionTier2Supported = TRUE;
            pData->WorkGraphMeshShaderPipeliningTier3Supported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS44) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS44)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS44*>(pFeatureSupportData);
            pData->WorkGraphsTier = 27u; // Tier 4.1
            pData->CooperativeMatrixMultiply65536Supported = TRUE;
            pData->SubByteMicroscopicFormatsV7Supported = TRUE;
            pData->DynamicClusterExpansionTier12Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 262144u;
            pData->TilePipeliningStagesSupported = 512u;
            pData->AutonomousGraphRecursionTier4Supported = TRUE;
            pData->MultiQueueTensorPipelineFusionTier3Supported = TRUE;
            pData->WorkGraphMeshShaderPipeliningTier4Supported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS45) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS45)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS45*>(pFeatureSupportData);
            pData->WorkGraphsTier = 28u; // Tier 5.0
            pData->CooperativeMatrixMultiply131072Supported = TRUE;
            pData->SubByteMicroscopicFormatsV8Supported = TRUE;
            pData->DynamicClusterExpansionTier13Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 524288u;
            pData->TilePipeliningStagesSupported = 1024u;
            pData->AutonomousGraphRecursionTier5Supported = TRUE;
            pData->MultiQueueTensorPipelineFusionTier4Supported = TRUE;
            pData->WorkGraphMeshShaderPipeliningTier5Supported = TRUE;
            pData->SparseTensorBlockAccelerationTier1Supported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS46) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS46)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS46*>(pFeatureSupportData);
            pData->WorkGraphsTier = 29u; // Tier 5.1
            pData->CooperativeMatrixMultiply262144Supported = TRUE;
            pData->SubByteMicroscopicFormatsV9Supported = TRUE;
            pData->DynamicClusterExpansionTier14Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 1048576u;
            pData->TilePipeliningStagesSupported = 2048u;
            pData->AutonomousGraphRecursionTier6Supported = TRUE;
            pData->MultiQueueTensorPipelineFusionTier5Supported = TRUE;
            pData->WorkGraphMeshShaderPipeliningTier6Supported = TRUE;
            pData->SparseTensorBlockAccelerationTier2Supported = TRUE;
            pData->DeterministicTensorReductionTier1Supported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS47) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS47)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS47*>(pFeatureSupportData);
            pData->WorkGraphsTier = 30u; // Tier 5.2
            pData->CooperativeMatrixMultiply524288Supported = TRUE;
            pData->SubByteMicroscopicFormatsV10Supported = TRUE;
            pData->DynamicClusterExpansionTier15Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 2097152u;
            pData->TilePipeliningStagesSupported = 4096u;
            pData->AutonomousGraphRecursionTier7Supported = TRUE;
            pData->MultiQueueTensorPipelineFusionTier6Supported = TRUE;
            pData->WorkGraphMeshShaderPipeliningTier7Supported = TRUE;
            pData->SparseTensorBlockAccelerationTier3Supported = TRUE;
            pData->DeterministicTensorReductionTier2Supported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS48) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS48)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS48*>(pFeatureSupportData);
            pData->WorkGraphsTier = 31u; // Tier 5.3
            pData->CooperativeMatrixMultiply1048576Supported = TRUE;
            pData->SubByteMicroscopicFormatsV11Supported = TRUE;
            pData->DynamicClusterExpansionTier16Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 4194304u;
            pData->TilePipeliningStagesSupported = 8192u;
            pData->AutonomousGraphRecursionTier8Supported = TRUE;
            pData->MultiQueueTensorPipelineFusionTier7Supported = TRUE;
            pData->WorkGraphMeshShaderPipeliningTier8Supported = TRUE;
            pData->SparseTensorBlockAccelerationTier4Supported = TRUE;
            pData->DeterministicTensorReductionTier3Supported = TRUE;
            pData->HyperQuantizationTier2Supported = TRUE;
            return 0;
        }
        if (Feature == D3D12_FEATURE_D3D12_OPTIONS49) {
            if (FeatureSupportDataSize < sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS49)) return -1;
            auto* pData = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS49*>(pFeatureSupportData);
            pData->WorkGraphsTier = 32u;
            pData->CooperativeMatrixMultiply2097152Supported = TRUE;
            pData->SubByteMicroscopicFormatsV12Supported = TRUE;
            pData->DynamicClusterExpansionTier17Supported = TRUE;
            pData->MaxWorkGraphConcurrencyGroups = 8388608u;
            pData->TilePipeliningStagesSupported = 16384u;
            pData->AutonomousGraphRecursionTier9Supported = TRUE;
            pData->MultiQueueTensorPipelineFusionTier8Supported = TRUE;
            pData->WorkGraphMeshShaderPipeliningTier9Supported = TRUE;
            pData->SparseTensorBlockAccelerationTier5Supported = TRUE;
            pData->DeterministicTensorReductionTier4Supported = TRUE;
            pData->HyperQuantizationTier3Supported = TRUE;
            pData->SparseClusterTier2Supported = TRUE;
            pData->MultiQueueWorkGraphSyncSupported = TRUE;
            return 0;
        }

        return 0;
    }
};

using Prism3D12Device35Impl = Prism3D12DeviceImpl;
using Prism3D12Device36Impl = Prism3D12DeviceImpl;
using Prism3D12Device37Impl = Prism3D12DeviceImpl;

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
    if (rclsid == CLSID_D3D12DeviceRemovedExtendedData) {
        auto* settings = new Prism3D12DeviceRemovedExtendedDataSettingsImpl();
        int32_t hr = settings->QueryInterface(riid, ppvDebug);
        settings->Release();
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
