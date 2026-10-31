#ifndef MICANT_NPS_HPP
#define MICANT_NPS_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <atomic>
#include <cstring>
#include <deque>
#include <optional>
#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/servers/ad_ds.hpp"

namespace micant::nps {

// ============================================================================
// Clean-Room Cryptographic Synthesizer (RFC 1321 MD5, RFC 3174 SHA-1, RFC 1320 MD4, DES)
// ============================================================================
namespace crypto {

inline uint32_t rotl(uint32_t x, uint32_t s) {
    return (x << s) | (x >> (32 - s));
}

// ----------------------------------------------------------------------------
// RFC 1321 MD5 & HMAC-MD5
// ----------------------------------------------------------------------------
inline void Md5Transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t x[16];

    for (size_t i = 0; i < 16; ++i) {
        x[i] = static_cast<uint32_t>(block[i * 4 + 0]) |
              (static_cast<uint32_t>(block[i * 4 + 1]) << 8) |
              (static_cast<uint32_t>(block[i * 4 + 2]) << 16) |
              (static_cast<uint32_t>(block[i * 4 + 3]) << 24);
    }

    #define NPS_MD5_F(x, y, z) (((x) & (y)) | ((~x) & (z)))
    #define NPS_MD5_G(x, y, z) (((x) & (z)) | ((y) & (~z)))
    #define NPS_MD5_H(x, y, z) ((x) ^ (y) ^ (z))
    #define NPS_MD5_I(x, y, z) ((y) ^ ((x) | (~z)))

    #define NPS_MD5_STEP(f, a, b, c, d, x, s, ac) { \
        (a) += f((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = rotl((a), (s)); \
        (a) += (b); \
    }

    // Round 1
    NPS_MD5_STEP(NPS_MD5_F, a, b, c, d, x[ 0],  7, 0xd76aa478);
    NPS_MD5_STEP(NPS_MD5_F, d, a, b, c, x[ 1], 12, 0xe8c7b756);
    NPS_MD5_STEP(NPS_MD5_F, c, d, a, b, x[ 2], 17, 0x242070db);
    NPS_MD5_STEP(NPS_MD5_F, b, c, d, a, x[ 3], 22, 0xc1bdceee);
    NPS_MD5_STEP(NPS_MD5_F, a, b, c, d, x[ 4],  7, 0xf57c0faf);
    NPS_MD5_STEP(NPS_MD5_F, d, a, b, c, x[ 5], 12, 0x4787c62a);
    NPS_MD5_STEP(NPS_MD5_F, c, d, a, b, x[ 6], 17, 0xa8304613);
    NPS_MD5_STEP(NPS_MD5_F, b, c, d, a, x[ 7], 22, 0xfd469501);
    NPS_MD5_STEP(NPS_MD5_F, a, b, c, d, x[ 8],  7, 0x698098d8);
    NPS_MD5_STEP(NPS_MD5_F, d, a, b, c, x[ 9], 12, 0x8b44f7af);
    NPS_MD5_STEP(NPS_MD5_F, c, d, a, b, x[10], 17, 0xffff5bb1);
    NPS_MD5_STEP(NPS_MD5_F, b, c, d, a, x[11], 22, 0x895cd7be);
    NPS_MD5_STEP(NPS_MD5_F, a, b, c, d, x[12],  7, 0x6b901122);
    NPS_MD5_STEP(NPS_MD5_F, d, a, b, c, x[13], 12, 0xfd987193);
    NPS_MD5_STEP(NPS_MD5_F, c, d, a, b, x[14], 17, 0xa679438e);
    NPS_MD5_STEP(NPS_MD5_F, b, c, d, a, x[15], 22, 0x49b40821);

    // Round 2
    NPS_MD5_STEP(NPS_MD5_G, a, b, c, d, x[ 1],  5, 0xf61e2562);
    NPS_MD5_STEP(NPS_MD5_G, d, a, b, c, x[ 6],  9, 0xc040b340);
    NPS_MD5_STEP(NPS_MD5_G, c, d, a, b, x[11], 14, 0x265e5a51);
    NPS_MD5_STEP(NPS_MD5_G, b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
    NPS_MD5_STEP(NPS_MD5_G, a, b, c, d, x[ 5],  5, 0xd62f105d);
    NPS_MD5_STEP(NPS_MD5_G, d, a, b, c, x[10],  9, 0x02441453);
    NPS_MD5_STEP(NPS_MD5_G, c, d, a, b, x[15], 14, 0xd8a1e681);
    NPS_MD5_STEP(NPS_MD5_G, b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
    NPS_MD5_STEP(NPS_MD5_G, a, b, c, d, x[ 9],  5, 0x21e1cde6);
    NPS_MD5_STEP(NPS_MD5_G, d, a, b, c, x[14],  9, 0xc33707d6);
    NPS_MD5_STEP(NPS_MD5_G, c, d, a, b, x[ 3], 14, 0xf4d50d87);
    NPS_MD5_STEP(NPS_MD5_G, b, c, d, a, x[ 8], 20, 0x455a14ed);
    NPS_MD5_STEP(NPS_MD5_G, a, b, c, d, x[13],  5, 0xa9e3e905);
    NPS_MD5_STEP(NPS_MD5_G, d, a, b, c, x[ 2],  9, 0xfcefa3f8);
    NPS_MD5_STEP(NPS_MD5_G, c, d, a, b, x[ 7], 14, 0x676f02d9);
    NPS_MD5_STEP(NPS_MD5_G, b, c, d, a, x[12], 20, 0x8d2a4c8a);

    // Round 3
    NPS_MD5_STEP(NPS_MD5_H, a, b, c, d, x[ 5],  4, 0xfffa3942);
    NPS_MD5_STEP(NPS_MD5_H, d, a, b, c, x[ 8], 11, 0x8771f681);
    NPS_MD5_STEP(NPS_MD5_H, c, d, a, b, x[11], 16, 0x6d9d6122);
    NPS_MD5_STEP(NPS_MD5_H, b, c, d, a, x[14], 23, 0xfde5380c);
    NPS_MD5_STEP(NPS_MD5_H, a, b, c, d, x[ 1],  4, 0xa4beea44);
    NPS_MD5_STEP(NPS_MD5_H, d, a, b, c, x[ 4], 11, 0x4bdecfa9);
    NPS_MD5_STEP(NPS_MD5_H, c, d, a, b, x[ 7], 16, 0xf6bb4b60);
    NPS_MD5_STEP(NPS_MD5_H, b, c, d, a, x[10], 23, 0xbebfbc70);
    NPS_MD5_STEP(NPS_MD5_H, a, b, c, d, x[13],  4, 0x289b7ec6);
    NPS_MD5_STEP(NPS_MD5_H, d, a, b, c, x[ 0], 11, 0xeaa127fa);
    NPS_MD5_STEP(NPS_MD5_H, c, d, a, b, x[ 3], 16, 0xd4ef3085);
    NPS_MD5_STEP(NPS_MD5_H, b, c, d, a, x[ 6], 23, 0x04881d05);
    NPS_MD5_STEP(NPS_MD5_H, a, b, c, d, x[ 9],  4, 0xd9d4d039);
    NPS_MD5_STEP(NPS_MD5_H, d, a, b, c, x[12], 11, 0xe6db99e5);
    NPS_MD5_STEP(NPS_MD5_H, c, d, a, b, x[15], 16, 0x1fa27cf8);
    NPS_MD5_STEP(NPS_MD5_H, b, c, d, a, x[ 2], 23, 0xc4ac5665);

    // Round 4
    NPS_MD5_STEP(NPS_MD5_I, a, b, c, d, x[ 0],  6, 0xf4292244);
    NPS_MD5_STEP(NPS_MD5_I, d, a, b, c, x[ 7], 10, 0x432aff97);
    NPS_MD5_STEP(NPS_MD5_I, c, d, a, b, x[14], 15, 0xab9423a7);
    NPS_MD5_STEP(NPS_MD5_I, b, c, d, a, x[ 5], 21, 0xfc93a039);
    NPS_MD5_STEP(NPS_MD5_I, a, b, c, d, x[12],  6, 0x655b59c3);
    NPS_MD5_STEP(NPS_MD5_I, d, a, b, c, x[ 3], 10, 0x8f0ccc92);
    NPS_MD5_STEP(NPS_MD5_I, c, d, a, b, x[10], 15, 0xffeff47d);
    NPS_MD5_STEP(NPS_MD5_I, b, c, d, a, x[ 1], 21, 0x85845dd1);
    NPS_MD5_STEP(NPS_MD5_I, a, b, c, d, x[ 8],  6, 0x6fa87e4f);
    NPS_MD5_STEP(NPS_MD5_I, d, a, b, c, x[15], 10, 0xfe2ce6e0);
    NPS_MD5_STEP(NPS_MD5_I, c, d, a, b, x[ 6], 15, 0xa3014314);
    NPS_MD5_STEP(NPS_MD5_I, b, c, d, a, x[13], 21, 0x4e0811a1);
    NPS_MD5_STEP(NPS_MD5_I, a, b, c, d, x[ 4],  6, 0xf7537e82);
    NPS_MD5_STEP(NPS_MD5_I, d, a, b, c, x[11], 10, 0xbd3af235);
    NPS_MD5_STEP(NPS_MD5_I, c, d, a, b, x[ 2], 15, 0x2ad7d2bb);
    NPS_MD5_STEP(NPS_MD5_I, b, c, d, a, x[ 9], 21, 0xeb86d391);

    #undef NPS_MD5_STEP
    #undef NPS_MD5_F
    #undef NPS_MD5_G
    #undef NPS_MD5_H
    #undef NPS_MD5_I

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

inline void Md5Hash(const uint8_t* data, size_t length, uint8_t out[16]) {
    uint32_t state[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};

    size_t fullBlocks = length / 64;
    for (size_t i = 0; i < fullBlocks; ++i) {
        Md5Transform(state, data + i * 64);
    }

    uint8_t tail[128]{};
    size_t rem = length % 64;
    if (data && rem > 0) {
        std::memcpy(tail, data + fullBlocks * 64, rem);
    }
    tail[rem] = 0x80;

    size_t padLen = (rem < 56) ? 64 : 128;
    uint64_t bitLen = static_cast<uint64_t>(length) * 8;
    for (int i = 0; i < 8; ++i) {
        tail[padLen - 8 + i] = static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF);
    }

    for (size_t i = 0; i < padLen; i += 64) {
        Md5Transform(state, tail + i);
    }

    for (int i = 0; i < 4; ++i) {
        out[i * 4 + 0] = static_cast<uint8_t>(state[i] & 0xFF);
        out[i * 4 + 1] = static_cast<uint8_t>((state[i] >> 8) & 0xFF);
        out[i * 4 + 2] = static_cast<uint8_t>((state[i] >> 16) & 0xFF);
        out[i * 4 + 3] = static_cast<uint8_t>((state[i] >> 24) & 0xFF);
    }
}

inline void HmacMd5(const uint8_t* key, size_t keyLen, const uint8_t* data, size_t dataLen, uint8_t out[16]) {
    uint8_t k[64]{};
    if (keyLen > 64) {
        Md5Hash(key, keyLen, k);
    } else if (key && keyLen > 0) {
        std::memcpy(k, key, keyLen);
    }

    uint8_t ipad[64];
    uint8_t opad[64];
    for (size_t i = 0; i < 64; ++i) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    std::vector<uint8_t> innerData;
    innerData.reserve(64 + dataLen);
    innerData.insert(innerData.end(), ipad, ipad + 64);
    if (data && dataLen > 0) {
        innerData.insert(innerData.end(), data, data + dataLen);
    }

    uint8_t innerHash[16];
    Md5Hash(innerData.data(), innerData.size(), innerHash);

    std::vector<uint8_t> outerData;
    outerData.reserve(64 + 16);
    outerData.insert(outerData.end(), opad, opad + 64);
    outerData.insert(outerData.end(), innerHash, innerHash + 16);

    Md5Hash(outerData.data(), outerData.size(), out);
}

// ----------------------------------------------------------------------------
// RFC 3174 SHA-1
// ----------------------------------------------------------------------------
inline void Sha1Hash(const uint8_t* data, size_t length, uint8_t out[20]) {
    uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    std::vector<uint8_t> msg;
    msg.reserve(length + 72);
    if (data && length > 0) {
        msg.assign(data, data + length);
    }
    uint64_t bitLen = static_cast<uint64_t>(length) * 8;
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));
    }
    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80]{};
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 3]));
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotl(b, 30);
            b = a;
            a = temp;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }
    for (int i = 0; i < 5; ++i) {
        out[i * 4 + 0] = static_cast<uint8_t>((h[i] >> 24) & 0xFF);
        out[i * 4 + 1] = static_cast<uint8_t>((h[i] >> 16) & 0xFF);
        out[i * 4 + 2] = static_cast<uint8_t>((h[i] >> 8) & 0xFF);
        out[i * 4 + 3] = static_cast<uint8_t>(h[i] & 0xFF);
    }
}

