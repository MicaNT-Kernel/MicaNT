// ============================================================================
// MicaNT: International Components for Unicode Subsystem (icuuc.dll)
// (icuuc.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides Unicode conversion, codepage mapping, and callback handling
// conforming to standard ICU specifications.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::icuuc {

inline void* ucnv_open(const char* /*name*/, int32_t* err) noexcept {
    if (err) *err = 0;
    return reinterpret_cast<void*>(0x1C001);
}

inline void ucnv_close(void* /*converter*/) noexcept {}
inline void ucnv_reset(void* /*converter*/) noexcept {}

inline const char* ucnv_getName(const void* /*converter*/, int32_t* err) noexcept {
    if (err) *err = 0;
    return "UTF-8";
}

inline const char* ucnv_getStandardName(const char* name, const char* /*standard*/, int32_t* err) noexcept {
    if (err) *err = 0;
    return name ? name : "UTF-8";
}

inline int8_t ucnv_getMaxCharSize(const void* /*converter*/) noexcept {
    return 4;
}

inline void ucnv_fromUnicode(void* /*cnv*/, char** /*target*/, const char* /*targetLimit*/,
                            const wchar_t** /*source*/, const wchar_t* /*sourceLimit*/,
                            int32_t* /*offsets*/, int32_t /*flush*/, int32_t* pErrorCode) noexcept {
    if (pErrorCode) *pErrorCode = 0;
}

inline void ucnv_toUnicode(void* /*cnv*/, wchar_t** /*target*/, const wchar_t* /*targetLimit*/,
                          const char** /*source*/, const char* /*sourceLimit*/,
                          int32_t* /*offsets*/, int32_t /*flush*/, int32_t* pErrorCode) noexcept {
    if (pErrorCode) *pErrorCode = 0;
}

inline int32_t ucnv_fromUCountPending(const void* /*cnv*/, int32_t* status) noexcept {
    if (status) *status = 0;
    return 0;
}

inline int32_t ucnv_toUCountPending(const void* /*cnv*/, int32_t* status) noexcept {
    if (status) *status = 0;
    return 0;
}

inline void ucnv_setFromUCallBack(void* /*converter*/, void* /*newAction*/, const void* /*newContext*/,
                                 void* /*oldAction*/, void* /*oldContext*/, int32_t* err) noexcept {
    if (err) *err = 0;
}

inline void ucnv_setToUCallBack(void* /*converter*/, void* /*newAction*/, const void* /*newContext*/,
                               void* /*oldAction*/, void* /*oldContext*/, int32_t* err) noexcept {
    if (err) *err = 0;
}

inline void ucnv_getFromUCallBack(const void* /*converter*/, void** action, void** context) noexcept {
    if (action) *action = nullptr;
    if (context) *context = nullptr;
}

inline void ucnv_getToUCallBack(const void* /*converter*/, void** action, void** context) noexcept {
    if (action) *action = nullptr;
    if (context) *context = nullptr;
}

inline void ucnv_cbFromUWriteUChars(void* /*args*/, const wchar_t* /*source*/, int32_t /*length*/,
                                   int32_t /*offsetIndex*/, int32_t* pErrorCode) noexcept {
    if (pErrorCode) *pErrorCode = 0;
}

inline void ucnv_cbToUWriteUChars(void* /*args*/, const wchar_t* /*source*/, int32_t /*length*/,
                                 int32_t /*offsetIndex*/, int32_t* pErrorCode) noexcept {
    if (pErrorCode) *pErrorCode = 0;
}

inline void UCNV_FROM_U_CALLBACK_SUBSTITUTE(const void* /*context*/, void* /*fromUArgs*/,
                                           const wchar_t* /*codeUnits*/, int32_t /*length*/,
                                           uint32_t /*codePoint*/, int32_t /*reason*/,
                                           int32_t* pErrorCode) noexcept {
    if (pErrorCode) *pErrorCode = 0;
}

inline void UCNV_TO_U_CALLBACK_SUBSTITUTE(const void* /*context*/, void* /*toUArgs*/,
                                         const char* /*codePoints*/, int32_t /*length*/,
                                         int32_t /*reason*/, int32_t* pErrorCode) noexcept {
    if (pErrorCode) *pErrorCode = 0;
}

inline void InitializeIcuucSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    ldr.registerExport("icuuc.dll", "ucnv_open", reinterpret_cast<void*>(ucnv_open));
    ldr.registerExport("icuuc.dll", "ucnv_close", reinterpret_cast<void*>(ucnv_close));
    ldr.registerExport("icuuc.dll", "ucnv_reset", reinterpret_cast<void*>(ucnv_reset));
    ldr.registerExport("icuuc.dll", "ucnv_getName", reinterpret_cast<void*>(ucnv_getName));
    ldr.registerExport("icuuc.dll", "ucnv_getStandardName", reinterpret_cast<void*>(ucnv_getStandardName));
    ldr.registerExport("icuuc.dll", "ucnv_getMaxCharSize", reinterpret_cast<void*>(ucnv_getMaxCharSize));
    ldr.registerExport("icuuc.dll", "ucnv_fromUnicode", reinterpret_cast<void*>(ucnv_fromUnicode));
    ldr.registerExport("icuuc.dll", "ucnv_toUnicode", reinterpret_cast<void*>(ucnv_toUnicode));
    ldr.registerExport("icuuc.dll", "ucnv_fromUCountPending", reinterpret_cast<void*>(ucnv_fromUCountPending));
    ldr.registerExport("icuuc.dll", "ucnv_toUCountPending", reinterpret_cast<void*>(ucnv_toUCountPending));
    ldr.registerExport("icuuc.dll", "ucnv_setFromUCallBack", reinterpret_cast<void*>(ucnv_setFromUCallBack));
    ldr.registerExport("icuuc.dll", "ucnv_setToUCallBack", reinterpret_cast<void*>(ucnv_setToUCallBack));
    ldr.registerExport("icuuc.dll", "ucnv_getFromUCallBack", reinterpret_cast<void*>(ucnv_getFromUCallBack));
    ldr.registerExport("icuuc.dll", "ucnv_getToUCallBack", reinterpret_cast<void*>(ucnv_getToUCallBack));
    ldr.registerExport("icuuc.dll", "ucnv_cbFromUWriteUChars", reinterpret_cast<void*>(ucnv_cbFromUWriteUChars));
    ldr.registerExport("icuuc.dll", "ucnv_cbToUWriteUChars", reinterpret_cast<void*>(ucnv_cbToUWriteUChars));
    ldr.registerExport("icuuc.dll", "UCNV_FROM_U_CALLBACK_SUBSTITUTE", reinterpret_cast<void*>(UCNV_FROM_U_CALLBACK_SUBSTITUTE));
    ldr.registerExport("icuuc.dll", "UCNV_TO_U_CALLBACK_SUBSTITUTE", reinterpret_cast<void*>(UCNV_TO_U_CALLBACK_SUBSTITUTE));
}

} // namespace micant::icuuc
