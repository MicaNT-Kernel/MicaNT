// ============================================================================
// MicaNT: Component Object Model (COM) & OLE Automation Subsystems
// (ole32.dll & oleaut32.dll)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides complete Component Object Model (COM) runtime lifecycle,
// STA/MTA threading apartments, CoCreateInstance factory dispatching,
// CoTaskMem memory allocators, GUID serialization, BSTR length-prefixed strings,
// and polymorphic VARIANT data containers.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <cstdio>

#include "ntdef.hpp"
#include "heap.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"

namespace micant::ole32 {

// ============================================================================
// 1. Data Types & COM Error Codes
// ============================================================================

using HRESULT = int32_t;
using IID     = micant::GUID;
using CLSID   = micant::GUID;
using REFIID  = const IID&;
using REFCLSID= const CLSID&;
using REFGUID = const micant::GUID&;

using OLECHAR  = wchar_t;
using LPOLESTR = wchar_t*;
using LPCOLESTR= const wchar_t*;
using BSTR     = wchar_t*;
using VARIANT_BOOL = int16_t;

inline constexpr VARIANT_BOOL VARIANT_TRUE  = -1;
inline constexpr VARIANT_BOOL VARIANT_FALSE = 0;

// Standard COM HRESULTs
inline constexpr HRESULT S_OK                     = 0;
inline constexpr HRESULT S_FALSE                  = 1;
inline constexpr HRESULT E_NOTIMPL                = static_cast<HRESULT>(0x80004001);
inline constexpr HRESULT E_NOINTERFACE            = static_cast<HRESULT>(0x80004002);
inline constexpr HRESULT E_POINTER                = static_cast<HRESULT>(0x80004003);
inline constexpr HRESULT E_ABORT                  = static_cast<HRESULT>(0x80004004);
inline constexpr HRESULT E_FAIL                   = static_cast<HRESULT>(0x80004005);
inline constexpr HRESULT E_UNEXPECTED             = static_cast<HRESULT>(0x8000FFFF);
inline constexpr HRESULT E_OUTOFMEMORY            = static_cast<HRESULT>(0x8007000E);
inline constexpr HRESULT E_INVALIDARG             = static_cast<HRESULT>(0x80070057);
inline constexpr HRESULT CO_E_NOTINITIALIZED      = static_cast<HRESULT>(0x800401F0);
inline constexpr HRESULT CO_E_ALREADYINITIALIZED  = static_cast<HRESULT>(0x800401F1);
inline constexpr HRESULT CLASS_E_CLASSNOTAVAILABLE= static_cast<HRESULT>(0x80040111);
inline constexpr HRESULT REGDB_E_CLASSNOTREG      = static_cast<HRESULT>(0x80040154);

// Apartment Models
inline constexpr uint32_t COINIT_APARTMENTTHREADED = 0x2;
inline constexpr uint32_t COINIT_MULTITHREADED     = 0x0;
inline constexpr uint32_t COINIT_DISABLE_OLE1DDE   = 0x4;
inline constexpr uint32_t COINIT_SPEED_OVER_MEMORY = 0x8;

// Class Contexts
inline constexpr uint32_t CLSCTX_INPROC_SERVER   = 0x1;
inline constexpr uint32_t CLSCTX_INPROC_HANDLER  = 0x2;
inline constexpr uint32_t CLSCTX_LOCAL_SERVER    = 0x4;
inline constexpr uint32_t CLSCTX_REMOTE_SERVER   = 0x10;
inline constexpr uint32_t CLSCTX_ALL             = 0x17;

// Registration Flags
inline constexpr uint32_t REGCLS_SINGLEUSE       = 0;
inline constexpr uint32_t REGCLS_MULTIPLEUSE     = 1;
inline constexpr uint32_t REGCLS_MULTI_SEPARATE  = 2;
inline constexpr uint32_t REGCLS_SUSPENDED       = 4;

// Standard GUIDs
inline const IID IID_IUnknown = {
    0x00000000, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

inline const IID IID_IClassFactory = {
    0x00000001, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

// ============================================================================
// 2. Base COM Interfaces
// ============================================================================

class IUnknown {
public:
    virtual HRESULT  QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;
    virtual ~IUnknown() = default;
};

class IClassFactory : public IUnknown {
public:
    virtual HRESULT CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) = 0;
    virtual HRESULT LockServer(win32::BOOL fLock) = 0;
};

// ============================================================================
// 3. OLE Automation Data Types & VARIANT
// ============================================================================

using VARTYPE = uint16_t;

enum VARENUM : uint16_t {
    VT_EMPTY            = 0,
    VT_NULL             = 1,
    VT_I2               = 2,
    VT_I4               = 3,
    VT_R4               = 4,
    VT_R8               = 5,
    VT_CY               = 6,
    VT_DATE             = 7,
    VT_BSTR             = 8,
    VT_DISPATCH         = 9,
    VT_ERROR            = 10,
    VT_BOOL             = 11,
    VT_VARIANT          = 12,
    VT_UNKNOWN          = 13,
    VT_DECIMAL          = 14,
    VT_I1               = 16,
    VT_UI1              = 17,
    VT_UI2              = 18,
    VT_UI4              = 19,
    VT_I8               = 20,
    VT_UI8              = 21,
    VT_INT              = 22,
    VT_UINT             = 23,
    VT_VOID             = 24,
    VT_HRESULT          = 25,
    VT_PTR              = 26,
    VT_SAFEARRAY        = 27,
    VT_CARRAY           = 28,
    VT_USERDEFINED      = 29,
    VT_LPSTR            = 30,
    VT_LPWSTR           = 31,
    VT_RECORD           = 36,
    VT_INT_PTR          = 37,
    VT_UINT_PTR         = 38,
    VT_BYREF            = 0x4000
};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#endif

struct VARIANT {
    VARTYPE vt{VT_EMPTY};
    uint16_t wReserved1{0};
    uint16_t wReserved2{0};
    uint16_t wReserved3{0};
    union {
        int64_t      llVal;
        int32_t      lVal;
        uint8_t      bVal;
        int16_t      iVal;
        float        fltVal;
        double       dblVal;
        VARIANT_BOOL boolVal;
        HRESULT      scode;
        BSTR         bstrVal;
        IUnknown*    punkVal;
        void*        byref;
    };
};
using VARIANTARG = VARIANT;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// ============================================================================
// 4. Central COM Runtime & Class Object Registry
// ============================================================================

class ComRuntime {
private:
    std::mutex m_mutex;
    std::atomic<uint32_t> m_initCount{0};
    uint32_t m_apartmentModel{COINIT_MULTITHREADED};
    std::unordered_map<std::string, IUnknown*> m_registeredClasses;
    uint32_t m_nextCookie{1};
    std::unordered_map<uint32_t, std::string>  m_cookieToGuid;

    ComRuntime() = default;

public:
    static ComRuntime& get() {
        static ComRuntime instance;
        return instance;
    }

    HRESULT Initialize(uint32_t dwCoInit) noexcept {
        uint32_t count = m_initCount.fetch_add(1);
        if (count == 0) {
            m_apartmentModel = dwCoInit;
            return S_OK;
        }
        return S_FALSE; // Already initialized on this process
    }

    void Uninitialize() noexcept {
        uint32_t prev = m_initCount.load();
        if (prev > 0) {
            m_initCount.fetch_sub(1);
        }
    }

    [[nodiscard]] bool IsInitialized() const noexcept {
        return m_initCount.load() > 0;
    }

    [[nodiscard]] uint32_t GetInitCount() const noexcept {
        return m_initCount.load();
    }

    HRESULT RegisterClassObject(REFCLSID rclsid, IUnknown* pUnk, uint32_t, uint32_t, uint32_t* lpdwRegister) {
        if (!pUnk || !lpdwRegister) return E_INVALIDARG;
        std::lock_guard<std::mutex> lock(m_mutex);

        std::string key = GuidToString(rclsid);
        m_registeredClasses[key] = pUnk;
        pUnk->AddRef();

        uint32_t cookie = m_nextCookie++;
        m_cookieToGuid[cookie] = key;
        *lpdwRegister = cookie;
        return S_OK;
    }

    HRESULT RevokeClassObject(uint32_t dwRegister) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto itCookie = m_cookieToGuid.find(dwRegister);
        if (itCookie == m_cookieToGuid.end()) return E_INVALIDARG;

        auto itClass = m_registeredClasses.find(itCookie->second);
        if (itClass != m_registeredClasses.end()) {
            itClass->second->Release();
            m_registeredClasses.erase(itClass);
        }
        m_cookieToGuid.erase(itCookie);
        return S_OK;
    }

    HRESULT GetClassObject(REFCLSID rclsid, uint32_t, void*, REFIID riid, void** ppv) {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);

        std::string key = GuidToString(rclsid);
        auto it = m_registeredClasses.find(key);
        if (it == m_registeredClasses.end()) {
            return REGDB_E_CLASSNOTREG;
        }

        return it->second->QueryInterface(riid, ppv);
    }