// ----------------------------------------------------------------------------
// RFC 1320 MD4 & NTLM Hash
// ----------------------------------------------------------------------------
inline void Md4Hash(const uint8_t* data, size_t length, uint8_t out[16]) {
    uint32_t A = 0x67452301;
    uint32_t B = 0xefcdab89;
    uint32_t C = 0x98badcfe;
    uint32_t D = 0x10325476;

    std::vector<uint8_t> msg;
    msg.reserve(length + 72);
    if (data && length > 0) msg.assign(data, data + length);

    uint64_t bitLen = static_cast<uint64_t>(length) * 8;
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    for (int i = 0; i < 8; ++i) {
        msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));
    }

    #define MD4_F(x, y, z) (((x) & (y)) | ((~x) & (z)))
    #define MD4_G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
    #define MD4_H(x, y, z) ((x) ^ (y) ^ (z))

    #define MD4_STEP_1(a, b, c, d, k, s) { \
        (a) += MD4_F((b), (c), (d)) + X[k]; \
        (a) = rotl((a), (s)); \
    }
    #define MD4_STEP_2(a, b, c, d, k, s) { \
        (a) += MD4_G((b), (c), (d)) + X[k] + 0x5a827999; \
        (a) = rotl((a), (s)); \
    }
    #define MD4_STEP_3(a, b, c, d, k, s) { \
        (a) += MD4_H((b), (c), (d)) + X[k] + 0x6ed9eba1; \
        (a) = rotl((a), (s)); \
    }

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t X[16];
        for (int i = 0; i < 16; ++i) {
            X[i] = static_cast<uint32_t>(msg[chunk + i * 4]) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 8) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 16) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 3]) << 24);
        }

        uint32_t a = A, b = B, c = C, d = D;

        // Round 1
        MD4_STEP_1(a, b, c, d,  0,  3); MD4_STEP_1(d, a, b, c,  1,  7);
        MD4_STEP_1(c, d, a, b,  2, 11); MD4_STEP_1(b, c, d, a,  3, 19);
        MD4_STEP_1(a, b, c, d,  4,  3); MD4_STEP_1(d, a, b, c,  5,  7);
        MD4_STEP_1(c, d, a, b,  6, 11); MD4_STEP_1(b, c, d, a,  7, 19);
        MD4_STEP_1(a, b, c, d,  8,  3); MD4_STEP_1(d, a, b, c,  9,  7);
        MD4_STEP_1(c, d, a, b, 10, 11); MD4_STEP_1(b, c, d, a, 11, 19);
        MD4_STEP_1(a, b, c, d, 12,  3); MD4_STEP_1(d, a, b, c, 13,  7);
        MD4_STEP_1(c, d, a, b, 14, 11); MD4_STEP_1(b, c, d, a, 15, 19);

        // Round 2
        MD4_STEP_2(a, b, c, d,  0,  3); MD4_STEP_2(d, a, b, c,  4,  5);
        MD4_STEP_2(c, d, a, b,  8,  9); MD4_STEP_2(b, c, d, a, 12, 13);
        MD4_STEP_2(a, b, c, d,  1,  3); MD4_STEP_2(d, a, b, c,  5,  5);
        MD4_STEP_2(c, d, a, b,  9,  9); MD4_STEP_2(b, c, d, a, 13, 13);
        MD4_STEP_2(a, b, c, d,  2,  3); MD4_STEP_2(d, a, b, c,  6,  5);
        MD4_STEP_2(c, d, a, b, 10,  9); MD4_STEP_2(b, c, d, a, 14, 13);
        MD4_STEP_2(a, b, c, d,  3,  3); MD4_STEP_2(d, a, b, c,  7,  5);
        MD4_STEP_2(c, d, a, b, 11,  9); MD4_STEP_2(b, c, d, a, 15, 13);

        // Round 3
        MD4_STEP_3(a, b, c, d,  0,  3); MD4_STEP_3(d, a, b, c,  8,  9);
        MD4_STEP_3(c, d, a, b,  4, 11); MD4_STEP_3(b, c, d, a, 12, 15);
        MD4_STEP_3(a, b, c, d,  2,  3); MD4_STEP_3(d, a, b, c, 10,  9);
        MD4_STEP_3(c, d, a, b,  6, 11); MD4_STEP_3(b, c, d, a, 14, 15);
        MD4_STEP_3(a, b, c, d,  1,  3); MD4_STEP_3(d, a, b, c,  9,  9);
        MD4_STEP_3(c, d, a, b,  5, 11); MD4_STEP_3(b, c, d, a, 13, 15);
        MD4_STEP_3(a, b, c, d,  3,  3); MD4_STEP_3(d, a, b, c, 11,  9);
        MD4_STEP_3(c, d, a, b,  7, 11); MD4_STEP_3(b, c, d, a, 15, 15);

        A += a; B += b; C += c; D += d;
    }

    #undef MD4_F
    #undef MD4_G
    #undef MD4_H
    #undef MD4_STEP_1
    #undef MD4_STEP_2
    #undef MD4_STEP_3

    for (int i = 0; i < 4; ++i) {
        out[i * 4 + 0] = static_cast<uint8_t>((A >> (i * 8)) & 0xFF);
        out[i * 4 + 1] = static_cast<uint8_t>((B >> (i * 8)) & 0xFF);
        out[i * 4 + 2] = static_cast<uint8_t>((C >> (i * 8)) & 0xFF);
        out[i * 4 + 3] = static_cast<uint8_t>((D >> (i * 8)) & 0xFF);
    }
}

inline void NtPasswordHash(const std::string& password, uint8_t outNtHash[16]) {
    std::vector<uint8_t> utf16;
    utf16.reserve(password.size() * 2);
    for (char c : password) {
        utf16.push_back(static_cast<uint8_t>(c));
        utf16.push_back(0);
    }
    Md4Hash(utf16.data(), utf16.size(), outNtHash);
}

// ----------------------------------------------------------------------------
// Clean-Room DES (Data Encryption Standard) Algorithm for MS-CHAPv2
// ----------------------------------------------------------------------------
static const uint8_t DES_IP[64] = {
    58, 50, 42, 34, 26, 18, 10, 2,  60, 52, 44, 36, 28, 20, 12, 4,
    62, 54, 46, 38, 30, 22, 14, 6,  64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17,  9, 1,  59, 51, 43, 35, 27, 19, 11, 3,
    61, 53, 45, 37, 29, 21, 13, 5,  63, 55, 47, 39, 31, 23, 15, 7
};

static const uint8_t DES_FP[64] = {
    40, 8, 48, 16, 56, 24, 64, 32,  39, 7, 47, 15, 55, 23, 63, 31,
    38, 6, 46, 14, 54, 22, 62, 30,  37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28,  35, 3, 43, 11, 51, 19, 59, 27,
    34, 2, 42, 10, 50, 18, 58, 26,  33, 1, 41,  9, 49, 17, 57, 25
};

static const uint8_t DES_E[48] = {
    32,  1,  2,  3,  4,  5,   4,  5,  6,  7,  8,  9,
     8,  9, 10, 11, 12, 13,  12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21,  20, 21, 22, 23, 24, 25,
    24, 25, 26, 27, 28, 29,  28, 29, 30, 31, 32,  1
};

static const uint8_t DES_P[32] = {
    16,  7, 20, 21,  29, 12, 28, 17,   1, 15, 23, 26,   5, 18, 31, 10,
     2,  8, 24, 14,  32, 27,  3,  9,  19, 13, 30,  6,  22, 11,  4, 25
};

static const uint8_t DES_SBOX[8][64] = {
    { 14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7,
       0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
       4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0,
      15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13 },
    { 15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10,
       3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
       0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15,
      13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9 },
    { 10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8,
      13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
      13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7,
       1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12 },
    {  7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15,
      13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
      10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4,
       3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14 },
    {  2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9,
      14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
       4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14,
      11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3 },
    { 12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11,
      10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
       9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6,
       4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13 },
    {  4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1,
      13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
       1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2,
       6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12 },
    { 13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7,
       1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
       7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8,
       2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11 }
};

static const uint8_t DES_PC1[56] = {
    57, 49, 41, 33, 25, 17,  9,   1, 58, 50, 42, 34, 26, 18,
    10,  2, 59, 51, 43, 35, 27,  19, 11,  3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15,   7, 62, 54, 46, 38, 30, 22,
    14,  6, 61, 53, 45, 37, 29,  21, 13,  5, 28, 20, 12,  4
};

static const uint8_t DES_PC2[48] = {
    14, 17, 11, 24,  1,  5,   3, 28, 15,  6, 21, 10,
    23, 19, 12,  4, 26,  8,  16,  7, 27, 20, 13,  2,
    41, 52, 31, 37, 47, 55,  30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53,  46, 42, 50, 36, 29, 32
};

static const uint8_t DES_SHIFTS[16] = { 1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1 };

inline uint64_t desPermute(uint64_t inVal, const uint8_t* table, int outBits, int inTotalBits) {
    uint64_t out = 0;
    for (int i = 0; i < outBits; ++i) {
        int bitPos = inTotalBits - table[i];
        if ((inVal >> bitPos) & 1) {
            out |= (1ULL << (outBits - 1 - i));
        }
    }
    return out;
}

inline void generateDesSubkeys(uint64_t key64, uint64_t subkeys[16]) {
    uint64_t cd = desPermute(key64, DES_PC1, 56, 64);
    uint32_t c = static_cast<uint32_t>((cd >> 28) & 0x0FFFFFFF);
    uint32_t d = static_cast<uint32_t>(cd & 0x0FFFFFFF);

    for (int round = 0; round < 16; ++round) {
        int shift = DES_SHIFTS[round];
        c = ((c << shift) | (c >> (28 - shift))) & 0x0FFFFFFF;
        d = ((d << shift) | (d >> (28 - shift))) & 0x0FFFFFFF;
        uint64_t combined = (static_cast<uint64_t>(c) << 28) | d;
        subkeys[round] = desPermute(combined, DES_PC2, 48, 56);
    }
}

inline uint64_t desEncryptBlock64(uint64_t block64, const uint64_t subkeys[16]) {
    uint64_t perm = desPermute(block64, DES_IP, 64, 64);
    uint32_t l = static_cast<uint32_t>((perm >> 32) & 0xFFFFFFFF);
    uint32_t r = static_cast<uint32_t>(perm & 0xFFFFFFFF);

    for (int round = 0; round < 16; ++round) {
        uint64_t er = desPermute(r, DES_E, 48, 32);
        uint64_t xorVal = er ^ subkeys[round];
        uint32_t sOut = 0;
        for (int i = 0; i < 8; ++i) {
            uint8_t chunk = static_cast<uint8_t>((xorVal >> (42 - 6 * i)) & 0x3F);
            uint8_t row = ((chunk & 0x20) >> 4) | (chunk & 0x01);
            uint8_t col = (chunk >> 1) & 0x0F;
            uint8_t val = DES_SBOX[i][row * 16 + col];
            sOut = (sOut << 4) | val;
        }
        uint32_t pOut = static_cast<uint32_t>(desPermute(sOut, DES_P, 32, 32));
        uint32_t nextR = l ^ pOut;
        l = r;
        r = nextR;
    }

    uint64_t preOutput = (static_cast<uint64_t>(r) << 32) | l;
    return desPermute(preOutput, DES_FP, 64, 64);
}

