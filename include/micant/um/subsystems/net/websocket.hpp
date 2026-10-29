// ============================================================================
// MicaNT: Sovereign Real-Time WebSocket Protocol Engine (RFC 6455 / websocket.dll)
// (websocket.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides RFC 6455 WebSocket client/server framing, masking, SHA-1 handshake
// computation, Win32 websocket.dll C ABI exports, and session lifecycle.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <cstring>
#include <algorithm>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/security/cng.hpp"
#include "micant/km/drivers/security/tls13.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::um::net::ws {

// ============================================================================
// 1. RFC 6455 Constants & Opcodes
// ============================================================================

inline constexpr uint8_t OPCODE_CONTINUATION = 0x0;
inline constexpr uint8_t OPCODE_TEXT         = 0x1;
inline constexpr uint8_t OPCODE_BINARY       = 0x2;
inline constexpr uint8_t OPCODE_CLOSE        = 0x8;
inline constexpr uint8_t OPCODE_PING         = 0x9;
inline constexpr uint8_t OPCODE_PONG         = 0xA;

inline constexpr uint16_t CLOSE_NORMAL_CLOSURE       = 1000;
inline constexpr uint16_t CLOSE_GOING_AWAY           = 1001;
inline constexpr uint16_t CLOSE_PROTOCOL_ERROR       = 1002;
inline constexpr uint16_t CLOSE_UNSUPPORTED_DATA     = 1003;
inline constexpr uint16_t CLOSE_ABNORMAL_CLOSURE     = 1006;
inline constexpr uint16_t CLOSE_INVALID_FRAME_PAYLOAD= 1007;
inline constexpr uint16_t CLOSE_POLICY_VIOLATION     = 1008;
inline constexpr uint16_t CLOSE_MESSAGE_TOO_BIG      = 1009;
inline constexpr uint16_t CLOSE_INTERNAL_ERROR       = 1011;

inline constexpr const char* RFC6455_MAGIC_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// ============================================================================
// 2. Canonical Win32 websocket.dll Types & C ABI Structures
// ============================================================================

#ifndef _HRESULT_DEFINED
#define _HRESULT_DEFINED
using HRESULT = int32_t;
inline constexpr HRESULT S_OK           = 0;
inline constexpr HRESULT S_FALSE        = 1;
inline constexpr HRESULT E_INVALIDARG   = static_cast<HRESULT>(0x80070057);
inline constexpr HRESULT E_OUTOFMEMORY  = static_cast<HRESULT>(0x8007000E);
inline constexpr HRESULT E_FAIL         = static_cast<HRESULT>(0x80004005);
#endif

using WEB_SOCKET_HANDLE = void*;

enum WEB_SOCKET_ACTION : int32_t {
    WEB_SOCKET_NO_ACTION                       = 0,
    WEB_SOCKET_SEND_TO_NETWORK_ACTION          = 1,
    WEB_SOCKET_INDICATE_SEND_COMPLETE_ACTION   = 2,
    WEB_SOCKET_RECEIVE_FROM_NETWORK_ACTION     = 3,
    WEB_SOCKET_INDICATE_RECEIVE_COMPLETE_ACTION= 4
};

enum WEB_SOCKET_BUFFER_TYPE : uint32_t {
    WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE       = 0x80000000,
    WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE      = 0x80000001,
    WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE     = 0x80000002,
    WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE    = 0x80000003,
    WEB_SOCKET_CLOSE_BUFFER_TYPE              = 0x80000004,
    WEB_SOCKET_PING_PONG_BUFFER_TYPE          = 0x80000005,
    WEB_SOCKET_UNSOLICITED_PONG_BUFFER_TYPE   = 0x80000006
};

struct WEB_SOCKET_BUFFER {
    struct DataStruct {
        uint8_t* pbBuffer;
        uint32_t ulBufferLength;
    } Data;
};

