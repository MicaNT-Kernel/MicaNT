#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include <span>
#include <queue>
#include <optional>
#include <atomic>
#include <cstring>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ob.hpp"
#include "se.hpp"

namespace micant::lpc {

// ============================================================================
// Standard NT LPC / ALPC Message Types
// ============================================================================
enum class PortMessageType : uint16_t {
    LpcRequest           = 1,
    LpcReply             = 2,
    LpcDatagram          = 3,
    LpcConnectionRequest = 4,
    LpcClientDied        = 5,
    LpcPortClosed        = 6,
    LpcErrorEvent        = 7,
    LpcConnectionReply   = 8
};

// ALPC Message Attribute Flags
inline constexpr uint32_t ALPC_MESSAGE_SECURITY_ATTRIBUTE = 0x80000000;
inline constexpr uint32_t ALPC_MESSAGE_VIEW_ATTRIBUTE     = 0x40000000;
inline constexpr uint32_t ALPC_MESSAGE_CONTEXT_ATTRIBUTE  = 0x20000000;
inline constexpr uint32_t ALPC_MESSAGE_HANDLE_ATTRIBUTE   = 0x10000000;
inline constexpr uint32_t ALPC_VIEWFLG_NOT_AT_BASE        = 0x00010000;
inline constexpr size_t   PORT_MAXIMUM_MESSAGE_LENGTH     = 65536;

/**
 * @brief Standard NT LPC/ALPC Message Header (PORT_MESSAGE).
 */
struct PortMessage {
    struct {
        uint16_t dataLength{0};
        uint16_t totalLength{sizeof(PortMessage)};
    } u1;
    struct {
        uint16_t type{static_cast<uint16_t>(PortMessageType::LpcRequest)};
        uint16_t dataInfoOffset{0};
    } u2;
    ClientId clientId{};
    uint32_t messageId{0};
    uint32_t callbackId{0};
};

/**
 * @brief ALPC Data View Attribute (Section Mapping for Large Payloads).
 */
struct AlpcDataView {
    uint32_t flags{0};
    size_t   sectionOffset{0};
    size_t   viewSize{0};
    void*    viewBase{nullptr};
};

/**
 * @brief ALPC Handle Attribute (Passing object handles across process boundaries).
 */
struct AlpcHandleAttribute {
    uint32_t flags{0};
    uintptr_t handle{0};
    uint32_t desiredAccess{0};
};

/**
 * @brief ALPC Security Attribute (Client identity, impersonation, and MIC level).
 */
struct AlpcSecurityAttribute {
    uint32_t flags{0};
    se::Sid  userSid;
    uint32_t integrityLevel{se::SECURITY_MANDATORY_MEDIUM_RID};
};

/**
 * @brief ALPC Direct Message Attributes Container.
 */
struct AlpcMessageAttributes {
    uint32_t allocatedAttributes{0};
    uint32_t validAttributes{0};
    std::optional<AlpcDataView> view;
    std::optional<AlpcHandleAttribute> handle;
    std::optional<AlpcSecurityAttribute> security;
    uintptr_t context{0};
};

/**
 * @brief Internal LPC Packet containing header, payload, and optional ALPC attributes.
 */
struct PortPacket {
    PortMessage header{};
    std::vector<uint8_t> payload;
    std::optional<AlpcMessageAttributes> attributes;
};

enum class PortType : uint32_t {
    ConnectionPort,          // Named listener port (e.g. \RPC Control\MicaPort)
    ServerCommunicationPort, // Server-side dedicated endpoint
    ClientCommunicationPort  // Client-side dedicated endpoint
};

/**
 * @brief ALPC Port Shared Memory Section.
 * Backs zero-copy large data views between client and server processes.
 */
class AlpcPortSection {
public:
    explicit AlpcPortSection(size_t size) : buffer_(size, 0) {}

    [[nodiscard]] size_t getSize() const noexcept { return buffer_.size(); }
    [[nodiscard]] uint8_t* getBase() noexcept { return buffer_.data(); }
    [[nodiscard]] const uint8_t* getBase() const noexcept { return buffer_.data(); }

