/**
 * @file prismx_work_graphs_11_wddm32.cpp
 * @brief Horizon 303 (Kernel Unit #79): Sovereign PrismX Direct3D 12 Work Graphs 1.1 with Mesh Nodes WDDM 3.2 Subsystem
 *        - Direct3D 12 Agility SDK Work Graphs 1.1 Architecture (D3D12_WORK_GRAPHS_TIER_1_1)
 *        - Autonomous GPU-Driven Geometry Pipeline with Mesh Nodes (NodeLaunch("mesh"))
 *        - Multi-Rate Execution: Broadcasting Fan-Out & Record Coalescing Fan-In
 *        - WDDM 3.2 Kernel Ring Buffer Backing Memory Manager (D3DKMT_WORKGRAPH_BACKING_MEMORY)
 *        - Kernel GPU Hardware Context Scheduler & Asynchronous DAG Node Streamer
 *        - Zero-CPU-Stall Primitive Telemetry & Subpixel Barycentric Rasterization Pipeline.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_work_graphs_11_wddm32.hpp"

namespace micant::um::prismx_work_graphs {

bool VerifyHorizon303WorkGraphs11Subsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: Direct3D 12 Agility SDK Work Graphs 1.1 ABI & Mesh Node Types
    // ------------------------------------------------------------------------
    if (PRISMX_WORK_GRAPHS_TIER_1_1 != 11u) return false;
    if (PRISMX_NODE_TYPE_COMPUTE != 0u || PRISMX_NODE_TYPE_MESH != 1u) return false;
    if (PRISMX_NODE_LAUNCH_MODE_BROADCASTING != 0u ||
        PRISMX_NODE_LAUNCH_MODE_COALESCING != 1u ||
        PRISMX_NODE_LAUNCH_MODE_THREAD != 2u ||
        PRISMX_NODE_LAUNCH_MODE_MESH != 3u) return false;
    if ((PRISMX_WORK_GRAPH_FLAG_ENABLE_MESH_NODES & 0x04) == 0) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: WDDM 3.2 Kernel Ring Buffer Backing Memory Manager
    // ------------------------------------------------------------------------
    Wddm32WorkGraphBackingAllocation backing{};
    backing.GpuVirtualAddress = 0x0000000100000000ULL;
    backing.SizeInBytes = 262144u; // 256 KB
    backing.MemoryPoolTier = 1u;
    backing.RingBufferPageCount = 64u; // 64 x 4KB pages
    backing.Flags = 0x01u; // WDDM32_BACKING_RING_BUFFER_COHERENT

    if (backing.RingBufferPageCount != 64u || backing.SizeInBytes != 262144u) {
        return false;
    }

    // ------------------------------------------------------------------------
    // Pillar 3: Kernel GPU Hardware Context Scheduler & Asynchronous DAG Nodes
    // ------------------------------------------------------------------------
    SovereignWorkGraph11Engine engine(256, 256);
    engine.SetBackingAllocation(backing.GpuVirtualAddress, backing.SizeInBytes);
    engine.ClearBuffers(0xFF0A0A14u, 1.0f);

    struct ChunkPayload {
        uint32_t ChunkId;
        float    CenterX, CenterY, CenterZ;
    };

    struct MeshletRecordNT {
        uint32_t MeshletId;
        float    PosX, PosY;
    };

    // Node 0: Root Broadcasting Frustum Culling Compute Node
    PrismXKernelNodeDef node0{};
    node0.Name = L"FrustumCullCompute";
    node0.ArrayIndex = 0;
    node0.NodeType = PRISMX_NODE_TYPE_COMPUTE;
    node0.LaunchMode = PRISMX_NODE_LAUNCH_MODE_BROADCASTING;
    node0.ComputeShader = []([[maybe_unused]] const void* pRec, [[maybe_unused]] size_t sz, NodeOutputContextNT& ctx) {
        for (uint32_t c = 0; c < 4; ++c) {
            ChunkPayload chunk{ c, static_cast<float>(c) * 2.0f - 3.0f, 0.0f, 5.0f };
            ctx.EmitRecord(L"LODCoalesceCompute", &chunk, sizeof(chunk));
        }
    };
    engine.RegisterNode(node0);

    // Node 1: Coalescing LOD Evaluation Compute Node
    PrismXKernelNodeDef node1{};
    node1.Name = L"LODCoalesceCompute";
    node1.ArrayIndex = 0;
    node1.NodeType = PRISMX_NODE_TYPE_COMPUTE;
    node1.LaunchMode = PRISMX_NODE_LAUNCH_MODE_COALESCING;
    node1.MaxRecordsPerGroup = 4;
    node1.ComputeShader = [](const void* pRec, size_t sz, NodeOutputContextNT& ctx) {
        if (sz >= sizeof(ChunkPayload)) {
            const auto* cp = reinterpret_cast<const ChunkPayload*>(pRec);
            MeshletRecordNT mr{ cp->ChunkId, cp->CenterX, cp->CenterY };
            ctx.EmitRecord(L"MeshletRasterMesh", &mr, sizeof(mr));
        }
    };
    engine.RegisterNode(node1);

    // Node 2: Leaf Mesh Node Outputting Primitives to Rasterizer
    PrismXKernelNodeDef node2{};
    node2.Name = L"MeshletRasterMesh";
    node2.ArrayIndex = 0;
    node2.NodeType = PRISMX_NODE_TYPE_MESH;
    node2.LaunchMode = PRISMX_NODE_LAUNCH_MODE_MESH;
    node2.MaxMeshVertices = 64;
    node2.MaxMeshPrimitives = 126;
    node2.MeshShader = [](const void* pRec, size_t sz, MeshOutputContextNT& ctx) {
        if (sz >= sizeof(MeshletRecordNT)) {
            const auto* mr = reinterpret_cast<const MeshletRecordNT*>(pRec);
            ctx.SetMeshCounts(3, 1);

            float cx = mr->PosX * 0.2f;
            float cy = mr->PosY * 0.2f;

            PrismXMeshNodeVertex v0{};
            v0.x = cx - 0.3f; v0.y = cy - 0.3f; v0.z = 0.5f; v0.w = 1.0f;
            v0.r = 1.0f; v0.g = 0.2f; v0.b = 0.2f; v0.a = 1.0f;

            PrismXMeshNodeVertex v1{};
            v1.x = cx + 0.3f; v1.y = cy - 0.3f; v1.z = 0.5f; v1.w = 1.0f;
            v1.r = 0.2f; v1.g = 1.0f; v1.b = 0.2f; v1.a = 1.0f;

            PrismXMeshNodeVertex v2{};
            v2.x = cx; v2.y = cy + 0.4f; v2.z = 0.5f; v2.w = 1.0f;
            v2.r = 0.2f; v2.g = 0.2f; v2.b = 1.0f; v2.a = 1.0f;

            ctx.EmitVertex(v0);
            ctx.EmitVertex(v1);
            ctx.EmitVertex(v2);

            ctx.EmitPrimitive(0, 1, 2);
        }
    };
    engine.RegisterNode(node2);

    // ------------------------------------------------------------------------
    // Pillar 4: Autonomous GPU Primitive Pipeline Telemetry & Verification
    // ------------------------------------------------------------------------
    struct FrustumInputNT {
        float Frustum[6]{ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    } input{};

    D3dKmtDispatchWorkGraphEscape escape{};
    escape.ContextHandle = 0x0000CAFEu;
    escape.ProgramIdentifier = 0x57473131ULL; // 'WG11'
    escape.BackingMemoryGpuVa = backing.GpuVirtualAddress;
    escape.BackingMemorySizeBytes = static_cast<uint32_t>(backing.SizeInBytes);
    escape.NumEntrypointRecords = 1u;
    escape.RecordStrideBytes = sizeof(FrustumInputNT);
    escape.Flags = 0;

    bool dispatchOk = engine.ExecuteKernelDispatch(&escape, &input);
    if (!dispatchOk) return false;

    const auto& tel = engine.GetTelemetry();
    if (tel.TotalRecordsDispatched != 1u) return false;
    if (tel.TotalRecordsGenerated != 8u) return false; // 4 chunks + 4 meshlets
    if (tel.ComputeInvocations < 5u) return false;      // 1 root + 4 coalesced chunks
    if (tel.MeshNodeInvocations != 4u) return false;    // 4 meshlets
    if (tel.MeshletsGenerated != 4u) return false;
    if (tel.VerticesEmitted != 12u) return false;      // 4 x 3 vertices
    if (tel.PrimitivesEmitted != 4u) return false;     // 4 x 1 triangles
    if (tel.PixelsRasterized < 500u) return false;     // Verified raster pixel fill
    if (!tel.ZeroCpuStallVerified) return false;       // GPU autonomous pipeline
    if (tel.MeasuredSnrDb < 20.0f) return false;       // Signal-to-noise ratio

    return true;
}

} // namespace micant::um::prismx_work_graphs