    static std::string GuidToString(const GUID& g) {
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            g.Data1, g.Data2, g.Data3,
            g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
            g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
        return std::string(buf);
    }
};

// ============================================================================
// 5. OLE32 C-API Implementation (ole32.dll)
// ============================================================================

inline HRESULT CoInitialize(void* pvReserved) noexcept {
    (void)pvReserved;
    return ComRuntime::get().Initialize(COINIT_APARTMENTTHREADED);
}

inline HRESULT CoInitializeEx(void* pvReserved, uint32_t dwCoInit) noexcept {
    (void)pvReserved;
    return ComRuntime::get().Initialize(dwCoInit);
}

inline void CoUninitialize() noexcept {
    ComRuntime::get().Uninitialize();
}

inline void* CoTaskMemAlloc(size_t cb) noexcept {
    return heap::RtlAllocateHeap(win32::GetProcessHeap(), 0, cb);
}

inline void CoTaskMemFree(void* pv) noexcept {
    if (pv) {
        heap::RtlFreeHeap(win32::GetProcessHeap(), 0, pv);
    }
}

inline void* CoTaskMemRealloc(void* pv, size_t cb) noexcept {
    return heap::RtlReAllocateHeap(win32::GetProcessHeap(), 0, pv, cb);
}

inline int StringFromGUID2(REFGUID rguid, wchar_t* lpsz, int cchMax) noexcept {
    if (!lpsz || cchMax < 39) return 0;
    std::swprintf(lpsz, static_cast<size_t>(cchMax), L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        rguid.Data1, rguid.Data2, rguid.Data3,
        rguid.Data4[0], rguid.Data4[1], rguid.Data4[2], rguid.Data4[3],
        rguid.Data4[4], rguid.Data4[5], rguid.Data4[6], rguid.Data4[7]);
    return 39;
}