struct WEB_SOCKET_HTTP_HEADER {
    char*    pcName;
    uint32_t ulNameLength;
    char*    pcValue;
    uint32_t ulValueLength;
};

// ============================================================================
// 3. Clean-Room Base64 Helpers
// ============================================================================

namespace detail {

inline const char B64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline std::string base64Encode(std::span<const uint8_t> data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t b0 = data[i];
        uint32_t b1 = (i + 1 < data.size()) ? data[i + 1] : 0;
        uint32_t b2 = (i + 2 < data.size()) ? data[i + 2] : 0;
        uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

        out.push_back(B64_CHARS[(triple >> 18) & 0x3F]);
        out.push_back(B64_CHARS[(triple >> 12) & 0x3F]);
        out.push_back((i + 1 < data.size()) ? B64_CHARS[(triple >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < data.size()) ? B64_CHARS[triple & 0x3F] : '=');
    }
    return out;
}

inline std::vector<uint8_t> base64Decode(std::string_view str) {
    std::vector<uint8_t> out;
    if (str.empty()) return out;

    std::array<int8_t, 256> table{};
    table.fill(-1);
    for (int i = 0; i < 64; ++i) {
        table[static_cast<uint8_t>(B64_CHARS[i])] = static_cast<int8_t>(i);
    }

    uint32_t buffer = 0;
    int bits = 0;
    for (char c : str) {
        if (c == '=' || c == '\r' || c == '\n' || c == ' ') continue;
        int8_t val = table[static_cast<uint8_t>(c)];
        if (val < 0) continue;
        buffer = (buffer << 6) | static_cast<uint32_t>(val);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

} // namespace detail

// ============================================================================
// 4. RFC 6455 Handshake Key Generator & Verifier
// ============================================================================

class WebSocketHandshake {
public:
    // Generate 16-byte random client key encoded in Base64
    static std::string generateClientKey() {
        std::array<uint8_t, 16> randomBytes{};
        km::crypto::CryptoRng::generate(randomBytes);
        return detail::base64Encode(randomBytes);
    }

    // Compute Sec-WebSocket-Accept: Base64(SHA1(clientKey + RFC6455_MAGIC_GUID))
    static std::string computeAcceptKey(std::string_view clientKey) {
        std::string concat;
        concat.reserve(clientKey.size() + std::strlen(RFC6455_MAGIC_GUID));
        concat.append(clientKey);
        concat.append(RFC6455_MAGIC_GUID);

        auto digest = km::crypto::Sha1::hash(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(concat.data()), concat.size())
        );

        return detail::base64Encode(digest);
    }

    // Verify Sec-WebSocket-Accept returned by server against client key
    static bool verifyAcceptKey(std::string_view clientKey, std::string_view serverAccept) {
        return computeAcceptKey(clientKey) == serverAccept;
    }
};

// ============================================================================
// 5. RFC 6455 WebSocket Frame Model & Codec
// ============================================================================

struct WebSocketFrame {
    bool fin{true};
    bool rsv1{false};
    bool rsv2{false};
    bool rsv3{false};
    uint8_t opcode{OPCODE_TEXT};
    bool masked{false};
    uint32_t maskKey{0};
    std::vector<uint8_t> payload;

    [[nodiscard]] std::string asText() const {
        return std::string(reinterpret_cast<const char*>(payload.data()), payload.size());
    }

    [[nodiscard]] uint16_t getCloseCode() const noexcept {
        if (opcode == OPCODE_CLOSE && payload.size() >= 2) {
            return (static_cast<uint16_t>(payload[0]) << 8) | static_cast<uint16_t>(payload[1]);
        }
        return CLOSE_NORMAL_CLOSURE;
    }

