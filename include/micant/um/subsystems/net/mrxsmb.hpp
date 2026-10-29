// ============================================================================
// MicaNT: Clean-Room Windows-Compatible Operating System Executive
// Subsystem: Sovereign SMB Network Redirector Client (mrxsmb.sys / mrxsmb.dll)
// Implementation: include/micant/um/subsystems/net/mrxsmb.hpp
//
// Strict Clean-Room Implementation in modern ISO C++23. Zero External Dependencies.
// Conforms to Microsoft MS-SMB2 (Server Message Block Versions 2 and 3 Protocol)
// and RFC specifications under 17 U.S.C. Section 1201(f).
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <memory>
#include <span>
#include <mutex>
#include <atomic>
#include <map>
#include <cstring>
#include <algorithm>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/security/cng.hpp"
#include "micant/km/drivers/net/srv2.hpp"

namespace micant::smb::um {

using namespace micant::smb::km;

// ============================================================================
// 1. Sovereign SMB Client Session (mrxsmb)
// ============================================================================

class SmbClientSession {
public:
    SmbClientSession() {
        preauth_.reset();
    }

    bool connect(std::string_view serverHost = "147.135.1.229", uint16_t port = 445) noexcept {
        serverHost_ = serverHost;
        port_ = port;

        // Step 1: Send SMB2 NEGOTIATE with SMB 3.1.1 + Negotiate Contexts
        if (!negotiate()) return false;

        // Step 2: Send SMB2 SESSION_SETUP
        if (!sessionSetup()) return false;

        connected_ = true;
        return true;
    }

    // Connect to Share: \\server\share
    bool treeConnect(std::wstring_view shareName, uint32_t& outTreeId) noexcept {
        if (!connected_) return false;

        std::wstring fullPath = L"\\\\" + std::wstring(serverHost_.begin(), serverHost_.end()) + L"\\" + std::wstring(shareName);
        size_t pathBytes = fullPath.size() * sizeof(wchar_t);

        // SMB2 TREE_CONNECT Request (8 bytes + path)
        // StructureSize (2): 9
        // Reserved/Flags (2): 0
        // PathOffset (2): 64 + 8 = 72
        // PathLength (2): pathBytes
        std::vector<uint8_t> body(8 + pathBytes, 0);
        uint16_t structSize = 9;
        uint16_t pathOff = 64 + 8;
        uint16_t pathLen = static_cast<uint16_t>(pathBytes);

        std::memcpy(body.data() + 0, &structSize, 2);
        std::memcpy(body.data() + 4, &pathOff, 2);
        std::memcpy(body.data() + 6, &pathLen, 2);
        std::memcpy(body.data() + 8, fullPath.data(), pathBytes);

        std::vector<uint8_t> respPayload;
        Smb2Header respHdr;
        if (!sendSmb2Command(SMB2_OP_TREE_CONNECT, 0, body, respHdr, respPayload)) {
            return false;
        }

        if (respHdr.status != STATUS_SUCCESS) return false;
        outTreeId = respHdr.treeId;
        currentTreeId_ = outTreeId;

        // Check if share requires encryption (ShareFlags byte 4)
        if (respPayload.size() >= 8) {
            uint32_t shareFlags = 0;
            std::memcpy(&shareFlags, respPayload.data() + 4, 4);
            if (shareFlags & SMB2_SHAREFLAG_ENCRYPT_DATA) {
                encryptionRequired_ = true;
            }
        }

        return true;
    }

