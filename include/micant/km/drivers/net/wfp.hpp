#pragma once

/**
 * @file wfp.hpp
 * @brief Clean-Room Sovereign Windows Filtering Platform (WFP) Kernel Driver
 *        (fwpkclnt.sys), Callout Inspection Pipeline, Stateful Packet Inspection
 *        (SPI) Conntrack Engine & Windows Defender Firewall Core (mpsdrv.sys).
 *
 * Implements standard Windows NT kernel-mode packet classification, callout
 * registration ABI (FwpsCalloutRegister0/UnregisterById0), bi-directional stateful
 * flow tracking (RFC 793 / RFC 9293), TCP anomaly / stealth scanning mitigation,
 * and Windows Defender Firewall IOCTL management.
 *
 * References:
 * - Microsoft Windows Filtering Platform (WFP) Kernel-Mode Architecture
 * - RFC 791 (IPv4), RFC 792 (ICMP), RFC 793 / 9293 (TCP), RFC 768 (UDP)
 * - 17 U.S.C. § 1201(f) & Google v. Oracle clean-room protocol.
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <span>
#include <algorithm>
#include <functional>
#include <sstream>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/net/tcpip.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/um/subsystems/net/fwpuclnt.hpp"

namespace micant::wfp::km {

// ============================================================================
// 1. Kernel-Mode WFP Execution Layer Identifiers (fwpkclnt.h / fwpsk.h)
// ============================================================================

inline constexpr uint16_t FWPS_LAYER_INBOUND_IPPACKET_V4     = 0x0001;
inline constexpr uint16_t FWPS_LAYER_OUTBOUND_IPPACKET_V4    = 0x0002;
inline constexpr uint16_t FWPS_LAYER_INBOUND_TRANSPORT_V4    = 0x0003;
inline constexpr uint16_t FWPS_LAYER_OUTBOUND_TRANSPORT_V4   = 0x0004;
inline constexpr uint16_t FWPS_LAYER_ALE_AUTH_CONNECT_V4     = 0x0005;
inline constexpr uint16_t FWPS_LAYER_ALE_AUTH_RECV_ACCEPT_V4 = 0x0006;
inline constexpr uint16_t FWPS_LAYER_ALE_FLOW_ESTABLISHED_V4 = 0x0007;

inline constexpr uint16_t FWPS_LAYER_INBOUND_IPPACKET_V6     = 0x0008;
inline constexpr uint16_t FWPS_LAYER_OUTBOUND_IPPACKET_V6    = 0x0009;
inline constexpr uint16_t FWPS_LAYER_INBOUND_TRANSPORT_V6    = 0x000A;
inline constexpr uint16_t FWPS_LAYER_OUTBOUND_TRANSPORT_V6   = 0x000B;

// Callout Notification Types
enum FWPS_CALLOUT_NOTIFY_TYPE {
    FWPS_CALLOUT_NOTIFY_ADD_FILTER    = 0,
    FWPS_CALLOUT_NOTIFY_DELETE_FILTER = 1
};

// Classify Output Flags
inline constexpr uint32_t FWPS_CLASSIFY_OUT_FLAG_ABSORBED    = 0x00000001;
inline constexpr uint32_t FWPS_CLASSIFY_OUT_FLAG_NO_MORE_CALLOUTS = 0x00000002;

// Firewall Device IOCTL Codes (mpsdrv.sys)
inline constexpr uint32_t IOCTL_MPS_GET_PROFILE        = 0x0012E004;
inline constexpr uint32_t IOCTL_MPS_SET_PROFILE        = 0x0012E008;
inline constexpr uint32_t IOCTL_MPS_ADD_RULE           = 0x0012E00C;
inline constexpr uint32_t IOCTL_MPS_DELETE_RULE        = 0x0012E010;
inline constexpr uint32_t IOCTL_MPS_GET_STATISTICS     = 0x0012E014;
inline constexpr uint32_t IOCTL_MPS_RESET_STATISTICS   = 0x0012E018;

// ============================================================================
// 2. Kernel WFP Classification Structures
// ============================================================================

struct FWPS_CLASSIFY_OUT0 {
    uint32_t actionType{FWP_ACTION_PERMIT};
    uint64_t outContext{0};
    uint64_t filterId{0};
    uint32_t rights{0xFFFFFFFF}; // Downstream modify rights
    uint32_t flags{0};
};

struct FWPS_INCOMING_VALUES0 {
    uint16_t layerId{0};
    uint32_t count{0};
    tcpip::Ipv4Address localIp{};
    tcpip::Ipv4Address remoteIp{};
    uint16_t localPort{0};
    uint16_t remotePort{0};
    uint8_t protocol{0};
    uint64_t ifLuid{0};
};

struct FWPS_INCOMING_METADATA_VALUES0 {
    uint64_t flowHandle{0};
    uint32_t processId{0};
    FWP_DIRECTION direction{FWP_DIRECTION_INBOUND};
    uint64_t packetTimestamp{0};
};

// Forward declaration
struct FWPS_CALLOUT0;

// Callout Function Prototypes
using FWPS_CALLOUT_CLASSIFY_FN0 = void (*)(
    const FWPS_INCOMING_VALUES0* inFixedValues,
    const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
    void* layerData,
    const void* classifyContext,
    const FWPM_FILTER0* filter,
    uint64_t flowContext,
    FWPS_CLASSIFY_OUT0* classifyOut
);

using FWPS_CALLOUT_NOTIFY_FN0 = uint32_t (*)(
    FWPS_CALLOUT_NOTIFY_TYPE notifyType,
    const GUID* filterKey,
    FWPM_FILTER0* filter
);

using FWPS_CALLOUT_FLOW_DELETE_NOTIFY_FN0 = void (*)(
    uint16_t layerId,
    uint32_t calloutId,
    uint64_t flowContext
);

struct FWPS_CALLOUT0 {
    GUID calloutKey{};
    uint32_t flags{0};
    FWPS_CALLOUT_CLASSIFY_FN0 classifyFn{nullptr};
    FWPS_CALLOUT_NOTIFY_FN0 notifyFn{nullptr};
    FWPS_CALLOUT_FLOW_DELETE_NOTIFY_FN0 flowDeleteFn{nullptr};
};

// ============================================================================
// 3. Kernel Callout Registry (fwpkclnt.sys)
// ============================================================================

class WfpCalloutRegistry {
public:
    static WfpCalloutRegistry& get() {
        static WfpCalloutRegistry instance;
        return instance;
    }

    void resetForTest() {
        std::lock_guard<std::mutex> lock(mutex_);
        callouts_.clear();
        nextCalloutId_ = 1000;
        flowContexts_.clear();
    }

    uint32_t registerCallout(void* /*deviceObject*/, const FWPS_CALLOUT0* callout, uint32_t* outCalloutId) {
        if (!callout || !outCalloutId || !callout->classifyFn) return ERROR_INVALID_PARAMETER;
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t id = nextCalloutId_++;
        callouts_[id] = *callout;
        *outCalloutId = id;
        return ERROR_SUCCESS;
    }

    uint32_t unregisterCallout(uint32_t calloutId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = callouts_.find(calloutId);
        if (it == callouts_.end()) return ERROR_NOT_FOUND;
        callouts_.erase(it);
        return ERROR_SUCCESS;
    }

    const FWPS_CALLOUT0* getCallout(uint32_t calloutId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = callouts_.find(calloutId);
        if (it == callouts_.end()) return nullptr;
        return &it->second;
    }

    uint32_t associateFlowContext(uint64_t flowId, uint16_t layerId, uint32_t calloutId, uint64_t flowContext) {
        (void)layerId; (void)calloutId;
        std::lock_guard<std::mutex> lock(mutex_);
        flowContexts_[flowId] = flowContext;
        return ERROR_SUCCESS;
    }

    uint32_t removeFlowContext(uint64_t flowId, uint16_t layerId, uint32_t calloutId) {
        (void)layerId; (void)calloutId;
        std::lock_guard<std::mutex> lock(mutex_);
        flowContexts_.erase(flowId);
        return ERROR_SUCCESS;
    }

    uint64_t getFlowContext(uint64_t flowId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = flowContexts_.find(flowId);
        return (it != flowContexts_.end()) ? it->second : 0;
    }

    [[nodiscard]] size_t getCalloutCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return callouts_.size();
    }

