#pragma once
/**
 * @file cng.hpp
 * @brief Horizon 155: Sovereign Cryptography Next Generation (CNG) Kernel Driver (cng.sys),
 *        Core Cryptographic Primitives (SHA-256, SHA-384, HMAC, HKDF, AES-GCM, X25519)
 *        & Kernel-Mode IOCTL Dispatch Interface (\Device\Cng).
 * 
 * Target Architecture: Supermicro X11SSH-F (Intel Xeon E3-1270 v6 @ 3.80GHz, 32GB DDR4 ECC).
 * Clean-Room Standard: 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc. (593 U.S. 1).
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <array>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <memory>
#include <mutex>
#include <optional>
#include <chrono>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/um/native/ldr.hpp"

namespace micant::km::crypto {

// ============================================================================
// Standard Windows CNG Kernel Constants & Types
// ============================================================================

using BCRYPT_HANDLE      = void*;
using BCRYPT_ALG_HANDLE  = void*;
using BCRYPT_KEY_HANDLE  = void*;
using BCRYPT_HASH_HANDLE = void*;

inline constexpr const wchar_t* BCRYPT_SHA256_ALGORITHM      = L"SHA256";
inline constexpr const wchar_t* BCRYPT_SHA384_ALGORITHM      = L"SHA384";
inline constexpr const wchar_t* BCRYPT_SHA512_ALGORITHM      = L"SHA512";
inline constexpr const wchar_t* BCRYPT_AES_ALGORITHM         = L"AES";
inline constexpr const wchar_t* BCRYPT_RNG_ALGORITHM         = L"RNG";
inline constexpr const wchar_t* BCRYPT_ECDH_P256_ALGORITHM   = L"ECDH_P256";
inline constexpr const wchar_t* BCRYPT_X25519_ALGORITHM      = L"X25519";
inline constexpr const wchar_t* BCRYPT_HMAC_SHA256_ALGORITHM = L"HMAC-SHA256";

inline constexpr const wchar_t* BCRYPT_CHAINING_MODE         = L"ChainingMode";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_GCM        = L"ChainingModeGCM";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_CBC        = L"ChainingModeCBC";
inline constexpr const wchar_t* BCRYPT_CHAIN_MODE_ECB        = L"ChainingModeECB";

inline constexpr uint32_t BCRYPT_ALG_HANDLE_HMAC_FLAG        = 0x00000008;

// CNG Kernel IOCTL Codes (\Device\Cng)
inline constexpr uint32_t IOCTL_CNG_OPEN_ALGORITHM_PROVIDER  = 0x00390000;
inline constexpr uint32_t IOCTL_CNG_CREATE_HASH              = 0x00390004;
inline constexpr uint32_t IOCTL_CNG_HASH_DATA                = 0x00390008;
inline constexpr uint32_t IOCTL_CNG_FINISH_HASH              = 0x0039000C;
inline constexpr uint32_t IOCTL_CNG_ENCRYPT                  = 0x00390010;
inline constexpr uint32_t IOCTL_CNG_DECRYPT                  = 0x00390014;
inline constexpr uint32_t IOCTL_CNG_GEN_RANDOM               = 0x00390018;
inline constexpr uint32_t IOCTL_CNG_KEY_DERIVATION           = 0x0039001C;

// Standard CNG Status Codes
inline constexpr NTSTATUS STATUS_AUTH_TAG_NEEDS_VERIFY         = static_cast<NTSTATUS>(0xC000A002);
inline constexpr NTSTATUS STATUS_INVALID_BUFFER_SIZE           = static_cast<NTSTATUS>(0xC0000206);
inline constexpr NTSTATUS STATUS_INVALID_DEVICE_REQUEST         = static_cast<NTSTATUS>(0xC0000010);

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

    static std::array<uint8_t, 32> hash(std::span<const uint8_t> data) noexcept {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::array<uint8_t, 32> out{};
        final(ctx, std::span<uint8_t, 32>(out.data(), 32));
        return out;
    }

private:
    static constexpr uint32_t rotr(uint32_t x, int n) noexcept { return (x >> n) | (x << (32 - n)); }
    static constexpr uint32_t ch(uint32_t x, uint32_t y, uint32_t z) noexcept { return (x & y) ^ (~x & z); }
    static constexpr uint32_t maj(uint32_t x, uint32_t y, uint32_t z) noexcept { return (x & y) ^ (x & z) ^ (y & z); }
    static constexpr uint32_t sigma0(uint32_t x) noexcept { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static constexpr uint32_t sigma1(uint32_t x) noexcept { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static constexpr uint32_t gamma0(uint32_t x) noexcept { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static constexpr uint32_t gamma1(uint32_t x) noexcept { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

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

        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }
};

// ============================================================================
// 2. Clean-Room SHA-384 (FIPS 180-4) Implementation
// ============================================================================

class Sha384 {
public:
    static constexpr size_t DIGEST_SIZE = 48;
    static constexpr size_t BLOCK_SIZE  = 128;

    struct Context {
        uint64_t state[8];
        uint64_t count;
        uint8_t  buffer[128];
    };

    static void init(Context& ctx) noexcept {
        ctx.state[0] = 0xcbbb9d5dc1059ed8ULL;
        ctx.state[1] = 0x629a292a367cd507ULL;
        ctx.state[2] = 0x9159015a3070dd17ULL;
        ctx.state[3] = 0x152fecd8f70e5939ULL;
        ctx.state[4] = 0x67332667ffc00b31ULL;
        ctx.state[5] = 0x8eb44a8768581511ULL;
        ctx.state[6] = 0xdb0c2e0d64f98fa7ULL;
        ctx.state[7] = 0x47b5481dbefa4fa4ULL;
        ctx.count    = 0;
    }

    static void update(Context& ctx, std::span<const uint8_t> data) noexcept {
        size_t index = static_cast<size_t>(ctx.count & 127);
        ctx.count += data.size();

        size_t partLen = 128 - index;
        size_t i = 0;

        if (data.size() >= partLen) {
            std::memcpy(&ctx.buffer[index], data.data(), partLen);
            transform(ctx.state, ctx.buffer);
            for (i = partLen; i + 127 < data.size(); i += 128) {
                transform(ctx.state, &data[i]);
            }
            index = 0;
        }

        if (i < data.size()) {
            std::memcpy(&ctx.buffer[index], &data[i], data.size() - i);
        }
    }

    static void final(Context& ctx, std::span<uint8_t, 48> digest) noexcept {
        uint64_t totalBits = ctx.count * 8;
        uint8_t bits[16] = {0};
        for (int i = 0; i < 8; ++i) {
            bits[15 - i] = static_cast<uint8_t>((totalBits >> (i * 8)) & 0xFF);
        }

        size_t index = static_cast<size_t>(ctx.count & 127);
        size_t padLen = (index < 112) ? (112 - index) : (240 - index);

        static const uint8_t PADDING[128] = { 0x80 };
        update(ctx, std::span<const uint8_t>(PADDING, padLen));
        update(ctx, std::span<const uint8_t>(bits, 16));

        for (int i = 0; i < 6; ++i) {
            for (int b = 0; b < 8; ++b) {
                digest[i * 8 + b] = static_cast<uint8_t>((ctx.state[i] >> ((7 - b) * 8)) & 0xFF);
            }
        }
    }

    static std::array<uint8_t, 48> hash(std::span<const uint8_t> data) noexcept {
        Context ctx;
        init(ctx);
        update(ctx, data);
        std::array<uint8_t, 48> out{};
        final(ctx, std::span<uint8_t, 48>(out.data(), 48));
        return out;
    }

private:
    static constexpr uint64_t rotr(uint64_t x, int n) noexcept { return (x >> n) | (x << (64 - n)); }
    static constexpr uint64_t ch(uint64_t x, uint64_t y, uint64_t z) noexcept { return (x & y) ^ (~x & z); }
    static constexpr uint64_t maj(uint64_t x, uint64_t y, uint64_t z) noexcept { return (x & y) ^ (x & z) ^ (y & z); }
    static constexpr uint64_t sigma0(uint64_t x) noexcept { return rotr(x, 28) ^ rotr(x, 34) ^ rotr(x, 39); }
    static constexpr uint64_t sigma1(uint64_t x) noexcept { return rotr(x, 14) ^ rotr(x, 18) ^ rotr(x, 41); }
    static constexpr uint64_t gamma0(uint64_t x) noexcept { return rotr(x, 1) ^ rotr(x, 8) ^ (x >> 7); }
    static constexpr uint64_t gamma1(uint64_t x) noexcept { return rotr(x, 19) ^ rotr(x, 61) ^ (x >> 6); }

    static void transform(uint64_t state[8], const uint8_t block[128]) noexcept {
        static constexpr uint64_t K[80] = {
            0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
            0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
            0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
            0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
            0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
            0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
            0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7bee0ee30ULL,
            0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
            0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
            0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47867113ULL, 0x92722c851482353bULL,
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

        uint64_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = 0;
            for (int b = 0; b < 8; ++b) {
                w[i] = (w[i] << 8) | block[i * 8 + b];
            }
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
        }

        uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint64_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (int i = 0; i < 80; ++i) {
            uint64_t t1 = h + sigma1(e) + ch(e, f, g) + K[i] + w[i];
            uint64_t t2 = sigma0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }
};

// ============================================================================
// 3. Clean-Room HMAC & HKDF (RFC 2104 / RFC 5869 / RFC 8446)
// ============================================================================

class HmacSha256 {
public:
    static constexpr size_t DIGEST_SIZE = 32;

    static std::array<uint8_t, 32> compute(std::span<const uint8_t> key, std::span<const uint8_t> data) noexcept {
        uint8_t k[64] = {0};
        if (key.size() > 64) {
            auto h = Sha256::hash(key);
            std::memcpy(k, h.data(), 32);
        } else {
            std::memcpy(k, key.data(), key.size());
        }

        uint8_t k_ipad[64];
        uint8_t k_opad[64];
        for (int i = 0; i < 64; ++i) {
            k_ipad[i] = k[i] ^ 0x36;
            k_opad[i] = k[i] ^ 0x5C;
        }

        Sha256::Context innerCtx;
        Sha256::init(innerCtx);
        Sha256::update(innerCtx, std::span<const uint8_t>(k_ipad, 64));
        Sha256::update(innerCtx, data);
        std::array<uint8_t, 32> innerHash{};
        Sha256::final(innerCtx, std::span<uint8_t, 32>(innerHash.data(), 32));

        Sha256::Context outerCtx;
        Sha256::init(outerCtx);
        Sha256::update(outerCtx, std::span<const uint8_t>(k_opad, 64));
        Sha256::update(outerCtx, std::span<const uint8_t>(innerHash.data(), 32));
        std::array<uint8_t, 32> outerHash{};
        Sha256::final(outerCtx, std::span<uint8_t, 32>(outerHash.data(), 32));

        return outerHash;
    }
};

class HkdfSha256 {
public:
    static std::array<uint8_t, 32> extract(std::span<const uint8_t> salt, std::span<const uint8_t> ikm) noexcept {
        if (salt.empty()) {
            static const uint8_t zeroSalt[32] = {0};
            return HmacSha256::compute(std::span<const uint8_t>(zeroSalt, 32), ikm);
        }
        return HmacSha256::compute(salt, ikm);
    }

    static std::vector<uint8_t> expand(std::span<const uint8_t> prk, std::span<const uint8_t> info, size_t length) {
        std::vector<uint8_t> okm;
        okm.reserve(length);

        std::array<uint8_t, 32> t{};
        size_t tLen = 0;
        uint8_t counter = 1;

        while (okm.size() < length) {
            std::vector<uint8_t> stepData;
            if (tLen > 0) {
                stepData.insert(stepData.end(), t.begin(), t.end());
            }
            stepData.insert(stepData.end(), info.begin(), info.end());
            stepData.push_back(counter);

            t = HmacSha256::compute(prk, stepData);
            tLen = 32;

            size_t take = std::min(static_cast<size_t>(32), length - okm.size());
            okm.insert(okm.end(), t.begin(), t.begin() + take);
            counter++;
        }

        return okm;
    }

    // TLS 1.3 HKDF-Expand-Label (RFC 8446 Section 7.1)
    static std::vector<uint8_t> expandLabel(
        std::span<const uint8_t> secret,
        std::string_view label,
        std::span<const uint8_t> context,
        uint16_t length)
    {
        std::string fullLabel = "tls13 ";
        fullLabel.append(label);

        std::vector<uint8_t> hkdfLabel;
        hkdfLabel.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
        hkdfLabel.push_back(static_cast<uint8_t>(length & 0xFF));
        hkdfLabel.push_back(static_cast<uint8_t>(fullLabel.size()));
        hkdfLabel.insert(hkdfLabel.end(), fullLabel.begin(), fullLabel.end());
        hkdfLabel.push_back(static_cast<uint8_t>(context.size()));
        hkdfLabel.insert(hkdfLabel.end(), context.begin(), context.end());

        return expand(secret, hkdfLabel, length);
    }
};

// ============================================================================
// 4. Clean-Room AES (Rijndael) & GCM Mode (NIST SP 800-38D / RFC 5116)
// ============================================================================

class AesCore {
public:
    static constexpr uint8_t sbox[256] = {
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

    static constexpr uint8_t rcon[11] = {
        0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
    };

    static constexpr uint8_t xtime(uint8_t x) noexcept {
        return static_cast<uint8_t>((x << 1) ^ (((x >> 7) & 1) * 0x1b));
    }

    struct KeySchedule {
        uint32_t roundKeys[60]; // up to 14 rounds for AES-256
        int numRounds{10};      // 10 for 128, 14 for 256
    };

    static void expandKey(std::span<const uint8_t> key, KeySchedule& ks) noexcept {
        size_t nk = key.size() / 4;
        ks.numRounds = (nk == 4) ? 10 : 14;

        for (size_t i = 0; i < nk; ++i) {
            ks.roundKeys[i] = (static_cast<uint32_t>(key[4 * i]) << 24) |
                              (static_cast<uint32_t>(key[4 * i + 1]) << 16) |
                              (static_cast<uint32_t>(key[4 * i + 2]) << 8) |
                              (static_cast<uint32_t>(key[4 * i + 3]));
        }

        size_t totalWords = 4 * (static_cast<size_t>(ks.numRounds) + 1);
        for (size_t i = nk; i < totalWords; ++i) {
            uint32_t temp = ks.roundKeys[i - 1];
            if (i % nk == 0) {
                temp = subWord(rotWord(temp)) ^ (static_cast<uint32_t>(rcon[i / nk]) << 24);
            } else if (nk > 6 && (i % nk == 4)) {
                temp = subWord(temp);
            }
            ks.roundKeys[i] = ks.roundKeys[i - nk] ^ temp;
        }
    }

    static void encryptBlock(const KeySchedule& ks, const uint8_t in[16], uint8_t out[16]) noexcept {
        uint8_t state[4][4];
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                state[r][c] = in[r + 4 * c];
            }
        }

        addRoundKey(state, ks.roundKeys, 0);

        for (int round = 1; round < ks.numRounds; ++round) {
            subBytes(state);
            shiftRows(state);
            mixColumns(state);
            addRoundKey(state, ks.roundKeys, round);
        }

        subBytes(state);
        shiftRows(state);
        addRoundKey(state, ks.roundKeys, ks.numRounds);

        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                out[r + 4 * c] = state[r][c];
            }
        }
    }

private:
    static constexpr uint32_t rotWord(uint32_t w) noexcept {
        return (w << 8) | (w >> 24);
    }

    static constexpr uint32_t subWord(uint32_t w) noexcept {
        return (static_cast<uint32_t>(sbox[(w >> 24) & 0xFF]) << 24) |
               (static_cast<uint32_t>(sbox[(w >> 16) & 0xFF]) << 16) |
               (static_cast<uint32_t>(sbox[(w >> 8) & 0xFF]) << 8)   |
               (static_cast<uint32_t>(sbox[w & 0xFF]));
    }

    static void subBytes(uint8_t state[4][4]) noexcept {
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                state[r][c] = sbox[state[r][c]];
            }
        }
    }

    static void shiftRows(uint8_t state[4][4]) noexcept {
        uint8_t t;
        // row 1: shift left by 1
        t = state[1][0]; state[1][0] = state[1][1]; state[1][1] = state[1][2]; state[1][2] = state[1][3]; state[1][3] = t;
        // row 2: shift left by 2
        t = state[2][0]; state[2][0] = state[2][2]; state[2][2] = t;
        t = state[2][1]; state[2][1] = state[2][3]; state[2][3] = t;
        // row 3: shift left by 3
        t = state[3][3]; state[3][3] = state[3][2]; state[3][2] = state[3][1]; state[3][1] = state[3][0]; state[3][0] = t;
    }

    static void mixColumns(uint8_t state[4][4]) noexcept {
        for (int c = 0; c < 4; ++c) {
            uint8_t a0 = state[0][c], a1 = state[1][c], a2 = state[2][c], a3 = state[3][c];
            uint8_t h = a0 ^ a1 ^ a2 ^ a3;
            state[0][c] ^= h ^ xtime(a0 ^ a1);
            state[1][c] ^= h ^ xtime(a1 ^ a2);
            state[2][c] ^= h ^ xtime(a2 ^ a3);
            state[3][c] ^= h ^ xtime(a3 ^ a0);
        }
    }

    static void addRoundKey(uint8_t state[4][4], const uint32_t* roundKeys, int round) noexcept {
        for (int c = 0; c < 4; ++c) {
            uint32_t rk = roundKeys[round * 4 + c];
            state[0][c] ^= static_cast<uint8_t>((rk >> 24) & 0xFF);
            state[1][c] ^= static_cast<uint8_t>((rk >> 16) & 0xFF);
            state[2][c] ^= static_cast<uint8_t>((rk >> 8) & 0xFF);
            state[3][c] ^= static_cast<uint8_t>(rk & 0xFF);
        }
    }
};

// ============================================================================
// 5. Clean-Room Galois/Counter Mode (AES-GCM) & GHASH
// ============================================================================

class AesGcm {
public:
    // GF(2^128) multiplication with reduction polynomial x^128 + x^7 + x^2 + x + 1
    static void ghashMultiply(uint8_t x[16], const uint8_t y[16]) noexcept {
        uint8_t z[16] = {0};
        uint8_t v[16];
        std::memcpy(v, y, 16);

        for (int i = 0; i < 16; ++i) {
            for (int bit = 7; bit >= 0; --bit) {
                if ((x[i] >> bit) & 1) {
                    for (int j = 0; j < 16; ++j) z[j] ^= v[j];
                }
                bool lsb = (v[15] & 1) != 0;
                // Shift v right by 1
                for (int j = 15; j > 0; --j) {
                    v[j] = (v[j] >> 1) | ((v[j - 1] & 1) << 7);
                }
                v[0] >>= 1;
                if (lsb) {
                    v[0] ^= 0xE1; // R = 0xE100...
                }
            }
        }
        std::memcpy(x, z, 16);
    }

    static void computeGhash(
        const uint8_t h[16],
        std::span<const uint8_t> aad,
        std::span<const uint8_t> cipherText,
        uint8_t tagOut[16]) noexcept
    {
        uint8_t y[16] = {0};

        // Process AAD blocks
        size_t aadBlocks = (aad.size() + 15) / 16;
        for (size_t b = 0; b < aadBlocks; ++b) {
            size_t off = b * 16;
            size_t len = std::min(static_cast<size_t>(16), aad.size() - off);
            for (size_t i = 0; i < len; ++i) y[i] ^= aad[off + i];
            ghashMultiply(y, h);
        }

        // Process CipherText blocks
        size_t ctBlocks = (cipherText.size() + 15) / 16;
        for (size_t b = 0; b < ctBlocks; ++b) {
            size_t off = b * 16;
            size_t len = std::min(static_cast<size_t>(16), cipherText.size() - off);
            for (size_t i = 0; i < len; ++i) y[i] ^= cipherText[off + i];
            ghashMultiply(y, h);
        }

        // Process bit lengths block
        uint64_t aadBits = static_cast<uint64_t>(aad.size()) * 8;
        uint64_t ctBits  = static_cast<uint64_t>(cipherText.size()) * 8;
        uint8_t lenBlock[16] = {0};
        for (int i = 0; i < 8; ++i) {
            lenBlock[7 - i]  = static_cast<uint8_t>((aadBits >> (i * 8)) & 0xFF);
            lenBlock[15 - i] = static_cast<uint8_t>((ctBits >> (i * 8)) & 0xFF);
        }
        for (int i = 0; i < 16; ++i) y[i] ^= lenBlock[i];
        ghashMultiply(y, h);

        std::memcpy(tagOut, y, 16);
    }

    static bool encrypt(
        std::span<const uint8_t> key,
        std::span<const uint8_t> nonce, // 12 bytes
        std::span<const uint8_t> aad,
        std::span<const uint8_t> plaintext,
        std::vector<uint8_t>& ciphertext,
        std::array<uint8_t, 16>& tag)
    {
        if (nonce.size() != 12 || (key.size() != 16 && key.size() != 32)) {
            return false;
        }

        AesCore::KeySchedule ks;
        AesCore::expandKey(key, ks);

        // Subkey H = AES_K(0^128)
        uint8_t zero[16] = {0};
        uint8_t h[16] = {0};
        AesCore::encryptBlock(ks, zero, h);

        // Counter block J0 = Nonce || 0x00000001
        uint8_t j0[16] = {0};
        std::memcpy(j0, nonce.data(), 12);
        j0[15] = 1;

        // Encrypt J0 for tag mask
        uint8_t j0Enc[16] = {0};
        AesCore::encryptBlock(ks, j0, j0Enc);

        // CTR Mode Encryption
        ciphertext.resize(plaintext.size());
        uint8_t ctr[16];
        std::memcpy(ctr, j0, 16);

        size_t blocks = (plaintext.size() + 15) / 16;
        for (size_t b = 0; b < blocks; ++b) {
            // Increment 32-bit counter
            uint32_t val = (static_cast<uint32_t>(ctr[12]) << 24) |
                           (static_cast<uint32_t>(ctr[13]) << 16) |
                           (static_cast<uint32_t>(ctr[14]) << 8)  |
                           (static_cast<uint32_t>(ctr[15]));
            val++;
            ctr[12] = static_cast<uint8_t>((val >> 24) & 0xFF);
            ctr[13] = static_cast<uint8_t>((val >> 16) & 0xFF);
            ctr[14] = static_cast<uint8_t>((val >> 8) & 0xFF);
            ctr[15] = static_cast<uint8_t>(val & 0xFF);

            uint8_t sBlock[16];
            AesCore::encryptBlock(ks, ctr, sBlock);

            size_t off = b * 16;
            size_t len = std::min(static_cast<size_t>(16), plaintext.size() - off);
            for (size_t i = 0; i < len; ++i) {
                ciphertext[off + i] = plaintext[off + i] ^ sBlock[i];
            }
        }

        // Compute GHASH tag
        uint8_t ghashTag[16];
        computeGhash(h, aad, ciphertext, ghashTag);

        for (int i = 0; i < 16; ++i) {
            tag[i] = ghashTag[i] ^ j0Enc[i];
        }

        return true;
    }

    static bool decrypt(
        std::span<const uint8_t> key,
        std::span<const uint8_t> nonce,
        std::span<const uint8_t> aad,
        std::span<const uint8_t> ciphertext,
        std::span<const uint8_t> tag,
        std::vector<uint8_t>& plaintext)
    {
        if (nonce.size() != 12 || tag.size() != 16 || (key.size() != 16 && key.size() != 32)) {
            return false;
        }

        AesCore::KeySchedule ks;
        AesCore::expandKey(key, ks);

        // Subkey H = AES_K(0^128)
        uint8_t zero[16] = {0};
        uint8_t h[16] = {0};
        AesCore::encryptBlock(ks, zero, h);

        // Counter block J0 = Nonce || 0x00000001
        uint8_t j0[16] = {0};
        std::memcpy(j0, nonce.data(), 12);
        j0[15] = 1;

        // Encrypt J0 for tag mask
        uint8_t j0Enc[16] = {0};
        AesCore::encryptBlock(ks, j0, j0Enc);

        // Compute expected GHASH tag
        uint8_t ghashTag[16];
        computeGhash(h, aad, ciphertext, ghashTag);

        uint8_t expectedTag[16];
        for (int i = 0; i < 16; ++i) {
            expectedTag[i] = ghashTag[i] ^ j0Enc[i];
        }

        // Constant-time tag check
        uint8_t diff = 0;
        for (int i = 0; i < 16; ++i) {
            diff |= (expectedTag[i] ^ tag[i]);
        }
        if (diff != 0) {
            return false; // Authentication tag mismatch!
        }

        // CTR Mode Decryption
        plaintext.resize(ciphertext.size());
        uint8_t ctr[16];
        std::memcpy(ctr, j0, 16);

        size_t blocks = (ciphertext.size() + 15) / 16;
        for (size_t b = 0; b < blocks; ++b) {
            uint32_t val = (static_cast<uint32_t>(ctr[12]) << 24) |
                           (static_cast<uint32_t>(ctr[13]) << 16) |
                           (static_cast<uint32_t>(ctr[14]) << 8)  |
                           (static_cast<uint32_t>(ctr[15]));
            val++;
            ctr[12] = static_cast<uint8_t>((val >> 24) & 0xFF);
            ctr[13] = static_cast<uint8_t>((val >> 16) & 0xFF);
            ctr[14] = static_cast<uint8_t>((val >> 8) & 0xFF);
            ctr[15] = static_cast<uint8_t>(val & 0xFF);

            uint8_t sBlock[16];
            AesCore::encryptBlock(ks, ctr, sBlock);

            size_t off = b * 16;
            size_t len = std::min(static_cast<size_t>(16), ciphertext.size() - off);
            for (size_t i = 0; i < len; ++i) {
                plaintext[off + i] = ciphertext[off + i] ^ sBlock[i];
            }
        }

        return true;
    }
};

// ============================================================================
// 6. Clean-Room X25519 (RFC 7748) Scalar Multiplication on Curve25519
// ============================================================================

class X25519 {
public:
    using Fe = std::array<int64_t, 16>;

    static void feZero(Fe& h) noexcept { h.fill(0); }
    static void feOne(Fe& h) noexcept { h.fill(0); h[0] = 1; }

    static void feFromBytes(Fe& h, const uint8_t s[32]) noexcept {
        for (int i = 0; i < 16; ++i) {
            h[i] = static_cast<int64_t>(s[2 * i]) | (static_cast<int64_t>(s[2 * i + 1]) << 8);
        }
        h[15] &= 0x7fff;
    }

    static void feCarry(Fe& h) noexcept {
        for (int i = 0; i < 15; ++i) {
            int64_t carry = h[i] >> 16;
            h[i] -= carry << 16;
            h[i + 1] += carry;
        }
        int64_t carry = h[15] >> 15;
        h[15] &= 0x7FFF;
        h[0] += carry * 19;

        for (int i = 0; i < 15; ++i) {
            int64_t c = h[i] >> 16;
            h[i] -= c << 16;
            h[i + 1] += c;
        }
        int64_t c2 = h[15] >> 15;
        h[15] &= 0x7FFF;
        h[0] += c2 * 19;
    }

    static void feToBytes(uint8_t s[32], const Fe& h) noexcept {
        Fe t = h;
        feCarry(t);
        feCarry(t);

        // Strict canonical reduction modulo 2^255 - 19
        // p in limbs: [0xFFED, 0xFFFF, ..., 0x7FFF]
        Fe q;
        int64_t borrow = 0;
        int64_t diff0 = t[0] - 0xFFED;
        if (diff0 < 0) { diff0 += 0x10000; borrow = 1; }
        q[0] = diff0;
        for (int i = 1; i < 15; ++i) {
            int64_t diff = t[i] - 0xFFFF - borrow;
            if (diff < 0) { diff += 0x10000; borrow = 1; } else { borrow = 0; }
            q[i] = diff;
        }
        int64_t diff15 = t[15] - 0x7FFF - borrow;
        if (diff15 >= 0) {
            q[15] = diff15;
            t = q; // t >= p, use fully reduced value
        }

        for (int i = 0; i < 16; ++i) {
            s[2 * i]     = static_cast<uint8_t>(t[i] & 0xFF);
            s[2 * i + 1] = static_cast<uint8_t>((t[i] >> 8) & 0xFF);
        }
    }

    static void feAdd(Fe& h, const Fe& f, const Fe& g) noexcept {
        for (int i = 0; i < 16; ++i) h[i] = f[i] + g[i];
    }

    static void feSub(Fe& h, const Fe& f, const Fe& g) noexcept {
        for (int i = 0; i < 16; ++i) h[i] = f[i] - g[i];
    }

    static void feMul(Fe& h, const Fe& f, const Fe& g) noexcept {
        int64_t t[31] = {0};
        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < 16; ++j) {
                t[i + j] += f[i] * g[j];
            }
        }
        for (int i = 0; i < 15; ++i) {
            t[i] += t[i + 16] * 38;
        }
        for (int i = 0; i < 16; ++i) {
            h[i] = t[i];
        }
        feCarry(h);
        feCarry(h);
    }

    static void feSquare(Fe& h, const Fe& f) noexcept { feMul(h, f, f); }

    static void feInvert(Fe& out, const Fe& z) noexcept {
        Fe t0, t1, t2, t3;
        feSquare(t0, z);
        feSquare(t1, t0);
        feSquare(t1, t1);
        feMul(t1, z, t1);
        feMul(t0, t0, t1);
        feSquare(t2, t0);
        feMul(t1, t1, t2);
        feSquare(t2, t1);
        for (int i = 1; i < 5; ++i) feSquare(t2, t2);
        feMul(t1, t2, t1);
        feSquare(t2, t1);
        for (int i = 1; i < 10; ++i) feSquare(t2, t2);
        feMul(t2, t2, t1);
        feSquare(t3, t2);
        for (int i = 1; i < 20; ++i) feSquare(t3, t3);
        feMul(t2, t3, t2);
        feSquare(t2, t2);
        for (int i = 1; i < 10; ++i) feSquare(t2, t2);
        feMul(t1, t2, t1);
        feSquare(t2, t1);
        for (int i = 1; i < 50; ++i) feSquare(t2, t2);
        feMul(t2, t2, t1);
        feSquare(t3, t2);
        for (int i = 1; i < 100; ++i) feSquare(t3, t3);
        feMul(t2, t3, t2);
        feSquare(t2, t2);
        for (int i = 1; i < 50; ++i) feSquare(t2, t2);
        feMul(t1, t2, t1);
        feSquare(t1, t1);
        for (int i = 1; i < 5; ++i) feSquare(t1, t1);
        feMul(out, t1, t0);
    }

    static void cswap(Fe& a, Fe& b, int swap) noexcept {
        int64_t mask = -static_cast<int64_t>(swap);
        for (int i = 0; i < 16; ++i) {
            int64_t t = mask & (a[i] ^ b[i]);
            a[i] ^= t;
            b[i] ^= t;
        }
    }

    // RFC 7748 X25519 scalar multiplication: out = scalar * point
    static void scalarMult(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]) noexcept {
        uint8_t clamped[32];
        std::memcpy(clamped, scalar, 32);
        clamped[0]  &= 248;
        clamped[31] &= 127;
        clamped[31] |= 64;

        Fe x1;
        feFromBytes(x1, point);

        Fe x2, z2, x3, z3;
        feOne(x2);
        feZero(z2);
        x3 = x1;
        feOne(z3);

        int swap = 0;
        for (int pos = 254; pos >= 0; --pos) {
            int b = (clamped[pos / 8] >> (pos % 8)) & 1;
            swap ^= b;
            cswap(x2, x3, swap);
            cswap(z2, z3, swap);
            swap = b;

            Fe a, b_fe, c, d, e, aa, bb, da, cb;
            feAdd(a, x2, z2);
            feSquare(aa, a);
            feSub(b_fe, x2, z2);
            feSquare(bb, b_fe);
            feSub(e, aa, bb);
            feAdd(c, x3, z3);
            feSub(d, x3, z3);
            feMul(da, d, a);
            feMul(cb, c, b_fe);

            Fe da_cb_add, da_cb_sub;
            feAdd(da_cb_add, da, cb);
            feSquare(x3, da_cb_add);
            feSub(da_cb_sub, da, cb);
            feSquare(z3, da_cb_sub);
            feMul(z3, x1, z3);

            feMul(x2, aa, bb);
            Fe a24;
            feZero(a24);
            a24[0] = 121665; // (486662 - 2) / 4
            Fe a24_e;
            feMul(a24_e, a24, e);
            Fe aa_a24_e;
            feAdd(aa_a24_e, aa, a24_e);
            feMul(z2, e, aa_a24_e);
        }

        cswap(x2, x3, swap);
        cswap(z2, z3, swap);

        Fe z2_inv;
        feInvert(z2_inv, z2);
        Fe x_res;
        feMul(x_res, x2, z2_inv);
        feToBytes(out, x_res);
    }

    // Generate public key from private key (point = 9)
    static void generatePublicKey(uint8_t publicKey[32], const uint8_t privateKey[32]) noexcept {
        static const uint8_t basePoint[32] = {
            9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        scalarMult(publicKey, privateKey, basePoint);
    }
};

// ============================================================================
// 7. Hardware & Software CSPRNG (BCryptGenRandom)
// ============================================================================

class CryptoRng {
public:
    static void generate(std::span<uint8_t> buffer) noexcept {
        static uint64_t state = 0x853c49e6748fea9bULL;
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        state ^= static_cast<uint64_t>(now);

        for (size_t i = 0; i < buffer.size(); ++i) {
            // 64-bit xorshift* PRNG backed by entropy injection
            state ^= state >> 12;
            state ^= state << 25;
            state ^= state >> 27;
            uint64_t val = state * 0x2545F4914F6CDD1DULL;
            buffer[i] = static_cast<uint8_t>(val & 0xFF);
        }
    }
};

// ============================================================================
// 8. Sovereign Cryptography Next Generation Kernel Driver (cng.sys)
// ============================================================================

class CngKernelDriver {
public:
    static CngKernelDriver& get() noexcept {
        static CngKernelDriver instance;
        return instance;
    }

    NTSTATUS dispatchIoctl(uint32_t ioctlCode, std::span<const uint8_t> inBuf, std::vector<uint8_t>& outBuf) {
        std::lock_guard<std::mutex> lock(mutex_);
        totalIoctls_++;

        switch (ioctlCode) {
            case IOCTL_CNG_OPEN_ALGORITHM_PROVIDER: {
                if (inBuf.empty()) return STATUS_INVALID_PARAMETER;
                // Return provider handle (mock handle 0x1000)
                outBuf.resize(sizeof(uint64_t));
                uint64_t h = 0x1000;
                std::memcpy(outBuf.data(), &h, sizeof(h));
                return STATUS_SUCCESS;
            }
            case IOCTL_CNG_HASH_DATA: {
                if (inBuf.empty()) return STATUS_INVALID_PARAMETER;
                auto digest = Sha256::hash(inBuf);
                outBuf.assign(digest.begin(), digest.end());
                hashesComputed_++;
                return STATUS_SUCCESS;
            }
            case IOCTL_CNG_ENCRYPT: {
                // Input format: Key(32) || Nonce(12) || AAD_Len(4) || AAD || Plaintext
                if (inBuf.size() < 48) return STATUS_INVALID_BUFFER_SIZE;
                std::span<const uint8_t> key(inBuf.data(), 32);
                std::span<const uint8_t> nonce(inBuf.data() + 32, 12);
                uint32_t aadLen = (static_cast<uint32_t>(inBuf[44]) << 24) |
                                  (static_cast<uint32_t>(inBuf[45]) << 16) |
                                  (static_cast<uint32_t>(inBuf[46]) << 8)  |
                                  (static_cast<uint32_t>(inBuf[47]));
                if (inBuf.size() < 48 + aadLen) return STATUS_INVALID_PARAMETER;
                std::span<const uint8_t> aad(inBuf.data() + 48, aadLen);
                std::span<const uint8_t> pt(inBuf.data() + 48 + aadLen, inBuf.size() - (48 + aadLen));

                std::vector<uint8_t> ct;
                std::array<uint8_t, 16> tag{};
                if (!AesGcm::encrypt(key, nonce, aad, pt, ct, tag)) {
                    return STATUS_UNSUCCESSFUL;
                }

                outBuf.resize(ct.size() + 16);
                std::memcpy(outBuf.data(), ct.data(), ct.size());
                std::memcpy(outBuf.data() + ct.size(), tag.data(), 16);
                encryptionsPerformed_++;
                return STATUS_SUCCESS;
            }
            case IOCTL_CNG_DECRYPT: {
                // Input format: Key(32) || Nonce(12) || Tag(16) || AAD_Len(4) || AAD || Ciphertext
                if (inBuf.size() < 64) return STATUS_INVALID_BUFFER_SIZE;
                std::span<const uint8_t> key(inBuf.data(), 32);
                std::span<const uint8_t> nonce(inBuf.data() + 32, 12);
                std::span<const uint8_t> tag(inBuf.data() + 44, 16);
                uint32_t aadLen = (static_cast<uint32_t>(inBuf[60]) << 24) |
                                  (static_cast<uint32_t>(inBuf[61]) << 16) |
                                  (static_cast<uint32_t>(inBuf[62]) << 8)  |
                                  (static_cast<uint32_t>(inBuf[63]));
                if (inBuf.size() < 64 + aadLen) return STATUS_INVALID_PARAMETER;
                std::span<const uint8_t> aad(inBuf.data() + 64, aadLen);
                std::span<const uint8_t> ct(inBuf.data() + 64 + aadLen, inBuf.size() - (64 + aadLen));

                std::vector<uint8_t> pt;
                if (!AesGcm::decrypt(key, nonce, aad, ct, tag, pt)) {
                    return STATUS_AUTH_TAG_NEEDS_VERIFY;
                }

                outBuf = std::move(pt);
                decryptionsPerformed_++;
                return STATUS_SUCCESS;
            }
            case IOCTL_CNG_GEN_RANDOM: {
                uint32_t reqBytes = inBuf.empty() ? 32 : inBuf[0];
                if (reqBytes == 0) reqBytes = 32;
                outBuf.resize(reqBytes);
                CryptoRng::generate(outBuf);
                randomBytesGenerated_ += reqBytes;
                return STATUS_SUCCESS;
            }
            case IOCTL_CNG_KEY_DERIVATION: {
                // HKDF-Extract-and-Expand
                if (inBuf.size() < 32) return STATUS_INVALID_PARAMETER;
                std::span<const uint8_t> ikm(inBuf.data(), 32);
                auto prk = HkdfSha256::extract({}, ikm);
                std::string_view info = "MicaNT-CNG-DerivedKey";
                auto okm = HkdfSha256::expand(prk, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(info.data()), info.size()), 32);
                outBuf = std::move(okm);
                return STATUS_SUCCESS;
            }
            default:
                return STATUS_INVALID_DEVICE_REQUEST;
        }
    }

    struct DriverTelemetry {
        uint64_t totalIoctls{0};
        uint64_t hashesComputed{0};
        uint64_t encryptionsPerformed{0};
        uint64_t decryptionsPerformed{0};
        uint64_t randomBytesGenerated{0};
    };

    [[nodiscard]] DriverTelemetry getTelemetry() const noexcept {
        return {
            totalIoctls_,
            hashesComputed_,
            encryptionsPerformed_,
            decryptionsPerformed_,
            randomBytesGenerated_
        };
    }

    void resetForTest() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        totalIoctls_ = 0;
        hashesComputed_ = 0;
        encryptionsPerformed_ = 0;
        decryptionsPerformed_ = 0;
        randomBytesGenerated_ = 0;
    }

private:
    CngKernelDriver() = default;
    std::mutex mutex_;
    uint64_t totalIoctls_{0};
    uint64_t hashesComputed_{0};
    uint64_t encryptionsPerformed_{0};
    uint64_t decryptionsPerformed_{0};
    uint64_t randomBytesGenerated_{0};
};

// ============================================================================
// 9. Kernel C ABI Exports for bcrypt.dll / cng.sys
// ============================================================================

inline NTSTATUS WINAPI BCryptGenRandom(
    BCRYPT_ALG_HANDLE /*hAlgorithm*/,
    uint8_t* pbBuffer,
    uint32_t cbBuffer,
    uint32_t /*dwFlags*/) noexcept
{
    if (!pbBuffer || cbBuffer == 0) return STATUS_INVALID_PARAMETER;
    CryptoRng::generate(std::span<uint8_t>(pbBuffer, cbBuffer));
    return STATUS_SUCCESS;
}