    bool writeData(size_t offset, std::span<const uint8_t> data) {
        if (offset + data.size() > buffer_.size()) return false;
        std::memcpy(buffer_.data() + offset, data.data(), data.size());
        return true;
    }

    bool readData(size_t offset, std::span<uint8_t> outBuffer) const {
        if (offset + outBuffer.size() > buffer_.size()) return false;
        std::memcpy(outBuffer.data(), buffer_.data() + offset, outBuffer.size());
        return true;
    }

private:
    std::vector<uint8_t> buffer_;
};

class PortObject;

/**
 * @brief Advanced Local Procedure Call (ALPC) Port Object.
 * Implements high-throughput, synchronous rendezvous message queues,
 * shared data views, and security validation between NT processes.
 */
class PortObject : public std::enable_shared_from_this<PortObject> {
public:
    PortObject(std::wstring name, PortType type)
        : name_(std::move(name)), type_(type) {}

    [[nodiscard]] const std::wstring& getName() const noexcept { return name_; }
    [[nodiscard]] PortType getType() const noexcept { return type_; }
    [[nodiscard]] bool isConnected() const noexcept { return !peerPort_.expired(); }

    void setPeerPort(std::shared_ptr<PortObject> peer) {
        peerPort_ = peer;
    }

    [[nodiscard]] std::shared_ptr<PortObject> getPeerPort() const {
        return peerPort_.lock();
    }

    // Security Descriptor for Port Objects
    void setSecurityDescriptor(se::SecurityDescriptor sd) {
        securityDescriptor_ = std::move(sd);
    }

    [[nodiscard]] const std::optional<se::SecurityDescriptor>& getSecurityDescriptor() const noexcept {
        return securityDescriptor_;
    }

    // Section Management (ALPC Large Data Transfer)
    uintptr_t createSection(size_t size) {
        std::unique_lock<std::mutex> lock(mutex_);
        uintptr_t id = nextSectionId_++;
        sections_[id] = std::make_shared<AlpcPortSection>(size);
        return id;
    }

    bool deleteSection(uintptr_t sectionId) {
        std::unique_lock<std::mutex> lock(mutex_);
        return sections_.erase(sectionId) > 0;
    }

    [[nodiscard]] std::shared_ptr<AlpcPortSection> getSection(uintptr_t sectionId) const {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = sections_.find(sectionId);
        if (it != sections_.end()) return it->second;
        return nullptr;
    }

    bool createDataView(uintptr_t sectionId, size_t offset, size_t size, AlpcDataView& outView) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = sections_.find(sectionId);
        if (it == sections_.end()) return false;
        if (offset + size > it->second->getSize()) return false;

        outView.flags = 0;
        outView.sectionOffset = offset;
        outView.viewSize = size;
        outView.viewBase = it->second->getBase() + offset;
        return true;
    }

    /**
     * @brief Post a message packet into this port's incoming queue.
     */
    NtStatus postMessage(
        const PortMessage& header,
        std::span<const uint8_t> payload,
        const AlpcMessageAttributes* attrs = nullptr
    ) {
        std::unique_lock<std::mutex> lock(mutex_);
        PortPacket packet{
            .header = header,
            .payload = std::vector<uint8_t>(payload.begin(), payload.end())
        };
        packet.header.u1.dataLength = static_cast<uint16_t>(payload.size());
        packet.header.u1.totalLength = static_cast<uint16_t>(sizeof(PortMessage) + payload.size());

        if (attrs) {
            packet.attributes = *attrs;
        }

        inboundQueue_.push(std::move(packet));
        cv_.notify_one();
        return NtStatus::Success;
    }

    /**
     * @brief Sends a message to the connected peer port's incoming queue.
     */
    NtStatus sendMessage(
        const PortMessage& header,
        std::span<const uint8_t> payload,
        const AlpcMessageAttributes* attrs = nullptr
    ) {
        auto peer = peerPort_.lock();
        if (!peer) return NtStatus::PortConnectionRefused;
        return peer->postMessage(header, payload, attrs);
    }

