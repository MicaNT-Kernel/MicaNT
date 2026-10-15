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
#include "micant/km/drivers/net/afd_wsk_rio.hpp"

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
        if (m_initialized) {
            return STATUS_SUCCESS;
        }

        // 1. Register Ring 3 Winsock2 (ws2_32.dll / wsock32.dll), WinHTTP & SSPI/Schannel exports
        micant::ws2_32::InitializeWs2_32SubsystemExports();
        micant::winhttp::InitializeWinHttpSubsystemExports();
        micant::sspi::InitializeSspiSubsystemExports();

        // 2. Initialize Winsock 2.2 & Create/Bind a TCP Stream Socket
        micant::ws2_32::WSADATA wsaData{};
        int wsaRes = micant::ws2_32::WSAStartup(0x0202, &wsaData);
        if (wsaRes != 0) {
            return STATUS_UNSUCCESSFUL;
        }
        std::strncpy(m_telemetry.wsaDescription, wsaData.szDescription, sizeof(m_telemetry.wsaDescription) - 1);
        m_telemetry.wsaNegotiatedVersion = wsaData.wVersion;

        micant::ws2_32::SOCKET tcpSock = micant::ws2_32::socket(
            micant::ws2_32::AF_INET,
            micant::ws2_32::SOCK_STREAM,
            micant::ws2_32::IPPROTO_TCP
        );
        if (tcpSock == micant::ws2_32::INVALID_SOCKET) {
            return STATUS_UNSUCCESSFUL;
        }
        m_telemetry.createdTcpSocketId = static_cast<uint32_t>(tcpSock);

        micant::ws2_32::sockaddr_in localAddr{};
        localAddr.sin_family = micant::ws2_32::AF_INET;
        localAddr.sin_port   = micant::ws2_32::htons(8443);
        localAddr.sin_addr.S_un.S_addr = micant::ws2_32::htonl(micant::ws2_32::INADDR_LOOPBACK);
        (void)micant::ws2_32::bind(
            tcpSock,
            reinterpret_cast<const micant::ws2_32::sockaddr*>(&localAddr),
            sizeof(localAddr)
        );
        m_telemetry.boundLocalPort = 8443;

        // 3. Verify WinHTTP Session, Connection & HTTPS Request Pipeline
        auto hSession = micant::winhttp::WinHttpOpen(L"MicaNT-Sovereign-Agent/1.0", 0, nullptr, nullptr, 0);
        auto hConnect = micant::winhttp::WinHttpConnect(hSession, L"telemetry-zero.micant.local", 443, 0);
        auto hRequest = micant::winhttp::WinHttpOpenRequest(hConnect, L"GET", L"/v1/health", nullptr, nullptr, nullptr, 0);
        auto sendOk   = micant::winhttp::WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0);
        auto recvOk   = micant::winhttp::WinHttpReceiveResponse(hRequest, nullptr);
        m_telemetry.winHttpSessionAndRequestActive = (hSession != nullptr && hConnect != nullptr && hRequest != nullptr && sendOk != 0 && recvOk != 0);

        micant::winhttp::WinHttpCloseHandle(hRequest);
        micant::winhttp::WinHttpCloseHandle(hConnect);
        micant::winhttp::WinHttpCloseHandle(hSession);

        // 4. Acquire Schannel TLS 1.3 Outbound Credential Handle via SSPI
        micant::sspi::CredHandle tlsCred{};
        micant::win32::FILETIME expiry{};
        micant::sspi::SECURITY_STATUS secStatus = micant::sspi::AcquireCredentialsHandleW(
            nullptr,
            const_cast<wchar_t*>(L"Microsoft Unified Security Protocol Provider"),
            micant::sspi::SECPKG_CRED_OUTBOUND,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            &tlsCred,
            &expiry
        );
        m_telemetry.schannelTls13CredHandleValid = (secStatus == micant::sspi::SEC_E_OK && tlsCred.isValid());
        if (tlsCred.isValid()) {
            micant::sspi::FreeCredentialsHandle(&tlsCred);
        }

        // 5. Tunnel Through Ring 0 \Device\Afd\Endpoint Fast-I/O, RIO Pre-Pinned MDL Ring & WSK Direct
        micant::km::afdwsk::AfdFastIoEndpointReport afdReport{};
        micant::km::afdwsk::TitanAfdWskAndRioEngine::openAndBindAfdFastIoEndpoint(
            0x7F000001u, 8443, &afdReport
        );

        micant::km::afdwsk::RioZeroCopyRingReport rioReport{};
        micant::km::afdwsk::TitanAfdWskAndRioEngine::executeRioZeroCopyBurst(
            262144, 64, 4096, &rioReport
        );

        micant::km::afdwsk::WskDirectProviderReport wskReport{};
        micant::km::afdwsk::TitanAfdWskAndRioEngine::dispatchWskDirectTransport(
            12, &wskReport
        );

        m_telemetry.afdFastIoBindLatencyNs     = afdReport.FastIoBindConnectLatencyNs;
        m_telemetry.afdBypassedIrpAllocation   = afdReport.FastIoDispatchBypassedIrpAlloc;
        m_telemetry.rioSubmittedPackets        = rioReport.SubmittedTxPackets;
        m_telemetry.rioCompletedCqEntries      = rioReport.CompletedCqEntries;
        m_telemetry.rioTotalTransferredBytes   = rioReport.TotalTransferredBytes;
        m_telemetry.rioPerPacketMdlAllocations = rioReport.PerPacketMdlAllocations;
        m_telemetry.wskThroughputGbps          = wskReport.ThroughputGbpsEquivalent;
        m_telemetry.wskLoopbackRttUs           = wskReport.LoopbackRoundTripMicroseconds;
        m_telemetry.networkTunnelSealed        = (rioReport.ZeroCopyCompletionVerified && wskReport.EstablishedFlowTupleShortCircuited);

        m_initialized = true;
        return STATUS_SUCCESS;
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