inline void ExpandKey7To8(const uint8_t in7[7], uint8_t out8[8]) {
    out8[0] = static_cast<uint8_t>(in7[0] & 0xFE);
    out8[1] = static_cast<uint8_t>(((in7[0] << 7) | (in7[1] >> 1)) & 0xFE);
    out8[2] = static_cast<uint8_t>(((in7[1] << 6) | (in7[2] >> 2)) & 0xFE);
    out8[3] = static_cast<uint8_t>(((in7[2] << 5) | (in7[3] >> 3)) & 0xFE);
    out8[4] = static_cast<uint8_t>(((in7[3] << 4) | (in7[4] >> 4)) & 0xFE);
    out8[5] = static_cast<uint8_t>(((in7[4] << 3) | (in7[5] >> 5)) & 0xFE);
    out8[6] = static_cast<uint8_t>(((in7[5] << 2) | (in7[6] >> 6)) & 0xFE);
    out8[7] = static_cast<uint8_t>((in7[6] << 1) & 0xFE);

    for (int i = 0; i < 8; ++i) {
        uint8_t b = out8[i];
        int c = 0;
        for (int bit = 1; bit < 8; ++bit) {
            if ((b >> bit) & 1) c++;
        }
        if ((c % 2) == 0) {
            out8[i] |= 1;
        }
    }
}

inline void DesEncrypt(const uint8_t clear[8], const uint8_t key7[7], uint8_t cipher[8]) {
    uint8_t key8[8];
    ExpandKey7To8(key7, key8);

    uint64_t k64 = 0;
    for (int i = 0; i < 8; ++i) {
        k64 = (k64 << 8) | key8[i];
    }

    uint64_t c64 = 0;
    for (int i = 0; i < 8; ++i) {
        c64 = (c64 << 8) | clear[i];
    }

    uint64_t subkeys[16];
    generateDesSubkeys(k64, subkeys);
    uint64_t enc64 = desEncryptBlock64(c64, subkeys);

    for (int i = 7; i >= 0; --i) {
        cipher[i] = static_cast<uint8_t>(enc64 & 0xFF);
        enc64 >>= 8;
    }
}

// ----------------------------------------------------------------------------
// RFC 2759 MS-CHAPv2 Cryptographic Routines
// ----------------------------------------------------------------------------
inline void ChallengeHash(const uint8_t peerChallenge[16],
                          const uint8_t authChallenge[16],
                          const std::string& userName,
                          uint8_t outChallenge[8]) {
    std::vector<uint8_t> buf;
    buf.reserve(16 + 16 + userName.size());
    buf.insert(buf.end(), peerChallenge, peerChallenge + 16);
    buf.insert(buf.end(), authChallenge, authChallenge + 16);
    buf.insert(buf.end(), userName.begin(), userName.end());

    uint8_t sha[20];
    Sha1Hash(buf.data(), buf.size(), sha);
    std::memcpy(outChallenge, sha, 8);
}

inline void ChallengeResponse(const uint8_t challenge[8],
                              const uint8_t passwordHash[16],
                              uint8_t response[24]) {
    uint8_t zPass[21]{};
    std::memcpy(zPass, passwordHash, 16);
    DesEncrypt(challenge, zPass + 0, response + 0);
    DesEncrypt(challenge, zPass + 7, response + 8);
    DesEncrypt(challenge, zPass + 14, response + 16);
}

inline void GenerateAuthenticatorResponse(const uint8_t passwordHashHash[16],
                                          const uint8_t ntResponse[24],
                                          const uint8_t peerChallenge[16],
                                          const uint8_t authChallenge[16],
                                          const std::string& userName,
                                          std::string& outAuthResponseHex) {
    static const char MAGIC1[] = "Magic server to client signing constant";
    static const char MAGIC2[] = "Pad to make it do more than one hash";

    std::vector<uint8_t> data1;
    data1.reserve(16 + 24 + std::strlen(MAGIC1));
    data1.insert(data1.end(), passwordHashHash, passwordHashHash + 16);
    data1.insert(data1.end(), ntResponse, ntResponse + 24);
    data1.insert(data1.end(), reinterpret_cast<const uint8_t*>(MAGIC1),
                 reinterpret_cast<const uint8_t*>(MAGIC1) + std::strlen(MAGIC1));

    uint8_t digest[20];
    Sha1Hash(data1.data(), data1.size(), digest);

    std::vector<uint8_t> data2;
    data2.reserve(16 + 16 + userName.size());
    data2.insert(data2.end(), peerChallenge, peerChallenge + 16);
    data2.insert(data2.end(), authChallenge, authChallenge + 16);
    data2.insert(data2.end(), userName.begin(), userName.end());

    uint8_t challenge[20];
    Sha1Hash(data2.data(), data2.size(), challenge);

    std::vector<uint8_t> data3;
    data3.reserve(20 + 20 + std::strlen(MAGIC2));
    data3.insert(data3.end(), digest, digest + 20);
    data3.insert(data3.end(), challenge, challenge + 20);
    data3.insert(data3.end(), reinterpret_cast<const uint8_t*>(MAGIC2),
                 reinterpret_cast<const uint8_t*>(MAGIC2) + std::strlen(MAGIC2));

    uint8_t finalHash[20];
    Sha1Hash(data3.data(), data3.size(), finalHash);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 20; ++i) {
        ss << std::setw(2) << static_cast<int>(finalHash[i]);
    }
    std::string hexStr = ss.str();
    for (char& c : hexStr) {
        if (c >= 'a' && c <= 'f') c = static_cast<char>(c - 'a' + 'A');
    }
    outAuthResponseHex = hexStr;
}

inline void DeriveMppeKeys(const uint8_t passwordHashHash[16],
                           const uint8_t ntResponse[24],
                           std::vector<uint8_t>& outSendKey,
                           std::vector<uint8_t>& outRecvKey) {
    static const char MAGIC_MASTER[] = "This is the security level1";
    static const char MAGIC_SEND[]   = "On the client side, this is the send key; on the server side, it is the receive key.";
    static const char MAGIC_RECV[]   = "On the client side, this is the receive key; on the server side, it is the send key.";

    std::vector<uint8_t> d1;
    d1.reserve(16 + 24 + std::strlen(MAGIC_MASTER));
    d1.insert(d1.end(), passwordHashHash, passwordHashHash + 16);
    d1.insert(d1.end(), ntResponse, ntResponse + 24);
    d1.insert(d1.end(), reinterpret_cast<const uint8_t*>(MAGIC_MASTER),
              reinterpret_cast<const uint8_t*>(MAGIC_MASTER) + std::strlen(MAGIC_MASTER));

    uint8_t masterKey[20];
    Sha1Hash(d1.data(), d1.size(), masterKey);

    std::vector<uint8_t> dSend;
    dSend.reserve(16 + 40 + std::strlen(MAGIC_SEND));
    dSend.insert(dSend.end(), masterKey, masterKey + 16);
    for (int i = 0; i < 40; ++i) dSend.push_back(0x00);
    dSend.insert(dSend.end(), reinterpret_cast<const uint8_t*>(MAGIC_SEND),
                 reinterpret_cast<const uint8_t*>(MAGIC_SEND) + std::strlen(MAGIC_SEND));

    uint8_t sendDigest[20];
    Sha1Hash(dSend.data(), dSend.size(), sendDigest);
    outSendKey.assign(sendDigest, sendDigest + 16);

    std::vector<uint8_t> dRecv;
    dRecv.reserve(16 + 40 + std::strlen(MAGIC_RECV));
    dRecv.insert(dRecv.end(), masterKey, masterKey + 16);
    for (int i = 0; i < 40; ++i) dRecv.push_back(0x00);
    dRecv.insert(dRecv.end(), reinterpret_cast<const uint8_t*>(MAGIC_RECV),
                 reinterpret_cast<const uint8_t*>(MAGIC_RECV) + std::strlen(MAGIC_RECV));

    uint8_t recvDigest[20];
    Sha1Hash(dRecv.data(), dRecv.size(), recvDigest);
    outRecvKey.assign(recvDigest, recvDigest + 16);
}

} // namespace crypto

// ============================================================================
// RADIUS RFC 2865, RFC 2866 & RFC 3579 Protocol Definitions
// ============================================================================
enum RadiusCode : uint8_t {
    RadiusCode_AccessRequest       = 1,
    RadiusCode_AccessAccept        = 2,
    RadiusCode_AccessReject        = 3,
    RadiusCode_AccountingRequest   = 4,
    RadiusCode_AccountingResponse  = 5,
    RadiusCode_AccessChallenge     = 11,
    RadiusCode_StatusServer        = 12,
    RadiusCode_StatusClient        = 13,
    RadiusCode_DisconnectRequest   = 40,
    RadiusCode_DisconnectACK       = 41,
    RadiusCode_DisconnectNAK       = 42
};

enum RadiusAttrType : uint8_t {
    RadiusAttr_UserName            = 1,
    RadiusAttr_UserPassword        = 2,
    RadiusAttr_ChapPassword        = 3,
    RadiusAttr_NasIpAddress        = 4,
    RadiusAttr_NasPort             = 5,
    RadiusAttr_ServiceType         = 6,
    RadiusAttr_FramedProtocol      = 7,
    RadiusAttr_FramedIpAddress     = 8,
    RadiusAttr_ReplyMessage        = 18,
    RadiusAttr_Class               = 25,
    RadiusAttr_VendorSpecific      = 26,
    RadiusAttr_SessionTimeout      = 27,
    RadiusAttr_IdleTimeout         = 28,
    RadiusAttr_CalledStationId     = 30,
    RadiusAttr_CallingStationId    = 31,
    RadiusAttr_NasIdentifier       = 32,
    RadiusAttr_AcctStatusType      = 40,
    RadiusAttr_AcctDelayTime       = 41,
    RadiusAttr_AcctInputOctets     = 42,
    RadiusAttr_AcctOutputOctets    = 43,
    RadiusAttr_AcctSessionId       = 44,
    RadiusAttr_AcctAuthentic       = 45,
    RadiusAttr_AcctSessionTime     = 46,
    RadiusAttr_AcctTerminateCause  = 49,
    RadiusAttr_NasPortType         = 61,
    RadiusAttr_TunnelType          = 64,
    RadiusAttr_TunnelMediumType    = 65,
    RadiusAttr_EapMessage          = 79,
    RadiusAttr_MessageAuthenticator= 80,
    RadiusAttr_TunnelPrivateGroupId= 81
};

// ============================================================================
// Microsoft Vendor-Specific Attributes (RFC 2548, Vendor ID 311)
// ============================================================================
inline constexpr uint32_t VENDOR_ID_MICROSOFT = 311;

enum MicrosoftVsaSubtype : uint8_t {
    MsVsa_ChapResponse             = 1,
    MsVsa_ChapError                = 2,
    MsVsa_ChapCpw1                 = 3,
    MsVsa_ChapCpw2                 = 4,
    MsVsa_ChapLmEncPw              = 5,
    MsVsa_ChapNtEncPw              = 6,
    MsVsa_MppeSendKey              = 7,
    MsVsa_MppeRecvKey              = 8,
    MsVsa_ChapChallenge            = 11,
    MsVsa_ChapDomain               = 12,
    MsVsa_ChapChallenge2           = 13,
    MsVsa_ChapMppeKeys             = 15,
    MsVsa_MppeEncryptionPolicy     = 16,
    MsVsa_MppeEncryptionTypes      = 17,
    MsVsa_RasVendor                = 18,
    MsVsa_Chap2Response            = 25,
    MsVsa_Chap2Success             = 26,
    MsVsa_Chap2Cpw                 = 27,
    MsVsa_NetworkAccessServerType  = 28
};

