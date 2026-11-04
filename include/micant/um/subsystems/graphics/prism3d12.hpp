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
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cstring>
#include <algorithm>
#include <functional>

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
    D3D12_HEAP_FLAG_CREATE_NOT_ZEROED            = 0x1000
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

struct D3D12_GRAPHICS_PIPELINE_STATE_DESC {
    void* pRootSignature;
    D3D11_RASTERIZER_DESC RasterizerState;
    D3D11_BLEND_DESC BlendState;
    D3D11_DEPTH_STENCIL_DESC DepthStencilState;
    DXGI_FORMAT RTVFormats[8];
    uint32_t NumRenderTargets;
    DXGI_FORMAT DSVFormat;
    DXGI_SAMPLE_DESC SampleDesc;
    uint32_t NodeMask;
};

// ============================================================================
// 3. COM Interface GUIDs
// ============================================================================

static constexpr IID IID_ID3D12Object = 
    { 0xc4fec28f, 0x796e, 0x4e23, { 0x9e, 0x18, 0x0d, 0x96, 0x5a, 0x00, 0xd9, 0x59 } };

static constexpr IID IID_ID3D12DeviceChild = 
    { 0x905db44e, 0xbb35, 0x4455, { 0xb1, 0x0b, 0xac, 0x13, 0x27, 0xf4, 0x73, 0xa0 } };

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

class ID3D12Resource : public ID3D12DeviceChild {
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
};

static constexpr IID IID_ID3D12Device8 = 
    { 0x9218e66e, 0x19b4, 0x4ec2, { 0xa7, 0x76, 0xb2, 0xd4, 0x44, 0x53, 0x4f, 0x16 } };

class ID3D12Device8 : public ID3D12Device {
public:
    virtual void CreateSamplerFeedbackUnorderedAccessView(
        ID3D12Resource* pTargetedResource,
        ID3D12Resource* pFeedbackMinMipMap,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) = 0;
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

public:
    Prism3D12ResourceImpl(ID3D12Device* pDev, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state)
        : m_pDevice(pDev), m_desc(desc), m_currentState(state) {
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

// D3D12 Device Implementation
class Prism3D12DeviceImpl : public ID3D12Device8 {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    IUnknown* m_pAdapter{ nullptr };
    D3D_FEATURE_LEVEL m_featureLevel{ D3D_FEATURE_LEVEL_12_0 };
    uint32_t m_samplerFeedbackViewsCreated{ 0 };

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
        if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12Device || riid == IID_ID3D12Device8) {
            *ppv = static_cast<ID3D12Device8*>(this);
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
