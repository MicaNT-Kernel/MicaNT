#pragma once

#include <cstdint>

// Minimal mode-neutral WinCrypt ABI needed by native TLS consumers. These are
// documented crypt32 exports; no Executive or kernel-private type is exposed.
namespace micant::native_crypt32 {

struct CertificateContext {
    uint32_t encodingType;
    const uint8_t* encoded;
    uint32_t encodedSize;
    const void* certificateInfo;
    void* store;
};

#if defined(MICANT_NATIVE_USERMODE) && defined(_WIN32)
#define MICANT_CRYPT32_IMPORT __declspec(dllimport)
#else
#define MICANT_CRYPT32_IMPORT
#endif

extern "C" {
MICANT_CRYPT32_IMPORT void* CertOpenSystemStoreW(uintptr_t provider, const wchar_t* storeName) noexcept;
MICANT_CRYPT32_IMPORT const CertificateContext* CertEnumCertificatesInStore(
    void* store, const CertificateContext* previous) noexcept;
MICANT_CRYPT32_IMPORT int32_t CertCloseStore(void* store, uint32_t flags) noexcept;
}

#undef MICANT_CRYPT32_IMPORT

} // namespace micant::native_crypt32