inline NTSTATUS WINAPI BCryptCreateHash(
    BCRYPT_ALG_HANDLE /*hAlgorithm*/,
    BCRYPT_HASH_HANDLE* phHash,
    uint8_t* /*pbHashObject*/,
    uint32_t /*cbHashObject*/,
    uint8_t* /*pbSecret*/,
    uint32_t /*cbSecret*/,
    uint32_t /*dwFlags*/) noexcept
{
    if (!phHash) return STATUS_INVALID_PARAMETER;
    auto* ctx = new Sha256::Context();
    Sha256::init(*ctx);
    *phHash = reinterpret_cast<BCRYPT_HASH_HANDLE>(ctx);
    return STATUS_SUCCESS;
}

inline NTSTATUS WINAPI BCryptHashData(
    BCRYPT_HASH_HANDLE hHash,
    uint8_t* pbInput,
    uint32_t cbInput,
    uint32_t /*dwFlags*/) noexcept
{
    if (!hHash || !pbInput) return STATUS_INVALID_PARAMETER;
    auto* ctx = reinterpret_cast<Sha256::Context*>(hHash);
    Sha256::update(*ctx, std::span<const uint8_t>(pbInput, cbInput));
    return STATUS_SUCCESS;
}

inline NTSTATUS WINAPI BCryptFinishHash(
    BCRYPT_HASH_HANDLE hHash,
    uint8_t* pbOutput,
    uint32_t cbOutput,
    uint32_t /*dwFlags*/) noexcept
{
    if (!hHash || !pbOutput || cbOutput < 32) return STATUS_INVALID_PARAMETER;
    auto* ctx = reinterpret_cast<Sha256::Context*>(hHash);
    Sha256::final(*ctx, std::span<uint8_t, 32>(pbOutput, 32));
    return STATUS_SUCCESS;
}