private:
    WfpCalloutRegistry() = default;

    mutable std::mutex mutex_;
    uint32_t nextCalloutId_{1000};
    std::unordered_map<uint32_t, FWPS_CALLOUT0> callouts_;
    std::unordered_map<uint64_t, uint64_t> flowContexts_;
};

// ============================================================================
// 4. Stateful Packet Inspection (SPI) Connection Tracker (WfpConntrackEngine)
// ============================================================================

enum class TcpConntrackState {
    None,
    SynSent,
    SynReceived,
    Established,
    FinWait,
    CloseWait,
    TimeWait,
    Closed
};

[[nodiscard]] inline const char* getTcpConntrackStateName(TcpConntrackState s) noexcept {
    switch (s) {
        case TcpConntrackState::None:        return "NONE";
        case TcpConntrackState::SynSent:     return "SYN_SENT";
        case TcpConntrackState::SynReceived: return "SYN_RECV";
        case TcpConntrackState::Established: return "ESTABLISHED";
        case TcpConntrackState::FinWait:     return "FIN_WAIT";
        case TcpConntrackState::CloseWait:   return "CLOSE_WAIT";
        case TcpConntrackState::TimeWait:    return "TIME_WAIT";
        case TcpConntrackState::Closed:      return "CLOSED";
        default:                             return "UNKNOWN";
    }
}

struct FlowKey {
    tcpip::Ipv4Address srcIp{};
    tcpip::Ipv4Address dstIp{};
    uint16_t srcPort{0};
    uint16_t dstPort{0};
    uint8_t protocol{0};

    bool operator==(const FlowKey& o) const noexcept {
        return srcIp == o.srcIp && dstIp == o.dstIp &&
               srcPort == o.srcPort && dstPort == o.dstPort &&
               protocol == o.protocol;
    }
};

struct FlowKeyHasher {
    size_t operator()(const FlowKey& k) const noexcept {
        size_t h = std::hash<uint32_t>{}(k.srcIp.addr);
        h ^= (std::hash<uint32_t>{}(k.dstIp.addr) + 0x9e3779b9 + (h << 6) + (h >> 2));
        h ^= (std::hash<uint32_t>{}(static_cast<uint32_t>(k.srcPort) | (static_cast<uint32_t>(k.dstPort) << 16)) + 0x9e3779b9 + (h << 6) + (h >> 2));
        h ^= (std::hash<uint8_t>{}(k.protocol) + 0x9e3779b9 + (h << 6) + (h >> 2));
        return h;
    }
};

struct FlowEntry {
    uint64_t flowId{0};
    FlowKey key{};
    TcpConntrackState tcpState{TcpConntrackState::None};
    uint64_t packetsSent{0};
    uint64_t packetsReceived{0};
    uint64_t bytesSent{0};
    uint64_t bytesReceived{0};
    std::chrono::steady_clock::time_point lastSeen{};
    uint32_t timeoutSeconds{300};
};

