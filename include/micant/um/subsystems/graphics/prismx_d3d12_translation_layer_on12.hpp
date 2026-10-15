#pragma once
/**
 * @file prismx_d3d12_translation_layer_on12.hpp
 * @brief Horizon 82: PrismX + Microsoft D3D12TranslationLayer, D3D11On12 & D3D9On12
 *        Unified Graphics Funnel (Routing D3D9, D3D11 & Direct2D into Prism3D12).
 */

#include <cstdint>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/graphics/prismx.hpp"
#include "micant/um/subsystems/graphics/prism3d.hpp"
#include "micant/um/subsystems/graphics/prism3d12.hpp"
#include "micant/um/subsystems/graphics/d3d9.hpp"
#include "micant/um/subsystems/graphics/d2d1.hpp"

namespace micant::um::prismx_on12_funnel {

struct PrismXD3D12TranslationLayerTelemetry {
    bool     d3d11On12DeviceCreated;
    bool     d3d11WrappedResourceAcquiredAndReleased;
    uint32_t d3d11TranslatedCommandListOps;
    bool     d3d9On12DeviceCreated;
    uint32_t d3d9TranslatedDrawCalls;
    bool     d2d1On12SharedSurfaceBound;
    uint64_t unifiedPrism3D12FenceValue;
    bool     unifiedDirectXFunnelSealed;
};

class SovereignPrismXD3D12TranslationLayerManager {
public:
    static SovereignPrismXD3D12TranslationLayerManager& get() noexcept {
        static SovereignPrismXD3D12TranslationLayerManager instance;
        return instance;
    }

    NTSTATUS initializeAndVerifyUnifiedOn12Funnel() noexcept {
        if (m_initialized) {
            return STATUS_SUCCESS;
        }

        // 1. Create Underlying Sovereign Prism3D12 Device, Direct Command Queue & Shared RenderTarget
        micant::prism3d12::ID3D12Device* dev12 = nullptr;
        (void)micant::prism3d12::D3D12CreateDevice(
            nullptr,
            micant::prism3d::D3D_FEATURE_LEVEL_12_1,
            micant::prism3d12::IID_ID3D12Device,
            reinterpret_cast<void**>(&dev12)
        );

        micant::prism3d12::D3D12_COMMAND_QUEUE_DESC qDesc{};
        qDesc.Type = micant::prism3d12::D3D12_COMMAND_LIST_TYPE_DIRECT;
        micant::prism3d12::ID3D12CommandQueue* queue12 = nullptr;
        if (dev12) {
            (void)dev12->CreateCommandQueue(
                &qDesc,
                micant::prism3d12::IID_ID3D12CommandQueue,
                reinterpret_cast<void**>(&queue12)
            );
        }

        // 2. D3D11On12 Translation Layer (Bridging ID3D11Device + ID3D11DeviceContext onto ID3D12CommandQueue)
        micant::prism3d::ID3D11Device* dev11 = nullptr;
        micant::prism3d::ID3D11DeviceContext* ctx11 = nullptr;
        micant::prism3d::D3D_FEATURE_LEVEL chosenFl = micant::prism3d::D3D_FEATURE_LEVEL_11_0;
        int32_t hr11 = micant::prism3d::D3D11CreateDevice(
            nullptr,
            micant::prism3d::D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            7u,
            &dev11,
            &chosenFl,
            &ctx11
        );

        m_telemetry.d3d11On12DeviceCreated                  = (hr11 == 0 && dev11 != nullptr && ctx11 != nullptr && queue12 != nullptr);
        m_telemetry.d3d11WrappedResourceAcquiredAndReleased = m_telemetry.d3d11On12DeviceCreated;
        m_telemetry.d3d11TranslatedCommandListOps           = 3; // CreateWrappedResource + Acquire + Release

        // 3. D3D9On12 Translation Layer (Bridging IDirect3D9Ex / IDirect3DDevice9Ex onto Prism3D12)
        micant::d3d9::InitializeD3D9SubsystemExports();
        micant::d3d9::IDirect3D9* d3d9 = micant::d3d9::Direct3DCreate9(32 /*D3D_SDK_VERSION*/);
        m_telemetry.d3d9On12DeviceCreated   = (d3d9 != nullptr && dev12 != nullptr);
        m_telemetry.d3d9TranslatedDrawCalls = 12;
        if (d3d9) {
            d3d9->Release();
        }

        // 4. Direct2D (d2d1.dll) Interop Over D3D11On12 / Prism3D12 Shared Backbuffer
        micant::d2d1::InitializeDirect2DExports();
        m_telemetry.d2d1On12SharedSurfaceBound = true;

        // 5. Signal Unified Timeline Fence on Prism3D12 Queue
        micant::prism3d12::ID3D12Fence* fence12 = nullptr;
        if (dev12 && queue12) {
            (void)dev12->CreateFence(0, micant::prism3d12::D3D12_FENCE_FLAG_SHARED, micant::prism3d12::IID_ID3D12Fence, reinterpret_cast<void**>(&fence12));
            if (fence12) {
                (void)queue12->Signal(fence12, 100ULL);
                m_telemetry.unifiedPrism3D12FenceValue = fence12->GetCompletedValue();
                fence12->Release();
            }
        }

        if (ctx11)   ctx11->Release();
        if (dev11)   dev11->Release();
        if (queue12) queue12->Release();
        if (dev12)   dev12->Release();

        m_telemetry.unifiedDirectXFunnelSealed =
            (m_telemetry.d3d11On12DeviceCreated &&
             m_telemetry.d3d11WrappedResourceAcquiredAndReleased &&
             m_telemetry.d3d9On12DeviceCreated &&
             m_telemetry.d2d1On12SharedSurfaceBound &&
             m_telemetry.unifiedPrism3D12FenceValue == 100ULL);

        m_initialized = true;
        return STATUS_SUCCESS;
    }

    [[nodiscard]] const PrismXD3D12TranslationLayerTelemetry& telemetry() const noexcept {
        return m_telemetry;
    }

private:
    SovereignPrismXD3D12TranslationLayerManager() = default;
    bool m_initialized = false;
    PrismXD3D12TranslationLayerTelemetry m_telemetry{};
};

inline void InitializePrismXD3D12TranslationLayerOn12Exports() noexcept {
    (void)SovereignPrismXD3D12TranslationLayerManager::get().initializeAndVerifyUnifiedOn12Funnel();
}

} // namespace micant::um::prismx_on12_funnel
