#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/cm.hpp"
#include <unordered_map>
#include <memory>
#include <cstring>

namespace micant::sys::internal {

static std::unordered_map<Handle, std::shared_ptr<cm::KeyObject>> g_KernelKeys;

bool CmCloseKeyHandle(Handle h) {
    return g_KernelKeys.erase(h) > 0;
}

NtStatus NtOpenKeySys(const SyscallFrame& f) {
    auto* outHandle = reinterpret_cast<Handle*>(f.arg1);
    auto* objAttr = reinterpret_cast<ObjectAttributes*>(f.arg3);
    if (!outHandle || !objAttr || !objAttr->objectName) return NtStatus::InvalidParameter;

    auto keyObj = cm::ConfigurationManager::get().resolvePath(objAttr->objectName->view());
    if (!keyObj) return NtStatus::ObjectNameNotFound;

    Handle h = ObAllocateExecutiveHandle();
    g_KernelKeys[h] = keyObj;
    *outHandle = h;
    return NtStatus::Success;
}

NtStatus NtQueryValueKeySys(const SyscallFrame& f) {
    Handle h = static_cast<Handle>(f.arg1);
    auto* valName = reinterpret_cast<UnicodeString*>(f.arg2);
    void* outBuffer = reinterpret_cast<void*>(f.arg4);
    if (!valName) return NtStatus::InvalidParameter;

    auto it = g_KernelKeys.find(h);
    if (it == g_KernelKeys.end()) return NtStatus::InvalidHandle;

    const auto* val = it->second->getValue(valName->view());
    if (!val) return NtStatus::ObjectNameNotFound;

    size_t bufLen = 0;
    size_t* retLen = nullptr;
    if (f.stackArgs && f.stackArgCount >= 2) {
        bufLen = static_cast<size_t>(f.stackArgs[0]);
        retLen = reinterpret_cast<size_t*>(f.stackArgs[1]);
    }

    if (retLen) *retLen = val->data.size();
    if (outBuffer && bufLen >= val->data.size()) {
        std::memcpy(outBuffer, val->data.data(), val->data.size());
        return NtStatus::Success;
    }

    return (bufLen < val->data.size()) ? NtStatus::BufferTooSmall : NtStatus::Success;
}

NtStatus NtSetValueKeySys(const SyscallFrame& f) {
    Handle h = static_cast<Handle>(f.arg1);
    auto* valName = reinterpret_cast<UnicodeString*>(f.arg2);
    uint32_t type = static_cast<uint32_t>(f.arg4);
    if (!valName) return NtStatus::InvalidParameter;

    auto it = g_KernelKeys.find(h);
    if (it == g_KernelKeys.end()) return NtStatus::InvalidHandle;

    const void* data = nullptr;
    uint32_t dataSize = 0;
    if (f.stackArgs && f.stackArgCount >= 2) {
        data = reinterpret_cast<const void*>(f.stackArgs[0]);
        dataSize = static_cast<uint32_t>(f.stackArgs[1]);
    }
    if (!data || dataSize == 0) return NtStatus::InvalidParameter;

    if (type == static_cast<uint32_t>(cm::RegType::Sz)) {
        std::wstring strVal(reinterpret_cast<const wchar_t*>(data), dataSize / sizeof(wchar_t));
        it->second->setValueString(valName->view(), strVal);
    } else if (type == static_cast<uint32_t>(cm::RegType::Dword) && dataSize >= sizeof(uint32_t)) {
        it->second->setValueDword(valName->view(), *reinterpret_cast<const uint32_t*>(data));
    } else if (type == static_cast<uint32_t>(cm::RegType::Qword) && dataSize >= sizeof(uint64_t)) {
        it->second->setValueQword(valName->view(), *reinterpret_cast<const uint64_t*>(data));
    }

    return NtStatus::Success;
}

} // namespace micant::sys::internal
