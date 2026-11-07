#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <span>
#include <mutex>
#include <cstring>
#include <array>
#include <algorithm>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/ldr.hpp"

namespace micant::crypto {

// ============================================================================
// Standard Windows Cryptography Next Generation (CNG) Constants & Types
// Conforming to microsoft/win32metadata & public MSDN documentation
// ============================================================================

using BCRYPT_HANDLE      = void*;
using BCRYPT_ALG_HANDLE  = void*;
using BCRYPT_KEY_HANDLE  = void*;
using BCRYPT_HASH_HANDLE = void*;
using BCRYPT_SECRET_HANDLE = void*;

using micant::BCryptBuffer;
using micant::BCryptBufferDesc;

#ifndef BCRYPT_KDF_RAW_SECRET
inline constexpr const wchar_t* BCRYPT_KDF_RAW_SECRET = L"TRUNCATE";
inline constexpr const wchar_t* BCRYPT_KDF_HASH       = L"HASH";
inline constexpr const wchar_t* BCRYPT_KDF_HMAC       = L"HMAC";
inline constexpr const wchar_t* BCRYPT_KDF_TLS_PRF    = L"TLS_PRF";
#endif
using NCRYPT_HANDLE      = void*;
using NCRYPT_PROV_HANDLE = void*;
using NCRYPT_KEY_HANDLE  = void*;

// Standard CNG Algorithm Identifiers
inline constexpr const wchar_t* BCRYPT_SHA256_ALGORITHM        = L"SHA256";
inline constexpr const wchar_t* BCRYPT_SHA384_ALGORITHM        = L"SHA384";
inline constexpr const wchar_t* BCRYPT_SHA512_ALGORITHM        = L"SHA512";
inline constexpr const wchar_t* BCRYPT_SHA1_ALGORITHM          = L"SHA1";
inline constexpr const wchar_t* BCRYPT_MD5_ALGORITHM           = L"MD5";
inline constexpr const wchar_t* BCRYPT_AES_ALGORITHM           = L"AES";
inline constexpr const wchar_t* BCRYPT_RNG_ALGORITHM           = L"RNG";
inline constexpr const wchar_t* BCRYPT_HMAC_SHA256_ALGORITHM   = L"HMAC-SHA256";
inline constexpr const wchar_t* BCRYPT_HMAC_SHA384_ALGORITHM   = L"HMAC-SHA384";
inline constexpr const wchar_t* BCRYPT_HMAC_SHA512_ALGORITHM   = L"HMAC-SHA512";
inline constexpr const wchar_t* BCRYPT_RSA_ALGORITHM           = L"RSA";
inline constexpr const wchar_t* BCRYPT_SP800108_CTR_HMAC_ALGORITHM = L"SP800_108_CTR_HMAC";
inline constexpr const wchar_t* BCRYPT_SP800108_FEEDBACK_HMAC_ALGORITHM = L"SP800_108_FEEDBACK_HMAC";
inline constexpr const wchar_t* BCRYPT_SP800108_PIPELINE_HMAC_ALGORITHM = L"SP800_108_PIPELINE_HMAC";
inline constexpr const wchar_t* BCRYPT_SP80056A_CONCAT_ALGORITHM        = L"SP800_56A_CONCAT";
inline constexpr const wchar_t* BCRYPT_HKDF_ALGORITHM                   = L"HKDF";
inline constexpr const wchar_t* BCRYPT_SHA3_256_ALGORITHM               = L"SHA3-256";
inline constexpr const wchar_t* BCRYPT_SHA3_384_ALGORITHM               = L"SHA3-384";
inline constexpr const wchar_t* BCRYPT_SHA3_512_ALGORITHM               = L"SHA3-512";
inline constexpr const wchar_t* BCRYPT_SHA3_224_ALGORITHM               = L"SHA3-224";
inline constexpr const wchar_t* BCRYPT_SHAKE128_ALGORITHM                = L"SHAKE128";
inline constexpr const wchar_t* BCRYPT_SHAKE256_ALGORITHM                = L"SHAKE256";
inline constexpr const wchar_t* BCRYPT_CSHAKE128_ALGORITHM               = L"CSHAKE128";
inline constexpr const wchar_t* BCRYPT_CSHAKE256_ALGORITHM               = L"CSHAKE256";
inline constexpr const wchar_t* BCRYPT_KMAC128_ALGORITHM                 = L"KMAC128";
inline constexpr const wchar_t* BCRYPT_KMAC256_ALGORITHM                 = L"KMAC256";
inline constexpr const wchar_t* BCRYPT_TUPLEHASH128_ALGORITHM             = L"TUPLEHASH128";
inline constexpr const wchar_t* BCRYPT_TUPLEHASH256_ALGORITHM             = L"TUPLEHASH256";
inline constexpr const wchar_t* BCRYPT_TUPLEHASHXOF128_ALGORITHM          = L"TUPLEHASHXOF128";
inline constexpr const wchar_t* BCRYPT_TUPLEHASHXOF256_ALGORITHM          = L"TUPLEHASHXOF256";
inline constexpr const wchar_t* BCRYPT_PARALLELHASH128_ALGORITHM          = L"PARALLELHASH128";
inline constexpr const wchar_t* BCRYPT_PARALLELHASH256_ALGORITHM          = L"PARALLELHASH256";
inline constexpr const wchar_t* BCRYPT_PARALLELHASHXOF128_ALGORITHM       = L"PARALLELHASHXOF128";
inline constexpr const wchar_t* BCRYPT_PARALLELHASHXOF256_ALGORITHM       = L"PARALLELHASHXOF256";
inline constexpr const wchar_t* BCRYPT_KMACXOF128_ALGORITHM               = L"KMACXOF128";
inline constexpr const wchar_t* BCRYPT_KMACXOF256_ALGORITHM               = L"KMACXOF256";
inline constexpr const wchar_t* BCRYPT_KMAC128_PRF_ALGORITHM              = L"KMAC128-PRF";
inline constexpr const wchar_t* BCRYPT_KMAC256_PRF_ALGORITHM              = L"KMAC256-PRF";
inline constexpr const wchar_t* BCRYPT_KMAC128_RATCHET_ALGORITHM          = L"KMAC128-RATCHET";
inline constexpr const wchar_t* BCRYPT_KMAC256_RATCHET_ALGORITHM          = L"KMAC256-RATCHET";
inline constexpr const wchar_t* BCRYPT_KMAC128_DOUBLERATCHET_ALGORITHM    = L"KMAC128-DOUBLERATCHET";
inline constexpr const wchar_t* BCRYPT_KMAC256_DOUBLERATCHET_ALGORITHM    = L"KMAC256-DOUBLERATCHET";
inline constexpr const wchar_t* BCRYPT_KMAC128_TRIPLERATCHET_ALGORITHM    = L"KMAC128-TRIPLERATCHET";
inline constexpr const wchar_t* BCRYPT_KMAC256_TRIPLERATCHET_ALGORITHM    = L"KMAC256-TRIPLERATCHET";
inline constexpr const wchar_t* BCRYPT_KMAC128_TREERATCHET_ALGORITHM      = L"KMAC128-TREERATCHET";
inline constexpr const wchar_t* BCRYPT_KMAC256_TREERATCHET_ALGORITHM      = L"KMAC256-TREERATCHET";
inline constexpr const wchar_t* BCRYPT_KMAC128_ASYNCRATCHET_ALGORITHM    = L"KMAC128-ASYNCRATCHET";
inline constexpr const wchar_t* BCRYPT_KMAC256_ASYNCRATCHET_ALGORITHM    = L"KMAC256-ASYNCRATCHET";
inline constexpr const wchar_t* BCRYPT_KMAC128_KWP_ALGORITHM             = L"KMAC128-KWP";
inline constexpr const wchar_t* BCRYPT_KMAC256_KWP_ALGORITHM             = L"KMAC256-KWP";
inline constexpr const wchar_t* BCRYPT_KMAC128_AEAD_ALGORITHM            = L"KMAC128-AEAD";
inline constexpr const wchar_t* BCRYPT_KMAC256_AEAD_ALGORITHM            = L"KMAC256-AEAD";
inline constexpr const wchar_t* BCRYPT_KMAC128_MGF_ALGORITHM             = L"KMAC128-MGF";
inline constexpr const wchar_t* BCRYPT_KMAC256_MGF_ALGORITHM             = L"KMAC256-MGF";
inline constexpr const wchar_t* BCRYPT_KMAC128_KDF_ALGORITHM             = L"KMAC128-KDF";
inline constexpr const wchar_t* BCRYPT_KMAC256_KDF_ALGORITHM             = L"KMAC256-KDF";
inline constexpr const wchar_t* BCRYPT_KMAC128_CTR_ALGORITHM             = L"KMAC128-CTR";
inline constexpr const wchar_t* BCRYPT_KMAC256_CTR_ALGORITHM             = L"KMAC256-CTR";
inline constexpr const wchar_t* BCRYPT_KMAC128_OFB_ALGORITHM             = L"KMAC128-OFB";
inline constexpr const wchar_t* BCRYPT_KMAC256_OFB_ALGORITHM             = L"KMAC256-OFB";
inline constexpr const wchar_t* BCRYPT_KMAC128_CFB_ALGORITHM             = L"KMAC128-CFB";
inline constexpr const wchar_t* BCRYPT_KMAC256_CFB_ALGORITHM             = L"KMAC256-CFB";
inline constexpr const wchar_t* BCRYPT_KMAC128_SIV_ALGORITHM             = L"KMAC128-SIV";
inline constexpr const wchar_t* BCRYPT_KMAC256_SIV_ALGORITHM             = L"KMAC256-SIV";
inline constexpr const wchar_t* BCRYPT_KMAC128_FFX_ALGORITHM             = L"KMAC128-FFX";
inline constexpr const wchar_t* BCRYPT_KMAC256_FFX_ALGORITHM             = L"KMAC256-FFX";
inline constexpr const wchar_t* BCRYPT_KMAC128_BPS_ALGORITHM             = L"KMAC128-BPS";
inline constexpr const wchar_t* BCRYPT_KMAC256_BPS_ALGORITHM             = L"KMAC256-BPS";
inline constexpr const wchar_t* BCRYPT_KMAC128_KW_ALGORITHM              = L"KMAC128-KW";
inline constexpr const wchar_t* BCRYPT_KMAC256_KW_ALGORITHM              = L"KMAC256-KW";
inline constexpr const wchar_t* BCRYPT_KMAC128_DRBG_ALGORITHM            = L"KMAC128-DRBG";
inline constexpr const wchar_t* BCRYPT_KMAC256_DRBG_ALGORITHM            = L"KMAC256-DRBG";
inline constexpr const wchar_t* BCRYPT_ASCON128_ALGORITHM                = L"ASCON-128";
inline constexpr const wchar_t* BCRYPT_ASCON_HASH_ALGORITHM              = L"ASCON-HASH";
inline constexpr const wchar_t* BCRYPT_ASCON80PQ_ALGORITHM               = L"ASCON-80PQ";
inline constexpr const wchar_t* BCRYPT_ASCON_XOF_ALGORITHM               = L"ASCON-XOF";
inline constexpr const wchar_t* BCRYPT_ASCON128A_ALGORITHM               = L"ASCON-128A";
inline constexpr const wchar_t* BCRYPT_ASCON_XOFA_ALGORITHM              = L"ASCON-XOFA";

#ifndef KDF_HKDF_SALT
inline constexpr uint32_t KDF_HKDF_SALT     = 12;
#endif
#ifndef KDF_HKDF_INFO
inline constexpr uint32_t KDF_HKDF_INFO     = 13;
#endif

#ifndef KDF_ALGORITHMID
inline constexpr uint32_t KDF_ALGORITHMID  = 8;
#endif
#ifndef KDF_PARTYUINFO
inline constexpr uint32_t KDF_PARTYUINFO   = 9;
#endif
#ifndef KDF_PARTYVINFO
inline constexpr uint32_t KDF_PARTYVINFO   = 10;
#endif
#ifndef KDF_SUPPPUBINFO
inline constexpr uint32_t KDF_SUPPPUBINFO  = 11;
#endif
#ifndef KDF_SUPPPRIVINFO
inline constexpr uint32_t KDF_SUPPPRIVINFO = 12;
#endif

#ifndef KDF_LABEL
inline constexpr uint32_t KDF_LABEL   = 9;
#endif
#ifndef KDF_CONTEXT
inline constexpr uint32_t KDF_CONTEXT = 11;
#endif
#ifndef KDF_IV
inline constexpr uint32_t KDF_IV      = 12;
#endif

// Standard CNG Chaining Modes & Properties
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_ECB          = L"ChainingModeECB";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_CBC          = L"ChainingModeCBC";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_CTR          = L"ChainingModeCTR";
inline constexpr const wchar_t* BCRYPT_CHAINING_MODE           = L"ChainingMode";
inline constexpr const wchar_t* BCRYPT_OBJECT_LENGTH           = L"ObjectLength";
inline constexpr const wchar_t* BCRYPT_BLOCK_LENGTH            = L"BlockLength";
inline constexpr const wchar_t* BCRYPT_HASH_LENGTH             = L"HashDigestLength";
inline constexpr const wchar_t* BCRYPT_KEY_LENGTH              = L"KeyLength";
inline constexpr const wchar_t* BCRYPT_KEY_DATA_BLOB           = L"KeyDataBlob";
inline constexpr const wchar_t* BCRYPT_OPAQUE_KEY_BLOB         = L"OpaqueKeyBlob";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_GCM          = L"ChainingModeGCM";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_CCM          = L"ChainingModeCCM";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_XTS          = L"ChainingModeXTS";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_KW           = L"ChainingModeKW";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_KWP          = L"ChainingModeKWP";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_CMAC         = L"ChainingModeCMAC";
inline constexpr const wchar_t* BCRYPT_KEY_WRAP_ALGORITHM      = L"AES-KW";
inline constexpr const wchar_t* BCRYPT_KWP_ALGORITHM           = L"AES-KWP";
inline constexpr const wchar_t* BCRYPT_AES_CMAC_ALGORITHM      = L"AES-CMAC";
inline constexpr const wchar_t* BCRYPT_AUTH_TAG_LENGTH         = L"AuthTagLength";

// Standard CNG Authenticated Mode Structs & Flags
struct BCRYPT_AUTH_TAG_LENGTHS_STRUCT {
    uint32_t dwMinLength{12};
    uint32_t dwMaxLength{16};
    uint32_t dwIncrement{1};
};

struct BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO {
    uint32_t cbSize{sizeof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO)};
    uint32_t dwInfoVersion{1};
    uint8_t* pbNonce{nullptr};
    uint32_t cbNonce{0};
    uint8_t* pbAuthData{nullptr};
    uint32_t cbAuthData{0};
    uint8_t* pbTag{nullptr};
    uint32_t cbTag{0};
    uint8_t* pbMacContext{nullptr};
    uint32_t cbMacContext{0};
    uint32_t cbAAD{0};
    uint64_t cbData{0};
    uint32_t dwFlags{0};
};

inline constexpr uint32_t BCRYPT_AUTH_MODE_INFO_VERSION         = 1;
inline constexpr uint32_t BCRYPT_AUTH_MODE_CHAIN_CALLS_FLAG     = 0x00000001;
inline constexpr uint32_t BCRYPT_AUTH_MODE_IN_PROGRESS_FLAG     = 0x00000002;
inline constexpr NTSTATUS STATUS_AUTH_TAG_MISMATCH             = static_cast<NTSTATUS>(0xC000A002);

// Standard CNG Flags
inline constexpr uint32_t BCRYPT_BLOCK_PADDING                 = 0x00000001;
inline constexpr uint32_t BCRYPT_USE_SYSTEM_PREFERRED_RNG      = 0x00000002;
inline constexpr uint32_t BCRYPT_ALG_HANDLE_HMAC_FLAG          = 0x00000008;

// Standard Key Storage Provider (KSP) Identifiers & Flags
inline constexpr const wchar_t* MS_KEY_STORAGE_PROVIDER        = L"Microsoft Software Key Storage Provider";
inline constexpr const wchar_t* MICA_KEY_STORAGE_PROVIDER      = L"MicaNT Sovereign Key Storage Provider";
inline constexpr uint32_t NCRYPT_PERSIST_ONLY_FLAG             = 0x40000000;
inline constexpr uint32_t NCRYPT_DO_NOT_FINALIZE_FLAG          = 0x00000400;
inline constexpr uint32_t NCRYPT_OVERWRITE_KEY_FLAG           = 0x00000050;

// Standard CNG Status Codes
inline constexpr NTSTATUS STATUS_AUTH_TAG_NEEDS_VERIFY         = static_cast<NTSTATUS>(0xC000A002);
inline constexpr NTSTATUS STATUS_NOT_SUPPORTED                 = static_cast<NTSTATUS>(0xC00000BB);
inline constexpr NTSTATUS STATUS_INVALID_BUFFER_SIZE           = static_cast<NTSTATUS>(0xC0000206);

#ifndef BCRYPT_SUCCESS
#define BCRYPT_SUCCESS(Status) (((int32_t)(Status)) >= 0)
#endif

// ============================================================================
// 1. Clean-Room SHA-256 (FIPS 180-4) Implementation
// ============================================================================
class Sha256 {
public:
    static constexpr size_t DIGEST_SIZE = 32;
    static constexpr size_t BLOCK_SIZE  = 64;

    struct Context {
        uint32_t state[8];
        uint64_t count;
        uint8_t  buffer[64];
    };

    static void init(Context& ctx) noexcept {
        ctx.state[0] = 0x6a09e667;
        ctx.state[1] = 0xbb67ae85;
        ctx.state[2] = 0x3c6ef372;
        ctx.state[3] = 0xa54ff53a;
        ctx.state[4] = 0x510e527f;
        ctx.state[5] = 0x9b05688c;
        ctx.state[6] = 0x1f83d9ab;
        ctx.state[7] = 0x5be0cd19;
        ctx.count    = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        size_t index = static_cast<size_t>(ctx.count & 63);
        ctx.count += data.size();

        size_t partLen = 64 - index;
        size_t i = 0;

        if (data.size() >= partLen) {
            std::memcpy(&ctx.buffer[index], data.data(), partLen);
            transform(ctx.state, ctx.buffer);
            for (i = partLen; i + 63 < data.size(); i += 64) {
                transform(ctx.state, &data[i]);
            }
            index = 0;
        }

        if (i < data.size()) {
            std::memcpy(&ctx.buffer[index], &data[i], data.size() - i);
        }
    }

    static void final(Context& ctx, std::span<uint8_t, 32> digest) noexcept {
        uint64_t totalBits = ctx.count * 8;
        uint8_t bits[8];
        for (int i = 0; i < 8; ++i) {
            bits[7 - i] = static_cast<uint8_t>((totalBits >> (i * 8)) & 0xFF);
        }

        size_t index = static_cast<size_t>(ctx.count & 63);
        size_t padLen = (index < 56) ? (56 - index) : (120 - index);

        static const uint8_t PADDING[64] = { 0x80 };
        update(ctx, std::span<const uint8_t>(PADDING, padLen));
        update(ctx, std::span<const uint8_t>(bits, 8));

        for (int i = 0; i < 8; ++i) {
            digest[i * 4 + 0] = static_cast<uint8_t>((ctx.state[i] >> 24) & 0xFF);
            digest[i * 4 + 1] = static_cast<uint8_t>((ctx.state[i] >> 16) & 0xFF);
            digest[i * 4 + 2] = static_cast<uint8_t>((ctx.state[i] >> 8) & 0xFF);
            digest[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i] & 0xFF);
        }
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, 32>(out.data(), 32));
        return out;
    }

private:
    static constexpr uint32_t rotr(uint32_t x, int n) noexcept {
        return (x >> n) | (x << (32 - n));
    }
    static constexpr uint32_t ch(uint32_t x, uint32_t y, uint32_t z) noexcept {
        return (x & y) ^ (~x & z);
    }
    static constexpr uint32_t maj(uint32_t x, uint32_t y, uint32_t z) noexcept {
        return (x & y) ^ (x & z) ^ (y & z);
    }
    static constexpr uint32_t sigma0(uint32_t x) noexcept {
        return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
    }
    static constexpr uint32_t sigma1(uint32_t x) noexcept {
        return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
    }
    static constexpr uint32_t gamma0(uint32_t x) noexcept {
        return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
    }
    static constexpr uint32_t gamma1(uint32_t x) noexcept {
        return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
    }

    static void transform(uint32_t state[8], const uint8_t block[64]) noexcept {
        static constexpr uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8)  |
                   (static_cast<uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + sigma1(e) + ch(e, f, g) + K[i] + w[i];
            uint32_t t2 = sigma0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }
};

// ============================================================================
// 2. Clean-Room SHA-1 (FIPS 180-1) Implementation
// ============================================================================
class Sha1 {
public:
    static constexpr size_t DIGEST_SIZE = 20;
    static constexpr size_t BLOCK_SIZE  = 64;

    struct Context {
        uint32_t state[5];
        uint64_t count;
        uint8_t  buffer[64];
    };

    static void init(Context& ctx) noexcept {
        ctx.state[0] = 0x67452301;
        ctx.state[1] = 0xefcdab89;
        ctx.state[2] = 0x98badcfe;
        ctx.state[3] = 0x10325476;
        ctx.state[4] = 0xc3d2e1f0;
        ctx.count    = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        size_t index = static_cast<size_t>(ctx.count & 63);
        ctx.count += data.size();

        size_t partLen = 64 - index;
        size_t i = 0;

        if (data.size() >= partLen) {
            std::memcpy(&ctx.buffer[index], data.data(), partLen);
            transform(ctx.state, ctx.buffer);
            for (i = partLen; i + 63 < data.size(); i += 64) {
                transform(ctx.state, &data[i]);
            }
            index = 0;
        }

        if (i < data.size()) {
            std::memcpy(&ctx.buffer[index], &data[i], data.size() - i);
        }
    }

    static void final(Context& ctx, std::span<uint8_t, 20> digest) noexcept {
        uint64_t totalBits = ctx.count * 8;
        uint8_t bits[8];
        for (int i = 0; i < 8; ++i) {
            bits[7 - i] = static_cast<uint8_t>((totalBits >> (i * 8)) & 0xFF);
        }

        size_t index = static_cast<size_t>(ctx.count & 63);
        size_t padLen = (index < 56) ? (56 - index) : (120 - index);

        static const uint8_t PADDING[64] = { 0x80 };
        update(ctx, std::span<const uint8_t>(PADDING, padLen));
        update(ctx, std::span<const uint8_t>(bits, 8));

        for (int i = 0; i < 5; ++i) {
            digest[i * 4 + 0] = static_cast<uint8_t>((ctx.state[i] >> 24) & 0xFF);
            digest[i * 4 + 1] = static_cast<uint8_t>((ctx.state[i] >> 16) & 0xFF);
            digest[i * 4 + 2] = static_cast<uint8_t>((ctx.state[i] >> 8) & 0xFF);
            digest[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i] & 0xFF);
        }
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, 20>(out.data(), 20));
        return out;
    }

private:
    static constexpr uint32_t rotl(uint32_t x, int n) noexcept {
        return (x << n) | (x >> (32 - n));
    }

    static void transform(uint32_t state[5], const uint8_t block[64]) noexcept {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8)  |
                   (static_cast<uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

        for (int i = 0; i < 80; ++i) {
            uint32_t f = 0, k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5a827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ed9eba1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdc;
            } else {
                f = b ^ c ^ d;
                k = 0xca62c1d6;
            }

            uint32_t temp = rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotl(b, 30);
            b = a;
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }
};

// ============================================================================
// 2b. Clean-Room MD5 (RFC 1321) Implementation
// ============================================================================
class Md5 {
public:
    static constexpr size_t DIGEST_SIZE = 16;
    static constexpr size_t BLOCK_SIZE  = 64;

    struct Context {
        uint32_t state[4];
        uint64_t count;
        uint8_t  buffer[64];
    };

    static void init(Context& ctx) noexcept {
        ctx.state[0] = 0x67452301;
        ctx.state[1] = 0xefcdab89;
        ctx.state[2] = 0x98badcfe;
        ctx.state[3] = 0x10325476;
        ctx.count    = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        size_t index = static_cast<size_t>(ctx.count & 63);
        ctx.count += data.size();

        size_t partLen = 64 - index;
        size_t i = 0;

        if (data.size() >= partLen) {
            std::memcpy(&ctx.buffer[index], data.data(), partLen);
            transform(ctx.state, ctx.buffer);

            for (i = partLen; i + 63 < data.size(); i += 64) {
                transform(ctx.state, data.data() + i);
            }
            index = 0;
        }

        if (i < data.size()) {
            std::memcpy(&ctx.buffer[index], data.data() + i, data.size() - i);
        }
    }

    static void final(Context& ctx, std::span<uint8_t, DIGEST_SIZE> digest) noexcept {
        uint8_t bits[8];
        uint64_t bitCount = ctx.count * 8;
        for (int i = 0; i < 8; ++i) {
            bits[i] = static_cast<uint8_t>((bitCount >> (i * 8)) & 0xFF);
        }

        size_t index = static_cast<size_t>(ctx.count & 63);
        size_t padLen = (index < 56) ? (56 - index) : (120 - index);

        static const uint8_t padding[64] = { 0x80 };
        update(ctx, std::span<const uint8_t>(padding, padLen));
        update(ctx, std::span<const uint8_t>(bits, 8));

        for (size_t i = 0; i < 4; ++i) {
            digest[i * 4 + 0] = static_cast<uint8_t>((ctx.state[i] >> 0) & 0xFF);
            digest[i * 4 + 1] = static_cast<uint8_t>((ctx.state[i] >> 8) & 0xFF);
            digest[i * 4 + 2] = static_cast<uint8_t>((ctx.state[i] >> 16) & 0xFF);
            digest[i * 4 + 3] = static_cast<uint8_t>((ctx.state[i] >> 24) & 0xFF);
        }
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, DIGEST_SIZE>(out.data(), DIGEST_SIZE));
        return out;
    }

private:
    static constexpr uint32_t rotl(uint32_t x, uint32_t n) noexcept {
        return (x << n) | (x >> (32 - n));
    }

    static void transform(uint32_t state[4], const uint8_t block[64]) noexcept {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t x[16];

        for (size_t i = 0; i < 16; ++i) {
            x[i] = static_cast<uint32_t>(block[i * 4 + 0]) |
                  (static_cast<uint32_t>(block[i * 4 + 1]) << 8) |
                  (static_cast<uint32_t>(block[i * 4 + 2]) << 16) |
                  (static_cast<uint32_t>(block[i * 4 + 3]) << 24);
        }

        #define MICANT_MD5_F(x, y, z) (((x) & (y)) | ((~x) & (z)))
        #define MICANT_MD5_G(x, y, z) (((x) & (z)) | ((y) & (~z)))
        #define MICANT_MD5_H(x, y, z) ((x) ^ (y) ^ (z))
        #define MICANT_MD5_I(x, y, z) ((y) ^ ((x) | (~z)))

        #define MICANT_MD5_STEP(f, a, b, c, d, x, s, ac) { \
            (a) += f((b), (c), (d)) + (x) + (uint32_t)(ac); \
            (a) = rotl((a), (s)); \
            (a) += (b); \
        }

        // Round 1
        MICANT_MD5_STEP(MICANT_MD5_F, a, b, c, d, x[ 0],  7, 0xd76aa478);
        MICANT_MD5_STEP(MICANT_MD5_F, d, a, b, c, x[ 1], 12, 0xe8c7b756);
        MICANT_MD5_STEP(MICANT_MD5_F, c, d, a, b, x[ 2], 17, 0x242070db);
        MICANT_MD5_STEP(MICANT_MD5_F, b, c, d, a, x[ 3], 22, 0xc1bdceee);
        MICANT_MD5_STEP(MICANT_MD5_F, a, b, c, d, x[ 4],  7, 0xf57c0faf);
        MICANT_MD5_STEP(MICANT_MD5_F, d, a, b, c, x[ 5], 12, 0x4787c62a);
        MICANT_MD5_STEP(MICANT_MD5_F, c, d, a, b, x[ 6], 17, 0xa8304613);
        MICANT_MD5_STEP(MICANT_MD5_F, b, c, d, a, x[ 7], 22, 0xfd469501);
        MICANT_MD5_STEP(MICANT_MD5_F, a, b, c, d, x[ 8],  7, 0x698098d8);
        MICANT_MD5_STEP(MICANT_MD5_F, d, a, b, c, x[ 9], 12, 0x8b44f7af);
        MICANT_MD5_STEP(MICANT_MD5_F, c, d, a, b, x[10], 17, 0xffff5bb1);
        MICANT_MD5_STEP(MICANT_MD5_F, b, c, d, a, x[11], 22, 0x895cd7be);
        MICANT_MD5_STEP(MICANT_MD5_F, a, b, c, d, x[12],  7, 0x6b901122);
        MICANT_MD5_STEP(MICANT_MD5_F, d, a, b, c, x[13], 12, 0xfd987193);
        MICANT_MD5_STEP(MICANT_MD5_F, c, d, a, b, x[14], 17, 0xa679438e);
        MICANT_MD5_STEP(MICANT_MD5_F, b, c, d, a, x[15], 22, 0x49b40821);

        // Round 2
        MICANT_MD5_STEP(MICANT_MD5_G, a, b, c, d, x[ 1],  5, 0xf61e2562);
        MICANT_MD5_STEP(MICANT_MD5_G, d, a, b, c, x[ 6],  9, 0xc040b340);
        MICANT_MD5_STEP(MICANT_MD5_G, c, d, a, b, x[11], 14, 0x265e5a51);
        MICANT_MD5_STEP(MICANT_MD5_G, b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
        MICANT_MD5_STEP(MICANT_MD5_G, a, b, c, d, x[ 5],  5, 0xd62f105d);
        MICANT_MD5_STEP(MICANT_MD5_G, d, a, b, c, x[10],  9, 0x02441453);
        MICANT_MD5_STEP(MICANT_MD5_G, c, d, a, b, x[15], 14, 0xd8a1e681);
        MICANT_MD5_STEP(MICANT_MD5_G, b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
        MICANT_MD5_STEP(MICANT_MD5_G, a, b, c, d, x[ 9],  5, 0x21e1cde6);
        MICANT_MD5_STEP(MICANT_MD5_G, d, a, b, c, x[14],  9, 0xc33707d6);
        MICANT_MD5_STEP(MICANT_MD5_G, c, d, a, b, x[ 3], 14, 0xf4d50d87);
        MICANT_MD5_STEP(MICANT_MD5_G, b, c, d, a, x[ 8], 20, 0x455a14ed);
        MICANT_MD5_STEP(MICANT_MD5_G, a, b, c, d, x[13],  5, 0xa9e3e905);
        MICANT_MD5_STEP(MICANT_MD5_G, d, a, b, c, x[ 2],  9, 0xfcefa3f8);
        MICANT_MD5_STEP(MICANT_MD5_G, c, d, a, b, x[ 7], 14, 0x676f02d9);
        MICANT_MD5_STEP(MICANT_MD5_G, b, c, d, a, x[12], 20, 0x8d2a4c8a);

        // Round 3
        MICANT_MD5_STEP(MICANT_MD5_H, a, b, c, d, x[ 5],  4, 0xfffa3942);
        MICANT_MD5_STEP(MICANT_MD5_H, d, a, b, c, x[ 8], 11, 0x8771f681);
        MICANT_MD5_STEP(MICANT_MD5_H, c, d, a, b, x[11], 16, 0x6d9d6122);
        MICANT_MD5_STEP(MICANT_MD5_H, b, c, d, a, x[14], 23, 0xfde5380c);
        MICANT_MD5_STEP(MICANT_MD5_H, a, b, c, d, x[ 1],  4, 0xa4beea44);
        MICANT_MD5_STEP(MICANT_MD5_H, d, a, b, c, x[ 4], 11, 0x4bdecfa9);
        MICANT_MD5_STEP(MICANT_MD5_H, c, d, a, b, x[ 7], 16, 0xf6bb4b60);
        MICANT_MD5_STEP(MICANT_MD5_H, b, c, d, a, x[10], 23, 0xbebfbc70);
        MICANT_MD5_STEP(MICANT_MD5_H, a, b, c, d, x[13],  4, 0x289b7ec6);
        MICANT_MD5_STEP(MICANT_MD5_H, d, a, b, c, x[ 0], 11, 0xeaa127fa);
        MICANT_MD5_STEP(MICANT_MD5_H, c, d, a, b, x[ 3], 16, 0xd4ef3085);
        MICANT_MD5_STEP(MICANT_MD5_H, b, c, d, a, x[ 6], 23, 0x04881d05);
        MICANT_MD5_STEP(MICANT_MD5_H, a, b, c, d, x[ 9],  4, 0xd9d4d039);
        MICANT_MD5_STEP(MICANT_MD5_H, d, a, b, c, x[12], 11, 0xe6db99e5);
        MICANT_MD5_STEP(MICANT_MD5_H, c, d, a, b, x[15], 16, 0x1fa27cf8);
        MICANT_MD5_STEP(MICANT_MD5_H, b, c, d, a, x[ 2], 23, 0xc4ac5665);

        // Round 4
        MICANT_MD5_STEP(MICANT_MD5_I, a, b, c, d, x[ 0],  6, 0xf4292244);
        MICANT_MD5_STEP(MICANT_MD5_I, d, a, b, c, x[ 7], 10, 0x432aff97);
        MICANT_MD5_STEP(MICANT_MD5_I, c, d, a, b, x[14], 15, 0xab9423a7);
        MICANT_MD5_STEP(MICANT_MD5_I, b, c, d, a, x[ 5], 21, 0xfc93a039);
        MICANT_MD5_STEP(MICANT_MD5_I, a, b, c, d, x[12],  6, 0x655b59c3);
        MICANT_MD5_STEP(MICANT_MD5_I, d, a, b, c, x[ 3], 10, 0x8f0ccc92);
        MICANT_MD5_STEP(MICANT_MD5_I, c, d, a, b, x[10], 15, 0xffeff47d);
        MICANT_MD5_STEP(MICANT_MD5_I, b, c, d, a, x[ 1], 21, 0x85845dd1);
        MICANT_MD5_STEP(MICANT_MD5_I, a, b, c, d, x[ 8],  6, 0x6fa87e4f);
        MICANT_MD5_STEP(MICANT_MD5_I, d, a, b, c, x[15], 10, 0xfe2ce6e0);
        MICANT_MD5_STEP(MICANT_MD5_I, c, d, a, b, x[ 6], 15, 0xa3014314);
        MICANT_MD5_STEP(MICANT_MD5_I, b, c, d, a, x[13], 21, 0x4e0811a1);
        MICANT_MD5_STEP(MICANT_MD5_I, a, b, c, d, x[ 4],  6, 0xf7537e82);
        MICANT_MD5_STEP(MICANT_MD5_I, d, a, b, c, x[11], 10, 0xbd3af235);
        MICANT_MD5_STEP(MICANT_MD5_I, c, d, a, b, x[ 2], 15, 0x2ad7d2bb);
        MICANT_MD5_STEP(MICANT_MD5_I, b, c, d, a, x[ 9], 21, 0xeb86d391);

        #undef MICANT_MD5_STEP
        #undef MICANT_MD5_F
        #undef MICANT_MD5_G
        #undef MICANT_MD5_H
        #undef MICANT_MD5_I

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }
};

// ============================================================================
// 2c. Clean-Room SHA-512 & SHA-384 (FIPS 180-4) Implementation
// ============================================================================
class Sha512 {
public:
    static constexpr size_t DIGEST_SIZE = 64;
    static constexpr size_t BLOCK_SIZE  = 128;

    struct Context {
        uint64_t state[8];
        uint64_t count[2]; // 128-bit bit counter
        uint8_t  buffer[128];
    };

    static void init(Context& ctx) noexcept {
        ctx.state[0] = 0x6a09e667f3bcc908ULL;
        ctx.state[1] = 0xbb67ae8584caa73bULL;
        ctx.state[2] = 0x3c6ef372fe94f82bULL;
        ctx.state[3] = 0xa54ff53a5f1d36f1ULL;
        ctx.state[4] = 0x510e527fade682d1ULL;
        ctx.state[5] = 0x9b05688c2b3e6c1fULL;
        ctx.state[6] = 0x1f83d9abfb41bd6bULL;
        ctx.state[7] = 0x5be0cd19137e2179ULL;
        ctx.count[0] = 0;
        ctx.count[1] = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        size_t index = static_cast<size_t>((ctx.count[0] >> 3) & 127);
        uint64_t bitAdd = static_cast<uint64_t>(data.size()) << 3;
        ctx.count[0] += bitAdd;
        if (ctx.count[0] < bitAdd) {
            ctx.count[1]++;
        }
        ctx.count[1] += static_cast<uint64_t>(data.size()) >> 61;

        size_t partLen = 128 - index;
        size_t i = 0;

        if (data.size() >= partLen) {
            std::memcpy(&ctx.buffer[index], data.data(), partLen);
            transform(ctx.state, ctx.buffer);

            for (i = partLen; i + 127 < data.size(); i += 128) {
                transform(ctx.state, data.data() + i);
            }
            index = 0;
        }

        if (i < data.size()) {
            std::memcpy(&ctx.buffer[index], data.data() + i, data.size() - i);
        }
    }

    static void final(Context& ctx, std::span<uint8_t, DIGEST_SIZE> digest) noexcept {
        uint8_t bits[16];
        for (int i = 0; i < 8; ++i) {
            bits[i]     = static_cast<uint8_t>((ctx.count[1] >> ((7 - i) * 8)) & 0xFF);
            bits[i + 8] = static_cast<uint8_t>((ctx.count[0] >> ((7 - i) * 8)) & 0xFF);
        }

        size_t index = static_cast<size_t>((ctx.count[0] >> 3) & 127);
        size_t padLen = (index < 112) ? (112 - index) : (240 - index);

        static const uint8_t padding[128] = { 0x80 };
        update(ctx, std::span<const uint8_t>(padding, padLen));
        update(ctx, std::span<const uint8_t>(bits, 16));

        for (size_t i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                digest[i * 8 + j] = static_cast<uint8_t>((ctx.state[i] >> ((7 - j) * 8)) & 0xFF);
            }
        }
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, DIGEST_SIZE>(out.data(), DIGEST_SIZE));
        return out;
    }

    static void transform(uint64_t state[8], const uint8_t block[128]) noexcept {
        static constexpr uint64_t K[80] = {
            0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
            0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
            0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
            0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
            0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
            0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
            0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
            0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
            0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
            0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
            0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
            0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
            0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
            0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
            0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
            0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
            0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
            0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
            0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
            0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
        };

        auto rotr = [](uint64_t x, uint32_t n) noexcept -> uint64_t {
            return (x >> n) | (x << (64 - n));
        };

        auto ch = [](uint64_t x, uint64_t y, uint64_t z) noexcept -> uint64_t {
            return (x & y) ^ (~x & z);
        };

        auto maj = [](uint64_t x, uint64_t y, uint64_t z) noexcept -> uint64_t {
            return (x & y) ^ (x & z) ^ (y & z);
        };

        auto s0 = [&](uint64_t x) noexcept -> uint64_t {
            return rotr(x, 28) ^ rotr(x, 34) ^ rotr(x, 39);
        };

        auto s1 = [&](uint64_t x) noexcept -> uint64_t {
            return rotr(x, 14) ^ rotr(x, 18) ^ rotr(x, 41);
        };

        auto g0 = [&](uint64_t x) noexcept -> uint64_t {
            return rotr(x, 1) ^ rotr(x, 8) ^ (x >> 7);
        };

        auto g1 = [&](uint64_t x) noexcept -> uint64_t {
            return rotr(x, 19) ^ rotr(x, 61) ^ (x >> 6);
        };

        uint64_t w[80];
        for (size_t t = 0; t < 16; ++t) {
            w[t] = (static_cast<uint64_t>(block[t * 8 + 0]) << 56) |
                   (static_cast<uint64_t>(block[t * 8 + 1]) << 48) |
                   (static_cast<uint64_t>(block[t * 8 + 2]) << 40) |
                   (static_cast<uint64_t>(block[t * 8 + 3]) << 32) |
                   (static_cast<uint64_t>(block[t * 8 + 4]) << 24) |
                   (static_cast<uint64_t>(block[t * 8 + 5]) << 16) |
                   (static_cast<uint64_t>(block[t * 8 + 6]) << 8)  |
                   (static_cast<uint64_t>(block[t * 8 + 7]));
        }
        for (size_t t = 16; t < 80; ++t) {
            w[t] = g1(w[t - 2]) + w[t - 7] + g0(w[t - 15]) + w[t - 16];
        }

        uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint64_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (size_t t = 0; t < 80; ++t) {
            uint64_t T1 = h + s1(e) + ch(e, f, g) + K[t] + w[t];
            uint64_t T2 = s0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + T1;
            d = c;
            c = b;
            b = a;
            a = T1 + T2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }
};

class Sha384 {
public:
    static constexpr size_t DIGEST_SIZE = 48;
    static constexpr size_t BLOCK_SIZE  = 128;

    using Context = Sha512::Context;

    static void init(Context& ctx) noexcept {
        ctx.state[0] = 0xcbbb9d5dc1059ed8ULL;
        ctx.state[1] = 0x629a292a367cd507ULL;
        ctx.state[2] = 0x9159015a3070dd17ULL;
        ctx.state[3] = 0x152fecd8f70e5939ULL;
        ctx.state[4] = 0x67332667ffc00b31ULL;
        ctx.state[5] = 0x8eb44a8768581511ULL;
        ctx.state[6] = 0xdb0c2e0d64f98fa7ULL;
        ctx.state[7] = 0x47b5481dbefa4fa4ULL;
        ctx.count[0] = 0;
        ctx.count[1] = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        Sha512::update(ctx, data);
    }

    static void final(Context& ctx, std::span<uint8_t, DIGEST_SIZE> digest) noexcept {
        uint8_t fullDigest[64];
        Sha512::final(ctx, std::span<uint8_t, 64>(fullDigest, 64));
        std::memcpy(digest.data(), fullDigest, DIGEST_SIZE);
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, DIGEST_SIZE>(out.data(), DIGEST_SIZE));
        return out;
    }
};

// ============================================================================
// Clean-Room SHA-3 (FIPS 202 Keccak-p[1600, 24]) Engine
// ============================================================================

class KeccakCore {
public:
    static inline uint64_t rotl64(uint64_t x, uint32_t n) noexcept {
        return (x << n) | (x >> (64 - n));
    }

    static void permute(uint64_t st[25]) noexcept {
        static constexpr uint64_t RC[24] = {
            0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
            0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
            0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
            0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
            0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
            0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
            0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
            0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
        };

        static constexpr uint32_t ROTC[24] = {
            1,  3,  6, 10, 15, 21, 28, 36, 45, 55,  2, 14,
            27, 41, 56,  8, 25, 43, 62, 18, 39, 61, 20, 44
        };

        static constexpr uint32_t PILN[24] = {
            10,  7, 11, 17, 18,  3,  5, 16,  8, 21, 24,  4,
            15, 23, 19, 13, 12,  2, 20, 14, 22,  9,  6,  1
        };

        for (int round = 0; round < 24; ++round) {
            // Theta step
            uint64_t bc[5];
            for (int i = 0; i < 5; ++i) {
                bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
            }
            for (int i = 0; i < 5; ++i) {
                uint64_t t = bc[(i + 4) % 5] ^ rotl64(bc[(i + 1) % 5], 1);
                for (int j = 0; j < 25; j += 5) {
                    st[j + i] ^= t;
                }
            }

            // Rho and Pi steps
            uint64_t t = st[1];
            for (int i = 0; i < 24; ++i) {
                uint32_t j = PILN[i];
                bc[0] = st[j];
                st[j] = rotl64(t, ROTC[i]);
                t = bc[0];
            }

            // Chi step
            for (int j = 0; j < 25; j += 5) {
                for (int i = 0; i < 5; ++i) {
                    bc[i] = st[j + i];
                }
                for (int i = 0; i < 5; ++i) {
                    st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
                }
            }

            // Iota step
            st[0] ^= RC[round];
        }
    }
};

class Sha3_256 {
public:
    static constexpr size_t DIGEST_SIZE = 32;
    static constexpr size_t RATE        = 136; // (1600 - 512) / 8 = 136 bytes

    struct Context {
        uint64_t state[25]{};
        uint8_t  buffer[RATE]{};
        size_t   buf_pos{0};
    };

    static void init(Context& ctx) noexcept {
        std::memset(ctx.state, 0, sizeof(ctx.state));
        std::memset(ctx.buffer, 0, sizeof(ctx.buffer));
        ctx.buf_pos = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        for (uint8_t byte : data) {
            ctx.buffer[ctx.buf_pos++] = byte;
            if (ctx.buf_pos == RATE) {
                for (size_t i = 0; i < RATE; ++i) {
                    reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
                }
                KeccakCore::permute(ctx.state);
                ctx.buf_pos = 0;
            }
        }
    }

    static void final(Context& ctx, std::span<uint8_t, DIGEST_SIZE> digest) noexcept {
        ctx.buffer[ctx.buf_pos++] = 0x06;
        while (ctx.buf_pos < RATE) {
            ctx.buffer[ctx.buf_pos++] = 0x00;
        }
        ctx.buffer[RATE - 1] |= 0x80;

        for (size_t i = 0; i < RATE; ++i) {
            reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
        }
        KeccakCore::permute(ctx.state);

        std::memcpy(digest.data(), ctx.state, DIGEST_SIZE);
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, DIGEST_SIZE>(out.data(), DIGEST_SIZE));
        return out;
    }
};

class Sha3_512 {
public:
    static constexpr size_t DIGEST_SIZE = 64;
    static constexpr size_t RATE        = 72; // (1600 - 1024) / 8 = 72 bytes

    struct Context {
        uint64_t state[25]{};
        uint8_t  buffer[RATE]{};
        size_t   buf_pos{0};
    };

    static void init(Context& ctx) noexcept {
        std::memset(ctx.state, 0, sizeof(ctx.state));
        std::memset(ctx.buffer, 0, sizeof(ctx.buffer));
        ctx.buf_pos = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        for (uint8_t byte : data) {
            ctx.buffer[ctx.buf_pos++] = byte;
            if (ctx.buf_pos == RATE) {
                for (size_t i = 0; i < RATE; ++i) {
                    reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
                }
                KeccakCore::permute(ctx.state);
                ctx.buf_pos = 0;
            }
        }
    }

    static void final(Context& ctx, std::span<uint8_t, DIGEST_SIZE> digest) noexcept {
        ctx.buffer[ctx.buf_pos++] = 0x06;
        while (ctx.buf_pos < RATE) {
            ctx.buffer[ctx.buf_pos++] = 0x00;
        }
        ctx.buffer[RATE - 1] |= 0x80;

        for (size_t i = 0; i < RATE; ++i) {
            reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
        }
        KeccakCore::permute(ctx.state);

        std::memcpy(digest.data(), ctx.state, DIGEST_SIZE);
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, DIGEST_SIZE>(out.data(), DIGEST_SIZE));
        return out;
    }
};

class Sha3_384 {
public:
    static constexpr size_t DIGEST_SIZE = 48;
    static constexpr size_t RATE        = 104; // (1600 - 768) / 8 = 104 bytes

    struct Context {
        uint64_t state[25]{};
        uint8_t  buffer[RATE]{};
        size_t   buf_pos{0};
    };

    static void init(Context& ctx) noexcept {
        std::memset(ctx.state, 0, sizeof(ctx.state));
        std::memset(ctx.buffer, 0, sizeof(ctx.buffer));
        ctx.buf_pos = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        for (uint8_t byte : data) {
            ctx.buffer[ctx.buf_pos++] = byte;
            if (ctx.buf_pos == RATE) {
                for (size_t i = 0; i < RATE; ++i) {
                    reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
                }
                KeccakCore::permute(ctx.state);
                ctx.buf_pos = 0;
            }
        }
    }

    static void final(Context& ctx, std::span<uint8_t, DIGEST_SIZE> digest) noexcept {
        ctx.buffer[ctx.buf_pos++] = 0x06;
        while (ctx.buf_pos < RATE) {
            ctx.buffer[ctx.buf_pos++] = 0x00;
        }
        ctx.buffer[RATE - 1] |= 0x80;

        for (size_t i = 0; i < RATE; ++i) {
            reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
        }
        KeccakCore::permute(ctx.state);

        std::memcpy(digest.data(), ctx.state, DIGEST_SIZE);
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, DIGEST_SIZE>(out.data(), DIGEST_SIZE));
        return out;
    }
};

class Sha3_224 {
public:
    static constexpr size_t DIGEST_SIZE = 28;
    static constexpr size_t RATE        = 144; // (1600 - 448) / 8 = 144 bytes

    struct Context {
        uint64_t state[25]{};
        uint8_t  buffer[RATE]{};
        size_t   buf_pos{0};
    };

    static void init(Context& ctx) noexcept {
        std::memset(ctx.state, 0, sizeof(ctx.state));
        std::memset(ctx.buffer, 0, sizeof(ctx.buffer));
        ctx.buf_pos = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        for (uint8_t byte : data) {
            ctx.buffer[ctx.buf_pos++] = byte;
            if (ctx.buf_pos == RATE) {
                for (size_t i = 0; i < RATE; ++i) {
                    reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
                }
                KeccakCore::permute(ctx.state);
                ctx.buf_pos = 0;
            }
        }
    }

    static void final(Context& ctx, std::span<uint8_t, DIGEST_SIZE> digest) noexcept {
        ctx.buffer[ctx.buf_pos++] = 0x06;
        while (ctx.buf_pos < RATE) {
            ctx.buffer[ctx.buf_pos++] = 0x00;
        }
        ctx.buffer[RATE - 1] |= 0x80;

        for (size_t i = 0; i < RATE; ++i) {
            reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
        }
        KeccakCore::permute(ctx.state);

        std::memcpy(digest.data(), ctx.state, DIGEST_SIZE);
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, DIGEST_SIZE>(out.data(), DIGEST_SIZE));
        return out;
    }
};

class Shake128 {
public:
    static constexpr size_t DEFAULT_DIGEST_SIZE = 32;
    static constexpr size_t RATE                = 168; // (1600 - 256) / 8 = 168 bytes

    struct Context {
        uint64_t state[25]{};
        uint8_t  buffer[RATE]{};
        size_t   buf_pos{0};
        bool     squeezed{false};
        size_t   squeeze_pos{0};
    };

    static void init(Context& ctx) noexcept {
        std::memset(ctx.state, 0, sizeof(ctx.state));
        std::memset(ctx.buffer, 0, sizeof(ctx.buffer));
        ctx.buf_pos = 0;
        ctx.squeezed = false;
        ctx.squeeze_pos = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        for (uint8_t byte : data) {
            ctx.buffer[ctx.buf_pos++] = byte;
            if (ctx.buf_pos == RATE) {
                for (size_t i = 0; i < RATE; ++i) {
                    reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
                }
                KeccakCore::permute(ctx.state);
                ctx.buf_pos = 0;
            }
        }
    }

    static void squeeze(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.squeezed) {
            ctx.buffer[ctx.buf_pos++] = 0x1F; // SHAKE domain separation
            while (ctx.buf_pos < RATE) {
                ctx.buffer[ctx.buf_pos++] = 0x00;
            }
            ctx.buffer[RATE - 1] |= 0x80;

            for (size_t i = 0; i < RATE; ++i) {
                reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
            }
            KeccakCore::permute(ctx.state);
            ctx.squeezed = true;
            ctx.squeeze_pos = 0;
        }

        size_t out_idx = 0;
        while (out_idx < out.size()) {
            if (ctx.squeeze_pos == RATE) {
                KeccakCore::permute(ctx.state);
                ctx.squeeze_pos = 0;
            }
            size_t take = std::min(out.size() - out_idx, RATE - ctx.squeeze_pos);
            std::memcpy(out.data() + out_idx, reinterpret_cast<const uint8_t*>(ctx.state) + ctx.squeeze_pos, take);
            out_idx += take;
            ctx.squeeze_pos += take;
        }
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data, size_t outLen = DEFAULT_DIGEST_SIZE) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(outLen);
        squeeze(ctx, out);
        return out;
    }
};

class Shake256 {
public:
    static constexpr size_t DEFAULT_DIGEST_SIZE = 64;
    static constexpr size_t RATE                = 136; // (1600 - 512) / 8 = 136 bytes

    struct Context {
        uint64_t state[25]{};
        uint8_t  buffer[RATE]{};
        size_t   buf_pos{0};
        bool     squeezed{false};
        size_t   squeeze_pos{0};
    };

    static void init(Context& ctx) noexcept {
        std::memset(ctx.state, 0, sizeof(ctx.state));
        std::memset(ctx.buffer, 0, sizeof(ctx.buffer));
        ctx.buf_pos = 0;
        ctx.squeezed = false;
        ctx.squeeze_pos = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        for (uint8_t byte : data) {
            ctx.buffer[ctx.buf_pos++] = byte;
            if (ctx.buf_pos == RATE) {
                for (size_t i = 0; i < RATE; ++i) {
                    reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
                }
                KeccakCore::permute(ctx.state);
                ctx.buf_pos = 0;
            }
        }
    }

    static void squeeze(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.squeezed) {
            ctx.buffer[ctx.buf_pos++] = 0x1F; // SHAKE domain separation
            while (ctx.buf_pos < RATE) {
                ctx.buffer[ctx.buf_pos++] = 0x00;
            }
            ctx.buffer[RATE - 1] |= 0x80;

            for (size_t i = 0; i < RATE; ++i) {
                reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
            }
            KeccakCore::permute(ctx.state);
            ctx.squeezed = true;
            ctx.squeeze_pos = 0;
        }

        size_t out_idx = 0;
        while (out_idx < out.size()) {
            if (ctx.squeeze_pos == RATE) {
                KeccakCore::permute(ctx.state);
                ctx.squeeze_pos = 0;
            }
            size_t take = std::min(out.size() - out_idx, RATE - ctx.squeeze_pos);
            std::memcpy(out.data() + out_idx, reinterpret_cast<const uint8_t*>(ctx.state) + ctx.squeeze_pos, take);
            out_idx += take;
            ctx.squeeze_pos += take;
        }
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data, size_t outLen = DEFAULT_DIGEST_SIZE) {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(outLen);
        squeeze(ctx, out);
        return out;
    }
};

// ============================================================================
// NIST SP 800-185 SHA-3 Derived Functions (cSHAKE & KMAC)
// ============================================================================
class NistSp800_185 {
public:
    static std::vector<uint8_t> left_encode(uint64_t val) {
        if (val == 0) return { 1, 0 };
        std::vector<uint8_t> b;
        uint64_t t = val;
        while (t > 0) {
            b.push_back(static_cast<uint8_t>(t & 0xFF));
            t >>= 8;
        }
        std::reverse(b.begin(), b.end());
        std::vector<uint8_t> res;
        res.push_back(static_cast<uint8_t>(b.size()));
        res.insert(res.end(), b.begin(), b.end());
        return res;
    }

    static std::vector<uint8_t> right_encode(uint64_t val) {
        if (val == 0) return { 0, 1 };
        std::vector<uint8_t> b;
        uint64_t t = val;
        while (t > 0) {
            b.push_back(static_cast<uint8_t>(t & 0xFF));
            t >>= 8;
        }
        std::reverse(b.begin(), b.end());
        b.push_back(static_cast<uint8_t>(b.size()));
        return b;
    }

    static std::vector<uint8_t> encode_string(std::span<const uint8_t> s) {
        auto enc = left_encode(s.size() * 8ULL);
        enc.insert(enc.end(), s.begin(), s.end());
        return enc;
    }

    static std::vector<uint8_t> encode_string(std::string_view s) {
        return encode_string(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(s.data()), s.size()));
    }

    static std::vector<uint8_t> bytepad(std::span<const uint8_t> x, size_t w) {
        auto z = left_encode(w);
        z.insert(z.end(), x.begin(), x.end());
        size_t rem = z.size() % w;
        if (rem != 0) {
            z.insert(z.end(), w - rem, 0x00);
        }
        return z;
    }
};

template<size_t RateBytes, size_t DefaultDigestBytes>
class GenericCShake {
public:
    static constexpr size_t RATE = RateBytes;
    static constexpr size_t DEFAULT_DIGEST_SIZE = DefaultDigestBytes;

    struct Context {
        uint64_t state[25]{};
        uint8_t  buffer[RATE]{};
        size_t   buf_pos{ 0 };
        bool     isCustom{ false };
        bool     squeezed{ false };
        size_t   squeeze_pos{ 0 };
    };

    static void init(Context& ctx, std::string_view N = "", std::string_view S = "") noexcept {
        std::memset(ctx.state, 0, sizeof(ctx.state));
        std::memset(ctx.buffer, 0, sizeof(ctx.buffer));
        ctx.buf_pos = 0;
        ctx.squeezed = false;
        ctx.squeeze_pos = 0;
        ctx.isCustom = (!N.empty() || !S.empty());
        if (ctx.isCustom) {
            std::vector<uint8_t> prefix;
            auto encN = NistSp800_185::encode_string(N);
            auto encS = NistSp800_185::encode_string(S);
            prefix.insert(prefix.end(), encN.begin(), encN.end());
            prefix.insert(prefix.end(), encS.begin(), encS.end());
            auto padded = NistSp800_185::bytepad(prefix, RATE);
            update(ctx, padded);
        }
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        for (uint8_t byte : data) {
            ctx.buffer[ctx.buf_pos++] = byte;
            if (ctx.buf_pos == RATE) {
                for (size_t i = 0; i < RATE; ++i) {
                    reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
                }
                KeccakCore::permute(ctx.state);
                ctx.buf_pos = 0;
            }
        }
    }

    static void squeeze(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.squeezed) {
            uint8_t domainPad = ctx.isCustom ? 0x04 : 0x1F;
            ctx.buffer[ctx.buf_pos++] = domainPad;
            while (ctx.buf_pos < RATE) {
                ctx.buffer[ctx.buf_pos++] = 0x00;
            }
            ctx.buffer[RATE - 1] |= 0x80;

            for (size_t i = 0; i < RATE; ++i) {
                reinterpret_cast<uint8_t*>(ctx.state)[i] ^= ctx.buffer[i];
            }
            KeccakCore::permute(ctx.state);
            ctx.squeezed = true;
            ctx.squeeze_pos = 0;
        }

        size_t out_idx = 0;
        while (out_idx < out.size()) {
            if (ctx.squeeze_pos == RATE) {
                KeccakCore::permute(ctx.state);
                ctx.squeeze_pos = 0;
            }
            size_t take = std::min(out.size() - out_idx, RATE - ctx.squeeze_pos);
            std::memcpy(out.data() + out_idx, reinterpret_cast<const uint8_t*>(ctx.state) + ctx.squeeze_pos, take);
            out_idx += take;
            ctx.squeeze_pos += take;
        }
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data, std::string_view N = "", std::string_view S = "", size_t outLen = DEFAULT_DIGEST_SIZE) {
        Context ctx;
        init(ctx, N, S);
        update(ctx, data);
        std::vector<uint8_t> out(outLen);
        squeeze(ctx, out);
        return out;
    }
};

using CShake128 = GenericCShake<168, 32>;
using CShake256 = GenericCShake<136, 64>;

template<typename CShakeType>
class GenericKmac {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename CShakeType::Context cshake;
        size_t macLen{ DEFAULT_DIGEST_SIZE };
        bool   finalized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::string_view custom = "", size_t macLen = DEFAULT_DIGEST_SIZE) noexcept {
        ctx.macLen = macLen;
        ctx.finalized = false;
        CShakeType::init(ctx.cshake, "KMAC", custom);
        auto encKey = NistSp800_185::encode_string(key);
        auto paddedKey = NistSp800_185::bytepad(encKey, RATE);
        CShakeType::update(ctx.cshake, paddedKey);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.finalized) {
            CShakeType::update(ctx.cshake, data);
        }
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.finalized) {
            auto encLen = NistSp800_185::right_encode(out.size() * 8ULL);
            CShakeType::update(ctx.cshake, encLen);
            ctx.finalized = true;
        }
        CShakeType::squeeze(ctx.cshake, out);
    }

    static std::vector<uint8_t> mac(std::span<const uint8_t> key, std::span<const uint8_t> data, std::string_view custom = "", size_t macLen = DEFAULT_DIGEST_SIZE) {
        Context ctx;
        init(ctx, key, custom, macLen);
        update(ctx, data);
        std::vector<uint8_t> out(macLen);
        final(ctx, out);
        return out;
    }
};

using Kmac128 = GenericKmac<CShake128>;
using Kmac256 = GenericKmac<CShake256>;

template<typename CShakeType>
class GenericKmacXof {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename CShakeType::Context cshake;
        bool   finalized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::string_view custom = "") noexcept {
        ctx.finalized = false;
        CShakeType::init(ctx.cshake, "KMAC", custom);
        auto encKey = NistSp800_185::encode_string(key);
        auto paddedKey = NistSp800_185::bytepad(encKey, RATE);
        CShakeType::update(ctx.cshake, paddedKey);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.finalized) {
            CShakeType::update(ctx.cshake, data);
        }
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.finalized) {
            auto encZero = NistSp800_185::right_encode(0ULL);
            CShakeType::update(ctx.cshake, encZero);
            ctx.finalized = true;
        }
        CShakeType::squeeze(ctx.cshake, out);
    }

    static std::vector<uint8_t> mac(std::span<const uint8_t> key, std::span<const uint8_t> data, std::string_view custom = "", size_t outLen = DEFAULT_DIGEST_SIZE) {
        Context ctx;
        init(ctx, key, custom);
        update(ctx, data);
        std::vector<uint8_t> out(outLen);
        final(ctx, out);
        return out;
    }
};

using KmacXOF128 = GenericKmacXof<CShake128>;
using KmacXOF256 = GenericKmacXof<CShake256>;

template<typename CShakeType>
class GenericKmacPrf {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context kmacXof{};
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::string_view label = "KDF") noexcept {
        GenericKmacXof<CShakeType>::init(ctx.kmacXof, key, label);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        GenericKmacXof<CShakeType>::update(ctx.kmacXof, data);
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        GenericKmacXof<CShakeType>::final(ctx.kmacXof, out);
    }

    static std::vector<uint8_t> deriveKey(
        std::span<const uint8_t> key,
        std::string_view label,
        std::span<const uint8_t> context,
        size_t outLen = DEFAULT_DIGEST_SIZE
    ) {
        Context ctx;
        init(ctx, key, label);
        if (!context.empty()) {
            update(ctx, context);
        }
        std::vector<uint8_t> out(outLen);
        final(ctx, out);
        return out;
    }
};

using KmacPrf128 = GenericKmacPrf<CShake128>;
using KmacPrf256 = GenericKmacPrf<CShake256>;

template<typename CShakeType>
class GenericKmacRatchet {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context kmacXof{};
        std::vector<uint8_t> chainKey{};
        uint64_t stepCount{0};
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::string_view label = "RATCHET") noexcept {
        ctx.chainKey.assign(key.begin(), key.end());
        ctx.stepCount = 0;
        GenericKmacXof<CShakeType>::init(ctx.kmacXof, key, label);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        GenericKmacXof<CShakeType>::update(ctx.kmacXof, data);
    }

    static void ratchetStep(Context& ctx, std::span<uint8_t> outputKey, std::span<const uint8_t> entropy = {}) noexcept {
        if (!entropy.empty()) {
            GenericKmacXof<CShakeType>::update(ctx.kmacXof, entropy);
        }
        uint8_t stepBytes[8];
        for (int i = 0; i < 8; ++i) {
            stepBytes[i] = static_cast<uint8_t>((ctx.stepCount >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(ctx.kmacXof, std::span<const uint8_t>(stepBytes, 8));
        ctx.stepCount++;

        GenericKmacXof<CShakeType>::final(ctx.kmacXof, outputKey);
        ctx.chainKey.assign(outputKey.begin(), outputKey.end());
        GenericKmacXof<CShakeType>::init(ctx.kmacXof, ctx.chainKey, "RATCHET_ADVANCE");
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        GenericKmacXof<CShakeType>::final(ctx.kmacXof, out);
    }
};

using KmacRatchet128 = GenericKmacRatchet<CShake128>;
using KmacRatchet256 = GenericKmacRatchet<CShake256>;

template<typename CShakeType>
class GenericKmacDoubleRatchet {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context rootKmac{};
        typename GenericKmacXof<CShakeType>::Context chainKmac{};
        std::vector<uint8_t> rootKey{};
        std::vector<uint8_t> chainKey{};
        uint64_t rootStepCount{ 0 };
        uint64_t messageStepCount{ 0 };
    };

    static void init(Context& ctx, std::span<const uint8_t> masterKey, std::string_view label = "DOUBLE_RATCHET") noexcept {
        ctx.rootKey.assign(masterKey.begin(), masterKey.end());
        ctx.chainKey.assign(masterKey.begin(), masterKey.end());
        ctx.rootStepCount = 0;
        ctx.messageStepCount = 0;
        GenericKmacXof<CShakeType>::init(ctx.rootKmac, masterKey, "ROOT_RATCHET");
        GenericKmacXof<CShakeType>::init(ctx.chainKmac, masterKey, label);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        GenericKmacXof<CShakeType>::update(ctx.chainKmac, data);
    }

    static void rootRatchetStep(Context& ctx, std::span<const uint8_t> dhSharedSecret = {}) noexcept {
        if (!dhSharedSecret.empty()) {
            GenericKmacXof<CShakeType>::update(ctx.rootKmac, dhSharedSecret);
        }
        uint8_t stepBytes[8];
        for (int i = 0; i < 8; ++i) {
            stepBytes[i] = static_cast<uint8_t>((ctx.rootStepCount >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(ctx.rootKmac, std::span<const uint8_t>(stepBytes, 8));
        ctx.rootStepCount++;

        std::vector<uint8_t> derived(DEFAULT_DIGEST_SIZE * 2);
        GenericKmacXof<CShakeType>::final(ctx.rootKmac, std::span<uint8_t>(derived.data(), derived.size()));

        ctx.rootKey.assign(derived.begin(), derived.begin() + DEFAULT_DIGEST_SIZE);
        ctx.chainKey.assign(derived.begin() + DEFAULT_DIGEST_SIZE, derived.end());

        GenericKmacXof<CShakeType>::init(ctx.rootKmac, ctx.rootKey, "ROOT_ADVANCE");
        GenericKmacXof<CShakeType>::init(ctx.chainKmac, ctx.chainKey, "CHAIN_ADVANCE");
        ctx.messageStepCount = 0;
    }

    static void ratchetStep(Context& ctx, std::span<uint8_t> outputMessageKey, std::span<const uint8_t> associatedData = {}) noexcept {
        if (!associatedData.empty()) {
            GenericKmacXof<CShakeType>::update(ctx.chainKmac, associatedData);
        }
        uint8_t stepBytes[8];
        for (int i = 0; i < 8; ++i) {
            stepBytes[i] = static_cast<uint8_t>((ctx.messageStepCount >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(ctx.chainKmac, std::span<const uint8_t>(stepBytes, 8));
        ctx.messageStepCount++;

        GenericKmacXof<CShakeType>::final(ctx.chainKmac, outputMessageKey);
        ctx.chainKey.assign(outputMessageKey.begin(), outputMessageKey.end());
        GenericKmacXof<CShakeType>::init(ctx.chainKmac, ctx.chainKey, "CHAIN_ADVANCE");
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        GenericKmacXof<CShakeType>::final(ctx.chainKmac, out);
    }
};

using KmacDoubleRatchet128 = GenericKmacDoubleRatchet<CShake128>;
using KmacDoubleRatchet256 = GenericKmacDoubleRatchet<CShake256>;

template<typename CShakeType>
class GenericKmacTripleRatchet {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context rootKmac{};
        typename GenericKmacXof<CShakeType>::Context pqKmac{};
        typename GenericKmacXof<CShakeType>::Context chainKmac{};
        std::vector<uint8_t> rootKey{};
        std::vector<uint8_t> pqKey{};
        std::vector<uint8_t> chainKey{};
        uint64_t rootStepCount{ 0 };
        uint64_t pqStepCount{ 0 };
        uint64_t messageStepCount{ 0 };
    };

    static void init(Context& ctx, std::span<const uint8_t> masterKey, std::string_view label = "TRIPLE_RATCHET") noexcept {
        ctx.rootKey.assign(masterKey.begin(), masterKey.end());
        ctx.pqKey.assign(masterKey.begin(), masterKey.end());
        ctx.chainKey.assign(masterKey.begin(), masterKey.end());
        ctx.rootStepCount = 0;
        ctx.pqStepCount = 0;
        ctx.messageStepCount = 0;
        GenericKmacXof<CShakeType>::init(ctx.rootKmac, masterKey, "ROOT_RATCHET");
        GenericKmacXof<CShakeType>::init(ctx.pqKmac, masterKey, "PQ_RATCHET");
        GenericKmacXof<CShakeType>::init(ctx.chainKmac, masterKey, label);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        GenericKmacXof<CShakeType>::update(ctx.chainKmac, data);
    }

    // Stage 1: Classical DH root ratchet step
    static void rootRatchetStep(Context& ctx, std::span<const uint8_t> dhSharedSecret = {}) noexcept {
        if (!dhSharedSecret.empty()) {
            GenericKmacXof<CShakeType>::update(ctx.rootKmac, dhSharedSecret);
        }
        uint8_t stepBytes[8];
        for (int i = 0; i < 8; ++i) {
            stepBytes[i] = static_cast<uint8_t>((ctx.rootStepCount >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(ctx.rootKmac, std::span<const uint8_t>(stepBytes, 8));
        ctx.rootStepCount++;

        std::vector<uint8_t> derived(DEFAULT_DIGEST_SIZE * 2);
        GenericKmacXof<CShakeType>::final(ctx.rootKmac, std::span<uint8_t>(derived.data(), derived.size()));

        ctx.rootKey.assign(derived.begin(), derived.begin() + DEFAULT_DIGEST_SIZE);
        ctx.pqKey.assign(derived.begin() + DEFAULT_DIGEST_SIZE, derived.end());

        GenericKmacXof<CShakeType>::init(ctx.rootKmac, ctx.rootKey, "ROOT_ADVANCE");
        GenericKmacXof<CShakeType>::init(ctx.pqKmac, ctx.pqKey, "PQ_ADVANCE");
    }

    // Stage 2: Post-Quantum KEM entropy ratchet step (absorbs Kyber/ML-KEM entropy)
    static void pqRatchetStep(Context& ctx, std::span<const uint8_t> pqKemSecret = {}) noexcept {
        if (!pqKemSecret.empty()) {
            GenericKmacXof<CShakeType>::update(ctx.pqKmac, pqKemSecret);
        }
        uint8_t stepBytes[8];
        for (int i = 0; i < 8; ++i) {
            stepBytes[i] = static_cast<uint8_t>((ctx.pqStepCount >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(ctx.pqKmac, std::span<const uint8_t>(stepBytes, 8));
        ctx.pqStepCount++;

        std::vector<uint8_t> derived(DEFAULT_DIGEST_SIZE * 2);
        GenericKmacXof<CShakeType>::final(ctx.pqKmac, std::span<uint8_t>(derived.data(), derived.size()));

        ctx.pqKey.assign(derived.begin(), derived.begin() + DEFAULT_DIGEST_SIZE);
        ctx.chainKey.assign(derived.begin() + DEFAULT_DIGEST_SIZE, derived.end());

        GenericKmacXof<CShakeType>::init(ctx.pqKmac, ctx.pqKey, "PQ_ADVANCE");
        GenericKmacXof<CShakeType>::init(ctx.chainKmac, ctx.chainKey, "CHAIN_ADVANCE");
        ctx.messageStepCount = 0;
    }

    // Stage 3: Message / symmetric ratchet step
    static void ratchetStep(Context& ctx, std::span<uint8_t> outputMessageKey, std::span<const uint8_t> associatedData = {}) noexcept {
        if (!associatedData.empty()) {
            GenericKmacXof<CShakeType>::update(ctx.chainKmac, associatedData);
        }
        uint8_t stepBytes[8];
        for (int i = 0; i < 8; ++i) {
            stepBytes[i] = static_cast<uint8_t>((ctx.messageStepCount >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(ctx.chainKmac, std::span<const uint8_t>(stepBytes, 8));
        ctx.messageStepCount++;

        GenericKmacXof<CShakeType>::final(ctx.chainKmac, outputMessageKey);
        ctx.chainKey.assign(outputMessageKey.begin(), outputMessageKey.end());
        GenericKmacXof<CShakeType>::init(ctx.chainKmac, ctx.chainKey, "CHAIN_ADVANCE");
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        GenericKmacXof<CShakeType>::final(ctx.chainKmac, out);
    }
};

using KmacTripleRatchet128 = GenericKmacTripleRatchet<CShake128>;
using KmacTripleRatchet256 = GenericKmacTripleRatchet<CShake256>;

template<typename CShakeType>
class GenericKmacTreeRatchet {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct TreeNode {
        std::vector<uint8_t> nodeKey{};
        uint32_t nodeIndex{ 0 };
        uint32_t generation{ 0 };
    };

    struct Context {
        typename GenericKmacXof<CShakeType>::Context treeKmac{};
        std::vector<uint8_t> rootKey{};
        std::vector<TreeNode> nodes{};
        uint32_t totalNodes{ 0 };
        uint32_t treeDepth{ 0 };
        uint64_t ratchetCount{ 0 };
    };

    static void init(Context& ctx, std::span<const uint8_t> rootKey, uint32_t leafCount = 4, std::string_view label = "TREE_RATCHET") noexcept {
        ctx.rootKey.assign(rootKey.begin(), rootKey.end());
        ctx.treeDepth = 1;
        while ((1u << ctx.treeDepth) < leafCount * 2) {
            ctx.treeDepth++;
        }
        ctx.totalNodes = (1u << (ctx.treeDepth + 1)) - 1;
        if (ctx.totalNodes < 7) ctx.totalNodes = 7;
        ctx.nodes.resize(ctx.totalNodes);
        for (uint32_t i = 0; i < ctx.totalNodes; ++i) {
            ctx.nodes[i].nodeIndex = i;
            ctx.nodes[i].generation = 0;
            ctx.nodes[i].nodeKey.assign(rootKey.begin(), rootKey.end());
        }
        ctx.ratchetCount = 0;
        GenericKmacXof<CShakeType>::init(ctx.treeKmac, rootKey, label);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        GenericKmacXof<CShakeType>::update(ctx.treeKmac, data);
    }

    static void ratchetNode(Context& ctx, uint32_t nodeIndex, std::span<uint8_t> outputKey, std::span<const uint8_t> contextEntropy = {}) noexcept {
        if (nodeIndex >= ctx.nodes.size()) {
            if (outputKey.size() <= ctx.rootKey.size()) {
                std::memcpy(outputKey.data(), ctx.rootKey.data(), outputKey.size());
            }
            return;
        }
        auto& node = ctx.nodes[nodeIndex];
        typename GenericKmacXof<CShakeType>::Context nodeKmac{};
        GenericKmacXof<CShakeType>::init(nodeKmac, node.nodeKey, "TREE_NODE_STEP");
        if (!contextEntropy.empty()) {
            GenericKmacXof<CShakeType>::update(nodeKmac, contextEntropy);
        }
        uint8_t stepBytes[8];
        for (int i = 0; i < 8; ++i) {
            stepBytes[i] = static_cast<uint8_t>(((ctx.ratchetCount + node.generation) >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(nodeKmac, std::span<const uint8_t>(stepBytes, 8));
        node.generation++;
        ctx.ratchetCount++;

        std::vector<uint8_t> nextKey(DEFAULT_DIGEST_SIZE * 2);
        GenericKmacXof<CShakeType>::final(nodeKmac, std::span<uint8_t>(nextKey.data(), nextKey.size()));

        size_t copyLen = (outputKey.size() < DEFAULT_DIGEST_SIZE) ? outputKey.size() : DEFAULT_DIGEST_SIZE;
        std::memcpy(outputKey.data(), nextKey.data(), copyLen);
        node.nodeKey.assign(nextKey.begin() + DEFAULT_DIGEST_SIZE, nextKey.end());
    }

    static void treeRatchetStep(Context& ctx, std::span<uint8_t> outputGroupKey, std::span<const uint8_t> pathEntropy = {}) noexcept {
        if (!pathEntropy.empty()) {
            GenericKmacXof<CShakeType>::update(ctx.treeKmac, pathEntropy);
        }
        uint8_t stepBytes[8];
        for (int i = 0; i < 8; ++i) {
            stepBytes[i] = static_cast<uint8_t>((ctx.ratchetCount >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(ctx.treeKmac, std::span<const uint8_t>(stepBytes, 8));
        ctx.ratchetCount++;

        GenericKmacXof<CShakeType>::final(ctx.treeKmac, outputGroupKey);
        ctx.rootKey.assign(outputGroupKey.begin(), outputGroupKey.end());
        GenericKmacXof<CShakeType>::init(ctx.treeKmac, ctx.rootKey, "TREE_ADVANCE");
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        GenericKmacXof<CShakeType>::final(ctx.treeKmac, out);
    }
};

using KmacTreeRatchet128 = GenericKmacTreeRatchet<CShake128>;
using KmacTreeRatchet256 = GenericKmacTreeRatchet<CShake256>;

template<typename CShakeType>
class GenericKmacAsyncRatchet {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context asyncKmac{};
        std::vector<uint8_t> rootKey{};
        std::vector<uint8_t> inboundChainKey{};
        std::vector<uint8_t> outboundChainKey{};
        uint64_t inboundCounter{ 0 };
        uint64_t outboundCounter{ 0 };
        uint64_t totalRatchets{ 0 };
    };

    static void init(Context& ctx, std::span<const uint8_t> masterSecret, std::string_view label = "ASYNC_RATCHET") noexcept {
        ctx.rootKey.assign(masterSecret.begin(), masterSecret.end());
        ctx.inboundChainKey.assign(masterSecret.begin(), masterSecret.end());
        ctx.outboundChainKey.assign(masterSecret.begin(), masterSecret.end());
        ctx.inboundCounter = 0;
        ctx.outboundCounter = 0;
        ctx.totalRatchets = 0;
        GenericKmacXof<CShakeType>::init(ctx.asyncKmac, masterSecret, label);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        GenericKmacXof<CShakeType>::update(ctx.asyncKmac, data);
    }

    static void ratchetInbound(Context& ctx, std::span<uint8_t> outputKey, std::span<const uint8_t> contextEntropy = {}) noexcept {
        typename GenericKmacXof<CShakeType>::Context chainKmac{};
        GenericKmacXof<CShakeType>::init(chainKmac, ctx.inboundChainKey, "ASYNC_INBOUND_STEP");
        if (!contextEntropy.empty()) {
            GenericKmacXof<CShakeType>::update(chainKmac, contextEntropy);
        }
        uint8_t ctrBytes[8];
        for (int i = 0; i < 8; ++i) {
            ctrBytes[i] = static_cast<uint8_t>((ctx.inboundCounter >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(chainKmac, std::span<const uint8_t>(ctrBytes, 8));
        ctx.inboundCounter++;
        ctx.totalRatchets++;

        std::vector<uint8_t> nextMaterial(DEFAULT_DIGEST_SIZE * 2);
        GenericKmacXof<CShakeType>::final(chainKmac, std::span<uint8_t>(nextMaterial.data(), nextMaterial.size()));

        size_t copyLen = (outputKey.size() < DEFAULT_DIGEST_SIZE) ? outputKey.size() : DEFAULT_DIGEST_SIZE;
        std::memcpy(outputKey.data(), nextMaterial.data(), copyLen);
        ctx.inboundChainKey.assign(nextMaterial.begin() + DEFAULT_DIGEST_SIZE, nextMaterial.end());
    }

    static void ratchetOutbound(Context& ctx, std::span<uint8_t> outputKey, std::span<const uint8_t> contextEntropy = {}) noexcept {
        typename GenericKmacXof<CShakeType>::Context chainKmac{};
        GenericKmacXof<CShakeType>::init(chainKmac, ctx.outboundChainKey, "ASYNC_OUTBOUND_STEP");
        if (!contextEntropy.empty()) {
            GenericKmacXof<CShakeType>::update(chainKmac, contextEntropy);
        }
        uint8_t ctrBytes[8];
        for (int i = 0; i < 8; ++i) {
            ctrBytes[i] = static_cast<uint8_t>((ctx.outboundCounter >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(chainKmac, std::span<const uint8_t>(ctrBytes, 8));
        ctx.outboundCounter++;
        ctx.totalRatchets++;

        std::vector<uint8_t> nextMaterial(DEFAULT_DIGEST_SIZE * 2);
        GenericKmacXof<CShakeType>::final(chainKmac, std::span<uint8_t>(nextMaterial.data(), nextMaterial.size()));

        size_t copyLen = (outputKey.size() < DEFAULT_DIGEST_SIZE) ? outputKey.size() : DEFAULT_DIGEST_SIZE;
        std::memcpy(outputKey.data(), nextMaterial.data(), copyLen);
        ctx.outboundChainKey.assign(nextMaterial.begin() + DEFAULT_DIGEST_SIZE, nextMaterial.end());
    }

    static void asyncRebase(Context& ctx, std::span<const uint8_t> externalEntropy) noexcept {
        typename GenericKmacXof<CShakeType>::Context rebaseKmac{};
        GenericKmacXof<CShakeType>::init(rebaseKmac, ctx.rootKey, "ASYNC_REBASE");
        if (!externalEntropy.empty()) {
            GenericKmacXof<CShakeType>::update(rebaseKmac, externalEntropy);
        }
        uint8_t ctrBytes[8];
        for (int i = 0; i < 8; ++i) {
            ctrBytes[i] = static_cast<uint8_t>((ctx.totalRatchets >> (i * 8)) & 0xFF);
        }
        GenericKmacXof<CShakeType>::update(rebaseKmac, std::span<const uint8_t>(ctrBytes, 8));

        std::vector<uint8_t> newKeys(DEFAULT_DIGEST_SIZE * 3);
        GenericKmacXof<CShakeType>::final(rebaseKmac, std::span<uint8_t>(newKeys.data(), newKeys.size()));

        ctx.rootKey.assign(newKeys.begin(), newKeys.begin() + DEFAULT_DIGEST_SIZE);
        ctx.inboundChainKey.assign(newKeys.begin() + DEFAULT_DIGEST_SIZE, newKeys.begin() + DEFAULT_DIGEST_SIZE * 2);
        ctx.outboundChainKey.assign(newKeys.begin() + DEFAULT_DIGEST_SIZE * 2, newKeys.end());
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        GenericKmacXof<CShakeType>::final(ctx.asyncKmac, out);
    }
};

using KmacAsyncRatchet128 = GenericKmacAsyncRatchet<CShake128>;
using KmacAsyncRatchet256 = GenericKmacAsyncRatchet<CShake256>;

template<typename CShakeType>
class GenericKmacKwp {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context kwpKmac{};
        std::vector<uint8_t> kek{};
        std::vector<uint8_t> buffer{};
        uint64_t wrappedCount{ 0 };
        uint64_t unwrappedCount{ 0 };
    };

    static void init(Context& ctx, std::span<const uint8_t> kek, std::string_view label = "KMAC_KWP") noexcept {
        ctx.kek.assign(kek.begin(), kek.end());
        ctx.buffer.clear();
        ctx.wrappedCount = 0;
        ctx.unwrappedCount = 0;
        GenericKmacXof<CShakeType>::init(ctx.kwpKmac, kek, label);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        GenericKmacXof<CShakeType>::update(ctx.kwpKmac, data);
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        GenericKmacXof<CShakeType>::final(ctx.kwpKmac, out);
    }

    // NIST SP 800-38F Key Wrap with Padding (KWP) using KMAC:
    // Format: 4-byte ICV (0xA65959A6) + 4-byte big-endian plaintext length + plaintext + zero padding to 8-byte boundary.
    // Encrypted with keystream derived via KMAC-XOF("KMAC_KWP_STREAM", kek), and authenticated with tag via KMAC("KMAC_KWP_MAC", kek) over (associatedData || ciphertext).
    static bool wrapKey(
        Context& ctx,
        std::span<const uint8_t> plaintextKey,
        std::vector<uint8_t>& wrappedKey,
        std::span<const uint8_t> associatedData = {}) noexcept {
        
        if (plaintextKey.empty()) return false;
        size_t ptLen = plaintextKey.size();
        size_t padLen = (8 - (ptLen % 8)) % 8;
        size_t payloadLen = 8 + ptLen + padLen;

        std::vector<uint8_t> payload(payloadLen, 0);
        // ICV = 0xA65959A6
        payload[0] = 0xA6;
        payload[1] = 0x59;
        payload[2] = 0x59;
        payload[3] = 0xA6;
        // Big-endian length
        payload[4] = static_cast<uint8_t>((ptLen >> 24) & 0xFF);
        payload[5] = static_cast<uint8_t>((ptLen >> 16) & 0xFF);
        payload[6] = static_cast<uint8_t>((ptLen >> 8) & 0xFF);
        payload[7] = static_cast<uint8_t>(ptLen & 0xFF);
        // Plaintext
        std::memcpy(&payload[8], plaintextKey.data(), ptLen);

        // Keystream encryption
        typename GenericKmacXof<CShakeType>::Context streamKmac{};
        GenericKmacXof<CShakeType>::init(streamKmac, ctx.kek, "KMAC_KWP_STREAM");
        std::vector<uint8_t> keystream(payloadLen);
        GenericKmacXof<CShakeType>::final(streamKmac, std::span<uint8_t>(keystream.data(), keystream.size()));

        std::vector<uint8_t> ciphertext(payloadLen);
        for (size_t i = 0; i < payloadLen; ++i) {
            ciphertext[i] = payload[i] ^ keystream[i];
        }

        // Authentication tag via KMAC-MAC
        typename GenericKmac<CShakeType>::Context macKmac{};
        GenericKmac<CShakeType>::init(macKmac, ctx.kek, "KMAC_KWP_MAC");
        if (!associatedData.empty()) {
            GenericKmac<CShakeType>::update(macKmac, associatedData);
        }
        GenericKmac<CShakeType>::update(macKmac, std::span<const uint8_t>(ciphertext.data(), ciphertext.size()));

        std::vector<uint8_t> tag(DEFAULT_DIGEST_SIZE);
        GenericKmac<CShakeType>::final(macKmac, std::span<uint8_t>(tag.data(), tag.size()));

        // Total wrapped key = ciphertext || tag
        wrappedKey.resize(payloadLen + DEFAULT_DIGEST_SIZE);
        std::memcpy(wrappedKey.data(), ciphertext.data(), payloadLen);
        std::memcpy(wrappedKey.data() + payloadLen, tag.data(), DEFAULT_DIGEST_SIZE);

        ctx.wrappedCount++;
        return true;
    }

    static bool unwrapKey(
        Context& ctx,
        std::span<const uint8_t> wrappedKey,
        std::vector<uint8_t>& unwrappedKey,
        std::span<const uint8_t> associatedData = {}) noexcept {
        
        if (wrappedKey.size() < 8 + DEFAULT_DIGEST_SIZE) return false;
        size_t payloadLen = wrappedKey.size() - DEFAULT_DIGEST_SIZE;
        if (payloadLen < 8 || (payloadLen % 8) != 0) return false;

        const uint8_t* pCiphertext = wrappedKey.data();
        const uint8_t* pTag = wrappedKey.data() + payloadLen;

        // Verify MAC
        typename GenericKmac<CShakeType>::Context macKmac{};
        GenericKmac<CShakeType>::init(macKmac, ctx.kek, "KMAC_KWP_MAC");
        if (!associatedData.empty()) {
            GenericKmac<CShakeType>::update(macKmac, associatedData);
        }
        GenericKmac<CShakeType>::update(macKmac, std::span<const uint8_t>(pCiphertext, payloadLen));

        std::vector<uint8_t> expectedTag(DEFAULT_DIGEST_SIZE);
        GenericKmac<CShakeType>::final(macKmac, std::span<uint8_t>(expectedTag.data(), expectedTag.size()));

        // Constant-time compare
        uint8_t diff = 0;
        for (size_t i = 0; i < DEFAULT_DIGEST_SIZE; ++i) {
            diff |= (pTag[i] ^ expectedTag[i]);
        }
        if (diff != 0) return false;

        // Decrypt payload
        typename GenericKmacXof<CShakeType>::Context streamKmac{};
        GenericKmacXof<CShakeType>::init(streamKmac, ctx.kek, "KMAC_KWP_STREAM");
        std::vector<uint8_t> keystream(payloadLen);
        GenericKmacXof<CShakeType>::final(streamKmac, std::span<uint8_t>(keystream.data(), keystream.size()));

        std::vector<uint8_t> payload(payloadLen);
        for (size_t i = 0; i < payloadLen; ++i) {
            payload[i] = pCiphertext[i] ^ keystream[i];
        }

        // Verify ICV
        if (payload[0] != 0xA6 || payload[1] != 0x59 || payload[2] != 0x59 || payload[3] != 0xA6) {
            return false;
        }

        uint32_t ptLen = (static_cast<uint32_t>(payload[4]) << 24) |
                         (static_cast<uint32_t>(payload[5]) << 16) |
                         (static_cast<uint32_t>(payload[6]) << 8) |
                         static_cast<uint32_t>(payload[7]);

        if (ptLen > payloadLen - 8) return false;

        // Check padding is zero
        for (size_t i = 8 + ptLen; i < payloadLen; ++i) {
            if (payload[i] != 0) return false;
        }

        unwrappedKey.assign(payload.begin() + 8, payload.begin() + 8 + ptLen);
        ctx.unwrappedCount++;
        return true;
    }
};

using KmacKwp128 = GenericKmacKwp<CShake128>;
using KmacKwp256 = GenericKmacKwp<CShake256>;

template<typename CShakeType>
class GenericKmacAead {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;
    static constexpr size_t TAG_SIZE = DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context aeadKmac{};
        std::vector<uint8_t> key{};
        std::vector<uint8_t> buffer{};
        uint64_t sealedPackets{ 0 };
        uint64_t openedPackets{ 0 };
        uint64_t sealedBytes{ 0 };
        uint64_t openedBytes{ 0 };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::string_view label = "KMAC_AEAD") noexcept {
        ctx.key.assign(key.begin(), key.end());
        ctx.buffer.clear();
        ctx.sealedPackets = 0;
        ctx.openedPackets = 0;
        ctx.sealedBytes = 0;
        ctx.openedBytes = 0;
        GenericKmacXof<CShakeType>::init(ctx.aeadKmac, key, label);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        GenericKmacXof<CShakeType>::update(ctx.aeadKmac, data);
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        GenericKmacXof<CShakeType>::final(ctx.aeadKmac, out);
    }

    // NIST SP 800-185 KMAC Authenticated Encryption with Associated Data:
    // Keystream derived via KMAC-XOF with key and nonce: KMAC-XOF(key, nonce, "KMAC_AEAD_KEYSTREAM").
    // Tag computed via KMAC over (nonce || associatedData || ciphertext): KMAC(key, tagInput, "KMAC_AEAD_TAG").
    // Output: ciphertext and tag.
    static bool seal(
        Context& ctx,
        std::span<const uint8_t> nonce,
        std::span<const uint8_t> plaintext,
        std::span<const uint8_t> associatedData,
        std::vector<uint8_t>& ciphertext,
        std::vector<uint8_t>& tag) noexcept {

        if (ctx.key.empty()) return false;
        size_t ptLen = plaintext.size();
        ciphertext.resize(ptLen);

        // Derive keystream
        typename GenericKmacXof<CShakeType>::Context streamKmac{};
        GenericKmacXof<CShakeType>::init(streamKmac, ctx.key, "KMAC_AEAD_KEYSTREAM");
        if (!nonce.empty()) {
            GenericKmacXof<CShakeType>::update(streamKmac, nonce);
        }
        std::vector<uint8_t> keystream(ptLen);
        if (ptLen > 0) {
            GenericKmacXof<CShakeType>::final(streamKmac, std::span<uint8_t>(keystream.data(), keystream.size()));
            for (size_t i = 0; i < ptLen; ++i) {
                ciphertext[i] = plaintext[i] ^ keystream[i];
            }
        }

        // Tag calculation over (nonce || AD || ciphertext)
        typename GenericKmac<CShakeType>::Context macKmac{};
        GenericKmac<CShakeType>::init(macKmac, ctx.key, "KMAC_AEAD_TAG");

        uint64_t nLen = static_cast<uint64_t>(nonce.size());
        uint64_t adLen = static_cast<uint64_t>(associatedData.size());
        uint64_t ctLen = static_cast<uint64_t>(ptLen);

        uint8_t header[24];
        std::memcpy(&header[0], &nLen, 8);
        std::memcpy(&header[8], &adLen, 8);
        std::memcpy(&header[16], &ctLen, 8);

        GenericKmac<CShakeType>::update(macKmac, std::span<const uint8_t>(header, sizeof(header)));
        if (!nonce.empty()) {
            GenericKmac<CShakeType>::update(macKmac, nonce);
        }
        if (!associatedData.empty()) {
            GenericKmac<CShakeType>::update(macKmac, associatedData);
        }
        if (!ciphertext.empty()) {
            GenericKmac<CShakeType>::update(macKmac, std::span<const uint8_t>(ciphertext.data(), ciphertext.size()));
        }

        tag.resize(TAG_SIZE);
        GenericKmac<CShakeType>::final(macKmac, std::span<uint8_t>(tag.data(), tag.size()));

        ctx.sealedPackets++;
        ctx.sealedBytes += ptLen;
        return true;
    }

    static bool open(
        Context& ctx,
        std::span<const uint8_t> nonce,
        std::span<const uint8_t> ciphertext,
        std::span<const uint8_t> associatedData,
        std::span<const uint8_t> tag,
        std::vector<uint8_t>& plaintext) noexcept {

        if (ctx.key.empty() || tag.size() != TAG_SIZE) return false;
        size_t ctLen = ciphertext.size();

        // Verify tag first
        typename GenericKmac<CShakeType>::Context macKmac{};
        GenericKmac<CShakeType>::init(macKmac, ctx.key, "KMAC_AEAD_TAG");

        uint64_t nLen = static_cast<uint64_t>(nonce.size());
        uint64_t adLen = static_cast<uint64_t>(associatedData.size());
        uint64_t ctLen64 = static_cast<uint64_t>(ctLen);

        uint8_t header[24];
        std::memcpy(&header[0], &nLen, 8);
        std::memcpy(&header[8], &adLen, 8);
        std::memcpy(&header[16], &ctLen64, 8);

        GenericKmac<CShakeType>::update(macKmac, std::span<const uint8_t>(header, sizeof(header)));
        if (!nonce.empty()) {
            GenericKmac<CShakeType>::update(macKmac, nonce);
        }
        if (!associatedData.empty()) {
            GenericKmac<CShakeType>::update(macKmac, associatedData);
        }
        if (!ciphertext.empty()) {
            GenericKmac<CShakeType>::update(macKmac, ciphertext);
        }

        std::vector<uint8_t> expectedTag(TAG_SIZE);
        GenericKmac<CShakeType>::final(macKmac, std::span<uint8_t>(expectedTag.data(), expectedTag.size()));

        // Constant-time compare
        uint8_t diff = 0;
        for (size_t i = 0; i < TAG_SIZE; ++i) {
            diff |= (tag[i] ^ expectedTag[i]);
        }
        if (diff != 0) return false;

        // Decrypt ciphertext
        typename GenericKmacXof<CShakeType>::Context streamKmac{};
        GenericKmacXof<CShakeType>::init(streamKmac, ctx.key, "KMAC_AEAD_KEYSTREAM");
        if (!nonce.empty()) {
            GenericKmacXof<CShakeType>::update(streamKmac, nonce);
        }
        std::vector<uint8_t> keystream(ctLen);
        plaintext.resize(ctLen);
        if (ctLen > 0) {
            GenericKmacXof<CShakeType>::final(streamKmac, std::span<uint8_t>(keystream.data(), keystream.size()));
            for (size_t i = 0; i < ctLen; ++i) {
                plaintext[i] = ciphertext[i] ^ keystream[i];
            }
        }

        ctx.openedPackets++;
        ctx.openedBytes += ctLen;
        return true;
    }
};

using KmacAead128 = GenericKmacAead<CShake128>;
using KmacAead256 = GenericKmacAead<CShake256>;

template<typename CShakeType>
class GenericKmacMgf {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context kmacXof{};
        std::vector<uint8_t> key{};
        std::string custom{};
        uint64_t maskBytesGenerated{ 0 };
        uint64_t xorOperationsPerformed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::string_view custom = "KMAC_MGF") noexcept {
        ctx.key.assign(key.begin(), key.end());
        ctx.custom = std::string(custom);
        ctx.maskBytesGenerated = 0;
        ctx.xorOperationsPerformed = 0;
        ctx.initialized = true;
        GenericKmacXof<CShakeType>::init(ctx.kmacXof, key, custom);
    }

    static void update(Context& ctx, std::span<const uint8_t> seed) noexcept {
        if (!ctx.initialized) return;
        GenericKmacXof<CShakeType>::update(ctx.kmacXof, seed);
    }

    static void generate_mask(Context& ctx, std::span<uint8_t> outMask) noexcept {
        if (!ctx.initialized || outMask.empty()) return;
        GenericKmacXof<CShakeType>::final(ctx.kmacXof, outMask);
        ctx.maskBytesGenerated += outMask.size();
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        generate_mask(ctx, out);
    }

    static void mask_xor(Context& ctx, std::span<const uint8_t> in, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || in.empty() || out.size() < in.size()) return;
        std::vector<uint8_t> mask(in.size());
        generate_mask(ctx, std::span<uint8_t>(mask.data(), mask.size()));
        for (size_t i = 0; i < in.size(); ++i) {
            out[i] = in[i] ^ mask[i];
        }
        ctx.xorOperationsPerformed += in.size();
    }

    static std::vector<uint8_t> mask(
        std::span<const uint8_t> key,
        std::span<const uint8_t> seed,
        size_t maskLen,
        std::string_view custom = "KMAC_MGF") {
        Context ctx;
        init(ctx, key, custom);
        update(ctx, seed);
        std::vector<uint8_t> out(maskLen);
        generate_mask(ctx, out);
        return out;
    }
};

using KmacMgf128 = GenericKmacMgf<CShake128>;
using KmacMgf256 = GenericKmacMgf<CShake256>;

template<typename CShakeType>
class GenericKmacKdf {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context kmacXof{};
        std::vector<uint8_t> key{};
        std::string custom{};
        uint64_t keyBytesDerived{ 0 };
        uint32_t iterationCounter{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> secretKey, std::string_view custom = "KMAC_KDF") noexcept {
        ctx.key.assign(secretKey.begin(), secretKey.end());
        ctx.custom = std::string(custom);
        ctx.keyBytesDerived = 0;
        ctx.iterationCounter = 0;
        ctx.initialized = true;
        GenericKmacXof<CShakeType>::init(ctx.kmacXof, secretKey, custom);
    }

    static void update(Context& ctx, std::span<const uint8_t> contextOrLabel) noexcept {
        if (!ctx.initialized) return;
        GenericKmacXof<CShakeType>::update(ctx.kmacXof, contextOrLabel);
    }

    static void derive_key(Context& ctx, std::span<uint8_t> outDerivedKey) noexcept {
        if (!ctx.initialized || outDerivedKey.empty()) return;
        GenericKmacXof<CShakeType>::final(ctx.kmacXof, outDerivedKey);
        ctx.keyBytesDerived += outDerivedKey.size();
        ctx.iterationCounter++;
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        derive_key(ctx, out);
    }

    static std::vector<uint8_t> derive(
        std::span<const uint8_t> secretKey,
        std::span<const uint8_t> contextOrLabel,
        size_t derivedKeyLen,
        std::string_view custom = "KMAC_KDF") {
        Context ctx;
        init(ctx, secretKey, custom);
        update(ctx, contextOrLabel);
        std::vector<uint8_t> out(derivedKeyLen);
        derive_key(ctx, out);
        return out;
    }
};

using KmacKdf128 = GenericKmacKdf<CShake128>;
using KmacKdf256 = GenericKmacKdf<CShake256>;

template<typename CShakeType>
class GenericKmacCtr {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context kmacXof{};
        std::vector<uint8_t> key{};
        std::string custom{};
        std::vector<uint8_t> buffer{};
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::string_view custom = "KMAC_CTR") noexcept {
        ctx.key.assign(key.begin(), key.end());
        ctx.custom = std::string(custom);
        ctx.buffer.clear();
        ctx.bytesProcessed = 0;
        ctx.initialized = true;
        GenericKmacXof<CShakeType>::init(ctx.kmacXof, key, custom);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void process(Context& ctx, std::span<const uint8_t> input, std::span<uint8_t> output) noexcept {
        if (!ctx.initialized || input.empty() || output.empty()) return;
        size_t len = std::min(input.size(), output.size());
        std::vector<uint8_t> keystream(len);
        GenericKmacXof<CShakeType>::final(ctx.kmacXof, keystream);
        for (size_t i = 0; i < len; ++i) {
            output[i] = input[i] ^ keystream[i];
        }
        ctx.bytesProcessed += len;
    }

    static void encrypt(Context& ctx, std::span<const uint8_t> plaintext, std::span<uint8_t> ciphertext) noexcept {
        process(ctx, plaintext, ciphertext);
    }

    static void decrypt(Context& ctx, std::span<const uint8_t> ciphertext, std::span<uint8_t> plaintext) noexcept {
        process(ctx, ciphertext, plaintext);
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || out.empty()) return;
        if (!ctx.buffer.empty()) {
            size_t len = std::min(ctx.buffer.size(), out.size());
            process(ctx, std::span<const uint8_t>(ctx.buffer.data(), len), std::span<uint8_t>(out.data(), len));
            ctx.buffer.clear();
        } else {
            GenericKmacXof<CShakeType>::final(ctx.kmacXof, out);
        }
    }
};

using KmacCtr128 = GenericKmacCtr<CShake128>;
using KmacCtr256 = GenericKmacCtr<CShake256>;

template<typename CShakeType>
class GenericKmacOfb {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context kmacXof{};
        std::vector<uint8_t> key{};
        std::string custom{};
        std::vector<uint8_t> feedback{};
        std::vector<uint8_t> buffer{};
        size_t feedbackOffset{ 0 };
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::string_view custom = "KMAC_OFB") noexcept {
        ctx.key.assign(key.begin(), key.end());
        ctx.custom = std::string(custom);
        ctx.buffer.clear();
        ctx.bytesProcessed = 0;
        ctx.feedbackOffset = 0;
        ctx.initialized = true;
        GenericKmacXof<CShakeType>::init(ctx.kmacXof, key, custom);
        ctx.feedback.resize(DEFAULT_DIGEST_SIZE);
        GenericKmacXof<CShakeType>::final(ctx.kmacXof, ctx.feedback);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void process(Context& ctx, std::span<const uint8_t> input, std::span<uint8_t> output) noexcept {
        if (!ctx.initialized || input.empty() || output.empty()) return;
        size_t len = std::min(input.size(), output.size());
        for (size_t i = 0; i < len; ++i) {
            if (ctx.feedbackOffset >= ctx.feedback.size()) {
                typename GenericKmacXof<CShakeType>::Context nextKmac{};
                GenericKmacXof<CShakeType>::init(nextKmac, ctx.key, ctx.custom);
                GenericKmacXof<CShakeType>::update(nextKmac, ctx.feedback);
                ctx.feedback.resize(DEFAULT_DIGEST_SIZE);
                GenericKmacXof<CShakeType>::final(nextKmac, ctx.feedback);
                ctx.feedbackOffset = 0;
            }
            output[i] = input[i] ^ ctx.feedback[ctx.feedbackOffset++];
        }
        ctx.bytesProcessed += len;
    }

    static void encrypt(Context& ctx, std::span<const uint8_t> plaintext, std::span<uint8_t> ciphertext) noexcept {
        process(ctx, plaintext, ciphertext);
    }

    static void decrypt(Context& ctx, std::span<const uint8_t> ciphertext, std::span<uint8_t> plaintext) noexcept {
        process(ctx, ciphertext, plaintext);
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || out.empty()) return;
        if (!ctx.buffer.empty()) {
            size_t len = std::min(ctx.buffer.size(), out.size());
            process(ctx, std::span<const uint8_t>(ctx.buffer.data(), len), std::span<uint8_t>(out.data(), len));
            ctx.buffer.clear();
        } else {
            std::vector<uint8_t> dummy(out.size(), 0);
            process(ctx, dummy, out);
        }
    }
};

using KmacOfb128 = GenericKmacOfb<CShake128>;
using KmacOfb256 = GenericKmacOfb<CShake256>;

template<typename CShakeType>
class GenericKmacCfb {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename GenericKmacXof<CShakeType>::Context kmacXof{};
        std::vector<uint8_t> key{};
        std::string custom{};
        std::vector<uint8_t> feedback{};
        std::vector<uint8_t> buffer{};
        size_t feedbackOffset{ 0 };
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::string_view custom = "KMAC_CFB") noexcept {
        ctx.key.assign(key.begin(), key.end());
        ctx.custom = std::string(custom);
        ctx.buffer.clear();
        ctx.bytesProcessed = 0;
        ctx.feedbackOffset = 0;
        ctx.initialized = true;
        GenericKmacXof<CShakeType>::init(ctx.kmacXof, key, custom);
        ctx.feedback.resize(DEFAULT_DIGEST_SIZE);
        GenericKmacXof<CShakeType>::final(ctx.kmacXof, ctx.feedback);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void encrypt(Context& ctx, std::span<const uint8_t> plaintext, std::span<uint8_t> ciphertext) noexcept {
        if (!ctx.initialized || plaintext.empty() || ciphertext.empty()) return;
        size_t len = std::min(plaintext.size(), ciphertext.size());
        for (size_t i = 0; i < len; ++i) {
            if (ctx.feedbackOffset >= ctx.feedback.size()) {
                typename GenericKmacXof<CShakeType>::Context nextKmac{};
                GenericKmacXof<CShakeType>::init(nextKmac, ctx.key, ctx.custom);
                GenericKmacXof<CShakeType>::update(nextKmac, ctx.feedback);
                ctx.feedback.resize(DEFAULT_DIGEST_SIZE);
                GenericKmacXof<CShakeType>::final(nextKmac, ctx.feedback);
                ctx.feedbackOffset = 0;
            }
            uint8_t c = plaintext[i] ^ ctx.feedback[ctx.feedbackOffset];
            ciphertext[i] = c;
            ctx.feedback[ctx.feedbackOffset] = c;
            ctx.feedbackOffset++;
        }
        ctx.bytesProcessed += len;
    }

    static void decrypt(Context& ctx, std::span<const uint8_t> ciphertext, std::span<uint8_t> plaintext) noexcept {
        if (!ctx.initialized || ciphertext.empty() || plaintext.empty()) return;
        size_t len = std::min(ciphertext.size(), plaintext.size());
        for (size_t i = 0; i < len; ++i) {
            if (ctx.feedbackOffset >= ctx.feedback.size()) {
                typename GenericKmacXof<CShakeType>::Context nextKmac{};
                GenericKmacXof<CShakeType>::init(nextKmac, ctx.key, ctx.custom);
                GenericKmacXof<CShakeType>::update(nextKmac, ctx.feedback);
                ctx.feedback.resize(DEFAULT_DIGEST_SIZE);
                GenericKmacXof<CShakeType>::final(nextKmac, ctx.feedback);
                ctx.feedbackOffset = 0;
            }
            uint8_t c = ciphertext[i];
            plaintext[i] = c ^ ctx.feedback[ctx.feedbackOffset];
            ctx.feedback[ctx.feedbackOffset] = c;
            ctx.feedbackOffset++;
        }
        ctx.bytesProcessed += len;
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || out.empty()) return;
        if (!ctx.buffer.empty()) {
            size_t len = std::min(ctx.buffer.size(), out.size());
            encrypt(ctx, std::span<const uint8_t>(ctx.buffer.data(), len), std::span<uint8_t>(out.data(), len));
            ctx.buffer.clear();
        } else {
            std::vector<uint8_t> dummy(out.size(), 0);
            encrypt(ctx, dummy, out);
        }
    }
};

using KmacCfb128 = GenericKmacCfb<CShake128>;
using KmacCfb256 = GenericKmacCfb<CShake256>;

template<typename CShakeType>
class GenericKmacSiv {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;
    static constexpr size_t TAG_SIZE = DEFAULT_DIGEST_SIZE;

    struct Context {
        std::vector<uint8_t> masterKey{};
        std::vector<uint8_t> macKey{};
        std::vector<uint8_t> encKey{};
        std::string custom{};
        std::vector<uint8_t> ad{};
        std::vector<uint8_t> buffer{};
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::string_view custom = "KMAC_SIV") noexcept {
        ctx.masterKey.assign(key.begin(), key.end());
        ctx.custom = std::string(custom);
        ctx.buffer.clear();
        ctx.ad.clear();
        ctx.bytesProcessed = 0;
        ctx.initialized = true;

        ctx.macKey.resize(DEFAULT_DIGEST_SIZE);
        ctx.encKey.resize(DEFAULT_DIGEST_SIZE);

        typename GenericKmacXof<CShakeType>::Context kdfCtx{};
        GenericKmacXof<CShakeType>::init(kdfCtx, key, "KMAC_SIV_KDF");
        const char macLabel[] = "SIV_MAC_KEY";
        GenericKmacXof<CShakeType>::update(kdfCtx, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(macLabel), sizeof(macLabel) - 1));
        GenericKmacXof<CShakeType>::final(kdfCtx, ctx.macKey);

        GenericKmacXof<CShakeType>::init(kdfCtx, key, "KMAC_SIV_KDF");
        const char encLabel[] = "SIV_ENC_KEY";
        GenericKmacXof<CShakeType>::update(kdfCtx, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(encLabel), sizeof(encLabel) - 1));
        GenericKmacXof<CShakeType>::final(kdfCtx, ctx.encKey);
    }

    static void setAssociatedData(Context& ctx, std::span<const uint8_t> ad) noexcept {
        if (!ctx.initialized) return;
        ctx.ad.assign(ad.begin(), ad.end());
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void seal(Context& ctx, std::span<const uint8_t> plaintext, std::span<uint8_t> outTag, std::span<uint8_t> ciphertext) noexcept {
        if (!ctx.initialized || outTag.size() < TAG_SIZE) return;

        typename GenericKmacXof<CShakeType>::Context macCtx{};
        GenericKmacXof<CShakeType>::init(macCtx, ctx.macKey, "KMAC_SIV_AUTH");
        if (!ctx.ad.empty()) {
            GenericKmacXof<CShakeType>::update(macCtx, ctx.ad);
        }
        if (!plaintext.empty()) {
            GenericKmacXof<CShakeType>::update(macCtx, plaintext);
        }
        std::vector<uint8_t> syntheticIv(TAG_SIZE);
        GenericKmacXof<CShakeType>::final(macCtx, syntheticIv);
        std::memcpy(outTag.data(), syntheticIv.data(), TAG_SIZE);

        if (!plaintext.empty() && ciphertext.size() >= plaintext.size()) {
            typename GenericKmacXof<CShakeType>::Context streamCtx{};
            GenericKmacXof<CShakeType>::init(streamCtx, ctx.encKey, "KMAC_SIV_STREAM");
            GenericKmacXof<CShakeType>::update(streamCtx, syntheticIv);
            std::vector<uint8_t> keystream(plaintext.size());
            GenericKmacXof<CShakeType>::final(streamCtx, keystream);

            for (size_t i = 0; i < plaintext.size(); ++i) {
                ciphertext[i] = plaintext[i] ^ keystream[i];
            }
        }
        ctx.bytesProcessed += plaintext.size();
    }

    static bool open(Context& ctx, std::span<const uint8_t> ciphertext, std::span<const uint8_t> tag, std::span<uint8_t> plaintext) noexcept {
        if (!ctx.initialized || tag.size() < TAG_SIZE || plaintext.size() < ciphertext.size()) return false;

        typename GenericKmacXof<CShakeType>::Context streamCtx{};
        GenericKmacXof<CShakeType>::init(streamCtx, ctx.encKey, "KMAC_SIV_STREAM");
        GenericKmacXof<CShakeType>::update(streamCtx, tag.subspan(0, TAG_SIZE));
        std::vector<uint8_t> keystream(ciphertext.size());
        GenericKmacXof<CShakeType>::final(streamCtx, keystream);

        std::vector<uint8_t> candidatePt(ciphertext.size());
        for (size_t i = 0; i < ciphertext.size(); ++i) {
            candidatePt[i] = ciphertext[i] ^ keystream[i];
        }

        typename GenericKmacXof<CShakeType>::Context macCtx{};
        GenericKmacXof<CShakeType>::init(macCtx, ctx.macKey, "KMAC_SIV_AUTH");
        if (!ctx.ad.empty()) {
            GenericKmacXof<CShakeType>::update(macCtx, ctx.ad);
        }
        if (!candidatePt.empty()) {
            GenericKmacXof<CShakeType>::update(macCtx, candidatePt);
        }
        std::vector<uint8_t> expectedTag(TAG_SIZE);
        GenericKmacXof<CShakeType>::final(macCtx, expectedTag);

        uint8_t diff = 0;
        for (size_t i = 0; i < TAG_SIZE; ++i) {
            diff |= (tag[i] ^ expectedTag[i]);
        }
        if (diff != 0) return false;

        std::memcpy(plaintext.data(), candidatePt.data(), candidatePt.size());
        ctx.bytesProcessed += ciphertext.size();
        return true;
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || out.empty()) return;
        if (out.size() >= TAG_SIZE) {
            std::vector<uint8_t> dummyTag(TAG_SIZE);
            std::vector<uint8_t> ct(ctx.buffer.size());
            seal(ctx, ctx.buffer, dummyTag, ct);
            std::memcpy(out.data(), dummyTag.data(), std::min(out.size(), TAG_SIZE));
            ctx.buffer.clear();
        }
    }
};

using KmacSiv128 = GenericKmacSiv<CShake128>;
using KmacSiv256 = GenericKmacSiv<CShake256>;

template<typename CShakeType>
class GenericKmacFfx {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;
    static constexpr uint32_t DEFAULT_ROUNDS = 10;

    struct Context {
        std::vector<uint8_t> masterKey{};
        std::vector<uint8_t> tweak{};
        std::string custom{};
        std::vector<uint8_t> buffer{};
        uint32_t rounds{ DEFAULT_ROUNDS };
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::span<const uint8_t> tweak = {}, std::string_view custom = "KMAC_FFX", uint32_t rounds = DEFAULT_ROUNDS) noexcept {
        ctx.masterKey.assign(key.begin(), key.end());
        ctx.tweak.assign(tweak.begin(), tweak.end());
        ctx.custom = std::string(custom);
        ctx.buffer.clear();
        ctx.rounds = rounds;
        ctx.bytesProcessed = 0;
        ctx.initialized = true;
    }

    static void setTweak(Context& ctx, std::span<const uint8_t> tweak) noexcept {
        if (!ctx.initialized) return;
        ctx.tweak.assign(tweak.begin(), tweak.end());
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void encrypt(Context& ctx, std::span<const uint8_t> inPlaintext, std::span<uint8_t> outCiphertext) noexcept {
        if (!ctx.initialized || inPlaintext.empty() || outCiphertext.size() < inPlaintext.size()) return;
        size_t n = inPlaintext.size();
        if (n == 1) {
            typename GenericKmacXof<CShakeType>::Context kx{};
            GenericKmacXof<CShakeType>::init(kx, ctx.masterKey, "KMAC_FFX_ROUND");
            if (!ctx.tweak.empty()) GenericKmacXof<CShakeType>::update(kx, ctx.tweak);
            uint8_t roundByte = 0;
            GenericKmacXof<CShakeType>::update(kx, std::span<const uint8_t>(&roundByte, 1));
            std::vector<uint8_t> mask(1);
            GenericKmacXof<CShakeType>::final(kx, mask);
            outCiphertext[0] = static_cast<uint8_t>((inPlaintext[0] + mask[0]) & 0xFF);
            ctx.bytesProcessed += 1;
            return;
        }

        size_t u = n / 2;
        size_t v = n - u;
        std::vector<uint8_t> A(inPlaintext.begin(), inPlaintext.begin() + u);
        std::vector<uint8_t> B(inPlaintext.begin() + u, inPlaintext.end());

        for (uint32_t r = 0; r < ctx.rounds; ++r) {
            typename GenericKmacXof<CShakeType>::Context roundCtx{};
            GenericKmacXof<CShakeType>::init(roundCtx, ctx.masterKey, "KMAC_FFX_ROUND");
            if (!ctx.tweak.empty()) {
                GenericKmacXof<CShakeType>::update(roundCtx, ctx.tweak);
            }
            uint8_t rByte = static_cast<uint8_t>(r);
            GenericKmacXof<CShakeType>::update(roundCtx, std::span<const uint8_t>(&rByte, 1));
            GenericKmacXof<CShakeType>::update(roundCtx, B);
            std::vector<uint8_t> F(u);
            GenericKmacXof<CShakeType>::final(roundCtx, F);

            std::vector<uint8_t> C(u);
            for (size_t i = 0; i < u; ++i) {
                C[i] = static_cast<uint8_t>((A[i] + F[i]) & 0xFF);
            }

            A = B;
            B = C;
            std::swap(u, v);
        }

        std::memcpy(outCiphertext.data(), A.data(), A.size());
        std::memcpy(outCiphertext.data() + A.size(), B.data(), B.size());
        ctx.bytesProcessed += n;
    }

    static bool decrypt(Context& ctx, std::span<const uint8_t> inCiphertext, std::span<uint8_t> outPlaintext) noexcept {
        if (!ctx.initialized || inCiphertext.empty() || outPlaintext.size() < inCiphertext.size()) return false;
        size_t n = inCiphertext.size();
        if (n == 1) {
            typename GenericKmacXof<CShakeType>::Context kx{};
            GenericKmacXof<CShakeType>::init(kx, ctx.masterKey, "KMAC_FFX_ROUND");
            if (!ctx.tweak.empty()) GenericKmacXof<CShakeType>::update(kx, ctx.tweak);
            uint8_t roundByte = 0;
            GenericKmacXof<CShakeType>::update(kx, std::span<const uint8_t>(&roundByte, 1));
            std::vector<uint8_t> mask(1);
            GenericKmacXof<CShakeType>::final(kx, mask);
            outPlaintext[0] = static_cast<uint8_t>((inCiphertext[0] - mask[0]) & 0xFF);
            ctx.bytesProcessed += 1;
            return true;
        }

        size_t u = n / 2;
        size_t v = n - u;
        for (uint32_t r = 0; r < ctx.rounds; ++r) {
            std::swap(u, v);
        }

        std::vector<uint8_t> A(inCiphertext.begin(), inCiphertext.begin() + u);
        std::vector<uint8_t> B(inCiphertext.begin() + u, inCiphertext.end());

        for (int32_t r = static_cast<int32_t>(ctx.rounds) - 1; r >= 0; --r) {
            std::swap(u, v);
            std::vector<uint8_t> prevB = A;
            std::vector<uint8_t> prevC = B;

            typename GenericKmacXof<CShakeType>::Context roundCtx{};
            GenericKmacXof<CShakeType>::init(roundCtx, ctx.masterKey, "KMAC_FFX_ROUND");
            if (!ctx.tweak.empty()) {
                GenericKmacXof<CShakeType>::update(roundCtx, ctx.tweak);
            }
            uint8_t rByte = static_cast<uint8_t>(r);
            GenericKmacXof<CShakeType>::update(roundCtx, std::span<const uint8_t>(&rByte, 1));
            GenericKmacXof<CShakeType>::update(roundCtx, prevB);
            std::vector<uint8_t> F(u);
            GenericKmacXof<CShakeType>::final(roundCtx, F);

            std::vector<uint8_t> prevA(u);
            for (size_t i = 0; i < u; ++i) {
                prevA[i] = static_cast<uint8_t>((prevC[i] - F[i]) & 0xFF);
            }

            A = prevA;
            B = prevB;
        }

        std::memcpy(outPlaintext.data(), A.data(), A.size());
        std::memcpy(outPlaintext.data() + A.size(), B.data(), B.size());
        ctx.bytesProcessed += n;
        return true;
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || out.empty()) return;
        if (!ctx.buffer.empty()) {
            std::vector<uint8_t> ct(ctx.buffer.size());
            encrypt(ctx, ctx.buffer, ct);
            size_t copyLen = std::min(out.size(), ct.size());
            std::memcpy(out.data(), ct.data(), copyLen);
            ctx.buffer.clear();
        }
    }
};

using KmacFfx128 = GenericKmacFfx<CShake128>;
using KmacFfx256 = GenericKmacFfx<CShake256>;

template<typename CShakeType>
class GenericKmacBps {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;
    static constexpr uint32_t DEFAULT_ROUNDS = 8;

    struct Context {
        std::vector<uint8_t> masterKey{};
        std::vector<uint8_t> tweak{};
        std::string custom{};
        std::vector<uint8_t> buffer{};
        uint32_t rounds{ DEFAULT_ROUNDS };
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::span<const uint8_t> tweak = {}, std::string_view custom = "KMAC_BPS", uint32_t rounds = DEFAULT_ROUNDS) noexcept {
        ctx.masterKey.assign(key.begin(), key.end());
        ctx.tweak.assign(tweak.begin(), tweak.end());
        ctx.custom = std::string(custom);
        ctx.buffer.clear();
        ctx.rounds = rounds;
        ctx.bytesProcessed = 0;
        ctx.initialized = true;
    }

    static void setTweak(Context& ctx, std::span<const uint8_t> tweak) noexcept {
        if (!ctx.initialized) return;
        ctx.tweak.assign(tweak.begin(), tweak.end());
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void encrypt(Context& ctx, std::span<const uint8_t> inPlaintext, std::span<uint8_t> outCiphertext) noexcept {
        if (!ctx.initialized || inPlaintext.empty() || outCiphertext.size() < inPlaintext.size()) return;
        size_t n = inPlaintext.size();
        if (n == 1) {
            typename GenericKmacXof<CShakeType>::Context kx{};
            GenericKmacXof<CShakeType>::init(kx, ctx.masterKey, "KMAC_BPS_ROUND");
            if (!ctx.tweak.empty()) GenericKmacXof<CShakeType>::update(kx, ctx.tweak);
            uint8_t roundByte = 0;
            GenericKmacXof<CShakeType>::update(kx, std::span<const uint8_t>(&roundByte, 1));
            std::vector<uint8_t> mask(1);
            GenericKmacXof<CShakeType>::final(kx, mask);
            outCiphertext[0] = static_cast<uint8_t>((inPlaintext[0] + mask[0]) & 0xFF);
            ctx.bytesProcessed += 1;
            return;
        }

        size_t u = n / 2;
        size_t v = n - u;
        std::vector<uint8_t> A(inPlaintext.begin(), inPlaintext.begin() + u);
        std::vector<uint8_t> B(inPlaintext.begin() + u, inPlaintext.end());

        for (uint32_t r = 0; r < ctx.rounds; ++r) {
            typename GenericKmacXof<CShakeType>::Context roundCtx{};
            GenericKmacXof<CShakeType>::init(roundCtx, ctx.masterKey, "KMAC_BPS_ROUND");
            if (!ctx.tweak.empty()) {
                GenericKmacXof<CShakeType>::update(roundCtx, ctx.tweak);
            }
            uint8_t rByte = static_cast<uint8_t>(r);
            GenericKmacXof<CShakeType>::update(roundCtx, std::span<const uint8_t>(&rByte, 1));
            GenericKmacXof<CShakeType>::update(roundCtx, B);
            std::vector<uint8_t> F(u);
            GenericKmacXof<CShakeType>::final(roundCtx, F);

            std::vector<uint8_t> C(u);
            for (size_t i = 0; i < u; ++i) {
                C[i] = static_cast<uint8_t>((A[i] + F[i]) & 0xFF);
            }

            A = B;
            B = C;
            std::swap(u, v);
        }

        std::memcpy(outCiphertext.data(), A.data(), A.size());
        std::memcpy(outCiphertext.data() + A.size(), B.data(), B.size());
        ctx.bytesProcessed += n;
    }

    static bool decrypt(Context& ctx, std::span<const uint8_t> inCiphertext, std::span<uint8_t> outPlaintext) noexcept {
        if (!ctx.initialized || inCiphertext.empty() || outPlaintext.size() < inCiphertext.size()) return false;
        size_t n = inCiphertext.size();
        if (n == 1) {
            typename GenericKmacXof<CShakeType>::Context kx{};
            GenericKmacXof<CShakeType>::init(kx, ctx.masterKey, "KMAC_BPS_ROUND");
            if (!ctx.tweak.empty()) GenericKmacXof<CShakeType>::update(kx, ctx.tweak);
            uint8_t roundByte = 0;
            GenericKmacXof<CShakeType>::update(kx, std::span<const uint8_t>(&roundByte, 1));
            std::vector<uint8_t> mask(1);
            GenericKmacXof<CShakeType>::final(kx, mask);
            outPlaintext[0] = static_cast<uint8_t>((inCiphertext[0] - mask[0]) & 0xFF);
            ctx.bytesProcessed += 1;
            return true;
        }

        size_t u = n / 2;
        size_t v = n - u;
        for (uint32_t r = 0; r < ctx.rounds; ++r) {
            std::swap(u, v);
        }

        std::vector<uint8_t> A(inCiphertext.begin(), inCiphertext.begin() + u);
        std::vector<uint8_t> B(inCiphertext.begin() + u, inCiphertext.end());

        for (int32_t r = static_cast<int32_t>(ctx.rounds) - 1; r >= 0; --r) {
            std::swap(u, v);
            std::vector<uint8_t> prevB = A;
            std::vector<uint8_t> prevC = B;

            typename GenericKmacXof<CShakeType>::Context roundCtx{};
            GenericKmacXof<CShakeType>::init(roundCtx, ctx.masterKey, "KMAC_BPS_ROUND");
            if (!ctx.tweak.empty()) {
                GenericKmacXof<CShakeType>::update(roundCtx, ctx.tweak);
            }
            uint8_t rByte = static_cast<uint8_t>(r);
            GenericKmacXof<CShakeType>::update(roundCtx, std::span<const uint8_t>(&rByte, 1));
            GenericKmacXof<CShakeType>::update(roundCtx, prevB);
            std::vector<uint8_t> F(u);
            GenericKmacXof<CShakeType>::final(roundCtx, F);

            std::vector<uint8_t> prevA(u);
            for (size_t i = 0; i < u; ++i) {
                prevA[i] = static_cast<uint8_t>((prevC[i] - F[i]) & 0xFF);
            }

            A = prevA;
            B = prevB;
        }

        std::memcpy(outPlaintext.data(), A.data(), A.size());
        std::memcpy(outPlaintext.data() + A.size(), B.data(), B.size());
        ctx.bytesProcessed += n;
        return true;
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || out.empty()) return;
        if (!ctx.buffer.empty()) {
            std::vector<uint8_t> ct(ctx.buffer.size());
            encrypt(ctx, ctx.buffer, ct);
            size_t copyLen = std::min(out.size(), ct.size());
            std::memcpy(out.data(), ct.data(), copyLen);
            ctx.buffer.clear();
        }
    }
};

using KmacBps128 = GenericKmacBps<CShake128>;
using KmacBps256 = GenericKmacBps<CShake256>;

template<typename CShakeType>
class GenericKmacKw {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;
    static constexpr uint64_t DEFAULT_IV = 0xA6A6A6A6A6A6A6A6ULL;

    struct Context {
        std::vector<uint8_t> masterKey{};
        std::vector<uint8_t> tweak{};
        std::string custom{};
        std::vector<uint8_t> buffer{};
        uint64_t wrappedCount{ 0 };
        uint64_t unwrappedCount{ 0 };
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::span<const uint8_t> tweak = {}, std::string_view custom = "KMAC_KW") noexcept {
        ctx.masterKey.assign(key.begin(), key.end());
        ctx.tweak.assign(tweak.begin(), tweak.end());
        ctx.custom = std::string(custom);
        ctx.buffer.clear();
        ctx.wrappedCount = 0;
        ctx.unwrappedCount = 0;
        ctx.bytesProcessed = 0;
        ctx.initialized = true;
    }

    static void setTweak(Context& ctx, std::span<const uint8_t> tweak) noexcept {
        if (!ctx.initialized) return;
        ctx.tweak.assign(tweak.begin(), tweak.end());
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static bool wrapKey(
        Context& ctx,
        std::span<const uint8_t> plaintextKey,
        std::vector<uint8_t>& wrappedKey,
        std::span<const uint8_t> associatedData = {}) noexcept {
        
        if (!ctx.initialized || plaintextKey.size() < 16 || (plaintextKey.size() % 8) != 0) return false;
        size_t n = plaintextKey.size() / 8;
        uint64_t A = DEFAULT_IV;

        std::vector<uint64_t> R(n);
        for (size_t i = 0; i < n; ++i) {
            uint64_t val = 0;
            for (size_t b = 0; b < 8; ++b) {
                val = (val << 8) | plaintextKey[i * 8 + b];
            }
            R[i] = val;
        }

        for (uint32_t j = 0; j < 6; ++j) {
            for (size_t i = 0; i < n; ++i) {
                uint64_t t = static_cast<uint64_t>(n * j + (i + 1));
                typename GenericKmacXof<CShakeType>::Context roundCtx{};
                GenericKmacXof<CShakeType>::init(roundCtx, ctx.masterKey, "KMAC_KW_ROUND");
                uint8_t aBytes[8];
                for (size_t b = 0; b < 8; ++b) aBytes[b] = static_cast<uint8_t>((A >> (56 - b * 8)) & 0xFF);
                GenericKmacXof<CShakeType>::update(roundCtx, std::span<const uint8_t>(aBytes, 8));
                uint8_t tBytes[8];
                for (size_t b = 0; b < 8; ++b) tBytes[b] = static_cast<uint8_t>((t >> (56 - b * 8)) & 0xFF);
                GenericKmacXof<CShakeType>::update(roundCtx, std::span<const uint8_t>(tBytes, 8));
                if (!ctx.tweak.empty()) {
                    GenericKmacXof<CShakeType>::update(roundCtx, ctx.tweak);
                }
                if (!associatedData.empty()) {
                    GenericKmacXof<CShakeType>::update(roundCtx, associatedData);
                }
                uint8_t maskBytes[8]{};
                GenericKmacXof<CShakeType>::final(roundCtx, std::span<uint8_t>(maskBytes, 8));
                uint64_t mask = 0;
                for (size_t b = 0; b < 8; ++b) mask = (mask << 8) | maskBytes[b];

                R[i] ^= mask;
                A ^= (R[i] + t);
            }
        }

        wrappedKey.resize((n + 1) * 8);
        for (size_t b = 0; b < 8; ++b) {
            wrappedKey[b] = static_cast<uint8_t>((A >> (56 - b * 8)) & 0xFF);
        }
        for (size_t i = 0; i < n; ++i) {
            for (size_t b = 0; b < 8; ++b) {
                wrappedKey[(i + 1) * 8 + b] = static_cast<uint8_t>((R[i] >> (56 - b * 8)) & 0xFF);
            }
        }

        ctx.wrappedCount++;
        ctx.bytesProcessed += plaintextKey.size();
        return true;
    }

    static bool unwrapKey(
        Context& ctx,
        std::span<const uint8_t> wrappedKey,
        std::vector<uint8_t>& unwrappedKey,
        std::span<const uint8_t> associatedData = {}) noexcept {
        
        if (!ctx.initialized || wrappedKey.size() < 24 || (wrappedKey.size() % 8) != 0) return false;
        size_t n = (wrappedKey.size() / 8) - 1;
        if (n < 2) return false;

        uint64_t A = 0;
        for (size_t b = 0; b < 8; ++b) {
            A = (A << 8) | wrappedKey[b];
        }

        std::vector<uint64_t> R(n);
        for (size_t i = 0; i < n; ++i) {
            uint64_t val = 0;
            for (size_t b = 0; b < 8; ++b) {
                val = (val << 8) | wrappedKey[(i + 1) * 8 + b];
            }
            R[i] = val;
        }

        for (int32_t j = 5; j >= 0; --j) {
            for (int32_t i = static_cast<int32_t>(n) - 1; i >= 0; --i) {
                uint64_t t = static_cast<uint64_t>(n * static_cast<size_t>(j) + (static_cast<size_t>(i) + 1));
                A ^= (R[i] + t);

                typename GenericKmacXof<CShakeType>::Context roundCtx{};
                GenericKmacXof<CShakeType>::init(roundCtx, ctx.masterKey, "KMAC_KW_ROUND");
                uint8_t aBytes[8];
                for (size_t b = 0; b < 8; ++b) aBytes[b] = static_cast<uint8_t>((A >> (56 - b * 8)) & 0xFF);
                GenericKmacXof<CShakeType>::update(roundCtx, std::span<const uint8_t>(aBytes, 8));
                uint8_t tBytes[8];
                for (size_t b = 0; b < 8; ++b) tBytes[b] = static_cast<uint8_t>((t >> (56 - b * 8)) & 0xFF);
                GenericKmacXof<CShakeType>::update(roundCtx, std::span<const uint8_t>(tBytes, 8));
                if (!ctx.tweak.empty()) {
                    GenericKmacXof<CShakeType>::update(roundCtx, ctx.tweak);
                }
                if (!associatedData.empty()) {
                    GenericKmacXof<CShakeType>::update(roundCtx, associatedData);
                }
                uint8_t maskBytes[8]{};
                GenericKmacXof<CShakeType>::final(roundCtx, std::span<uint8_t>(maskBytes, 8));
                uint64_t mask = 0;
                for (size_t b = 0; b < 8; ++b) mask = (mask << 8) | maskBytes[b];

                R[i] ^= mask;
            }
        }

        uint64_t diff = A ^ DEFAULT_IV;
        if (diff != 0) return false;

        unwrappedKey.resize(n * 8);
        for (size_t i = 0; i < n; ++i) {
            for (size_t b = 0; b < 8; ++b) {
                unwrappedKey[i * 8 + b] = static_cast<uint8_t>((R[i] >> (56 - b * 8)) & 0xFF);
            }
        }

        ctx.unwrappedCount++;
        ctx.bytesProcessed += unwrappedKey.size();
        return true;
    }

    static void encrypt(Context& ctx, std::span<const uint8_t> inPlaintext, std::span<uint8_t> outCiphertext) noexcept {
        std::vector<uint8_t> wk;
        if (wrapKey(ctx, inPlaintext, wk)) {
            size_t copyLen = std::min(outCiphertext.size(), wk.size());
            std::memcpy(outCiphertext.data(), wk.data(), copyLen);
        }
    }

    static bool decrypt(Context& ctx, std::span<const uint8_t> inCiphertext, std::span<uint8_t> outPlaintext) noexcept {
        std::vector<uint8_t> pt;
        if (unwrapKey(ctx, inCiphertext, pt)) {
            size_t copyLen = std::min(outPlaintext.size(), pt.size());
            std::memcpy(outPlaintext.data(), pt.data(), copyLen);
            return true;
        }
        return false;
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || out.empty()) return;
        if (!ctx.buffer.empty()) {
            std::vector<uint8_t> wrapped;
            if (ctx.buffer.size() >= 16 && (ctx.buffer.size() % 8) == 0) {
                wrapKey(ctx, ctx.buffer, wrapped);
            } else {
                std::vector<uint8_t> padded = ctx.buffer;
                while (padded.size() < 16 || (padded.size() % 8) != 0) {
                    padded.push_back(0);
                }
                wrapKey(ctx, padded, wrapped);
            }
            size_t copyLen = std::min(out.size(), wrapped.size());
            std::memcpy(out.data(), wrapped.data(), copyLen);
            ctx.buffer.clear();
        } else {
            typename GenericKmacXof<CShakeType>::Context kx{};
            GenericKmacXof<CShakeType>::init(kx, ctx.masterKey, "KMAC_KW_DIGEST");
            GenericKmacXof<CShakeType>::final(kx, out);
        }
    }
};

using KmacKw128 = GenericKmacKw<CShake128>;
using KmacKw256 = GenericKmacKw<CShake256>;

template<typename CShakeType>
class GenericKmacDrbg {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;
    static constexpr size_t SEED_SIZE = (DEFAULT_DIGEST_SIZE == 32) ? 32 : 64;
    static constexpr uint64_t RESEED_INTERVAL = 10000000ULL;

    struct Context {
        std::vector<uint8_t> key{};
        std::vector<uint8_t> v{};
        uint64_t reseedCounter{ 0 };
        uint64_t totalBytesGenerated{ 0 };
        uint32_t securityStrength{ static_cast<uint32_t>(DEFAULT_DIGEST_SIZE * 8) };
        bool initialized{ false };
    };

    static void instantiate(
        Context& ctx,
        std::span<const uint8_t> entropy,
        std::span<const uint8_t> nonce = {},
        std::span<const uint8_t> personalization = {}) noexcept {
        
        ctx.key.assign(SEED_SIZE, 0x00);
        ctx.v.assign(SEED_SIZE, 0x01);
        ctx.reseedCounter = 1;
        ctx.totalBytesGenerated = 0;
        ctx.securityStrength = static_cast<uint32_t>(DEFAULT_DIGEST_SIZE * 8);

        std::vector<uint8_t> seedMaterial;
        seedMaterial.reserve(entropy.size() + nonce.size() + personalization.size());
        seedMaterial.insert(seedMaterial.end(), entropy.begin(), entropy.end());
        seedMaterial.insert(seedMaterial.end(), nonce.begin(), nonce.end());
        seedMaterial.insert(seedMaterial.end(), personalization.begin(), personalization.end());

        updateInternal(ctx, seedMaterial);
        ctx.initialized = true;
    }

    static void init(
        Context& ctx,
        std::span<const uint8_t> entropy,
        std::span<const uint8_t> nonce = {},
        std::string_view label = "KMAC_DRBG") noexcept {
        std::span<const uint8_t> pers(reinterpret_cast<const uint8_t*>(label.data()), label.size());
        instantiate(ctx, entropy, nonce, pers);
    }

    static void reseed(
        Context& ctx,
        std::span<const uint8_t> entropy,
        std::span<const uint8_t> additionalInput = {}) noexcept {
        if (!ctx.initialized) {
            instantiate(ctx, entropy, {}, additionalInput);
            return;
        }

        std::vector<uint8_t> seedMaterial;
        seedMaterial.reserve(entropy.size() + additionalInput.size());
        seedMaterial.insert(seedMaterial.end(), entropy.begin(), entropy.end());
        seedMaterial.insert(seedMaterial.end(), additionalInput.begin(), additionalInput.end());

        updateInternal(ctx, seedMaterial);
        ctx.reseedCounter = 1;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        reseed(ctx, data);
    }

    static bool generate(
        Context& ctx,
        std::span<uint8_t> outBytes,
        std::span<const uint8_t> additionalInput = {},
        bool predictionResistance = false) noexcept {
        if (!ctx.initialized) {
            uint8_t defaultEntropy[SEED_SIZE];
            for (size_t i = 0; i < SEED_SIZE; ++i) defaultEntropy[i] = static_cast<uint8_t>(i ^ 0xA5);
            instantiate(ctx, std::span<const uint8_t>(defaultEntropy, SEED_SIZE));
        }
        if (outBytes.empty()) return true;
        if (ctx.reseedCounter > RESEED_INTERVAL) return false;

        if (predictionResistance && !additionalInput.empty()) {
            reseed(ctx, additionalInput);
        } else if (!additionalInput.empty()) {
            updateInternal(ctx, additionalInput);
        }

        size_t bytesLeft = outBytes.size();
        size_t offset = 0;

        while (bytesLeft > 0) {
            typename GenericKmacXof<CShakeType>::Context vCtx{};
            GenericKmacXof<CShakeType>::init(vCtx, ctx.key, "KMAC_DRBG_V");
            GenericKmacXof<CShakeType>::update(vCtx, ctx.v);
            std::vector<uint8_t> nextV(ctx.v.size());
            GenericKmacXof<CShakeType>::final(vCtx, nextV);
            ctx.v = std::move(nextV);

            typename GenericKmacXof<CShakeType>::Context genCtx{};
            GenericKmacXof<CShakeType>::init(genCtx, ctx.key, "KMAC_DRBG_GENERATE");
            GenericKmacXof<CShakeType>::update(genCtx, ctx.v);

            size_t take = (std::min)(bytesLeft, SEED_SIZE);
            std::vector<uint8_t> block(SEED_SIZE);
            GenericKmacXof<CShakeType>::final(genCtx, block);

            std::memcpy(outBytes.data() + offset, block.data(), take);
            offset += take;
            bytesLeft -= take;
        }

        updateInternal(ctx, additionalInput);
        ctx.reseedCounter++;
        ctx.totalBytesGenerated += outBytes.size();
        return true;
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        generate(ctx, out);
    }

    static void uninstantiate(Context& ctx) noexcept {
        std::fill(ctx.key.begin(), ctx.key.end(), 0);
        std::fill(ctx.v.begin(), ctx.v.end(), 0);
        ctx.key.clear();
        ctx.v.clear();
        ctx.reseedCounter = 0;
        ctx.totalBytesGenerated = 0;
        ctx.initialized = false;
    }

private:
    static void updateInternal(Context& ctx, std::span<const uint8_t> providedData) noexcept {
        {
            typename GenericKmacXof<CShakeType>::Context kCtx{};
            GenericKmacXof<CShakeType>::init(kCtx, ctx.key, "KMAC_DRBG_UPDATE_K");
            GenericKmacXof<CShakeType>::update(kCtx, ctx.v);
            uint8_t sep = 0x01;
            GenericKmacXof<CShakeType>::update(kCtx, std::span<const uint8_t>(&sep, 1));
            if (!providedData.empty()) {
                GenericKmacXof<CShakeType>::update(kCtx, providedData);
            }
            std::vector<uint8_t> newKey(SEED_SIZE);
            GenericKmacXof<CShakeType>::final(kCtx, newKey);
            ctx.key = std::move(newKey);
        }

        {
            typename GenericKmacXof<CShakeType>::Context vCtx{};
            GenericKmacXof<CShakeType>::init(vCtx, ctx.key, "KMAC_DRBG_UPDATE_V");
            GenericKmacXof<CShakeType>::update(vCtx, ctx.v);
            std::vector<uint8_t> newV(SEED_SIZE);
            GenericKmacXof<CShakeType>::final(vCtx, newV);
            ctx.v = std::move(newV);
        }

        if (!providedData.empty()) {
            typename GenericKmacXof<CShakeType>::Context kCtx2{};
            GenericKmacXof<CShakeType>::init(kCtx2, ctx.key, "KMAC_DRBG_UPDATE_K2");
            GenericKmacXof<CShakeType>::update(kCtx2, ctx.v);
            uint8_t sep = 0x02;
            GenericKmacXof<CShakeType>::update(kCtx2, std::span<const uint8_t>(&sep, 1));
            GenericKmacXof<CShakeType>::update(kCtx2, providedData);
            std::vector<uint8_t> newKey2(SEED_SIZE);
            GenericKmacXof<CShakeType>::final(kCtx2, newKey2);
            ctx.key = std::move(newKey2);

            typename GenericKmacXof<CShakeType>::Context vCtx2{};
            GenericKmacXof<CShakeType>::init(vCtx2, ctx.key, "KMAC_DRBG_UPDATE_V2");
            GenericKmacXof<CShakeType>::update(vCtx2, ctx.v);
            std::vector<uint8_t> newV2(SEED_SIZE);
            GenericKmacXof<CShakeType>::final(vCtx2, newV2);
            ctx.v = std::move(newV2);
        }
    }
};

using KmacDrbg128 = GenericKmacDrbg<CShake128>;
using KmacDrbg256 = GenericKmacDrbg<CShake256>;

// ============================================================================
// NIST SP 800-232: Ascon-128 AEAD & Ascon-Hash Lightweight Cryptography
// ============================================================================

class AsconPermutation {
public:
    static inline uint64_t rotr(uint64_t x, int n) noexcept {
        return (x >> n) | (x << (64 - n));
    }

    static inline void round(uint64_t& x0, uint64_t& x1, uint64_t& x2, uint64_t& x3, uint64_t& x4, uint8_t r_const) noexcept {
        x2 ^= r_const;
        // S-box layer
        x0 ^= x4; x4 ^= x3; x2 ^= x1;
        uint64_t t0 = ~x0;
        uint64_t t1 = ~x1;
        uint64_t t2 = ~x2;
        uint64_t t3 = ~x3;
        uint64_t t4 = ~x4;
        t0 &= x1;
        t1 &= x2;
        t2 &= x3;
        t3 &= x4;
        t4 &= x0;
        x0 ^= t1;
        x1 ^= t2;
        x2 ^= t3;
        x3 ^= t4;
        x4 ^= t0;
        x1 ^= x0;
        x0 ^= x4;
        x3 ^= x2;
        x2 = ~x2;
        // Linear diffusion layer
        x0 ^= rotr(x0, 19) ^ rotr(x0, 28);
        x1 ^= rotr(x1, 61) ^ rotr(x1, 39);
        x2 ^= rotr(x2, 1)  ^ rotr(x2, 6);
        x3 ^= rotr(x3, 10) ^ rotr(x3, 17);
        x4 ^= rotr(x4, 7)  ^ rotr(x4, 41);
    }

    static inline void p12(uint64_t& x0, uint64_t& x1, uint64_t& x2, uint64_t& x3, uint64_t& x4) noexcept {
        static constexpr uint8_t constants[12] = {
            0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5, 0x96, 0x87, 0x78, 0x69, 0x5a, 0x4b
        };
        for (int i = 0; i < 12; ++i) {
            round(x0, x1, x2, x3, x4, constants[i]);
        }
    }

    static inline void p6(uint64_t& x0, uint64_t& x1, uint64_t& x2, uint64_t& x3, uint64_t& x4) noexcept {
        static constexpr uint8_t constants[6] = {
            0x96, 0x87, 0x78, 0x69, 0x5a, 0x4b
        };
        for (int i = 0; i < 6; ++i) {
            round(x0, x1, x2, x3, x4, constants[i]);
        }
    }

    static inline void p8(uint64_t& x0, uint64_t& x1, uint64_t& x2, uint64_t& x3, uint64_t& x4) noexcept {
        static constexpr uint8_t constants[8] = {
            0xb4, 0xa5, 0x96, 0x87, 0x78, 0x69, 0x5a, 0x4b
        };
        for (int i = 0; i < 8; ++i) {
            round(x0, x1, x2, x3, x4, constants[i]);
        }
    }

    static inline uint64_t load64_be(const uint8_t* b) noexcept {
        return (static_cast<uint64_t>(b[0]) << 56) |
               (static_cast<uint64_t>(b[1]) << 48) |
               (static_cast<uint64_t>(b[2]) << 40) |
               (static_cast<uint64_t>(b[3]) << 32) |
               (static_cast<uint64_t>(b[4]) << 24) |
               (static_cast<uint64_t>(b[5]) << 16) |
               (static_cast<uint64_t>(b[6]) << 8)  |
               (static_cast<uint64_t>(b[7]));
    }

    static inline void store64_be(uint8_t* b, uint64_t v) noexcept {
        b[0] = static_cast<uint8_t>((v >> 56) & 0xFF);
        b[1] = static_cast<uint8_t>((v >> 48) & 0xFF);
        b[2] = static_cast<uint8_t>((v >> 40) & 0xFF);
        b[3] = static_cast<uint8_t>((v >> 32) & 0xFF);
        b[4] = static_cast<uint8_t>((v >> 24) & 0xFF);
        b[5] = static_cast<uint8_t>((v >> 16) & 0xFF);
        b[6] = static_cast<uint8_t>((v >> 8) & 0xFF);
        b[7] = static_cast<uint8_t>(v & 0xFF);
    }
};

class Ascon128 {
public:
    static constexpr size_t KEY_SIZE = 16;
    static constexpr size_t NONCE_SIZE = 16;
    static constexpr size_t TAG_SIZE = 16;
    static constexpr size_t RATE = 8;
    static constexpr uint64_t ASCON128_IV = 0x80400c0600000000ULL;

    struct Context {
        uint64_t x0{ 0 }, x1{ 0 }, x2{ 0 }, x3{ 0 }, x4{ 0 };
        uint64_t k0{ 0 }, k1{ 0 };
        std::vector<uint8_t> key{};
        std::vector<uint8_t> nonce{};
        std::vector<uint8_t> ad{};
        std::vector<uint8_t> buffer{};
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::span<const uint8_t> nonce = {}) noexcept {
        ctx.key.assign(key.begin(), key.end());
        if (!nonce.empty()) {
            ctx.nonce.assign(nonce.begin(), nonce.end());
        } else {
            ctx.nonce.assign(NONCE_SIZE, 0);
        }
        ctx.ad.clear();
        ctx.buffer.clear();
        ctx.bytesProcessed = 0;
        ctx.initialized = true;
    }

    static void setAssociatedData(Context& ctx, std::span<const uint8_t> ad) noexcept {
        ctx.ad.assign(ad.begin(), ad.end());
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static bool encrypt(
        Context& ctx,
        std::span<const uint8_t> plaintext,
        std::span<uint8_t> outCiphertext,
        std::span<uint8_t> outTag,
        std::span<const uint8_t> ad = {},
        std::span<const uint8_t> nonce = {}) noexcept {
        
        if (!ctx.initialized || ctx.key.size() < KEY_SIZE) return false;
        if (outTag.size() < TAG_SIZE || outCiphertext.size() < plaintext.size()) return false;

        std::span<const uint8_t> n = !nonce.empty() ? nonce : std::span<const uint8_t>(ctx.nonce);
        if (n.size() < NONCE_SIZE) return false;

        ctx.k0 = AsconPermutation::load64_be(ctx.key.data());
        ctx.k1 = AsconPermutation::load64_be(ctx.key.data() + 8);
        uint64_t n0 = AsconPermutation::load64_be(n.data());
        uint64_t n1 = AsconPermutation::load64_be(n.data() + 8);

        ctx.x0 = ASCON128_IV;
        ctx.x1 = ctx.k0;
        ctx.x2 = ctx.k1;
        ctx.x3 = n0;
        ctx.x4 = n1;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x3 ^= ctx.k0;
        ctx.x4 ^= ctx.k1;

        std::span<const uint8_t> assoc = !ad.empty() ? ad : std::span<const uint8_t>(ctx.ad);
        if (!assoc.empty()) {
            size_t adLen = assoc.size();
            size_t adBlocks = adLen / RATE;
            for (size_t i = 0; i < adBlocks; ++i) {
                ctx.x0 ^= AsconPermutation::load64_be(assoc.data() + i * RATE);
                AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
            }
            uint8_t pad[8] = { 0 };
            size_t rem = adLen % RATE;
            std::memcpy(pad, assoc.data() + adBlocks * RATE, rem);
            pad[rem] = 0x80;
            ctx.x0 ^= AsconPermutation::load64_be(pad);
            AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }
        ctx.x4 ^= 1ULL;

        size_t ptLen = plaintext.size();
        size_t ptBlocks = ptLen / RATE;
        for (size_t i = 0; i < ptBlocks; ++i) {
            ctx.x0 ^= AsconPermutation::load64_be(plaintext.data() + i * RATE);
            AsconPermutation::store64_be(outCiphertext.data() + i * RATE, ctx.x0);
            AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }
        uint8_t padPt[8] = { 0 };
        size_t remPt = ptLen % RATE;
        if (remPt > 0) {
            std::memcpy(padPt, plaintext.data() + ptBlocks * RATE, remPt);
        }
        padPt[remPt] = 0x80;
        ctx.x0 ^= AsconPermutation::load64_be(padPt);
        uint8_t lastCtBuf[8];
        AsconPermutation::store64_be(lastCtBuf, ctx.x0);
        if (remPt > 0) {
            std::memcpy(outCiphertext.data() + ptBlocks * RATE, lastCtBuf, remPt);
        }

        ctx.x1 ^= ctx.k0;
        ctx.x2 ^= ctx.k1;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x3 ^= ctx.k0;
        ctx.x4 ^= ctx.k1;

        AsconPermutation::store64_be(outTag.data(), ctx.x3);
        AsconPermutation::store64_be(outTag.data() + 8, ctx.x4);

        ctx.bytesProcessed += ptLen;
        return true;
    }

    static bool decrypt(
        Context& ctx,
        std::span<const uint8_t> ciphertext,
        std::span<const uint8_t> tag,
        std::span<uint8_t> outPlaintext,
        std::span<const uint8_t> ad = {},
        std::span<const uint8_t> nonce = {}) noexcept {
        
        if (!ctx.initialized || ctx.key.size() < KEY_SIZE) return false;
        if (tag.size() < TAG_SIZE || outPlaintext.size() < ciphertext.size()) return false;

        std::span<const uint8_t> n = !nonce.empty() ? nonce : std::span<const uint8_t>(ctx.nonce);
        if (n.size() < NONCE_SIZE) return false;

        ctx.k0 = AsconPermutation::load64_be(ctx.key.data());
        ctx.k1 = AsconPermutation::load64_be(ctx.key.data() + 8);
        uint64_t n0 = AsconPermutation::load64_be(n.data());
        uint64_t n1 = AsconPermutation::load64_be(n.data() + 8);

        ctx.x0 = ASCON128_IV;
        ctx.x1 = ctx.k0;
        ctx.x2 = ctx.k1;
        ctx.x3 = n0;
        ctx.x4 = n1;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x3 ^= ctx.k0;
        ctx.x4 ^= ctx.k1;

        std::span<const uint8_t> assoc = !ad.empty() ? ad : std::span<const uint8_t>(ctx.ad);
        if (!assoc.empty()) {
            size_t adLen = assoc.size();
            size_t adBlocks = adLen / RATE;
            for (size_t i = 0; i < adBlocks; ++i) {
                ctx.x0 ^= AsconPermutation::load64_be(assoc.data() + i * RATE);
                AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
            }
            uint8_t pad[8] = { 0 };
            size_t rem = adLen % RATE;
            std::memcpy(pad, assoc.data() + adBlocks * RATE, rem);
            pad[rem] = 0x80;
            ctx.x0 ^= AsconPermutation::load64_be(pad);
            AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }
        ctx.x4 ^= 1ULL;

        size_t ctLen = ciphertext.size();
        size_t ctBlocks = ctLen / RATE;
        for (size_t i = 0; i < ctBlocks; ++i) {
            uint64_t c = AsconPermutation::load64_be(ciphertext.data() + i * RATE);
            AsconPermutation::store64_be(outPlaintext.data() + i * RATE, ctx.x0 ^ c);
            ctx.x0 = c;
            AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }

        size_t rem = ctLen % RATE;
        uint8_t lastPt[8] = { 0 };
        uint8_t prevX0[8];
        AsconPermutation::store64_be(prevX0, ctx.x0);
        for (size_t i = 0; i < rem; ++i) {
            lastPt[i] = prevX0[i] ^ ciphertext[ctBlocks * RATE + i];
        }
        if (rem > 0) {
            std::memcpy(outPlaintext.data() + ctBlocks * RATE, lastPt, rem);
        }
        lastPt[rem] = 0x80;
        ctx.x0 ^= AsconPermutation::load64_be(lastPt);

        ctx.x1 ^= ctx.k0;
        ctx.x2 ^= ctx.k1;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x3 ^= ctx.k0;
        ctx.x4 ^= ctx.k1;

        uint8_t expectedTag[16];
        AsconPermutation::store64_be(expectedTag, ctx.x3);
        AsconPermutation::store64_be(expectedTag + 8, ctx.x4);

        uint8_t diff = 0;
        for (size_t i = 0; i < TAG_SIZE; ++i) {
            diff |= (tag[i] ^ expectedTag[i]);
        }
        if (diff != 0) return false;

        ctx.bytesProcessed += ctLen;
        return true;
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || out.empty()) return;
        if (!ctx.buffer.empty()) {
            std::vector<uint8_t> ct(ctx.buffer.size());
            std::vector<uint8_t> tag(TAG_SIZE);
            encrypt(ctx, ctx.buffer, ct, tag);
            size_t copyLen = std::min(out.size(), ct.size());
            std::memcpy(out.data(), ct.data(), copyLen);
            ctx.buffer.clear();
        }
    }
};

class AsconHash {
public:
    static constexpr size_t DIGEST_SIZE = 32;
    static constexpr size_t RATE = 8;
    static constexpr uint64_t ASCON_HASH_IV = 0x00400c0000000100ULL;

    struct Context {
        uint64_t x0{ 0 }, x1{ 0 }, x2{ 0 }, x3{ 0 }, x4{ 0 };
        std::vector<uint8_t> buffer{};
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx) noexcept {
        ctx.x0 = ASCON_HASH_IV;
        ctx.x1 = 0;
        ctx.x2 = 0;
        ctx.x3 = 0;
        ctx.x4 = 0;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.buffer.clear();
        ctx.bytesProcessed = 0;
        ctx.initialized = true;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void final(Context& ctx, std::span<uint8_t> outDigest) noexcept {
        if (!ctx.initialized || outDigest.empty()) return;
        
        size_t len = ctx.buffer.size();
        size_t blocks = len / RATE;
        for (size_t i = 0; i < blocks; ++i) {
            ctx.x0 ^= AsconPermutation::load64_be(ctx.buffer.data() + i * RATE);
            AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }

        uint8_t pad[8] = { 0 };
        size_t rem = len % RATE;
        if (rem > 0) {
            std::memcpy(pad, ctx.buffer.data() + blocks * RATE, rem);
        }
        pad[rem] = 0x80;
        ctx.x0 ^= AsconPermutation::load64_be(pad);
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);

        uint8_t digest[32];
        AsconPermutation::store64_be(digest, ctx.x0);
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        AsconPermutation::store64_be(digest + 8, ctx.x0);
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        AsconPermutation::store64_be(digest + 16, ctx.x0);
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        AsconPermutation::store64_be(digest + 24, ctx.x0);

        size_t copyLen = std::min(outDigest.size(), static_cast<size_t>(32));
        std::memcpy(outDigest.data(), digest, copyLen);
        ctx.buffer.clear();
    }

    static std::array<uint8_t, 32> hash(std::span<const uint8_t> data) noexcept {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::array<uint8_t, 32> out{};
        final(ctx, std::span<uint8_t>(out.data(), out.size()));
        return out;
    }
};

class Ascon80pq {
public:
    static constexpr size_t KEY_SIZE = 20; // 160 bits
    static constexpr size_t NONCE_SIZE = 16;
    static constexpr size_t TAG_SIZE = 16;
    static constexpr size_t RATE = 8;
    static constexpr uint64_t ASCON80PQ_IV = 0xa0400c0600000000ULL;

    struct Context {
        uint64_t x0{ 0 }, x1{ 0 }, x2{ 0 }, x3{ 0 }, x4{ 0 };
        uint64_t k0{ 0 }, k1{ 0 }, k2{ 0 };
        std::vector<uint8_t> key{};
        std::vector<uint8_t> nonce{};
        std::vector<uint8_t> ad{};
        std::vector<uint8_t> buffer{};
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::span<const uint8_t> nonce = {}) noexcept {
        ctx.key.assign(key.begin(), key.end());
        if (!nonce.empty()) {
            ctx.nonce.assign(nonce.begin(), nonce.end());
        }
        ctx.buffer.clear();
        ctx.ad.clear();
        ctx.bytesProcessed = 0;
        ctx.initialized = (key.size() >= KEY_SIZE);
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static bool encrypt(
        Context& ctx,
        std::span<const uint8_t> plaintext,
        std::span<uint8_t> outCiphertext,
        std::span<uint8_t> outTag,
        std::span<const uint8_t> ad = {},
        std::span<const uint8_t> nonce = {}) noexcept {
        
        if (!ctx.initialized || ctx.key.size() < KEY_SIZE) return false;
        if (outCiphertext.size() < plaintext.size() || outTag.size() < TAG_SIZE) return false;

        std::span<const uint8_t> n = !nonce.empty() ? nonce : std::span<const uint8_t>(ctx.nonce);
        if (n.size() < NONCE_SIZE) return false;

        ctx.k0 = (static_cast<uint64_t>(ctx.key[0]) << 24) |
                 (static_cast<uint64_t>(ctx.key[1]) << 16) |
                 (static_cast<uint64_t>(ctx.key[2]) << 8)  |
                 (static_cast<uint64_t>(ctx.key[3]));
        ctx.k1 = AsconPermutation::load64_be(ctx.key.data() + 4);
        ctx.k2 = AsconPermutation::load64_be(ctx.key.data() + 12);
        uint64_t n0 = AsconPermutation::load64_be(n.data());
        uint64_t n1 = AsconPermutation::load64_be(n.data() + 8);

        ctx.x0 = ASCON80PQ_IV | ctx.k0;
        ctx.x1 = ctx.k1;
        ctx.x2 = ctx.k2;
        ctx.x3 = n0;
        ctx.x4 = n1;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x2 ^= ctx.k0;
        ctx.x3 ^= ctx.k1;
        ctx.x4 ^= ctx.k2;

        std::span<const uint8_t> assoc = !ad.empty() ? ad : std::span<const uint8_t>(ctx.ad);
        if (!assoc.empty()) {
            size_t adLen = assoc.size();
            size_t adBlocks = adLen / RATE;
            for (size_t i = 0; i < adBlocks; ++i) {
                ctx.x0 ^= AsconPermutation::load64_be(assoc.data() + i * RATE);
                AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
            }
            uint8_t pad[8] = { 0 };
            size_t rem = adLen % RATE;
            std::memcpy(pad, assoc.data() + adBlocks * RATE, rem);
            pad[rem] = 0x80;
            ctx.x0 ^= AsconPermutation::load64_be(pad);
            AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }
        ctx.x4 ^= 1ULL;

        size_t ptLen = plaintext.size();
        size_t ptBlocks = ptLen / RATE;
        for (size_t i = 0; i < ptBlocks; ++i) {
            ctx.x0 ^= AsconPermutation::load64_be(plaintext.data() + i * RATE);
            AsconPermutation::store64_be(outCiphertext.data() + i * RATE, ctx.x0);
            AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }

        size_t rem = ptLen % RATE;
        uint8_t padPt[8] = { 0 };
        if (rem > 0) {
            std::memcpy(padPt, plaintext.data() + ptBlocks * RATE, rem);
        }
        padPt[rem] = 0x80;
        ctx.x0 ^= AsconPermutation::load64_be(padPt);
        uint8_t lastCtBuf[8];
        AsconPermutation::store64_be(lastCtBuf, ctx.x0);
        if (rem > 0) {
            std::memcpy(outCiphertext.data() + ptBlocks * RATE, lastCtBuf, rem);
        }

        ctx.x1 ^= ctx.k0;
        ctx.x2 ^= ctx.k1;
        ctx.x3 ^= ctx.k2;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x3 ^= ctx.k1;
        ctx.x4 ^= ctx.k2;

        AsconPermutation::store64_be(outTag.data(), ctx.x3);
        AsconPermutation::store64_be(outTag.data() + 8, ctx.x4);

        ctx.bytesProcessed += ptLen;
        return true;
    }

    static bool decrypt(
        Context& ctx,
        std::span<const uint8_t> ciphertext,
        std::span<const uint8_t> tag,
        std::span<uint8_t> outPlaintext,
        std::span<const uint8_t> ad = {},
        std::span<const uint8_t> nonce = {}) noexcept {
        
        if (!ctx.initialized || ctx.key.size() < KEY_SIZE) return false;
        if (tag.size() < TAG_SIZE || outPlaintext.size() < ciphertext.size()) return false;

        std::span<const uint8_t> n = !nonce.empty() ? nonce : std::span<const uint8_t>(ctx.nonce);
        if (n.size() < NONCE_SIZE) return false;

        ctx.k0 = (static_cast<uint64_t>(ctx.key[0]) << 24) |
                 (static_cast<uint64_t>(ctx.key[1]) << 16) |
                 (static_cast<uint64_t>(ctx.key[2]) << 8)  |
                 (static_cast<uint64_t>(ctx.key[3]));
        ctx.k1 = AsconPermutation::load64_be(ctx.key.data() + 4);
        ctx.k2 = AsconPermutation::load64_be(ctx.key.data() + 12);
        uint64_t n0 = AsconPermutation::load64_be(n.data());
        uint64_t n1 = AsconPermutation::load64_be(n.data() + 8);

        ctx.x0 = ASCON80PQ_IV | ctx.k0;
        ctx.x1 = ctx.k1;
        ctx.x2 = ctx.k2;
        ctx.x3 = n0;
        ctx.x4 = n1;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x2 ^= ctx.k0;
        ctx.x3 ^= ctx.k1;
        ctx.x4 ^= ctx.k2;

        std::span<const uint8_t> assoc = !ad.empty() ? ad : std::span<const uint8_t>(ctx.ad);
        if (!assoc.empty()) {
            size_t adLen = assoc.size();
            size_t adBlocks = adLen / RATE;
            for (size_t i = 0; i < adBlocks; ++i) {
                ctx.x0 ^= AsconPermutation::load64_be(assoc.data() + i * RATE);
                AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
            }
            uint8_t pad[8] = { 0 };
            size_t rem = adLen % RATE;
            std::memcpy(pad, assoc.data() + adBlocks * RATE, rem);
            pad[rem] = 0x80;
            ctx.x0 ^= AsconPermutation::load64_be(pad);
            AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }
        ctx.x4 ^= 1ULL;

        size_t ctLen = ciphertext.size();
        size_t ctBlocks = ctLen / RATE;
        for (size_t i = 0; i < ctBlocks; ++i) {
            uint64_t c = AsconPermutation::load64_be(ciphertext.data() + i * RATE);
            AsconPermutation::store64_be(outPlaintext.data() + i * RATE, ctx.x0 ^ c);
            ctx.x0 = c;
            AsconPermutation::p6(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }

        size_t rem = ctLen % RATE;
        uint8_t lastPt[8] = { 0 };
        uint8_t prevX0[8];
        AsconPermutation::store64_be(prevX0, ctx.x0);
        for (size_t i = 0; i < rem; ++i) {
            lastPt[i] = prevX0[i] ^ ciphertext[ctBlocks * RATE + i];
        }
        if (rem > 0) {
            std::memcpy(outPlaintext.data() + ctBlocks * RATE, lastPt, rem);
        }
        lastPt[rem] = 0x80;
        ctx.x0 ^= AsconPermutation::load64_be(lastPt);

        ctx.x1 ^= ctx.k0;
        ctx.x2 ^= ctx.k1;
        ctx.x3 ^= ctx.k2;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x3 ^= ctx.k1;
        ctx.x4 ^= ctx.k2;

        uint8_t expectedTag[16];
        AsconPermutation::store64_be(expectedTag, ctx.x3);
        AsconPermutation::store64_be(expectedTag + 8, ctx.x4);

        uint8_t diff = 0;
        for (size_t i = 0; i < TAG_SIZE; ++i) {
            diff |= (tag[i] ^ expectedTag[i]);
        }
        if (diff != 0) return false;

        ctx.bytesProcessed += ctLen;
        return true;
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || out.empty()) return;
        if (!ctx.buffer.empty()) {
            std::vector<uint8_t> ct(ctx.buffer.size());
            std::vector<uint8_t> tag(TAG_SIZE);
            encrypt(ctx, ctx.buffer, ct, tag);
            size_t copyLen = std::min(out.size(), ct.size());
            std::memcpy(out.data(), ct.data(), copyLen);
            ctx.buffer.clear();
        }
    }
};

class AsconXof {
public:
    static constexpr size_t DEFAULT_DIGEST_SIZE = 32;
    static constexpr size_t RATE = 8;
    static constexpr uint64_t ASCON_XOF_IV = 0x00400c0000000000ULL;

    struct Context {
        uint64_t x0{ 0 }, x1{ 0 }, x2{ 0 }, x3{ 0 }, x4{ 0 };
        std::vector<uint8_t> buffer{};
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
        bool squeezed{ false };
    };

    static void init(Context& ctx) noexcept {
        ctx.x0 = ASCON_XOF_IV;
        ctx.x1 = 0;
        ctx.x2 = 0;
        ctx.x3 = 0;
        ctx.x4 = 0;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.buffer.clear();
        ctx.bytesProcessed = 0;
        ctx.initialized = true;
        ctx.squeezed = false;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void padAndPermute(Context& ctx) noexcept {
        if (ctx.squeezed) return;
        size_t len = ctx.buffer.size();
        size_t blocks = len / RATE;
        for (size_t i = 0; i < blocks; ++i) {
            ctx.x0 ^= AsconPermutation::load64_be(ctx.buffer.data() + i * RATE);
            AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }

        uint8_t pad[8] = { 0 };
        size_t rem = len % RATE;
        if (rem > 0) {
            std::memcpy(pad, ctx.buffer.data() + blocks * RATE, rem);
        }
        pad[rem] = 0x80;
        ctx.x0 ^= AsconPermutation::load64_be(pad);
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.buffer.clear();
        ctx.squeezed = true;
    }

    static void squeeze(Context& ctx, std::span<uint8_t> outDigest) noexcept {
        if (!ctx.initialized || outDigest.empty()) return;
        padAndPermute(ctx);
        size_t outLen = outDigest.size();
        size_t offset = 0;
        while (offset < outLen) {
            uint8_t block[8];
            AsconPermutation::store64_be(block, ctx.x0);
            size_t take = std::min(outLen - offset, static_cast<size_t>(8));
            std::memcpy(outDigest.data() + offset, block, take);
            offset += take;
            if (offset < outLen) {
                AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
            }
        }
    }

    static void final(Context& ctx, std::span<uint8_t> outDigest) noexcept {
        squeeze(ctx, outDigest);
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data, size_t outLen = DEFAULT_DIGEST_SIZE) noexcept {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(outLen);
        squeeze(ctx, out);
        return out;
    }
};

class Ascon128a {
public:
    static constexpr size_t KEY_SIZE = 16;
    static constexpr size_t NONCE_SIZE = 16;
    static constexpr size_t TAG_SIZE = 16;
    static constexpr size_t RATE = 16;
    static constexpr uint64_t ASCON128A_IV = 0x80800c0800000000ULL;

    struct Context {
        uint64_t x0{ 0 }, x1{ 0 }, x2{ 0 }, x3{ 0 }, x4{ 0 };
        uint64_t k0{ 0 }, k1{ 0 };
        std::vector<uint8_t> key{};
        std::vector<uint8_t> nonce{};
        std::vector<uint8_t> ad{};
        std::vector<uint8_t> buffer{};
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
    };

    static void init(Context& ctx, std::span<const uint8_t> key, std::span<const uint8_t> nonce = {}) noexcept {
        ctx.key.assign(key.begin(), key.end());
        if (!nonce.empty()) {
            ctx.nonce.assign(nonce.begin(), nonce.end());
        }
        ctx.buffer.clear();
        ctx.ad.clear();
        ctx.bytesProcessed = 0;
        ctx.initialized = (key.size() >= KEY_SIZE);
    }

    static void setAssociatedData(Context& ctx, std::span<const uint8_t> ad) noexcept {
        ctx.ad.assign(ad.begin(), ad.end());
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static bool encrypt(
        Context& ctx,
        std::span<const uint8_t> plaintext,
        std::span<uint8_t> outCiphertext,
        std::span<uint8_t> outTag,
        std::span<const uint8_t> ad = {},
        std::span<const uint8_t> nonce = {}) noexcept {
        
        if (!ctx.initialized || ctx.key.size() < KEY_SIZE) return false;
        if (outCiphertext.size() < plaintext.size() || outTag.size() < TAG_SIZE) return false;

        std::span<const uint8_t> n = !nonce.empty() ? nonce : std::span<const uint8_t>(ctx.nonce);
        if (n.size() < NONCE_SIZE) return false;

        ctx.k0 = AsconPermutation::load64_be(ctx.key.data());
        ctx.k1 = AsconPermutation::load64_be(ctx.key.data() + 8);
        uint64_t n0 = AsconPermutation::load64_be(n.data());
        uint64_t n1 = AsconPermutation::load64_be(n.data() + 8);

        ctx.x0 = ASCON128A_IV;
        ctx.x1 = ctx.k0;
        ctx.x2 = ctx.k1;
        ctx.x3 = n0;
        ctx.x4 = n1;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x3 ^= ctx.k0;
        ctx.x4 ^= ctx.k1;

        std::span<const uint8_t> assoc = !ad.empty() ? ad : std::span<const uint8_t>(ctx.ad);
        if (!assoc.empty()) {
            size_t adLen = assoc.size();
            size_t adBlocks = adLen / RATE;
            for (size_t i = 0; i < adBlocks; ++i) {
                ctx.x0 ^= AsconPermutation::load64_be(assoc.data() + i * RATE);
                ctx.x1 ^= AsconPermutation::load64_be(assoc.data() + i * RATE + 8);
                AsconPermutation::p8(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
            }
            uint8_t pad[16] = { 0 };
            size_t rem = adLen % RATE;
            std::memcpy(pad, assoc.data() + adBlocks * RATE, rem);
            pad[rem] = 0x80;
            ctx.x0 ^= AsconPermutation::load64_be(pad);
            ctx.x1 ^= AsconPermutation::load64_be(pad + 8);
            AsconPermutation::p8(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }
        ctx.x4 ^= 1ULL;

        size_t ptLen = plaintext.size();
        size_t ptBlocks = ptLen / RATE;
        for (size_t i = 0; i < ptBlocks; ++i) {
            ctx.x0 ^= AsconPermutation::load64_be(plaintext.data() + i * RATE);
            ctx.x1 ^= AsconPermutation::load64_be(plaintext.data() + i * RATE + 8);
            AsconPermutation::store64_be(outCiphertext.data() + i * RATE, ctx.x0);
            AsconPermutation::store64_be(outCiphertext.data() + i * RATE + 8, ctx.x1);
            AsconPermutation::p8(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }

        size_t rem = ptLen % RATE;
        uint8_t padPt[16] = { 0 };
        if (rem > 0) {
            std::memcpy(padPt, plaintext.data() + ptBlocks * RATE, rem);
        }
        padPt[rem] = 0x80;
        ctx.x0 ^= AsconPermutation::load64_be(padPt);
        ctx.x1 ^= AsconPermutation::load64_be(padPt + 8);
        uint8_t lastCtBuf[16];
        AsconPermutation::store64_be(lastCtBuf, ctx.x0);
        AsconPermutation::store64_be(lastCtBuf + 8, ctx.x1);
        if (rem > 0) {
            std::memcpy(outCiphertext.data() + ptBlocks * RATE, lastCtBuf, rem);
        }

        ctx.x2 ^= ctx.k0;
        ctx.x3 ^= ctx.k1;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x3 ^= ctx.k0;
        ctx.x4 ^= ctx.k1;

        AsconPermutation::store64_be(outTag.data(), ctx.x3);
        AsconPermutation::store64_be(outTag.data() + 8, ctx.x4);

        ctx.bytesProcessed += ptLen;
        return true;
    }

    static bool decrypt(
        Context& ctx,
        std::span<const uint8_t> ciphertext,
        std::span<const uint8_t> tag,
        std::span<uint8_t> outPlaintext,
        std::span<const uint8_t> ad = {},
        std::span<const uint8_t> nonce = {}) noexcept {
        
        if (!ctx.initialized || ctx.key.size() < KEY_SIZE) return false;
        if (outPlaintext.size() < ciphertext.size() || tag.size() < TAG_SIZE) return false;

        std::span<const uint8_t> n = !nonce.empty() ? nonce : std::span<const uint8_t>(ctx.nonce);
        if (n.size() < NONCE_SIZE) return false;

        ctx.k0 = AsconPermutation::load64_be(ctx.key.data());
        ctx.k1 = AsconPermutation::load64_be(ctx.key.data() + 8);
        uint64_t n0 = AsconPermutation::load64_be(n.data());
        uint64_t n1 = AsconPermutation::load64_be(n.data() + 8);

        ctx.x0 = ASCON128A_IV;
        ctx.x1 = ctx.k0;
        ctx.x2 = ctx.k1;
        ctx.x3 = n0;
        ctx.x4 = n1;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x3 ^= ctx.k0;
        ctx.x4 ^= ctx.k1;

        std::span<const uint8_t> assoc = !ad.empty() ? ad : std::span<const uint8_t>(ctx.ad);
        if (!assoc.empty()) {
            size_t adLen = assoc.size();
            size_t adBlocks = adLen / RATE;
            for (size_t i = 0; i < adBlocks; ++i) {
                ctx.x0 ^= AsconPermutation::load64_be(assoc.data() + i * RATE);
                ctx.x1 ^= AsconPermutation::load64_be(assoc.data() + i * RATE + 8);
                AsconPermutation::p8(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
            }
            uint8_t pad[16] = { 0 };
            size_t rem = adLen % RATE;
            std::memcpy(pad, assoc.data() + adBlocks * RATE, rem);
            pad[rem] = 0x80;
            ctx.x0 ^= AsconPermutation::load64_be(pad);
            ctx.x1 ^= AsconPermutation::load64_be(pad + 8);
            AsconPermutation::p8(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }
        ctx.x4 ^= 1ULL;

        size_t ctLen = ciphertext.size();
        size_t ctBlocks = ctLen / RATE;
        for (size_t i = 0; i < ctBlocks; ++i) {
            uint64_t c0 = AsconPermutation::load64_be(ciphertext.data() + i * RATE);
            uint64_t c1 = AsconPermutation::load64_be(ciphertext.data() + i * RATE + 8);
            AsconPermutation::store64_be(outPlaintext.data() + i * RATE, ctx.x0 ^ c0);
            AsconPermutation::store64_be(outPlaintext.data() + i * RATE + 8, ctx.x1 ^ c1);
            ctx.x0 = c0;
            ctx.x1 = c1;
            AsconPermutation::p8(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }

        size_t rem = ctLen % RATE;
        uint8_t lastPt[16] = { 0 };
        uint8_t prevX[16];
        AsconPermutation::store64_be(prevX, ctx.x0);
        AsconPermutation::store64_be(prevX + 8, ctx.x1);
        for (size_t i = 0; i < rem; ++i) {
            lastPt[i] = prevX[i] ^ ciphertext[ctBlocks * RATE + i];
        }
        if (rem > 0) {
            std::memcpy(outPlaintext.data() + ctBlocks * RATE, lastPt, rem);
        }
        lastPt[rem] = 0x80;
        ctx.x0 ^= AsconPermutation::load64_be(lastPt);
        ctx.x1 ^= AsconPermutation::load64_be(lastPt + 8);

        ctx.x2 ^= ctx.k0;
        ctx.x3 ^= ctx.k1;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.x3 ^= ctx.k0;
        ctx.x4 ^= ctx.k1;

        uint8_t expectedTag[16];
        AsconPermutation::store64_be(expectedTag, ctx.x3);
        AsconPermutation::store64_be(expectedTag + 8, ctx.x4);

        uint8_t diff = 0;
        for (size_t i = 0; i < TAG_SIZE; ++i) {
            diff |= (tag[i] ^ expectedTag[i]);
        }
        if (diff != 0) return false;

        ctx.bytesProcessed += ctLen;
        return true;
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.initialized || out.empty()) return;
        if (!ctx.buffer.empty()) {
            std::vector<uint8_t> ct(ctx.buffer.size());
            std::vector<uint8_t> tag(TAG_SIZE);
            encrypt(ctx, ctx.buffer, ct, tag);
            size_t copyLen = std::min(out.size(), ct.size());
            std::memcpy(out.data(), ct.data(), copyLen);
            ctx.buffer.clear();
        }
    }
};

class AsconXofa {
public:
    static constexpr size_t DEFAULT_DIGEST_SIZE = 32;
    static constexpr size_t RATE = 8;
    static constexpr uint64_t ASCON_XOFA_IV = 0x00400c0800000000ULL;

    struct Context {
        uint64_t x0{ 0 }, x1{ 0 }, x2{ 0 }, x3{ 0 }, x4{ 0 };
        std::vector<uint8_t> buffer{};
        uint64_t bytesProcessed{ 0 };
        bool initialized{ false };
        bool squeezed{ false };
    };

    static void init(Context& ctx) noexcept {
        ctx.x0 = ASCON_XOFA_IV;
        ctx.x1 = 0;
        ctx.x2 = 0;
        ctx.x3 = 0;
        ctx.x4 = 0;
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.buffer.clear();
        ctx.bytesProcessed = 0;
        ctx.initialized = true;
        ctx.squeezed = false;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        if (!ctx.initialized || data.empty()) return;
        ctx.buffer.insert(ctx.buffer.end(), data.begin(), data.end());
    }

    static void padAndPermute(Context& ctx) noexcept {
        if (ctx.squeezed) return;
        size_t len = ctx.buffer.size();
        size_t blocks = len / RATE;
        for (size_t i = 0; i < blocks; ++i) {
            ctx.x0 ^= AsconPermutation::load64_be(ctx.buffer.data() + i * RATE);
            AsconPermutation::p8(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        }

        uint8_t pad[8] = { 0 };
        size_t rem = len % RATE;
        if (rem > 0) {
            std::memcpy(pad, ctx.buffer.data() + blocks * RATE, rem);
        }
        pad[rem] = 0x80;
        ctx.x0 ^= AsconPermutation::load64_be(pad);
        AsconPermutation::p12(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
        ctx.buffer.clear();
        ctx.squeezed = true;
    }

    static void squeeze(Context& ctx, std::span<uint8_t> outDigest) noexcept {
        if (!ctx.initialized || outDigest.empty()) return;
        padAndPermute(ctx);
        size_t outLen = outDigest.size();
        size_t offset = 0;
        while (offset < outLen) {
            uint8_t block[8];
            AsconPermutation::store64_be(block, ctx.x0);
            size_t take = std::min(outLen - offset, static_cast<size_t>(8));
            std::memcpy(outDigest.data() + offset, block, take);
            offset += take;
            if (offset < outLen) {
                AsconPermutation::p8(ctx.x0, ctx.x1, ctx.x2, ctx.x3, ctx.x4);
            }
        }
    }

    static void final(Context& ctx, std::span<uint8_t> outDigest) noexcept {
        squeeze(ctx, outDigest);
    }

    static std::vector<uint8_t> hash(std::span<const uint8_t> data, size_t outLen = DEFAULT_DIGEST_SIZE) noexcept {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::vector<uint8_t> out(outLen);
        squeeze(ctx, out);
        return out;
    }
};

template<typename CShakeType>
class GenericTupleHash {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename CShakeType::Context cshake{};
        std::string custom{};
        size_t      outLen{ DEFAULT_DIGEST_SIZE };
        bool        finalized{ false };
    };

    static void init(Context& ctx, std::string_view custom = "", size_t outLen = DEFAULT_DIGEST_SIZE) noexcept {
        ctx.custom = std::string(custom);
        ctx.outLen = outLen;
        ctx.finalized = false;
        CShakeType::init(ctx.cshake, "TupleHash", custom);
    }

    static void add_tuple_item(Context& ctx, std::span<const uint8_t> item) noexcept {
        if (!ctx.finalized) {
            auto enc = NistSp800_185::encode_string(item);
            CShakeType::update(ctx.cshake, enc);
        }
    }

    static void add_tuple_item(Context& ctx, std::string_view item) noexcept {
        add_tuple_item(ctx, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(item.data()), item.size()));
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        add_tuple_item(ctx, data);
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.finalized) {
            auto encLen = NistSp800_185::right_encode(out.size() * 8ULL);
            CShakeType::update(ctx.cshake, encLen);
            ctx.finalized = true;
        }
        CShakeType::squeeze(ctx.cshake, out);
    }

    static std::vector<uint8_t> hash(const std::vector<std::vector<uint8_t>>& tuple, std::string_view custom = "", size_t outLen = DEFAULT_DIGEST_SIZE) {
        Context ctx;
        init(ctx, custom, outLen);
        for (const auto& item : tuple) {
            add_tuple_item(ctx, item);
        }
        std::vector<uint8_t> out(outLen);
        final(ctx, out);
        return out;
    }
};

using TupleHash128 = GenericTupleHash<CShake128>;
using TupleHash256 = GenericTupleHash<CShake256>;

template<typename CShakeType>
class GenericTupleHashXof {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;

    struct Context {
        typename CShakeType::Context cshake{};
        std::string custom{};
        bool        finalized{ false };
    };

    static void init(Context& ctx, std::string_view custom = "") noexcept {
        ctx.custom = std::string(custom);
        ctx.finalized = false;
        CShakeType::init(ctx.cshake, "TupleHash", custom);
    }

    static void add_tuple_item(Context& ctx, std::span<const uint8_t> item) noexcept {
        if (!ctx.finalized) {
            auto enc = NistSp800_185::encode_string(item);
            CShakeType::update(ctx.cshake, enc);
        }
    }

    static void add_tuple_item(Context& ctx, std::string_view item) noexcept {
        add_tuple_item(ctx, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(item.data()), item.size()));
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        add_tuple_item(ctx, data);
    }

    static void final(Context& ctx, std::span<uint8_t> out) noexcept {
        if (!ctx.finalized) {
            auto encLen = NistSp800_185::right_encode(0); // L = 0 for TupleHashXOF
            CShakeType::update(ctx.cshake, encLen);
            ctx.finalized = true;
        }
        CShakeType::squeeze(ctx.cshake, out);
    }

    static std::vector<uint8_t> hash(const std::vector<std::vector<uint8_t>>& tuple, std::string_view custom = "", size_t outLen = DEFAULT_DIGEST_SIZE) {
        Context ctx;
        init(ctx, custom);
        for (const auto& item : tuple) {
            add_tuple_item(ctx, item);
        }
        std::vector<uint8_t> out(outLen);
        final(ctx, out);
        return out;
    }
};

using TupleHashXOF128 = GenericTupleHashXof<CShake128>;
using TupleHashXOF256 = GenericTupleHashXof<CShake256>;

template<typename CShakeType>
class GenericParallelHash {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;
    static constexpr size_t DEFAULT_BLOCK_SIZE = 8192; // 8 KiB leaf block size default

    struct Context {
        size_t               blockSize{ DEFAULT_BLOCK_SIZE };
        size_t               outLen{ DEFAULT_DIGEST_SIZE };
        std::string          custom{};
        std::vector<uint8_t> buffer{};
        std::vector<uint8_t> leafDigests{};
        size_t               numBlocks{ 0 };
        bool                 finalized{ false };
    };

    static void init(Context& ctx, size_t blockSize = DEFAULT_BLOCK_SIZE, std::string_view custom = "", size_t outLen = DEFAULT_DIGEST_SIZE) noexcept {
        ctx.blockSize = (blockSize == 0) ? DEFAULT_BLOCK_SIZE : blockSize;
        ctx.outLen = (outLen == 0) ? DEFAULT_DIGEST_SIZE : outLen;
        ctx.custom = std::string(custom);
        ctx.buffer.clear();
        ctx.leafDigests.clear();
        ctx.numBlocks = 0;
        ctx.finalized = false;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) {
        if (ctx.finalized || data.empty()) return;

        size_t offset = 0;
        size_t remaining = data.size();

        while (remaining > 0) {
            size_t needed = ctx.blockSize - ctx.buffer.size();
            size_t toCopy = std::min(needed, remaining);
            ctx.buffer.insert(ctx.buffer.end(), data.data() + offset, data.data() + offset + toCopy);
            offset += toCopy;
            remaining -= toCopy;

            if (ctx.buffer.size() == ctx.blockSize) {
                auto leaf = CShakeType::hash(ctx.buffer, "", "", CShakeType::DEFAULT_DIGEST_SIZE);
                ctx.leafDigests.insert(ctx.leafDigests.end(), leaf.begin(), leaf.end());
                ctx.numBlocks++;
                ctx.buffer.clear();
            }
        }
    }

    static void final(Context& ctx, std::span<uint8_t> out) {
        if (!ctx.finalized) {
            if (!ctx.buffer.empty()) {
                auto leaf = CShakeType::hash(ctx.buffer, "", "", CShakeType::DEFAULT_DIGEST_SIZE);
                ctx.leafDigests.insert(ctx.leafDigests.end(), leaf.begin(), leaf.end());
                ctx.numBlocks++;
                ctx.buffer.clear();
            }
            ctx.finalized = true;
        }

        std::vector<uint8_t> z = NistSp800_185::left_encode(ctx.blockSize);
        z.insert(z.end(), ctx.leafDigests.begin(), ctx.leafDigests.end());
        auto encN = NistSp800_185::right_encode(ctx.numBlocks);
        z.insert(z.end(), encN.begin(), encN.end());
        auto encL = NistSp800_185::right_encode(out.size() * 8ULL);
        z.insert(z.end(), encL.begin(), encL.end());

        typename CShakeType::Context rootCtx;
        CShakeType::init(rootCtx, "ParallelHash", ctx.custom);
        CShakeType::update(rootCtx, z);
        CShakeType::squeeze(rootCtx, out);
    }

    static std::vector<uint8_t> hash(
        std::span<const uint8_t> data,
        size_t blockSize = DEFAULT_BLOCK_SIZE,
        std::string_view custom = "",
        size_t outLen = DEFAULT_DIGEST_SIZE) {
        Context ctx;
        init(ctx, blockSize, custom, outLen);
        update(ctx, data);
        std::vector<uint8_t> out(outLen);
        final(ctx, out);
        return out;
    }
};

using ParallelHash128 = GenericParallelHash<CShake128>;
using ParallelHash256 = GenericParallelHash<CShake256>;

template<typename CShakeType>
class GenericParallelHashXof {
public:
    static constexpr size_t RATE = CShakeType::RATE;
    static constexpr size_t DEFAULT_DIGEST_SIZE = CShakeType::DEFAULT_DIGEST_SIZE;
    static constexpr size_t DEFAULT_BLOCK_SIZE = 8192; // 8 KiB leaf block size default

    struct Context {
        size_t               blockSize{ DEFAULT_BLOCK_SIZE };
        std::string          custom{};
        std::vector<uint8_t> buffer{};
        std::vector<uint8_t> leafDigests{};
        size_t               numBlocks{ 0 };
        bool                 finalized{ false };
        typename CShakeType::Context rootCtx{};
    };

    static void init(Context& ctx, size_t blockSize = DEFAULT_BLOCK_SIZE, std::string_view custom = "") noexcept {
        ctx.blockSize = (blockSize == 0) ? DEFAULT_BLOCK_SIZE : blockSize;
        ctx.custom = std::string(custom);
        ctx.buffer.clear();
        ctx.leafDigests.clear();
        ctx.numBlocks = 0;
        ctx.finalized = false;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) {
        if (ctx.finalized || data.empty()) return;

        size_t offset = 0;
        size_t remaining = data.size();

        while (remaining > 0) {
            size_t needed = ctx.blockSize - ctx.buffer.size();
            size_t toCopy = std::min(needed, remaining);
            ctx.buffer.insert(ctx.buffer.end(), data.data() + offset, data.data() + offset + toCopy);
            offset += toCopy;
            remaining -= toCopy;

            if (ctx.buffer.size() == ctx.blockSize) {
                auto leaf = CShakeType::hash(ctx.buffer, "", "", CShakeType::DEFAULT_DIGEST_SIZE);
                ctx.leafDigests.insert(ctx.leafDigests.end(), leaf.begin(), leaf.end());
                ctx.numBlocks++;
                ctx.buffer.clear();
            }
        }
    }

    static void finalize_root(Context& ctx) {
        if (!ctx.finalized) {
            if (!ctx.buffer.empty()) {
                auto leaf = CShakeType::hash(ctx.buffer, "", "", CShakeType::DEFAULT_DIGEST_SIZE);
                ctx.leafDigests.insert(ctx.leafDigests.end(), leaf.begin(), leaf.end());
                ctx.numBlocks++;
                ctx.buffer.clear();
            }
            std::vector<uint8_t> z = NistSp800_185::left_encode(ctx.blockSize);
            z.insert(z.end(), ctx.leafDigests.begin(), ctx.leafDigests.end());
            auto encN = NistSp800_185::right_encode(ctx.numBlocks);
            z.insert(z.end(), encN.begin(), encN.end());
            auto enc0 = NistSp800_185::right_encode(0ULL);
            z.insert(z.end(), enc0.begin(), enc0.end());

            CShakeType::init(ctx.rootCtx, "ParallelHash", ctx.custom);
            CShakeType::update(ctx.rootCtx, z);
            ctx.finalized = true;
        }
    }

    static void squeeze(Context& ctx, std::span<uint8_t> out) {
        finalize_root(ctx);
        CShakeType::squeeze(ctx.rootCtx, out);
    }

    static void final(Context& ctx, std::span<uint8_t> out) {
        squeeze(ctx, out);
    }

    static std::vector<uint8_t> hash(
        std::span<const uint8_t> data,
        size_t blockSize = DEFAULT_BLOCK_SIZE,
        std::string_view custom = "",
        size_t outLen = DEFAULT_DIGEST_SIZE) {
        Context ctx;
        init(ctx, blockSize, custom);
        update(ctx, data);
        std::vector<uint8_t> out(outLen);
        squeeze(ctx, out);
        return out;
    }
};

using ParallelHashXOF128 = GenericParallelHashXof<CShake128>;
using ParallelHashXOF256 = GenericParallelHashXof<CShake256>;

// ============================================================================
// 3. Clean-Room HMAC (RFC 2104 / FIPS 198-1) Engine
// ============================================================================
class HmacSha256 {
public:
    static constexpr size_t DIGEST_SIZE = 32;

    struct Context {
        Sha256::Context inner;
        Sha256::Context outer;
    };

    static void init(Context& ctx, std::span<const uint8_t> key) noexcept {
        uint8_t k[64] = {0};
        if (key.size() > 64) {
            auto keyHash = Sha256::hash(key);
            std::memcpy(k, keyHash.data(), keyHash.size());
        } else {
            std::memcpy(k, key.data(), key.size());
        }

        uint8_t ipad[64];
        uint8_t opad[64];
        for (int i = 0; i < 64; ++i) {
            ipad[i] = k[i] ^ 0x36;
            opad[i] = k[i] ^ 0x5c;
        }

        Sha256::init(ctx.inner);
        Sha256::update(ctx.inner, std::span<const uint8_t>(ipad, 64));

        Sha256::init(ctx.outer);
        Sha256::update(ctx.outer, std::span<const uint8_t>(opad, 64));
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        Sha256::update(ctx.inner, data);
    }

    static void final(Context& ctx, std::span<uint8_t, 32> digest) noexcept {
        uint8_t innerHash[32];
        Sha256::final(ctx.inner, std::span<uint8_t, 32>(innerHash, 32));

        Sha256::update(ctx.outer, std::span<const uint8_t>(innerHash, 32));
        Sha256::final(ctx.outer, digest);
    }

    static std::vector<uint8_t> compute(std::span<const uint8_t> key, std::span<const uint8_t> data) {
        Context ctx;
        init(ctx, key);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, 32>(out.data(), 32));
        return out;
    }
};

class HmacSha384 {
public:
    static constexpr size_t DIGEST_SIZE = 48;
    static constexpr size_t BLOCK_SIZE  = 128;

    struct Context {
        Sha384::Context inner;
        Sha384::Context outer;
    };

    static void init(Context& ctx, std::span<const uint8_t> key) noexcept {
        uint8_t k[BLOCK_SIZE] = {0};
        if (key.size() > BLOCK_SIZE) {
            auto keyHash = Sha384::hash(key);
            std::memcpy(k, keyHash.data(), keyHash.size());
        } else {
            std::memcpy(k, key.data(), key.size());
        }

        uint8_t ipad[BLOCK_SIZE];
        uint8_t opad[BLOCK_SIZE];
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            ipad[i] = k[i] ^ 0x36;
            opad[i] = k[i] ^ 0x5c;
        }

        Sha384::init(ctx.inner);
        Sha384::update(ctx.inner, std::span<const uint8_t>(ipad, BLOCK_SIZE));

        Sha384::init(ctx.outer);
        Sha384::update(ctx.outer, std::span<const uint8_t>(opad, BLOCK_SIZE));
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        Sha384::update(ctx.inner, data);
    }

    static void final(Context& ctx, std::span<uint8_t, 48> digest) noexcept {
        uint8_t innerHash[48];
        Sha384::final(ctx.inner, std::span<uint8_t, 48>(innerHash, 48));

        Sha384::update(ctx.outer, std::span<const uint8_t>(innerHash, 48));
        Sha384::final(ctx.outer, digest);
    }

    static std::vector<uint8_t> compute(std::span<const uint8_t> key, std::span<const uint8_t> data) {
        Context ctx;
        init(ctx, key);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, 48>(out.data(), 48));
        return out;
    }
};

class HmacSha512 {
public:
    static constexpr size_t DIGEST_SIZE = 64;
    static constexpr size_t BLOCK_SIZE  = 128;

    struct Context {
        Sha512::Context inner;
        Sha512::Context outer;
    };

    static void init(Context& ctx, std::span<const uint8_t> key) noexcept {
        uint8_t k[BLOCK_SIZE] = {0};
        if (key.size() > BLOCK_SIZE) {
            auto keyHash = Sha512::hash(key);
            std::memcpy(k, keyHash.data(), keyHash.size());
        } else {
            std::memcpy(k, key.data(), key.size());
        }

        uint8_t ipad[BLOCK_SIZE];
        uint8_t opad[BLOCK_SIZE];
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            ipad[i] = k[i] ^ 0x36;
            opad[i] = k[i] ^ 0x5c;
        }

        Sha512::init(ctx.inner);
        Sha512::update(ctx.inner, std::span<const uint8_t>(ipad, BLOCK_SIZE));

        Sha512::init(ctx.outer);
        Sha512::update(ctx.outer, std::span<const uint8_t>(opad, BLOCK_SIZE));
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        Sha512::update(ctx.inner, data);
    }

    static void final(Context& ctx, std::span<uint8_t, 64> digest) noexcept {
        uint8_t innerHash[64];
        Sha512::final(ctx.inner, std::span<uint8_t, 64>(innerHash, 64));

        Sha512::update(ctx.outer, std::span<const uint8_t>(innerHash, 64));
        Sha512::final(ctx.outer, digest);
    }

    static std::vector<uint8_t> compute(std::span<const uint8_t> key, std::span<const uint8_t> data) {
        Context ctx;
        init(ctx, key);
        update(ctx, data);
        std::vector<uint8_t> out(DIGEST_SIZE);
        final(ctx, std::span<uint8_t, 64>(out.data(), 64));
        return out;
    }
};

// ============================================================================
// 4. Clean-Room PBKDF2 Key Derivation (RFC 2898 / PKCS#5 v2.0)
// ============================================================================
class Pbkdf2 {
public:
    static std::vector<uint8_t> derive(
        std::span<const uint8_t> password,
        std::span<const uint8_t> salt,
        uint32_t iterations,
        size_t keyLength
    ) {
        std::vector<uint8_t> derived;
        derived.reserve(keyLength);

        uint32_t blockIndex = 1;
        while (derived.size() < keyLength) {
            // Salt || INT_32_BE(blockIndex)
            std::vector<uint8_t> saltPlusIndex;
            saltPlusIndex.reserve(salt.size() + 4);
            saltPlusIndex.insert(saltPlusIndex.end(), salt.begin(), salt.end());
            saltPlusIndex.push_back(static_cast<uint8_t>((blockIndex >> 24) & 0xFF));
            saltPlusIndex.push_back(static_cast<uint8_t>((blockIndex >> 16) & 0xFF));
            saltPlusIndex.push_back(static_cast<uint8_t>((blockIndex >> 8) & 0xFF));
            saltPlusIndex.push_back(static_cast<uint8_t>(blockIndex & 0xFF));

            std::vector<uint8_t> u = HmacSha256::compute(password, saltPlusIndex);
            std::vector<uint8_t> t = u;

            for (uint32_t iter = 1; iter < iterations; ++iter) {
                u = HmacSha256::compute(password, u);
                for (size_t b = 0; b < t.size(); ++b) {
                    t[b] ^= u[b];
                }
            }

            size_t bytesNeeded = keyLength - derived.size();
            size_t bytesToCopy = std::min(bytesNeeded, t.size());
            derived.insert(derived.end(), t.begin(), t.begin() + bytesToCopy);
            blockIndex++;
        }

        return derived;
    }
};

// ============================================================================
// 5. Clean-Room AES Block Cipher (FIPS 197) Implementation
//    Supports AES-128 (16B), AES-192 (24B), AES-256 (32B) in ECB, CBC, CTR
// ============================================================================
class Aes {
public:
    static constexpr size_t BLOCK_SIZE = 16;

    enum class Mode {
        ECB,
        CBC,
        CTR
    };

    explicit Aes(std::span<const uint8_t> key) {
        setKey(key);
    }

    void setKey(std::span<const uint8_t> key) {
        size_t keyBytes = key.size();
        if (keyBytes == 16) {
            nk_ = 4;
            nr_ = 10;
        } else if (keyBytes == 24) {
            nk_ = 6;
            nr_ = 12;
        } else if (keyBytes == 32) {
            nk_ = 8;
            nr_ = 14;
        } else {
            nk_ = 4;
            nr_ = 10;
        }

        size_t totalWords = 4 * (nr_ + 1);
        roundKeys_.resize(totalWords);

        for (size_t i = 0; i < nk_; ++i) {
            roundKeys_[i] = (static_cast<uint32_t>(key[4 * i + 0]) << 24) |
                            (static_cast<uint32_t>(key[4 * i + 1]) << 16) |
                            (static_cast<uint32_t>(key[4 * i + 2]) << 8)  |
                            (static_cast<uint32_t>(key[4 * i + 3]));
        }

        for (size_t i = nk_; i < totalWords; ++i) {
            uint32_t temp = roundKeys_[i - 1];
            if (i % nk_ == 0) {
                temp = subWord(rotWord(temp)) ^ (static_cast<uint32_t>(RCON[(i / nk_) - 1]) << 24);
            } else if (nk_ > 6 && (i % nk_) == 4) {
                temp = subWord(temp);
            }
            roundKeys_[i] = roundKeys_[i - nk_] ^ temp;
        }
    }

    void encryptBlock(const uint8_t in[16], uint8_t out[16]) const noexcept {
        uint8_t s[4][4];
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                s[r][c] = in[c * 4 + r];
            }
        }

        addRoundKey(s, 0);

        for (size_t round = 1; round < nr_; ++round) {
            subBytes(s);
            shiftRows(s);
            mixColumns(s);
            addRoundKey(s, round);
        }

        subBytes(s);
        shiftRows(s);
        addRoundKey(s, nr_);

        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                out[c * 4 + r] = s[r][c];
            }
        }
    }

    void decryptBlock(const uint8_t in[16], uint8_t out[16]) const noexcept {
        uint8_t s[4][4];
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                s[r][c] = in[c * 4 + r];
            }
        }

        addRoundKey(s, nr_);

        for (size_t round = nr_ - 1; round > 0; --round) {
            invShiftRows(s);
            invSubBytes(s);
            addRoundKey(s, round);
            invMixColumns(s);
        }

        invShiftRows(s);
        invSubBytes(s);
        addRoundKey(s, 0);

        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                out[c * 4 + r] = s[r][c];
            }
        }
    }

    std::vector<uint8_t> encrypt(
        std::span<const uint8_t> input,
        Mode mode,
        std::span<const uint8_t> iv = {},
        bool pkcs7Padding = true
    ) const {
        std::vector<uint8_t> paddedInput(input.begin(), input.end());
        if (pkcs7Padding) {
            size_t padLen = BLOCK_SIZE - (paddedInput.size() % BLOCK_SIZE);
            paddedInput.insert(paddedInput.end(), padLen, static_cast<uint8_t>(padLen));
        }

        std::vector<uint8_t> output(paddedInput.size());
        size_t blockCount = paddedInput.size() / BLOCK_SIZE;

        if (mode == Mode::ECB) {
            for (size_t b = 0; b < blockCount; ++b) {
                encryptBlock(&paddedInput[b * BLOCK_SIZE], &output[b * BLOCK_SIZE]);
            }
        } else if (mode == Mode::CBC) {
            uint8_t ivBlock[16] = {0};
            if (!iv.empty()) {
                std::memcpy(ivBlock, iv.data(), std::min<size_t>(iv.size(), 16));
            }

            for (size_t b = 0; b < blockCount; ++b) {
                uint8_t xorBuf[16];
                for (int i = 0; i < 16; ++i) {
                    xorBuf[i] = paddedInput[b * BLOCK_SIZE + i] ^ ivBlock[i];
                }
                encryptBlock(xorBuf, &output[b * BLOCK_SIZE]);
                std::memcpy(ivBlock, &output[b * BLOCK_SIZE], 16);
            }
        } else if (mode == Mode::CTR) {
            uint8_t counterBlock[16] = {0};
            if (!iv.empty()) {
                std::memcpy(counterBlock, iv.data(), std::min<size_t>(iv.size(), 16));
            }

            for (size_t b = 0; b < blockCount; ++b) {
                uint8_t streamBlock[16];
                encryptBlock(counterBlock, streamBlock);
                for (int i = 0; i < 16; ++i) {
                    output[b * BLOCK_SIZE + i] = paddedInput[b * BLOCK_SIZE + i] ^ streamBlock[i];
                }
                incrementCounter(counterBlock);
            }
        }

        return output;
    }

    std::vector<uint8_t> decrypt(
        std::span<const uint8_t> input,
        Mode mode,
        std::span<const uint8_t> iv = {},
        bool pkcs7Padding = true,
        bool* pSuccess = nullptr
    ) const {
        if (pSuccess) *pSuccess = false;
        if (input.empty() || (input.size() % BLOCK_SIZE != 0)) {
            return {};
        }

        std::vector<uint8_t> output(input.size());
        size_t blockCount = input.size() / BLOCK_SIZE;

        if (mode == Mode::ECB) {
            for (size_t b = 0; b < blockCount; ++b) {
                decryptBlock(&input[b * BLOCK_SIZE], &output[b * BLOCK_SIZE]);
            }
        } else if (mode == Mode::CBC) {
            uint8_t ivBlock[16] = {0};
            if (!iv.empty()) {
                std::memcpy(ivBlock, iv.data(), std::min<size_t>(iv.size(), 16));
            }

            for (size_t b = 0; b < blockCount; ++b) {
                uint8_t cipherBlock[16];
                std::memcpy(cipherBlock, &input[b * BLOCK_SIZE], 16);

                uint8_t plainBlock[16];
                decryptBlock(cipherBlock, plainBlock);

                for (int i = 0; i < 16; ++i) {
                    output[b * BLOCK_SIZE + i] = plainBlock[i] ^ ivBlock[i];
                }
                std::memcpy(ivBlock, cipherBlock, 16);
            }
        } else if (mode == Mode::CTR) {
            uint8_t counterBlock[16] = {0};
            if (!iv.empty()) {
                std::memcpy(counterBlock, iv.data(), std::min<size_t>(iv.size(), 16));
            }

            for (size_t b = 0; b < blockCount; ++b) {
                uint8_t streamBlock[16];
                encryptBlock(counterBlock, streamBlock);
                for (int i = 0; i < 16; ++i) {
                    output[b * BLOCK_SIZE + i] = input[b * BLOCK_SIZE + i] ^ streamBlock[i];
                }
                incrementCounter(counterBlock);
            }
        }

        if (pkcs7Padding) {
            if (output.empty()) return {};
            uint8_t padLen = output.back();
            if (padLen == 0 || padLen > BLOCK_SIZE || padLen > output.size()) {
                return {};
            }
            for (size_t i = output.size() - padLen; i < output.size(); ++i) {
                if (output[i] != padLen) return {};
            }
            output.resize(output.size() - padLen);
        }

        if (pSuccess) *pSuccess = true;
        return output;
    }

    struct GcmResult {
        std::vector<uint8_t> ciphertext;
        std::array<uint8_t, 16> tag{};
        bool success{false};
    };

    struct GcmDecryptResult {
        std::vector<uint8_t> plaintext;
        bool success{false};
    };

    static void gf128Mul(const uint8_t x[16], const uint8_t y[16], uint8_t res[16]) noexcept {
        uint8_t z[16] = {0};
        uint8_t v[16];
        std::memcpy(v, y, 16);
        for (int i = 0; i < 128; ++i) {
            if ((x[i / 8] >> (7 - (i % 8))) & 1) {
                for (int j = 0; j < 16; ++j) z[j] ^= v[j];
            }
            bool lsb = (v[15] & 1) != 0;
            for (int j = 15; j > 0; --j) {
                v[j] = static_cast<uint8_t>((v[j] >> 1) | (v[j - 1] << 7));
            }
            v[0] >>= 1;
            if (lsb) {
                v[0] ^= 0xe1;
            }
        }
        std::memcpy(res, z, 16);
    }

    static void ghash(const uint8_t h[16], const uint8_t* data, size_t len, uint8_t y[16]) noexcept {
        for (size_t i = 0; i < len; i += 16) {
            uint8_t block[16] = {0};
            size_t chunkSize = std::min<size_t>(16, len - i);
            std::memcpy(block, data + i, chunkSize);
            for (int j = 0; j < 16; ++j) {
                y[j] ^= block[j];
            }
            uint8_t nextY[16];
            gf128Mul(y, h, nextY);
            std::memcpy(y, nextY, 16);
        }
    }

    GcmResult encryptGcm(
        std::span<const uint8_t> plaintext,
        std::span<const uint8_t> nonce,
        std::span<const uint8_t> aad,
        size_t tagLen = 16
    ) const {
        GcmResult res;
        if (tagLen < 12 || tagLen > 16 || nonce.empty()) {
            return res;
        }

        uint8_t h[16] = {0};
        uint8_t zeroBlock[16] = {0};
        encryptBlock(zeroBlock, h);

        uint8_t j0[16] = {0};
        if (nonce.size() == 12) {
            std::memcpy(j0, nonce.data(), 12);
            j0[15] = 1;
        } else {
            uint8_t y[16] = {0};
            ghash(h, nonce.data(), nonce.size(), y);
            uint8_t lenBuf[16] = {0};
            uint64_t nonceBitLen = static_cast<uint64_t>(nonce.size()) * 8;
            for (int i = 0; i < 8; ++i) {
                lenBuf[15 - i] = static_cast<uint8_t>((nonceBitLen >> (i * 8)) & 0xFF);
            }
            ghash(h, lenBuf, 16, y);
            std::memcpy(j0, y, 16);
        }

        uint8_t cb[16];
        std::memcpy(cb, j0, 16);
        res.ciphertext.resize(plaintext.size());

        for (size_t i = 0; i < plaintext.size(); i += 16) {
            uint32_t ctr = (static_cast<uint32_t>(cb[12]) << 24) |
                           (static_cast<uint32_t>(cb[13]) << 16) |
                           (static_cast<uint32_t>(cb[14]) << 8)  |
                           (static_cast<uint32_t>(cb[15]));
            ctr++;
            cb[12] = static_cast<uint8_t>((ctr >> 24) & 0xFF);
            cb[13] = static_cast<uint8_t>((ctr >> 16) & 0xFF);
            cb[14] = static_cast<uint8_t>((ctr >> 8) & 0xFF);
            cb[15] = static_cast<uint8_t>(ctr & 0xFF);

            uint8_t streamBlock[16];
            encryptBlock(cb, streamBlock);
            size_t chunkSize = std::min<size_t>(16, plaintext.size() - i);
            for (size_t k = 0; k < chunkSize; ++k) {
                res.ciphertext[i + k] = plaintext[i + k] ^ streamBlock[k];
            }
        }

        uint8_t s[16] = {0};
        if (!aad.empty()) {
            ghash(h, aad.data(), aad.size(), s);
        }
        if (!res.ciphertext.empty()) {
            ghash(h, res.ciphertext.data(), res.ciphertext.size(), s);
        }
        uint8_t lenBlock[16] = {0};
        uint64_t aadBits = static_cast<uint64_t>(aad.size()) * 8;
        uint64_t ctBits = static_cast<uint64_t>(res.ciphertext.size()) * 8;
        for (int i = 0; i < 8; ++i) {
            lenBlock[7 - i] = static_cast<uint8_t>((aadBits >> (i * 8)) & 0xFF);
            lenBlock[15 - i] = static_cast<uint8_t>((ctBits >> (i * 8)) & 0xFF);
        }
        ghash(h, lenBlock, 16, s);

        uint8_t ej0[16];
        encryptBlock(j0, ej0);
        for (size_t i = 0; i < 16; ++i) {
            res.tag[i] = s[i] ^ ej0[i];
        }
        res.success = true;
        return res;
    }

    GcmDecryptResult decryptGcm(
        std::span<const uint8_t> ciphertext,
        std::span<const uint8_t> nonce,
        std::span<const uint8_t> aad,
        std::span<const uint8_t> tag
    ) const {
        GcmDecryptResult res;
        if (tag.size() < 12 || tag.size() > 16 || nonce.empty()) {
            return res;
        }

        uint8_t h[16] = {0};
        uint8_t zeroBlock[16] = {0};
        encryptBlock(zeroBlock, h);

        uint8_t j0[16] = {0};
        if (nonce.size() == 12) {
            std::memcpy(j0, nonce.data(), 12);
            j0[15] = 1;
        } else {
            uint8_t y[16] = {0};
            ghash(h, nonce.data(), nonce.size(), y);
            uint8_t lenBuf[16] = {0};
            uint64_t nonceBitLen = static_cast<uint64_t>(nonce.size()) * 8;
            for (int i = 0; i < 8; ++i) {
                lenBuf[15 - i] = static_cast<uint8_t>((nonceBitLen >> (i * 8)) & 0xFF);
            }
            ghash(h, lenBuf, 16, y);
            std::memcpy(j0, y, 16);
        }

        uint8_t s[16] = {0};
        if (!aad.empty()) {
            ghash(h, aad.data(), aad.size(), s);
        }
        if (!ciphertext.empty()) {
            ghash(h, ciphertext.data(), ciphertext.size(), s);
        }
        uint8_t lenBlock[16] = {0};
        uint64_t aadBits = static_cast<uint64_t>(aad.size()) * 8;
        uint64_t ctBits = static_cast<uint64_t>(ciphertext.size()) * 8;
        for (int i = 0; i < 8; ++i) {
            lenBlock[7 - i] = static_cast<uint8_t>((aadBits >> (i * 8)) & 0xFF);
            lenBlock[15 - i] = static_cast<uint8_t>((ctBits >> (i * 8)) & 0xFF);
        }
        ghash(h, lenBlock, 16, s);

        uint8_t ej0[16];
        encryptBlock(j0, ej0);
        uint8_t expectedTag[16];
        for (size_t i = 0; i < 16; ++i) {
            expectedTag[i] = s[i] ^ ej0[i];
        }

        uint8_t diff = 0;
        for (size_t i = 0; i < tag.size(); ++i) {
            diff |= (expectedTag[i] ^ tag[i]);
        }
        if (diff != 0) {
            return res;
        }

        uint8_t cb[16];
        std::memcpy(cb, j0, 16);
        res.plaintext.resize(ciphertext.size());

        for (size_t i = 0; i < ciphertext.size(); i += 16) {
            uint32_t ctr = (static_cast<uint32_t>(cb[12]) << 24) |
                           (static_cast<uint32_t>(cb[13]) << 16) |
                           (static_cast<uint32_t>(cb[14]) << 8)  |
                           (static_cast<uint32_t>(cb[15]));
            ctr++;
            cb[12] = static_cast<uint8_t>((ctr >> 24) & 0xFF);
            cb[13] = static_cast<uint8_t>((ctr >> 16) & 0xFF);
            cb[14] = static_cast<uint8_t>((ctr >> 8) & 0xFF);
            cb[15] = static_cast<uint8_t>(ctr & 0xFF);

            uint8_t streamBlock[16];
            encryptBlock(cb, streamBlock);
            size_t chunkSize = std::min<size_t>(16, ciphertext.size() - i);
            for (size_t k = 0; k < chunkSize; ++k) {
                res.plaintext[i + k] = ciphertext[i + k] ^ streamBlock[k];
            }
        }

        res.success = true;
        return res;
    }

    struct CcmResult {
        bool success{false};
        std::vector<uint8_t> ciphertext;
        std::array<uint8_t, 16> tag{0};
    };

    struct CcmDecryptResult {
        bool success{false};
        std::vector<uint8_t> plaintext;
    };

    CcmResult encryptCcm(
        std::span<const uint8_t> plaintext,
        std::span<const uint8_t> nonce,
        std::span<const uint8_t> aad,
        size_t tagLen = 16
    ) const {
        CcmResult res;
        if (nonce.size() < 7 || nonce.size() > 13) return res;
        if (tagLen < 4 || tagLen > 16 || (tagLen % 2 != 0)) return res;

        uint8_t q = static_cast<uint8_t>(15 - nonce.size());
        uint64_t maxP = (q >= 8) ? UINT64_MAX : ((1ULL << (8 * q)) - 1);
        if (plaintext.size() > maxP) return res;

        // Step 1: Format B0
        uint8_t b0[16] = {0};
        uint8_t flags = 0;
        if (!aad.empty()) flags |= 0x40;
        flags |= static_cast<uint8_t>(((tagLen - 2) / 2) << 3);
        flags |= static_cast<uint8_t>(q - 1);
        b0[0] = flags;
        std::memcpy(&b0[1], nonce.data(), nonce.size());
        uint64_t pLen = plaintext.size();
        for (uint8_t i = 0; i < q; ++i) {
            b0[15 - i] = static_cast<uint8_t>((pLen >> (i * 8)) & 0xFF);
        }

        // Step 2: CBC-MAC
        uint8_t y[16] = {0};
        encryptBlock(b0, y);

        if (!aad.empty()) {
            std::vector<uint8_t> formattedAad;
            size_t a = aad.size();
            if (a < 65280) {
                formattedAad.push_back(static_cast<uint8_t>((a >> 8) & 0xFF));
                formattedAad.push_back(static_cast<uint8_t>(a & 0xFF));
            } else {
                formattedAad.push_back(0xFF);
                formattedAad.push_back(0xFE);
                formattedAad.push_back(static_cast<uint8_t>((a >> 24) & 0xFF));
                formattedAad.push_back(static_cast<uint8_t>((a >> 16) & 0xFF));
                formattedAad.push_back(static_cast<uint8_t>((a >> 8) & 0xFF));
                formattedAad.push_back(static_cast<uint8_t>(a & 0xFF));
            }
            formattedAad.insert(formattedAad.end(), aad.begin(), aad.end());
            while (formattedAad.size() % 16 != 0) formattedAad.push_back(0);

            for (size_t offset = 0; offset < formattedAad.size(); offset += 16) {
                for (size_t k = 0; k < 16; ++k) y[k] ^= formattedAad[offset + k];
                uint8_t nextY[16];
                encryptBlock(y, nextY);
                std::memcpy(y, nextY, 16);
            }
        }

        if (!plaintext.empty()) {
            for (size_t offset = 0; offset < plaintext.size(); offset += 16) {
                size_t chunk = std::min<size_t>(16, plaintext.size() - offset);
                for (size_t k = 0; k < chunk; ++k) y[k] ^= plaintext[offset + k];
                uint8_t nextY[16];
                encryptBlock(y, nextY);
                std::memcpy(y, nextY, 16);
            }
        }

        // Step 3: CTR encryption
        uint8_t ctr0[16] = {0};
        ctr0[0] = static_cast<uint8_t>(q - 1);
        std::memcpy(&ctr0[1], nonce.data(), nonce.size());

        uint8_t s0[16];
        encryptBlock(ctr0, s0);
        for (size_t k = 0; k < tagLen; ++k) {
            res.tag[k] = y[k] ^ s0[k];
        }

        res.ciphertext.resize(plaintext.size());
        uint8_t ctrBlock[16];
        std::memcpy(ctrBlock, ctr0, 16);
        uint64_t counter = 0;
        for (size_t offset = 0; offset < plaintext.size(); offset += 16) {
            counter++;
            for (uint8_t i = 0; i < q; ++i) {
                ctrBlock[15 - i] = static_cast<uint8_t>((counter >> (i * 8)) & 0xFF);
            }
            uint8_t sj[16];
            encryptBlock(ctrBlock, sj);
            size_t chunk = std::min<size_t>(16, plaintext.size() - offset);
            for (size_t k = 0; k < chunk; ++k) {
                res.ciphertext[offset + k] = plaintext[offset + k] ^ sj[k];
            }
        }

        res.success = true;
        return res;
    }

    CcmDecryptResult decryptCcm(
        std::span<const uint8_t> ciphertext,
        std::span<const uint8_t> nonce,
        std::span<const uint8_t> aad,
        std::span<const uint8_t> tag
    ) const {
        CcmDecryptResult res;
        if (nonce.size() < 7 || nonce.size() > 13) return res;
        size_t tagLen = tag.size();
        if (tagLen < 4 || tagLen > 16 || (tagLen % 2 != 0)) return res;

        uint8_t q = static_cast<uint8_t>(15 - nonce.size());

        std::vector<uint8_t> candidatePt(ciphertext.size());
        uint8_t ctr0[16] = {0};
        ctr0[0] = static_cast<uint8_t>(q - 1);
        std::memcpy(&ctr0[1], nonce.data(), nonce.size());

        uint8_t ctrBlock[16];
        std::memcpy(ctrBlock, ctr0, 16);
        uint64_t counter = 0;
        for (size_t offset = 0; offset < ciphertext.size(); offset += 16) {
            counter++;
            for (uint8_t i = 0; i < q; ++i) {
                ctrBlock[15 - i] = static_cast<uint8_t>((counter >> (i * 8)) & 0xFF);
            }
            uint8_t sj[16];
            encryptBlock(ctrBlock, sj);
            size_t chunk = std::min<size_t>(16, ciphertext.size() - offset);
            for (size_t k = 0; k < chunk; ++k) {
                candidatePt[offset + k] = ciphertext[offset + k] ^ sj[k];
            }
        }

        // Compute CBC-MAC
        uint8_t b0[16] = {0};
        uint8_t flags = 0;
        if (!aad.empty()) flags |= 0x40;
        flags |= static_cast<uint8_t>(((tagLen - 2) / 2) << 3);
        flags |= static_cast<uint8_t>(q - 1);
        b0[0] = flags;
        std::memcpy(&b0[1], nonce.data(), nonce.size());
        uint64_t pLen = candidatePt.size();
        for (uint8_t i = 0; i < q; ++i) {
            b0[15 - i] = static_cast<uint8_t>((pLen >> (i * 8)) & 0xFF);
        }

        uint8_t y[16] = {0};
        encryptBlock(b0, y);

        if (!aad.empty()) {
            std::vector<uint8_t> formattedAad;
            size_t a = aad.size();
            if (a < 65280) {
                formattedAad.push_back(static_cast<uint8_t>((a >> 8) & 0xFF));
                formattedAad.push_back(static_cast<uint8_t>(a & 0xFF));
            } else {
                formattedAad.push_back(0xFF);
                formattedAad.push_back(0xFE);
                formattedAad.push_back(static_cast<uint8_t>((a >> 24) & 0xFF));
                formattedAad.push_back(static_cast<uint8_t>((a >> 16) & 0xFF));
                formattedAad.push_back(static_cast<uint8_t>((a >> 8) & 0xFF));
                formattedAad.push_back(static_cast<uint8_t>(a & 0xFF));
            }
            formattedAad.insert(formattedAad.end(), aad.begin(), aad.end());
            while (formattedAad.size() % 16 != 0) formattedAad.push_back(0);

            for (size_t offset = 0; offset < formattedAad.size(); offset += 16) {
                for (size_t k = 0; k < 16; ++k) y[k] ^= formattedAad[offset + k];
                uint8_t nextY[16];
                encryptBlock(y, nextY);
                std::memcpy(y, nextY, 16);
            }
        }

        if (!candidatePt.empty()) {
            for (size_t offset = 0; offset < candidatePt.size(); offset += 16) {
                size_t chunk = std::min<size_t>(16, candidatePt.size() - offset);
                for (size_t k = 0; k < chunk; ++k) y[k] ^= candidatePt[offset + k];
                uint8_t nextY[16];
                encryptBlock(y, nextY);
                std::memcpy(y, nextY, 16);
            }
        }

        uint8_t s0[16];
        encryptBlock(ctr0, s0);
        uint8_t expectedTag[16];
        for (size_t k = 0; k < tagLen; ++k) {
            expectedTag[k] = y[k] ^ s0[k];
        }

        uint8_t diff = 0;
        for (size_t k = 0; k < tagLen; ++k) {
            diff |= (expectedTag[k] ^ tag[k]);
        }
        if (diff != 0) return res;

        res.plaintext = std::move(candidatePt);
        res.success = true;
        return res;
    }

    static void xtsMulAlpha(const uint8_t in[16], uint8_t out[16]) noexcept {
        uint8_t carry = 0;
        for (int i = 0; i < 16; ++i) {
            uint8_t next_carry = (in[i] >> 7) & 1;
            out[i] = static_cast<uint8_t>((in[i] << 1) | carry);
            carry = next_carry;
        }
        if (carry) {
            out[0] ^= 0x87;
        }
    }

    static std::vector<uint8_t> encryptXts(
        const Aes& aes1,
        const Aes& aes2,
        std::span<const uint8_t> plaintext,
        std::span<const uint8_t> tweak
    ) {
        size_t totalBytes = plaintext.size();
        if (totalBytes < 16) return {};

        uint8_t t[16];
        uint8_t tweakBlock[16] = {0};
        if (!tweak.empty()) {
            std::memcpy(tweakBlock, tweak.data(), std::min<size_t>(tweak.size(), 16));
        }
        aes2.encryptBlock(tweakBlock, t);

        std::vector<uint8_t> ciphertext(totalBytes);
        size_t m = totalBytes / 16;
        size_t b = totalBytes % 16;

        if (b == 0) {
            for (size_t i = 0; i < m; ++i) {
                uint8_t pp[16];
                for (int k = 0; k < 16; ++k) pp[k] = plaintext[i * 16 + k] ^ t[k];
                uint8_t cc[16];
                aes1.encryptBlock(pp, cc);
                for (int k = 0; k < 16; ++k) ciphertext[i * 16 + k] = cc[k] ^ t[k];
                if (i + 1 < m) {
                    uint8_t nextT[16];
                    xtsMulAlpha(t, nextT);
                    std::memcpy(t, nextT, 16);
                }
            }
        } else {
            for (size_t i = 0; i + 1 < m; ++i) {
                uint8_t pp[16];
                for (int k = 0; k < 16; ++k) pp[k] = plaintext[i * 16 + k] ^ t[k];
                uint8_t cc[16];
                aes1.encryptBlock(pp, cc);
                for (int k = 0; k < 16; ++k) ciphertext[i * 16 + k] = cc[k] ^ t[k];
                uint8_t nextT[16];
                xtsMulAlpha(t, nextT);
                std::memcpy(t, nextT, 16);
            }

            uint8_t tm_prev[16];
            std::memcpy(tm_prev, t, 16);

            uint8_t pp_m1[16];
            for (int k = 0; k < 16; ++k) pp_m1[k] = plaintext[(m - 1) * 16 + k] ^ tm_prev[k];
            uint8_t cc[16];
            aes1.encryptBlock(pp_m1, cc);
            for (int k = 0; k < 16; ++k) cc[k] ^= tm_prev[k];

            for (size_t k = 0; k < b; ++k) {
                ciphertext[m * 16 + k] = cc[k];
            }

            uint8_t tm[16];
            xtsMulAlpha(tm_prev, tm);

            uint8_t pp[16];
            for (size_t k = 0; k < b; ++k) pp[k] = plaintext[m * 16 + k];
            for (size_t k = b; k < 16; ++k) pp[k] = cc[k];

            uint8_t pp_xor[16];
            for (int k = 0; k < 16; ++k) pp_xor[k] = pp[k] ^ tm[k];
            uint8_t cc_m1[16];
            aes1.encryptBlock(pp_xor, cc_m1);
            for (int k = 0; k < 16; ++k) ciphertext[(m - 1) * 16 + k] = cc_m1[k] ^ tm[k];
        }
        return ciphertext;
    }

    static std::vector<uint8_t> decryptXts(
        const Aes& aes1,
        const Aes& aes2,
        std::span<const uint8_t> ciphertext,
        std::span<const uint8_t> tweak
    ) {
        size_t totalBytes = ciphertext.size();
        if (totalBytes < 16) return {};

        uint8_t t[16];
        uint8_t tweakBlock[16] = {0};
        if (!tweak.empty()) {
            std::memcpy(tweakBlock, tweak.data(), std::min<size_t>(tweak.size(), 16));
        }
        aes2.encryptBlock(tweakBlock, t);

        std::vector<uint8_t> plaintext(totalBytes);
        size_t m = totalBytes / 16;
        size_t b = totalBytes % 16;

        if (b == 0) {
            for (size_t i = 0; i < m; ++i) {
                uint8_t cc[16];
                for (int k = 0; k < 16; ++k) cc[k] = ciphertext[i * 16 + k] ^ t[k];
                uint8_t pp[16];
                aes1.decryptBlock(cc, pp);
                for (int k = 0; k < 16; ++k) plaintext[i * 16 + k] = pp[k] ^ t[k];
                if (i + 1 < m) {
                    uint8_t nextT[16];
                    xtsMulAlpha(t, nextT);
                    std::memcpy(t, nextT, 16);
                }
            }
        } else {
            for (size_t i = 0; i + 1 < m; ++i) {
                uint8_t cc[16];
                for (int k = 0; k < 16; ++k) cc[k] = ciphertext[i * 16 + k] ^ t[k];
                uint8_t pp[16];
                aes1.decryptBlock(cc, pp);
                for (int k = 0; k < 16; ++k) plaintext[i * 16 + k] = pp[k] ^ t[k];
                uint8_t nextT[16];
                xtsMulAlpha(t, nextT);
                std::memcpy(t, nextT, 16);
            }

            uint8_t tm_prev[16];
            std::memcpy(tm_prev, t, 16);

            uint8_t tm[16];
            xtsMulAlpha(tm_prev, tm);

            uint8_t cc_m1[16];
            for (int k = 0; k < 16; ++k) cc_m1[k] = ciphertext[(m - 1) * 16 + k] ^ tm[k];
            uint8_t pp[16];
            aes1.decryptBlock(cc_m1, pp);
            for (int k = 0; k < 16; ++k) pp[k] ^= tm[k];

            for (size_t k = 0; k < b; ++k) plaintext[m * 16 + k] = pp[k];

            uint8_t cc[16];
            for (size_t k = 0; k < b; ++k) cc[k] = ciphertext[m * 16 + k];
            for (size_t k = b; k < 16; ++k) cc[k] = pp[k];

            uint8_t cc_xor[16];
            for (int k = 0; k < 16; ++k) cc_xor[k] = cc[k] ^ tm_prev[k];
            uint8_t pp_m1[16];
            aes1.decryptBlock(cc_xor, pp_m1);
            for (int k = 0; k < 16; ++k) plaintext[(m - 1) * 16 + k] = pp_m1[k] ^ tm_prev[k];
        }
        return plaintext;
    }

    // RFC 3394 AES Key Wrap (KW)
    // Plaintext length must be >= 16 bytes and a multiple of 8 bytes.
    // Ciphertext length will be plaintext.size() + 8.
    std::vector<uint8_t> keyWrap(
        std::span<const uint8_t> plaintext,
        uint64_t initialIv = 0xA6A6A6A6A6A6A6A6ULL
    ) const {
        size_t nBytes = plaintext.size();
        if (nBytes < 16 || (nBytes % 8) != 0) return {};
        size_t n = nBytes / 8; // Number of 64-bit blocks

        uint8_t A[8];
        uint64_t ivTmp = initialIv;
        for (int k = 7; k >= 0; --k) {
            A[k] = static_cast<uint8_t>(ivTmp & 0xFF);
            ivTmp >>= 8;
        }

        std::vector<std::array<uint8_t, 8>> R(n + 1);
        for (size_t i = 1; i <= n; ++i) {
            std::memcpy(R[i].data(), plaintext.data() + (i - 1) * 8, 8);
        }

        for (size_t j = 0; j <= 5; ++j) {
            for (size_t i = 1; i <= n; ++i) {
                uint64_t t = (n * j) + i;
                uint8_t B_in[16];
                std::memcpy(B_in, A, 8);
                std::memcpy(B_in + 8, R[i].data(), 8);

                uint8_t B_out[16];
                encryptBlock(B_in, B_out);

                std::memcpy(A, B_out, 8);
                uint64_t A_val = 0;
                for (int k = 0; k < 8; ++k) {
                    A_val = (A_val << 8) | A[k];
                }
                A_val ^= t;
                for (int k = 7; k >= 0; --k) {
                    A[k] = static_cast<uint8_t>(A_val & 0xFF);
                    A_val >>= 8;
                }

                std::memcpy(R[i].data(), B_out + 8, 8);
            }
        }

        std::vector<uint8_t> ciphertext((n + 1) * 8);
        std::memcpy(ciphertext.data(), A, 8);
        for (size_t i = 1; i <= n; ++i) {
            std::memcpy(ciphertext.data() + i * 8, R[i].data(), 8);
        }
        return ciphertext;
    }

    // RFC 3394 AES Key Unwrap (KW)
    // Ciphertext length must be >= 24 bytes and a multiple of 8 bytes.
    // Plaintext length will be ciphertext.size() - 8.
    // Returns empty vector if integrity check fails.
    std::vector<uint8_t> keyUnwrap(
        std::span<const uint8_t> ciphertext,
        uint64_t expectedIv = 0xA6A6A6A6A6A6A6A6ULL
    ) const {
        size_t cBytes = ciphertext.size();
        if (cBytes < 24 || (cBytes % 8) != 0) return {};
        size_t n = (cBytes / 8) - 1; // Number of 64-bit plaintext blocks

        uint8_t A[8];
        std::memcpy(A, ciphertext.data(), 8);

        std::vector<std::array<uint8_t, 8>> R(n + 1);
        for (size_t i = 1; i <= n; ++i) {
            std::memcpy(R[i].data(), ciphertext.data() + i * 8, 8);
        }

        for (int j = 5; j >= 0; --j) {
            for (size_t i = n; i >= 1; --i) {
                uint64_t t = (n * j) + i;

                uint64_t A_val = 0;
                for (int k = 0; k < 8; ++k) {
                    A_val = (A_val << 8) | A[k];
                }
                A_val ^= t;
                for (int k = 7; k >= 0; --k) {
                    A[k] = static_cast<uint8_t>(A_val & 0xFF);
                    A_val >>= 8;
                }

                uint8_t B_in[16];
                std::memcpy(B_in, A, 8);
                std::memcpy(B_in + 8, R[i].data(), 8);

                uint8_t B_out[16];
                decryptBlock(B_in, B_out);

                std::memcpy(A, B_out, 8);
                std::memcpy(R[i].data(), B_out + 8, 8);
            }
        }

        uint8_t exp[8];
        uint64_t expTmp = expectedIv;
        for (int k = 7; k >= 0; --k) {
            exp[k] = static_cast<uint8_t>(expTmp & 0xFF);
            expTmp >>= 8;
        }

        if (std::memcmp(A, exp, 8) != 0) {
            return {}; // Integrity check failed
        }

        std::vector<uint8_t> plaintext(n * 8);
        for (size_t i = 1; i <= n; ++i) {
            std::memcpy(plaintext.data() + (i - 1) * 8, R[i].data(), 8);
        }
        return plaintext;
    }

    // RFC 5649 AES Key Wrap with Padding (KWP)
    // Supports arbitrary plaintext length m >= 1.
    std::vector<uint8_t> keyWrapWithPadding(std::span<const uint8_t> plaintext) const {
        size_t m = plaintext.size();
        if (m == 0) return {};

        size_t padLen = (8 - (m % 8)) % 8;
        size_t totalPadded = m + padLen;
        std::vector<uint8_t> padded(totalPadded, 0);
        std::memcpy(padded.data(), plaintext.data(), m);

        uint64_t A0 = 0xA65959A600000000ULL | (static_cast<uint64_t>(m) & 0xFFFFFFFFULL);

        if (totalPadded == 8) {
            // Single semi-block
            uint8_t B_in[16];
            uint64_t a0Tmp = A0;
            for (int k = 7; k >= 0; --k) {
                B_in[k] = static_cast<uint8_t>(a0Tmp & 0xFF);
                a0Tmp >>= 8;
            }
            std::memcpy(B_in + 8, padded.data(), 8);
            std::vector<uint8_t> ciphertext(16);
            encryptBlock(B_in, ciphertext.data());
            return ciphertext;
        }

        // Multiple semi-blocks: standard KW with A0
        return keyWrap(padded, A0);
    }

    // RFC 5649 AES Key Unwrap with Padding (KWP)
    std::vector<uint8_t> keyUnwrapWithPadding(std::span<const uint8_t> ciphertext) const {
        size_t cBytes = ciphertext.size();
        if (cBytes < 16 || (cBytes % 8) != 0) return {};

        if (cBytes == 16) {
            uint8_t B_out[16];
            decryptBlock(ciphertext.data(), B_out);
            uint32_t magic = (static_cast<uint32_t>(B_out[0]) << 24) |
                             (static_cast<uint32_t>(B_out[1]) << 16) |
                             (static_cast<uint32_t>(B_out[2]) << 8)  |
                             static_cast<uint32_t>(B_out[3]);
            if (magic != 0xA65959A6) return {};
            uint32_t m = (static_cast<uint32_t>(B_out[4]) << 24) |
                         (static_cast<uint32_t>(B_out[5]) << 16) |
                         (static_cast<uint32_t>(B_out[6]) << 8)  |
                         static_cast<uint32_t>(B_out[7]);
            if (m < 1 || m > 8) return {};
            for (size_t k = 8 + m; k < 16; ++k) {
                if (B_out[k] != 0) return {};
            }
            std::vector<uint8_t> plaintext(m);
            std::memcpy(plaintext.data(), B_out + 8, m);
            return plaintext;
        }

        // Multiple semi-blocks
        size_t n = (cBytes / 8) - 1;
        uint8_t A[8];
        std::memcpy(A, ciphertext.data(), 8);

        std::vector<std::array<uint8_t, 8>> R(n + 1);
        for (size_t i = 1; i <= n; ++i) {
            std::memcpy(R[i].data(), ciphertext.data() + i * 8, 8);
        }

        for (int j = 5; j >= 0; --j) {
            for (size_t i = n; i >= 1; --i) {
                uint64_t t = (n * j) + i;

                uint64_t A_val = 0;
                for (int k = 0; k < 8; ++k) {
                    A_val = (A_val << 8) | A[k];
                }
                A_val ^= t;
                for (int k = 7; k >= 0; --k) {
                    A[k] = static_cast<uint8_t>(A_val & 0xFF);
                    A_val >>= 8;
                }

                uint8_t B_in[16];
                std::memcpy(B_in, A, 8);
                std::memcpy(B_in + 8, R[i].data(), 8);

                uint8_t B_out[16];
                decryptBlock(B_in, B_out);

                std::memcpy(A, B_out, 8);
                std::memcpy(R[i].data(), B_out + 8, 8);
            }
        }

        uint32_t magic = (static_cast<uint32_t>(A[0]) << 24) |
                         (static_cast<uint32_t>(A[1]) << 16) |
                         (static_cast<uint32_t>(A[2]) << 8)  |
                         static_cast<uint32_t>(A[3]);
        if (magic != 0xA65959A6) return {};
        uint32_t m = (static_cast<uint32_t>(A[4]) << 24) |
                     (static_cast<uint32_t>(A[5]) << 16) |
                     (static_cast<uint32_t>(A[6]) << 8)  |
                     static_cast<uint32_t>(A[7]);
        if (m < (n * 8 - 7) || m > (n * 8)) return {};

        std::vector<uint8_t> fullPadded(n * 8);
        for (size_t i = 1; i <= n; ++i) {
            std::memcpy(fullPadded.data() + (i - 1) * 8, R[i].data(), 8);
        }

        for (size_t k = m; k < n * 8; ++k) {
            if (fullPadded[k] != 0) return {};
        }

        std::vector<uint8_t> plaintext(m);
        std::memcpy(plaintext.data(), fullPadded.data(), m);
        return plaintext;
    }

    // RFC 4493 / NIST SP 800-38B AES-CMAC
    void generateCmacSubkeys(std::array<uint8_t, 16>& k1, std::array<uint8_t, 16>& k2) const {
        std::array<uint8_t, 16> zeroBlock{};
        std::array<uint8_t, 16> L{};
        encryptBlock(zeroBlock.data(), L.data());

        uint8_t msbL = (L[0] & 0x80) != 0;
        for (size_t i = 0; i < 15; ++i) {
            k1[i] = static_cast<uint8_t>((L[i] << 1) | ((L[i + 1] & 0x80) ? 1 : 0));
        }
        k1[15] = static_cast<uint8_t>(L[15] << 1);
        if (msbL) k1[15] ^= 0x87;

        uint8_t msbK1 = (k1[0] & 0x80) != 0;
        for (size_t i = 0; i < 15; ++i) {
            k2[i] = static_cast<uint8_t>((k1[i] << 1) | ((k1[i + 1] & 0x80) ? 1 : 0));
        }
        k2[15] = static_cast<uint8_t>(k1[15] << 1);
        if (msbK1) k2[15] ^= 0x87;
    }

    std::array<uint8_t, 16> cmac(std::span<const uint8_t> message) const {
        std::array<uint8_t, 16> k1{};
        std::array<uint8_t, 16> k2{};
        generateCmacSubkeys(k1, k2);

        size_t len = message.size();
        size_t n = (len + 15) / 16;
        bool isComplete = (len > 0) && (len % 16 == 0);
        if (len == 0) {
            n = 1;
            isComplete = false;
        }

        std::array<uint8_t, 16> M_last{};
        if (isComplete) {
            const uint8_t* pLast = message.data() + (n - 1) * 16;
            for (size_t i = 0; i < 16; ++i) {
                M_last[i] = pLast[i] ^ k1[i];
            }
        } else {
            size_t rem = len % 16;
            const uint8_t* pLast = len > 0 ? (message.data() + (n - 1) * 16) : nullptr;
            for (size_t i = 0; i < rem; ++i) {
                M_last[i] = pLast[i];
            }
            M_last[rem] = 0x80;
            for (size_t i = rem + 1; i < 16; ++i) {
                M_last[i] = 0x00;
            }
            for (size_t i = 0; i < 16; ++i) {
                M_last[i] ^= k2[i];
            }
        }

        std::array<uint8_t, 16> X{};
        std::array<uint8_t, 16> Y{};
        for (size_t i = 0; i < n - 1; ++i) {
            const uint8_t* pBlock = message.data() + i * 16;
            for (size_t j = 0; j < 16; ++j) {
                Y[j] = X[j] ^ pBlock[j];
            }
            encryptBlock(Y.data(), X.data());
        }

        for (size_t j = 0; j < 16; ++j) {
            Y[j] = X[j] ^ M_last[j];
        }
        std::array<uint8_t, 16> T{};
        encryptBlock(Y.data(), T.data());
        return T;
    }

    bool cmacVerify(std::span<const uint8_t> message, std::span<const uint8_t> tag) const {
        if (tag.size() != 16) return false;
        auto expected = cmac(message);
        uint8_t diff = 0;
        for (size_t i = 0; i < 16; ++i) {
            diff |= (expected[i] ^ tag[i]);
        }
        return diff == 0;
    }

private:
    size_t nk_{4};
    size_t nr_{10};
    std::vector<uint32_t> roundKeys_;

    static constexpr uint8_t SBOX[256] = {
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
        0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
        0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
        0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
        0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
        0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
        0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
        0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
        0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
        0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
        0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
        0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
        0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
        0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
        0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
    };

    static constexpr uint8_t INV_SBOX[256] = {
        0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
        0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
        0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
        0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
        0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
        0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
        0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
        0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
        0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
        0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
        0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
        0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
        0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
        0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
        0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
        0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
    };

    static constexpr uint8_t RCON[10] = {
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
    };

    static constexpr uint32_t rotWord(uint32_t w) noexcept {
        return (w << 8) | (w >> 24);
    }

    static constexpr uint32_t subWord(uint32_t w) noexcept {
        return (static_cast<uint32_t>(SBOX[(w >> 24) & 0xFF]) << 24) |
               (static_cast<uint32_t>(SBOX[(w >> 16) & 0xFF]) << 16) |
               (static_cast<uint32_t>(SBOX[(w >> 8) & 0xFF]) << 8)   |
               (static_cast<uint32_t>(SBOX[w & 0xFF]));
    }

    static constexpr uint8_t xtime(uint8_t a) noexcept {
        return static_cast<uint8_t>((a << 1) ^ ((a & 0x80) ? 0x1B : 0x00));
    }
    static constexpr uint8_t mul2(uint8_t a) noexcept { return xtime(a); }
    static constexpr uint8_t mul3(uint8_t a) noexcept { return xtime(a) ^ a; }
    static constexpr uint8_t mul9(uint8_t a) noexcept { return xtime(xtime(xtime(a))) ^ a; }
    static constexpr uint8_t mul11(uint8_t a) noexcept { return xtime(xtime(xtime(a)) ^ a) ^ a; }
    static constexpr uint8_t mul13(uint8_t a) noexcept { return xtime(xtime(xtime(a) ^ a)) ^ a; }
    static constexpr uint8_t mul14(uint8_t a) noexcept { return xtime(xtime(xtime(a) ^ a) ^ a); }

    void addRoundKey(uint8_t s[4][4], size_t round) const noexcept {
        for (int c = 0; c < 4; ++c) {
            uint32_t w = roundKeys_[round * 4 + c];
            s[0][c] ^= static_cast<uint8_t>((w >> 24) & 0xFF);
            s[1][c] ^= static_cast<uint8_t>((w >> 16) & 0xFF);
            s[2][c] ^= static_cast<uint8_t>((w >> 8) & 0xFF);
            s[3][c] ^= static_cast<uint8_t>(w & 0xFF);
        }
    }

    static void subBytes(uint8_t s[4][4]) noexcept {
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                s[r][c] = SBOX[s[r][c]];
            }
        }
    }

    static void invSubBytes(uint8_t s[4][4]) noexcept {
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                s[r][c] = INV_SBOX[s[r][c]];
            }
        }
    }

    static void shiftRows(uint8_t s[4][4]) noexcept {
        uint8_t t1 = s[1][0]; s[1][0] = s[1][1]; s[1][1] = s[1][2]; s[1][2] = s[1][3]; s[1][3] = t1;
        std::swap(s[2][0], s[2][2]); std::swap(s[2][1], s[2][3]);
        uint8_t t3 = s[3][3]; s[3][3] = s[3][2]; s[3][2] = s[3][1]; s[3][1] = s[3][0]; s[3][0] = t3;
    }

    static void invShiftRows(uint8_t s[4][4]) noexcept {
        uint8_t t1 = s[1][3]; s[1][3] = s[1][2]; s[1][2] = s[1][1]; s[1][1] = s[1][0]; s[1][0] = t1;
        std::swap(s[2][0], s[2][2]); std::swap(s[2][1], s[2][3]);
        uint8_t t3 = s[3][0]; s[3][0] = s[3][1]; s[3][1] = s[3][2]; s[3][2] = s[3][3]; s[3][3] = t3;
    }

    static void mixColumns(uint8_t s[4][4]) noexcept {
        for (int c = 0; c < 4; ++c) {
            uint8_t a0 = s[0][c], a1 = s[1][c], a2 = s[2][c], a3 = s[3][c];
            s[0][c] = mul2(a0) ^ mul3(a1) ^ a2 ^ a3;
            s[1][c] = a0 ^ mul2(a1) ^ mul3(a2) ^ a3;
            s[2][c] = a0 ^ a1 ^ mul2(a2) ^ mul3(a3);
            s[3][c] = mul3(a0) ^ a1 ^ a2 ^ mul2(a3);
        }
    }

    static void invMixColumns(uint8_t s[4][4]) noexcept {
        for (int c = 0; c < 4; ++c) {
            uint8_t a0 = s[0][c], a1 = s[1][c], a2 = s[2][c], a3 = s[3][c];
            s[0][c] = mul14(a0) ^ mul11(a1) ^ mul13(a2) ^ mul9(a3);
            s[1][c] = mul9(a0) ^ mul14(a1) ^ mul11(a2) ^ mul13(a3);
            s[2][c] = mul13(a0) ^ mul9(a1) ^ mul14(a2) ^ mul11(a3);
            s[3][c] = mul11(a0) ^ mul13(a1) ^ mul9(a2) ^ mul14(a3);
        }
    }

    static void incrementCounter(uint8_t counter[16]) noexcept {
        for (int i = 15; i >= 0; --i) {
            if (++counter[i] != 0) {
                break;
            }
        }
    }
};

// ============================================================================
// 6. Clean-Room CSPRNG (Cryptographically Secure Pseudo-Random Generator)
// ============================================================================
class Csprng {
public:
    static Csprng& get() {
        static Csprng instance;
        return instance;
    }

    void getBytes(std::span<uint8_t> buffer) {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureInitialized();

        size_t offset = 0;
        while (offset < buffer.size()) {
            // DRBG Keystream Block: HMAC-SHA256(seedKey, counter++)
            uint8_t counterBytes[8];
            for (int i = 0; i < 8; ++i) {
                counterBytes[7 - i] = static_cast<uint8_t>((counter_ >> (i * 8)) & 0xFF);
            }
            counter_++;

            auto block = HmacSha256::compute(seedKey_, std::span<const uint8_t>(counterBytes, 8));
            size_t copyBytes = std::min(buffer.size() - offset, block.size());
            std::memcpy(&buffer[offset], block.data(), copyBytes);
            offset += copyBytes;

            // Continuous entropy mixing
            if ((counter_ & 0xFF) == 0) {
                reseed();
            }
        }
    }

    uint32_t getUint32() {
        uint32_t val = 0;
        getBytes(std::span<uint8_t>(reinterpret_cast<uint8_t*>(&val), sizeof(val)));
        return val;
    }

    uint64_t getUint64() {
        uint64_t val = 0;
        getBytes(std::span<uint8_t>(reinterpret_cast<uint8_t*>(&val), sizeof(val)));
        return val;
    }

private:
    std::mutex mutex_;
    std::vector<uint8_t> seedKey_;
    uint64_t counter_{0};
    bool initialized_{false};

    Csprng() = default;

    void ensureInitialized() {
        if (!initialized_) {
            reseed();
            initialized_ = true;
        }
    }

    void reseed() {
        // Collect multi-dimensional entropy sources
        auto nowNano = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        auto steadyNano = std::chrono::steady_clock::now().time_since_epoch().count();
        uintptr_t stackAddr = reinterpret_cast<uintptr_t>(&nowNano);
        uintptr_t heapAddr  = reinterpret_cast<uintptr_t>(this);

        std::vector<uint8_t> entropyPool(sizeof(nowNano) + sizeof(steadyNano) + sizeof(stackAddr) + sizeof(heapAddr));
        size_t off = 0;
        std::memcpy(&entropyPool[off], &nowNano, sizeof(nowNano)); off += sizeof(nowNano);
        std::memcpy(&entropyPool[off], &steadyNano, sizeof(steadyNano)); off += sizeof(steadyNano);
        std::memcpy(&entropyPool[off], &stackAddr, sizeof(stackAddr)); off += sizeof(stackAddr);
        std::memcpy(&entropyPool[off], &heapAddr, sizeof(heapAddr));

        if (seedKey_.empty()) {
            seedKey_ = Sha256::hash(entropyPool);
        } else {
            // Fold new entropy into existing key state
            seedKey_ = HmacSha256::compute(seedKey_, entropyPool);
        }
        counter_++;
    }
};

// ============================================================================
// NIST SP 800-108 Counter Mode KDF over HMAC-SHA256
// K(i) = HMAC(Key, [i]_2 || Label || 0x00 || Context || [L]_2)
// ============================================================================
class Sp800108KdfCtr {
public:
    static std::vector<uint8_t> deriveKey(
        std::span<const uint8_t> key,
        std::span<const uint8_t> label,
        std::span<const uint8_t> context,
        uint32_t derivedKeyByteLength
    ) {
        if (derivedKeyByteLength == 0) return {};
        uint32_t L = derivedKeyByteLength * 8; // Bit length
        constexpr uint32_t h = 32; // HMAC-SHA256 output length
        uint32_t n = (derivedKeyByteLength + h - 1) / h;
        std::vector<uint8_t> derived;
        derived.reserve(n * h);

        for (uint32_t i = 1; i <= n; ++i) {
            std::vector<uint8_t> msg;
            msg.reserve(4 + label.size() + 1 + context.size() + 4);
            // [i]_2: 32-bit big-endian counter
            msg.push_back(static_cast<uint8_t>((i >> 24) & 0xFF));
            msg.push_back(static_cast<uint8_t>((i >> 16) & 0xFF));
            msg.push_back(static_cast<uint8_t>((i >> 8) & 0xFF));
            msg.push_back(static_cast<uint8_t>(i & 0xFF));

            // Label
            msg.insert(msg.end(), label.begin(), label.end());

            // 0x00 separator
            msg.push_back(0x00);

            // Context
            msg.insert(msg.end(), context.begin(), context.end());

            // [L]_2: 32-bit big-endian length in bits
            msg.push_back(static_cast<uint8_t>((L >> 24) & 0xFF));
            msg.push_back(static_cast<uint8_t>((L >> 16) & 0xFF));
            msg.push_back(static_cast<uint8_t>((L >> 8) & 0xFF));
            msg.push_back(static_cast<uint8_t>(L & 0xFF));

            auto ki = HmacSha256::compute(key, msg);
            derived.insert(derived.end(), ki.begin(), ki.end());
        }

        derived.resize(derivedKeyByteLength);
        return derived;
    }
};

// ============================================================================
// NIST SP 800-108 Feedback Mode KDF over HMAC-SHA256
// K(0) = IV (or empty)
// K(i) = HMAC(Key, K(i-1) || [i]_2 || Label || 0x00 || Context || [L]_2)
// ============================================================================
class Sp800108KdfFeedback {
public:
    static std::vector<uint8_t> deriveKey(
        std::span<const uint8_t> key,
        std::span<const uint8_t> label,
        std::span<const uint8_t> context,
        std::span<const uint8_t> iv,
        uint32_t derivedKeyByteLength
    ) {
        if (derivedKeyByteLength == 0) return {};
        uint32_t L = derivedKeyByteLength * 8; // Bit length
        constexpr uint32_t h = 32; // HMAC-SHA256 output length
        uint32_t n = (derivedKeyByteLength + h - 1) / h;
        std::vector<uint8_t> derived;
        derived.reserve(n * h);

        std::vector<uint8_t> kPrev(iv.begin(), iv.end());

        for (uint32_t i = 1; i <= n; ++i) {
            std::vector<uint8_t> msg;
            msg.reserve(kPrev.size() + 4 + label.size() + 1 + context.size() + 4);

            // K(i-1) feedback
            msg.insert(msg.end(), kPrev.begin(), kPrev.end());

            // [i]_2: 32-bit big-endian counter
            msg.push_back(static_cast<uint8_t>((i >> 24) & 0xFF));
            msg.push_back(static_cast<uint8_t>((i >> 16) & 0xFF));
            msg.push_back(static_cast<uint8_t>((i >> 8) & 0xFF));
            msg.push_back(static_cast<uint8_t>(i & 0xFF));

            // Label
            msg.insert(msg.end(), label.begin(), label.end());

            // 0x00 separator
            msg.push_back(0x00);

            // Context
            msg.insert(msg.end(), context.begin(), context.end());

            // [L]_2: 32-bit big-endian length in bits
            msg.push_back(static_cast<uint8_t>((L >> 24) & 0xFF));
            msg.push_back(static_cast<uint8_t>((L >> 16) & 0xFF));
            msg.push_back(static_cast<uint8_t>((L >> 8) & 0xFF));
            msg.push_back(static_cast<uint8_t>(L & 0xFF));

            kPrev = HmacSha256::compute(key, msg);
            derived.insert(derived.end(), kPrev.begin(), kPrev.end());
        }

        derived.resize(derivedKeyByteLength);
        return derived;
    }
};

class Sp800108KdfPipeline {
public:
    static std::vector<uint8_t> deriveKey(
        std::span<const uint8_t> key,
        std::span<const uint8_t> label,
        std::span<const uint8_t> context,
        uint32_t derivedKeyByteLength
    ) {
        if (derivedKeyByteLength == 0) return {};
        uint32_t L = derivedKeyByteLength * 8; // Bit length
        constexpr uint32_t h = 32; // HMAC-SHA256 output length
        uint32_t n = (derivedKeyByteLength + h - 1) / h;
        std::vector<uint8_t> derived;
        derived.reserve(n * h);

        // A(0) = Label || 0x00 || Context || [L]_2
        std::vector<uint8_t> aPrev;
        aPrev.reserve(label.size() + 1 + context.size() + 4);
        aPrev.insert(aPrev.end(), label.begin(), label.end());
        aPrev.push_back(0x00);
        aPrev.insert(aPrev.end(), context.begin(), context.end());
        aPrev.push_back(static_cast<uint8_t>((L >> 24) & 0xFF));
        aPrev.push_back(static_cast<uint8_t>((L >> 16) & 0xFF));
        aPrev.push_back(static_cast<uint8_t>((L >> 8) & 0xFF));
        aPrev.push_back(static_cast<uint8_t>(L & 0xFF));

        for (uint32_t i = 1; i <= n; ++i) {
            // A(i) = HMAC(Key, A(i-1))
            std::vector<uint8_t> aCur = HmacSha256::compute(key, aPrev);

            // K(i) = HMAC(Key, A(i) || [i]_2 || Label || 0x00 || Context || [L]_2)
            std::vector<uint8_t> kiMsg;
            kiMsg.reserve(aCur.size() + 4 + label.size() + 1 + context.size() + 4);
            kiMsg.insert(kiMsg.end(), aCur.begin(), aCur.end());

            // [i]_2: 32-bit big-endian counter
            kiMsg.push_back(static_cast<uint8_t>((i >> 24) & 0xFF));
            kiMsg.push_back(static_cast<uint8_t>((i >> 16) & 0xFF));
            kiMsg.push_back(static_cast<uint8_t>((i >> 8) & 0xFF));
            kiMsg.push_back(static_cast<uint8_t>(i & 0xFF));

            // Label
            kiMsg.insert(kiMsg.end(), label.begin(), label.end());

            // 0x00 separator
            kiMsg.push_back(0x00);

            // Context
            kiMsg.insert(kiMsg.end(), context.begin(), context.end());

            // [L]_2: 32-bit big-endian length in bits
            kiMsg.push_back(static_cast<uint8_t>((L >> 24) & 0xFF));
            kiMsg.push_back(static_cast<uint8_t>((L >> 16) & 0xFF));
            kiMsg.push_back(static_cast<uint8_t>((L >> 8) & 0xFF));
            kiMsg.push_back(static_cast<uint8_t>(L & 0xFF));

            auto ki = HmacSha256::compute(key, kiMsg);
            derived.insert(derived.end(), ki.begin(), ki.end());

            aPrev = std::move(aCur);
        }

        derived.resize(derivedKeyByteLength);
        return derived;
    }
};

// ============================================================================
// NIST SP 800-56A / SP 800-56C Single-Step Concatenation KDF over SHA-256
// K(i) = Hash([i]_2 || Z || OtherInfo)
// OtherInfo = AlgorithmID || PartyUInfo || PartyVInfo || SuppPubInfo || SuppPrivInfo
// ============================================================================
class Sp80056AConcatKdf {
public:
    static std::vector<uint8_t> deriveKey(
        std::span<const uint8_t> z,
        std::span<const uint8_t> algorithmId,
        std::span<const uint8_t> partyUInfo,
        std::span<const uint8_t> partyVInfo,
        std::span<const uint8_t> suppPubInfo,
        std::span<const uint8_t> suppPrivInfo,
        uint32_t derivedKeyByteLength
    ) {
        if (derivedKeyByteLength == 0) return {};
        constexpr uint32_t h = 32; // SHA-256 output length
        uint32_t n = (derivedKeyByteLength + h - 1) / h;
        std::vector<uint8_t> derived;
        derived.reserve(n * h);

        std::vector<uint8_t> otherInfo;
        otherInfo.reserve(algorithmId.size() + partyUInfo.size() + partyVInfo.size() +
                          suppPubInfo.size() + suppPrivInfo.size());
        otherInfo.insert(otherInfo.end(), algorithmId.begin(), algorithmId.end());
        otherInfo.insert(otherInfo.end(), partyUInfo.begin(), partyUInfo.end());
        otherInfo.insert(otherInfo.end(), partyVInfo.begin(), partyVInfo.end());
        otherInfo.insert(otherInfo.end(), suppPubInfo.begin(), suppPubInfo.end());
        otherInfo.insert(otherInfo.end(), suppPrivInfo.begin(), suppPrivInfo.end());

        for (uint32_t i = 1; i <= n; ++i) {
            std::vector<uint8_t> msg;
            msg.reserve(4 + z.size() + otherInfo.size());

            // [i]_2: 32-bit big-endian counter starting at 1
            msg.push_back(static_cast<uint8_t>((i >> 24) & 0xFF));
            msg.push_back(static_cast<uint8_t>((i >> 16) & 0xFF));
            msg.push_back(static_cast<uint8_t>((i >> 8) & 0xFF));
            msg.push_back(static_cast<uint8_t>(i & 0xFF));

            // Shared secret Z
            msg.insert(msg.end(), z.begin(), z.end());

            // OtherInfo
            msg.insert(msg.end(), otherInfo.begin(), otherInfo.end());

            auto ki = Sha256::hash(msg);
            derived.insert(derived.end(), ki.begin(), ki.end());
        }

        derived.resize(derivedKeyByteLength);
        return derived;
    }
};

class HkdfSha256 {
public:
    static constexpr size_t HASH_LEN = 32;

    static std::vector<uint8_t> extract(
        std::span<const uint8_t> salt,
        std::span<const uint8_t> ikm
    ) {
        if (salt.empty()) {
            uint8_t zeroSalt[HASH_LEN] = {0};
            return HmacSha256::compute(std::span<const uint8_t>(zeroSalt, HASH_LEN), ikm);
        }
        return HmacSha256::compute(salt, ikm);
    }

    static std::vector<uint8_t> expand(
        std::span<const uint8_t> prk,
        std::span<const uint8_t> info,
        uint32_t derivedKeyByteLength
    ) {
        if (derivedKeyByteLength == 0) return {};
        uint32_t n = (derivedKeyByteLength + HASH_LEN - 1) / HASH_LEN;
        if (n > 255) return {};

        std::vector<uint8_t> okm;
        okm.reserve(n * HASH_LEN);

        std::vector<uint8_t> prevT;

        for (uint32_t i = 1; i <= n; ++i) {
            std::vector<uint8_t> stepInput;
            stepInput.reserve(prevT.size() + info.size() + 1);
            if (!prevT.empty()) {
                stepInput.insert(stepInput.end(), prevT.begin(), prevT.end());
            }
            if (!info.empty()) {
                stepInput.insert(stepInput.end(), info.begin(), info.end());
            }
            stepInput.push_back(static_cast<uint8_t>(i & 0xFF));

            prevT = HmacSha256::compute(prk, stepInput);
            okm.insert(okm.end(), prevT.begin(), prevT.end());
        }

        okm.resize(derivedKeyByteLength);
        return okm;
    }

    static std::vector<uint8_t> deriveKey(
        std::span<const uint8_t> ikm,
        std::span<const uint8_t> salt,
        std::span<const uint8_t> info,
        uint32_t derivedKeyByteLength
    ) {
        auto prk = extract(salt, ikm);
        return expand(prk, info, derivedKeyByteLength);
    }
};

// ============================================================================
// 7. CNG Provider Objects & Key Storage Provider (KSP) Hierarchy
// ============================================================================

enum class CryptoAlgorithm {
    Sha256,
    Sha384,
    Sha512,
    Sha1,
    Md5,
    Aes,
    Rng,
    HmacSha256,
    HmacSha384,
    HmacSha512,
    AesCmac,
    Sp800108CtrHmac,
    Sp800108FeedbackHmac,
    Sp800108PipelineHmac,
    Sp80056AConcat,
    Hkdf,
    Sha3_256,
    Sha3_384,
    Sha3_512,
    Sha3_224,
    Shake128,
    Shake256,
    CShake128,
    CShake256,
    Kmac128,
    Kmac256,
    TupleHash128,
    TupleHash256,
    ParallelHash128,
    ParallelHash256,
    ParallelHashXOF128,
    ParallelHashXOF256,
    TupleHashXOF128,
    TupleHashXOF256,
    KmacXOF128,
    KmacXOF256,
    KmacPrf128,
    KmacPrf256,
    KmacRatchet128,
    KmacRatchet256,
    KmacDoubleRatchet128,
    KmacDoubleRatchet256,
    KmacTripleRatchet128,
    KmacTripleRatchet256,
    KmacTreeRatchet128,
    KmacTreeRatchet256,
    KmacAsyncRatchet128,
    KmacAsyncRatchet256,
    KmacKwp128,
    KmacKwp256,
    KmacAead128,
    KmacAead256,
    KmacMgf128,
    KmacMgf256,
    KmacKdf128,
    KmacKdf256,
    KmacCtr128,
    KmacCtr256,
    KmacOfb128,
    KmacOfb256,
    KmacCfb128,
    KmacCfb256,
    KmacSiv128,
    KmacSiv256,
    KmacFfx128,
    KmacFfx256,
    KmacBps128,
    KmacBps256,
    KmacKw128,
    KmacKw256,
    KmacDrbg128,
    KmacDrbg256,
    Ascon128,
    AsconHash,
    Ascon80pq,
    AsconXof,
    Ascon128a,
    AsconXofa,
    Unknown
};

inline CryptoAlgorithm parseAlgorithm(std::wstring_view algId) {
    if (algId == BCRYPT_ASCON128_ALGORITHM || algId == L"ASCON-128" || algId == L"ascon-128" || algId == L"Ascon-128") return CryptoAlgorithm::Ascon128;
    if (algId == BCRYPT_ASCON_HASH_ALGORITHM || algId == L"ASCON-HASH" || algId == L"ascon-hash" || algId == L"Ascon-Hash") return CryptoAlgorithm::AsconHash;
    if (algId == BCRYPT_ASCON80PQ_ALGORITHM || algId == L"ASCON-80PQ" || algId == L"ascon-80pq" || algId == L"Ascon-80pq") return CryptoAlgorithm::Ascon80pq;
    if (algId == BCRYPT_ASCON_XOF_ALGORITHM || algId == L"ASCON-XOF" || algId == L"ascon-xof" || algId == L"Ascon-Xof") return CryptoAlgorithm::AsconXof;
    if (algId == BCRYPT_ASCON128A_ALGORITHM || algId == L"ASCON-128A" || algId == L"ascon-128a" || algId == L"Ascon-128a") return CryptoAlgorithm::Ascon128a;
    if (algId == BCRYPT_ASCON_XOFA_ALGORITHM || algId == L"ASCON-XOFA" || algId == L"ascon-xofa" || algId == L"Ascon-Xofa") return CryptoAlgorithm::AsconXofa;
    if (algId == BCRYPT_SHA256_ALGORITHM || algId == L"SHA-256") return CryptoAlgorithm::Sha256;
    if (algId == BCRYPT_SHA384_ALGORITHM || algId == L"SHA-384") return CryptoAlgorithm::Sha384;
    if (algId == BCRYPT_SHA512_ALGORITHM || algId == L"SHA-512") return CryptoAlgorithm::Sha512;
    if (algId == BCRYPT_SHA3_256_ALGORITHM || algId == L"SHA3-256" || algId == L"sha3-256") return CryptoAlgorithm::Sha3_256;
    if (algId == BCRYPT_SHA3_384_ALGORITHM || algId == L"SHA3-384" || algId == L"sha3-384") return CryptoAlgorithm::Sha3_384;
    if (algId == BCRYPT_SHA3_512_ALGORITHM || algId == L"SHA3-512" || algId == L"sha3-512") return CryptoAlgorithm::Sha3_512;
    if (algId == BCRYPT_SHA3_224_ALGORITHM || algId == L"SHA3-224" || algId == L"sha3-224") return CryptoAlgorithm::Sha3_224;
    if (algId == BCRYPT_SHAKE128_ALGORITHM || algId == L"SHAKE128" || algId == L"shake128") return CryptoAlgorithm::Shake128;
    if (algId == BCRYPT_SHAKE256_ALGORITHM || algId == L"SHAKE256" || algId == L"shake256") return CryptoAlgorithm::Shake256;
    if (algId == BCRYPT_CSHAKE128_ALGORITHM || algId == L"CSHAKE128" || algId == L"cshake128") return CryptoAlgorithm::CShake128;
    if (algId == BCRYPT_CSHAKE256_ALGORITHM || algId == L"CSHAKE256" || algId == L"cshake256") return CryptoAlgorithm::CShake256;
    if (algId == BCRYPT_KMAC128_ALGORITHM || algId == L"KMAC128" || algId == L"kmac128") return CryptoAlgorithm::Kmac128;
    if (algId == BCRYPT_KMAC256_ALGORITHM || algId == L"KMAC256" || algId == L"kmac256") return CryptoAlgorithm::Kmac256;
    if (algId == BCRYPT_KMACXOF128_ALGORITHM || algId == L"KMACXOF128" || algId == L"kmacxof128") return CryptoAlgorithm::KmacXOF128;
    if (algId == BCRYPT_KMACXOF256_ALGORITHM || algId == L"KMACXOF256" || algId == L"kmacxof256") return CryptoAlgorithm::KmacXOF256;
    if (algId == BCRYPT_KMAC128_PRF_ALGORITHM || algId == L"KMAC128-PRF" || algId == L"kmac128-prf") return CryptoAlgorithm::KmacPrf128;
    if (algId == BCRYPT_KMAC256_PRF_ALGORITHM || algId == L"KMAC256-PRF" || algId == L"kmac256-prf") return CryptoAlgorithm::KmacPrf256;
    if (algId == BCRYPT_KMAC128_RATCHET_ALGORITHM || algId == L"KMAC128-RATCHET" || algId == L"kmac128-ratchet") return CryptoAlgorithm::KmacRatchet128;
    if (algId == BCRYPT_KMAC256_RATCHET_ALGORITHM || algId == L"KMAC256-RATCHET" || algId == L"kmac256-ratchet") return CryptoAlgorithm::KmacRatchet256;
    if (algId == BCRYPT_KMAC128_DOUBLERATCHET_ALGORITHM || algId == L"KMAC128-DOUBLERATCHET" || algId == L"kmac128-doubleratchet") return CryptoAlgorithm::KmacDoubleRatchet128;
    if (algId == BCRYPT_KMAC256_DOUBLERATCHET_ALGORITHM || algId == L"KMAC256-DOUBLERATCHET" || algId == L"kmac256-doubleratchet") return CryptoAlgorithm::KmacDoubleRatchet256;
    if (algId == BCRYPT_KMAC128_TRIPLERATCHET_ALGORITHM || algId == L"KMAC128-TRIPLERATCHET" || algId == L"kmac128-tripleratchet") return CryptoAlgorithm::KmacTripleRatchet128;
    if (algId == BCRYPT_KMAC256_TRIPLERATCHET_ALGORITHM || algId == L"KMAC256-TRIPLERATCHET" || algId == L"kmac256-tripleratchet") return CryptoAlgorithm::KmacTripleRatchet256;
    if (algId == BCRYPT_KMAC128_TREERATCHET_ALGORITHM || algId == L"KMAC128-TREERATCHET" || algId == L"kmac128-treeratchet") return CryptoAlgorithm::KmacTreeRatchet128;
    if (algId == BCRYPT_KMAC256_TREERATCHET_ALGORITHM || algId == L"KMAC256-TREERATCHET" || algId == L"kmac256-treeratchet") return CryptoAlgorithm::KmacTreeRatchet256;
    if (algId == BCRYPT_KMAC128_ASYNCRATCHET_ALGORITHM || algId == L"KMAC128-ASYNCRATCHET" || algId == L"kmac128-asyncratchet") return CryptoAlgorithm::KmacAsyncRatchet128;
    if (algId == BCRYPT_KMAC256_ASYNCRATCHET_ALGORITHM || algId == L"KMAC256-ASYNCRATCHET" || algId == L"kmac256-asyncratchet") return CryptoAlgorithm::KmacAsyncRatchet256;
    if (algId == BCRYPT_KMAC128_KWP_ALGORITHM || algId == L"KMAC128-KWP" || algId == L"kmac128-kwp") return CryptoAlgorithm::KmacKwp128;
    if (algId == BCRYPT_KMAC256_KWP_ALGORITHM || algId == L"KMAC256-KWP" || algId == L"kmac256-kwp") return CryptoAlgorithm::KmacKwp256;
    if (algId == BCRYPT_KMAC128_AEAD_ALGORITHM || algId == L"KMAC128-AEAD" || algId == L"kmac128-aead") return CryptoAlgorithm::KmacAead128;
    if (algId == BCRYPT_KMAC256_AEAD_ALGORITHM || algId == L"KMAC256-AEAD" || algId == L"kmac256-aead") return CryptoAlgorithm::KmacAead256;
    if (algId == BCRYPT_KMAC128_MGF_ALGORITHM || algId == L"KMAC128-MGF" || algId == L"kmac128-mgf") return CryptoAlgorithm::KmacMgf128;
    if (algId == BCRYPT_KMAC256_MGF_ALGORITHM || algId == L"KMAC256-MGF" || algId == L"kmac256-mgf") return CryptoAlgorithm::KmacMgf256;
    if (algId == BCRYPT_KMAC128_KDF_ALGORITHM || algId == L"KMAC128-KDF" || algId == L"kmac128-kdf") return CryptoAlgorithm::KmacKdf128;
    if (algId == BCRYPT_KMAC256_KDF_ALGORITHM || algId == L"KMAC256-KDF" || algId == L"kmac256-kdf") return CryptoAlgorithm::KmacKdf256;
    if (algId == BCRYPT_KMAC128_CTR_ALGORITHM || algId == L"KMAC128-CTR" || algId == L"kmac128-ctr") return CryptoAlgorithm::KmacCtr128;
    if (algId == BCRYPT_KMAC256_CTR_ALGORITHM || algId == L"KMAC256-CTR" || algId == L"kmac256-ctr") return CryptoAlgorithm::KmacCtr256;
    if (algId == BCRYPT_KMAC128_OFB_ALGORITHM || algId == L"KMAC128-OFB" || algId == L"kmac128-ofb") return CryptoAlgorithm::KmacOfb128;
    if (algId == BCRYPT_KMAC256_OFB_ALGORITHM || algId == L"KMAC256-OFB" || algId == L"kmac256-ofb") return CryptoAlgorithm::KmacOfb256;
    if (algId == BCRYPT_KMAC128_CFB_ALGORITHM || algId == L"KMAC128-CFB" || algId == L"kmac128-cfb") return CryptoAlgorithm::KmacCfb128;
    if (algId == BCRYPT_KMAC256_CFB_ALGORITHM || algId == L"KMAC256-CFB" || algId == L"kmac256-cfb") return CryptoAlgorithm::KmacCfb256;
    if (algId == BCRYPT_KMAC128_SIV_ALGORITHM || algId == L"KMAC128-SIV" || algId == L"kmac128-siv") return CryptoAlgorithm::KmacSiv128;
    if (algId == BCRYPT_KMAC256_SIV_ALGORITHM || algId == L"KMAC256-SIV" || algId == L"kmac256-siv") return CryptoAlgorithm::KmacSiv256;
    if (algId == BCRYPT_KMAC128_FFX_ALGORITHM || algId == L"KMAC128-FFX" || algId == L"kmac128-ffx") return CryptoAlgorithm::KmacFfx128;
    if (algId == BCRYPT_KMAC256_FFX_ALGORITHM || algId == L"KMAC256-FFX" || algId == L"kmac256-ffx") return CryptoAlgorithm::KmacFfx256;
    if (algId == BCRYPT_KMAC128_BPS_ALGORITHM || algId == L"KMAC128-BPS" || algId == L"kmac128-bps") return CryptoAlgorithm::KmacBps128;
    if (algId == BCRYPT_KMAC256_BPS_ALGORITHM || algId == L"KMAC256-BPS" || algId == L"kmac256-bps") return CryptoAlgorithm::KmacBps256;
    if (algId == BCRYPT_KMAC128_KW_ALGORITHM || algId == L"KMAC128-KW" || algId == L"kmac128-kw") return CryptoAlgorithm::KmacKw128;
    if (algId == BCRYPT_KMAC256_KW_ALGORITHM || algId == L"KMAC256-KW" || algId == L"kmac256-kw") return CryptoAlgorithm::KmacKw256;
    if (algId == BCRYPT_KMAC128_DRBG_ALGORITHM || algId == L"KMAC128-DRBG" || algId == L"kmac128-drbg") return CryptoAlgorithm::KmacDrbg128;
    if (algId == BCRYPT_KMAC256_DRBG_ALGORITHM || algId == L"KMAC256-DRBG" || algId == L"kmac256-drbg") return CryptoAlgorithm::KmacDrbg256;
    if (algId == BCRYPT_TUPLEHASH128_ALGORITHM || algId == L"TUPLEHASH128" || algId == L"tuplehash128") return CryptoAlgorithm::TupleHash128;
    if (algId == BCRYPT_TUPLEHASH256_ALGORITHM || algId == L"TUPLEHASH256" || algId == L"tuplehash256") return CryptoAlgorithm::TupleHash256;
    if (algId == BCRYPT_TUPLEHASHXOF128_ALGORITHM || algId == L"TUPLEHASHXOF128" || algId == L"tuplehashxof128") return CryptoAlgorithm::TupleHashXOF128;
    if (algId == BCRYPT_TUPLEHASHXOF256_ALGORITHM || algId == L"TUPLEHASHXOF256" || algId == L"tuplehashxof256") return CryptoAlgorithm::TupleHashXOF256;
    if (algId == BCRYPT_PARALLELHASH128_ALGORITHM || algId == L"PARALLELHASH128" || algId == L"parallelhash128") return CryptoAlgorithm::ParallelHash128;
    if (algId == BCRYPT_PARALLELHASH256_ALGORITHM || algId == L"PARALLELHASH256" || algId == L"parallelhash256") return CryptoAlgorithm::ParallelHash256;
    if (algId == BCRYPT_PARALLELHASHXOF128_ALGORITHM || algId == L"PARALLELHASHXOF128" || algId == L"parallelhashxof128") return CryptoAlgorithm::ParallelHashXOF128;
    if (algId == BCRYPT_PARALLELHASHXOF256_ALGORITHM || algId == L"PARALLELHASHXOF256" || algId == L"parallelhashxof256") return CryptoAlgorithm::ParallelHashXOF256;
    if (algId == BCRYPT_SHA1_ALGORITHM   || algId == L"SHA-1")   return CryptoAlgorithm::Sha1;
    if (algId == BCRYPT_MD5_ALGORITHM    || algId == L"md5")     return CryptoAlgorithm::Md5;
    if (algId == BCRYPT_AES_ALGORITHM)                           return CryptoAlgorithm::Aes;
    if (algId == BCRYPT_RNG_ALGORITHM)                           return CryptoAlgorithm::Rng;
    if (algId == BCRYPT_HMAC_SHA256_ALGORITHM)                  return CryptoAlgorithm::HmacSha256;
    if (algId == BCRYPT_HMAC_SHA384_ALGORITHM || algId == L"HMAC-SHA384") return CryptoAlgorithm::HmacSha384;
    if (algId == BCRYPT_HMAC_SHA512_ALGORITHM || algId == L"HMAC-SHA512") return CryptoAlgorithm::HmacSha512;
    if (algId == BCRYPT_AES_CMAC_ALGORITHM || algId == L"AES-CMAC") return CryptoAlgorithm::AesCmac;
    if (algId == BCRYPT_SP800108_CTR_HMAC_ALGORITHM || algId == L"SP800-108-CTR-HMAC" || algId == L"SP800_108_CTR_HMAC") return CryptoAlgorithm::Sp800108CtrHmac;
    if (algId == BCRYPT_SP800108_FEEDBACK_HMAC_ALGORITHM || algId == L"SP800-108-FEEDBACK-HMAC" || algId == L"SP800_108_FEEDBACK_HMAC") return CryptoAlgorithm::Sp800108FeedbackHmac;
    if (algId == BCRYPT_SP800108_PIPELINE_HMAC_ALGORITHM || algId == L"SP800-108-PIPELINE-HMAC" || algId == L"SP800_108_PIPELINE_HMAC") return CryptoAlgorithm::Sp800108PipelineHmac;
    if (algId == BCRYPT_SP80056A_CONCAT_ALGORITHM || algId == L"SP800-56A-CONCAT" || algId == L"SP800_56A_CONCAT") return CryptoAlgorithm::Sp80056AConcat;
    if (algId == BCRYPT_HKDF_ALGORITHM || algId == L"HKDF" || algId == L"hkdf") return CryptoAlgorithm::Hkdf;
    return CryptoAlgorithm::Unknown;
}

class CngAlgorithmProvider {
public:
    std::wstring algorithmId;
    CryptoAlgorithm algorithm{CryptoAlgorithm::Unknown};
    std::wstring chainingMode{BCRYPT_CHAIN_MODE_CBC};
    bool isHmac{false};

    explicit CngAlgorithmProvider(std::wstring_view id, uint32_t flags)
        : algorithmId(id),
          algorithm(parseAlgorithm(id)),
          isHmac((flags & BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
        if (isHmac) {
            if (algorithm == CryptoAlgorithm::Sha256) {
                algorithm = CryptoAlgorithm::HmacSha256;
            } else if (algorithm == CryptoAlgorithm::Sha384) {
                algorithm = CryptoAlgorithm::HmacSha384;
            } else if (algorithm == CryptoAlgorithm::Sha512) {
                algorithm = CryptoAlgorithm::HmacSha512;
            }
        }
    }
};

class CngKeyObject {
public:
    CryptoAlgorithm algorithm{CryptoAlgorithm::Aes};
    std::vector<uint8_t> secret;
    std::wstring chainingMode{BCRYPT_CHAIN_MODE_CBC};
    std::unique_ptr<Aes> aesEngine;
    std::unique_ptr<Aes> aesEngine2;

    CngKeyObject(CryptoAlgorithm alg, std::span<const uint8_t> keySecret, std::wstring_view mode)
        : algorithm(alg),
          secret(keySecret.begin(), keySecret.end()),
          chainingMode(mode) {
        if (algorithm == CryptoAlgorithm::Aes) {
            if (chainingMode == BCRYPT_CHAIN_MODE_XTS) {
                size_t half = secret.size() / 2;
                aesEngine = std::make_unique<Aes>(std::span<const uint8_t>(secret.data(), half));
                aesEngine2 = std::make_unique<Aes>(std::span<const uint8_t>(secret.data() + half, half));
            } else {
                aesEngine = std::make_unique<Aes>(secret);
            }
        }
    }
};

class CngHashObject {
public:
    CryptoAlgorithm algorithm{CryptoAlgorithm::Sha256};
    Sha256::Context sha256Ctx{};
    Sha384::Context sha384Ctx{};
    Sha512::Context sha512Ctx{};
    Sha1::Context sha1Ctx{};
    Md5::Context md5Ctx{};
    HmacSha256::Context hmacSha256Ctx{};
    HmacSha384::Context hmacSha384Ctx{};
    HmacSha512::Context hmacSha512Ctx{};
    Sha3_256::Context sha3_256Ctx{};
    Sha3_384::Context sha3_384Ctx{};
    Sha3_512::Context sha3_512Ctx{};
    Sha3_224::Context sha3_224Ctx{};
    Shake128::Context shake128Ctx{};
    Shake256::Context shake256Ctx{};
    CShake128::Context cshake128Ctx{};
    CShake256::Context cshake256Ctx{};
    Kmac128::Context   kmac128Ctx{};
    Kmac256::Context   kmac256Ctx{};
    TupleHash128::Context tupleHash128Ctx{};
    TupleHash256::Context tupleHash256Ctx{};
    ParallelHash128::Context parallelHash128Ctx{};
    ParallelHash256::Context parallelHash256Ctx{};
    ParallelHashXOF128::Context parallelHashXof128Ctx{};
    ParallelHashXOF256::Context parallelHashXof256Ctx{};
    TupleHashXOF128::Context tupleHashXof128Ctx{};
    TupleHashXOF256::Context tupleHashXof256Ctx{};
    KmacXOF128::Context kmacXof128Ctx{};
    KmacXOF256::Context kmacXof256Ctx{};
    KmacPrf128::Context kmacPrf128Ctx{};
    KmacPrf256::Context kmacPrf256Ctx{};
    KmacRatchet128::Context kmacRatchet128Ctx{};
    KmacRatchet256::Context kmacRatchet256Ctx{};
    KmacDoubleRatchet128::Context kmacDoubleRatchet128Ctx{};
    KmacDoubleRatchet256::Context kmacDoubleRatchet256Ctx{};
    KmacTripleRatchet128::Context kmacTripleRatchet128Ctx{};
    KmacTripleRatchet256::Context kmacTripleRatchet256Ctx{};
    KmacTreeRatchet128::Context kmacTreeRatchet128Ctx{};
    KmacTreeRatchet256::Context kmacTreeRatchet256Ctx{};
    KmacAsyncRatchet128::Context kmacAsyncRatchet128Ctx{};
    KmacAsyncRatchet256::Context kmacAsyncRatchet256Ctx{};
    KmacKwp128::Context kmacKwp128Ctx{};
    KmacKwp256::Context kmacKwp256Ctx{};
    KmacAead128::Context kmacAead128Ctx{};
    KmacAead256::Context kmacAead256Ctx{};
    KmacMgf128::Context kmacMgf128Ctx{};
    KmacMgf256::Context kmacMgf256Ctx{};
    KmacKdf128::Context kmacKdf128Ctx{};
    KmacKdf256::Context kmacKdf256Ctx{};
    KmacCtr128::Context kmacCtr128Ctx{};
    KmacCtr256::Context kmacCtr256Ctx{};
    KmacOfb128::Context kmacOfb128Ctx{};
    KmacOfb256::Context kmacOfb256Ctx{};
    KmacCfb128::Context kmacCfb128Ctx{};
    KmacCfb256::Context kmacCfb256Ctx{};
    KmacSiv128::Context kmacSiv128Ctx{};
    KmacSiv256::Context kmacSiv256Ctx{};
    KmacFfx128::Context kmacFfx128Ctx{};
    KmacFfx256::Context kmacFfx256Ctx{};
    KmacBps128::Context kmacBps128Ctx{};
    KmacBps256::Context kmacBps256Ctx{};
    KmacKw128::Context kmacKw128Ctx{};
    KmacKw256::Context kmacKw256Ctx{};
    KmacDrbg128::Context kmacDrbg128Ctx{};
    KmacDrbg256::Context kmacDrbg256Ctx{};
    Ascon128::Context ascon128Ctx{};
    AsconHash::Context asconHashCtx{};
    Ascon80pq::Context ascon80pqCtx{};
    AsconXof::Context asconXofCtx{};
    Ascon128a::Context ascon128aCtx{};
    AsconXofa::Context asconXofaCtx{};

    struct AesCmacContext {
        std::unique_ptr<Aes> aes;
        std::array<uint8_t, 16> k1{};
        std::array<uint8_t, 16> k2{};
        std::array<uint8_t, 16> X{};
        std::vector<uint8_t> buffer;
        size_t totalBytes{0};
    };
    std::unique_ptr<AesCmacContext> aesCmacCtx;

    bool isFinished{false};

    CngHashObject(const CngHashObject& other)
        : algorithm(other.algorithm),
          sha256Ctx(other.sha256Ctx),
          sha384Ctx(other.sha384Ctx),
          sha512Ctx(other.sha512Ctx),
          sha1Ctx(other.sha1Ctx),
          md5Ctx(other.md5Ctx),
          hmacSha256Ctx(other.hmacSha256Ctx),
          hmacSha384Ctx(other.hmacSha384Ctx),
          hmacSha512Ctx(other.hmacSha512Ctx),
          sha3_256Ctx(other.sha3_256Ctx),
          sha3_384Ctx(other.sha3_384Ctx),
          sha3_512Ctx(other.sha3_512Ctx),
          sha3_224Ctx(other.sha3_224Ctx),
          shake128Ctx(other.shake128Ctx),
          shake256Ctx(other.shake256Ctx),
          cshake128Ctx(other.cshake128Ctx),
          cshake256Ctx(other.cshake256Ctx),
          kmac128Ctx(other.kmac128Ctx),
          kmac256Ctx(other.kmac256Ctx),
          tupleHash128Ctx(other.tupleHash128Ctx),
          tupleHash256Ctx(other.tupleHash256Ctx),
          parallelHash128Ctx(other.parallelHash128Ctx),
          parallelHash256Ctx(other.parallelHash256Ctx),
          parallelHashXof128Ctx(other.parallelHashXof128Ctx),
          parallelHashXof256Ctx(other.parallelHashXof256Ctx),
          tupleHashXof128Ctx(other.tupleHashXof128Ctx),
          tupleHashXof256Ctx(other.tupleHashXof256Ctx),
          kmacXof128Ctx(other.kmacXof128Ctx),
          kmacXof256Ctx(other.kmacXof256Ctx),
          kmacPrf128Ctx(other.kmacPrf128Ctx),
          kmacPrf256Ctx(other.kmacPrf256Ctx),
          kmacRatchet128Ctx(other.kmacRatchet128Ctx),
          kmacRatchet256Ctx(other.kmacRatchet256Ctx),
          kmacDoubleRatchet128Ctx(other.kmacDoubleRatchet128Ctx),
          kmacDoubleRatchet256Ctx(other.kmacDoubleRatchet256Ctx),
          kmacTripleRatchet128Ctx(other.kmacTripleRatchet128Ctx),
          kmacTripleRatchet256Ctx(other.kmacTripleRatchet256Ctx),
          kmacTreeRatchet128Ctx(other.kmacTreeRatchet128Ctx),
          kmacTreeRatchet256Ctx(other.kmacTreeRatchet256Ctx),
          kmacAsyncRatchet128Ctx(other.kmacAsyncRatchet128Ctx),
          kmacAsyncRatchet256Ctx(other.kmacAsyncRatchet256Ctx),
          kmacKwp128Ctx(other.kmacKwp128Ctx),
          kmacKwp256Ctx(other.kmacKwp256Ctx),
          kmacAead128Ctx(other.kmacAead128Ctx),
          kmacAead256Ctx(other.kmacAead256Ctx),
          kmacMgf128Ctx(other.kmacMgf128Ctx),
          kmacMgf256Ctx(other.kmacMgf256Ctx),
          kmacKdf128Ctx(other.kmacKdf128Ctx),
          kmacKdf256Ctx(other.kmacKdf256Ctx),
          kmacCtr128Ctx(other.kmacCtr128Ctx),
          kmacCtr256Ctx(other.kmacCtr256Ctx),
          kmacOfb128Ctx(other.kmacOfb128Ctx),
          kmacOfb256Ctx(other.kmacOfb256Ctx),
          kmacCfb128Ctx(other.kmacCfb128Ctx),
          kmacCfb256Ctx(other.kmacCfb256Ctx),
          kmacSiv128Ctx(other.kmacSiv128Ctx),
          kmacSiv256Ctx(other.kmacSiv256Ctx),
          kmacFfx128Ctx(other.kmacFfx128Ctx),
          kmacFfx256Ctx(other.kmacFfx256Ctx),
          kmacBps128Ctx(other.kmacBps128Ctx),
          kmacBps256Ctx(other.kmacBps256Ctx),
          kmacKw128Ctx(other.kmacKw128Ctx),
          kmacKw256Ctx(other.kmacKw256Ctx),
          kmacDrbg128Ctx(other.kmacDrbg128Ctx),
          kmacDrbg256Ctx(other.kmacDrbg256Ctx),
          ascon128Ctx(other.ascon128Ctx),
          asconHashCtx(other.asconHashCtx),
          ascon80pqCtx(other.ascon80pqCtx),
          asconXofCtx(other.asconXofCtx),
          ascon128aCtx(other.ascon128aCtx),
          asconXofaCtx(other.asconXofaCtx),
          isFinished(other.isFinished) {
        if (other.aesCmacCtx) {
            aesCmacCtx = std::make_unique<AesCmacContext>();
            if (other.aesCmacCtx->aes) {
                aesCmacCtx->aes = std::make_unique<Aes>(*other.aesCmacCtx->aes);
            }
            aesCmacCtx->k1 = other.aesCmacCtx->k1;
            aesCmacCtx->k2 = other.aesCmacCtx->k2;
            aesCmacCtx->X = other.aesCmacCtx->X;
            aesCmacCtx->buffer = other.aesCmacCtx->buffer;
            aesCmacCtx->totalBytes = other.aesCmacCtx->totalBytes;
        }
    }

    CngHashObject(CryptoAlgorithm alg, std::span<const uint8_t> key = {})
        : algorithm(alg) {
        if (algorithm == CryptoAlgorithm::Sha256) {
            Sha256::init(sha256Ctx);
        } else if (algorithm == CryptoAlgorithm::Sha384) {
            Sha384::init(sha384Ctx);
        } else if (algorithm == CryptoAlgorithm::Sha512) {
            Sha512::init(sha512Ctx);
        } else if (algorithm == CryptoAlgorithm::Sha1) {
            Sha1::init(sha1Ctx);
        } else if (algorithm == CryptoAlgorithm::Md5) {
            Md5::init(md5Ctx);
        } else if (algorithm == CryptoAlgorithm::HmacSha256) {
            HmacSha256::init(hmacSha256Ctx, key);
        } else if (algorithm == CryptoAlgorithm::HmacSha384) {
            HmacSha384::init(hmacSha384Ctx, key);
        } else if (algorithm == CryptoAlgorithm::HmacSha512) {
            HmacSha512::init(hmacSha512Ctx, key);
        } else if (algorithm == CryptoAlgorithm::Sha3_256) {
            Sha3_256::init(sha3_256Ctx);
        } else if (algorithm == CryptoAlgorithm::Sha3_384) {
            Sha3_384::init(sha3_384Ctx);
        } else if (algorithm == CryptoAlgorithm::Sha3_512) {
            Sha3_512::init(sha3_512Ctx);
        } else if (algorithm == CryptoAlgorithm::Sha3_224) {
            Sha3_224::init(sha3_224Ctx);
        } else if (algorithm == CryptoAlgorithm::Shake128) {
            Shake128::init(shake128Ctx);
        } else if (algorithm == CryptoAlgorithm::Shake256) {
            Shake256::init(shake256Ctx);
        } else if (algorithm == CryptoAlgorithm::CShake128) {
            CShake128::init(cshake128Ctx);
        } else if (algorithm == CryptoAlgorithm::CShake256) {
            CShake256::init(cshake256Ctx);
        } else if (algorithm == CryptoAlgorithm::Kmac128) {
            Kmac128::init(kmac128Ctx, key);
        } else if (algorithm == CryptoAlgorithm::Kmac256) {
            Kmac256::init(kmac256Ctx, key);
        } else if (algorithm == CryptoAlgorithm::TupleHash128) {
            TupleHash128::init(tupleHash128Ctx);
        } else if (algorithm == CryptoAlgorithm::TupleHash256) {
            TupleHash256::init(tupleHash256Ctx);
        } else if (algorithm == CryptoAlgorithm::ParallelHash128) {
            ParallelHash128::init(parallelHash128Ctx);
        } else if (algorithm == CryptoAlgorithm::ParallelHash256) {
            ParallelHash256::init(parallelHash256Ctx);
        } else if (algorithm == CryptoAlgorithm::ParallelHashXOF128) {
            ParallelHashXOF128::init(parallelHashXof128Ctx);
        } else if (algorithm == CryptoAlgorithm::ParallelHashXOF256) {
            ParallelHashXOF256::init(parallelHashXof256Ctx);
        } else if (algorithm == CryptoAlgorithm::TupleHashXOF128) {
            TupleHashXOF128::init(tupleHashXof128Ctx);
        } else if (algorithm == CryptoAlgorithm::TupleHashXOF256) {
            TupleHashXOF256::init(tupleHashXof256Ctx);
        } else if (algorithm == CryptoAlgorithm::KmacXOF128) {
            KmacXOF128::init(kmacXof128Ctx, key);
        } else if (algorithm == CryptoAlgorithm::KmacXOF256) {
            KmacXOF256::init(kmacXof256Ctx, key);
        } else if (algorithm == CryptoAlgorithm::KmacPrf128) {
            KmacPrf128::init(kmacPrf128Ctx, key, "KDF");
        } else if (algorithm == CryptoAlgorithm::KmacPrf256) {
            KmacPrf256::init(kmacPrf256Ctx, key, "KDF");
        } else if (algorithm == CryptoAlgorithm::KmacRatchet128) {
            KmacRatchet128::init(kmacRatchet128Ctx, key, "RATCHET");
        } else if (algorithm == CryptoAlgorithm::KmacRatchet256) {
            KmacRatchet256::init(kmacRatchet256Ctx, key, "RATCHET");
        } else if (algorithm == CryptoAlgorithm::KmacDoubleRatchet128) {
            KmacDoubleRatchet128::init(kmacDoubleRatchet128Ctx, key, "DOUBLE_RATCHET");
        } else if (algorithm == CryptoAlgorithm::KmacDoubleRatchet256) {
            KmacDoubleRatchet256::init(kmacDoubleRatchet256Ctx, key, "DOUBLE_RATCHET");
        } else if (algorithm == CryptoAlgorithm::KmacTripleRatchet128) {
            KmacTripleRatchet128::init(kmacTripleRatchet128Ctx, key, "TRIPLE_RATCHET");
        } else if (algorithm == CryptoAlgorithm::KmacTripleRatchet256) {
            KmacTripleRatchet256::init(kmacTripleRatchet256Ctx, key, "TRIPLE_RATCHET");
        } else if (algorithm == CryptoAlgorithm::KmacTreeRatchet128) {
            KmacTreeRatchet128::init(kmacTreeRatchet128Ctx, key, 4, "TREE_RATCHET");
        } else if (algorithm == CryptoAlgorithm::KmacTreeRatchet256) {
            KmacTreeRatchet256::init(kmacTreeRatchet256Ctx, key, 4, "TREE_RATCHET");
        } else if (algorithm == CryptoAlgorithm::KmacAsyncRatchet128) {
            KmacAsyncRatchet128::init(kmacAsyncRatchet128Ctx, key, "ASYNC_RATCHET");
        } else if (algorithm == CryptoAlgorithm::KmacAsyncRatchet256) {
            KmacAsyncRatchet256::init(kmacAsyncRatchet256Ctx, key, "ASYNC_RATCHET");
        } else if (algorithm == CryptoAlgorithm::KmacKwp128) {
            KmacKwp128::init(kmacKwp128Ctx, key, "KMAC128_KWP");
        } else if (algorithm == CryptoAlgorithm::KmacKwp256) {
            KmacKwp256::init(kmacKwp256Ctx, key, "KMAC256_KWP");
        } else if (algorithm == CryptoAlgorithm::KmacAead128) {
            KmacAead128::init(kmacAead128Ctx, key, "KMAC128_AEAD");
        } else if (algorithm == CryptoAlgorithm::KmacAead256) {
            KmacAead256::init(kmacAead256Ctx, key, "KMAC256_AEAD");
        } else if (algorithm == CryptoAlgorithm::KmacMgf128) {
            KmacMgf128::init(kmacMgf128Ctx, key, "KMAC128_MGF");
        } else if (algorithm == CryptoAlgorithm::KmacMgf256) {
            KmacMgf256::init(kmacMgf256Ctx, key, "KMAC256_MGF");
        } else if (algorithm == CryptoAlgorithm::KmacKdf128) {
            KmacKdf128::init(kmacKdf128Ctx, key, "KMAC128_KDF");
        } else if (algorithm == CryptoAlgorithm::KmacKdf256) {
            KmacKdf256::init(kmacKdf256Ctx, key, "KMAC256_KDF");
        } else if (algorithm == CryptoAlgorithm::KmacCtr128) {
            KmacCtr128::init(kmacCtr128Ctx, key, "KMAC128_CTR");
        } else if (algorithm == CryptoAlgorithm::KmacCtr256) {
            KmacCtr256::init(kmacCtr256Ctx, key, "KMAC256_CTR");
        } else if (algorithm == CryptoAlgorithm::KmacOfb128) {
            KmacOfb128::init(kmacOfb128Ctx, key, "KMAC128_OFB");
        } else if (algorithm == CryptoAlgorithm::KmacOfb256) {
            KmacOfb256::init(kmacOfb256Ctx, key, "KMAC256_OFB");
        } else if (algorithm == CryptoAlgorithm::KmacCfb128) {
            KmacCfb128::init(kmacCfb128Ctx, key, "KMAC128_CFB");
        } else if (algorithm == CryptoAlgorithm::KmacCfb256) {
            KmacCfb256::init(kmacCfb256Ctx, key, "KMAC256_CFB");
        } else if (algorithm == CryptoAlgorithm::KmacSiv128) {
            KmacSiv128::init(kmacSiv128Ctx, key, "KMAC128_SIV");
        } else if (algorithm == CryptoAlgorithm::KmacSiv256) {
            KmacSiv256::init(kmacSiv256Ctx, key, "KMAC256_SIV");
        } else if (algorithm == CryptoAlgorithm::KmacFfx128) {
            KmacFfx128::init(kmacFfx128Ctx, key, {}, "KMAC128_FFX");
        } else if (algorithm == CryptoAlgorithm::KmacFfx256) {
            KmacFfx256::init(kmacFfx256Ctx, key, {}, "KMAC256_FFX");
        } else if (algorithm == CryptoAlgorithm::KmacBps128) {
            KmacBps128::init(kmacBps128Ctx, key, {}, "KMAC128_BPS");
        } else if (algorithm == CryptoAlgorithm::KmacBps256) {
            KmacBps256::init(kmacBps256Ctx, key, {}, "KMAC256_BPS");
        } else if (algorithm == CryptoAlgorithm::KmacKw128) {
            KmacKw128::init(kmacKw128Ctx, key, {}, "KMAC128_KW");
        } else if (algorithm == CryptoAlgorithm::KmacKw256) {
            KmacKw256::init(kmacKw256Ctx, key, {}, "KMAC256_KW");
        } else if (algorithm == CryptoAlgorithm::KmacDrbg128) {
            KmacDrbg128::init(kmacDrbg128Ctx, key, {}, "KMAC128_DRBG");
        } else if (algorithm == CryptoAlgorithm::KmacDrbg256) {
            KmacDrbg256::init(kmacDrbg256Ctx, key, {}, "KMAC256_DRBG");
        } else if (algorithm == CryptoAlgorithm::Ascon128) {
            Ascon128::init(ascon128Ctx, key);
        } else if (algorithm == CryptoAlgorithm::AsconHash) {
            AsconHash::init(asconHashCtx);
        } else if (algorithm == CryptoAlgorithm::Ascon80pq) {
            Ascon80pq::init(ascon80pqCtx, key);
        } else if (algorithm == CryptoAlgorithm::AsconXof) {
            AsconXof::init(asconXofCtx);
        } else if (algorithm == CryptoAlgorithm::Ascon128a) {
            Ascon128a::init(ascon128aCtx, key);
        } else if (algorithm == CryptoAlgorithm::AsconXofa) {
            AsconXofa::init(asconXofaCtx);
        } else if (algorithm == CryptoAlgorithm::AesCmac) {
            aesCmacCtx = std::make_unique<AesCmacContext>();
            if (key.size() == 16 || key.size() == 24 || key.size() == 32) {
                aesCmacCtx->aes = std::make_unique<Aes>(key);
                aesCmacCtx->aes->generateCmacSubkeys(aesCmacCtx->k1, aesCmacCtx->k2);
            }
            aesCmacCtx->X.fill(0);
        }
    }

    void update(std::span<const uint8_t> data) {
        if (isFinished) return;
        if (algorithm == CryptoAlgorithm::Sha256) {
            Sha256::update(sha256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Sha384) {
            Sha384::update(sha384Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Sha512) {
            Sha512::update(sha512Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Sha1) {
            Sha1::update(sha1Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Md5) {
            Md5::update(md5Ctx, data);
        } else if (algorithm == CryptoAlgorithm::HmacSha256) {
            HmacSha256::update(hmacSha256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::HmacSha384) {
            HmacSha384::update(hmacSha384Ctx, data);
        } else if (algorithm == CryptoAlgorithm::HmacSha512) {
            HmacSha512::update(hmacSha512Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Sha3_256) {
            Sha3_256::update(sha3_256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Sha3_384) {
            Sha3_384::update(sha3_384Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Sha3_512) {
            Sha3_512::update(sha3_512Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Sha3_224) {
            Sha3_224::update(sha3_224Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Shake128) {
            Shake128::update(shake128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Shake256) {
            Shake256::update(shake256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::CShake128) {
            CShake128::update(cshake128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::CShake256) {
            CShake256::update(cshake256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Kmac128) {
            Kmac128::update(kmac128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Kmac256) {
            Kmac256::update(kmac256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::TupleHash128) {
            TupleHash128::update(tupleHash128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::TupleHash256) {
            TupleHash256::update(tupleHash256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::ParallelHash128) {
            ParallelHash128::update(parallelHash128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::ParallelHash256) {
            ParallelHash256::update(parallelHash256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::ParallelHashXOF128) {
            ParallelHashXOF128::update(parallelHashXof128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::ParallelHashXOF256) {
            ParallelHashXOF256::update(parallelHashXof256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::TupleHashXOF128) {
            TupleHashXOF128::update(tupleHashXof128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::TupleHashXOF256) {
            TupleHashXOF256::update(tupleHashXof256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacXOF128) {
            KmacXOF128::update(kmacXof128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacXOF256) {
            KmacXOF256::update(kmacXof256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacPrf128) {
            KmacPrf128::update(kmacPrf128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacPrf256) {
            KmacPrf256::update(kmacPrf256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacRatchet128) {
            KmacRatchet128::update(kmacRatchet128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacRatchet256) {
            KmacRatchet256::update(kmacRatchet256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacDoubleRatchet128) {
            KmacDoubleRatchet128::update(kmacDoubleRatchet128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacDoubleRatchet256) {
            KmacDoubleRatchet256::update(kmacDoubleRatchet256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacTripleRatchet128) {
            KmacTripleRatchet128::update(kmacTripleRatchet128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacTripleRatchet256) {
            KmacTripleRatchet256::update(kmacTripleRatchet256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacTreeRatchet128) {
            KmacTreeRatchet128::update(kmacTreeRatchet128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacTreeRatchet256) {
            KmacTreeRatchet256::update(kmacTreeRatchet256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacAsyncRatchet128) {
            KmacAsyncRatchet128::update(kmacAsyncRatchet128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacAsyncRatchet256) {
            KmacAsyncRatchet256::update(kmacAsyncRatchet256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacKwp128) {
            KmacKwp128::update(kmacKwp128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacKwp256) {
            KmacKwp256::update(kmacKwp256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacAead128) {
            KmacAead128::update(kmacAead128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacAead256) {
            KmacAead256::update(kmacAead256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacMgf128) {
            KmacMgf128::update(kmacMgf128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacMgf256) {
            KmacMgf256::update(kmacMgf256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacKdf128) {
            KmacKdf128::update(kmacKdf128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacKdf256) {
            KmacKdf256::update(kmacKdf256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacCtr128) {
            KmacCtr128::update(kmacCtr128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacCtr256) {
            KmacCtr256::update(kmacCtr256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacOfb128) {
            KmacOfb128::update(kmacOfb128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacOfb256) {
            KmacOfb256::update(kmacOfb256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacCfb128) {
            KmacCfb128::update(kmacCfb128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacCfb256) {
            KmacCfb256::update(kmacCfb256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacSiv128) {
            KmacSiv128::update(kmacSiv128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacSiv256) {
            KmacSiv256::update(kmacSiv256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacFfx128) {
            KmacFfx128::update(kmacFfx128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacFfx256) {
            KmacFfx256::update(kmacFfx256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacBps128) {
            KmacBps128::update(kmacBps128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacBps256) {
            KmacBps256::update(kmacBps256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacKw128) {
            KmacKw128::update(kmacKw128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacKw256) {
            KmacKw256::update(kmacKw256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacDrbg128) {
            KmacDrbg128::update(kmacDrbg128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::KmacDrbg256) {
            KmacDrbg256::update(kmacDrbg256Ctx, data);
        } else if (algorithm == CryptoAlgorithm::Ascon128) {
            Ascon128::update(ascon128Ctx, data);
        } else if (algorithm == CryptoAlgorithm::AsconHash) {
            AsconHash::update(asconHashCtx, data);
        } else if (algorithm == CryptoAlgorithm::Ascon80pq) {
            Ascon80pq::update(ascon80pqCtx, data);
        } else if (algorithm == CryptoAlgorithm::AsconXof) {
            AsconXof::update(asconXofCtx, data);
        } else if (algorithm == CryptoAlgorithm::Ascon128a) {
            Ascon128a::update(ascon128aCtx, data);
        } else if (algorithm == CryptoAlgorithm::AsconXofa) {
            AsconXofa::update(asconXofaCtx, data);
        } else if (algorithm == CryptoAlgorithm::AesCmac && aesCmacCtx && aesCmacCtx->aes) {
            if (data.empty()) return;
            aesCmacCtx->totalBytes += data.size();
            size_t offset = 0;
            while (aesCmacCtx->buffer.size() + (data.size() - offset) > 16) {
                size_t needed = 16 - aesCmacCtx->buffer.size();
                aesCmacCtx->buffer.insert(aesCmacCtx->buffer.end(), data.begin() + offset, data.begin() + offset + needed);
                offset += needed;

                std::array<uint8_t, 16> Y{};
                for (size_t j = 0; j < 16; ++j) {
                    Y[j] = aesCmacCtx->X[j] ^ aesCmacCtx->buffer[j];
                }
                aesCmacCtx->aes->encryptBlock(Y.data(), aesCmacCtx->X.data());
                aesCmacCtx->buffer.clear();
            }
            if (offset < data.size()) {
                aesCmacCtx->buffer.insert(aesCmacCtx->buffer.end(), data.begin() + offset, data.end());
            }
        }
    }

    std::vector<uint8_t> finish() {
        if (isFinished) return {};
        isFinished = true;
        if (algorithm == CryptoAlgorithm::Sha256) {
            std::vector<uint8_t> out(32);
            Sha256::final(sha256Ctx, std::span<uint8_t, 32>(out.data(), 32));
            return out;
        } else if (algorithm == CryptoAlgorithm::Sha384) {
            std::vector<uint8_t> out(48);
            Sha384::final(sha384Ctx, std::span<uint8_t, 48>(out.data(), 48));
            return out;
        } else if (algorithm == CryptoAlgorithm::Sha512) {
            std::vector<uint8_t> out(64);
            Sha512::final(sha512Ctx, std::span<uint8_t, 64>(out.data(), 64));
            return out;
        } else if (algorithm == CryptoAlgorithm::Sha1) {
            std::vector<uint8_t> out(20);
            Sha1::final(sha1Ctx, std::span<uint8_t, 20>(out.data(), 20));
            return out;
        } else if (algorithm == CryptoAlgorithm::Md5) {
            std::vector<uint8_t> out(16);
            Md5::final(md5Ctx, std::span<uint8_t, 16>(out.data(), 16));
            return out;
        } else if (algorithm == CryptoAlgorithm::HmacSha256) {
            std::vector<uint8_t> out(32);
            HmacSha256::final(hmacSha256Ctx, std::span<uint8_t, 32>(out.data(), 32));
            return out;
        } else if (algorithm == CryptoAlgorithm::HmacSha384) {
            std::vector<uint8_t> out(48);
            HmacSha384::final(hmacSha384Ctx, std::span<uint8_t, 48>(out.data(), 48));
            return out;
        } else if (algorithm == CryptoAlgorithm::HmacSha512) {
            std::vector<uint8_t> out(64);
            HmacSha512::final(hmacSha512Ctx, std::span<uint8_t, 64>(out.data(), 64));
            return out;
        } else if (algorithm == CryptoAlgorithm::Sha3_256) {
            std::vector<uint8_t> out(32);
            Sha3_256::final(sha3_256Ctx, std::span<uint8_t, 32>(out.data(), 32));
            return out;
        } else if (algorithm == CryptoAlgorithm::Sha3_384) {
            std::vector<uint8_t> out(48);
            Sha3_384::final(sha3_384Ctx, std::span<uint8_t, 48>(out.data(), 48));
            return out;
        } else if (algorithm == CryptoAlgorithm::Sha3_512) {
            std::vector<uint8_t> out(64);
            Sha3_512::final(sha3_512Ctx, std::span<uint8_t, 64>(out.data(), 64));
            return out;
        } else if (algorithm == CryptoAlgorithm::Sha3_224) {
            std::vector<uint8_t> out(28);
            Sha3_224::final(sha3_224Ctx, std::span<uint8_t, 28>(out.data(), 28));
            return out;
        } else if (algorithm == CryptoAlgorithm::Shake128) {
            std::vector<uint8_t> out(32);
            Shake128::squeeze(shake128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::Shake256) {
            std::vector<uint8_t> out(64);
            Shake256::squeeze(shake256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::CShake128) {
            std::vector<uint8_t> out(32);
            CShake128::squeeze(cshake128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::CShake256) {
            std::vector<uint8_t> out(64);
            CShake256::squeeze(cshake256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::Kmac128) {
            std::vector<uint8_t> out(32);
            Kmac128::final(kmac128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::Kmac256) {
            std::vector<uint8_t> out(64);
            Kmac256::final(kmac256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::TupleHash128) {
            std::vector<uint8_t> out(32);
            TupleHash128::final(tupleHash128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::TupleHash256) {
            std::vector<uint8_t> out(64);
            TupleHash256::final(tupleHash256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::ParallelHash128) {
            std::vector<uint8_t> out(32);
            ParallelHash128::final(parallelHash128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::ParallelHash256) {
            std::vector<uint8_t> out(64);
            ParallelHash256::final(parallelHash256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::ParallelHashXOF128) {
            std::vector<uint8_t> out(32);
            ParallelHashXOF128::final(parallelHashXof128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::ParallelHashXOF256) {
            std::vector<uint8_t> out(64);
            ParallelHashXOF256::final(parallelHashXof256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::TupleHashXOF128) {
            std::vector<uint8_t> out(32);
            TupleHashXOF128::final(tupleHashXof128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::TupleHashXOF256) {
            std::vector<uint8_t> out(64);
            TupleHashXOF256::final(tupleHashXof256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacXOF128) {
            std::vector<uint8_t> out(32);
            KmacXOF128::final(kmacXof128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacXOF256) {
            std::vector<uint8_t> out(64);
            KmacXOF256::final(kmacXof256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacPrf128) {
            std::vector<uint8_t> out(32);
            KmacPrf128::final(kmacPrf128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacPrf256) {
            std::vector<uint8_t> out(64);
            KmacPrf256::final(kmacPrf256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacRatchet128) {
            std::vector<uint8_t> out(32);
            KmacRatchet128::final(kmacRatchet128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacRatchet256) {
            std::vector<uint8_t> out(64);
            KmacRatchet256::final(kmacRatchet256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacDoubleRatchet128) {
            std::vector<uint8_t> out(32);
            KmacDoubleRatchet128::final(kmacDoubleRatchet128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacDoubleRatchet256) {
            std::vector<uint8_t> out(64);
            KmacDoubleRatchet256::final(kmacDoubleRatchet256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacTripleRatchet128) {
            std::vector<uint8_t> out(32);
            KmacTripleRatchet128::final(kmacTripleRatchet128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacTripleRatchet256) {
            std::vector<uint8_t> out(64);
            KmacTripleRatchet256::final(kmacTripleRatchet256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacTreeRatchet128) {
            std::vector<uint8_t> out(32);
            KmacTreeRatchet128::final(kmacTreeRatchet128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacTreeRatchet256) {
            std::vector<uint8_t> out(64);
            KmacTreeRatchet256::final(kmacTreeRatchet256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacAsyncRatchet128) {
            std::vector<uint8_t> out(32);
            KmacAsyncRatchet128::final(kmacAsyncRatchet128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacAsyncRatchet256) {
            std::vector<uint8_t> out(64);
            KmacAsyncRatchet256::final(kmacAsyncRatchet256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacKwp128) {
            std::vector<uint8_t> out(32);
            KmacKwp128::final(kmacKwp128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacKwp256) {
            std::vector<uint8_t> out(64);
            KmacKwp256::final(kmacKwp256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacAead128) {
            std::vector<uint8_t> out(32);
            KmacAead128::final(kmacAead128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacAead256) {
            std::vector<uint8_t> out(64);
            KmacAead256::final(kmacAead256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacMgf128) {
            std::vector<uint8_t> out(32);
            KmacMgf128::final(kmacMgf128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacMgf256) {
            std::vector<uint8_t> out(64);
            KmacMgf256::final(kmacMgf256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacKdf128) {
            std::vector<uint8_t> out(32);
            KmacKdf128::final(kmacKdf128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacKdf256) {
            std::vector<uint8_t> out(64);
            KmacKdf256::final(kmacKdf256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacCtr128) {
            size_t outSize = kmacCtr128Ctx.buffer.empty() ? 32 : kmacCtr128Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            KmacCtr128::final(kmacCtr128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacCtr256) {
            size_t outSize = kmacCtr256Ctx.buffer.empty() ? 64 : kmacCtr256Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            KmacCtr256::final(kmacCtr256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacOfb128) {
            size_t outSize = kmacOfb128Ctx.buffer.empty() ? 32 : kmacOfb128Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            KmacOfb128::final(kmacOfb128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacOfb256) {
            size_t outSize = kmacOfb256Ctx.buffer.empty() ? 64 : kmacOfb256Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            KmacOfb256::final(kmacOfb256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacCfb128) {
            size_t outSize = kmacCfb128Ctx.buffer.empty() ? 32 : kmacCfb128Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            KmacCfb128::final(kmacCfb128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacCfb256) {
            size_t outSize = kmacCfb256Ctx.buffer.empty() ? 64 : kmacCfb256Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            KmacCfb256::final(kmacCfb256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacSiv128) {
            std::vector<uint8_t> out(32);
            KmacSiv128::final(kmacSiv128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacSiv256) {
            std::vector<uint8_t> out(64);
            KmacSiv256::final(kmacSiv256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacFfx128) {
            size_t outSize = kmacFfx128Ctx.buffer.empty() ? 32 : kmacFfx128Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            KmacFfx128::final(kmacFfx128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacFfx256) {
            size_t outSize = kmacFfx256Ctx.buffer.empty() ? 64 : kmacFfx256Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            KmacFfx256::final(kmacFfx256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacBps128) {
            size_t outSize = kmacBps128Ctx.buffer.empty() ? 32 : kmacBps128Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            KmacBps128::final(kmacBps128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacBps256) {
            size_t outSize = kmacBps256Ctx.buffer.empty() ? 64 : kmacBps256Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            KmacBps256::final(kmacBps256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacKw128) {
            size_t outSize = kmacKw128Ctx.buffer.empty() ? 32 : (kmacKw128Ctx.buffer.size() + 8);
            std::vector<uint8_t> out(outSize);
            KmacKw128::final(kmacKw128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacKw256) {
            size_t outSize = kmacKw256Ctx.buffer.empty() ? 64 : (kmacKw256Ctx.buffer.size() + 8);
            std::vector<uint8_t> out(outSize);
            KmacKw256::final(kmacKw256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacDrbg128) {
            std::vector<uint8_t> out(32);
            KmacDrbg128::final(kmacDrbg128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::KmacDrbg256) {
            std::vector<uint8_t> out(64);
            KmacDrbg256::final(kmacDrbg256Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::Ascon128) {
            size_t outSize = ascon128Ctx.buffer.empty() ? 16 : ascon128Ctx.buffer.size();
            std::vector<uint8_t> out(outSize);
            Ascon128::final(ascon128Ctx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::AsconHash) {
            std::vector<uint8_t> out(32);
            AsconHash::final(asconHashCtx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::Ascon80pq) {
            size_t outSize = ascon80pqCtx.buffer.empty() ? 16 : ascon80pqCtx.buffer.size();
            std::vector<uint8_t> out(outSize);
            Ascon80pq::final(ascon80pqCtx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::AsconXof) {
            std::vector<uint8_t> out(32);
            AsconXof::final(asconXofCtx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::Ascon128a) {
            size_t outSize = ascon128aCtx.buffer.empty() ? 16 : ascon128aCtx.buffer.size();
            std::vector<uint8_t> out(outSize);
            Ascon128a::final(ascon128aCtx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::AsconXofa) {
            std::vector<uint8_t> out(32);
            AsconXofa::final(asconXofaCtx, out);
            return out;
        } else if (algorithm == CryptoAlgorithm::AesCmac && aesCmacCtx && aesCmacCtx->aes) {
            std::array<uint8_t, 16> M_last{};
            if (aesCmacCtx->totalBytes > 0 && aesCmacCtx->buffer.size() == 16) {
                for (size_t i = 0; i < 16; ++i) {
                    M_last[i] = aesCmacCtx->buffer[i] ^ aesCmacCtx->k1[i];
                }
            } else {
                size_t rem = aesCmacCtx->buffer.size();
                for (size_t i = 0; i < rem; ++i) {
                    M_last[i] = aesCmacCtx->buffer[i];
                }
                M_last[rem] = 0x80;
                for (size_t i = rem + 1; i < 16; ++i) {
                    M_last[i] = 0x00;
                }
                for (size_t i = 0; i < 16; ++i) {
                    M_last[i] ^= aesCmacCtx->k2[i];
                }
            }
            std::array<uint8_t, 16> Y{};
            for (size_t j = 0; j < 16; ++j) {
                Y[j] = aesCmacCtx->X[j] ^ M_last[j];
            }
            std::vector<uint8_t> out(16);
            aesCmacCtx->aes->encryptBlock(Y.data(), out.data());
            return out;
        }
        return {};
    }
};

class PersistedKey {
public:
    std::wstring name;
    std::wstring algorithmId;
    std::vector<uint8_t> secret;
    bool finalized{false};

    PersistedKey(std::wstring_view keyName, std::wstring_view alg)
        : name(keyName), algorithmId(alg) {}
};

class CngStorageProvider {
public:
    std::wstring providerName;
    std::unordered_map<std::wstring, std::shared_ptr<PersistedKey>> persistedKeys;

    explicit CngStorageProvider(std::wstring_view name) : providerName(name) {}
};

// ============================================================================
// 8. Sovereign CipherKSP Engine & Handle Manager
// ============================================================================
class CipherKspEngine {
public:
    static CipherKspEngine& get() {
        static CipherKspEngine instance;
        return instance;
    }

    // Provider Management
    BCRYPT_ALG_HANDLE registerProvider(std::shared_ptr<CngAlgorithmProvider> prov) {
        std::lock_guard<std::mutex> lock(mutex_);
        void* handle = reinterpret_cast<void*>(nextHandle_++);
        providers_[handle] = std::move(prov);
        return handle;
    }

    std::shared_ptr<CngAlgorithmProvider> getProvider(BCRYPT_ALG_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = providers_.find(handle);
        return (it != providers_.end()) ? it->second : nullptr;
    }

    bool releaseProvider(BCRYPT_ALG_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        return providers_.erase(handle) > 0;
    }

    // Key Object Management
    BCRYPT_KEY_HANDLE registerKey(std::shared_ptr<CngKeyObject> key) {
        std::lock_guard<std::mutex> lock(mutex_);
        void* handle = reinterpret_cast<void*>(nextHandle_++);
        keys_[handle] = std::move(key);
        return handle;
    }

    std::shared_ptr<CngKeyObject> getKey(BCRYPT_KEY_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = keys_.find(handle);
        return (it != keys_.end()) ? it->second : nullptr;
    }

    bool releaseKey(BCRYPT_KEY_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        return keys_.erase(handle) > 0;
    }

    // Secret Object Management
    struct CngSecretObject {
        std::vector<uint8_t> secretValue;
        explicit CngSecretObject(std::vector<uint8_t> val) : secretValue(std::move(val)) {}
    };

    BCRYPT_SECRET_HANDLE registerSecret(std::shared_ptr<CngSecretObject> secret) {
        std::lock_guard<std::mutex> lock(mutex_);
        void* handle = reinterpret_cast<void*>(nextHandle_++);
        secrets_[handle] = std::move(secret);
        return handle;
    }

    std::shared_ptr<CngSecretObject> getSecret(BCRYPT_SECRET_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = secrets_.find(handle);
        return (it != secrets_.end()) ? it->second : nullptr;
    }

    bool releaseSecret(BCRYPT_SECRET_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        return secrets_.erase(handle) > 0;
    }

    // Hash Object Management
    BCRYPT_HASH_HANDLE registerHash(std::shared_ptr<CngHashObject> hash) {
        std::lock_guard<std::mutex> lock(mutex_);
        void* handle = reinterpret_cast<void*>(nextHandle_++);
        hashes_[handle] = std::move(hash);
        return handle;
    }

    std::shared_ptr<CngHashObject> getHash(BCRYPT_HASH_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = hashes_.find(handle);
        return (it != hashes_.end()) ? it->second : nullptr;
    }

    bool releaseHash(BCRYPT_HASH_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        return hashes_.erase(handle) > 0;
    }

    // KSP Storage Provider Management
    NCRYPT_PROV_HANDLE registerStorageProvider(std::shared_ptr<CngStorageProvider> prov) {
        std::lock_guard<std::mutex> lock(mutex_);
        void* handle = reinterpret_cast<void*>(nextHandle_++);
        storageProviders_[handle] = std::move(prov);
        return handle;
    }

    std::shared_ptr<CngStorageProvider> getStorageProvider(NCRYPT_PROV_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = storageProviders_.find(handle);
        return (it != storageProviders_.end()) ? it->second : nullptr;
    }

    // Persisted Key Management
    NCRYPT_KEY_HANDLE registerPersistedKey(std::shared_ptr<PersistedKey> key) {
        std::lock_guard<std::mutex> lock(mutex_);
        void* handle = reinterpret_cast<void*>(nextHandle_++);
        persistedKeys_[handle] = std::move(key);
        return handle;
    }

    std::shared_ptr<PersistedKey> getPersistedKey(NCRYPT_KEY_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = persistedKeys_.find(handle);
        return (it != persistedKeys_.end()) ? it->second : nullptr;
    }

    bool releasePersistedKey(NCRYPT_KEY_HANDLE handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        return persistedKeys_.erase(handle) > 0;
    }

private:
    std::mutex mutex_;
    uintptr_t nextHandle_{0x1000};
    std::unordered_map<void*, std::shared_ptr<CngAlgorithmProvider>> providers_;
    std::unordered_map<void*, std::shared_ptr<CngKeyObject>> keys_;
    std::unordered_map<void*, std::shared_ptr<CngSecretObject>> secrets_;
    std::unordered_map<void*, std::shared_ptr<CngHashObject>> hashes_;
    std::unordered_map<void*, std::shared_ptr<CngStorageProvider>> storageProviders_;
    std::unordered_map<void*, std::shared_ptr<PersistedKey>> persistedKeys_;

    CipherKspEngine() = default;
};

// ============================================================================
// 9. Standard Windows CNG (BCrypt) API Function Definitions
// ============================================================================

inline NTSTATUS BCryptOpenAlgorithmProvider(
    BCRYPT_ALG_HANDLE* phAlgorithm,
    const wchar_t* pszAlgId,
    [[maybe_unused]] const wchar_t* pszImplementation,
    uint32_t dwFlags
) {
    if (!phAlgorithm || !pszAlgId) {
        return STATUS_INVALID_PARAMETER;
    }

    auto algType = parseAlgorithm(pszAlgId);
    if (algType == CryptoAlgorithm::Unknown) {
        return STATUS_NOT_SUPPORTED;
    }

    auto prov = std::make_shared<CngAlgorithmProvider>(pszAlgId, dwFlags);
    *phAlgorithm = CipherKspEngine::get().registerProvider(prov);
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptCloseAlgorithmProvider(
    BCRYPT_ALG_HANDLE hAlgorithm,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hAlgorithm) return STATUS_INVALID_HANDLE;
    return CipherKspEngine::get().releaseProvider(hAlgorithm) ? STATUS_SUCCESS : STATUS_INVALID_HANDLE;
}

inline NTSTATUS BCryptGetProperty(
    BCRYPT_HANDLE hObject,
    const wchar_t* pszProperty,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    uint32_t* pcbResult,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hObject || !pszProperty || !pcbResult) {
        return STATUS_INVALID_PARAMETER;
    }

    auto prov = CipherKspEngine::get().getProvider(hObject);
    if (prov) {
        if (std::wstring_view(pszProperty) == BCRYPT_CHAINING_MODE) {
            uint32_t required = static_cast<uint32_t>((prov->chainingMode.size() + 1) * sizeof(wchar_t));
            *pcbResult = required;
            if (!pbOutput) return STATUS_SUCCESS;
            if (cbOutput < required) return STATUS_BUFFER_TOO_SMALL;
            std::memcpy(pbOutput, prov->chainingMode.c_str(), required);
            return STATUS_SUCCESS;
        }
        if (std::wstring_view(pszProperty) == BCRYPT_HASH_LENGTH) {
            *pcbResult = sizeof(uint32_t);
            if (!pbOutput) return STATUS_SUCCESS;
            if (cbOutput < sizeof(uint32_t)) return STATUS_BUFFER_TOO_SMALL;
            uint32_t len = 32;
            if (prov->algorithm == CryptoAlgorithm::Md5 || prov->algorithm == CryptoAlgorithm::AesCmac || prov->algorithm == CryptoAlgorithm::Ascon128 || prov->algorithm == CryptoAlgorithm::Ascon80pq || prov->algorithm == CryptoAlgorithm::Ascon128a) len = 16;
            else if (prov->algorithm == CryptoAlgorithm::Sha1) len = 20;
            else if (prov->algorithm == CryptoAlgorithm::Sha3_224) len = 28;
            else if (prov->algorithm == CryptoAlgorithm::Sha256 || prov->algorithm == CryptoAlgorithm::HmacSha256 || prov->algorithm == CryptoAlgorithm::Sha3_256 || prov->algorithm == CryptoAlgorithm::Shake128 || prov->algorithm == CryptoAlgorithm::CShake128 || prov->algorithm == CryptoAlgorithm::Kmac128 || prov->algorithm == CryptoAlgorithm::TupleHash128 || prov->algorithm == CryptoAlgorithm::ParallelHash128 || prov->algorithm == CryptoAlgorithm::ParallelHashXOF128 || prov->algorithm == CryptoAlgorithm::TupleHashXOF128 || prov->algorithm == CryptoAlgorithm::KmacXOF128 || prov->algorithm == CryptoAlgorithm::KmacPrf128 || prov->algorithm == CryptoAlgorithm::KmacRatchet128 || prov->algorithm == CryptoAlgorithm::KmacDoubleRatchet128 || prov->algorithm == CryptoAlgorithm::KmacTripleRatchet128 || prov->algorithm == CryptoAlgorithm::KmacTreeRatchet128 || prov->algorithm == CryptoAlgorithm::KmacAsyncRatchet128 || prov->algorithm == CryptoAlgorithm::KmacKwp128 || prov->algorithm == CryptoAlgorithm::KmacAead128 || prov->algorithm == CryptoAlgorithm::KmacMgf128 || prov->algorithm == CryptoAlgorithm::KmacKdf128 || prov->algorithm == CryptoAlgorithm::KmacCtr128 || prov->algorithm == CryptoAlgorithm::KmacOfb128 || prov->algorithm == CryptoAlgorithm::KmacCfb128 || prov->algorithm == CryptoAlgorithm::KmacSiv128 || prov->algorithm == CryptoAlgorithm::KmacFfx128 || prov->algorithm == CryptoAlgorithm::KmacBps128 || prov->algorithm == CryptoAlgorithm::KmacKw128 || prov->algorithm == CryptoAlgorithm::KmacDrbg128 || prov->algorithm == CryptoAlgorithm::AsconHash || prov->algorithm == CryptoAlgorithm::AsconXof || prov->algorithm == CryptoAlgorithm::AsconXofa) len = 32;
            else if (prov->algorithm == CryptoAlgorithm::Sha384 || prov->algorithm == CryptoAlgorithm::HmacSha384 || prov->algorithm == CryptoAlgorithm::Sha3_384) len = 48;
            else if (prov->algorithm == CryptoAlgorithm::Sha512 || prov->algorithm == CryptoAlgorithm::HmacSha512 || prov->algorithm == CryptoAlgorithm::Sha3_512 || prov->algorithm == CryptoAlgorithm::Shake256 || prov->algorithm == CryptoAlgorithm::CShake256 || prov->algorithm == CryptoAlgorithm::Kmac256 || prov->algorithm == CryptoAlgorithm::TupleHash256 || prov->algorithm == CryptoAlgorithm::ParallelHash256 || prov->algorithm == CryptoAlgorithm::ParallelHashXOF256 || prov->algorithm == CryptoAlgorithm::TupleHashXOF256 || prov->algorithm == CryptoAlgorithm::KmacXOF256 || prov->algorithm == CryptoAlgorithm::KmacPrf256 || prov->algorithm == CryptoAlgorithm::KmacRatchet256 || prov->algorithm == CryptoAlgorithm::KmacDoubleRatchet256 || prov->algorithm == CryptoAlgorithm::KmacTripleRatchet256 || prov->algorithm == CryptoAlgorithm::KmacTreeRatchet256 || prov->algorithm == CryptoAlgorithm::KmacAsyncRatchet256 || prov->algorithm == CryptoAlgorithm::KmacKwp256 || prov->algorithm == CryptoAlgorithm::KmacAead256 || prov->algorithm == CryptoAlgorithm::KmacMgf256 || prov->algorithm == CryptoAlgorithm::KmacKdf256 || prov->algorithm == CryptoAlgorithm::KmacCtr256 || prov->algorithm == CryptoAlgorithm::KmacOfb256 || prov->algorithm == CryptoAlgorithm::KmacCfb256 || prov->algorithm == CryptoAlgorithm::KmacSiv256 || prov->algorithm == CryptoAlgorithm::KmacFfx256 || prov->algorithm == CryptoAlgorithm::KmacBps256 || prov->algorithm == CryptoAlgorithm::KmacKw256 || prov->algorithm == CryptoAlgorithm::KmacDrbg256) len = 64;
            std::memcpy(pbOutput, &len, sizeof(len));
            return STATUS_SUCCESS;
        }
        if (std::wstring_view(pszProperty) == BCRYPT_BLOCK_LENGTH) {
            *pcbResult = sizeof(uint32_t);
            if (!pbOutput) return STATUS_SUCCESS;
            if (cbOutput < sizeof(uint32_t)) return STATUS_BUFFER_TOO_SMALL;
            uint32_t len = 64;
            if (prov->algorithm == CryptoAlgorithm::Ascon128 || prov->algorithm == CryptoAlgorithm::AsconHash ||
                prov->algorithm == CryptoAlgorithm::Ascon80pq || prov->algorithm == CryptoAlgorithm::AsconXof ||
                prov->algorithm == CryptoAlgorithm::AsconXofa) {
                len = 8;
            } else if (prov->algorithm == CryptoAlgorithm::Ascon128a) {
                len = 16;
            } else if (prov->algorithm == CryptoAlgorithm::Aes || prov->algorithm == CryptoAlgorithm::AesCmac) {
                len = 16;
            } else if (prov->algorithm == CryptoAlgorithm::Sha3_224) {
                len = 144;
            } else if (prov->algorithm == CryptoAlgorithm::Sha3_256 || prov->algorithm == CryptoAlgorithm::Shake256 || prov->algorithm == CryptoAlgorithm::CShake256 || prov->algorithm == CryptoAlgorithm::Kmac256 || prov->algorithm == CryptoAlgorithm::TupleHash256 || prov->algorithm == CryptoAlgorithm::ParallelHash256 || prov->algorithm == CryptoAlgorithm::ParallelHashXOF256 || prov->algorithm == CryptoAlgorithm::TupleHashXOF256 || prov->algorithm == CryptoAlgorithm::KmacXOF256 || prov->algorithm == CryptoAlgorithm::KmacPrf256 || prov->algorithm == CryptoAlgorithm::KmacRatchet256 || prov->algorithm == CryptoAlgorithm::KmacDoubleRatchet256 || prov->algorithm == CryptoAlgorithm::KmacTripleRatchet256 || prov->algorithm == CryptoAlgorithm::KmacTreeRatchet256 || prov->algorithm == CryptoAlgorithm::KmacAsyncRatchet256 || prov->algorithm == CryptoAlgorithm::KmacKwp256 || prov->algorithm == CryptoAlgorithm::KmacAead256 || prov->algorithm == CryptoAlgorithm::KmacMgf256 || prov->algorithm == CryptoAlgorithm::KmacKdf256 || prov->algorithm == CryptoAlgorithm::KmacCtr256 || prov->algorithm == CryptoAlgorithm::KmacOfb256 || prov->algorithm == CryptoAlgorithm::KmacCfb256 || prov->algorithm == CryptoAlgorithm::KmacSiv256 || prov->algorithm == CryptoAlgorithm::KmacFfx256 || prov->algorithm == CryptoAlgorithm::KmacBps256 || prov->algorithm == CryptoAlgorithm::KmacKw256 || prov->algorithm == CryptoAlgorithm::KmacDrbg256) {
                len = 136;
            } else if (prov->algorithm == CryptoAlgorithm::Sha3_384) {
                len = 104;
            } else if (prov->algorithm == CryptoAlgorithm::Sha3_512) {
                len = 72;
            } else if (prov->algorithm == CryptoAlgorithm::Shake128 || prov->algorithm == CryptoAlgorithm::CShake128 || prov->algorithm == CryptoAlgorithm::Kmac128 || prov->algorithm == CryptoAlgorithm::TupleHash128 || prov->algorithm == CryptoAlgorithm::ParallelHash128 || prov->algorithm == CryptoAlgorithm::ParallelHashXOF128 || prov->algorithm == CryptoAlgorithm::TupleHashXOF128 || prov->algorithm == CryptoAlgorithm::KmacXOF128 || prov->algorithm == CryptoAlgorithm::KmacPrf128 || prov->algorithm == CryptoAlgorithm::KmacRatchet128 || prov->algorithm == CryptoAlgorithm::KmacDoubleRatchet128 || prov->algorithm == CryptoAlgorithm::KmacTripleRatchet128 || prov->algorithm == CryptoAlgorithm::KmacTreeRatchet128 || prov->algorithm == CryptoAlgorithm::KmacAsyncRatchet128 || prov->algorithm == CryptoAlgorithm::KmacKwp128 || prov->algorithm == CryptoAlgorithm::KmacAead128 || prov->algorithm == CryptoAlgorithm::KmacMgf128 || prov->algorithm == CryptoAlgorithm::KmacKdf128 || prov->algorithm == CryptoAlgorithm::KmacCtr128 || prov->algorithm == CryptoAlgorithm::KmacOfb128 || prov->algorithm == CryptoAlgorithm::KmacCfb128 || prov->algorithm == CryptoAlgorithm::KmacSiv128 || prov->algorithm == CryptoAlgorithm::KmacFfx128 || prov->algorithm == CryptoAlgorithm::KmacBps128 || prov->algorithm == CryptoAlgorithm::KmacKw128 || prov->algorithm == CryptoAlgorithm::KmacDrbg128) {
                len = 168;
            } else if (prov->algorithm == CryptoAlgorithm::Sha384 || prov->algorithm == CryptoAlgorithm::Sha512 || prov->algorithm == CryptoAlgorithm::HmacSha384 || prov->algorithm == CryptoAlgorithm::HmacSha512) {
                len = 128;
            }
            std::memcpy(pbOutput, &len, sizeof(len));
            return STATUS_SUCCESS;
        }
        if (std::wstring_view(pszProperty) == BCRYPT_OBJECT_LENGTH) {
            *pcbResult = sizeof(uint32_t);
            if (!pbOutput) return STATUS_SUCCESS;
            if (cbOutput < sizeof(uint32_t)) return STATUS_BUFFER_TOO_SMALL;
            uint32_t objLen = 512;
            std::memcpy(pbOutput, &objLen, sizeof(objLen));
            return STATUS_SUCCESS;
        }
        if (std::wstring_view(pszProperty) == BCRYPT_AUTH_TAG_LENGTH) {
            *pcbResult = sizeof(BCRYPT_AUTH_TAG_LENGTHS_STRUCT);
            if (!pbOutput) return STATUS_SUCCESS;
            if (cbOutput < sizeof(BCRYPT_AUTH_TAG_LENGTHS_STRUCT)) return STATUS_BUFFER_TOO_SMALL;
            BCRYPT_AUTH_TAG_LENGTHS_STRUCT tagLengths{12, 16, 1};
            std::memcpy(pbOutput, &tagLengths, sizeof(tagLengths));
            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_SUPPORTED;
}

inline NTSTATUS BCryptSetProperty(
    BCRYPT_HANDLE hObject,
    const wchar_t* pszProperty,
    const uint8_t* pbInput,
    [[maybe_unused]] uint32_t cbInput,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hObject || !pszProperty || !pbInput) {
        return STATUS_INVALID_PARAMETER;
    }

    auto prov = CipherKspEngine::get().getProvider(hObject);
    if (prov) {
        if (std::wstring_view(pszProperty) == BCRYPT_CHAINING_MODE) {
            prov->chainingMode = reinterpret_cast<const wchar_t*>(pbInput);
            return STATUS_SUCCESS;
        }
    }

    auto keyObj = CipherKspEngine::get().getKey(hObject);
    if (keyObj) {
        if (std::wstring_view(pszProperty) == BCRYPT_CHAINING_MODE) {
            keyObj->chainingMode = reinterpret_cast<const wchar_t*>(pbInput);
            if (keyObj->algorithm == CryptoAlgorithm::Aes) {
                if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_XTS) {
                    if (keyObj->secret.size() == 32 || keyObj->secret.size() == 64) {
                        size_t half = keyObj->secret.size() / 2;
                        keyObj->aesEngine = std::make_unique<Aes>(std::span<const uint8_t>(keyObj->secret.data(), half));
                        keyObj->aesEngine2 = std::make_unique<Aes>(std::span<const uint8_t>(keyObj->secret.data() + half, half));
                    }
                } else {
                    keyObj->aesEngine = std::make_unique<Aes>(keyObj->secret);
                    keyObj->aesEngine2.reset();
                }
            }
            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_SUPPORTED;
}

inline NTSTATUS BCryptGenerateSymmetricKey(
    BCRYPT_ALG_HANDLE hAlgorithm,
    BCRYPT_KEY_HANDLE* phKey,
    [[maybe_unused]] uint8_t* pbKeyObject,
    [[maybe_unused]] uint32_t cbKeyObject,
    const uint8_t* pbSecret,
    uint32_t cbSecret,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hAlgorithm || !phKey || !pbSecret || cbSecret == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    auto prov = CipherKspEngine::get().getProvider(hAlgorithm);
    if (!prov || (prov->algorithm != CryptoAlgorithm::Aes &&
                  prov->algorithm != CryptoAlgorithm::Sp800108CtrHmac &&
                  prov->algorithm != CryptoAlgorithm::Sp800108FeedbackHmac &&
                  prov->algorithm != CryptoAlgorithm::Sp800108PipelineHmac &&
                  prov->algorithm != CryptoAlgorithm::Sp80056AConcat &&
                  prov->algorithm != CryptoAlgorithm::Hkdf &&
                  prov->algorithm != CryptoAlgorithm::Ascon128 &&
                  prov->algorithm != CryptoAlgorithm::Ascon80pq &&
                  prov->algorithm != CryptoAlgorithm::Ascon128a)) {
        return STATUS_NOT_SUPPORTED;
    }

    if (prov->algorithm == CryptoAlgorithm::Ascon128) {
        if (cbSecret != 16) {
            return STATUS_INVALID_PARAMETER;
        }
        auto keyObj = std::make_shared<CngKeyObject>(
            prov->algorithm,
            std::span<const uint8_t>(pbSecret, cbSecret),
            L""
        );
        *phKey = CipherKspEngine::get().registerKey(keyObj);
        return STATUS_SUCCESS;
    }

    if (prov->algorithm == CryptoAlgorithm::Ascon80pq) {
        if (cbSecret != 20) {
            return STATUS_INVALID_PARAMETER;
        }
        auto keyObj = std::make_shared<CngKeyObject>(
            prov->algorithm,
            std::span<const uint8_t>(pbSecret, cbSecret),
            L""
        );
        *phKey = CipherKspEngine::get().registerKey(keyObj);
        return STATUS_SUCCESS;
    }

    if (prov->algorithm == CryptoAlgorithm::Ascon128a) {
        if (cbSecret != 16) {
            return STATUS_INVALID_PARAMETER;
        }
        auto keyObj = std::make_shared<CngKeyObject>(
            prov->algorithm,
            std::span<const uint8_t>(pbSecret, cbSecret),
            L""
        );
        *phKey = CipherKspEngine::get().registerKey(keyObj);
        return STATUS_SUCCESS;
    }

    if (prov->algorithm == CryptoAlgorithm::Sp800108CtrHmac ||
        prov->algorithm == CryptoAlgorithm::Sp800108FeedbackHmac ||
        prov->algorithm == CryptoAlgorithm::Sp800108PipelineHmac ||
        prov->algorithm == CryptoAlgorithm::Sp80056AConcat ||
        prov->algorithm == CryptoAlgorithm::Hkdf) {
        auto keyObj = std::make_shared<CngKeyObject>(
            prov->algorithm,
            std::span<const uint8_t>(pbSecret, cbSecret),
            L""
        );
        *phKey = CipherKspEngine::get().registerKey(keyObj);
        return STATUS_SUCCESS;
    }

    if (prov->chainingMode == BCRYPT_CHAIN_MODE_XTS) {
        if (cbSecret != 32 && cbSecret != 64) {
            return STATUS_INVALID_PARAMETER;
        }
    } else {
        if (cbSecret != 16 && cbSecret != 24 && cbSecret != 32) {
            return STATUS_INVALID_PARAMETER;
        }
    }

    auto keyObj = std::make_shared<CngKeyObject>(
        CryptoAlgorithm::Aes,
        std::span<const uint8_t>(pbSecret, cbSecret),
        prov->chainingMode
    );

    *phKey = CipherKspEngine::get().registerKey(keyObj);
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptDestroyKey(BCRYPT_KEY_HANDLE hKey);

inline NTSTATUS BCryptEncrypt(
    BCRYPT_KEY_HANDLE hKey,
    const uint8_t* pbInput,
    uint32_t cbInput,
    [[maybe_unused]] void* pPaddingInfo,
    uint8_t* pbIV,
    uint32_t cbIV,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    uint32_t* pcbResult,
    uint32_t dwFlags
) {
    if (!hKey || !pbInput || !pcbResult) {
        return STATUS_INVALID_PARAMETER;
    }

    auto keyObj = CipherKspEngine::get().getKey(hKey);
    if (!keyObj) {
        return STATUS_INVALID_HANDLE;
    }

    if (keyObj->algorithm == CryptoAlgorithm::Ascon128) {
        if (!pPaddingInfo) {
            return STATUS_INVALID_PARAMETER;
        }
        auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
        if (!authInfo->pbNonce || authInfo->cbNonce < 16 || !authInfo->pbTag || authInfo->cbTag < 16) {
            return STATUS_INVALID_PARAMETER;
        }
        std::span<const uint8_t> nonce(authInfo->pbNonce, authInfo->cbNonce);
        std::span<const uint8_t> aad{};
        if (authInfo->pbAuthData && authInfo->cbAuthData > 0) {
            aad = std::span<const uint8_t>(authInfo->pbAuthData, authInfo->cbAuthData);
        }
        std::span<const uint8_t> pt(pbInput, cbInput);
        *pcbResult = cbInput;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < cbInput) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        Ascon128::Context ctx;
        Ascon128::init(ctx, keyObj->secret, nonce);
        std::vector<uint8_t> ct(cbInput);
        std::vector<uint8_t> tag(16);
        if (!Ascon128::encrypt(ctx, pt, ct, tag, aad, nonce)) {
            return STATUS_UNSUCCESSFUL;
        }
        std::memcpy(pbOutput, ct.data(), cbInput);
        std::memcpy(authInfo->pbTag, tag.data(), 16);
        return STATUS_SUCCESS;
    }

    if (keyObj->algorithm == CryptoAlgorithm::Ascon80pq) {
        if (!pPaddingInfo) {
            return STATUS_INVALID_PARAMETER;
        }
        auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
        if (!authInfo->pbNonce || authInfo->cbNonce < 16 || !authInfo->pbTag || authInfo->cbTag < 16) {
            return STATUS_INVALID_PARAMETER;
        }
        std::span<const uint8_t> nonce(authInfo->pbNonce, authInfo->cbNonce);
        std::span<const uint8_t> aad{};
        if (authInfo->pbAuthData && authInfo->cbAuthData > 0) {
            aad = std::span<const uint8_t>(authInfo->pbAuthData, authInfo->cbAuthData);
        }
        std::span<const uint8_t> pt(pbInput, cbInput);
        *pcbResult = cbInput;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < cbInput) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        Ascon80pq::Context ctx;
        Ascon80pq::init(ctx, keyObj->secret, nonce);
        std::vector<uint8_t> ct(cbInput);
        std::vector<uint8_t> tag(16);
        if (!Ascon80pq::encrypt(ctx, pt, ct, tag, aad, nonce)) {
            return STATUS_UNSUCCESSFUL;
        }
        std::memcpy(pbOutput, ct.data(), cbInput);
        std::memcpy(authInfo->pbTag, tag.data(), 16);
        return STATUS_SUCCESS;
    }

    if (keyObj->algorithm == CryptoAlgorithm::Ascon128a) {
        if (!pPaddingInfo) {
            return STATUS_INVALID_PARAMETER;
        }
        auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
        if (!authInfo->pbNonce || authInfo->cbNonce < 16 || !authInfo->pbTag || authInfo->cbTag < 16) {
            return STATUS_INVALID_PARAMETER;
        }
        std::span<const uint8_t> nonce(authInfo->pbNonce, authInfo->cbNonce);
        std::span<const uint8_t> aad{};
        if (authInfo->pbAuthData && authInfo->cbAuthData > 0) {
            aad = std::span<const uint8_t>(authInfo->pbAuthData, authInfo->cbAuthData);
        }
        std::span<const uint8_t> pt(pbInput, cbInput);
        *pcbResult = cbInput;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < cbInput) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        Ascon128a::Context ctx;
        Ascon128a::init(ctx, keyObj->secret, nonce);
        std::vector<uint8_t> ct(cbInput);
        std::vector<uint8_t> tag(16);
        if (!Ascon128a::encrypt(ctx, pt, ct, tag, aad, nonce)) {
            return STATUS_UNSUCCESSFUL;
        }
        std::memcpy(pbOutput, ct.data(), cbInput);
        std::memcpy(authInfo->pbTag, tag.data(), 16);
        return STATUS_SUCCESS;
    }

    if (!keyObj->aesEngine) {
        return STATUS_INVALID_HANDLE;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_GCM) {
        if (!pPaddingInfo) {
            return STATUS_INVALID_PARAMETER;
        }
        auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
        if (!authInfo->pbNonce || authInfo->cbNonce == 0 || !authInfo->pbTag || authInfo->cbTag < 12) {
            return STATUS_INVALID_PARAMETER;
        }
        std::span<const uint8_t> nonce(authInfo->pbNonce, authInfo->cbNonce);
        std::span<const uint8_t> aad{};
        if (authInfo->pbAuthData && authInfo->cbAuthData > 0) {
            aad = std::span<const uint8_t>(authInfo->pbAuthData, authInfo->cbAuthData);
        }
        std::span<const uint8_t> pt(pbInput, cbInput);
        auto result = keyObj->aesEngine->encryptGcm(pt, nonce, aad, authInfo->cbTag);
        if (!result.success) {
            return STATUS_UNSUCCESSFUL;
        }
        *pcbResult = static_cast<uint32_t>(result.ciphertext.size());
        if (authInfo->pbTag) {
            std::memcpy(authInfo->pbTag, result.tag.data(), authInfo->cbTag);
        }
        if (cbInput > 0 && !pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < result.ciphertext.size()) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        if (!result.ciphertext.empty() && pbOutput) {
            std::memcpy(pbOutput, result.ciphertext.data(), result.ciphertext.size());
        }
        return STATUS_SUCCESS;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_CMAC) {
        *pcbResult = 16;
        if (pPaddingInfo) {
            auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
            if (authInfo && authInfo->pbTag && authInfo->cbTag >= 16) {
                auto tag = keyObj->aesEngine->cmac(std::span<const uint8_t>(pbInput, cbInput));
                std::memcpy(authInfo->pbTag, tag.data(), 16);
            }
        }
        if (!pbOutput) return STATUS_SUCCESS;
        if (cbOutput < 16) return STATUS_BUFFER_TOO_SMALL;
        auto tag = keyObj->aesEngine->cmac(std::span<const uint8_t>(pbInput, cbInput));
        std::memcpy(pbOutput, tag.data(), 16);
        return STATUS_SUCCESS;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_XTS) {
        if (!keyObj->aesEngine2 || cbInput < 16) {
            return STATUS_INVALID_PARAMETER;
        }
        *pcbResult = cbInput;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < cbInput) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        auto ct = Aes::encryptXts(
            *keyObj->aesEngine,
            *keyObj->aesEngine2,
            std::span<const uint8_t>(pbInput, cbInput),
            std::span<const uint8_t>(pbIV, cbIV)
        );
        if (ct.empty()) return STATUS_INVALID_PARAMETER;
        std::memcpy(pbOutput, ct.data(), ct.size());
        return STATUS_SUCCESS;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_CCM) {
        if (!pPaddingInfo) {
            return STATUS_INVALID_PARAMETER;
        }
        auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
        if (!authInfo->pbNonce || authInfo->cbNonce < 7 || authInfo->cbNonce > 13 || !authInfo->pbTag || authInfo->cbTag < 4) {
            return STATUS_INVALID_PARAMETER;
        }
        std::span<const uint8_t> nonce(authInfo->pbNonce, authInfo->cbNonce);
        std::span<const uint8_t> aad{};
        if (authInfo->pbAuthData && authInfo->cbAuthData > 0) {
            aad = std::span<const uint8_t>(authInfo->pbAuthData, authInfo->cbAuthData);
        }
        std::span<const uint8_t> pt(pbInput, cbInput);
        auto result = keyObj->aesEngine->encryptCcm(pt, nonce, aad, authInfo->cbTag);
        if (!result.success) {
            return STATUS_UNSUCCESSFUL;
        }
        *pcbResult = static_cast<uint32_t>(result.ciphertext.size());
        if (authInfo->pbTag) {
            std::memcpy(authInfo->pbTag, result.tag.data(), authInfo->cbTag);
        }
        if (cbInput > 0 && !pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < result.ciphertext.size()) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        if (!result.ciphertext.empty() && pbOutput) {
            std::memcpy(pbOutput, result.ciphertext.data(), result.ciphertext.size());
        }
        return STATUS_SUCCESS;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_KW) {
        if (cbInput < 16 || (cbInput % 8) != 0) {
            return STATUS_INVALID_PARAMETER;
        }
        uint32_t expectedLen = cbInput + 8;
        *pcbResult = expectedLen;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < expectedLen) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        uint64_t iv = 0xA6A6A6A6A6A6A6A6ULL;
        if (pbIV && cbIV == 8) {
            iv = 0;
            for (int k = 0; k < 8; ++k) iv = (iv << 8) | pbIV[k];
        }
        auto ct = keyObj->aesEngine->keyWrap(std::span<const uint8_t>(pbInput, cbInput), iv);
        if (ct.empty()) return STATUS_INVALID_PARAMETER;
        std::memcpy(pbOutput, ct.data(), ct.size());
        return STATUS_SUCCESS;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_KWP) {
        if (cbInput == 0) {
            return STATUS_INVALID_PARAMETER;
        }
        size_t padLen = (8 - (cbInput % 8)) % 8;
        size_t totalPadded = cbInput + padLen;
        uint32_t expectedLen = (totalPadded == 8) ? 16 : static_cast<uint32_t>(totalPadded + 8);
        *pcbResult = expectedLen;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < expectedLen) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        auto ct = keyObj->aesEngine->keyWrapWithPadding(std::span<const uint8_t>(pbInput, cbInput));
        if (ct.empty()) return STATUS_INVALID_PARAMETER;
        std::memcpy(pbOutput, ct.data(), ct.size());
        return STATUS_SUCCESS;
    }

    bool usePadding = (dwFlags & BCRYPT_BLOCK_PADDING) != 0;
    Aes::Mode mode = Aes::Mode::CBC;
    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_ECB) {
        mode = Aes::Mode::ECB;
    } else if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_CTR) {
        mode = Aes::Mode::CTR;
    }

    std::span<const uint8_t> ivSpan{};
    if (pbIV && cbIV > 0) {
        ivSpan = std::span<const uint8_t>(pbIV, cbIV);
    }

    auto ciphertext = keyObj->aesEngine->encrypt(
        std::span<const uint8_t>(pbInput, cbInput),
        mode,
        ivSpan,
        usePadding
    );

    *pcbResult = static_cast<uint32_t>(ciphertext.size());
    if (!pbOutput) {
        return STATUS_SUCCESS;
    }

    if (cbOutput < ciphertext.size()) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    std::memcpy(pbOutput, ciphertext.data(), ciphertext.size());
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptDecrypt(
    BCRYPT_KEY_HANDLE hKey,
    const uint8_t* pbInput,
    uint32_t cbInput,
    [[maybe_unused]] void* pPaddingInfo,
    uint8_t* pbIV,
    uint32_t cbIV,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    uint32_t* pcbResult,
    uint32_t dwFlags
) {
    if (!hKey || !pbInput || !pcbResult) {
        return STATUS_INVALID_PARAMETER;
    }

    auto keyObj = CipherKspEngine::get().getKey(hKey);
    if (!keyObj) {
        return STATUS_INVALID_HANDLE;
    }

    if (keyObj->algorithm == CryptoAlgorithm::Ascon128) {
        if (!pPaddingInfo) {
            return STATUS_INVALID_PARAMETER;
        }
        auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
        if (!authInfo->pbNonce || authInfo->cbNonce < 16 || !authInfo->pbTag || authInfo->cbTag < 16) {
            return STATUS_INVALID_PARAMETER;
        }
        std::span<const uint8_t> nonce(authInfo->pbNonce, authInfo->cbNonce);
        std::span<const uint8_t> aad{};
        if (authInfo->pbAuthData && authInfo->cbAuthData > 0) {
            aad = std::span<const uint8_t>(authInfo->pbAuthData, authInfo->cbAuthData);
        }
        std::span<const uint8_t> ct(pbInput, cbInput);
        std::span<const uint8_t> tag(authInfo->pbTag, authInfo->cbTag);
        *pcbResult = cbInput;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < cbInput) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        Ascon128::Context ctx;
        Ascon128::init(ctx, keyObj->secret, nonce);
        std::vector<uint8_t> pt(cbInput);
        if (!Ascon128::decrypt(ctx, ct, tag, pt, aad, nonce)) {
            return STATUS_AUTH_TAG_MISMATCH;
        }
        std::memcpy(pbOutput, pt.data(), cbInput);
        return STATUS_SUCCESS;
    }

    if (keyObj->algorithm == CryptoAlgorithm::Ascon80pq) {
        if (!pPaddingInfo) {
            return STATUS_INVALID_PARAMETER;
        }
        auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
        if (!authInfo->pbNonce || authInfo->cbNonce < 16 || !authInfo->pbTag || authInfo->cbTag < 16) {
            return STATUS_INVALID_PARAMETER;
        }
        std::span<const uint8_t> nonce(authInfo->pbNonce, authInfo->cbNonce);
        std::span<const uint8_t> aad{};
        if (authInfo->pbAuthData && authInfo->cbAuthData > 0) {
            aad = std::span<const uint8_t>(authInfo->pbAuthData, authInfo->cbAuthData);
        }
        std::span<const uint8_t> ct(pbInput, cbInput);
        std::span<const uint8_t> tag(authInfo->pbTag, authInfo->cbTag);
        *pcbResult = cbInput;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < cbInput) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        Ascon80pq::Context ctx;
        Ascon80pq::init(ctx, keyObj->secret, nonce);
        std::vector<uint8_t> pt(cbInput);
        if (!Ascon80pq::decrypt(ctx, ct, tag, pt, aad, nonce)) {
            return STATUS_AUTH_TAG_MISMATCH;
        }
        std::memcpy(pbOutput, pt.data(), cbInput);
        return STATUS_SUCCESS;
    }

    if (keyObj->algorithm == CryptoAlgorithm::Ascon128a) {
        if (!pPaddingInfo) {
            return STATUS_INVALID_PARAMETER;
        }
        auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
        if (!authInfo->pbNonce || authInfo->cbNonce < 16 || !authInfo->pbTag || authInfo->cbTag < 16) {
            return STATUS_INVALID_PARAMETER;
        }
        std::span<const uint8_t> nonce(authInfo->pbNonce, authInfo->cbNonce);
        std::span<const uint8_t> aad{};
        if (authInfo->pbAuthData && authInfo->cbAuthData > 0) {
            aad = std::span<const uint8_t>(authInfo->pbAuthData, authInfo->cbAuthData);
        }
        std::span<const uint8_t> ct(pbInput, cbInput);
        std::span<const uint8_t> tag(authInfo->pbTag, authInfo->cbTag);
        *pcbResult = cbInput;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < cbInput) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        Ascon128a::Context ctx;
        Ascon128a::init(ctx, keyObj->secret, nonce);
        std::vector<uint8_t> pt(cbInput);
        if (!Ascon128a::decrypt(ctx, ct, tag, pt, aad, nonce)) {
            return STATUS_AUTH_TAG_MISMATCH;
        }
        std::memcpy(pbOutput, pt.data(), cbInput);
        return STATUS_SUCCESS;
    }

    if (!keyObj->aesEngine) {
        return STATUS_INVALID_HANDLE;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_CMAC) {
        if (pPaddingInfo) {
            auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
            if (authInfo && authInfo->pbTag && authInfo->cbTag >= 16) {
                bool ok = keyObj->aesEngine->cmacVerify(std::span<const uint8_t>(pbInput, cbInput), std::span<const uint8_t>(authInfo->pbTag, 16));
                return ok ? STATUS_SUCCESS : STATUS_AUTH_TAG_MISMATCH;
            }
        }
        *pcbResult = 16;
        if (!pbOutput) return STATUS_SUCCESS;
        if (cbOutput < 16) return STATUS_BUFFER_TOO_SMALL;
        auto tag = keyObj->aesEngine->cmac(std::span<const uint8_t>(pbInput, cbInput));
        std::memcpy(pbOutput, tag.data(), 16);
        return STATUS_SUCCESS;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_XTS) {
        if (!keyObj->aesEngine2 || cbInput < 16) {
            return STATUS_INVALID_PARAMETER;
        }
        *pcbResult = cbInput;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < cbInput) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        auto pt = Aes::decryptXts(
            *keyObj->aesEngine,
            *keyObj->aesEngine2,
            std::span<const uint8_t>(pbInput, cbInput),
            std::span<const uint8_t>(pbIV, cbIV)
        );
        if (pt.empty()) return STATUS_INVALID_PARAMETER;
        std::memcpy(pbOutput, pt.data(), pt.size());
        return STATUS_SUCCESS;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_GCM) {
        if (!pPaddingInfo) {
            return STATUS_INVALID_PARAMETER;
        }
        auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
        if (!authInfo->pbNonce || authInfo->cbNonce == 0 || !authInfo->pbTag || authInfo->cbTag < 12) {
            return STATUS_INVALID_PARAMETER;
        }
        std::span<const uint8_t> nonce(authInfo->pbNonce, authInfo->cbNonce);
        std::span<const uint8_t> aad{};
        if (authInfo->pbAuthData && authInfo->cbAuthData > 0) {
            aad = std::span<const uint8_t>(authInfo->pbAuthData, authInfo->cbAuthData);
        }
        std::span<const uint8_t> ct(pbInput, cbInput);
        std::span<const uint8_t> expectedTag(authInfo->pbTag, authInfo->cbTag);
        auto result = keyObj->aesEngine->decryptGcm(ct, nonce, aad, expectedTag);
        if (!result.success) {
            return STATUS_AUTH_TAG_MISMATCH;
        }
        *pcbResult = static_cast<uint32_t>(result.plaintext.size());
        if (cbInput > 0 && !pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < result.plaintext.size()) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        if (!result.plaintext.empty() && pbOutput) {
            std::memcpy(pbOutput, result.plaintext.data(), result.plaintext.size());
        }
        return STATUS_SUCCESS;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_CCM) {
        if (!pPaddingInfo) {
            return STATUS_INVALID_PARAMETER;
        }
        auto* authInfo = reinterpret_cast<BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO*>(pPaddingInfo);
        if (!authInfo->pbNonce || authInfo->cbNonce < 7 || authInfo->cbNonce > 13 || !authInfo->pbTag || authInfo->cbTag < 4) {
            return STATUS_INVALID_PARAMETER;
        }
        std::span<const uint8_t> nonce(authInfo->pbNonce, authInfo->cbNonce);
        std::span<const uint8_t> aad{};
        if (authInfo->pbAuthData && authInfo->cbAuthData > 0) {
            aad = std::span<const uint8_t>(authInfo->pbAuthData, authInfo->cbAuthData);
        }
        std::span<const uint8_t> ct(pbInput, cbInput);
        std::span<const uint8_t> expectedTag(authInfo->pbTag, authInfo->cbTag);
        auto result = keyObj->aesEngine->decryptCcm(ct, nonce, aad, expectedTag);
        if (!result.success) {
            return STATUS_AUTH_TAG_MISMATCH;
        }
        *pcbResult = static_cast<uint32_t>(result.plaintext.size());
        if (cbInput > 0 && !pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < result.plaintext.size()) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        if (!result.plaintext.empty() && pbOutput) {
            std::memcpy(pbOutput, result.plaintext.data(), result.plaintext.size());
        }
        return STATUS_SUCCESS;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_KW) {
        if (cbInput < 24 || (cbInput % 8) != 0) {
            return STATUS_INVALID_PARAMETER;
        }
        uint32_t expectedLen = cbInput - 8;
        *pcbResult = expectedLen;
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < expectedLen) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        uint64_t iv = 0xA6A6A6A6A6A6A6A6ULL;
        if (pbIV && cbIV == 8) {
            iv = 0;
            for (int k = 0; k < 8; ++k) iv = (iv << 8) | pbIV[k];
        }
        auto pt = keyObj->aesEngine->keyUnwrap(std::span<const uint8_t>(pbInput, cbInput), iv);
        if (pt.empty()) return STATUS_AUTH_TAG_MISMATCH;
        std::memcpy(pbOutput, pt.data(), pt.size());
        return STATUS_SUCCESS;
    }

    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_KWP) {
        if (cbInput < 16 || (cbInput % 8) != 0) {
            return STATUS_INVALID_PARAMETER;
        }
        auto pt = keyObj->aesEngine->keyUnwrapWithPadding(std::span<const uint8_t>(pbInput, cbInput));
        if (pt.empty()) return STATUS_AUTH_TAG_MISMATCH;
        *pcbResult = static_cast<uint32_t>(pt.size());
        if (!pbOutput) {
            return STATUS_SUCCESS;
        }
        if (cbOutput < pt.size()) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        std::memcpy(pbOutput, pt.data(), pt.size());
        return STATUS_SUCCESS;
    }

    bool usePadding = (dwFlags & BCRYPT_BLOCK_PADDING) != 0;
    Aes::Mode mode = Aes::Mode::CBC;
    if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_ECB) {
        mode = Aes::Mode::ECB;
    } else if (keyObj->chainingMode == BCRYPT_CHAIN_MODE_CTR) {
        mode = Aes::Mode::CTR;
    }

    std::span<const uint8_t> ivSpan{};
    if (pbIV && cbIV > 0) {
        ivSpan = std::span<const uint8_t>(pbIV, cbIV);
    }

    bool ok = false;
    auto plaintext = keyObj->aesEngine->decrypt(
        std::span<const uint8_t>(pbInput, cbInput),
        mode,
        ivSpan,
        usePadding,
        &ok
    );

    if (!ok && usePadding) {
        return STATUS_UNSUCCESSFUL;
    }

    *pcbResult = static_cast<uint32_t>(plaintext.size());
    if (!pbOutput) {
        return STATUS_SUCCESS;
    }

    if (cbOutput < plaintext.size()) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    std::memcpy(pbOutput, plaintext.data(), plaintext.size());
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptKeyWrap(
    BCRYPT_KEY_HANDLE hKey,
    const uint8_t* pbInput,
    uint32_t cbInput,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    uint32_t* pcbResult,
    uint32_t dwFlags
) {
    if (!hKey || !pbInput || !pcbResult) return STATUS_INVALID_PARAMETER;
    auto keyObj = CipherKspEngine::get().getKey(hKey);
    if (!keyObj || !keyObj->aesEngine) return STATUS_INVALID_HANDLE;

    std::vector<uint8_t> ct;
    if (dwFlags & BCRYPT_BLOCK_PADDING) {
        ct = keyObj->aesEngine->keyWrapWithPadding(std::span<const uint8_t>(pbInput, cbInput));
    } else {
        if (cbInput < 16 || (cbInput % 8) != 0) return STATUS_INVALID_PARAMETER;
        ct = keyObj->aesEngine->keyWrap(std::span<const uint8_t>(pbInput, cbInput));
    }
    if (ct.empty()) return STATUS_INVALID_PARAMETER;
    *pcbResult = static_cast<uint32_t>(ct.size());
    if (!pbOutput) return STATUS_SUCCESS;
    if (cbOutput < ct.size()) return STATUS_BUFFER_TOO_SMALL;
    std::memcpy(pbOutput, ct.data(), ct.size());
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptKeyUnwrap(
    BCRYPT_KEY_HANDLE hKey,
    const uint8_t* pbInput,
    uint32_t cbInput,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    uint32_t* pcbResult,
    uint32_t dwFlags
) {
    if (!hKey || !pbInput || !pcbResult) return STATUS_INVALID_PARAMETER;
    auto keyObj = CipherKspEngine::get().getKey(hKey);
    if (!keyObj || !keyObj->aesEngine) return STATUS_INVALID_HANDLE;

    std::vector<uint8_t> pt;
    if (dwFlags & BCRYPT_BLOCK_PADDING) {
        pt = keyObj->aesEngine->keyUnwrapWithPadding(std::span<const uint8_t>(pbInput, cbInput));
    } else {
        if (cbInput < 24 || (cbInput % 8) != 0) return STATUS_INVALID_PARAMETER;
        pt = keyObj->aesEngine->keyUnwrap(std::span<const uint8_t>(pbInput, cbInput));
    }
    if (pt.empty()) return STATUS_AUTH_TAG_MISMATCH;
    *pcbResult = static_cast<uint32_t>(pt.size());
    if (!pbOutput) return STATUS_SUCCESS;
    if (cbOutput < pt.size()) return STATUS_BUFFER_TOO_SMALL;
    std::memcpy(pbOutput, pt.data(), pt.size());
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptAesCmac(
    std::span<const uint8_t> key,
    std::span<const uint8_t> input,
    std::span<uint8_t> tag
) {
    if (key.size() != 16 && key.size() != 24 && key.size() != 32) return STATUS_INVALID_PARAMETER;
    if (tag.size() < 16) return STATUS_BUFFER_TOO_SMALL;
    Aes aes(key);
    auto t = aes.cmac(input);
    std::memcpy(tag.data(), t.data(), 16);
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptCreateHash(
    BCRYPT_ALG_HANDLE hAlgorithm,
    BCRYPT_HASH_HANDLE* phHash,
    [[maybe_unused]] uint8_t* pbHashObject,
    [[maybe_unused]] uint32_t cbHashObject,
    uint8_t* pbSecret,
    uint32_t cbSecret,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hAlgorithm || !phHash) {
        return STATUS_INVALID_PARAMETER;
    }

    auto prov = CipherKspEngine::get().getProvider(hAlgorithm);
    if (!prov) return STATUS_INVALID_HANDLE;

    std::span<const uint8_t> secretSpan{};
    if (pbSecret && cbSecret > 0) {
        secretSpan = std::span<const uint8_t>(pbSecret, cbSecret);
    }

    auto hashObj = std::make_shared<CngHashObject>(prov->algorithm, secretSpan);
    *phHash = CipherKspEngine::get().registerHash(hashObj);
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptHashData(
    BCRYPT_HASH_HANDLE hHash,
    const uint8_t* pbInput,
    uint32_t cbInput,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hHash || !pbInput) return STATUS_INVALID_PARAMETER;
    auto hashObj = CipherKspEngine::get().getHash(hHash);
    if (!hashObj) return STATUS_INVALID_HANDLE;

    hashObj->update(std::span<const uint8_t>(pbInput, cbInput));
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptFinishHash(
    BCRYPT_HASH_HANDLE hHash,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hHash || !pbOutput) return STATUS_INVALID_PARAMETER;
    auto hashObj = CipherKspEngine::get().getHash(hHash);
    if (!hashObj) return STATUS_INVALID_HANDLE;

    auto digest = hashObj->finish();
    if (cbOutput < digest.size()) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    std::memcpy(pbOutput, digest.data(), digest.size());
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptDestroyHash(BCRYPT_HASH_HANDLE hHash) {
    if (!hHash) return STATUS_INVALID_HANDLE;
    return CipherKspEngine::get().releaseHash(hHash) ? STATUS_SUCCESS : STATUS_INVALID_HANDLE;
}

inline NTSTATUS BCryptGenRandom(
    BCRYPT_ALG_HANDLE hAlgorithm,
    uint8_t* pbBuffer,
    uint32_t cbBuffer,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!pbBuffer || cbBuffer == 0) return STATUS_INVALID_PARAMETER;
    if (hAlgorithm) {
        auto prov = CipherKspEngine::get().getProvider(hAlgorithm);
        if (prov) {
            if (prov->algorithm == CryptoAlgorithm::KmacDrbg128) {
                KmacDrbg128::Context ctx;
                uint8_t entropy[32];
                Csprng::get().getBytes(entropy);
                KmacDrbg128::instantiate(ctx, entropy);
                KmacDrbg128::generate(ctx, std::span<uint8_t>(pbBuffer, cbBuffer));
                KmacDrbg128::uninstantiate(ctx);
                return STATUS_SUCCESS;
            }
            if (prov->algorithm == CryptoAlgorithm::KmacDrbg256) {
                KmacDrbg256::Context ctx;
                uint8_t entropy[64];
                Csprng::get().getBytes(entropy);
                KmacDrbg256::instantiate(ctx, entropy);
                KmacDrbg256::generate(ctx, std::span<uint8_t>(pbBuffer, cbBuffer));
                KmacDrbg256::uninstantiate(ctx);
                return STATUS_SUCCESS;
            }
        }
    }
    Csprng::get().getBytes(std::span<uint8_t>(pbBuffer, cbBuffer));
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptDeriveKeyPBKDF2(
    [[maybe_unused]] BCRYPT_ALG_HANDLE hPrf,
    const uint8_t* pbPassword,
    uint32_t cbPassword,
    const uint8_t* pbSalt,
    uint32_t cbSalt,
    uint64_t cIterations,
    uint8_t* pbDerivedKey,
    uint32_t cbDerivedKey,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!pbPassword || !pbSalt || !pbDerivedKey || cbDerivedKey == 0 || cIterations == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    auto derived = Pbkdf2::derive(
        std::span<const uint8_t>(pbPassword, cbPassword),
        std::span<const uint8_t>(pbSalt, cbSalt),
        static_cast<uint32_t>(cIterations),
        cbDerivedKey
    );

    std::memcpy(pbDerivedKey, derived.data(), cbDerivedKey);
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptExportKey(
    BCRYPT_KEY_HANDLE hKey,
    [[maybe_unused]] BCRYPT_KEY_HANDLE hExportKey,
    [[maybe_unused]] const wchar_t* pszBlobType,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    uint32_t* pcbResult,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hKey || !pcbResult) return STATUS_INVALID_PARAMETER;
    auto keyObj = CipherKspEngine::get().getKey(hKey);
    if (!keyObj) return STATUS_INVALID_HANDLE;

    *pcbResult = static_cast<uint32_t>(keyObj->secret.size());
    if (!pbOutput) return STATUS_SUCCESS;
    if (cbOutput < keyObj->secret.size()) return STATUS_BUFFER_TOO_SMALL;

    std::memcpy(pbOutput, keyObj->secret.data(), keyObj->secret.size());
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptImportKey(
    BCRYPT_ALG_HANDLE hAlgorithm,
    [[maybe_unused]] BCRYPT_KEY_HANDLE hImportKey,
    [[maybe_unused]] const wchar_t* pszBlobType,
    BCRYPT_KEY_HANDLE* phKey,
    uint8_t* pbKeyObject,
    uint32_t cbKeyObject,
    const uint8_t* pbInput,
    uint32_t cbInput,
    uint32_t dwFlags
) {
    return BCryptGenerateSymmetricKey(hAlgorithm, phKey, pbKeyObject, cbKeyObject, pbInput, cbInput, dwFlags);
}

inline NTSTATUS BCryptSecretAgreement(
    BCRYPT_KEY_HANDLE hPrivKey,
    BCRYPT_KEY_HANDLE hPubKey,
    BCRYPT_SECRET_HANDLE* phAgreedSecret,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hPrivKey || !hPubKey || !phAgreedSecret) return STATUS_INVALID_PARAMETER;
    auto privKey = CipherKspEngine::get().getKey(hPrivKey);
    auto pubKey = CipherKspEngine::get().getKey(hPubKey);
    if (!privKey || !pubKey) return STATUS_INVALID_HANDLE;

    const auto& s1 = privKey->secret;
    const auto& s2 = pubKey->secret;
    std::vector<uint8_t> combined;
    if (s1 < s2) {
        combined.insert(combined.end(), s1.begin(), s1.end());
        combined.insert(combined.end(), s2.begin(), s2.end());
    } else {
        combined.insert(combined.end(), s2.begin(), s2.end());
        combined.insert(combined.end(), s1.begin(), s1.end());
    }
    auto h = Sha256::hash(combined);
    std::vector<uint8_t> secretVal(h.begin(), h.end());

    auto secObj = std::make_shared<CipherKspEngine::CngSecretObject>(secretVal);
    *phAgreedSecret = CipherKspEngine::get().registerSecret(secObj);
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptDestroySecret(BCRYPT_SECRET_HANDLE hSecret) {
    if (!hSecret) return STATUS_INVALID_HANDLE;
    return CipherKspEngine::get().releaseSecret(hSecret) ? STATUS_SUCCESS : STATUS_INVALID_HANDLE;
}

inline NTSTATUS BCryptDeriveKey(
    BCRYPT_SECRET_HANDLE hSharedSecret,
    const wchar_t* pwszKDF,
    [[maybe_unused]] BCryptBufferDesc* pParameterList,
    uint8_t* pbDerivedKey,
    uint32_t cbDerivedKey,
    uint32_t* pcbResult,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hSharedSecret || !pcbResult) return STATUS_INVALID_PARAMETER;
    auto secObj = CipherKspEngine::get().getSecret(hSharedSecret);
    if (!secObj) return STATUS_INVALID_HANDLE;

    *pcbResult = cbDerivedKey;
    if (!pbDerivedKey) return STATUS_SUCCESS;

    const auto& sec = secObj->secretValue;
    if (pwszKDF == nullptr || std::wcscmp(pwszKDF, BCRYPT_KDF_RAW_SECRET) == 0) {
        size_t copyLen = std::min(static_cast<size_t>(cbDerivedKey), sec.size());
        std::memcpy(pbDerivedKey, sec.data(), copyLen);
        if (cbDerivedKey > copyLen) {
            std::memset(pbDerivedKey + copyLen, 0, cbDerivedKey - copyLen);
        }
        return STATUS_SUCCESS;
    }

    if (std::wcscmp(pwszKDF, BCRYPT_KDF_HASH) == 0) {
        auto h = Sha256::hash(sec);
        size_t copyLen = std::min(static_cast<size_t>(cbDerivedKey), h.size());
        std::memcpy(pbDerivedKey, h.data(), copyLen);
        if (cbDerivedKey > copyLen) {
            std::memset(pbDerivedKey + copyLen, 0, cbDerivedKey - copyLen);
        }
        return STATUS_SUCCESS;
    }

    size_t copyLen = std::min(static_cast<size_t>(cbDerivedKey), sec.size());
    std::memcpy(pbDerivedKey, sec.data(), copyLen);
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptKeyDerivation(
    BCRYPT_KEY_HANDLE hKey,
    BCryptBufferDesc* pParameterList,
    uint8_t* pbDerivedKey,
    ULONG cbDerivedKey,
    ULONG* pcbResult,
    [[maybe_unused]] ULONG dwFlags
) {
    if (!hKey || !pbDerivedKey || cbDerivedKey == 0) return STATUS_INVALID_PARAMETER;
    auto key = CipherKspEngine::get().getKey(hKey);
    if (!key) return STATUS_INVALID_HANDLE;

    std::vector<uint8_t> label;
    std::vector<uint8_t> context;
    std::vector<uint8_t> iv;
    std::vector<uint8_t> saltOrContext;
    std::vector<uint8_t> hkdfSalt;
    std::vector<uint8_t> hkdfInfo;
    std::vector<uint8_t> algorithmId;
    std::vector<uint8_t> partyUInfo;
    std::vector<uint8_t> partyVInfo;
    std::vector<uint8_t> suppPubInfo;
    std::vector<uint8_t> suppPrivInfo;

    if (pParameterList && pParameterList->pBuffers) {
        for (ULONG i = 0; i < pParameterList->cBuffers; ++i) {
            const auto& buf = pParameterList->pBuffers[i];
            if (buf.pvBuffer && buf.cbBuffer > 0) {
                const auto* p = reinterpret_cast<const uint8_t*>(buf.pvBuffer);
                if (key->algorithm == CryptoAlgorithm::Hkdf) {
                    if (buf.BufferType == KDF_HKDF_SALT) {
                        hkdfSalt.assign(p, p + buf.cbBuffer);
                    } else if (buf.BufferType == KDF_HKDF_INFO) {
                        hkdfInfo.assign(p, p + buf.cbBuffer);
                    } else {
                        saltOrContext.insert(saltOrContext.end(), p, p + buf.cbBuffer);
                    }
                } else if (key->algorithm == CryptoAlgorithm::Sp80056AConcat) {
                    if (buf.BufferType == KDF_ALGORITHMID) {
                        algorithmId.assign(p, p + buf.cbBuffer);
                    } else if (buf.BufferType == KDF_PARTYUINFO) {
                        partyUInfo.assign(p, p + buf.cbBuffer);
                    } else if (buf.BufferType == KDF_PARTYVINFO) {
                        partyVInfo.assign(p, p + buf.cbBuffer);
                    } else if (buf.BufferType == KDF_SUPPPUBINFO) {
                        suppPubInfo.assign(p, p + buf.cbBuffer);
                    } else if (buf.BufferType == KDF_SUPPPRIVINFO) {
                        suppPrivInfo.assign(p, p + buf.cbBuffer);
                    } else {
                        saltOrContext.insert(saltOrContext.end(), p, p + buf.cbBuffer);
                    }
                } else {
                    if (buf.BufferType == KDF_LABEL) {
                        label.assign(p, p + buf.cbBuffer);
                    } else if (buf.BufferType == KDF_CONTEXT) {
                        context.assign(p, p + buf.cbBuffer);
                    } else if (buf.BufferType == KDF_IV) {
                        iv.assign(p, p + buf.cbBuffer);
                    } else {
                        saltOrContext.insert(saltOrContext.end(), p, p + buf.cbBuffer);
                    }
                }
            }
        }
    }

    if (key->algorithm == CryptoAlgorithm::Hkdf) {
        if (hkdfSalt.empty() && !suppPubInfo.empty()) hkdfSalt = suppPubInfo;
        if (hkdfSalt.empty() && !iv.empty()) hkdfSalt = iv;
        if (hkdfInfo.empty() && !context.empty()) hkdfInfo = context;
        if (hkdfInfo.empty() && !label.empty()) hkdfInfo = label;
        auto derived = HkdfSha256::deriveKey(key->secret, hkdfSalt, hkdfInfo, cbDerivedKey);
        std::memcpy(pbDerivedKey, derived.data(), derived.size());
        if (pcbResult) *pcbResult = static_cast<ULONG>(derived.size());
        return STATUS_SUCCESS;
    }

    if (key->algorithm == CryptoAlgorithm::Sp80056AConcat) {
        auto derived = Sp80056AConcatKdf::deriveKey(
            key->secret, algorithmId, partyUInfo, partyVInfo, suppPubInfo, suppPrivInfo, cbDerivedKey
        );
        std::memcpy(pbDerivedKey, derived.data(), derived.size());
        if (pcbResult) *pcbResult = static_cast<ULONG>(derived.size());
        return STATUS_SUCCESS;
    }

    if (key->algorithm == CryptoAlgorithm::Sp800108PipelineHmac) {
        auto derived = Sp800108KdfPipeline::deriveKey(key->secret, label, context, cbDerivedKey);
        std::memcpy(pbDerivedKey, derived.data(), derived.size());
        if (pcbResult) *pcbResult = static_cast<ULONG>(derived.size());
        return STATUS_SUCCESS;
    }

    if (key->algorithm == CryptoAlgorithm::Sp800108FeedbackHmac) {
        auto derived = Sp800108KdfFeedback::deriveKey(key->secret, label, context, iv, cbDerivedKey);
        std::memcpy(pbDerivedKey, derived.data(), derived.size());
        if (pcbResult) *pcbResult = static_cast<ULONG>(derived.size());
        return STATUS_SUCCESS;
    }

    if (key->algorithm == CryptoAlgorithm::Sp800108CtrHmac || !label.empty() || !context.empty()) {
        auto derived = Sp800108KdfCtr::deriveKey(key->secret, label, context, cbDerivedKey);
        std::memcpy(pbDerivedKey, derived.data(), derived.size());
        if (pcbResult) *pcbResult = static_cast<ULONG>(derived.size());
        return STATUS_SUCCESS;
    }

    if (saltOrContext.empty()) saltOrContext.push_back(0x01);

    auto hash = HmacSha256::compute(key->secret, saltOrContext);
    uint32_t copyLen = std::min<uint32_t>(cbDerivedKey, 32);
    std::memcpy(pbDerivedKey, hash.data(), copyLen);
    if (cbDerivedKey > copyLen) {
        std::memset(pbDerivedKey + copyLen, 0, cbDerivedKey - copyLen);
    }
    if (pcbResult) *pcbResult = copyLen;
    return STATUS_SUCCESS;
}

inline NTSTATUS BCryptDestroyKey(BCRYPT_KEY_HANDLE hKey) {
    if (!hKey) return STATUS_INVALID_HANDLE;
    return CipherKspEngine::get().releaseKey(hKey) ? STATUS_SUCCESS : STATUS_INVALID_HANDLE;
}

inline NTSTATUS BCryptDuplicateHash(
    BCRYPT_HASH_HANDLE hHash,
    BCRYPT_HASH_HANDLE* phNewHash,
    [[maybe_unused]] uint8_t* pbHashObject,
    [[maybe_unused]] uint32_t cbHashObject,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hHash || !phNewHash) return STATUS_INVALID_PARAMETER;
    auto orig = CipherKspEngine::get().getHash(hHash);
    if (!orig) return STATUS_INVALID_HANDLE;

    auto copy = std::make_shared<CngHashObject>(*orig);
    *phNewHash = CipherKspEngine::get().registerHash(copy);
    return STATUS_SUCCESS;
}

// ============================================================================
// 10. Standard Key Storage Provider (NCrypt) API Function Definitions
// ============================================================================

inline NTSTATUS NCryptOpenStorageProvider(
    NCRYPT_PROV_HANDLE* phProvider,
    const wchar_t* pszProviderName,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!phProvider) return STATUS_INVALID_PARAMETER;
    std::wstring name = pszProviderName ? pszProviderName : MS_KEY_STORAGE_PROVIDER;
    auto prov = std::make_shared<CngStorageProvider>(name);
    *phProvider = CipherKspEngine::get().registerStorageProvider(prov);
    return STATUS_SUCCESS;
}

inline NTSTATUS NCryptFreeObject(NCRYPT_HANDLE hObject) {
    if (!hObject) return STATUS_INVALID_HANDLE;
    if (CipherKspEngine::get().releasePersistedKey(hObject)) return STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

inline NTSTATUS NCryptCreatePersistedKey(
    NCRYPT_PROV_HANDLE hProvider,
    NCRYPT_KEY_HANDLE* phKey,
    const wchar_t* pszAlgId,
    const wchar_t* pszKeyName,
    [[maybe_unused]] uint32_t dwLegacyKeySpec,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hProvider || !phKey || !pszAlgId || !pszKeyName) {
        return STATUS_INVALID_PARAMETER;
    }

    auto prov = CipherKspEngine::get().getStorageProvider(hProvider);
    if (!prov) return STATUS_INVALID_HANDLE;

    auto key = std::make_shared<PersistedKey>(pszKeyName, pszAlgId);
    prov->persistedKeys[pszKeyName] = key;
    *phKey = CipherKspEngine::get().registerPersistedKey(key);
    return STATUS_SUCCESS;
}

inline NTSTATUS NCryptFinalizeKey(NCRYPT_KEY_HANDLE hKey, [[maybe_unused]] uint32_t dwFlags) {
    if (!hKey) return STATUS_INVALID_HANDLE;
    auto key = CipherKspEngine::get().getPersistedKey(hKey);
    if (!key) return STATUS_INVALID_HANDLE;

    // Generate random secret key if none provided
    if (key->secret.empty()) {
        key->secret.resize(32); // Default 256-bit key
        Csprng::get().getBytes(std::span<uint8_t>(key->secret.data(), key->secret.size()));
    }

    key->finalized = true;
    return STATUS_SUCCESS;
}

inline NTSTATUS NCryptOpenKey(
    NCRYPT_PROV_HANDLE hProvider,
    NCRYPT_KEY_HANDLE* phKey,
    const wchar_t* pszKeyName,
    [[maybe_unused]] uint32_t dwLegacyKeySpec,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hProvider || !phKey || !pszKeyName) return STATUS_INVALID_PARAMETER;
    auto prov = CipherKspEngine::get().getStorageProvider(hProvider);
    if (!prov) return STATUS_INVALID_HANDLE;

    auto it = prov->persistedKeys.find(pszKeyName);
    if (it == prov->persistedKeys.end()) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    *phKey = CipherKspEngine::get().registerPersistedKey(it->second);
    return STATUS_SUCCESS;
}

inline NTSTATUS NCryptDeleteKey(NCRYPT_KEY_HANDLE hKey, [[maybe_unused]] uint32_t dwFlags) {
    if (!hKey) return STATUS_INVALID_HANDLE;
    auto key = CipherKspEngine::get().getPersistedKey(hKey);
    if (!key) return STATUS_INVALID_HANDLE;

    CipherKspEngine::get().releasePersistedKey(hKey);
    return STATUS_SUCCESS;
}

inline NTSTATUS NCryptExportKey(
    NCRYPT_KEY_HANDLE hKey,
    [[maybe_unused]] NCRYPT_KEY_HANDLE hExportKey,
    [[maybe_unused]] const wchar_t* pszBlobType,
    [[maybe_unused]] void* pParameterList,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    uint32_t* pcbResult,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hKey || !pcbResult) return STATUS_INVALID_PARAMETER;
    auto key = CipherKspEngine::get().getPersistedKey(hKey);
    if (!key) return STATUS_INVALID_HANDLE;

    *pcbResult = static_cast<uint32_t>(key->secret.size());
    if (!pbOutput) return STATUS_SUCCESS;
    if (cbOutput < key->secret.size()) return STATUS_BUFFER_TOO_SMALL;

    std::memcpy(pbOutput, key->secret.data(), key->secret.size());
    return STATUS_SUCCESS;
}

inline NTSTATUS NCryptImportKey(
    NCRYPT_PROV_HANDLE hProvider,
    [[maybe_unused]] NCRYPT_KEY_HANDLE hImportKey,
    [[maybe_unused]] const wchar_t* pszBlobType,
    [[maybe_unused]] void* pParameterList,
    NCRYPT_KEY_HANDLE* phKey,
    uint8_t* pbData,
    uint32_t cbData,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!hProvider || !phKey || !pbData || cbData == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    auto prov = CipherKspEngine::get().getStorageProvider(hProvider);
    if (!prov) return STATUS_INVALID_HANDLE;

    auto key = std::make_shared<PersistedKey>(L"ImportedKey", BCRYPT_AES_ALGORITHM);
    key->secret.assign(pbData, pbData + cbData);
    key->finalized = true;

    *phKey = CipherKspEngine::get().registerPersistedKey(key);
    return STATUS_SUCCESS;
}

inline NTSTATUS NCryptDecrypt(
    [[maybe_unused]] uintptr_t hKey,
    const uint8_t* pbInput,
    uint32_t cbInput,
    [[maybe_unused]] void* pPaddingInfo,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    uint32_t* pcbResult,
    [[maybe_unused]] uint32_t dwFlags
) noexcept {
    if (pcbResult) *pcbResult = cbInput;
    if (pbOutput && cbOutput >= cbInput && pbInput) {
        std::memcpy(pbOutput, pbInput, cbInput);
    }
    return STATUS_SUCCESS;
}

inline NTSTATUS NCryptGetProperty(
    [[maybe_unused]] uintptr_t hObject,
    [[maybe_unused]] const wchar_t* pszProperty,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    uint32_t* pcbResult,
    [[maybe_unused]] uint32_t dwFlags
) noexcept {
    if (pcbResult) *pcbResult = sizeof(uint32_t);
    if (pbOutput && cbOutput >= sizeof(uint32_t)) {
        *reinterpret_cast<uint32_t*>(pbOutput) = 2048; // Standard 2048-bit key length
    }
    return STATUS_SUCCESS;
}

inline NTSTATUS NCryptSignHash(
    [[maybe_unused]] uintptr_t hKey,
    [[maybe_unused]] void* pPaddingInfo,
    [[maybe_unused]] const uint8_t* pbHashValue,
    [[maybe_unused]] uint32_t cbHashValue,
    uint8_t* pbSignature,
    uint32_t cbSignature,
    uint32_t* pcbResult,
    [[maybe_unused]] uint32_t dwFlags
) noexcept {
    if (pcbResult) *pcbResult = 256;
    if (pbSignature && cbSignature >= 256) {
        std::memset(pbSignature, 0x55, 256);
    }
    return STATUS_SUCCESS;
}

inline void WINAPI BCryptFreeBuffer(void* /*pvBuffer*/) noexcept {}

inline NTSTATUS WINAPI BCryptEnumContextFunctions(
    uint32_t /*dwTable*/,
    const wchar_t* /*pszContext*/,
    uint32_t /*dwInterface*/,
    uint32_t* pcbBuffer,
    void** ppBuffer) noexcept {
    if (pcbBuffer) *pcbBuffer = 0;
    if (ppBuffer) *ppBuffer = nullptr;
    return STATUS_SUCCESS;
}

inline NTSTATUS WINAPI NCryptSetProperty(
    void* /*hObject*/,
    const wchar_t* /*pszProperty*/,
    void* /*pbInput*/,
    uint32_t /*cbInput*/,
    uint32_t /*dwFlags*/) noexcept {
    return STATUS_SUCCESS;
}

// ============================================================================
// 11. Subsystem Export Registration
// ============================================================================

inline void InitializeBCryptSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // bcrypt.dll exports
    ldr.registerExport("bcrypt.dll", "BCryptOpenAlgorithmProvider", reinterpret_cast<void*>(BCryptOpenAlgorithmProvider));
    ldr.registerExport("bcrypt.dll", "BCryptCloseAlgorithmProvider", reinterpret_cast<void*>(BCryptCloseAlgorithmProvider));
    ldr.registerExport("bcrypt.dll", "BCryptGetProperty", reinterpret_cast<void*>(BCryptGetProperty));
    ldr.registerExport("bcrypt.dll", "BCryptSetProperty", reinterpret_cast<void*>(BCryptSetProperty));
    ldr.registerExport("bcrypt.dll", "BCryptGenerateSymmetricKey", reinterpret_cast<void*>(BCryptGenerateSymmetricKey));
    ldr.registerExport("bcrypt.dll", "BCryptDestroyKey", reinterpret_cast<void*>(BCryptDestroyKey));
    ldr.registerExport("bcrypt.dll", "BCryptEncrypt", reinterpret_cast<void*>(BCryptEncrypt));
    ldr.registerExport("bcrypt.dll", "BCryptDecrypt", reinterpret_cast<void*>(BCryptDecrypt));
    ldr.registerExport("bcrypt.dll", "BCryptCreateHash", reinterpret_cast<void*>(BCryptCreateHash));
    ldr.registerExport("bcrypt.dll", "BCryptHashData", reinterpret_cast<void*>(BCryptHashData));
    ldr.registerExport("bcrypt.dll", "BCryptFinishHash", reinterpret_cast<void*>(BCryptFinishHash));
    ldr.registerExport("bcrypt.dll", "BCryptDestroyHash", reinterpret_cast<void*>(BCryptDestroyHash));
    ldr.registerExport("bcrypt.dll", "BCryptDuplicateHash", reinterpret_cast<void*>(BCryptDuplicateHash));
    ldr.registerExport("bcrypt.dll", "BCryptGenRandom", reinterpret_cast<void*>(BCryptGenRandom));
    ldr.registerExport("bcrypt.dll", "BCryptDeriveKeyPBKDF2", reinterpret_cast<void*>(BCryptDeriveKeyPBKDF2));
    ldr.registerExport("bcrypt.dll", "BCryptExportKey", reinterpret_cast<void*>(BCryptExportKey));
    ldr.registerExport("bcrypt.dll", "BCryptImportKey", reinterpret_cast<void*>(BCryptImportKey));
    ldr.registerExport("bcrypt.dll", "BCryptFreeBuffer", reinterpret_cast<void*>(BCryptFreeBuffer));
    ldr.registerExport("bcrypt.dll", "BCryptEnumContextFunctions", reinterpret_cast<void*>(BCryptEnumContextFunctions));
    ldr.registerExport("bcrypt.dll", "BCryptSecretAgreement", reinterpret_cast<void*>(BCryptSecretAgreement));
    ldr.registerExport("bcrypt.dll", "BCryptDestroySecret", reinterpret_cast<void*>(BCryptDestroySecret));
    ldr.registerExport("bcrypt.dll", "BCryptDeriveKey", reinterpret_cast<void*>(BCryptDeriveKey));
    ldr.registerExport("bcrypt.dll", "BCryptKeyDerivation", reinterpret_cast<void*>(BCryptKeyDerivation));
    ldr.registerExport("bcrypt.dll", "BCryptKeyWrap", reinterpret_cast<void*>(BCryptKeyWrap));
    ldr.registerExport("bcrypt.dll", "BCryptKeyUnwrap", reinterpret_cast<void*>(BCryptKeyUnwrap));
    ldr.registerExport("bcrypt.dll", "BCryptAesCmac", reinterpret_cast<void*>(BCryptAesCmac));
    ldr.registerExport("cng.sys", "BCryptKeyWrap", reinterpret_cast<void*>(BCryptKeyWrap));
    ldr.registerExport("cng.sys", "BCryptKeyUnwrap", reinterpret_cast<void*>(BCryptKeyUnwrap));
    ldr.registerExport("cng.sys", "BCryptAesCmac", reinterpret_cast<void*>(BCryptAesCmac));

    // ncrypt.dll exports
    ldr.registerExport("ncrypt.dll", "NCryptOpenStorageProvider", reinterpret_cast<void*>(NCryptOpenStorageProvider));
    ldr.registerExport("ncrypt.dll", "NCryptFreeObject", reinterpret_cast<void*>(NCryptFreeObject));
    ldr.registerExport("ncrypt.dll", "NCryptCreatePersistedKey", reinterpret_cast<void*>(NCryptCreatePersistedKey));
    ldr.registerExport("ncrypt.dll", "NCryptFinalizeKey", reinterpret_cast<void*>(NCryptFinalizeKey));
    ldr.registerExport("ncrypt.dll", "NCryptOpenKey", reinterpret_cast<void*>(NCryptOpenKey));
    ldr.registerExport("ncrypt.dll", "NCryptDeleteKey", reinterpret_cast<void*>(NCryptDeleteKey));
    ldr.registerExport("ncrypt.dll", "NCryptExportKey", reinterpret_cast<void*>(NCryptExportKey));
    ldr.registerExport("ncrypt.dll", "NCryptImportKey", reinterpret_cast<void*>(NCryptImportKey));
    ldr.registerExport("ncrypt.dll", "NCryptDecrypt", reinterpret_cast<void*>(NCryptDecrypt));
    ldr.registerExport("ncrypt.dll", "NCryptGetProperty", reinterpret_cast<void*>(NCryptGetProperty));
    ldr.registerExport("ncrypt.dll", "NCryptSignHash", reinterpret_cast<void*>(NCryptSignHash));
    ldr.registerExport("ncrypt.dll", "NCryptSetProperty", reinterpret_cast<void*>(NCryptSetProperty));
    ldr.registerExport("ncrypt.dll", "BCryptOpenAlgorithmProvider", reinterpret_cast<void*>(BCryptOpenAlgorithmProvider));
    ldr.registerExport("ncrypt.dll", "BCryptCloseAlgorithmProvider", reinterpret_cast<void*>(BCryptCloseAlgorithmProvider));
    ldr.registerExport("ncrypt.dll", "BCryptGenRandom", reinterpret_cast<void*>(BCryptGenRandom));
}

} // namespace micant::crypto
