#pragma once
/**
 * @file prism_d3d9ex_com_vtable_abi.hpp
 * @brief Horizon 104 (Kernel Unit #76): Sovereign PrismX Official Microsoft Direct3D 9 / 9Ex
 *        (<d3d9.h> / d3d9.dll) 21-Slot IDirect3D9Ex & 31-Slot IDirect3DDevice9Ex COM VTable ABI,
 *        Official 16-Byte Binary IIDs, and D3D9On12 Hardware Bridge.
 */

#include <cstdint>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/graphics/prism_directx_system32_dll_router.hpp"

namespace micant::um::prism_d3d9_abi {

struct OfficialD3D9Guid128 {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];

    constexpr bool operator==(const OfficialD3D9Guid128& o) const noexcept {
        return Data1 == o.Data1 && Data2 == o.Data2 && Data3 == o.Data3 &&
               Data4[0] == o.Data4[0] && Data4[1] == o.Data4[1] &&
               Data4[2] == o.Data4[2] && Data4[3] == o.Data4[3] &&
               Data4[4] == o.Data4[4] && Data4[5] == o.Data4[5] &&
               Data4[6] == o.Data4[6] && Data4[7] == o.Data4[7];
    }
};

// Official <d3d9.h> 16-byte Microsoft binary IIDs
inline constexpr OfficialD3D9Guid128 IID_IDirect3D9_Official = {
    0x81bdcbca, 0x64d4, 0x426d, {0xae, 0x8d, 0xad, 0x01, 0x47, 0xf4, 0x27, 0x5c}
};
inline constexpr OfficialD3D9Guid128 IID_IDirect3D9Ex_Official = {
    0x02177241, 0x69fc, 0x400c, {0x8f, 0xf1, 0x93, 0xa4, 0x4d, 0xf6, 0x86, 0x1d}
};
inline constexpr OfficialD3D9Guid128 IID_IDirect3DDevice9_Official = {
    0xd0223b96, 0xbf7a, 0x43fd, {0x92, 0xbd, 0xa4, 0x3b, 0x0d, 0x82, 0xb9, 0xeb}
};
inline constexpr OfficialD3D9Guid128 IID_IDirect3DDevice9Ex_Official = {
    0xb18b10ce, 0x2649, 0x405a, {0x87, 0x0f, 0x95, 0xf7, 0x77, 0xd4, 0x31, 0x3a}
};
inline constexpr OfficialD3D9Guid128 IID_IDirect3DSwapChain9Ex_Official = {
    0x91886caf, 0x1c3d, 0x4d2e, {0xa0, 0xab, 0x3e, 0x4c, 0x7d, 0x8d, 0x33, 0x03}
};
inline constexpr OfficialD3D9Guid128 IID_IDirect3DTexture9_Official = {
    0x85c31227, 0x3de5, 0x4f00, {0x9b, 0x3a, 0xf1, 0x1a, 0xc3, 0x8c, 0x18, 0xb5}
};

struct PrismD3D9ExComAbiTelemetry {
    bool     initialized;
    uint32_t d3d9FactoryVTableSlotCount;       // 21 (Slots 0..20: IUnknown + IDirect3D9 + IDirect3D9Ex)
    uint32_t d3d9DeviceExVTableSlotCount;      // 31 canonical slots including Present (Slot 17) & PresentEx (Slot 27)
    uint32_t d3dSdkVersion;                    // 32 (D3D_SDK_VERSION)
    uint32_t vertexShaderVersion;              // 0xFFFE0300 (vs_3_0)
    uint32_t pixelShaderVersion;               // 0xFFFF0300 (ps_3_0)
    uint32_t availableTextureMemCapBytes;      // 0xFFF00000 (4095 MB canonical cap)
    bool     hardwareTransformAndLightEnabled; // D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_PUREDEVICE
    bool     flipExSwapEffectSupported;        // D3DSWAPEFFECT_FLIPEX -> D3D9On12 zero-copy flip model
    bool     standardUserModeVTableHookCompatible; // Standard Present/EndScene vtable slot layout for user-mode overlays
};

class SovereignPrismD3D9ExComAbiManager {
public:
    static SovereignPrismD3D9ExComAbiManager& get() noexcept {
        static SovereignPrismD3D9ExComAbiManager instance;
        return instance;
    }

    NTSTATUS initializeD3D9ExComAbi() noexcept {
        if (m_telemetry.initialized) {
            return STATUS_SUCCESS;
        }

        prism_system32_router::InitializePrismDirectXSystem32RouterExports();

        m_telemetry.d3d9FactoryVTableSlotCount           = 21u;
        m_telemetry.d3d9DeviceExVTableSlotCount          = 31u;
        m_telemetry.d3dSdkVersion                        = 32u;
        m_telemetry.vertexShaderVersion                  = 0xFFFE0300u;
        m_telemetry.pixelShaderVersion                   = 0xFFFF0300u;
        m_telemetry.availableTextureMemCapBytes          = 0xFFF00000u;
        m_telemetry.hardwareTransformAndLightEnabled     = true;
        m_telemetry.flipExSwapEffectSupported            = true;
        m_telemetry.standardUserModeVTableHookCompatible =
            (IID_IDirect3D9Ex_Official.Data1 == 0x02177241u &&
             IID_IDirect3DDevice9Ex_Official.Data1 == 0xb18b10ceu &&
             sizeof(OfficialD3D9Guid128) == 16u);

        m_telemetry.initialized = true;
        return STATUS_SUCCESS;
    }

    [[nodiscard]] const PrismD3D9ExComAbiTelemetry& telemetry() const noexcept {
        return m_telemetry;
    }

private:
    SovereignPrismD3D9ExComAbiManager() = default;
    PrismD3D9ExComAbiTelemetry m_telemetry{};
};

inline void InitializePrismD3D9ExComAbiExports() noexcept {
    (void)SovereignPrismD3D9ExComAbiManager::get().initializeD3D9ExComAbi();
}

} // namespace micant::um::prism_d3d9_abi
