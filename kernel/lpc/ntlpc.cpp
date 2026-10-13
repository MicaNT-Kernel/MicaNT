#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/lpc.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
#include <span>
#include <cstring>
#include <algorithm>

namespace micant::sys::internal {

static std::unordered_map<Handle, std::shared_ptr<lpc::PortObject>> g_KernelPorts;

bool LpcClosePortHandle(Handle h) {
    return g_KernelPorts.erase(h) > 0;
}

NtStatus NtCreatePortSys(const SyscallFrame& f) {
    auto* outPortHandle = reinterpret_cast<Handle*>(f.arg1);
    auto* objAttr = reinterpret_cast<ObjectAttributes*>(f.arg2);
    if (!outPortHandle || !objAttr || !objAttr->objectName) return NtStatus::InvalidParameter;

    auto port = lpc::PortManager::get().createPort(objAttr->objectName->view());
    if (!port) return NtStatus::ObjectNameCollision;

    Handle h = ObAllocateExecutiveHandle();
    g_KernelPorts[h] = port;
    *outPortHandle = h;
    return NtStatus::Success;
}

NtStatus NtConnectPortSys(const SyscallFrame& f) {
    auto* outPortHandle = reinterpret_cast<Handle*>(f.arg1);
    auto* portName = reinterpret_cast<UnicodeString*>(f.arg2);
    if (!outPortHandle || !portName) return NtStatus::InvalidParameter;

    std::shared_ptr<lpc::PortObject> clientPort;
    std::shared_ptr<lpc::PortObject> serverPort;
    NtStatus status = lpc::PortManager::get().connectPort(portName->view(), clientPort, serverPort);
    if (!NT_SUCCESS(status)) return status;

    Handle h = ObAllocateExecutiveHandle();
    g_KernelPorts[h] = clientPort;
    *outPortHandle = h;
    return NtStatus::Success;
}

NtStatus NtRequestWaitReplyPortSys(const SyscallFrame& f) {
    Handle h = static_cast<Handle>(f.arg1);
    auto* reqMsg = reinterpret_cast<lpc::PortMessage*>(f.arg2);
    auto* replyMsg = reinterpret_cast<lpc::PortMessage*>(f.arg3);
    if (!reqMsg || !replyMsg) return NtStatus::InvalidParameter;

    auto it = g_KernelPorts.find(h);
    if (it == g_KernelPorts.end()) return NtStatus::InvalidHandle;

    const uint8_t* reqData = reinterpret_cast<const uint8_t*>(reqMsg + 1);
    size_t reqDataLen = reqMsg->u1.dataLength;

    std::vector<uint8_t> replyData;
    NtStatus res = it->second->requestWaitReply(*reqMsg, std::span<const uint8_t>(reqData, reqDataLen), *replyMsg, replyData, 5000);
    if (NT_SUCCESS(res) && !replyData.empty()) {
        uint8_t* outReplyData = reinterpret_cast<uint8_t*>(replyMsg + 1);
        std::memcpy(outReplyData, replyData.data(), std::min<size_t>(replyData.size(), 256));
    }
    return res;
}

} // namespace micant::sys::internal
