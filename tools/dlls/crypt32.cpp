// Minimal native crypt32 system ROOT store. The on-disk store is an append-only
// sequence of DER certificates in the root of the mounted system volume; an
// absent store is empty. No built-in certificate is trusted.
#define MICANT_NATIVE_USERMODE 1
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "micant/shared/native_nt_api.hpp"
#include "micant/shared/native_crypt32_api.hpp"
#include "micant/shared/x509_profile.hpp"

namespace {
using namespace micant;
using micant::native_crypt32::CertificateContext;
constexpr uint32_t StoreMagic = 0x5254434d; // MCTR
constexpr uint32_t ContextMagic = 0x4354434d; // MCTC
constexpr size_t MaximumBundleBytes = 16 * 1024 * 1024;
constexpr size_t MaximumCertificateBytes = 64 * 1024;
constexpr size_t MaximumCertificates = 512;
constexpr size_t MaximumStores = 4;
constexpr size_t MaximumContexts = 8;
constexpr uint8_t BundleHeader[8] = {'M','C','R','O','O','T','1',0};

struct RootRecord { uint32_t offset; uint32_t length; };
struct StoreHandle {
    uint32_t magic;
    bool active;
    size_t nextIndex;
    CertificateContext enumerationContext;
    uint8_t enumerationDer[MaximumCertificateBytes];
};
struct OwnedCertificate {
    uint32_t magic;
    bool active;
    CertificateContext context;
    uint8_t der[MaximumCertificateBytes];
};
// Do not reserve the maximum store as static image data: the process managed
// VA window is itself 16 MiB. Grow only as needed for the actual bundle.
uint8_t* g_bundle{};
size_t g_bundleCapacity{};
uint8_t g_appendBuffer[MaximumCertificateBytes + sizeof(BundleHeader) + 4];
RootRecord g_records[MaximumCertificates];
size_t g_bundleLength{};
size_t g_recordCount{};
bool g_loaded{};
StoreHandle g_stores[MaximumStores]{};
OwnedCertificate g_contexts[MaximumContexts]{};
volatile uint32_t g_lock{};

// Store operations may perform native I/O and allocation while owning this
// state. A user APC can reenter Crypt32 on the same thread during a syscall;
// spinning there would deadlock that thread. Busy callers fail explicitly.
bool lock() noexcept { return __atomic_exchange_n(&g_lock, 1, __ATOMIC_ACQUIRE) == 0; }
void unlock() noexcept { __atomic_store_n(&g_lock, 0, __ATOMIC_RELEASE); }

bool ensureBundleCapacity(size_t required, size_t preserved) noexcept {
    if (required <= g_bundleCapacity) return true;
    if (required > MaximumBundleBytes || preserved > g_bundleCapacity) return false;
    size_t capacity = g_bundleCapacity ? g_bundleCapacity : 64 * 1024;
    while (capacity < required) {
        if (capacity > MaximumBundleBytes / 2) { capacity = MaximumBundleBytes; break; }
        capacity *= 2;
    }
    void* allocation{};
    size_t region = capacity;
    const auto status = native_nt::NtAllocateVirtualMemory(
        static_cast<Handle>(-1), &allocation, 0, &region, 0x3000, 0x04);
    if (status != NtStatus::Success || !allocation) return false;
    if (region < required) {
        size_t cleanupSize{};
        (void)native_nt::NtFreeVirtualMemory(static_cast<Handle>(-1),
                                             &allocation, &cleanupSize, 0x8000);
        return false;
    }
    if (preserved) std::memcpy(allocation, g_bundle, preserved);
    if (g_bundle) {
        void* old = g_bundle;
        size_t releaseSize{};
        if (native_nt::NtFreeVirtualMemory(static_cast<Handle>(-1),
                                            &old, &releaseSize, 0x8000) != NtStatus::Success) {
            size_t cleanupSize{};
            (void)native_nt::NtFreeVirtualMemory(static_cast<Handle>(-1),
                                                 &allocation, &cleanupSize, 0x8000);
            return false;
        }
    }
    g_bundle = static_cast<uint8_t*>(allocation);
    g_bundleCapacity = region;
    return true;
}

void setUnicode(UnicodeString& name, const wchar_t* path) noexcept {
    name.buffer = path;
    size_t count = 0;
    while (path[count]) ++count;
    name.length = static_cast<uint16_t>(count * sizeof(wchar_t));
    name.maximumLength = static_cast<uint16_t>((count + 1) * sizeof(wchar_t));
}

NtStatus openBundle(Handle& file, bool write) noexcept {
    // FsRtl publishes the selected installation root through the standard
    // read-only shared page. A missing/unmounted system volume is not an
    // empty, trusted store; report it to the caller explicitly.
    const auto* shared = reinterpret_cast<const volatile KUserSharedData*>(UserSharedDataAddress);
    char16_t drive = __atomic_load_n(&shared->ntSystemRoot[0], __ATOMIC_ACQUIRE);
    if (drive >= u'a' && drive <= u'z') drive = static_cast<char16_t>(drive - (u'a' - u'A'));
    if (drive < u'A' || drive > u'Z' || shared->ntSystemRoot[1] != u':' ||
        shared->ntSystemRoot[2] != u'\\') return NtStatus::DeviceNotReady;
    wchar_t path[] = L"X:\\ROOT.DER";
    path[0] = static_cast<wchar_t>(drive);
    UnicodeString name{}; setUnicode(name, path);
    ObjectAttributes attributes{};
    attributes.objectName = &name;
    attributes.attributes = OBJ_CASE_INSENSITIVE;
    IoStatusBlock iosb{};
    const uint32_t access = (write ? 0x00000003u : 0x00000001u) | 0x00100000u;
    return native_nt::NtCreateFile(&file, access, &attributes, &iosb, nullptr, 0,
        0x00000003u, write ? 3u : 1u, 0x00000020u, nullptr, 0);
}

bool readBundle() noexcept {
    g_bundleLength = g_recordCount = 0;
    Handle file{};
    NtStatus status = openBundle(file, false);
    if (status == NtStatus::ObjectNameNotFound || status == NtStatus::ObjectPathNotFound) {
        g_loaded = true; return true;
    }
    if (status != NtStatus::Success || !file) return false;
    size_t offset = 0;
    while (offset < MaximumBundleBytes) {
        LargeInteger byteOffset{}; byteOffset.QuadPart = static_cast<int64_t>(offset);
        IoStatusBlock iosb{};
        // NtReadFile stages data in kernel memory; bound each request instead
        // of allocating the entire 16 MiB maximum for a small trust store.
        constexpr size_t ReadChunk = 64 * 1024;
        const size_t remaining = MaximumBundleBytes - offset;
        const uint32_t request = static_cast<uint32_t>(remaining < ReadChunk ? remaining : ReadChunk);
        if (!ensureBundleCapacity(offset + request, offset)) {
            native_nt::NtClose(file);
            return false;
        }
        status = native_nt::NtReadFile(file, 0, nullptr, nullptr, &iosb,
            g_bundle + offset, request, &byteOffset, nullptr);
        if (status == NtStatus::EndOfFile) {
            if (iosb.Information > request) { native_nt::NtClose(file); return false; }
            offset += iosb.Information;
            break;
        }
        if (status != NtStatus::Success || iosb.Information > request) {
            native_nt::NtClose(file); return false;
        }
        if (iosb.Information == 0) break;
        offset += iosb.Information;
    }
    native_nt::NtClose(file);
    // A previous append can create the file and then fail before writing its
    // first complete record. Treat a genuinely empty file as an empty trust
    // store so that an administrator can retry provisioning it. A non-empty
    // malformed bundle still fails closed below.
    if (offset == 0) {
        g_bundleLength = 0;
        g_recordCount = 0;
        g_loaded = true;
        return true;
    }
    if (offset == MaximumBundleBytes || offset < sizeof(BundleHeader) ||
        std::memcmp(g_bundle, BundleHeader, sizeof(BundleHeader)) != 0) return false;
    size_t cursor = sizeof(BundleHeader);
    while (cursor < offset) {
        if (offset - cursor < 4 || g_recordCount == MaximumCertificates) return false;
        const uint32_t length = static_cast<uint32_t>(g_bundle[cursor]) |
            (static_cast<uint32_t>(g_bundle[cursor + 1]) << 8) |
            (static_cast<uint32_t>(g_bundle[cursor + 2]) << 16) |
            (static_cast<uint32_t>(g_bundle[cursor + 3]) << 24);
        cursor += 4;
        if (!length || length > MaximumCertificateBytes || length > offset - cursor) return false;
        g_records[g_recordCount++] = {static_cast<uint32_t>(cursor), length};
        cursor += length;
    }
    g_bundleLength = offset;
    g_loaded = true;
    return true;
}

bool isRootStore(const wchar_t* name) noexcept {
    if (!name) return false;
    constexpr wchar_t root[] = L"ROOT";
    for (size_t i = 0; i < 5; ++i) {
        wchar_t c = name[i];
        if (c >= L'a' && c <= L'z') c -= 32;
        if (c != root[i]) return false;
        if (!c) return true;
    }
    return name[4] == 0;
}

StoreHandle* findStore(void* handle) noexcept {
    auto* candidate = static_cast<StoreHandle*>(handle);
    for (auto& store : g_stores)
        if (&store == candidate && store.active && store.magic == StoreMagic) return &store;
    return nullptr;
}
OwnedCertificate* findCertificate(const CertificateContext* context) noexcept {
    for (auto& cert : g_contexts)
        if (&cert.context == context && cert.active && cert.magic == ContextMagic) return &cert;
    return nullptr;
}
OwnedCertificate* allocateCertificate(const uint8_t* der, uint32_t length) noexcept {
    if (!der || !length || length > MaximumCertificateBytes) return nullptr;
    for (auto& cert : g_contexts) {
        if (cert.active) continue;
        std::memcpy(cert.der, der, length);
        cert.magic = ContextMagic;
        cert.active = true;
        cert.context = {1, cert.der, length, nullptr, nullptr};
        return &cert;
    }
    return nullptr;
}

bool appendRoot(const uint8_t* der, uint32_t length) noexcept {
    if (!der || !length || length > MaximumCertificateBytes || g_recordCount >= MaximumCertificates)
        return false;
    if (!g_loaded && !readBundle()) return false;
    for (size_t i = 0; i < g_recordCount; ++i) {
        const auto& record = g_records[i];
        if (record.length == length && std::memcmp(g_bundle + record.offset, der, length) == 0)
            return false;
    }
    const bool newFile = g_bundleLength == 0;
    const size_t prefix = newFile ? sizeof(BundleHeader) : 0;
    if (g_bundleLength > MaximumBundleBytes - prefix - 4 - length) return false;
    Handle file{};
    const NtStatus status = openBundle(file, true);
    if (status != NtStatus::Success || !file) return false;
    size_t recordBytes{};
    if (newFile) {
        std::memcpy(g_appendBuffer, BundleHeader, sizeof(BundleHeader));
        recordBytes = sizeof(BundleHeader);
    }
    uint8_t lengthBytes[4] = {static_cast<uint8_t>(length), static_cast<uint8_t>(length >> 8),
        static_cast<uint8_t>(length >> 16), static_cast<uint8_t>(length >> 24)};
    std::memcpy(g_appendBuffer + recordBytes, lengthBytes, sizeof(lengthBytes));
    std::memcpy(g_appendBuffer + recordBytes + sizeof(lengthBytes), der, length);
    recordBytes += sizeof(lengthBytes) + length;
    const size_t offset = newFile ? 0 : g_bundleLength;
    LargeInteger byteOffset{}; byteOffset.QuadPart = static_cast<int64_t>(offset);
    IoStatusBlock iosb{};
    const NtStatus writeStatus = native_nt::NtWriteFile(file, 0, nullptr, nullptr, &iosb,
        g_appendBuffer, static_cast<uint32_t>(recordBytes), &byteOffset, nullptr);
    native_nt::NtClose(file);
    if (writeStatus != NtStatus::Success || iosb.Information != recordBytes) {
        std::memset(g_appendBuffer, 0, recordBytes);
        g_loaded = false;
        return false;
    }
    std::memset(g_appendBuffer, 0, recordBytes);
    g_loaded = false;
    return readBundle();
}
}

