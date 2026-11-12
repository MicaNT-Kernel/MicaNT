#pragma once
/**
 * @file ring3_winsock_winhttp_tls_tunnel.hpp
 * @brief Horizon 74: Sovereign Ring 3 Winsock2 (ws2_32.dll), WinHTTP (winhttp.dll) &
 *        Schannel TLS 1.3 (secur32.dll / schannel.dll) Tunneling Through Ring 2
 *        NetworkService & Ring 0 afd.sys Fast-I/O, Winsock Kernel (WSK) & Registered I/O (RIO).
 */

#include <cstdint>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/net/ws2_32.hpp"
#include "micant/um/subsystems/net/winhttp.hpp"
#include "micant/um/subsystems/security/sspi.hpp"

namespace micant::um::net_tunnel {

struct WinsockWinHttpTlsTunnelTelemetry {
    char     wsaDescription[64];
    uint16_t wsaNegotiatedVersion;
    uint32_t createdTcpSocketId;
    uint16_t boundLocalPort;
    bool     winHttpSessionAndRequestActive;
    bool     schannelTls13CredHandleValid;
    uint32_t afdFastIoBindLatencyNs;
    bool     afdBypassedIrpAllocation;
    uint32_t rioSubmittedPackets;
    uint32_t rioCompletedCqEntries;
    uint64_t rioTotalTransferredBytes;
    uint32_t rioPerPacketMdlAllocations;
    uint32_t wskThroughputGbps;
    uint32_t wskLoopbackRttUs;
    bool     networkTunnelSealed;
};

class SovereignWinsockWinHttpTlsTunnelManager {
public:
    static SovereignWinsockWinHttpTlsTunnelManager& get() noexcept {
        static SovereignWinsockWinHttpTlsTunnelManager instance;
        return instance;
    }

    NTSTATUS initializeAndVerifyNetworkTlsTunnel() noexcept {
        // WinHTTP/TLS and RIO are not backed by a complete transport contract yet.
        // Do not report a successful end-to-end tunnel from synthetic telemetry.
        return STATUS_NOT_SUPPORTED;
    }

    [[nodiscard]] const WinsockWinHttpTlsTunnelTelemetry& telemetry() const noexcept {
        return m_telemetry;
    }

private:
    SovereignWinsockWinHttpTlsTunnelManager() = default;
    bool m_initialized = false;
    WinsockWinHttpTlsTunnelTelemetry m_telemetry{};
};

inline void InitializeRing3WinsockWinHttpTlsTunnelExports() noexcept {
    (void)SovereignWinsockWinHttpTlsTunnelManager::get().initializeAndVerifyNetworkTlsTunnel();
}

} // namespace micant::um::net_tunnel