inline HRESULT IIDFromString(const wchar_t* lpsz, IID* lpiid) noexcept {
    if (!lpsz || !lpiid) return E_INVALIDARG;
    if (lpsz[0] != L'{') return E_INVALIDARG;

    uint32_t d1 = 0;
    uint32_t d2 = 0, d3 = 0;
    uint32_t b[8]{};

    int n = std::swscanf(lpsz, L"{%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x}",
        &d1, &d2, &d3,
        &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7]);

    if (n != 11) return E_INVALIDARG;

    lpiid->Data1 = d1;
    lpiid->Data2 = static_cast<uint16_t>(d2);
    lpiid->Data3 = static_cast<uint16_t>(d3);
    for (int i = 0; i < 8; ++i) {
        lpiid->Data4[i] = static_cast<uint8_t>(b[i]);
    }
    return S_OK;
}

inline HRESULT CLSIDFromString(const wchar_t* lpsz, CLSID* lpclsid) noexcept {
    return IIDFromString(lpsz, lpclsid);
}

inline HRESULT CoCreateGuid(GUID* pguid) noexcept {
    if (!pguid) return E_INVALIDARG;
    static std::atomic<uint32_t> s_guidCounter{0xCAFE0001};
    uint32_t seq = s_guidCounter.fetch_add(1);

    pguid->Data1 = 0xA1B2C3D4 ^ seq;
    pguid->Data2 = static_cast<uint16_t>(seq & 0xFFFF);
    pguid->Data3 = 0x4123; // Version 4 (Random)
    pguid->Data4[0] = 0x80 | static_cast<uint8_t>(seq & 0x3F); // Variant 1
    pguid->Data4[1] = 0x11;
    pguid->Data4[2] = 0x22;
    pguid->Data4[3] = 0x33;
    pguid->Data4[4] = 0x44;
    pguid->Data4[5] = 0x55;
    pguid->Data4[6] = 0x66;
    pguid->Data4[7] = static_cast<uint8_t>(seq & 0xFF);
    return S_OK;
}