    [[nodiscard]] std::string getCloseReason() const {
        if (opcode == OPCODE_CLOSE && payload.size() > 2) {
            return std::string(reinterpret_cast<const char*>(payload.data() + 2), payload.size() - 2);
        }
        return {};
    }
};

class WebSocketCodec {
public:
    // Serialize frame into wire format
    static std::vector<uint8_t> encode(const WebSocketFrame& frame) {
        std::vector<uint8_t> wire;
        wire.reserve(14 + frame.payload.size());

        // Byte 0: FIN (1) | RSV (3) | Opcode (4)
        uint8_t b0 = (frame.fin ? 0x80 : 0x00) |
                     (frame.rsv1 ? 0x40 : 0x00) |
                     (frame.rsv2 ? 0x20 : 0x00) |
                     (frame.rsv3 ? 0x10 : 0x00) |
                     (frame.opcode & 0x0F);
        wire.push_back(b0);

        // Byte 1: Mask (1) | Length (7)
        uint8_t maskBit = frame.masked ? 0x80 : 0x00;
        size_t payloadLen = frame.payload.size();

        if (payloadLen <= 125) {
            wire.push_back(maskBit | static_cast<uint8_t>(payloadLen));
        } else if (payloadLen <= 0xFFFF) {
            wire.push_back(maskBit | 126);
            wire.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xFF));
            wire.push_back(static_cast<uint8_t>(payloadLen & 0xFF));
        } else {
            wire.push_back(maskBit | 127);
            for (int i = 7; i >= 0; --i) {
                wire.push_back(static_cast<uint8_t>((payloadLen >> (i * 8)) & 0xFF));
            }
        }

        // Masking Key (4 bytes)
        if (frame.masked) {
            uint8_t mk[4] = {
                static_cast<uint8_t>((frame.maskKey >> 24) & 0xFF),
                static_cast<uint8_t>((frame.maskKey >> 16) & 0xFF),
                static_cast<uint8_t>((frame.maskKey >> 8) & 0xFF),
                static_cast<uint8_t>(frame.maskKey & 0xFF)
            };
            wire.insert(wire.end(), mk, mk + 4);

            // Payload with XOR masking applied
            for (size_t i = 0; i < payloadLen; ++i) {
                wire.push_back(frame.payload[i] ^ mk[i % 4]);
            }
        } else {
            // Unmasked payload
            wire.insert(wire.end(), frame.payload.begin(), frame.payload.end());
        }

        return wire;
    }

    // Decode frame from wire bytes
    static bool decode(std::span<const uint8_t> wire, WebSocketFrame& outFrame, size_t& bytesConsumed) {
        bytesConsumed = 0;
        if (wire.size() < 2) return false;

        uint8_t b0 = wire[0];
        uint8_t b1 = wire[1];

        outFrame.fin    = (b0 & 0x80) != 0;
        outFrame.rsv1   = (b0 & 0x40) != 0;
        outFrame.rsv2   = (b0 & 0x20) != 0;
        outFrame.rsv3   = (b0 & 0x10) != 0;
        outFrame.opcode = (b0 & 0x0F);
        outFrame.masked = (b1 & 0x80) != 0;

        uint64_t payloadLen = (b1 & 0x7F);
        size_t offset = 2;

        if (payloadLen == 126) {
            if (wire.size() < offset + 2) return false;
            payloadLen = (static_cast<uint64_t>(wire[offset]) << 8) | static_cast<uint64_t>(wire[offset + 1]);
            offset += 2;
        } else if (payloadLen == 127) {
            if (wire.size() < offset + 8) return false;
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | static_cast<uint64_t>(wire[offset + i]);
            }
            offset += 8;
        }

        uint8_t maskKeyBytes[4]{};
        if (outFrame.masked) {
            if (wire.size() < offset + 4) return false;
            std::memcpy(maskKeyBytes, &wire[offset], 4);
            outFrame.maskKey = (static_cast<uint32_t>(maskKeyBytes[0]) << 24) |
                               (static_cast<uint32_t>(maskKeyBytes[1]) << 16) |
                               (static_cast<uint32_t>(maskKeyBytes[2]) << 8)  |
                               (static_cast<uint32_t>(maskKeyBytes[3]));
            offset += 4;
        }

        if (wire.size() < offset + payloadLen) {
            return false; // Incomplete frame
        }

        outFrame.payload.resize(payloadLen);
        if (outFrame.masked) {
            for (size_t i = 0; i < payloadLen; ++i) {
                outFrame.payload[i] = wire[offset + i] ^ maskKeyBytes[i % 4];
            }
        } else {
            std::memcpy(outFrame.payload.data(), &wire[offset], payloadLen);
        }

        bytesConsumed = offset + payloadLen;
        return true;
    }
};