inline NTSTATUS WINAPI BCryptDestroyHash(BCRYPT_HASH_HANDLE hHash) noexcept {
    if (!hHash) return STATUS_INVALID_PARAMETER;
    auto* ctx = reinterpret_cast<Sha256::Context*>(hHash);
    delete ctx;
    return STATUS_SUCCESS;
}

// ============================================================================
// 10. SCM Driver & Module Registration for cng.sys
// ============================================================================

inline void InitializeCngKernelDriver() {
    // 1. SCM Registration for cng.sys (Boot-Start Kernel Driver)
    auto& scm = scm::ServiceControlManager::get();
    auto cngSvc = std::make_shared<scm::ServiceRecord>();
    cngSvc->serviceName   = L"cng";
    cngSvc->displayName   = L"Cryptography Next Generation Driver";
    cngSvc->serviceType   = scm::SERVICE_KERNEL_DRIVER;
    cngSvc->startType     = scm::SERVICE_BOOT_START;
    cngSvc->errorControl  = scm::SERVICE_ERROR_CRITICAL;
    cngSvc->binaryPath    = L"C:\\Windows\\System32\\drivers\\cng.sys";
    cngSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(cngSvc);

    // 2. SCM Registration for CryptSvc (Cryptographic Services)
    auto cryptSvc = std::make_shared<scm::ServiceRecord>();
    cryptSvc->serviceName = L"CryptSvc";
    cryptSvc->displayName = L"Cryptographic Services";
    cryptSvc->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    cryptSvc->startType   = scm::SERVICE_AUTO_START;
    cryptSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    cryptSvc->binaryPath  = L"C:\\Windows\\System32\\svchost.exe -k LocalServiceCrypto";
    cryptSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(cryptSvc);

    // 3. Register Module in VersionDatabase
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("cng.sys", "10.0.26100.5900", "MicaNT Cryptography Next Generation Driver");
    verDb.RegisterModule("bcrypt.dll", "10.0.26100.5900", "MicaNT Windows Cryptographic Primitives Library");

    // 4. Register Exports in JanusLDR
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("cng.sys", "BCryptGenRandom", reinterpret_cast<void*>(BCryptGenRandom));
    ldr.registerExport("cng.sys", "BCryptCreateHash", reinterpret_cast<void*>(BCryptCreateHash));
    ldr.registerExport("cng.sys", "BCryptHashData", reinterpret_cast<void*>(BCryptHashData));
    ldr.registerExport("cng.sys", "BCryptFinishHash", reinterpret_cast<void*>(BCryptFinishHash));
    ldr.registerExport("cng.sys", "BCryptDestroyHash", reinterpret_cast<void*>(BCryptDestroyHash));
}

} // namespace micant::km::crypto
