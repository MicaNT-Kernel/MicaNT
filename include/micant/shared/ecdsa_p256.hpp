#pragma once

// Clean-room ECDSA P-256/SHA-256 verification, using public SEC 1 / FIPS 186
// arithmetic.  This verifies signatures only; no private scalar is processed.
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "micant/shared/crypto_primitives.hpp"

namespace micant::shared::crypto {
class EcdsaP256Sha256Verifier {
    using N = std::array<uint32_t, 8>;
    struct Point { N x{}, y{}, z{}; };
    static constexpr N P{0xffffffffu,0xffffffffu,0xffffffffu,0u,0u,0u,1u,0xffffffffu};
    static constexpr N Order{0xfc632551u,0xf3b9cac2u,0xa7179e84u,0xbce6faadu,
                             0xffffffffu,0xffffffffu,0u,0xffffffffu};
    static constexpr N CurveB{0x27d2604bu,0x3bce3c3eu,0xcc53b0f6u,0x651d06b0u,
                              0x769886bcu,0xb3ebbd55u,0xaa3a93e7u,0x5ac635d8u};
    static constexpr N Gx{0xd898c296u,0xf4a13945u,0x2deb33a0u,0x77037d81u,
                          0x63a440f2u,0xf8bce6e5u,0xe12c4247u,0x6b17d1f2u};
    static constexpr N Gy{0x37bf51f5u,0xcbb64068u,0x6b315eceu,0x2bce3357u,
                          0x7c0f9e16u,0x8ee7eb4au,0xfe1a7f9bu,0x4fe342e2u};
    static bool zero(const N& a) noexcept {
        uint32_t combined{}; for (auto word : a) combined |= word; return combined == 0;
    }
    static int compare(const N& a, const N& b) noexcept {
        for (size_t i = 8; i--;) { if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1; }
        return 0;
    }
    static N subtractRaw(const N& a, const N& b) noexcept {
        N result{}; uint64_t borrow{};
        for (size_t i = 0; i < 8; ++i) {
            const uint64_t rhs = static_cast<uint64_t>(b[i]) + borrow;
            result[i] = static_cast<uint32_t>(static_cast<uint64_t>(a[i]) - rhs);
            borrow = static_cast<uint64_t>(a[i]) < rhs;
        }
        return result;
    }
    static N add(const N& a, const N& b, const N& modulus) noexcept {
        N result{}; uint64_t carry{};
        for (size_t i = 0; i < 8; ++i) {
            const uint64_t sum = static_cast<uint64_t>(a[i]) + b[i] + carry;
            result[i] = static_cast<uint32_t>(sum); carry = sum >> 32;
        }
        if (carry || compare(result, modulus) >= 0) result = subtractRaw(result, modulus);
        return result;
    }
    static N sub(const N& a, const N& b, const N& modulus) noexcept {
        N result = subtractRaw(a, b);
        if (compare(a, b) < 0) {
            uint64_t carry{};
            for (size_t i = 0; i < 8; ++i) {
                const uint64_t sum = static_cast<uint64_t>(result[i]) + modulus[i] + carry;
                result[i] = static_cast<uint32_t>(sum); carry = sum >> 32;
            }
        }
        return result;
    }
    static N mul(const N& a, const N& b, const N& modulus) noexcept {
        N result{}, doubled = a;
        for (size_t i = 0; i < 256; ++i) {
            if ((b[i / 32] >> (i % 32)) & 1u) result = add(result, doubled, modulus);
            doubled = add(doubled, doubled, modulus);
        }
        return result;
    }
    static N inverse(const N& a, const N& modulus) noexcept {
        N exponent = modulus; exponent[0] -= 2;
        N result{}; result[0] = 1;
        for (size_t bit = 256; bit--;) {
            result = mul(result, result, modulus);
            if ((exponent[bit / 32] >> (bit % 32)) & 1u) result = mul(result, a, modulus);
        }
        return result;
    }
    static N twice(const N& a) noexcept { return add(a, a, P); }
    static N four(const N& a) noexcept { return twice(twice(a)); }
    static N eight(const N& a) noexcept { return twice(four(a)); }
    static Point dbl(const Point& p) noexcept {
        if (zero(p.z) || zero(p.y)) return {};
        const N delta = mul(p.z, p.z, P), gamma = mul(p.y, p.y, P);
        const N beta = mul(p.x, gamma, P);
        const N a = mul(sub(p.x, delta, P), add(p.x, delta, P), P);
        const N alpha = add(twice(a), a, P);
        Point r{};
        r.x = sub(mul(alpha, alpha, P), eight(beta), P);
        r.z = sub(sub(mul(add(p.y, p.z, P), add(p.y, p.z, P), P), gamma, P), delta, P);
        r.y = sub(mul(alpha, sub(four(beta), r.x, P), P), eight(mul(gamma, gamma, P)), P);
        return r;
    }
    static Point addAffine(const Point& p, const N& x, const N& y) noexcept {
        if (zero(p.z)) { Point r{x, y, {}}; r.z[0] = 1; return r; }
        const N zz = mul(p.z, p.z, P), u = mul(x, zz, P);
        const N s = mul(y, mul(p.z, zz, P), P);
        const N h = sub(u, p.x, P), d = sub(s, p.y, P);
        if (zero(h)) return zero(d) ? dbl(p) : Point{};
        const N hh = mul(h, h, P), i = four(hh), j = mul(h, i, P);
        const N rr = twice(d), v = mul(p.x, i, P);
        Point r{};
        r.x = sub(sub(mul(rr, rr, P), j, P), twice(v), P);
        r.y = sub(mul(rr, sub(v, r.x, P), P), twice(mul(p.y, j, P)), P);
        r.z = sub(sub(mul(add(p.z, h, P), add(p.z, h, P), P), zz, P), hh, P);
        return r;
    }
    static bool fromBytes(std::span<const uint8_t> bytes, N& out) noexcept {
        if (bytes.size() > 32) return false;
        out = {};
        for (size_t i = 0; i < bytes.size(); ++i)
            out[i / 4] |= static_cast<uint32_t>(bytes[bytes.size() - 1 - i]) << (8 * (i % 4));
        return true;
    }
    static bool derInteger(std::span<const uint8_t> der, size_t& at, N& value) noexcept {
        if (at > der.size() || der.size() - at < 2 || der[at++] != 0x02) return false;
        const size_t length = der[at++];
        if (!length || length > 33 || length > der.size() - at || (der[at] & 0x80)) return false;
        auto bytes = der.subspan(at, length); at += length;
        if (length > 1 && bytes[0] == 0) {
            if (!(bytes[1] & 0x80)) return false;
            bytes = bytes.subspan(1);
        }
        return fromBytes(bytes, value) && !zero(value) && compare(value, Order) < 0;
    }
public:
    static bool verify(std::span<const uint8_t> publicKey, std::span<const uint8_t> message,
                       std::span<const uint8_t> signature) noexcept {
        if (publicKey.size() != 65 || publicKey[0] != 4 || signature.size() < 8 ||
            signature.size() > 72 || signature[0] != 0x30 || signature[1] != signature.size() - 2)
            return false;
        N qx{}, qy{}, r{}, s{};
        if (!fromBytes(publicKey.subspan(1, 32), qx) || !fromBytes(publicKey.subspan(33, 32), qy) ||
            compare(qx, P) >= 0 || compare(qy, P) >= 0) return false;
        const N x2 = mul(qx, qx, P);
        const N threeX = add(twice(qx), qx, P);
        if (compare(mul(qy, qy, P), add(sub(mul(x2, qx, P), threeX, P), CurveB, P)) != 0)
            return false;
        size_t at = 2;
        if (!derInteger(signature, at, r) || !derInteger(signature, at, s) || at != signature.size()) return false;
        const auto digest = Sha256::hash(message);
        N z{}; (void)fromBytes(digest, z);
        if (compare(z, Order) >= 0) z = subtractRaw(z, Order);
        const N w = inverse(s, Order);
        const N u1 = mul(z, w, Order), u2 = mul(r, w, Order);
        Point result{};
        for (size_t bit = 256; bit--;) {
            result = dbl(result);
            if ((u1[bit / 32] >> (bit % 32)) & 1u) result = addAffine(result, Gx, Gy);
            if ((u2[bit / 32] >> (bit % 32)) & 1u) result = addAffine(result, qx, qy);
        }
        if (zero(result.z)) return false;
        const N inverseZ = inverse(result.z, P);
        N x = mul(result.x, mul(inverseZ, inverseZ, P), P);
        if (compare(x, Order) >= 0) x = subtractRaw(x, Order);
        return compare(x, r) == 0;
    }
};
} // namespace micant::shared::crypto