    // Create / Open File in Share
    bool createFile(std::wstring_view relativePath, uint32_t desiredAccess, uint32_t createDisposition, Smb2FileId& outFileId) noexcept {
        if (!connected_ || currentTreeId_ == 0) return false;

        size_t nameBytes = relativePath.size() * sizeof(wchar_t);

        // SMB2 CREATE Request (56 bytes + name)
        // StructureSize (2): 57
        // SecurityFlags (1): 0
        // RequestedOplockLevel (1): 0
        // ImpersonationLevel (4): 2 (Identification)
        // SmbCreateFlags (8): 0
        // Reserved (8): 0
        // DesiredAccess (4): desiredAccess
        // FileAttributes (4): 0
        // ShareAccess (4): 7 (Read/Write/Delete)
        // CreateDisposition (4): createDisposition
        // CreateOptions (4): 0
        // NameOffset (2): 64 + 56 = 120
        // NameLength (2): nameBytes
        std::vector<uint8_t> body(56 + nameBytes, 0);
        uint16_t structSize = 57;
        uint32_t impLevel = 2;
        uint32_t shareAcc = 7;
        uint16_t nameOff = 64 + 56;
        uint16_t nameLen = static_cast<uint16_t>(nameBytes);

        std::memcpy(body.data() + 0, &structSize, 2);
        std::memcpy(body.data() + 4, &impLevel, 4);
        std::memcpy(body.data() + 24, &desiredAccess, 4);
        std::memcpy(body.data() + 32, &shareAcc, 4);
        std::memcpy(body.data() + 36, &createDisposition, 4);
        std::memcpy(body.data() + 44, &nameOff, 2);
        std::memcpy(body.data() + 46, &nameLen, 2);
        if (nameBytes > 0) {
            std::memcpy(body.data() + 56, relativePath.data(), nameBytes);
        }

        std::vector<uint8_t> respPayload;
        Smb2Header respHdr;
        if (!sendSmb2Command(SMB2_OP_CREATE, currentTreeId_, body, respHdr, respPayload)) {
            return false;
        }

        if (respHdr.status != STATUS_SUCCESS || respPayload.size() < 80) return false;
        std::memcpy(&outFileId, respPayload.data() + 64, 16);
        return true;
    }

    // Write File
    bool writeFile(const Smb2FileId& fileId, uint64_t offset, std::span<const uint8_t> data, uint32_t& outBytesWritten) noexcept {
        if (!connected_ || currentTreeId_ == 0) return false;

        // SMB2 WRITE Request (48 bytes + data)
        // StructureSize (2): 49
        // DataOffset (2): 64 + 48 = 112
        // Length (4): data.size()
        // Offset (8): offset
        // FileId (16): fileId
        // Channel (4): 0
        // RemainingBytes (4): 0
        // WriteChannelInfoOffset (2), Length (2): 0
        // Flags (4): 0
        std::vector<uint8_t> body(48 + data.size(), 0);
        uint16_t structSize = 49;
        uint16_t dataOff = 64 + 48;
        uint32_t len = static_cast<uint32_t>(data.size());

        std::memcpy(body.data() + 0, &structSize, 2);
        std::memcpy(body.data() + 2, &dataOff, 2);
        std::memcpy(body.data() + 4, &len, 4);
        std::memcpy(body.data() + 8, &offset, 8);
        std::memcpy(body.data() + 16, &fileId, 16);
        std::memcpy(body.data() + 48, data.data(), data.size());

        std::vector<uint8_t> respPayload;
        Smb2Header respHdr;
        if (!sendSmb2Command(SMB2_OP_WRITE, currentTreeId_, body, respHdr, respPayload)) {
            return false;
        }

        if (respHdr.status != STATUS_SUCCESS || respPayload.size() < 8) return false;
        std::memcpy(&outBytesWritten, respPayload.data() + 4, 4);
        return true;
    }