class WfpConntrackEngine {
public:
    static WfpConntrackEngine& get() {
        static WfpConntrackEngine instance;
        return instance;
    }

    void resetForTest() {
        std::lock_guard<std::mutex> lock(mutex_);
        flows_.clear();
        nextFlowId_ = 5000;
        anomalousDrops_ = 0;
    }

    /**
     * @brief Detects illegal TCP flag combinations (attack signatures).
     */
    static bool isAnomalousTcpFlags(uint8_t flags) noexcept {
        // SYN + FIN scan
        if ((flags & (tcpip::TCP_FLAG_SYN | tcpip::TCP_FLAG_FIN)) == (tcpip::TCP_FLAG_SYN | tcpip::TCP_FLAG_FIN)) {
            return true;
        }
        // NULL scan (all control flags zero)
        if ((flags & 0x3F) == 0) {
            return true;
        }
        // XMAS scan (FIN + PSH + URG)
        if ((flags & (tcpip::TCP_FLAG_FIN | tcpip::TCP_FLAG_PSH | tcpip::TCP_FLAG_URG)) ==
            (tcpip::TCP_FLAG_FIN | tcpip::TCP_FLAG_PSH | tcpip::TCP_FLAG_URG)) {
            return true;
        }
        return false;
    }

    /**
     * @brief Inspects or establishes a stateful flow for packet ingress/egress.
     * @return true if flow is recognized, established, or successfully initiated.
     */
    bool processPacket(
        bool inbound,
        tcpip::Ipv4Address srcIp,
        tcpip::Ipv4Address dstIp,
        uint8_t protocol,
        uint16_t srcPort,
        uint16_t dstPort,
        uint8_t tcpFlags,
        size_t byteCount,
        bool& outIsEstablishedFlow
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        outIsEstablishedFlow = false;

        // Check for illegal flag combinations
        if (protocol == tcpip::IPPROTO_TCP && isAnomalousTcpFlags(tcpFlags)) {
            anomalousDrops_++;
            return false; // Immediate anomaly drop
        }

        // Forward and reverse flow keys
        FlowKey fwdKey{srcIp, dstIp, srcPort, dstPort, protocol};
        FlowKey revKey{dstIp, srcIp, dstPort, srcPort, protocol};

        auto it = flows_.find(fwdKey);
        auto revIt = flows_.find(revKey);

        auto now = std::chrono::steady_clock::now();

        if (protocol == tcpip::IPPROTO_TCP) {
            // Outbound connection initiation (SYN)
            if (!inbound && (tcpFlags & tcpip::TCP_FLAG_SYN) && !(tcpFlags & tcpip::TCP_FLAG_ACK)) {
                FlowEntry entry{};
                entry.flowId = nextFlowId_++;
                entry.key = fwdKey;
                entry.tcpState = TcpConntrackState::SynSent;
                entry.packetsSent = 1;
                entry.bytesSent = byteCount;
                entry.lastSeen = now;
                entry.timeoutSeconds = 30;
                flows_[fwdKey] = entry;
                return true;
            }

            // Inbound response to outbound SYN (SYN-ACK)
            if (inbound && revIt != flows_.end() && revIt->second.tcpState == TcpConntrackState::SynSent &&
                (tcpFlags & tcpip::TCP_FLAG_SYN) && (tcpFlags & tcpip::TCP_FLAG_ACK)) {
                revIt->second.tcpState = TcpConntrackState::SynReceived;
                revIt->second.packetsReceived++;
                revIt->second.bytesReceived += byteCount;
                revIt->second.lastSeen = now;
                outIsEstablishedFlow = true;
                return true;
            }

            // Outbound ACK completing 3-way handshake
            if (!inbound && it != flows_.end() && it->second.tcpState == TcpConntrackState::SynReceived &&
                (tcpFlags & tcpip::TCP_FLAG_ACK)) {
                it->second.tcpState = TcpConntrackState::Established;
                it->second.packetsSent++;
                it->second.bytesSent += byteCount;
                it->second.lastSeen = now;
                it->second.timeoutSeconds = 3600; // 1 hour for established TCP
                outIsEstablishedFlow = true;
                return true;
            }

            // Existing Established Flow (Forward or Return)
            if (it != flows_.end() && it->second.tcpState == TcpConntrackState::Established) {
                it->second.packetsSent++;
                it->second.bytesSent += byteCount;
                it->second.lastSeen = now;
                outIsEstablishedFlow = true;
                if (tcpFlags & tcpip::TCP_FLAG_FIN) it->second.tcpState = TcpConntrackState::FinWait;
                if (tcpFlags & tcpip::TCP_FLAG_RST) it->second.tcpState = TcpConntrackState::Closed;
                return true;
            }

            if (revIt != flows_.end() && revIt->second.tcpState == TcpConntrackState::Established) {
                revIt->second.packetsReceived++;
                revIt->second.bytesReceived += byteCount;
                revIt->second.lastSeen = now;
                outIsEstablishedFlow = true;
                if (tcpFlags & tcpip::TCP_FLAG_FIN) revIt->second.tcpState = TcpConntrackState::CloseWait;
                if (tcpFlags & tcpip::TCP_FLAG_RST) revIt->second.tcpState = TcpConntrackState::Closed;
                return true;
            }

            // Inbound SYN on server listening port (handled upstream by rule, but trackable)
            if (inbound && (tcpFlags & tcpip::TCP_FLAG_SYN) && !(tcpFlags & tcpip::TCP_FLAG_ACK)) {
                FlowEntry entry{};
                entry.flowId = nextFlowId_++;
                entry.key = fwdKey;
                entry.tcpState = TcpConntrackState::SynReceived;
                entry.packetsReceived = 1;
                entry.bytesReceived = byteCount;
                entry.lastSeen = now;
                entry.timeoutSeconds = 30;
                flows_[fwdKey] = entry;
                return true;
            }
        } else if (protocol == tcpip::IPPROTO_UDP || protocol == tcpip::IPPROTO_ICMP) {
            if (it != flows_.end()) {
                it->second.packetsSent++;
                it->second.bytesSent += byteCount;
                it->second.lastSeen = now;
                outIsEstablishedFlow = true;
                return true;
            }
            if (revIt != flows_.end()) {
                revIt->second.packetsReceived++;
                revIt->second.bytesReceived += byteCount;
                revIt->second.lastSeen = now;
                outIsEstablishedFlow = true;
                return true;
            }

            // Create new UDP/ICMP flow
            FlowEntry entry{};
            entry.flowId = nextFlowId_++;
            entry.key = fwdKey;
            entry.packetsSent = inbound ? 0 : 1;
            entry.packetsReceived = inbound ? 1 : 0;
            entry.bytesSent = inbound ? 0 : byteCount;
            entry.bytesReceived = inbound ? byteCount : 0;
            entry.lastSeen = now;
            entry.timeoutSeconds = 30;
            flows_[fwdKey] = entry;
            return true;
        }

        return false;
    }