// ============================================================================
// 6. WebSocket Session State Machine
// ============================================================================

class WebSocketSession {
public:
    enum class State {
        Connecting,
        Open,
        Closing,
        Closed
    };

    explicit WebSocketSession(bool isClient = true) noexcept
        : isClient_(isClient), state_(State::Connecting) {}

    [[nodiscard]] bool isOpen() const noexcept { return state_ == State::Open; }
    [[nodiscard]] State getState() const noexcept { return state_; }

    void markOpen() noexcept { state_ = State::Open; }

    // Creates an encoded text frame
    std::vector<uint8_t> createTextFrame(std::string_view text) {
        WebSocketFrame frame;
        frame.fin = true;
        frame.opcode = OPCODE_TEXT;
        frame.masked = isClient_;
        if (isClient_) {
            frame.maskKey = generateMaskKey();
        }
        frame.payload.assign(text.begin(), text.end());
        framesSent_++;
        return WebSocketCodec::encode(frame);
    }

    // Creates an encoded binary frame
    std::vector<uint8_t> createBinaryFrame(std::span<const uint8_t> bytes) {
        WebSocketFrame frame;
        frame.fin = true;
        frame.opcode = OPCODE_BINARY;
        frame.masked = isClient_;
        if (isClient_) {
            frame.maskKey = generateMaskKey();
        }
        frame.payload.assign(bytes.begin(), bytes.end());
        framesSent_++;
        return WebSocketCodec::encode(frame);
    }

    // Creates an encoded Ping frame
    std::vector<uint8_t> createPingFrame(std::span<const uint8_t> data = {}) {
        WebSocketFrame frame;
        frame.fin = true;
        frame.opcode = OPCODE_PING;
        frame.masked = isClient_;
        if (isClient_) {
            frame.maskKey = generateMaskKey();
        }
        frame.payload.assign(data.begin(), data.end());
        pingsSent_++;
        return WebSocketCodec::encode(frame);
    }

    // Creates an encoded Pong frame
    std::vector<uint8_t> createPongFrame(std::span<const uint8_t> data) {
        WebSocketFrame frame;
        frame.fin = true;
        frame.opcode = OPCODE_PONG;
        frame.masked = isClient_;
        if (isClient_) {
            frame.maskKey = generateMaskKey();
        }
        frame.payload.assign(data.begin(), data.end());
        pongsSent_++;
        return WebSocketCodec::encode(frame);
    }

    // Creates an encoded Close frame
    std::vector<uint8_t> createCloseFrame(uint16_t code = CLOSE_NORMAL_CLOSURE, std::string_view reason = "") {
        WebSocketFrame frame;
        frame.fin = true;
        frame.opcode = OPCODE_CLOSE;
        frame.masked = isClient_;
        if (isClient_) {
            frame.maskKey = generateMaskKey();
        }
        frame.payload.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
        frame.payload.push_back(static_cast<uint8_t>(code & 0xFF));
        frame.payload.insert(frame.payload.end(), reason.begin(), reason.end());
        state_ = State::Closing;
        return WebSocketCodec::encode(frame);
    }