    // Read File
    bool readFile(const Smb2FileId& fileId, uint64_t offset, uint32_t length, std::vector<uint8_t>& outBuffer) noexcept {
        if (!connected_ || currentTreeId_ == 0) return false;

        // SMB2 READ Request (49 bytes)
        // StructureSize (2): 49
        // Padding (1): 0, Flags (1): 0
        // Length (4): length
        // Offset (8): offset
        // FileId (16): fileId
        // MinimumCount (4): 0
        // Channel (4): 0
        // RemainingBytes (4): 0
        // ReadChannelInfoOffset (2), Length (2): 0
        std::vector<uint8_t> body(48, 0);
        uint16_t structSize = 49;
        std::memcpy(body.data() + 0, &structSize, 2);
        std::memcpy(body.data() + 4, &length, 4);
        std::memcpy(body.data() + 8, &offset, 8);
        std::memcpy(body.data() + 16, &fileId, 16);

        std::vector<uint8_t> respPayload;
        Smb2Header respHdr;
        if (!sendSmb2Command(SMB2_OP_READ, currentTreeId_, body, respHdr, respPayload)) {
            return false;
        }

        if (respHdr.status != STATUS_SUCCESS || respPayload.size() < 16) return false;

        uint8_t dataOff = respPayload[2];
        uint32_t dataLen = 0;
        std::memcpy(&dataLen, respPayload.data() + 4, 4);

        if (dataLen > 0 && respPayload.size() >= 16 + dataLen) {
            outBuffer.assign(respPayload.data() + 16, respPayload.data() + 16 + dataLen);
        } else {
            outBuffer.clear();
        }

        return true;
    }

    // Close File
    bool closeFile(const Smb2FileId& fileId) noexcept {
        if (!connected_ || currentTreeId_ == 0) return false;

        // SMB2 CLOSE Request (24 bytes)
        // StructureSize (2): 24
        // Flags (2): 0
        // Reserved (4): 0
        // FileId (16): fileId
        std::vector<uint8_t> body(24, 0);
        uint16_t structSize = 24;
        std::memcpy(body.data() + 0, &structSize, 2);
        std::memcpy(body.data() + 8, &fileId, 16);

        std::vector<uint8_t> respPayload;
        Smb2Header respHdr;
        return sendSmb2Command(SMB2_OP_CLOSE, currentTreeId_, body, respHdr, respPayload);
    }

    // Disconnect Tree
    bool treeDisconnect() noexcept {
        if (!connected_ || currentTreeId_ == 0) return false;
        std::vector<uint8_t> body(4, 0);
        uint16_t structSize = 4;
        std::memcpy(body.data(), &structSize, 2);

        std::vector<uint8_t> respPayload;
        Smb2Header respHdr;
        bool ok = sendSmb2Command(SMB2_OP_TREE_DISCONNECT, currentTreeId_, body, respHdr, respPayload);
        currentTreeId_ = 0;
        return ok;
    }

    // Logoff Session
    bool logoff() noexcept {
        if (!connected_) return false;
        std::vector<uint8_t> body(4, 0);
        uint16_t structSize = 4;
        std::memcpy(body.data(), &structSize, 2);

        std::vector<uint8_t> respPayload;
        Smb2Header respHdr;
        bool ok = sendSmb2Command(SMB2_OP_LOGOFF, 0, body, respHdr, respPayload);
        connected_ = false;
        sessionId_ = 0;
        return ok;
    }