extern "C" {
__declspec(dllexport) void* CertOpenSystemStoreW(uintptr_t, const wchar_t* storeName) noexcept {
    if (!isRootStore(storeName)) return nullptr;
    if (!lock()) return nullptr;
    if (!readBundle()) { unlock(); return nullptr; }
    for (auto& store : g_stores) {
        if (!store.active) {
            store.magic = StoreMagic; store.active = true; store.nextIndex = 0;
            store.enumerationContext = {};
            store.enumerationContext.encodingType = 1;
            store.enumerationContext.store = &store;
            unlock(); return &store;
        }
    }
    unlock(); return nullptr;
}

__declspec(dllexport) const CertificateContext* CertEnumCertificatesInStore(
    void* handle, const CertificateContext* previous) noexcept {
    if (!lock()) return nullptr;
    StoreHandle* store = findStore(handle);
    if (!store || (previous && previous != &store->enumerationContext)) { unlock(); return nullptr; }
    if (previous) ++store->nextIndex;
    if (store->nextIndex >= g_recordCount) { unlock(); return nullptr; }
    const auto& record = g_records[store->nextIndex];
    std::memcpy(store->enumerationDer, g_bundle + record.offset, record.length);
    store->enumerationContext.encoded = store->enumerationDer;
    store->enumerationContext.encodedSize = record.length;
    unlock(); return &store->enumerationContext;
}

__declspec(dllexport) int32_t CertCloseStore(void* handle, uint32_t) noexcept {
    if (!lock()) return 0;
    StoreHandle* store = findStore(handle);
    if (!store) { unlock(); return 0; }
    store->active = false; store->magic = 0; store->nextIndex = 0;
    std::memset(store->enumerationDer, 0, sizeof(store->enumerationDer));
    unlock(); return 1;
}

__declspec(dllexport) const CertificateContext* CertCreateCertificateContext(
    uint32_t encoding, const uint8_t* der, uint32_t length) noexcept {
    if (encoding != 1 || !der || !length || length > MaximumCertificateBytes) return nullptr;
    micant::shared::BoundedVector<uint8_t> snapshot(der, der + length);
    if (!snapshot.good() || snapshot.size() != length) return nullptr;
    micant::shared::crypto::X509Certificate parsed{};
    if (!micant::shared::crypto::X509Certificate::parseDer(snapshot, parsed)) return nullptr;
    if (!lock()) return nullptr;
    auto* cert = allocateCertificate(snapshot.data(), length); unlock();
    return cert ? &cert->context : nullptr;
}

__declspec(dllexport) int32_t CertFreeCertificateContext(const CertificateContext* context) noexcept {
    if (!lock()) return 0;
    auto* cert = findCertificate(context);
    if (cert) { std::memset(cert->der, 0, cert->context.encodedSize); cert->active = false; cert->magic = 0; }
    unlock(); return cert != nullptr;
}

__declspec(dllexport) int32_t CertAddCertificateContextToStore(
    void* handle, const CertificateContext* context, uint32_t disposition,
    const CertificateContext** storedContext) noexcept {
    if (storedContext) *storedContext = nullptr;
    if (disposition != 1 || !context) return 0;
    const auto* encoded = context->encoded;
    const uint32_t encodedSize = context->encodedSize;
    if (!encoded || !encodedSize || encodedSize > MaximumCertificateBytes) return 0;
    // The caller owns the input context and can change it concurrently. The
    // bytes persisted as a trust anchor must be the same bytes we parsed.
    micant::shared::BoundedVector<uint8_t> snapshot(
        encoded, encoded + encodedSize);
    if (!snapshot.good() || snapshot.size() != encodedSize) return 0;
    micant::shared::crypto::X509Certificate parsed{};
    if (!micant::shared::crypto::X509Certificate::parseDer(snapshot, parsed) || !parsed.isCertificateAuthority)
        return 0;
    if (!lock()) return 0;
    if (!findStore(handle)) { unlock(); return 0; }
    // Reserve the optional returned context before committing the persistent
    // append. Otherwise an allocation failure could report failure after the
    // certificate has already become trusted.
    OwnedCertificate* copy = storedContext ? allocateCertificate(snapshot.data(), static_cast<uint32_t>(snapshot.size())) : nullptr;
    if (storedContext && !copy) { unlock(); return 0; }
    const bool added = appendRoot(snapshot.data(), static_cast<uint32_t>(snapshot.size()));
    if (!added && copy) {
        std::memset(copy->der, 0, copy->context.encodedSize);
        copy->active = false;
        copy->magic = 0;
        copy = nullptr;
    }
    if (copy) { copy->context.store = handle; *storedContext = &copy->context; }
    unlock(); return added;
}

int DllMain(void*, uint32_t reason, void*) noexcept {
    if (reason == 0 && g_bundle) { // DLL_PROCESS_DETACH
        void* allocation = g_bundle;
        size_t releaseSize{};
        if (native_nt::NtFreeVirtualMemory(static_cast<Handle>(-1),
                                           &allocation, &releaseSize, 0x8000) == NtStatus::Success) {
            g_bundle = nullptr;
            g_bundleCapacity = g_bundleLength = g_recordCount = 0;
            g_loaded = false;
        }
    }
    return 1;
}
}