// RADIUS Attribute Structure (TLV)
struct RadiusAttribute {
    uint8_t type{0};
    std::vector<uint8_t> value;

    std::string asString() const {
        return std::string(reinterpret_cast<const char*>(value.data()), value.size());
    }

    uint32_t asUint32() const {
        if (value.size() < 4) return 0;
        return (static_cast<uint32_t>(value[0]) << 24) |
               (static_cast<uint32_t>(value[1]) << 16) |
               (static_cast<uint32_t>(value[2]) << 8)  |
               (static_cast<uint32_t>(value[3]));
    }
};

// Microsoft Sub-Attribute
struct MicrosoftSubAttribute {
    uint8_t subType{0};
    std::vector<uint8_t> value;

    std::string asString() const {
        return std::string(reinterpret_cast<const char*>(value.data()), value.size());
    }

    uint32_t asUint32() const {
        if (value.size() < 4) return 0;
        return (static_cast<uint32_t>(value[0]) << 24) |
               (static_cast<uint32_t>(value[1]) << 16) |
               (static_cast<uint32_t>(value[2]) << 8)  |
               (static_cast<uint32_t>(value[3]));
    }
};

inline RadiusAttribute CreateMicrosoftVsa(uint8_t subType, const std::vector<uint8_t>& subValue) {
    RadiusAttribute attr;
    attr.type = RadiusAttr_VendorSpecific;
    attr.value.resize(4 + 2 + subValue.size());
    attr.value[0] = 0x00;
    attr.value[1] = 0x00;
    attr.value[2] = 0x01;
    attr.value[3] = 0x37; // 311
    attr.value[4] = subType;
    attr.value[5] = static_cast<uint8_t>(2 + subValue.size());
    if (!subValue.empty()) {
        std::memcpy(&attr.value[6], subValue.data(), subValue.size());
    }
    return attr;
}

inline RadiusAttribute CreateMicrosoftVsaString(uint8_t subType, const std::string& str) {
    std::vector<uint8_t> val(str.begin(), str.end());
    return CreateMicrosoftVsa(subType, val);
}