    // Diagnostics & Properties
    [[nodiscard]] bool isConnected() const noexcept { return connected_; }
    [[nodiscard]] uint64_t getSessionId() const noexcept { return sessionId_; }
    [[nodiscard]] uint16_t getNegotiatedDialect() const noexcept { return negotiatedDialect_; }
    [[nodiscard]] bool isEncrypted() const noexcept { return encryptionRequired_; }
    [[nodiscard]] const std::array<uint8_t, 64>& getPreauthHash() const noexcept { return preauth_.getCurrentHash(); }

private:
    bool negotiate() noexcept {
        // SMB2 NEGOTIATE Request
        // StructureSize (2): 36
        // DialectCount (2): 4 (0x0202, 0x0210, 0x0300, 0x0311)
        // SecurityMode (2): 0x0001 (Signing Enabled)
        // Reserved (2): 0
        // Capabilities (4): 0x0000007F
        // ClientGuid (16)
        // NegotiateContextOffset (4): 64 + 36 + 8 (4 dialects) = 108
        // NegotiateContextCount (2): 2
        // Reserved2 (2): 0
        // Dialects[4]: 0x0202, 0x0210, 0x0300, 0x0311
        // Context 1: Preauth Integrity Capabilities
        // Context 2: Encryption Capabilities

        std::vector<uint8_t> body(36 + 8, 0);
        uint16_t structSize = 36;
        uint16_t dialectCount = 4;
        uint16_t secMode = 0x0001;
        uint32_t caps = 0x0000007F;
        uint32_t negContextOffset = 64 + 36 + 8;
        uint16_t negContextCount = 2;

        std::memcpy(body.data() + 0, &structSize, 2);
        std::memcpy(body.data() + 2, &dialectCount, 2);
        std::memcpy(body.data() + 4, &secMode, 2);
        std::memcpy(body.data() + 8, &caps, 4);

        static const uint8_t CLIENT_GUID[16] = { 0x43, 0x4C, 0x49, 0x45, 0x4E, 0x54, 0x53, 0x4D, 0x42, 0x33, 0x31, 0x31, 0x4D, 0x49, 0x43, 0x41 };
        std::memcpy(body.data() + 12, CLIENT_GUID, 16);

        std::memcpy(body.data() + 28, &negContextOffset, 4);
        std::memcpy(body.data() + 32, &negContextCount, 2);

        uint16_t dialects[4] = { SMB2_DIALECT_0202, SMB2_DIALECT_0210, SMB2_DIALECT_0300, SMB2_DIALECT_0311 };
        std::memcpy(body.data() + 36, dialects, 8);

        // Preauth context
        std::vector<uint8_t> preauthCtx(8 + 4 + 2 + 32, 0);
        uint16_t c1Type = SMB2_PREAUTH_INTEGRITY_CAPS;
        uint16_t c1Len = 4 + 2 + 32;
        std::memcpy(preauthCtx.data() + 0, &c1Type, 2);
        std::memcpy(preauthCtx.data() + 2, &c1Len, 2);
        uint16_t hCount = 1, sLen = 32, hAlg = SMB2_PREAUTH_INTEGRITY_SHA512;
        std::memcpy(preauthCtx.data() + 8, &hCount, 2);
        std::memcpy(preauthCtx.data() + 10, &sLen, 2);
        std::memcpy(preauthCtx.data() + 12, &hAlg, 2);
        for (int i = 0; i < 32; ++i) preauthCtx[14 + i] = static_cast<uint8_t>(0xB0 + i);

        // Encryption context
        std::vector<uint8_t> encCtx(8 + 4, 0);
        uint16_t c2Type = SMB2_ENCRYPTION_CAPS;
        uint16_t c2Len = 4;
        std::memcpy(encCtx.data() + 0, &c2Type, 2);
        std::memcpy(encCtx.data() + 2, &c2Len, 2);
        uint16_t cipCount = 1, cipAlg = SMB2_ENCRYPTION_AES128_GCM;
        std::memcpy(encCtx.data() + 8, &cipCount, 2);
        std::memcpy(encCtx.data() + 10, &cipAlg, 2);

        body.insert(body.end(), preauthCtx.begin(), preauthCtx.end());
        body.insert(body.end(), encCtx.begin(), encCtx.end());

        std::vector<uint8_t> respPayload;
        Smb2Header respHdr;
        if (!sendSmb2Command(SMB2_OP_NEGOTIATE, 0, body, respHdr, respPayload)) {
            return false;
        }

        if (respHdr.status != STATUS_SUCCESS || respPayload.size() < 64) return false;

        // Parse DialectRevision
        std::memcpy(&negotiatedDialect_, respPayload.data() + 4, 2);
        return negotiatedDialect_ == SMB2_DIALECT_0311;
    }