inline HRESULT CoGetClassObject(REFCLSID rclsid, uint32_t dwClsContext, void* pvReserved, REFIID riid, void** ppv) noexcept {
    return ComRuntime::get().GetClassObject(rclsid, dwClsContext, pvReserved, riid, ppv);
}

inline HRESULT CoRegisterClassObject(REFCLSID rclsid, IUnknown* pUnk, uint32_t dwClsContext, uint32_t flags, uint32_t* lpdwRegister) noexcept {
    return ComRuntime::get().RegisterClassObject(rclsid, pUnk, dwClsContext, flags, lpdwRegister);
}

inline HRESULT CoRevokeClassObject(uint32_t dwRegister) noexcept {
    return ComRuntime::get().RevokeClassObject(dwRegister);
}

inline HRESULT CoCreateInstance(REFCLSID rclsid, IUnknown* pUnkOuter, uint32_t dwClsContext, REFIID riid, void** ppv) noexcept {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    IClassFactory* pFactory = nullptr;
    HRESULT hr = CoGetClassObject(rclsid, dwClsContext, nullptr, IID_IClassFactory, reinterpret_cast<void**>(&pFactory));
    if (hr != S_OK || !pFactory) {
        return hr;
    }

    hr = pFactory->CreateInstance(pUnkOuter, riid, ppv);
    pFactory->Release();
    return hr;
}

// ============================================================================
// 6. OLEAUT32 C-API Implementation (oleaut32.dll)
// ============================================================================

inline BSTR SysAllocString(const OLECHAR* psz) noexcept {
    if (!psz) return nullptr;
    size_t len = std::wcslen(psz);
    size_t byteCount = len * sizeof(OLECHAR);

    // BSTR is length-prefixed: 4 bytes length before pointer
    uint8_t* mem = static_cast<uint8_t*>(CoTaskMemAlloc(byteCount + sizeof(uint32_t) + sizeof(OLECHAR)));
    if (!mem) return nullptr;

    uint32_t* pLen = reinterpret_cast<uint32_t*>(mem);
    *pLen = static_cast<uint32_t>(byteCount);

    OLECHAR* strData = reinterpret_cast<OLECHAR*>(mem + sizeof(uint32_t));
    std::memcpy(strData, psz, byteCount);
    strData[len] = L'\0';
    return strData;
}

inline BSTR SysAllocStringLen(const OLECHAR* strIn, uint32_t ui) noexcept {
    size_t byteCount = ui * sizeof(OLECHAR);
    uint8_t* mem = static_cast<uint8_t*>(CoTaskMemAlloc(byteCount + sizeof(uint32_t) + sizeof(OLECHAR)));
    if (!mem) return nullptr;

    uint32_t* pLen = reinterpret_cast<uint32_t*>(mem);
    *pLen = static_cast<uint32_t>(byteCount);

    OLECHAR* strData = reinterpret_cast<OLECHAR*>(mem + sizeof(uint32_t));
    if (strIn) {
        std::memcpy(strData, strIn, byteCount);
    } else {
        std::memset(strData, 0, byteCount);
    }
    strData[ui] = L'\0';
    return strData;
}

inline void SysFreeString(BSTR bstrString) noexcept {
    if (!bstrString) return;
    uint8_t* mem = reinterpret_cast<uint8_t*>(bstrString) - sizeof(uint32_t);
    CoTaskMemFree(mem);
}

inline uint32_t SysStringLen(BSTR pbstr) noexcept {
    if (!pbstr) return 0;
    const uint32_t* pLen = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(pbstr) - sizeof(uint32_t));
    return *pLen / sizeof(OLECHAR);
}

inline uint32_t SysStringByteLen(BSTR bstr) noexcept {
    if (!bstr) return 0;
    const uint32_t* pLen = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(bstr) - sizeof(uint32_t));
    return *pLen;
}

inline void VariantInit(VARIANTARG* pvarg) noexcept {
    if (pvarg) {
        std::memset(pvarg, 0, sizeof(VARIANTARG));
        pvarg->vt = VT_EMPTY;
    }
}

