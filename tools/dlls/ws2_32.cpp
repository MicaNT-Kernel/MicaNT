// Minimal native WS2 boundary backed only by NTDLL and the canonical AFD
// device. No kernel Executive headers or direct kernel singleton calls.
#define MICANT_NATIVE_USERMODE 1
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <atomic>
#include "micant/shared/native_nt_api.hpp"
#include "micant/shared/afd_socket_abi.hpp"
#include "micant/shared/bounded_containers.hpp"

namespace {
using namespace micant;
using namespace micant::shared::afd;
constexpr uintptr_t InvalidSocket = ~uintptr_t{0};
constexpr int SocketError = -1;
constexpr int WsaInvalidArgument = 10022, WsaNotSocket = 10038, WsaConnectionRefused = 10061;
constexpr int WsaTimedOut = 10060, WsaConnectionReset = 10054, WsaUnsupported = 10045;
constexpr int WsaAccess = 10013, WsaNoBuffers = 10055, WsaAddressInUse = 10048;
constexpr int WsaWouldBlock = 10035;
constexpr int WsaMessageTooLong = 10040;
constexpr int WsaNotInitialised = 10093;
struct SocketSlot { uintptr_t handle{}; uint64_t generation{}; int type{}; bool pending{}; };
SocketSlot socketSlots[64]{};
std::atomic_flag socketTableLock = ATOMIC_FLAG_INIT;
uint32_t startupReferences{};
uint64_t startupGeneration{1};
void lockSocketTable() noexcept {
    while (socketTableLock.test_and_set(std::memory_order_acquire)) __asm__ __volatile__("pause");
}
void unlockSocketTable() noexcept { socketTableLock.clear(std::memory_order_release); }
struct sockaddr_in { int16_t family; uint16_t port; uint32_t address; uint8_t zero[8]; };
// Windows stores the Winsock last error in the calling thread's TEB.
// GS:0x30 is NtTib.Self; LastErrorValue is the public AMD64 TEB field at 0x68.
int& lastError() noexcept {
    uintptr_t teb{};
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    return *reinterpret_cast<int*>(teb + 0x68);
}
int setError(NtStatus status) noexcept {
    switch (status) {
    case NtStatus::InvalidHandle: case NtStatus::HandlesClosed: lastError() = WsaNotSocket; break;
    case NtStatus::PortConnectionRefused: lastError() = WsaConnectionRefused; break;
    case NtStatus::IoTimeout: lastError() = WsaTimedOut; break;
    case NtStatus::ConnectionReset: lastError() = WsaConnectionReset; break;
    case NtStatus::Conflict: lastError() = WsaAddressInUse; break;
    case NtStatus::AccessDenied: lastError() = WsaAccess; break;
    case NtStatus::InsufficientResources: case NtStatus::NoMemory: lastError() = WsaNoBuffers; break;
    case NtStatus::Timeout: lastError() = WsaTimedOut; break;
    case NtStatus::NotSupported: case NtStatus::InvalidDeviceRequest: lastError() = WsaUnsupported; break;
    case NtStatus::InvalidParameter: lastError() = WsaInvalidArgument; break;
    case NtStatus::DeviceBusy: lastError() = WsaWouldBlock; break;
    case NtStatus::BufferOverflow: lastError() = WsaMessageTooLong; break;
    default: lastError() = WsaConnectionReset; break;
    }
    return SocketError;
}
NtStatus control(Handle socket, uint32_t code, const void* input, uint32_t inputSize,
                 void* output, uint32_t outputSize, IoStatusBlock* iosb = nullptr) noexcept {
    IoStatusBlock local{};
    auto status = native_nt::NtDeviceIoControlFile(socket, 0, nullptr, nullptr,
        iosb ? iosb : &local, code, input, inputSize, output, outputSize);
    return status;
}
bool validSocket(uintptr_t socket) noexcept {
    lockSocketTable();
    const bool initialized = startupReferences != 0;
    unlockSocketTable();
    if (!initialized) { lastError() = WsaNotInitialised; return false; }
    if (socket == InvalidSocket || socket == 0) { lastError() = WsaNotSocket; return false; }
    return true;
}
bool validAddress(const sockaddr_in* address, int length) noexcept {
    if (!address || length < static_cast<int>(sizeof(sockaddr_in)) || address->family != 2) {
        lastError() = WsaInvalidArgument; return false;
    }
    return true;
}
}