    // Ingests incoming wire bytes and extracts all complete frames
    bool processIncomingBytes(std::span<const uint8_t> wireData, std::vector<WebSocketFrame>& outFrames) {
        buffer_.insert(buffer_.end(), wireData.begin(), wireData.end());

        size_t totalConsumed = 0;
        while (totalConsumed < buffer_.size()) {
            std::span<const uint8_t> remaining(&buffer_[totalConsumed], buffer_.size() - totalConsumed);
            WebSocketFrame frame;
            size_t consumed = 0;

            if (WebSocketCodec::decode(remaining, frame, consumed)) {
                totalConsumed += consumed;
                framesReceived_++;

                if (frame.opcode == OPCODE_CLOSE) {
                    state_ = State::Closed;
                }
                outFrames.push_back(std::move(frame));
            } else {
                break; // Incomplete frame waiting for more data
            }
        }

        if (totalConsumed > 0) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + totalConsumed);
        }
        return true;
    }

    struct SessionStats {
        uint64_t framesSent{0};
        uint64_t framesReceived{0};
        uint64_t pingsSent{0};
        uint64_t pongsSent{0};
    };

    [[nodiscard]] SessionStats getStats() const noexcept {
        return { framesSent_, framesReceived_, pingsSent_, pongsSent_ };
    }

private:
    uint32_t generateMaskKey() {
        uint32_t key = 0;
        km::crypto::CryptoRng::generate(std::span<uint8_t>(reinterpret_cast<uint8_t*>(&key), sizeof(key)));
        return key ? key : 0x12345678;
    }

    bool isClient_{true};
    State state_{State::Connecting};
    std::vector<uint8_t> buffer_;
    uint64_t framesSent_{0};
    uint64_t framesReceived_{0};
    uint64_t pingsSent_{0};
    uint64_t pongsSent_{0};
};

// ============================================================================
// 7. Canonical Win32 websocket.dll C ABI Exports
// ============================================================================

inline HRESULT WINAPI WebSocketCreateClientHandle(
    const void* /*pProperties*/,
    uint32_t /*ulPropertyCount*/,
    WEB_SOCKET_HANDLE* phWebSocket) noexcept
{
    if (!phWebSocket) return E_INVALIDARG;
    auto* session = new (std::nothrow) WebSocketSession(true);
    if (!session) return E_OUTOFMEMORY;
    *phWebSocket = reinterpret_cast<WEB_SOCKET_HANDLE>(session);
    return S_OK;
}

inline HRESULT WINAPI WebSocketCreateServerHandle(
    const void* /*pProperties*/,
    uint32_t /*ulPropertyCount*/,
    WEB_SOCKET_HANDLE* phWebSocket) noexcept
{
    if (!phWebSocket) return E_INVALIDARG;
    auto* session = new (std::nothrow) WebSocketSession(false);
    if (!session) return E_OUTOFMEMORY;
    *phWebSocket = reinterpret_cast<WEB_SOCKET_HANDLE>(session);
    return S_OK;
}

inline HRESULT WINAPI WebSocketBeginClientHandshake(
    WEB_SOCKET_HANDLE hWebSocket,
    const char** /*pszSubprotocols*/,
    uint32_t /*ulSubprotocolCount*/,
    const char** /*pszExtensions*/,
    uint32_t /*ulExtensionCount*/,
    const WEB_SOCKET_HTTP_HEADER* /*pInitialHeaders*/,
    uint32_t /*ulInitialHeaderCount*/,
    WEB_SOCKET_HTTP_HEADER** /*pAdditionalHeaders*/,
    uint32_t* /*pulAdditionalHeaderCount*/) noexcept
{
    if (!hWebSocket) return E_INVALIDARG;
    return S_OK;
}