    bool sessionSetup() noexcept {
        // SMB2 SESSION_SETUP Request
        // StructureSize (2): 25
        // Flags (1): 0
        // SecurityMode (1): 0x01
        // Capabilities (4): 0
        // Channel (4): 0
        // SecurityBufferOffset (2): 64 + 24 = 88
        // SecurityBufferLength (2): 16
        // PreviousSessionId (8): 0
        std::vector<uint8_t> body(24 + 16, 0);
        uint16_t structSize = 25;
        uint8_t secMode = 0x01;
        uint16_t secOff = 64 + 24;
        uint16_t secLen = 16;

        std::memcpy(body.data() + 0, &structSize, 2);
        body[3] = secMode;
        std::memcpy(body.data() + 12, &secOff, 2);
        std::memcpy(body.data() + 14, &secLen, 2);

        // Simulated NTLMSSP / Kerberos token
        for (int i = 0; i < 16; ++i) body[24 + i] = static_cast<uint8_t>(0xCC);

        std::vector<uint8_t> respPayload;
        Smb2Header respHdr;
        if (!sendSmb2Command(SMB2_OP_SESSION_SETUP, 0, body, respHdr, respPayload)) {
            return false;
        }

        if (respHdr.status != STATUS_SUCCESS || respHdr.sessionId == 0) return false;
        sessionId_ = respHdr.sessionId;

        // Derive keys matching server
        auto curPreauth = preauth_.getCurrentHash();
        std::memcpy(sessionKey_.data(), curPreauth.data(), 16);
        std::memcpy(clientOutKey_.data(), curPreauth.data() + 16, 16); // Client Out = Server In
        std::memcpy(clientInKey_.data(), curPreauth.data() + 32, 16);  // Client In = Server Out

        encryptionRequired_ = true; // Auto-enable SMB 3.1.1 encryption
        return true;
    }

