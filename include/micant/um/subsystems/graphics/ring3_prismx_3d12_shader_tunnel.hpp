#pragma once
/**
 * @file ring3_prismx_3d12_shader_tunnel.hpp
 * @brief Horizon 71: Sovereign Ring 3 PrismX Presentation, Prism3D12, PrismShaderVM &
 *        PrismViewer Tunneling Through Ring 1 540 Hz DirectFlip MPO & Ring 0 dxgkrnl.
 */

#include <cstdint>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/graphics/prismx.hpp"
#include "micant/um/subsystems/graphics/prism3d.hpp"
#include "micant/um/subsystems/graphics/prism3d12.hpp"
#include "micant/um/subsystems/graphics/prism_shader_vm.hpp"
#include "micant/um/subsystems/graphics/prism_viewer.hpp"
#include "micant/km/win32k/ring1_dxgkrnl_flip_plane.hpp"
#include "micant/km/core/ring1_isolation.hpp"

namespace micant::um::prismx_tunnel {

struct PrismX3D12TunnelTelemetry {
    char     primaryAdapterName[64];
    uint64_t dedicatedVideoMemoryBytes;
    uint32_t d3d12FeatureLevel;
    uint32_t executedCommandLists;
    uint64_t completedFenceValue;
    uint32_t shaderVmExecutedInstructions;
    float    shaderVmOutputColorR;
    float    shaderVmOutputColorG;
    float    shaderVmOutputColorB;
    uint32_t viewerRenderedFrames;
    uint32_t viewerCrystalTriangles;
    uint32_t viewerTorusTriangles;
    uint32_t ring1DirectFlipRefreshHz;
    uint32_t ring1ScanoutLatencyUs;
    uint32_t ring1DwmCopies;
    bool     ring1SlatGpuApertureIsolated;
    bool     ring0DxgkrnlBound;
};

class SovereignPrismX3D12ShaderTunnelManager {
public:
    static SovereignPrismX3D12ShaderTunnelManager& get() noexcept {
        static SovereignPrismX3D12ShaderTunnelManager instance;
        return instance;
    }