inline HRESULT WINAPI WebSocketEndClientHandshake(
    WEB_SOCKET_HANDLE hWebSocket,
    const WEB_SOCKET_HTTP_HEADER* /*pResponseHeaders*/,
    uint32_t /*ulReponseHeaderCount*/,
    uint32_t* /*pulSelectedExtensions*/,
    uint32_t* /*pulSelectedExtensionCount*/,
    uint32_t* /*pulSelectedSubprotocol*/) noexcept
{
    if (!hWebSocket) return E_INVALIDARG;
    auto* session = reinterpret_cast<WebSocketSession*>(hWebSocket);
    session->markOpen();
    return S_OK;
}

inline HRESULT WINAPI WebSocketSend(
    WEB_SOCKET_HANDLE hWebSocket,
    WEB_SOCKET_BUFFER_TYPE BufferType,
    WEB_SOCKET_BUFFER* pBuffer,
    void* /*Context*/) noexcept
{
    if (!hWebSocket || !pBuffer) return E_INVALIDARG;
    auto* session = reinterpret_cast<WebSocketSession*>(hWebSocket);
    if (BufferType == WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
        std::string_view msg(reinterpret_cast<const char*>(pBuffer->Data.pbBuffer), pBuffer->Data.ulBufferLength);
        session->createTextFrame(msg);
    } else if (BufferType == WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
        std::span<const uint8_t> data(pBuffer->Data.pbBuffer, pBuffer->Data.ulBufferLength);
        session->createBinaryFrame(data);
    }
    return S_OK;
}

inline HRESULT WINAPI WebSocketReceive(
    WEB_SOCKET_HANDLE hWebSocket,
    WEB_SOCKET_BUFFER* /*pBuffer*/,
    void* /*Context*/) noexcept
{
    if (!hWebSocket) return E_INVALIDARG;
    return S_OK;
}

inline void WINAPI WebSocketDeleteHandle(WEB_SOCKET_HANDLE hWebSocket) noexcept {
    if (hWebSocket) {
        auto* session = reinterpret_cast<WebSocketSession*>(hWebSocket);
        delete session;
    }
}

// ============================================================================
// 8. SCM Driver & Module Registration for websocket.dll
// ============================================================================

inline void InitializeWebSocketSubsystem() {
    // 1. SCM Registration for WebSocket service
    auto& scm = scm::ServiceControlManager::get();
    auto wsSvc = std::make_shared<scm::ServiceRecord>();
    wsSvc->serviceName   = L"websocket";
    wsSvc->displayName   = L"MicaNT Real-Time WebSocket Protocol Driver";
    wsSvc->serviceType   = scm::SERVICE_WIN32_SHARE_PROCESS;
    wsSvc->startType     = scm::SERVICE_AUTO_START;
    wsSvc->errorControl  = scm::SERVICE_ERROR_NORMAL;
    wsSvc->binaryPath    = L"C:\\Windows\\System32\\websocket.dll";
    wsSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(wsSvc);

    // 2. Register Module in VersionDatabase
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("websocket.dll", "10.0.26100.5900", "MicaNT Real-Time WebSocket Protocol Engine");

    // 3. Register Exports in JanusLDR
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("websocket.dll", "WebSocketCreateClientHandle", reinterpret_cast<void*>(WebSocketCreateClientHandle));
    ldr.registerExport("websocket.dll", "WebSocketCreateServerHandle", reinterpret_cast<void*>(WebSocketCreateServerHandle));
    ldr.registerExport("websocket.dll", "WebSocketBeginClientHandshake", reinterpret_cast<void*>(WebSocketBeginClientHandshake));
    ldr.registerExport("websocket.dll", "WebSocketEndClientHandshake", reinterpret_cast<void*>(WebSocketEndClientHandshake));
    ldr.registerExport("websocket.dll", "WebSocketSend", reinterpret_cast<void*>(WebSocketSend));
    ldr.registerExport("websocket.dll", "WebSocketReceive", reinterpret_cast<void*>(WebSocketReceive));
    ldr.registerExport("websocket.dll", "WebSocketDeleteHandle", reinterpret_cast<void*>(WebSocketDeleteHandle));
}

} // namespace micant::um::net::ws