    /**
     * @brief Receive an incoming message from the port queue.
     */
    NtStatus receiveMessage(
        PortMessage& outHeader,
        std::vector<uint8_t>& outPayload,
        uint32_t timeoutMs = 0xFFFFFFFF,
        AlpcMessageAttributes* outAttrs = nullptr
    ) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (inboundQueue_.empty()) {
            if (timeoutMs == 0) {
                return NtStatus::Timeout;
            }
            if (timeoutMs == 0xFFFFFFFF) {
                cv_.wait(lock, [this]() { return !inboundQueue_.empty(); });
            } else {
                bool ok = cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
                    return !inboundQueue_.empty();
                });
                if (!ok) return NtStatus::Timeout;
            }
        }

        auto packet = std::move(inboundQueue_.front());
        inboundQueue_.pop();

        outHeader = packet.header;
        outPayload = std::move(packet.payload);
        if (outAttrs && packet.attributes) {
            *outAttrs = *packet.attributes;
        }
        return NtStatus::Success;
    }

    /**
     * @brief Synchronous LPC Request-Wait-Reply Rendezvous.
     * Posts request to peer communication port and waits for matching reply.
     */
    NtStatus requestWaitReply(
        const PortMessage& reqHeader,
        std::span<const uint8_t> reqPayload,
        PortMessage& replyHeader,
        std::vector<uint8_t>& replyPayload,
        uint32_t timeoutMs = 5000,
        const AlpcMessageAttributes* reqAttrs = nullptr,
        AlpcMessageAttributes* replyAttrs = nullptr
    ) {
        auto peer = peerPort_.lock();
        if (!peer) return NtStatus::PortConnectionRefused;

        // Generate sequential message ID if not provided
        PortMessage msg = reqHeader;
        if (msg.messageId == 0) {
            static std::atomic<uint32_t> s_NextMsgId{1};
            msg.messageId = s_NextMsgId++;
        }
        msg.u2.type = static_cast<uint16_t>(PortMessageType::LpcRequest);

        // Send request to server peer
        NtStatus sendStatus = peer->postMessage(msg, reqPayload, reqAttrs);
        if (!NT_SUCCESS(sendStatus)) return sendStatus;

        // Wait for reply on this port
        auto startTime = std::chrono::steady_clock::now();
        while (true) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= timeoutMs) {
                return NtStatus::Timeout;
            }

            uint32_t remaining = static_cast<uint32_t>(timeoutMs - elapsed);
            PortMessage incoming{};
            std::vector<uint8_t> incomingData;

            NtStatus recStatus = receiveMessage(incoming, incomingData, remaining, replyAttrs);
            if (!NT_SUCCESS(recStatus)) return recStatus;

            // Check if this reply matches our messageId
            if (incoming.messageId == msg.messageId && 
                incoming.u2.type == static_cast<uint16_t>(PortMessageType::LpcReply)) {
                replyHeader = incoming;
                replyPayload = std::move(incomingData);
                return NtStatus::Success;
            }
        }
    }

    /**
     * @brief Send a reply to a previous request across the peer port.
     */
    NtStatus reply(
        const PortMessage& reqHeader,
        std::span<const uint8_t> replyPayload,
        const AlpcMessageAttributes* replyAttrs = nullptr
    ) {
        auto peer = peerPort_.lock();
        if (!peer) return NtStatus::PortConnectionRefused;

        PortMessage replyMsg = reqHeader;
        replyMsg.u2.type = static_cast<uint16_t>(PortMessageType::LpcReply);
        return peer->postMessage(replyMsg, replyPayload, replyAttrs);
    }

    /**
     * @brief Gracefully disconnect the port and notify the peer.
     */
    void disconnect() {
        auto peer = peerPort_.lock();
        if (peer) {
            PortMessage closedMsg{};
            closedMsg.u2.type = static_cast<uint16_t>(PortMessageType::LpcPortClosed);
            peer->postMessage(closedMsg, {});
        }
        peerPort_.reset();
    }

    [[nodiscard]] size_t getQueuedCount() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return inboundQueue_.size();
    }