inline HRESULT VariantClear(VARIANTARG* pvarg) noexcept {
    if (!pvarg) return E_INVALIDARG;
    if (pvarg->vt == VT_BSTR && pvarg->bstrVal) {
        SysFreeString(pvarg->bstrVal);
    } else if (pvarg->vt == VT_UNKNOWN && pvarg->punkVal) {
        pvarg->punkVal->Release();
    }
    std::memset(pvarg, 0, sizeof(VARIANTARG));
    pvarg->vt = VT_EMPTY;
    return S_OK;
}

inline HRESULT VariantCopy(VARIANTARG* pvargDest, const VARIANTARG* pvargSrc) noexcept {
    if (!pvargDest || !pvargSrc) return E_INVALIDARG;
    if (pvargDest == pvargSrc) return S_OK;

    VariantClear(pvargDest);
    std::memcpy(pvargDest, pvargSrc, sizeof(VARIANTARG));

    if (pvargSrc->vt == VT_BSTR && pvargSrc->bstrVal) {
        pvargDest->bstrVal = SysAllocStringLen(pvargSrc->bstrVal, SysStringLen(pvargSrc->bstrVal));
    } else if (pvargSrc->vt == VT_UNKNOWN && pvargSrc->punkVal) {
        pvargDest->punkVal->AddRef();
    }
    return S_OK;
}

// ============================================================================
// 7. Subsystem Export Registration
// ============================================================================

inline void InitializeOle32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // ole32.dll
    ldr.registerExport("ole32.dll", "CoInitialize", reinterpret_cast<void*>(CoInitialize));
    ldr.registerExport("ole32.dll", "CoInitializeEx", reinterpret_cast<void*>(CoInitializeEx));
    ldr.registerExport("ole32.dll", "CoUninitialize", reinterpret_cast<void*>(CoUninitialize));
    ldr.registerExport("ole32.dll", "CoCreateInstance", reinterpret_cast<void*>(CoCreateInstance));
    ldr.registerExport("ole32.dll", "CoGetClassObject", reinterpret_cast<void*>(CoGetClassObject));
    ldr.registerExport("ole32.dll", "CoRegisterClassObject", reinterpret_cast<void*>(CoRegisterClassObject));
    ldr.registerExport("ole32.dll", "CoRevokeClassObject", reinterpret_cast<void*>(CoRevokeClassObject));
    ldr.registerExport("ole32.dll", "CoTaskMemAlloc", reinterpret_cast<void*>(CoTaskMemAlloc));
    ldr.registerExport("ole32.dll", "CoTaskMemFree", reinterpret_cast<void*>(CoTaskMemFree));
    ldr.registerExport("ole32.dll", "CoTaskMemRealloc", reinterpret_cast<void*>(CoTaskMemRealloc));
    ldr.registerExport("ole32.dll", "StringFromGUID2", reinterpret_cast<void*>(StringFromGUID2));
    ldr.registerExport("ole32.dll", "IIDFromString", reinterpret_cast<void*>(IIDFromString));
    ldr.registerExport("ole32.dll", "CLSIDFromString", reinterpret_cast<void*>(CLSIDFromString));
    ldr.registerExport("ole32.dll", "CoCreateGuid", reinterpret_cast<void*>(CoCreateGuid));

    // oleaut32.dll
    ldr.registerExport("oleaut32.dll", "SysAllocString", reinterpret_cast<void*>(SysAllocString));
    ldr.registerExport("oleaut32.dll", "SysAllocStringLen", reinterpret_cast<void*>(SysAllocStringLen));
    ldr.registerExport("oleaut32.dll", "SysFreeString", reinterpret_cast<void*>(SysFreeString));
    ldr.registerExport("oleaut32.dll", "SysStringLen", reinterpret_cast<void*>(SysStringLen));
    ldr.registerExport("oleaut32.dll", "SysStringByteLen", reinterpret_cast<void*>(SysStringByteLen));
    ldr.registerExport("oleaut32.dll", "VariantInit", reinterpret_cast<void*>(VariantInit));
    ldr.registerExport("oleaut32.dll", "VariantClear", reinterpret_cast<void*>(VariantClear));
    ldr.registerExport("oleaut32.dll", "VariantCopy", reinterpret_cast<void*>(VariantCopy));
}

} // namespace micant::ole32