    [[nodiscard]] size_t getActiveFlowCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return flows_.size();
    }

    [[nodiscard]] uint64_t getAnomalousDropCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return anomalousDrops_;
    }

private:
    WfpConntrackEngine() = default;

    mutable std::mutex mutex_;
    uint64_t nextFlowId_{5000};
    uint64_t anomalousDrops_{0};
    std::unordered_map<FlowKey, FlowEntry, FlowKeyHasher> flows_;
};

// ============================================================================
// 5. Sovereign Network Stealth & Threat Mitigation Engine
// ============================================================================

class WfpThreatMitigationEngine {
public:
    static WfpThreatMitigationEngine& get() {
        static WfpThreatMitigationEngine instance;
        return instance;
    }

    void resetForTest() {
        std::lock_guard<std::mutex> lock(mutex_);
        stealthDrops_ = 0;
        portScansDetected_ = 0;
        synFloodEvents_ = 0;
        synFloodActive_ = false;
        halfOpenSynCount_ = 0;
        blacklist_.clear();
        probes_.clear();
    }

    void recordStealthDrop() noexcept {
        stealthDrops_++;
    }

    [[nodiscard]] uint64_t getStealthDropCount() const noexcept {
        return stealthDrops_;
    }

    [[nodiscard]] uint64_t getPortScansDetected() const noexcept {
        return portScansDetected_;
    }

    [[nodiscard]] bool isSynFloodActive() const noexcept {
        return synFloodActive_;
    }

    void recordSynPacket(bool synAckReceived = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (synAckReceived) {
            if (halfOpenSynCount_ > 0) halfOpenSynCount_--;
            if (halfOpenSynCount_ < 300) synFloodActive_ = false;
        } else {
            halfOpenSynCount_++;
            if (halfOpenSynCount_ >= 500) {
                synFloodActive_ = true;
                synFloodEvents_++;
            }
        }
    }

    /**
     * @brief Records a port probe attempt; triggers automated dynamic blacklisting
     *        if >= 5 distinct ports are hit within 1 second.
     */
    void recordPortProbe(tcpip::Ipv4Address srcIp, uint16_t dstPort) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();

        auto& record = probes_[srcIp];
        // Prune old probes (> 1000 ms)
        for (auto it = record.begin(); it != record.end(); ) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->timestamp).count() > 1000) {
                it = record.erase(it);
            } else {
                ++it;
            }
        }

        // Add current probe if port distinct
        bool found = false;
        for (const auto& p : record) {
            if (p.port == dstPort) { found = true; break; }
        }
        if (!found) {
            record.push_back({dstPort, now});
        }

        if (record.size() >= 5) {
            portScansDetected_++;
            // Blacklist IP for 60 seconds
            blacklist_[srcIp] = now + std::chrono::seconds(60);
        }
    }

    [[nodiscard]] bool isBlacklisted(tcpip::Ipv4Address ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = blacklist_.find(ip);
        if (it == blacklist_.end()) return false;
        if (std::chrono::steady_clock::now() > it->second) {
            blacklist_.erase(it);
            return false;
        }
        return true;
    }

private:
    struct ProbeEntry {
        uint16_t port{0};
        std::chrono::steady_clock::time_point timestamp{};
    };

    WfpThreatMitigationEngine() = default;

    mutable std::mutex mutex_;
    uint64_t stealthDrops_{0};
    uint64_t portScansDetected_{0};
    uint64_t synFloodEvents_{0};
    bool synFloodActive_{false};
    uint32_t halfOpenSynCount_{0};

    std::unordered_map<tcpip::Ipv4Address, std::chrono::steady_clock::time_point, tcpip::Ipv4Hasher> blacklist_;
    std::unordered_map<tcpip::Ipv4Address, std::vector<ProbeEntry>, tcpip::Ipv4Hasher> probes_;
};

// ============================================================================
// 6. Sovereign Kernel Filter Engine (fwpkclnt.sys)
// ============================================================================