private:
    std::wstring name_;
    PortType type_;
    std::weak_ptr<PortObject> peerPort_;
    std::optional<se::SecurityDescriptor> securityDescriptor_;
    std::unordered_map<uintptr_t, std::shared_ptr<AlpcPortSection>> sections_;
    std::atomic<uintptr_t> nextSectionId_{1};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<PortPacket> inboundQueue_;
};

/**
 * @brief ALPC Subsystem Manager.
 * Registers, authenticates, and connects message ports under \RPC Control.
 */
class PortManager {
public:
    static PortManager& get() {
        static PortManager instance;
        return instance;
    }

    /**
     * @brief Create a named connection port under \RPC Control\<name>.
     */
    std::shared_ptr<PortObject> createPort(
        std::wstring_view name,
        std::optional<se::SecurityDescriptor> sd = std::nullopt
    ) {
        std::unique_lock<std::mutex> lock(mutex_);
        std::wstring fullName = normalizePortName(name);

        auto it = namedPorts_.find(fullName);
        if (it != namedPorts_.end()) return nullptr;

        auto port = std::make_shared<PortObject>(fullName, PortType::ConnectionPort);
        if (sd) {
            port->setSecurityDescriptor(std::move(*sd));
        }
        namedPorts_[fullName] = port;
        return port;
    }

    /**
     * @brief Connect a client to a named connection port, generating paired endpoints.
     */
    NtStatus connectPort(
        std::wstring_view name,
        std::shared_ptr<PortObject>& clientCommPort,
        std::shared_ptr<PortObject>& serverCommPort,
        const se::TokenObject* callerToken = nullptr
    ) {
        std::unique_lock<std::mutex> lock(mutex_);
        std::wstring fullName = normalizePortName(name);

        auto it = namedPorts_.find(fullName);
        if (it == namedPorts_.end()) {
            return NtStatus::ObjectNameNotFound;
        }

        auto targetPort = it->second;

        // Perform security access check if port has security descriptor and caller token provided
        if (callerToken && targetPort->getSecurityDescriptor()) {
            uint32_t granted = 0;
            NtStatus secStatus = se::SecurityReferenceMonitor::accessCheck(
                *callerToken,
                *targetPort->getSecurityDescriptor(),
                se::GENERIC_READ | se::GENERIC_WRITE,
                granted,
                fullName
            );
            if (!NT_SUCCESS(secStatus)) {
                return NtStatus::AccessDenied;
            }
        }

        // Create paired bidirectional communication endpoints
        clientCommPort = std::make_shared<PortObject>(fullName + L":Client", PortType::ClientCommunicationPort);
        serverCommPort = std::make_shared<PortObject>(fullName + L":Server", PortType::ServerCommunicationPort);

        clientCommPort->setPeerPort(serverCommPort);
        serverCommPort->setPeerPort(clientCommPort);

        return NtStatus::Success;
    }

    [[nodiscard]] std::shared_ptr<PortObject> lookupPort(std::wstring_view name) const {
        std::unique_lock<std::mutex> lock(mutex_);
        std::wstring fullName = normalizePortName(name);
        auto it = namedPorts_.find(fullName);
        if (it != namedPorts_.end()) return it->second;
        return nullptr;
    }

    [[nodiscard]] size_t getPortCount() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return namedPorts_.size();
    }

private:
    PortManager() = default;

    static std::wstring normalizePortName(std::wstring_view name) {
        if (name.starts_with(L"\\RPC Control\\")) {
            return std::wstring(name);
        }
        if (name.starts_with(L"\\")) {
            return std::wstring(L"\\RPC Control") + std::wstring(name);
        }
        return std::wstring(L"\\RPC Control\\") + std::wstring(name);
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::wstring, std::shared_ptr<PortObject>> namedPorts_;
};

} // namespace micant::lpc

namespace micant::alpc {
    using namespace micant::lpc;
}