extern "C" {
__declspec(dllexport) int WSAGetLastError() noexcept { return lastError(); }
__declspec(dllexport) void WSASetLastError(int error) noexcept { lastError() = error; }
struct WSADATA {
    uint16_t version, highVersion;
    uint16_t maxSockets, maxUdpDatagram;
    char* vendorInfo;
    char description[257];
    char status[129];
};
__declspec(dllexport) int WSAStartup(uint16_t requestedVersion, WSADATA* data) noexcept {
    if (!data) { lastError() = WsaInvalidArgument; return WsaInvalidArgument; }
    const uint8_t major = static_cast<uint8_t>(requestedVersion);
    const uint8_t minor = static_cast<uint8_t>(requestedVersion >> 8);
    if (!((major == 1 && minor <= 1) || (major == 2 && minor <= 2))) {
        constexpr int WsaVersionNotSupported = 10092;
        lastError() = WsaVersionNotSupported;
        return WsaVersionNotSupported;
    }
    std::memset(data, 0, sizeof(*data));
    data->version = requestedVersion;
    data->highVersion = 0x0202;
    const char description[] = "MicaNT native AFD sockets";
    std::memcpy(data->description, description, sizeof(description));
    const char status[] = "Running";
    std::memcpy(data->status, status, sizeof(status));
    data->maxSockets = 64; data->maxUdpDatagram = 65467;
    lockSocketTable();
    if (startupReferences == UINT32_MAX) {
        unlockSocketTable();
        lastError() = WsaNoBuffers;
        return WsaNoBuffers;
    }
    ++startupReferences;
    unlockSocketTable();
    return 0;
}
__declspec(dllexport) int WSACleanup() noexcept {
    uintptr_t closing[64]{};
    size_t count{};
    lockSocketTable();
    if (!startupReferences) {
        unlockSocketTable();
        lastError() = WsaNotInitialised;
        return SocketError;
    }
    if (--startupReferences == 0) {
        ++startupGeneration;
        if (!startupGeneration) ++startupGeneration;
        for (auto& slot : socketSlots) {
            if (slot.pending || !slot.handle) continue;
            closing[count++] = slot.handle;
            slot = {};
        }
    }
    unlockSocketTable();
    // NtClose may block/cancel I/O and must not run while holding the table lock.
    for (size_t i = 0; i < count; ++i) (void)native_nt::NtClose(static_cast<Handle>(closing[i]));
    return 0;
}

__declspec(dllexport) uintptr_t socket(int family, int type, int protocol) noexcept {
    lockSocketTable();
    if (!startupReferences) {
        unlockSocketTable();
        lastError() = WsaNotInitialised;
        return InvalidSocket;
    }
    size_t slotIndex = 64;
    for (size_t i = 0; i < 64; ++i) {
        if (!socketSlots[i].pending && !socketSlots[i].handle) { slotIndex = i; break; }
    }
    if (slotIndex == 64) {
        unlockSocketTable();
        lastError() = WsaNoBuffers;
        return InvalidSocket;
    }
    const uint64_t generation = startupGeneration;
    socketSlots[slotIndex].generation = generation;
    socketSlots[slotIndex].pending = true;
    unlockSocketTable();
    const auto abandon = [slotIndex]() noexcept {
        lockSocketTable(); socketSlots[slotIndex] = {}; unlockSocketTable();
    };
    if (family != 2 || (type != 1 && type != 2) ||
        (protocol && !((type == 1 && protocol == 6) || (type == 2 && protocol == 17)))) {
        abandon();
        lastError() = WsaInvalidArgument; return InvalidSocket;
    }
    CreateEndpointV1 create{EndpointMagic, EndpointAbiVersion, sizeof(CreateEndpointV1),
        static_cast<uint32_t>(family), static_cast<uint32_t>(type), static_cast<uint32_t>(protocol), 0};
    constexpr size_t nameOffset = offsetof(FullEaInformationV1, EaName);
    constexpr size_t eaBytes = nameOffset + EndpointEaNameLength + 1 + sizeof(create);
    uint8_t ea[eaBytes]{};
    auto* eaHeader = reinterpret_cast<FullEaInformationV1*>(ea);
    eaHeader->EaNameLength = EndpointEaNameLength;
    eaHeader->EaValueLength = sizeof(create);
    std::memcpy(ea + nameOffset, EndpointEaName, EndpointEaNameLength);
    std::memcpy(ea + nameOffset + EndpointEaNameLength + 1, &create, sizeof(create));
    constexpr wchar_t path[] = L"\\Device\\Afd\\Endpoint";
    UnicodeString name{path};
    ObjectAttributes attributes{}; attributes.objectName = &name; attributes.attributes = OBJ_CASE_INSENSITIVE;
    Handle handle{}; IoStatusBlock iosb{};
    const auto status = native_nt::NtCreateFile(&handle, 0x00100003u, &attributes, &iosb,
        nullptr, 0, 0, 1, 0, ea, sizeof(ea));
    if (status != NtStatus::Success) { abandon(); setError(status); return InvalidSocket; }
    lockSocketTable();
    const bool stillActive = startupReferences && startupGeneration == generation;
    if (stillActive) {
        socketSlots[slotIndex].handle = static_cast<uintptr_t>(handle);
        socketSlots[slotIndex].type = type;
        socketSlots[slotIndex].pending = false;
    }
    else socketSlots[slotIndex] = {};
    unlockSocketTable();
    if (!stillActive) {
        (void)native_nt::NtClose(handle);
        lastError() = WsaNotInitialised;
        return InvalidSocket;
    }
    return static_cast<uintptr_t>(handle);
}

__declspec(dllexport) int bind(uintptr_t socketHandle, const sockaddr_in* address, int length) noexcept {
    if (!validSocket(socketHandle) || !validAddress(address, length)) return SocketError;
    SocketAddressV1 request{sizeof(SocketAddressV1), AddressFamilyInet, address->address,
        static_cast<uint16_t>((address->port << 8) | (address->port >> 8)), 0};
    const auto status = control(static_cast<Handle>(socketHandle), IoctlBind, &request, sizeof(request), nullptr, 0);
    return status == NtStatus::Success ? 0 : setError(status);
}

__declspec(dllexport) int connect(uintptr_t socketHandle, const sockaddr_in* address, int length) noexcept {
    if (!validSocket(socketHandle) || !validAddress(address, length)) return SocketError;
    SocketAddressV1 request{sizeof(SocketAddressV1), AddressFamilyInet, address->address,
        static_cast<uint16_t>((address->port << 8) | (address->port >> 8)), 0};
    const auto status = control(static_cast<Handle>(socketHandle), IoctlConnect, &request, sizeof(request), nullptr, 0);
    return status == NtStatus::Success ? 0 : setError(status);
}

__declspec(dllexport) int send(uintptr_t socketHandle, const char* buffer, int length, int flags) noexcept {
    if (!validSocket(socketHandle)) return SocketError;
    if (flags || length < 0 || (!buffer && length)) {
        lastError() = flags ? WsaUnsupported : WsaInvalidArgument; return SocketError;
    }
    size_t bytes = static_cast<size_t>(length);
    if (bytes > MaximumControlBuffer - sizeof(DataRequestV1)) {
        int type{};
        lockSocketTable();
        for (const auto& slot : socketSlots)
            if (!slot.pending && slot.handle == socketHandle) { type = slot.type; break; }
        unlockSocketTable();
        if (type != 1) { lastError() = type == 2 ? WsaMessageTooLong : WsaNotSocket; return SocketError; }
        bytes = MaximumControlBuffer - sizeof(DataRequestV1);
    }
    micant::shared::BoundedVector<uint8_t> request(sizeof(DataRequestV1) + bytes);
    if (!request.good()) { lastError() = WsaNoBuffers; return SocketError; }
    auto* header = reinterpret_cast<DataRequestV1*>(request.data());
    *header = {sizeof(DataRequestV1), 0, static_cast<uint32_t>(bytes), 0};
    if (bytes) std::memcpy(request.data() + sizeof(*header), buffer, bytes);
    IoStatusBlock iosb{};
    const auto status = control(static_cast<Handle>(socketHandle), IoctlSend, request.data(),
        sizeof(*header) + static_cast<uint32_t>(bytes), nullptr, 0, &iosb);
    return status == NtStatus::Success ? static_cast<int>(iosb.information) : setError(status);
}

__declspec(dllexport) int recv(uintptr_t socketHandle, char* buffer, int length, int flags) noexcept {
    if (!validSocket(socketHandle)) return SocketError;
    if (flags || length < 0 || (!buffer && length)) {
        lastError() = flags ? WsaUnsupported : WsaInvalidArgument; return SocketError;
    }
    const uint32_t capacity = static_cast<uint32_t>(length) > MaximumControlBuffer
        ? MaximumControlBuffer : static_cast<uint32_t>(length);
    DataRequestV1 request{sizeof(request), 0, capacity, 0}; IoStatusBlock iosb{};
    const auto status = control(static_cast<Handle>(socketHandle), IoctlReceive, &request, sizeof(request),
        buffer, capacity, &iosb);
    return status == NtStatus::Success ? static_cast<int>(iosb.information) : setError(status);
}

__declspec(dllexport) int sendto(uintptr_t socketHandle, const char* buffer, int length, int flags,
                                  const sockaddr_in* address, int addressLength) noexcept {
    if (!validSocket(socketHandle)) return SocketError;
    if (!validAddress(address, addressLength) || flags || length < 0 || (!buffer && length) ||
        static_cast<uint64_t>(length) + sizeof(DataRequestV1) + sizeof(SocketAddressV1) > MaximumControlBuffer) {
        if (flags) lastError() = WsaUnsupported;
        else if (length < 0 || (!buffer && length))
            lastError() = WsaInvalidArgument;
        else if (static_cast<uint64_t>(length) + sizeof(DataRequestV1) + sizeof(SocketAddressV1) > MaximumControlBuffer)
            lastError() = WsaMessageTooLong;
        return SocketError;
    }
    micant::shared::BoundedVector<uint8_t> request(sizeof(DataRequestV1) + sizeof(SocketAddressV1) + static_cast<size_t>(length));
    if (!request.good()) { lastError() = WsaNoBuffers; return SocketError; }
    auto* header = reinterpret_cast<DataRequestV1*>(request.data());
    *header = {sizeof(DataRequestV1), 0, static_cast<uint32_t>(length), 0};
    SocketAddressV1 destination{sizeof(destination), AddressFamilyInet, address->address,
        static_cast<uint16_t>((address->port << 8) | (address->port >> 8)), 0};
    std::memcpy(request.data() + sizeof(*header), &destination, sizeof(destination));
    if (length) std::memcpy(request.data() + sizeof(*header) + sizeof(destination), buffer, static_cast<size_t>(length));
    IoStatusBlock iosb{};
    const auto status = control(static_cast<Handle>(socketHandle), IoctlSendTo, request.data(),
        sizeof(*header) + sizeof(destination) + static_cast<uint32_t>(length), nullptr, 0, &iosb);
    return status == NtStatus::Success ? static_cast<int>(iosb.information) : setError(status);
}

__declspec(dllexport) int recvfrom(uintptr_t socketHandle, char* buffer, int length, int flags,
                                    sockaddr_in* source, int* sourceLength) noexcept {
    if (!validSocket(socketHandle)) return SocketError;
    if (flags || length < 0 || (!buffer && length) ||
        (source && !sourceLength)) { lastError() = flags ? WsaUnsupported : WsaInvalidArgument; return SocketError; }
    if (source && *sourceLength < static_cast<int>(sizeof(*source))) {
        *sourceLength = sizeof(*source); lastError() = WsaInvalidArgument; return SocketError;
    }
    // An IPv4 UDP payload cannot exceed 65507 bytes, even when the caller's
    // destination buffer is larger.
    const uint32_t capacity = static_cast<uint32_t>(length) > 65507u
        ? 65507u : static_cast<uint32_t>(length);
    micant::shared::BoundedVector<uint8_t> output(sizeof(ReceiveFromResultV1) + capacity);
    if (!output.good()) { lastError() = WsaNoBuffers; return SocketError; }
    DataRequestV1 request{sizeof(request), 0, capacity, 0}; IoStatusBlock iosb{};
    const auto status = control(static_cast<Handle>(socketHandle), IoctlReceiveFrom, &request, sizeof(request),
        output.data(), sizeof(ReceiveFromResultV1) + capacity, &iosb);
    if (status != NtStatus::Success && status != NtStatus::BufferOverflow) return setError(status);
    if (iosb.information < sizeof(ReceiveFromResultV1)) { lastError() = WsaConnectionReset; return SocketError; }
    const auto* result = reinterpret_cast<const ReceiveFromResultV1*>(output.data());
    if (result->Source.Size != sizeof(SocketAddressV1) || result->Source.AddressFamily != AddressFamilyInet ||
        result->DataLength > static_cast<uint32_t>(length) || iosb.information != sizeof(*result) + result->DataLength) {
        lastError() = WsaConnectionReset; return SocketError;
    }
    if (result->DataLength) std::memcpy(buffer, output.data() + sizeof(*result), result->DataLength);
    if (source) {
        std::memset(source, 0, sizeof(*source)); source->family = 2;
        source->address = result->Source.Ipv4AddressNetworkOrder;
        source->port = static_cast<uint16_t>((result->Source.PortHostOrder << 8) | (result->Source.PortHostOrder >> 8));
        *sourceLength = sizeof(*source);
    } else if (sourceLength) *sourceLength = 0;
    if (status == NtStatus::BufferOverflow) return setError(status);
    const int resultLength = static_cast<int>(result->DataLength);
    return resultLength;
}

__declspec(dllexport) int closesocket(uintptr_t socketHandle) noexcept {
    if (!validSocket(socketHandle)) return SocketError;
    bool owned = false;
    lockSocketTable();
    for (auto& slot : socketSlots) if (!slot.pending && slot.handle == socketHandle) {
        slot = {};
        owned = true;
        break;
    }
    unlockSocketTable();
    if (!owned) { lastError() = WsaNotSocket; return SocketError; }
    const auto status = native_nt::NtClose(static_cast<Handle>(socketHandle));
    return status == NtStatus::Success ? 0 : setError(status);
}

__declspec(dllexport) int setsockopt(uintptr_t socketHandle, int level, int option,
                                      const char* value, int length) noexcept {
    if (!validSocket(socketHandle)) return SocketError;
    if (level != 0xFFFF || (option != 0x1006 && option != 0x1005) || !value || length != sizeof(uint32_t)) {
        lastError() = WsaUnsupported; return SocketError;
    }
    uint32_t timeout{}; std::memcpy(&timeout, value, sizeof(timeout));
    const auto status = control(static_cast<Handle>(socketHandle),
        option == 0x1006 ? IoctlSetReceiveTimeout : IoctlSetSendTimeout,
        &timeout, sizeof(timeout), nullptr, 0);
    return status == NtStatus::Success ? 0 : setError(status);
}
}

extern "C" bool DllMain(void*, unsigned long, void*) noexcept { return true; }