struct WfpKernelFilterRule {
    uint64_t filterId{0};
    std::string name;
    uint16_t layerId{FWPS_LAYER_INBOUND_TRANSPORT_V4};
    uint32_t weight{1000};
    FWP_DIRECTION direction{FWP_DIRECTION_INBOUND};
    uint8_t protocol{0}; // 0 = ANY
    tcpip::Ipv4Address localIp{};
    tcpip::Ipv4Address remoteIp{};
    tcpip::Ipv4Address remoteMask{};
    uint16_t localPortStart{0};
    uint16_t localPortEnd{0};
    uint16_t remotePortStart{0};
    uint16_t remotePortEnd{0};
    uint32_t action{FWP_ACTION_PERMIT};
    uint32_t calloutId{0};
    bool enabled{true};

    [[nodiscard]] bool matches(
        FWP_DIRECTION dir,
        uint8_t proto,
        tcpip::Ipv4Address sIp,
        tcpip::Ipv4Address dIp,
        uint16_t sPort,
        uint16_t dPort
    ) const noexcept {
        if (!enabled || direction != dir) return false;
        if (protocol != 0 && protocol != proto) return false;

        // Subnet check for remote IP
        if (!remoteIp.isZero()) {
            tcpip::Ipv4Address checkIp = (dir == FWP_DIRECTION_INBOUND) ? sIp : dIp;
            if (!remoteMask.isZero()) {
                if ((checkIp.addr & remoteMask.addr) != (remoteIp.addr & remoteMask.addr)) return false;
            } else {
                if (checkIp != remoteIp) return false;
            }
        }

        // Port checks
        uint16_t portToCheck = (dir == FWP_DIRECTION_INBOUND) ? dPort : sPort;
        if (localPortStart > 0) {
            uint16_t end = (localPortEnd > 0) ? localPortEnd : localPortStart;
            if (portToCheck < localPortStart || portToCheck > end) return false;
        }

        uint16_t remPortToCheck = (dir == FWP_DIRECTION_INBOUND) ? sPort : dPort;
        if (remotePortStart > 0) {
            uint16_t end = (remotePortEnd > 0) ? remotePortEnd : remotePortStart;
            if (remPortToCheck < remotePortStart || remPortToCheck > end) return false;
        }

        return true;
    }
};

class WfpKernelFilterEngine {
public:
    static WfpKernelFilterEngine& get() {
        static WfpKernelFilterEngine instance;
        return instance;
    }

    void resetForTest() {
        std::lock_guard<std::mutex> lock(mutex_);
        rules_.clear();
        nextRuleId_ = 2000;
        totalClassified_ = 0;
        packetsPermitted_ = 0;
        packetsBlocked_ = 0;
        bytesPermitted_ = 0;
        bytesBlocked_ = 0;
        seedCanonicalFirewallRules();
    }

