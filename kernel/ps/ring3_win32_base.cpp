/**
 * @file ring3_win32_base.cpp
 * @brief Horizon 68: Ring 3 Win32 Base DLL & DynamicLoader Bridge Unit (#40)
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/native/ring3_win32_base_bridge.hpp"
#include <cstdio>

namespace micant::um::ring3_base {

void Ring3BaseInitializeExecutiveSubsystem() noexcept {
    auto& eng = Ring3Win32BaseBridgeEngine::instance();
    eng.initialize();

    const auto dllRep  = eng.bindWin32SubsystemDllsToRings();
    const auto timeRep = eng.verifyZeroSyscallKuserAndTeb();
    const auto brkRep  = eng.verifyRing2FontAndComBrokers();

    std::printf("[MicaNT Ring 3 Base] Mapped %u \\KnownDlls (%u Win32 Exports in %u us) | KUSER 0x%llX (%u Syscalls) | FontPort=%s (%u us) | RpcPort=%s (%u us)\n",
                dllRep.MappedKnownDllCount,
                dllRep.ResolvedWin32ExportCount,
                dllRep.IatBindingLatencyUs,
                static_cast<unsigned long long>(timeRep.KuserSharedDataAddress),
                timeRep.QpcReadSyscallCount,
                brkRep.FontBrokerPort,
                brkRep.FontGlyphRasterLatencyUs,
                brkRep.RpcEndpointPort,
                brkRep.ComLrpcActivationLatencyUs);
}

} // namespace micant::um::ring3_base

namespace micant::sys::internal {

void Ring3BaseInitializeExecutiveSubsystem() noexcept {
    micant::um::ring3_base::Ring3BaseInitializeExecutiveSubsystem();
}

} // namespace micant::sys::internal