inline RadiusAttribute CreateMicrosoftVsaUint32(uint8_t subType, uint32_t val) {
    std::vector<uint8_t> data(4);
    data[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    data[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    data[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(val & 0xFF);
    return CreateMicrosoftVsa(subType, data);
}

inline bool ParseMicrosoftVsa(const RadiusAttribute& attr, std::vector<MicrosoftSubAttribute>& outSubAttrs) {
    if (attr.type != RadiusAttr_VendorSpecific || attr.value.size() < 6) return false;
    uint32_t vendorId = (static_cast<uint32_t>(attr.value[0]) << 24) |
                        (static_cast<uint32_t>(attr.value[1]) << 16) |
                        (static_cast<uint32_t>(attr.value[2]) << 8)  |
                        (static_cast<uint32_t>(attr.value[3]));
    if (vendorId != VENDOR_ID_MICROSOFT) return false;

    size_t offset = 4;
    while (offset + 2 <= attr.value.size()) {
        uint8_t subType = attr.value[offset];
        uint8_t subLen = attr.value[offset + 1];
        if (subLen < 2 || offset + subLen > attr.value.size()) break;
        MicrosoftSubAttribute sub;
        sub.subType = subType;
        if (subLen > 2) {
            sub.value.assign(&attr.value[offset + 2], &attr.value[offset + subLen]);
        }
        outSubAttrs.push_back(std::move(sub));
        offset += subLen;
    }
    return !outSubAttrs.empty();
}

// RADIUS Packet (RFC 2865 Header + Attributes)
struct RadiusPacket {
    uint8_t code{0};
    uint8_t identifier{0};
    mutable uint16_t length{20};
    uint8_t authenticator[16]{};
    std::vector<RadiusAttribute> attributes;

    void addStringAttribute(uint8_t type, const std::string& val) {
        RadiusAttribute a;
        a.type = type;
        a.value.assign(val.begin(), val.end());
        attributes.push_back(std::move(a));
    }

    void addUint32Attribute(uint8_t type, uint32_t val) {
        RadiusAttribute a;
        a.type = type;
        a.value.resize(4);
        a.value[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
        a.value[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
        a.value[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
        a.value[3] = static_cast<uint8_t>(val & 0xFF);
        attributes.push_back(std::move(a));
    }

    void addRawAttribute(uint8_t type, const uint8_t* data, size_t len) {
        RadiusAttribute a;
        a.type = type;
        if (data && len > 0) {
            a.value.assign(data, data + len);
        }
        attributes.push_back(std::move(a));
    }

    void addAttribute(RadiusAttribute attr) {
        attributes.push_back(std::move(attr));
    }

    const RadiusAttribute* findAttribute(uint8_t type) const {
        for (const auto& a : attributes) {
            if (a.type == type) return &a;
        }
        return nullptr;
    }

    std::vector<uint8_t> serialize() const {
        uint16_t totalLen = 20;
        for (const auto& a : attributes) {
            totalLen += static_cast<uint16_t>(2 + a.value.size());
        }
        length = totalLen;

        std::vector<uint8_t> buf(totalLen);
        buf[0] = code;
        buf[1] = identifier;
        buf[2] = static_cast<uint8_t>((totalLen >> 8) & 0xFF);
        buf[3] = static_cast<uint8_t>(totalLen & 0xFF);
        std::memcpy(&buf[4], authenticator, 16);

        size_t offset = 20;
        for (const auto& a : attributes) {
            uint8_t attrLen = static_cast<uint8_t>(2 + a.value.size());
            buf[offset++] = a.type;
            buf[offset++] = attrLen;
            if (!a.value.empty()) {
                std::memcpy(&buf[offset], a.value.data(), a.value.size());
                offset += a.value.size();
            }
        }
        return buf;
    }

    bool deserialize(const uint8_t* data, size_t size) {
        if (!data || size < 20) return false;
        code = data[0];
        identifier = data[1];
        length = (static_cast<uint16_t>(data[2]) << 8) | static_cast<uint16_t>(data[3]);
        if (length > size || length < 20) return false;

        std::memcpy(authenticator, &data[4], 16);
        attributes.clear();

        size_t offset = 20;
        while (offset + 2 <= length) {
            uint8_t aType = data[offset];
            uint8_t aLen = data[offset + 1];
            if (aLen < 2 || offset + aLen > length) break;
            RadiusAttribute attr;
            attr.type = aType;
            if (aLen > 2) {
                attr.value.assign(&data[offset + 2], &data[offset + aLen]);
            }
            attributes.push_back(std::move(attr));
            offset += aLen;
        }
        return true;
    }
};

// ============================================================================
// RFC 2865 Password Obfuscation (User-Password)
// ============================================================================
inline std::string DecryptRadiusPassword(const std::vector<uint8_t>& encryptedPassword,
                                         const uint8_t requestAuth[16],
                                         const std::string& secret) {
    if (encryptedPassword.empty() || (encryptedPassword.size() % 16) != 0) return "";

    std::vector<uint8_t> plaintext;
    plaintext.reserve(encryptedPassword.size());

    uint8_t prevCipherBlock[16];
    std::memcpy(prevCipherBlock, requestAuth, 16);

    for (size_t i = 0; i < encryptedPassword.size(); i += 16) {
        std::vector<uint8_t> hashInput;
        hashInput.insert(hashInput.end(), secret.begin(), secret.end());
        hashInput.insert(hashInput.end(), prevCipherBlock, prevCipherBlock + 16);

        uint8_t b[16];
        crypto::Md5Hash(hashInput.data(), hashInput.size(), b);

        for (size_t j = 0; j < 16; ++j) {
            uint8_t plainByte = encryptedPassword[i + j] ^ b[j];
            plaintext.push_back(plainByte);
        }
        std::memcpy(prevCipherBlock, &encryptedPassword[i], 16);
    }

    while (!plaintext.empty() && plaintext.back() == 0) {
        plaintext.pop_back();
    }
    return std::string(plaintext.begin(), plaintext.end());
}

inline std::vector<uint8_t> EncryptRadiusPassword(const std::string& password,
                                                   const uint8_t requestAuth[16],
                                                   const std::string& secret) {
    size_t paddedLen = ((password.size() + 15) / 16) * 16;
    if (paddedLen == 0) paddedLen = 16;

    std::vector<uint8_t> padded(paddedLen, 0);
    std::memcpy(padded.data(), password.data(), password.size());

    std::vector<uint8_t> encrypted(paddedLen, 0);
    uint8_t prevCipherBlock[16];
    std::memcpy(prevCipherBlock, requestAuth, 16);

    for (size_t i = 0; i < paddedLen; i += 16) {
        std::vector<uint8_t> hashInput;
        hashInput.insert(hashInput.end(), secret.begin(), secret.end());
        hashInput.insert(hashInput.end(), prevCipherBlock, prevCipherBlock + 16);

        uint8_t b[16];
        crypto::Md5Hash(hashInput.data(), hashInput.size(), b);

        for (size_t j = 0; j < 16; ++j) {
            encrypted[i + j] = padded[i + j] ^ b[j];
        }
        std::memcpy(prevCipherBlock, &encrypted[i], 16);
    }
    return encrypted;
}

// ============================================================================
// Response Authenticator Calculation
// ============================================================================
inline void CalculateResponseAuthenticator(uint8_t outAuth[16],
                                           uint8_t code,
                                           uint8_t id,
                                           uint16_t length,
                                           const uint8_t requestAuth[16],
                                           const uint8_t* attrsData,
                                           size_t attrsLen,
                                           const std::string& secret) {
    std::vector<uint8_t> data;
    data.reserve(4 + 16 + attrsLen + secret.size());
    data.push_back(code);
    data.push_back(id);
    data.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(length & 0xFF));
    data.insert(data.end(), requestAuth, requestAuth + 16);
    if (attrsData && attrsLen > 0) {
        data.insert(data.end(), attrsData, attrsData + attrsLen);
    }
    data.insert(data.end(), secret.begin(), secret.end());
    crypto::Md5Hash(data.data(), data.size(), outAuth);
}

inline void CalculateAccountingRequestAuthenticator(uint8_t outAuth[16],
                                                    uint8_t code,
                                                    uint8_t id,
                                                    uint16_t length,
                                                    const uint8_t* attrsData,
                                                    size_t attrsLen,
                                                    const std::string& secret) {
    std::vector<uint8_t> data;
    data.reserve(4 + 16 + attrsLen + secret.size());
    data.push_back(code);
    data.push_back(id);
    data.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(length & 0xFF));
    uint8_t zeros[16]{};
    data.insert(data.end(), zeros, zeros + 16);
    if (attrsData && attrsLen > 0) {
        data.insert(data.end(), attrsData, attrsData + attrsLen);
    }
    data.insert(data.end(), secret.begin(), secret.end());
    crypto::Md5Hash(data.data(), data.size(), outAuth);
}

// ============================================================================
// RADIUS Client / NAS Representation
// ============================================================================
struct RadiusClient {
    std::string ipAddress;      // e.g. "192.168.1.1"
    std::string friendlyName;   // e.g. "Cisco-Core-Switch"
    std::string sharedSecret;   // e.g. "MicaNT-Secret-2026!"
    std::string vendorName;     // e.g. "Microsoft", "Cisco", "Aruba"
    bool requireMessageAuth{false};
    bool enabled{true};
};

// ============================================================================
// User Directory & Account Record
// ============================================================================
struct NpsUserAccount {
    std::string userName;
    std::string password;
    std::vector<uint8_t> ntPasswordHash; // 16-byte MD4 hash
    std::vector<std::string> groupMemberships;
    bool enabled{true};
    std::string framedIpAddress; // optional static IP assignment
};

// ============================================================================
// Connection Request Policies (CRP)
// ============================================================================
enum class CrpAction {
    AuthenticateLocally,
    ForwardToRemoteGroup,
    Discard
};

struct ConnectionRequestPolicy {
    std::string policyName;
    uint32_t priority{1};
    bool enabled{true};
    std::string nasPortTypePattern; // "Wireless-802.11", "Ethernet", "*"
    std::string clientIpPattern;    // IP or "*"
    CrpAction action{CrpAction::AuthenticateLocally};
    std::string remoteGroup;
};

// ============================================================================
// Network Policies (Authorization Rules & VLANs)
// ============================================================================
enum class PolicyPermission {
    GrantAccess,
    DenyAccess
};

enum class AuthMethod {
    Unspecified,
    PapChap,
    MsChapv2,
    EapTls,
    PeapMsChapv2
};

struct NetworkPolicy {
    std::string policyName;
    uint32_t priority{1};
    bool enabled{true};
    PolicyPermission permission{PolicyPermission::GrantAccess};
    std::vector<std::string> requiredUserGroups;
    std::string allowedNasPortType; // "Wireless-802.11", "*"
    AuthMethod allowedAuthMethod{AuthMethod::PeapMsChapv2};
    uint32_t sessionTimeoutSec{28800}; // 8 hours
    uint32_t idleTimeoutSec{900};      // 15 minutes
    std::string assignedVlanId;        // e.g. "100" (returns Tunnel-Type=13, Tunnel-Medium-Type=6, Tunnel-Private-Group-ID="100")
};

// ============================================================================
// RADIUS Accounting Session Record
// ============================================================================
struct AccountingSessionRecord {
    std::string sessionId;
    std::string userName;
    std::string clientIp;
    std::string callingStationId; // Client MAC
    std::string calledStationId;  // AP MAC / SSID
    uint32_t nasPort{0};
    uint32_t statusType{1}; // 1=Start, 2=Stop, 3=Interim
    uint64_t startTimeUs{0};
    uint64_t lastUpdateTimeUs{0};
    uint64_t stopTimeUs{0};
    uint64_t inputOctets{0};
    uint64_t outputOctets{0};
    uint32_t sessionTimeSec{0};
    uint32_t terminateCause{0};
    bool active{true};
};

// ============================================================================
// NetworkPolicyServer: Singleton Core Engine
// ============================================================================
class NetworkPolicyServer {
private:
    std::mutex m_mutex;
    bool m_initialized{false};

    // Configuration
    std::map<std::string, RadiusClient> m_clients; // IP -> Client
    std::map<std::string, NpsUserAccount> m_users; // UserName -> User
    std::vector<ConnectionRequestPolicy> m_crpPolicies;
    std::vector<NetworkPolicy> m_networkPolicies;

    // Accounting Data Store
    std::map<std::string, AccountingSessionRecord> m_accountingSessions; // SessionId -> Record
    std::deque<std::string> m_accountingAuditLog;

    // Metrics
    std::atomic<uint64_t> m_totalAuthRequests{0};
    std::atomic<uint64_t> m_totalAuthSuccesses{0};
    std::atomic<uint64_t> m_totalAuthRejections{0};
    std::atomic<uint64_t> m_totalAcctRequests{0};
    std::atomic<uint64_t> m_totalInputOctets{0};
    std::atomic<uint64_t> m_totalOutputOctets{0};
    std::atomic<uint64_t> m_lastAuthLatencyUs{12};

    NetworkPolicyServer() {
        initializeDefaults();
    }

public:
    static NetworkPolicyServer& instance() {
        static NetworkPolicyServer s_inst;
        return s_inst;
    }

    void initializeDefaults() {
        std::lock_guard<std::mutex> lock(m_mutex);
        initializeDefaultsLocked();
    }

private:
    void initializeDefaultsLocked() {
        if (m_initialized) return;

        // Register default clients (NAS devices)
        RadiusClient cLocal{};
        cLocal.ipAddress = "127.0.0.1";
        cLocal.friendlyName = "Localhost-Test-Client";
        cLocal.sharedSecret = "MicaSecret2026!";
        cLocal.vendorName = "Microsoft";
        m_clients[cLocal.ipAddress] = cLocal;

        RadiusClient cSwitch{};
        cSwitch.ipAddress = "192.168.1.1";
        cSwitch.friendlyName = "Cisco-Core-Switch";
        cSwitch.sharedSecret = "CiscoSecretKey!";
        cSwitch.vendorName = "Cisco";
        m_clients[cSwitch.ipAddress] = cSwitch;

        RadiusClient cAp{};
        cAp.ipAddress = "192.168.1.10";
        cAp.friendlyName = "Aruba-Corporate-AP";
        cAp.sharedSecret = "ArubaSecureWiFi!";
        cAp.vendorName = "Aruba";
        cAp.requireMessageAuth = true;
        m_clients[cAp.ipAddress] = cAp;

        // Register default enterprise users
        NpsUserAccount uAdmin{};
        uAdmin.userName = "Administrator";
        uAdmin.password = "Password123!";
        uAdmin.ntPasswordHash.resize(16);
        crypto::NtPasswordHash(uAdmin.password, uAdmin.ntPasswordHash.data());
        uAdmin.groupMemberships = {"Domain Admins", "Administrators", "Enterprise Admins"};
        uAdmin.enabled = true;
        uAdmin.framedIpAddress = "10.10.1.10";
        m_users[uAdmin.userName] = uAdmin;

        NpsUserAccount uMicaAdmin{};
        uMicaAdmin.userName = "MicaAdmin";
        uMicaAdmin.password = "MicaAdmin2026!";
        uMicaAdmin.ntPasswordHash.resize(16);
        crypto::NtPasswordHash(uMicaAdmin.password, uMicaAdmin.ntPasswordHash.data());
        uMicaAdmin.groupMemberships = {"Domain Admins", "Administrators"};
        uMicaAdmin.enabled = true;
        uMicaAdmin.framedIpAddress = "10.10.1.11";
        m_users[uMicaAdmin.userName] = uMicaAdmin;

        NpsUserAccount uAlice{};
        uAlice.userName = "Alice";
        uAlice.password = "AliceSecure2026!";
        uAlice.ntPasswordHash.resize(16);
        crypto::NtPasswordHash(uAlice.password, uAlice.ntPasswordHash.data());
        uAlice.groupMemberships = {"CorpUsers", "Engineering"};
        uAlice.enabled = true;
        uAlice.framedIpAddress = "10.10.100.50";
        m_users[uAlice.userName] = uAlice;

        NpsUserAccount uBob{};
        uBob.userName = "Bob";
        uBob.password = "BobPass!";
        uBob.ntPasswordHash.resize(16);
        crypto::NtPasswordHash(uBob.password, uBob.ntPasswordHash.data());
        uBob.groupMemberships = {"CorpUsers", "Sales"};
        uBob.enabled = true;
        uBob.framedIpAddress = "10.10.100.51";
        m_users[uBob.userName] = uBob;

        NpsUserAccount uDisabled{};
        uDisabled.userName = "DisabledUser";
        uDisabled.password = "NeverLogon!";
        uDisabled.ntPasswordHash.resize(16);
        crypto::NtPasswordHash(uDisabled.password, uDisabled.ntPasswordHash.data());
        uDisabled.groupMemberships = {"DisabledAccounts"};
        uDisabled.enabled = false;
        m_users[uDisabled.userName] = uDisabled;

        // Default Connection Request Policies (CRP)
        ConnectionRequestPolicy crp1{};
        crp1.policyName = "All 802.1X Wireless & Wired Access";
        crp1.priority = 1;
        crp1.enabled = true;
        crp1.nasPortTypePattern = "*";
        crp1.clientIpPattern = "*";
        crp1.action = CrpAction::AuthenticateLocally;
        m_crpPolicies.push_back(crp1);

        // Default Network Policies
        NetworkPolicy npDisabled{};
        npDisabled.policyName = "Deny Disabled Accounts Policy";
        npDisabled.priority = 0; // Highest precedence
        npDisabled.enabled = true;
        npDisabled.permission = PolicyPermission::DenyAccess;
        npDisabled.requiredUserGroups = {"DisabledAccounts"};
        npDisabled.allowedNasPortType = "*";
        m_networkPolicies.push_back(npDisabled);

        NetworkPolicy npWireless{};
        npWireless.policyName = "Corporate Wireless Network Policy (VLAN 100)";
        npWireless.priority = 1;
        npWireless.enabled = true;
        npWireless.permission = PolicyPermission::GrantAccess;
        npWireless.requiredUserGroups = {"CorpUsers"};
        npWireless.allowedNasPortType = "Wireless-802.11";
        npWireless.allowedAuthMethod = AuthMethod::PeapMsChapv2;
        npWireless.sessionTimeoutSec = 28800; // 8 hours
        npWireless.idleTimeoutSec = 900;     // 15 mins
        npWireless.assignedVlanId = "100";
        m_networkPolicies.push_back(npWireless);

        NetworkPolicy npAdmin{};
        npAdmin.policyName = "Domain Admins Infrastructure Access (VLAN 10)";
        npAdmin.priority = 2;
        npAdmin.enabled = true;
        npAdmin.permission = PolicyPermission::GrantAccess;
        npAdmin.requiredUserGroups = {"Domain Admins"};
        npAdmin.allowedNasPortType = "*";
        npAdmin.sessionTimeoutSec = 43200; // 12 hours
        npAdmin.idleTimeoutSec = 1800;    // 30 mins
        npAdmin.assignedVlanId = "10";
        m_networkPolicies.push_back(npAdmin);

        NetworkPolicy npWired{};
        npWired.policyName = "Corporate Wired Ethernet Policy (VLAN 20)";
        npWired.priority = 3;
        npWired.enabled = true;
        npWired.permission = PolicyPermission::GrantAccess;
        npWired.requiredUserGroups = {"CorpUsers"};
        npWired.allowedNasPortType = "Ethernet";
        npWired.sessionTimeoutSec = 28800; // 8 hours
        npWired.idleTimeoutSec = 900;     // 15 mins
        npWired.assignedVlanId = "20";
        m_networkPolicies.push_back(npWired);

        // Register SCM services & VersionDatabase
        registerScmServices();
        registerVersionDatabase();

        m_initialized = true;
    }

public:
    void resetForTest() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clients.clear();
        m_users.clear();
        m_crpPolicies.clear();
        m_networkPolicies.clear();
        m_accountingSessions.clear();
        m_accountingAuditLog.clear();

        m_totalAuthRequests.store(0);
        m_totalAuthSuccesses.store(0);
        m_totalAuthRejections.store(0);
        m_totalAcctRequests.store(0);
        m_totalInputOctets.store(0);
        m_totalOutputOctets.store(0);
        m_lastAuthLatencyUs.store(10);

        m_initialized = false;
        initializeDefaultsLocked();
    }

    void registerScmServices() {
        auto& scm = micant::scm::ServiceControlManager::get();

        // 1. IAS (Network Policy Server)
        auto recIas = std::make_shared<micant::scm::ServiceRecord>();
        recIas->serviceName = L"IAS";
        recIas->displayName = L"Network Policy Server";
        recIas->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recIas->startType = micant::scm::SERVICE_AUTO_START;
        recIas->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recIas->binaryPath = L"C:\\Windows\\System32\\ias.dll";
        recIas->status.dwServiceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recIas->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        recIas->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN;
        scm.registerServiceRecord(recIas);

        // 2. RadiusProxy (RADIUS Proxy Service)
        auto recProxy = std::make_shared<micant::scm::ServiceRecord>();
        recProxy->serviceName = L"RadiusProxy";
        recProxy->displayName = L"RADIUS Proxy Service";
        recProxy->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recProxy->startType = micant::scm::SERVICE_DEMAND_START;
        recProxy->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recProxy->binaryPath = L"C:\\Windows\\System32\\iasrad.dll";
        recProxy->status.dwServiceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recProxy->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        scm.registerServiceRecord(recProxy);

        // 3. RadiusSys (RADIUS Kernel Protocol Filter Driver)
        auto recDrv = std::make_shared<micant::scm::ServiceRecord>();
        recDrv->serviceName = L"RadiusSys";
        recDrv->displayName = L"RADIUS Protocol Filter Driver";
        recDrv->serviceType = micant::scm::SERVICE_KERNEL_DRIVER;
        recDrv->startType = micant::scm::SERVICE_DEMAND_START;
        recDrv->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recDrv->binaryPath = L"C:\\Windows\\System32\\drivers\\radius.sys";
        recDrv->status.dwServiceType = micant::scm::SERVICE_KERNEL_DRIVER;
        recDrv->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        scm.registerServiceRecord(recDrv);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("ias.dll", "10.0.26100.1", "Network Policy Server Core Engine");
        db.RegisterModule("iaspolcy.dll", "10.0.26100.1", "Network Policy Evaluation Engine");
        db.RegisterModule("iasrad.dll", "10.0.26100.1", "RADIUS Protocol & Packet Handler");
        db.RegisterModule("iascore.dll", "10.0.26100.1", "IAS Core Pipeline & State Machine");
        db.RegisterModule("iashlpr.dll", "10.0.26100.1", "Netsh NPS Helper Context");
        db.RegisterModule("radius.sys", "10.0.26100.1", "RADIUS Network Protocol Driver");
        db.RegisterModule("nps.msc", "10.0.26100.1", "Network Policy Server Management Console");
    }

    // Client Management
    bool registerClient(const RadiusClient& client) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clients[client.ipAddress] = client;
        return true;
    }

    bool deleteClient(const std::string& nameOrIp) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clients.find(nameOrIp);
        if (it != m_clients.end()) {
            m_clients.erase(it);
            return true;
        }
        for (auto cur = m_clients.begin(); cur != m_clients.end(); ++cur) {
            if (cur->second.friendlyName == nameOrIp) {
                m_clients.erase(cur);
                return true;
            }
        }
        return false;
    }

    bool getClient(const std::string& ip, RadiusClient* outClient) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clients.find(ip);
        if (it == m_clients.end()) return false;
        if (outClient) *outClient = it->second;
        return true;
    }

    std::vector<RadiusClient> getAllClients() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<RadiusClient> res;
        for (const auto& [ip, c] : m_clients) {
            res.push_back(c);
        }
        return res;
    }

    // User Directory Management
    bool registerUser(const NpsUserAccount& user) {
        std::lock_guard<std::mutex> lock(m_mutex);
        NpsUserAccount copy = user;
        if (copy.ntPasswordHash.empty() && !copy.password.empty()) {
            copy.ntPasswordHash.resize(16);
            crypto::NtPasswordHash(copy.password, copy.ntPasswordHash.data());
        }
        m_users[user.userName] = copy;
        return true;
    }

    bool getUser(const std::string& userName, NpsUserAccount* outUser) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Normalize user name: strip domain prefix e.g. "MICANT\User" -> "User"
        std::string sam = userName;
        auto bsPos = sam.find('\\');
        if (bsPos != std::string::npos) {
            sam = sam.substr(bsPos + 1);
        }
        auto atPos = sam.find('@');
        if (atPos != std::string::npos) {
            sam = sam.substr(0, atPos);
        }

        // 1. Check local NPS database
        auto it = m_users.find(sam);
        if (it != m_users.end()) {
            if (outUser) *outUser = it->second;
            return true;
        }

        // 2. Query Active Directory Domain Services engine
        auto adObjOpt = ad_ds::SovereignActiveDirectoryEngine::get().getObjectBySam(sam);
        if (adObjOpt.has_value() && adObjOpt->isUser()) {
            const auto& adObj = *adObjOpt;
            NpsUserAccount acct;
            acct.userName = adObj.samAccountName;
            acct.enabled = ((adObj.userAccountControl & ad_ds::UF_ACCOUNTDISABLE) == 0);
            if (!adObj.passwordHash.empty()) {
                // If stored as hex, convert to binary
                if (adObj.passwordHash.size() == 32) {
                    acct.ntPasswordHash.resize(16);
                    for (size_t i = 0; i < 16; ++i) {
                        std::string byteHex = adObj.passwordHash.substr(i * 2, 2);
                        acct.ntPasswordHash[i] = static_cast<uint8_t>(std::stoul(byteHex, nullptr, 16));
                    }
                } else {
                    acct.ntPasswordHash.assign(adObj.passwordHash.begin(), adObj.passwordHash.end());
                }
            }
            // Parse groups from memberOf
            for (const auto& gDn : adObj.memberOf) {
                // e.g. "CN=Domain Admins,CN=Users,DC=micant,DC=local"
                auto cnPos = gDn.find("CN=");
                if (cnPos != std::string::npos) {
                    auto commaPos = gDn.find(',', cnPos);
                    std::string gName = (commaPos != std::string::npos)
                        ? gDn.substr(cnPos + 3, commaPos - (cnPos + 3))
                        : gDn.substr(cnPos + 3);
                    acct.groupMemberships.push_back(gName);
                }
            }
            if (outUser) *outUser = acct;
            return true;
        }

        return false;
    }

    // Policy Management
    void addNetworkPolicy(const NetworkPolicy& pol) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_networkPolicies.push_back(pol);
        std::sort(m_networkPolicies.begin(), m_networkPolicies.end(),
                  [](const NetworkPolicy& a, const NetworkPolicy& b) { return a.priority < b.priority; });
    }

    std::vector<NetworkPolicy> getNetworkPolicies() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_networkPolicies;
    }

    // ========================================================================
    // RADIUS Packet Processing Engine
    // ========================================================================
    bool processPacket(const std::string& sourceIp, const uint8_t* inData, size_t inLen,
                       std::vector<uint8_t>& outResponse) {
        auto t0 = std::chrono::high_resolution_clock::now();
        RadiusPacket inPkt;
        if (!inPkt.deserialize(inData, inLen)) {
            return false;
        }

        RadiusClient client{};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_clients.find(sourceIp);
            if (it == m_clients.end()) {
                const auto* attrNas = inPkt.findAttribute(RadiusAttr_NasIpAddress);
                if (attrNas && attrNas->value.size() == 4) {
                    std::string nasIp = std::to_string(attrNas->value[0]) + "." +
                                        std::to_string(attrNas->value[1]) + "." +
                                        std::to_string(attrNas->value[2]) + "." +
                                        std::to_string(attrNas->value[3]);
                    it = m_clients.find(nasIp);
                }
            }
            if (it == m_clients.end() || !it->second.enabled) {
                return false; // Unknown or disabled RADIUS client
            }
            client = it->second;
        }

        bool result = false;
        if (inPkt.code == RadiusCode_AccessRequest) {
            result = handleAccessRequest(client, inPkt, outResponse);
        } else if (inPkt.code == RadiusCode_AccountingRequest) {
            result = handleAccountingRequest(client, inPkt, outResponse);
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        auto durUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        if (durUs > 0) m_lastAuthLatencyUs.store(static_cast<uint64_t>(durUs), std::memory_order_relaxed);

        return result;
    }