    uint64_t addRule(const WfpKernelFilterRule& rule) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto r = rule;
        if (r.filterId == 0) r.filterId = nextRuleId_++;
        rules_.push_back(r);
        std::sort(rules_.begin(), rules_.end(), [](const auto& a, const auto& b) {
            return a.weight > b.weight; // Highest weight first
        });
        return r.filterId;
    }

    bool deleteRuleById(uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = rules_.begin(); it != rules_.end(); ++it) {
            if (it->filterId == id) {
                rules_.erase(it);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Core classification dispatch hook executed on every IP packet.
     * @return true to PERMIT packet, false to BLOCK / silently drop.
     */
    bool classifyPacket(
        bool inbound,
        tcpip::Ipv4Address srcIp,
        tcpip::Ipv4Address dstIp,
        uint8_t protocol,
        std::span<const uint8_t> transportData
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        totalClassified_++;
        size_t byteCount = transportData.size();

        auto& threatEngine = WfpThreatMitigationEngine::get();
        auto& conntrack = WfpConntrackEngine::get();

        // 1. Dynamic Blacklist Check
        if (threatEngine.isBlacklisted(srcIp)) {
            packetsBlocked_++;
            bytesBlocked_ += byteCount;
            threatEngine.recordStealthDrop();
            return false;
        }

        // Parse transport headers
        uint16_t srcPort = 0;
        uint16_t dstPort = 0;
        uint8_t tcpFlags = 0;

        if (protocol == tcpip::IPPROTO_TCP && transportData.size() >= sizeof(tcpip::TcpHeader)) {
            const auto* tcp = reinterpret_cast<const tcpip::TcpHeader*>(transportData.data());
            srcPort = tcp->getSrcPort();
            dstPort = tcp->getDstPort();
            tcpFlags = tcp->getFlags();
        } else if (protocol == tcpip::IPPROTO_UDP && transportData.size() >= sizeof(tcpip::UdpHeader)) {
            const auto* udp = reinterpret_cast<const tcpip::UdpHeader*>(transportData.data());
            srcPort = udp->getSrcPort();
            dstPort = udp->getDstPort();
        }

        // 2. Stateful Packet Inspection (Conntrack)
        bool isEstablished = false;
        bool conntrackOk = conntrack.processPacket(
            inbound, srcIp, dstIp, protocol, srcPort, dstPort, tcpFlags, byteCount, isEstablished
        );

        if (!conntrackOk) {
            // Anomaly detected (SYN-FIN, NULL, XMAS, etc.)
            packetsBlocked_++;
            bytesBlocked_ += byteCount;
            threatEngine.recordStealthDrop();
            return false;
        }

        // If part of established return flow, immediately PERMIT
        if (isEstablished) {
            packetsPermitted_++;
            bytesPermitted_ += byteCount;
            return true;
        }

        // 3. Port Scan Probe Analysis
        if (inbound && protocol == tcpip::IPPROTO_TCP && (tcpFlags & tcpip::TCP_FLAG_SYN)) {
            threatEngine.recordPortProbe(srcIp, dstPort);
            threatEngine.recordSynPacket(false);
        }

        // 4. Evaluate Rule Table (Highest Priority First)
        FWP_DIRECTION dir = inbound ? FWP_DIRECTION_INBOUND : FWP_DIRECTION_OUTBOUND;
        for (const auto& rule : rules_) {
            if (rule.matches(dir, protocol, srcIp, dstIp, srcPort, dstPort)) {
                // Callout evaluation
                if (rule.calloutId > 0) {
                    const auto* callout = WfpCalloutRegistry::get().getCallout(rule.calloutId);
                    if (callout && callout->classifyFn) {
                        FWPS_INCOMING_VALUES0 inVals{};
                        inVals.layerId = rule.layerId;
                        inVals.localIp = inbound ? dstIp : srcIp;
                        inVals.remoteIp = inbound ? srcIp : dstIp;
                        inVals.localPort = inbound ? dstPort : srcPort;
                        inVals.remotePort = inbound ? srcPort : dstPort;
                        inVals.protocol = protocol;

                        FWPS_INCOMING_METADATA_VALUES0 inMeta{};
                        inMeta.direction = dir;

                        FWPS_CLASSIFY_OUT0 classifyOut{};
                        classifyOut.actionType = rule.action;

                        callout->classifyFn(&inVals, &inMeta, nullptr, nullptr, nullptr, 0, &classifyOut);

                        if (classifyOut.actionType == FWP_ACTION_BLOCK) {
                            packetsBlocked_++;
                            bytesBlocked_ += byteCount;
                            threatEngine.recordStealthDrop();
                            return false;
                        }
                        if (classifyOut.actionType == FWP_ACTION_PERMIT) {
                            packetsPermitted_++;
                            bytesPermitted_ += byteCount;
                            return true;
                        }
                    }
                }

                if (rule.action == FWP_ACTION_BLOCK) {
                    packetsBlocked_++;
                    bytesBlocked_ += byteCount;
                    threatEngine.recordStealthDrop();
                    return false;
                } else if (rule.action == FWP_ACTION_PERMIT) {
                    packetsPermitted_++;
                    bytesPermitted_ += byteCount;
                    return true;
                }
            }
        }

        // 5. Default Firewall Profile Policy:
        // Outbound = Allow; Inbound = Block
        if (!inbound) {
            packetsPermitted_++;
            bytesPermitted_ += byteCount;
            return true;
        } else {
            packetsBlocked_++;
            bytesBlocked_ += byteCount;
            threatEngine.recordStealthDrop();
            return false;
        }
    }

    [[nodiscard]] uint64_t getTotalClassified() const noexcept { return totalClassified_; }
    [[nodiscard]] uint64_t getPacketsPermitted() const noexcept { return packetsPermitted_; }
    [[nodiscard]] uint64_t getPacketsBlocked() const noexcept { return packetsBlocked_; }
    [[nodiscard]] uint64_t getBytesPermitted() const noexcept { return bytesPermitted_; }
    [[nodiscard]] uint64_t getBytesBlocked() const noexcept { return bytesBlocked_; }
    [[nodiscard]] size_t getRuleCount() const noexcept { return rules_.size(); }

private:
    WfpKernelFilterEngine() {
        seedCanonicalFirewallRules();
    }

    void seedCanonicalFirewallRules() {
        // Pre-seeded sovereign firewall rules:
        // 1. Inbound Permit: Remote Executive Console (TCP 23 / 8080)
        WfpKernelFilterRule rConsole23{};
        rConsole23.filterId = 101;
        rConsole23.name = "Allow-Remote-Executive-Console-TCP-23";
        rConsole23.direction = FWP_DIRECTION_INBOUND;
        rConsole23.protocol = tcpip::IPPROTO_TCP;
        rConsole23.localPortStart = 23;
        rConsole23.action = FWP_ACTION_PERMIT;
        rConsole23.weight = 5000;
        rules_.push_back(rConsole23);

        WfpKernelFilterRule rConsole8080{};
        rConsole8080.filterId = 102;
        rConsole8080.name = "Allow-Remote-Executive-Console-TCP-8080";
        rConsole8080.direction = FWP_DIRECTION_INBOUND;
        rConsole8080.protocol = tcpip::IPPROTO_TCP;
        rConsole8080.localPortStart = 8080;
        rConsole8080.action = FWP_ACTION_PERMIT;
        rConsole8080.weight = 5000;
        rules_.push_back(rConsole8080);

        // 2. Inbound Permit: DHCP Client (UDP 68)
        WfpKernelFilterRule rDhcp{};
        rDhcp.filterId = 103;
        rDhcp.name = "Allow-DHCP-Client-UDP-68";
        rDhcp.direction = FWP_DIRECTION_INBOUND;
        rDhcp.protocol = tcpip::IPPROTO_UDP;
        rDhcp.localPortStart = 68;
        rDhcp.action = FWP_ACTION_PERMIT;
        rDhcp.weight = 5000;
        rules_.push_back(rDhcp);

        // 3. Inbound Permit: DNS Client (UDP 53)
        WfpKernelFilterRule rDns{};
        rDns.filterId = 104;
        rDns.name = "Allow-DNS-Client-UDP-53";
        rDns.direction = FWP_DIRECTION_INBOUND;
        rDns.protocol = tcpip::IPPROTO_UDP;
        rDns.localPortStart = 53;
        rDns.action = FWP_ACTION_PERMIT;
        rDns.weight = 5000;
        rules_.push_back(rDns);

        // 4. Inbound Permit: ICMP Echo Requests (Ping)
        WfpKernelFilterRule rIcmp{};
        rIcmp.filterId = 105;
        rIcmp.name = "Allow-ICMPv4-Echo-Request";
        rIcmp.direction = FWP_DIRECTION_INBOUND;
        rIcmp.protocol = tcpip::IPPROTO_ICMP;
        rIcmp.action = FWP_ACTION_PERMIT;
        rIcmp.weight = 4000;
        rules_.push_back(rIcmp);

        // 5. Outbound Permit: All Traffic
        WfpKernelFilterRule rOutAll{};
        rOutAll.filterId = 106;
        rOutAll.name = "Allow-Outbound-All";
        rOutAll.direction = FWP_DIRECTION_OUTBOUND;
        rOutAll.protocol = 0; // ANY
        rOutAll.action = FWP_ACTION_PERMIT;
        rOutAll.weight = 1000;
        rules_.push_back(rOutAll);

        std::sort(rules_.begin(), rules_.end(), [](const auto& a, const auto& b) {
            return a.weight > b.weight;
        });
    }

    mutable std::mutex mutex_;
    uint64_t nextRuleId_{2000};
    uint64_t totalClassified_{0};
    uint64_t packetsPermitted_{0};
    uint64_t packetsBlocked_{0};
    uint64_t bytesPermitted_{0};
    uint64_t bytesBlocked_{0};
    std::vector<WfpKernelFilterRule> rules_;
};

// ============================================================================
// 7. Windows Defender Firewall Core Driver (mpsdrv.sys at \Device\MpsDrv)
// ============================================================================

struct MpsFirewallStats {
    uint64_t totalClassified{0};
    uint64_t packetsPermitted{0};
    uint64_t packetsBlocked{0};
    uint64_t stealthDrops{0};
    uint64_t portScansDetected{0};
    uint64_t activeConntrackFlows{0};
};

class MpsFirewallDriver {
public:
    static MpsFirewallDriver& get() {
        static MpsFirewallDriver instance;
        return instance;
    }

    void resetForTest() {
        std::lock_guard<std::mutex> lock(mutex_);
        domainProfileEnabled_ = true;
        privateProfileEnabled_ = true;
        publicProfileEnabled_ = true;
        WfpKernelFilterEngine::get().resetForTest();
        WfpConntrackEngine::get().resetForTest();
        WfpThreatMitigationEngine::get().resetForTest();
    }

    [[nodiscard]] bool isProfileEnabled(uint32_t profileType) const noexcept {
        if (profileType & FW_PROFILE_TYPE_DOMAIN) return domainProfileEnabled_;
        if (profileType & FW_PROFILE_TYPE_PRIVATE) return privateProfileEnabled_;
        if (profileType & FW_PROFILE_TYPE_PUBLIC) return publicProfileEnabled_;
        return false;
    }

    void setProfileEnabled(uint32_t profileType, bool enabled) noexcept {
        if (profileType & FW_PROFILE_TYPE_DOMAIN) domainProfileEnabled_ = enabled;
        if (profileType & FW_PROFILE_TYPE_PRIVATE) privateProfileEnabled_ = enabled;
        if (profileType & FW_PROFILE_TYPE_PUBLIC) publicProfileEnabled_ = enabled;
    }

    MpsFirewallStats getStatistics() const {
        auto& engine = WfpKernelFilterEngine::get();
        auto& threat = WfpThreatMitigationEngine::get();
        auto& conntrack = WfpConntrackEngine::get();

        MpsFirewallStats st{};
        st.totalClassified = engine.getTotalClassified();
        st.packetsPermitted = engine.getPacketsPermitted();
        st.packetsBlocked = engine.getPacketsBlocked();
        st.stealthDrops = threat.getStealthDropCount();
        st.portScansDetected = threat.getPortScansDetected();
        st.activeConntrackFlows = conntrack.getActiveFlowCount();
        return st;
    }

    /**
     * @brief Dispatches IOCTL requests sent to \Device\MpsDrv.
     */
    NtStatus dispatchDeviceControl(
        uint32_t ioctlCode,
        const void* inBuf,
        size_t inSize,
        void* outBuf,
        size_t outSize,
        size_t& bytesReturned
    ) {
        bytesReturned = 0;
        switch (ioctlCode) {
            case IOCTL_MPS_GET_PROFILE: {
                if (outSize < sizeof(uint32_t) || !outBuf) return NtStatus::BufferTooSmall;
                uint32_t prof = (domainProfileEnabled_ ? FW_PROFILE_TYPE_DOMAIN : 0) |
                                (privateProfileEnabled_ ? FW_PROFILE_TYPE_PRIVATE : 0) |
                                (publicProfileEnabled_ ? FW_PROFILE_TYPE_PUBLIC : 0);
                std::memcpy(outBuf, &prof, sizeof(uint32_t));
                bytesReturned = sizeof(uint32_t);
                return NtStatus::Success;
            }
            case IOCTL_MPS_SET_PROFILE: {
                if (inSize < sizeof(uint32_t) || !inBuf) return NtStatus::InvalidParameter;
                uint32_t prof = 0;
                std::memcpy(&prof, inBuf, sizeof(uint32_t));
                domainProfileEnabled_ = (prof & FW_PROFILE_TYPE_DOMAIN) != 0;
                privateProfileEnabled_ = (prof & FW_PROFILE_TYPE_PRIVATE) != 0;
                publicProfileEnabled_ = (prof & FW_PROFILE_TYPE_PUBLIC) != 0;
                return NtStatus::Success;
            }
            case IOCTL_MPS_GET_STATISTICS: {
                if (outSize < sizeof(MpsFirewallStats) || !outBuf) return NtStatus::BufferTooSmall;
                MpsFirewallStats st = getStatistics();
                std::memcpy(outBuf, &st, sizeof(MpsFirewallStats));
                bytesReturned = sizeof(MpsFirewallStats);
                return NtStatus::Success;
            }
            case IOCTL_MPS_RESET_STATISTICS: {
                resetForTest();
                return NtStatus::Success;
            }
            default:
                return NtStatus::NotImplemented;
        }
    }

private:
    MpsFirewallDriver() = default;

    mutable std::mutex mutex_;
    bool domainProfileEnabled_{true};
    bool privateProfileEnabled_{true};
    bool publicProfileEnabled_{true};
};

// ============================================================================
// 8. Win32 / Kernel ABI Exports (fwpkclnt.sys)
// ============================================================================

inline uint32_t WINAPI FwpsCalloutRegister0(
    void* deviceObject,
    const FWPS_CALLOUT0* callout,
    uint32_t* calloutId
) noexcept {
    return WfpCalloutRegistry::get().registerCallout(deviceObject, callout, calloutId);
}

inline uint32_t WINAPI FwpsCalloutUnregisterById0(uint32_t calloutId) noexcept {
    return WfpCalloutRegistry::get().unregisterCallout(calloutId);
}

inline uint32_t WINAPI FwpsFlowAssociateContext0(
    uint64_t flowId,
    uint16_t layerId,
    uint32_t calloutId,
    uint64_t flowContext
) noexcept {
    return WfpCalloutRegistry::get().associateFlowContext(flowId, layerId, calloutId, flowContext);
}

inline uint32_t WINAPI FwpsFlowRemoveContext0(
    uint64_t flowId,
    uint16_t layerId,
    uint32_t calloutId
) noexcept {
    return WfpCalloutRegistry::get().removeFlowContext(flowId, layerId, calloutId);
}

// ============================================================================
// 9. Kernel Driver & SCM Subsystem Initialization
// ============================================================================

inline void InitializeWfpKernelDriver() {
    // 1. Hook WFP Kernel Filter Engine directly into NetworkStack
    auto& net = tcpip::NetworkStack::get();
    net.setPacketFilterCallback([](
        bool inbound,
        tcpip::Ipv4Address srcIp,
        tcpip::Ipv4Address dstIp,
        uint8_t proto,
        std::span<const uint8_t> transportData
    ) -> bool {
        return WfpKernelFilterEngine::get().classifyPacket(inbound, srcIp, dstIp, proto, transportData);
    });

    // 2. Register Dynamic Exports in JanusLDR
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("fwpkclnt.sys", "FwpsCalloutRegister0", reinterpret_cast<void*>(FwpsCalloutRegister0));
    ldr.registerExport("fwpkclnt.sys", "FwpsCalloutUnregisterById0", reinterpret_cast<void*>(FwpsCalloutUnregisterById0));
    ldr.registerExport("fwpkclnt.sys", "FwpsFlowAssociateContext0", reinterpret_cast<void*>(FwpsFlowAssociateContext0));
    ldr.registerExport("fwpkclnt.sys", "FwpsFlowRemoveContext0", reinterpret_cast<void*>(FwpsFlowRemoveContext0));

    // 3. Register SCM Service Records
    auto& scm = scm::ServiceControlManager::get();

    // fwpkclnt.sys (Kernel Driver)
    auto fwpkclntSvc = std::make_shared<scm::ServiceRecord>();
    fwpkclntSvc->serviceName = L"fwpkclnt";
    fwpkclntSvc->displayName = L"Windows Filtering Platform Kernel Client Driver";
    fwpkclntSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    fwpkclntSvc->startType = scm::SERVICE_BOOT_START;
    fwpkclntSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    fwpkclntSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\fwpkclnt.sys";
    fwpkclntSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(fwpkclntSvc);

    // mpsdrv.sys (Firewall Driver)
    auto mpsdrvSvc = std::make_shared<scm::ServiceRecord>();
    mpsdrvSvc->serviceName = L"mpsdrv";
    mpsdrvSvc->displayName = L"Windows Defender Firewall Core Driver";
    mpsdrvSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    mpsdrvSvc->startType = scm::SERVICE_BOOT_START;
    mpsdrvSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    mpsdrvSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\mpsdrv.sys";
    mpsdrvSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(mpsdrvSvc);

    // bfe (Base Filtering Engine Service)
    auto bfeSvc = std::make_shared<scm::ServiceRecord>();
    bfeSvc->serviceName = L"bfe";
    bfeSvc->displayName = L"Base Filtering Engine";
    bfeSvc->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    bfeSvc->startType = scm::SERVICE_AUTO_START;
    bfeSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    bfeSvc->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k LocalServiceNoNetwork";
    bfeSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(bfeSvc);

    // mpssvc (Windows Defender Firewall Service)
    auto mpsSvc = std::make_shared<scm::ServiceRecord>();
    mpsSvc->serviceName = L"mpssvc";
    mpsSvc->displayName = L"Windows Defender Firewall";
    mpsSvc->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    mpsSvc->startType = scm::SERVICE_AUTO_START;
    mpsSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    mpsSvc->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k LocalServiceNoNetwork";
    mpsSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(mpsSvc);

    // 4. Register Modules in VersionDatabase
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("fwpkclnt.sys", "10.0.26100.5900", "MicaNT WFP Kernel-Mode Client Driver");
    verDb.RegisterModule("mpsdrv.sys", "10.0.26100.5900", "MicaNT Windows Defender Firewall Driver");
    verDb.RegisterModule("bfe.dll", "10.0.26100.5900", "MicaNT Base Filtering Engine");
}

} // namespace micant::wfp::km