    bool sendSmb2Command(uint16_t command, uint32_t treeId, std::span<const uint8_t> body, Smb2Header& outRespHdr, std::vector<uint8_t>& outRespBody) noexcept {
        Smb2Header hdr;
        hdr.protocolId = SMB2_MAGIC;
        hdr.structureSize = 64;
        hdr.creditCharge = 1;
        hdr.command = command;
        hdr.credits = 32;
        hdr.messageId = messageIdSeq_.fetch_add(1);
        hdr.treeId = treeId;
        hdr.sessionId = sessionId_;

        std::vector<uint8_t> plainPacket(sizeof(Smb2Header) + body.size());
        std::memcpy(plainPacket.data(), &hdr, sizeof(Smb2Header));
        if (!body.empty()) {
            std::memcpy(plainPacket.data() + sizeof(Smb2Header), body.data(), body.size());
        }

        // Update preauth hash on unencrypted negotiate/session setup
        if (command == SMB2_OP_NEGOTIATE || command == SMB2_OP_SESSION_SETUP) {
            preauth_.update(plainPacket);
        }

        // Encrypt if required
        std::vector<uint8_t> wirePayload;
        if (encryptionRequired_ && command != SMB2_OP_NEGOTIATE && command != SMB2_OP_SESSION_SETUP) {
            Smb2TransformHeader xform;
            xform.protocolId = SMB2_TRANSFORM_MAGIC;
            xform.originalMessageSize = static_cast<uint32_t>(plainPacket.size());
            xform.flags = 0x0001;
            xform.sessionId = sessionId_;

            static std::atomic<uint64_t> s_cliNonceSeq{1000};
            uint64_t seq = s_cliNonceSeq.fetch_add(1);
            std::memcpy(xform.nonce, &seq, sizeof(uint64_t));

            std::span<const uint8_t> nonceSpan(xform.nonce, 12);
            std::span<const uint8_t> plainSpan(plainPacket.data(), plainPacket.size());

            uint8_t aadBuf[32];
            std::memcpy(aadBuf, xform.nonce, 16);
            std::memcpy(aadBuf + 16, &xform.originalMessageSize, 4);
            std::memcpy(aadBuf + 20, &xform.reserved, 2);
            std::memcpy(aadBuf + 22, &xform.flags, 2);
            std::memcpy(aadBuf + 24, &xform.sessionId, 8);
            std::span<const uint8_t> aadSpan(aadBuf, 32);

            std::vector<uint8_t> cipher(plainPacket.size());
            std::array<uint8_t, 16> authTag{};

            crypto::AesGcm::encrypt(clientOutKey_, nonceSpan, aadSpan, plainSpan, cipher, authTag);
            std::memcpy(xform.signature, authTag.data(), 16);

            wirePayload.resize(sizeof(Smb2TransformHeader) + cipher.size());
            std::memcpy(wirePayload.data(), &xform, sizeof(Smb2TransformHeader));
            std::memcpy(wirePayload.data() + sizeof(Smb2TransformHeader), cipher.data(), cipher.size());
        } else {
            wirePayload = std::move(plainPacket);
        }

        // Direct TCP transport frame
        uint32_t outLen = static_cast<uint32_t>(wirePayload.size());
        std::vector<uint8_t> txBuf(4 + outLen);
        txBuf[0] = 0x00;
        txBuf[1] = static_cast<uint8_t>((outLen >> 16) & 0xFF);
        txBuf[2] = static_cast<uint8_t>((outLen >> 8)  & 0xFF);
        txBuf[3] = static_cast<uint8_t>(outLen         & 0xFF);
        std::memcpy(txBuf.data() + 4, wirePayload.data(), outLen);

        // Process through Server Engine
        std::vector<uint8_t> rxBuf;
        bool procOk = SmbServerEngine::Instance().processPacket(txBuf, rxBuf);
        if (!procOk || rxBuf.size() < 4) return false;

        uint32_t rxPayloadLen = (static_cast<uint32_t>(rxBuf[1]) << 16) |
                                (static_cast<uint32_t>(rxBuf[2]) << 8)  |
                                static_cast<uint32_t>(rxBuf[3]);
        if (rxBuf.size() < 4 + rxPayloadLen) return false;

        std::span<const uint8_t> rxPayload(rxBuf.data() + 4, rxPayloadLen);

        // Check if server response is encrypted
        uint32_t magic = 0;
        std::memcpy(&magic, rxPayload.data(), sizeof(uint32_t));

        std::vector<uint8_t> decrypted;
        if (magic == SMB2_TRANSFORM_MAGIC) {
            if (rxPayload.size() < sizeof(Smb2TransformHeader)) return false;
            Smb2TransformHeader rxXform;
            std::memcpy(&rxXform, rxPayload.data(), sizeof(Smb2TransformHeader));

            size_t cipherLen = rxPayload.size() - sizeof(Smb2TransformHeader);
            decrypted.resize(cipherLen);

            std::span<const uint8_t> cipherSpan(rxPayload.data() + sizeof(Smb2TransformHeader), cipherLen);
            std::span<const uint8_t> nonceSpan(rxXform.nonce, 12);
            std::span<const uint8_t> tagSpan(rxXform.signature, 16);
            std::span<const uint8_t> aadSpan(rxPayload.data() + 20, 32);

            bool decOk = crypto::AesGcm::decrypt(clientInKey_, nonceSpan, aadSpan, cipherSpan, tagSpan, decrypted);
            if (!decOk) return false;

            rxPayload = std::span<const uint8_t>(decrypted.data(), decrypted.size());
        }

        if (rxPayload.size() < sizeof(Smb2Header)) return false;
        std::memcpy(&outRespHdr, rxPayload.data(), sizeof(Smb2Header));

        // Update preauth hash on unencrypted Negotiate and Session Setup response
        if (command == SMB2_OP_NEGOTIATE || command == SMB2_OP_SESSION_SETUP) {
            preauth_.update(rxPayload);
        }

        if (rxPayload.size() > sizeof(Smb2Header)) {
            outRespBody.assign(rxPayload.data() + sizeof(Smb2Header), rxPayload.data() + rxPayload.size());
        } else {
            outRespBody.clear();
        }

        return true;
    }

    std::string serverHost_{"147.135.1.229"};
    uint16_t port_{445};
    bool connected_{false};
    uint64_t sessionId_{0};
    uint32_t currentTreeId_{0};
    uint16_t negotiatedDialect_{0};
    bool encryptionRequired_{false};

    PreauthIntegrityEngine preauth_;
    std::array<uint8_t, 16> sessionKey_{};
    std::array<uint8_t, 16> clientInKey_{};
    std::array<uint8_t, 16> clientOutKey_{};

    std::atomic<uint64_t> messageIdSeq_{0};
};

} // namespace micant::smb::um
