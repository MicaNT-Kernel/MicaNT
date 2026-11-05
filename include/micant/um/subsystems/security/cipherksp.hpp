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
inline constexpr const wchar_t* BCRYPT_SHA3_512_ALGORITHM               = L"SHA3-512";

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
    Sha3_512,
    Unknown
};

inline CryptoAlgorithm parseAlgorithm(std::wstring_view algId) {
    if (algId == BCRYPT_SHA256_ALGORITHM || algId == L"SHA-256") return CryptoAlgorithm::Sha256;
    if (algId == BCRYPT_SHA384_ALGORITHM || algId == L"SHA-384") return CryptoAlgorithm::Sha384;
    if (algId == BCRYPT_SHA512_ALGORITHM || algId == L"SHA-512") return CryptoAlgorithm::Sha512;
    if (algId == BCRYPT_SHA3_256_ALGORITHM || algId == L"SHA3-256" || algId == L"sha3-256") return CryptoAlgorithm::Sha3_256;
    if (algId == BCRYPT_SHA3_512_ALGORITHM || algId == L"SHA3-512" || algId == L"sha3-512") return CryptoAlgorithm::Sha3_512;
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
    Sha3_512::Context sha3_512Ctx{};

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
          sha3_512Ctx(other.sha3_512Ctx),
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
        } else if (algorithm == CryptoAlgorithm::Sha3_512) {
            Sha3_512::init(sha3_512Ctx);
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
        } else if (algorithm == CryptoAlgorithm::Sha3_512) {
            Sha3_512::update(sha3_512Ctx, data);
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
        } else if (algorithm == CryptoAlgorithm::Sha3_512) {
            std::vector<uint8_t> out(64);
            Sha3_512::final(sha3_512Ctx, std::span<uint8_t, 64>(out.data(), 64));
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
            if (prov->algorithm == CryptoAlgorithm::Md5 || prov->algorithm == CryptoAlgorithm::AesCmac) len = 16;
            else if (prov->algorithm == CryptoAlgorithm::Sha1) len = 20;
            else if (prov->algorithm == CryptoAlgorithm::Sha256 || prov->algorithm == CryptoAlgorithm::HmacSha256 || prov->algorithm == CryptoAlgorithm::Sha3_256) len = 32;
            else if (prov->algorithm == CryptoAlgorithm::Sha384 || prov->algorithm == CryptoAlgorithm::HmacSha384) len = 48;
            else if (prov->algorithm == CryptoAlgorithm::Sha512 || prov->algorithm == CryptoAlgorithm::HmacSha512 || prov->algorithm == CryptoAlgorithm::Sha3_512) len = 64;
            std::memcpy(pbOutput, &len, sizeof(len));
            return STATUS_SUCCESS;
        }
        if (std::wstring_view(pszProperty) == BCRYPT_BLOCK_LENGTH) {
            *pcbResult = sizeof(uint32_t);
            if (!pbOutput) return STATUS_SUCCESS;
            if (cbOutput < sizeof(uint32_t)) return STATUS_BUFFER_TOO_SMALL;
            uint32_t len = (prov->algorithm == CryptoAlgorithm::Aes || prov->algorithm == CryptoAlgorithm::AesCmac) ? 16 :
                           ((prov->algorithm == CryptoAlgorithm::Sha3_256) ? 136 :
                           ((prov->algorithm == CryptoAlgorithm::Sha3_512) ? 72 :
                           ((prov->algorithm == CryptoAlgorithm::Sha384 || prov->algorithm == CryptoAlgorithm::Sha512 ||
                             prov->algorithm == CryptoAlgorithm::HmacSha384 || prov->algorithm == CryptoAlgorithm::HmacSha512) ? 128 : 64)));
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
                  prov->algorithm != CryptoAlgorithm::Hkdf)) {
        return STATUS_NOT_SUPPORTED;
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
    if (!keyObj || !keyObj->aesEngine) {
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
    if (!keyObj || !keyObj->aesEngine) {
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
    [[maybe_unused]] BCRYPT_ALG_HANDLE hAlgorithm,
    uint8_t* pbBuffer,
    uint32_t cbBuffer,
    [[maybe_unused]] uint32_t dwFlags
) {
    if (!pbBuffer || cbBuffer == 0) return STATUS_INVALID_PARAMETER;
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