private:
    bool handleAccessRequest(const RadiusClient& client, const RadiusPacket& req, std::vector<uint8_t>& outResponse) {
        m_totalAuthRequests.fetch_add(1, std::memory_order_relaxed);

        const auto* aUser = req.findAttribute(RadiusAttr_UserName);
        const auto* aPass = req.findAttribute(RadiusAttr_UserPassword);
        const auto* aEap = req.findAttribute(RadiusAttr_EapMessage);
        const auto* aPortType = req.findAttribute(RadiusAttr_NasPortType);

        std::string userName = aUser ? aUser->asString() : "";
        uint32_t portType = aPortType ? aPortType->asUint32() : 15; // default 15 = Ethernet, 19 = Wireless

        // --------------------------------------------------------------------
        // Check for Microsoft MS-CHAPv2 Vendor-Specific Attributes (RFC 2548)
        // --------------------------------------------------------------------
        for (const auto& attr : req.attributes) {
            if (attr.type == RadiusAttr_VendorSpecific) {
                std::vector<MicrosoftSubAttribute> msSubs;
                if (ParseMicrosoftVsa(attr, msSubs)) {
                    for (const auto& sub : msSubs) {
                        if (sub.subType == MsVsa_Chap2Response && sub.value.size() >= 50) {
                            return handleMsChapv2Request(client, req, userName, sub.value, portType, outResponse);
                        }
                    }
                }
            }
        }

        // --------------------------------------------------------------------
        // 1. EAP Handshake (802.1X Wireless / Wired)
        // --------------------------------------------------------------------
        if (aEap && !aEap->value.empty()) {
            const auto& eapVal = aEap->value;
            uint8_t eapCode = eapVal[0];
            uint8_t eapId = (eapVal.size() > 1) ? eapVal[1] : 1;
            uint8_t eapType = (eapVal.size() > 4) ? eapVal[4] : 0;

            // EAP Response: Identity (Code 2, Type 1) -> Challenge (PEAP Start)
            if (eapCode == 2 && eapType == 1) {
                RadiusPacket resp;
                resp.code = RadiusCode_AccessChallenge;
                resp.identifier = req.identifier;

                uint8_t eapChallengePayload[] = { 0x01, static_cast<uint8_t>(eapId + 1), 0x00, 0x06, 0x19, 0x20 };
                resp.addRawAttribute(RadiusAttr_EapMessage, eapChallengePayload, sizeof(eapChallengePayload));
                resp.addStringAttribute(RadiusAttr_ReplyMessage, "EAP-TLS/PEAP Handshake Initiated");

                auto rawResp = resp.serialize();
                CalculateResponseAuthenticator(resp.authenticator, resp.code, resp.identifier,
                                               static_cast<uint16_t>(rawResp.size()), req.authenticator,
                                               rawResp.data() + 20, rawResp.size() - 20, client.sharedSecret);
                outResponse = resp.serialize();
                return true;
            }

            // EAP Response / Final Handshake verification
            NpsUserAccount userAcct{};
            bool userFound = getUser(userName, &userAcct);
            if (!userFound || !userAcct.enabled) {
                m_totalAuthRejections.fetch_add(1, std::memory_order_relaxed);
                return generateAccessReject(client, req, "EAP Authentication Failed: User invalid or disabled", outResponse);
            }

            // EAP Success -> Access-Accept
            RadiusPacket resp;
            resp.code = RadiusCode_AccessAccept;
            resp.identifier = req.identifier;
            uint8_t eapSuccessPayload[] = { 0x03, eapId, 0x00, 0x04 }; // Code 3 = Success
            resp.addRawAttribute(RadiusAttr_EapMessage, eapSuccessPayload, sizeof(eapSuccessPayload));
            resp.addStringAttribute(RadiusAttr_Class, "MicaNT-8021X-Authorized");
            resp.addUint32Attribute(RadiusAttr_SessionTimeout, 28800);

            // Check network policy for VLAN
            NetworkPolicy matchedPol{};
            if (evaluateNetworkPolicies(userAcct, portType, &matchedPol)) {
                if (!matchedPol.assignedVlanId.empty()) {
                    resp.addUint32Attribute(RadiusAttr_TunnelType, 13); // VLAN
                    resp.addUint32Attribute(RadiusAttr_TunnelMediumType, 6); // 802
                    resp.addStringAttribute(RadiusAttr_TunnelPrivateGroupId, matchedPol.assignedVlanId);
                }
            }

            auto rawResp = resp.serialize();
            CalculateResponseAuthenticator(resp.authenticator, resp.code, resp.identifier,
                                           static_cast<uint16_t>(rawResp.size()), req.authenticator,
                                           rawResp.data() + 20, rawResp.size() - 20, client.sharedSecret);
            outResponse = resp.serialize();
            m_totalAuthSuccesses.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        // --------------------------------------------------------------------
        // 2. Standard Password Authentication (RFC 2865 User-Password)
        // --------------------------------------------------------------------
        if (!aPass) {
            m_totalAuthRejections.fetch_add(1, std::memory_order_relaxed);
            return generateAccessReject(client, req, "Missing authentication credentials", outResponse);
        }

        std::string plainPass = DecryptRadiusPassword(aPass->value, req.authenticator, client.sharedSecret);

        NpsUserAccount userAcct{};
        bool userFound = getUser(userName, &userAcct);
        if (!userFound || userAcct.password != plainPass || !userAcct.enabled) {
            m_totalAuthRejections.fetch_add(1, std::memory_order_relaxed);
            return generateAccessReject(client, req, "Access Denied: Bad username or password", outResponse);
        }

        // 3. Network Policy Evaluation
        NetworkPolicy matchedPolicy{};
        if (!evaluateNetworkPolicies(userAcct, portType, &matchedPolicy)) {
            m_totalAuthRejections.fetch_add(1, std::memory_order_relaxed);
            return generateAccessReject(client, req, "Access Denied: Network Policy match rejected connection", outResponse);
        }

        if (matchedPolicy.permission == PolicyPermission::DenyAccess) {
            m_totalAuthRejections.fetch_add(1, std::memory_order_relaxed);
            return generateAccessReject(client, req, "Access Denied: Explicitly denied by policy " + matchedPolicy.policyName, outResponse);
        }

        // 4. Generate Access-Accept
        RadiusPacket acceptPkt;
        acceptPkt.code = RadiusCode_AccessAccept;
        acceptPkt.identifier = req.identifier;
        acceptPkt.addUint32Attribute(RadiusAttr_ServiceType, 2); // Framed
        acceptPkt.addUint32Attribute(RadiusAttr_FramedProtocol, 1); // PPP
        if (!userAcct.framedIpAddress.empty()) {
            acceptPkt.addStringAttribute(RadiusAttr_FramedIpAddress, userAcct.framedIpAddress);
        }
        acceptPkt.addUint32Attribute(RadiusAttr_SessionTimeout, matchedPolicy.sessionTimeoutSec);
        acceptPkt.addUint32Attribute(RadiusAttr_IdleTimeout, matchedPolicy.idleTimeoutSec);
        acceptPkt.addStringAttribute(RadiusAttr_Class, "MicaNT-NPS-Session:" + matchedPolicy.policyName);

        if (!matchedPolicy.assignedVlanId.empty()) {
            acceptPkt.addUint32Attribute(RadiusAttr_TunnelType, 13); // VLAN
            acceptPkt.addUint32Attribute(RadiusAttr_TunnelMediumType, 6); // 802
            acceptPkt.addStringAttribute(RadiusAttr_TunnelPrivateGroupId, matchedPolicy.assignedVlanId);
        }

        auto rawAccept = acceptPkt.serialize();
        CalculateResponseAuthenticator(acceptPkt.authenticator, acceptPkt.code, acceptPkt.identifier,
                                       static_cast<uint16_t>(rawAccept.size()), req.authenticator,
                                       rawAccept.data() + 20, rawAccept.size() - 20, client.sharedSecret);

        outResponse = acceptPkt.serialize();
        m_totalAuthSuccesses.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    bool handleMsChapv2Request(const RadiusClient& client, const RadiusPacket& req,
                               const std::string& userName, const std::vector<uint8_t>& chap2Val,
                               uint32_t portType, std::vector<uint8_t>& outResponse) {
        // MS-CHAP2-Response format:
        // Ident (1B), Flags (1B), PeerChallenge (16B), Reserved (8B), NTResponse (24B)
        uint8_t ident = chap2Val[0];
        const uint8_t* peerChallenge = &chap2Val[2];
        const uint8_t* receivedNtResponse = &chap2Val[26];

        // Determine Authenticator Challenge (MS-CHAP-Challenge or Request Authenticator)
        uint8_t authChallenge[16]{};
        bool foundCustomChallenge = false;
        for (const auto& attr : req.attributes) {
            if (attr.type == RadiusAttr_VendorSpecific) {
                std::vector<MicrosoftSubAttribute> msSubs;
                if (ParseMicrosoftVsa(attr, msSubs)) {
                    for (const auto& sub : msSubs) {
                        if (sub.subType == MsVsa_ChapChallenge && sub.value.size() >= 16) {
                            std::memcpy(authChallenge, sub.value.data(), 16);
                            foundCustomChallenge = true;
                            break;
                        }
                    }
                }
            }
            if (foundCustomChallenge) break;
        }
        if (!foundCustomChallenge) {
            std::memcpy(authChallenge, req.authenticator, 16);
        }

        // Lookup user
        NpsUserAccount userAcct{};
        bool userFound = getUser(userName, &userAcct);
        if (!userFound || !userAcct.enabled || userAcct.ntPasswordHash.size() != 16) {
            m_totalAuthRejections.fetch_add(1, std::memory_order_relaxed);
            return generateMsChapErrorReject(client, req, ident, "E=691 R=0 V=3 M=Authentication failure", outResponse);
        }

        // Compute Challenge Hash (RFC 2759)
        std::string samOnly = userAcct.userName;
        auto bsPos = samOnly.find('\\');
        if (bsPos != std::string::npos) samOnly = samOnly.substr(bsPos + 1);

        uint8_t challenge8[8];
        crypto::ChallengeHash(peerChallenge, authChallenge, samOnly, challenge8);

        // Compute expected NT-Response
        uint8_t expectedNtResponse[24];
        crypto::ChallengeResponse(challenge8, userAcct.ntPasswordHash.data(), expectedNtResponse);

        if (std::memcmp(receivedNtResponse, expectedNtResponse, 24) != 0) {
            m_totalAuthRejections.fetch_add(1, std::memory_order_relaxed);
            return generateMsChapErrorReject(client, req, ident, "E=691 R=0 V=3 M=Bad password", outResponse);
        }

        // Password hash of hash: MD4(NtPasswordHash)
        uint8_t pHashHash[16];
        crypto::Md4Hash(userAcct.ntPasswordHash.data(), 16, pHashHash);

        // Generate AuthenticatorResponse (RFC 2759)
        std::string authRespHex;
        crypto::GenerateAuthenticatorResponse(pHashHash, receivedNtResponse,
                                              peerChallenge, authChallenge, samOnly, authRespHex);

        // Evaluate network policy
        NetworkPolicy matchedPolicy{};
        if (!evaluateNetworkPolicies(userAcct, portType, &matchedPolicy) ||
            matchedPolicy.permission == PolicyPermission::DenyAccess) {
            m_totalAuthRejections.fetch_add(1, std::memory_order_relaxed);
            return generateMsChapErrorReject(client, req, ident, "E=691 R=0 V=3 M=Access denied by network policy", outResponse);
        }

        // Derive MPPE Keys
        std::vector<uint8_t> mppeSendKey, mppeRecvKey;
        crypto::DeriveMppeKeys(pHashHash, receivedNtResponse, mppeSendKey, mppeRecvKey);

        // Build Access-Accept Packet
        RadiusPacket acceptPkt;
        acceptPkt.code = RadiusCode_AccessAccept;
        acceptPkt.identifier = req.identifier;

        // MS-CHAP2-Success: Ident (1B) + String "S=<Hex> M=Welcome to MicaNT Enterprise Network"
        std::vector<uint8_t> successVal;
        successVal.push_back(ident);
        std::string successStr = "S=" + authRespHex + " M=Welcome to MicaNT Enterprise Network";
        successVal.insert(successVal.end(), successStr.begin(), successStr.end());
        acceptPkt.addAttribute(CreateMicrosoftVsa(MsVsa_Chap2Success, successVal));

        // MPPE Attributes
        acceptPkt.addAttribute(CreateMicrosoftVsaUint32(MsVsa_MppeEncryptionPolicy, 2)); // Encryption Required
        acceptPkt.addAttribute(CreateMicrosoftVsaUint32(MsVsa_MppeEncryptionTypes, 4));  // 128-bit RC4/AES

        // MPPE Send & Recv Keys with 2-byte salt (high bit set)
        std::vector<uint8_t> saltSendKey = { 0x80, 0x01 };
        saltSendKey.insert(saltSendKey.end(), mppeSendKey.begin(), mppeSendKey.end());
        acceptPkt.addAttribute(CreateMicrosoftVsa(MsVsa_MppeSendKey, saltSendKey));

        std::vector<uint8_t> saltRecvKey = { 0x80, 0x02 };
        saltRecvKey.insert(saltRecvKey.end(), mppeRecvKey.begin(), mppeRecvKey.end());
        acceptPkt.addAttribute(CreateMicrosoftVsa(MsVsa_MppeRecvKey, saltRecvKey));

        // Standard RADIUS framed attributes
        acceptPkt.addUint32Attribute(RadiusAttr_ServiceType, 2); // Framed
        acceptPkt.addUint32Attribute(RadiusAttr_FramedProtocol, 1); // PPP
        acceptPkt.addUint32Attribute(RadiusAttr_SessionTimeout, matchedPolicy.sessionTimeoutSec);
        acceptPkt.addUint32Attribute(RadiusAttr_IdleTimeout, matchedPolicy.idleTimeoutSec);
        acceptPkt.addStringAttribute(RadiusAttr_Class, "MicaNT-NPS-Session:" + matchedPolicy.policyName);

        if (!matchedPolicy.assignedVlanId.empty()) {
            acceptPkt.addUint32Attribute(RadiusAttr_TunnelType, 13); // VLAN
            acceptPkt.addUint32Attribute(RadiusAttr_TunnelMediumType, 6); // 802
            acceptPkt.addStringAttribute(RadiusAttr_TunnelPrivateGroupId, matchedPolicy.assignedVlanId);
        }

        auto rawAccept = acceptPkt.serialize();
        CalculateResponseAuthenticator(acceptPkt.authenticator, acceptPkt.code, acceptPkt.identifier,
                                       static_cast<uint16_t>(rawAccept.size()), req.authenticator,
                                       rawAccept.data() + 20, rawAccept.size() - 20, client.sharedSecret);

        outResponse = acceptPkt.serialize();
        m_totalAuthSuccesses.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    bool generateMsChapErrorReject(const RadiusClient& client, const RadiusPacket& req,
                                  uint8_t ident, const std::string& errString, std::vector<uint8_t>& outResponse) {
        RadiusPacket rejectPkt;
        rejectPkt.code = RadiusCode_AccessReject;
        rejectPkt.identifier = req.identifier;

        std::vector<uint8_t> errVal;
        errVal.push_back(ident);
        errVal.insert(errVal.end(), errString.begin(), errString.end());
        rejectPkt.addAttribute(CreateMicrosoftVsa(MsVsa_ChapError, errVal));
        rejectPkt.addStringAttribute(RadiusAttr_ReplyMessage, "Authentication failed (MS-CHAPv2)");

        auto rawReject = rejectPkt.serialize();
        CalculateResponseAuthenticator(rejectPkt.authenticator, rejectPkt.code, rejectPkt.identifier,
                                       static_cast<uint16_t>(rawReject.size()), req.authenticator,
                                       rawReject.data() + 20, rawReject.size() - 20, client.sharedSecret);
        outResponse = rejectPkt.serialize();
        return true;
    }

    bool generateAccessReject(const RadiusClient& client, const RadiusPacket& req,
                              const std::string& reason, std::vector<uint8_t>& outResponse) {
        RadiusPacket rejectPkt;
        rejectPkt.code = RadiusCode_AccessReject;
        rejectPkt.identifier = req.identifier;
        rejectPkt.addStringAttribute(RadiusAttr_ReplyMessage, reason);

        auto rawReject = rejectPkt.serialize();
        CalculateResponseAuthenticator(rejectPkt.authenticator, rejectPkt.code, rejectPkt.identifier,
                                       static_cast<uint16_t>(rawReject.size()), req.authenticator,
                                       rawReject.data() + 20, rawReject.size() - 20, client.sharedSecret);
        outResponse = rejectPkt.serialize();
        return true;
    }

public:
    bool evaluateNetworkPolicies(const NpsUserAccount& user, uint32_t portType, NetworkPolicy* outPolicy) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string portTypeStr = (portType == 19) ? "Wireless-802.11" : "Ethernet";

        for (const auto& pol : m_networkPolicies) {
            if (!pol.enabled) continue;

            // Check Port Type
            if (pol.allowedNasPortType != "*" && pol.allowedNasPortType != portTypeStr) {
                continue;
            }

            // Check Group Membership
            bool groupMatched = pol.requiredUserGroups.empty();
            for (const auto& reqGroup : pol.requiredUserGroups) {
                if (std::find(user.groupMemberships.begin(), user.groupMemberships.end(), reqGroup) != user.groupMemberships.end()) {
                    groupMatched = true;
                    break;
                }
            }

            if (groupMatched) {
                if (outPolicy) *outPolicy = pol;
                return true;
            }
        }
        return false;
    }

    bool handleAccountingRequest(const RadiusClient& client, const RadiusPacket& req, std::vector<uint8_t>& outResponse) {
        m_totalAcctRequests.fetch_add(1, std::memory_order_relaxed);

        uint8_t expectedAuth[16];
        auto rawIn = req.serialize();
        CalculateAccountingRequestAuthenticator(expectedAuth, req.code, req.identifier, req.length,
                                                rawIn.data() + 20, rawIn.size() - 20, client.sharedSecret);

        if (std::memcmp(req.authenticator, expectedAuth, 16) != 0) {
            return false;
        }

        const auto* aStatus = req.findAttribute(RadiusAttr_AcctStatusType);
        const auto* aSessId = req.findAttribute(RadiusAttr_AcctSessionId);
        const auto* aUser = req.findAttribute(RadiusAttr_UserName);
        const auto* aInOctets = req.findAttribute(RadiusAttr_AcctInputOctets);
        const auto* aOutOctets = req.findAttribute(RadiusAttr_AcctOutputOctets);
        const auto* aSessTime = req.findAttribute(RadiusAttr_AcctSessionTime);
        const auto* aTermCause = req.findAttribute(RadiusAttr_AcctTerminateCause);
        const auto* aCalling = req.findAttribute(RadiusAttr_CallingStationId);
        const auto* aCalled = req.findAttribute(RadiusAttr_CalledStationId);
        const auto* aPort = req.findAttribute(RadiusAttr_NasPort);

        uint32_t statusType = aStatus ? aStatus->asUint32() : 1;
        std::string sessId = aSessId ? aSessId->asString() : ("sess_" + std::to_string(req.identifier));
        std::string uName = aUser ? aUser->asString() : "Anonymous";
        uint32_t inBytes = aInOctets ? aInOctets->asUint32() : 0;
        uint32_t outBytes = aOutOctets ? aOutOctets->asUint32() : 0;
        uint32_t sessTime = aSessTime ? aSessTime->asUint32() : 0;
        uint32_t termCause = aTermCause ? aTermCause->asUint32() : 0;

        uint64_t nowUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_accountingSessions.find(sessId);
            if (it == m_accountingSessions.end()) {
                AccountingSessionRecord rec{};
                rec.sessionId = sessId;
                rec.userName = uName;
                rec.clientIp = client.ipAddress;
                rec.callingStationId = aCalling ? aCalling->asString() : "";
                rec.calledStationId = aCalled ? aCalled->asString() : "";
                rec.nasPort = aPort ? aPort->asUint32() : 0;
                rec.statusType = statusType;
                rec.startTimeUs = nowUs;
                rec.lastUpdateTimeUs = nowUs;
                rec.inputOctets = inBytes;
                rec.outputOctets = outBytes;
                rec.sessionTimeSec = sessTime;
                rec.terminateCause = termCause;
                rec.active = (statusType != 2);
                m_accountingSessions[sessId] = rec;
            } else {
                it->second.lastUpdateTimeUs = nowUs;
                it->second.statusType = statusType;
                it->second.inputOctets = inBytes;
                it->second.outputOctets = outBytes;
                it->second.sessionTimeSec = sessTime;
                if (statusType == 2) {
                    it->second.stopTimeUs = nowUs;
                    it->second.terminateCause = termCause;
                    it->second.active = false;
                }
            }

            m_totalInputOctets.fetch_add(inBytes, std::memory_order_relaxed);
            m_totalOutputOctets.fetch_add(outBytes, std::memory_order_relaxed);

            std::ostringstream oss;
            oss << "[ACCT] " << ((statusType == 1) ? "START" : (statusType == 2) ? "STOP" : "INTERIM")
                << " User=" << uName << " Sess=" << sessId << " In=" << inBytes << " Out=" << outBytes
                << " Time=" << sessTime << "s Client=" << client.ipAddress;
            m_accountingAuditLog.push_back(oss.str());
            if (m_accountingAuditLog.size() > 500) m_accountingAuditLog.pop_front();
        }

        RadiusPacket resp;
        resp.code = RadiusCode_AccountingResponse;
        resp.identifier = req.identifier;

        auto rawResp = resp.serialize();
        CalculateResponseAuthenticator(resp.authenticator, resp.code, resp.identifier,
                                       static_cast<uint16_t>(rawResp.size()), req.authenticator,
                                       rawResp.data() + 20, rawResp.size() - 20, client.sharedSecret);
        outResponse = resp.serialize();
        return true;
    }

public:
    // Accounting Store Queries
    bool getAccountingSession(const std::string& sessId, AccountingSessionRecord* outRec) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_accountingSessions.find(sessId);
        if (it == m_accountingSessions.end()) return false;
        if (outRec) *outRec = it->second;
        return true;
    }

    std::vector<AccountingSessionRecord> getAllAccountingSessions() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<AccountingSessionRecord> res;
        for (const auto& [id, s] : m_accountingSessions) {
            res.push_back(s);
        }
        return res;
    }

    std::vector<std::string> getAuditLog() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<std::string>(m_accountingAuditLog.begin(), m_accountingAuditLog.end());
    }

    // Telemetry & Metrics
    uint64_t getTotalAuthRequests() const { return m_totalAuthRequests.load(std::memory_order_relaxed); }
    uint64_t getTotalAuthSuccesses() const { return m_totalAuthSuccesses.load(std::memory_order_relaxed); }
    uint64_t getTotalAuthRejections() const { return m_totalAuthRejections.load(std::memory_order_relaxed); }
    uint64_t getTotalAcctRequests() const { return m_totalAcctRequests.load(std::memory_order_relaxed); }
    uint64_t getTotalInputOctets() const { return m_totalInputOctets.load(std::memory_order_relaxed); }
    uint64_t getTotalOutputOctets() const { return m_totalOutputOctets.load(std::memory_order_relaxed); }
    uint64_t getLastAuthLatencyUs() const { return m_lastAuthLatencyUs.load(std::memory_order_relaxed); }

    // XML Configuration Export / Import
    std::string exportConfigXml() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ostringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
           << "<IasConfiguration Version=\"10.0.26100.1882\">\r\n"
           << "  <RadiusClients>\r\n";
        for (const auto& [ip, c] : m_clients) {
            ss << "    <Client Name=\"" << c.friendlyName << "\" Address=\"" << c.ipAddress
               << "\" Vendor=\"" << c.vendorName << "\" Enabled=\"" << (c.enabled ? "true" : "false")
               << "\" RequireMessageAuth=\"" << (c.requireMessageAuth ? "true" : "false") << "\"/>\r\n";
        }
        ss << "  </RadiusClients>\r\n"
           << "  <NetworkPolicies>\r\n";
        for (const auto& p : m_networkPolicies) {
            ss << "    <Policy Name=\"" << p.policyName << "\" Priority=\"" << p.priority
               << "\" Action=\"" << ((p.permission == PolicyPermission::GrantAccess) ? "Grant" : "Deny")
               << "\" PortType=\"" << p.allowedNasPortType << "\" VlanId=\"" << p.assignedVlanId
               << "\" SessionTimeout=\"" << p.sessionTimeoutSec << "\"/>\r\n";
        }
        ss << "  </NetworkPolicies>\r\n"
           << "</IasConfiguration>\r\n";
        return ss.str();
    }
};