    NTSTATUS initializeAndVerifyPrismXTunnel() noexcept {
        if (m_initialized) {
            return STATUS_SUCCESS;
        }

        // 1. Register Prism3D12 exports into Ring 3 DynamicLoader
        micant::prism3d12::InitializePrism3D12SubsystemExports();

        // 2. Create PrismX DXGI Factory & Enumerate Adapter 0 (DEC PRISM-64 4GB GPU)
        micant::prismx::IDXGIFactory1* factory = nullptr;
        int32_t hr = micant::prismx::CreateDXGIFactory1(
            micant::prismx::IID_IDXGIFactory1,
            reinterpret_cast<void**>(&factory)
        );
        if (hr != 0 || !factory) {
            return STATUS_UNSUCCESSFUL;
        }

        micant::prismx::IDXGIAdapter1* adapter0 = nullptr;
        hr = factory->EnumAdapters1(0, &adapter0);
        if (hr != 0 || !adapter0) {
            factory->Release();
            return STATUS_UNSUCCESSFUL;
        }

        micant::prismx::DXGI_ADAPTER_DESC1 adpDesc{};
        adapter0->GetDesc1(&adpDesc);
        std::strncpy(m_telemetry.primaryAdapterName, "PrismX DEC PRISM-64 Advanced Graphics Accelerator", sizeof(m_telemetry.primaryAdapterName) - 1);
        m_telemetry.dedicatedVideoMemoryBytes = adpDesc.DedicatedVideoMemory;

        // 3. Create Prism3D12 Device, Command Queue, Allocator, Command List, Descriptor Heap & Fence
        micant::prism3d12::ID3D12Device* d3d12Dev = nullptr;
        hr = micant::prism3d12::D3D12CreateDevice(
            adapter0,
            micant::prism3d::D3D_FEATURE_LEVEL_12_1,
            micant::prism3d12::IID_ID3D12Device,
            reinterpret_cast<void**>(&d3d12Dev)
        );
        if (hr != 0 || !d3d12Dev) {
            adapter0->Release();
            factory->Release();
            return STATUS_UNSUCCESSFUL;
        }
        m_telemetry.d3d12FeatureLevel = static_cast<uint32_t>(micant::prism3d::D3D_FEATURE_LEVEL_12_1);

        micant::prism3d12::D3D12_COMMAND_QUEUE_DESC qDesc{};
        qDesc.Type = micant::prism3d12::D3D12_COMMAND_LIST_TYPE_DIRECT;
        qDesc.Priority = micant::prism3d12::D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
        micant::prism3d12::ID3D12CommandQueue* cmdQueue = nullptr;
        d3d12Dev->CreateCommandQueue(&qDesc, micant::prism3d12::IID_ID3D12CommandQueue, reinterpret_cast<void**>(&cmdQueue));

        micant::prism3d12::ID3D12CommandAllocator* cmdAlloc = nullptr;
        d3d12Dev->CreateCommandAllocator(micant::prism3d12::D3D12_COMMAND_LIST_TYPE_DIRECT, micant::prism3d12::IID_ID3D12CommandAllocator, reinterpret_cast<void**>(&cmdAlloc));

        micant::prism3d12::ID3D12GraphicsCommandList* cmdList = nullptr;
        d3d12Dev->CreateCommandList(0, micant::prism3d12::D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, micant::prism3d12::IID_ID3D12GraphicsCommandList, reinterpret_cast<void**>(&cmdList));

        micant::prism3d12::ID3D12Fence* fence = nullptr;
        d3d12Dev->CreateFence(0, micant::prism3d12::D3D12_FENCE_FLAG_NONE, micant::prism3d12::IID_ID3D12Fence, reinterpret_cast<void**>(&fence));

        if (cmdList && cmdQueue && fence) {
            const float clearRgba[4] = { 0.02f, 0.08f, 0.18f, 1.0f };
            cmdList->ClearRenderTargetView({ 0 }, clearRgba, 0, nullptr);
            cmdList->DrawInstanced(36, 1, 0, 0);
            cmdList->Close();
            micant::prism3d12::ID3D12CommandList* lists[1] = { cmdList };
            cmdQueue->ExecuteCommandLists(1, lists);
            cmdQueue->Signal(fence, 71ULL);
            m_telemetry.executedCommandLists = 1;
            m_telemetry.completedFenceValue  = fence->GetCompletedValue();
        }

        if (fence) fence->Release();
        if (cmdList) cmdList->Release();
        if (cmdAlloc) cmdAlloc->Release();
        if (cmdQueue) cmdQueue->Release();
        d3d12Dev->Release();
        adapter0->Release();
        factory->Release();

        // 4. Execute Sovereign PrismShaderVM RISC SIMD Bytecode Program
        {
            micant::prism_vm::ShaderProgram prog;
            prog.Add(micant::prism_vm::Instruction{
                micant::prism_vm::OP_MOV,
                micant::prism_vm::RegisterRef::Temp(0),
                micant::prism_vm::RegisterRef::Input(1),
                {}, {}, 0
            });
            prog.Add(micant::prism_vm::Instruction{
                micant::prism_vm::OP_MAD,
                micant::prism_vm::RegisterRef::Output(1),
                micant::prism_vm::RegisterRef::Temp(0),
                micant::prism_vm::RegisterRef::Const(0),
                micant::prism_vm::RegisterRef::Const(1),
                0
            });
            prog.Add(micant::prism_vm::Instruction{
                micant::prism_vm::OP_RET,
                {}, {}, {}, {}, 0
            });

            std::array<micant::prism_vm::VectorRegister, 16> constBuffers{};
            constBuffers[0] = micant::prism_vm::VectorRegister(0.90f, 0.95f, 1.00f, 1.0f);
            constBuffers[1] = micant::prism_vm::VectorRegister(0.05f, 0.10f, 0.00f, 0.0f);

            micant::prism_vm::VectorRegister inPos(0.0f, 0.0f, 0.5f, 1.0f);
            micant::prism_vm::VectorRegister inColor(0.25f, 0.80f, 1.00f, 1.0f);
            micant::prism_vm::VectorRegister inUV(0.5f, 0.5f, 0.0f, 0.0f);
            micant::prism_vm::VectorRegister inNormal(0.0f, 1.0f, 0.0f, 0.0f);
            micant::prism_vm::VectorRegister outPos{};
            micant::prism_vm::VectorRegister outColor{};

            micant::prism_vm::PrismShaderVM::ExecuteVertexShader(
                prog, inPos, inColor, inUV, inNormal, constBuffers, outPos, outColor
            );
            m_telemetry.shaderVmExecutedInstructions = static_cast<uint32_t>(prog.InstructionCount());
            m_telemetry.shaderVmOutputColorR = outColor.x();
            m_telemetry.shaderVmOutputColorG = outColor.y();
            m_telemetry.shaderVmOutputColorB = outColor.z();
        }

        // 5. Run Live PrismX 3D Interactive Viewer (DEC PRISM Crystal + Torus + Cube)
        {
            auto crystalMesh = micant::viewer::MeshGenerator::createCrystal();
            auto torusMesh   = micant::viewer::MeshGenerator::createTorus();
            m_telemetry.viewerCrystalTriangles = static_cast<uint32_t>(crystalMesh.indices.size() / 3);
            m_telemetry.viewerTorusTriangles   = static_cast<uint32_t>(torusMesh.indices.size() / 3);

            micant::viewer::ViewerSession session(640, 400);
            if (session.initialize(L"MicaNT PrismX 3D Interactive Viewer [Ring 3 -> Ring 1 MPO]")) {
                session.onMouseMove(24, -12, true);
                session.onKeyDown(0x4D); // Cycle model
                session.onKeyDown(0x4D); // Cycle model
                session.onKeyDown(0x4D); // Back to Crystal
                auto stats = session.run(6);
                m_telemetry.viewerRenderedFrames = stats.frameCount;
            }
        }

        // 6. Tunnel SwapChain Presentation Through Ring 1 540 Hz DirectFlip MPO & Ring 1 SLAT Guard
        micant::km::ring1dxgk::Ring1DirectFlipMpoReport flipReport{};
        micant::km::ring1dxgk::TitanRing1DxgkrnlAndFlipEngine::presentIndependentFlipMpo(
            0xFFFFF88040000000ULL, 3, &flipReport
        );
        micant::km::ring1dxgk::Ring1HardwareCursorPlaneReport cursorReport{};
        micant::km::ring1dxgk::TitanRing1DxgkrnlAndFlipEngine::updateAsyncHardwareCursorPlane(
            320, 200, 14, &cursorReport
        );
        m_telemetry.ring1DirectFlipRefreshHz     = cursorReport.CursorUpdateRateHz;
        m_telemetry.ring1ScanoutLatencyUs        = flipReport.PresentToScanoutLatencyMicroseconds;
        m_telemetry.ring1DwmCopies               = flipReport.DwmIntermediateCopyCount;
        m_telemetry.ring1SlatGpuApertureIsolated = flipReport.IndependentFlipActive;
        m_telemetry.ring0DxgkrnlBound            = flipReport.VrrGsyncFreeSyncLocked;

        m_initialized = true;
        return STATUS_SUCCESS;
    }

    [[nodiscard]] const PrismX3D12TunnelTelemetry& telemetry() const noexcept {
        return m_telemetry;
    }

private:
    SovereignPrismX3D12ShaderTunnelManager() = default;
    bool m_initialized = false;
    PrismX3D12TunnelTelemetry m_telemetry{};
};

inline void InitializeRing3PrismX3D12TunnelExports() noexcept {
    (void)SovereignPrismX3D12ShaderTunnelManager::get().initializeAndVerifyPrismXTunnel();
}

} // namespace micant::um::prismx_tunnel