// ============================================================================
// Clean-Room Win32 C ABI Parity Exports (`ias.dll`, `iasrad.dll`)
// ============================================================================
extern "C" {

inline int32_t WINAPI MicaIasInitialize(void** ppEngine) {
    if (!ppEngine) return 0;
    *ppEngine = &NetworkPolicyServer::instance();
    return 1;
}

inline int32_t WINAPI MicaIasRegisterClient(void* pEngine, const char* pClientIp, const char* pSecret, const char* pName) {
    if (!pClientIp || !pSecret || !pName) return 0;
    auto* srv = pEngine ? reinterpret_cast<NetworkPolicyServer*>(pEngine) : &NetworkPolicyServer::instance();

    RadiusClient c;
    c.ipAddress = pClientIp;
    c.sharedSecret = pSecret;
    c.friendlyName = pName;
    c.vendorName = "Standard RADIUS";
    return srv->registerClient(c) ? 1 : 0;
}

inline int32_t WINAPI MicaIasProcessPacket(void* pEngine, const char* pSourceIp, const uint8_t* pInPacket, uint32_t inLen,
                                          uint8_t* pOutPacket, uint32_t maxOutLen, uint32_t* pOutLen) {
    if (!pInPacket || !pOutPacket || !pOutLen || inLen < 20) return 0;
    auto* srv = pEngine ? reinterpret_cast<NetworkPolicyServer*>(pEngine) : &NetworkPolicyServer::instance();

    std::string srcIp = pSourceIp ? pSourceIp : "127.0.0.1";
    std::vector<uint8_t> resp;
    if (!srv->processPacket(srcIp, pInPacket, inLen, resp)) {
        return 0;
    }

    if (resp.size() > maxOutLen) return 0;
    std::memcpy(pOutPacket, resp.data(), resp.size());
    *pOutLen = static_cast<uint32_t>(resp.size());
    return 1;
}

inline int32_t WINAPI MicaIasVerifyAuthenticator(const uint8_t* pPacket, uint32_t length, const char* pSecret) {
    if (!pPacket || length < 20 || !pSecret) return 0;

    RadiusPacket pkt;
    if (!pkt.deserialize(pPacket, length)) return 0;

    if (pkt.code == RadiusCode_AccountingRequest) {
        uint8_t expectedAuth[16];
        CalculateAccountingRequestAuthenticator(expectedAuth, pkt.code, pkt.identifier, pkt.length,
                                                pPacket + 20, length - 20, pSecret);
        return (std::memcmp(pkt.authenticator, expectedAuth, 16) == 0) ? 1 : 0;
    }
    return 1;
}

inline int32_t WINAPI MicaIasGetAccountingStats(void* pEngine, uint64_t* pTotalAuth, uint64_t* pTotalAcct, uint64_t* pActiveSess) {
    auto* srv = pEngine ? reinterpret_cast<NetworkPolicyServer*>(pEngine) : &NetworkPolicyServer::instance();
    if (pTotalAuth) *pTotalAuth = srv->getTotalAuthRequests();
    if (pTotalAcct) *pTotalAcct = srv->getTotalAcctRequests();
    if (pActiveSess) {
        auto list = srv->getAllAccountingSessions();
        uint64_t active = 0;
        for (const auto& s : list) {
            if (s.active) active++;
        }
        *pActiveSess = active;
    }
    return 1;
}

inline void WINAPI MicaIasShutdown(void* pEngine) {
    (void)pEngine;
}

} // extern "C"

inline void RegisterNetworkPolicyServerSubsystem() {
    NetworkPolicyServer::instance();
}

} // namespace micant::nps

#endif // MICANT_NPS_HPP
