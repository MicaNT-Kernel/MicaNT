#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/afd_io.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/executive/io_file.hpp"
#include "micant/km/executive/ps.hpp"
#include "micant/km/drivers/net/tcpip_kernel_api.hpp"
#include "micant/shared/afd_socket_abi.hpp"

#include <cstring>
#include <new>
#include <atomic>

namespace micant::km::afdwsk {
namespace {
using namespace micant::shared::afd;
struct Endpoint {
    uint32_t magic{EndpointMagic};
    int socketId{};
    uint32_t family{};
    uint32_t type{};
    uint32_t protocol{};
    uint32_t receiveTimeoutMilliseconds{};
    std::atomic<bool> closing{false};
    std::atomic<bool> socketClosed{false};
    void* socketWaitObject{};
};

io::DriverObject g_AfdDriver{};
io::DeviceObject g_AfdDevice{};
se::FreestandingSecurityDescriptor g_NetworkConfigurationSecurity{};
bool g_Initialized{};

NtStatus endpointRead(void*, uint64_t, void*, size_t, size_t* bytesRead) noexcept {
    if (bytesRead) *bytesRead = 0;
    return NtStatus::InvalidDeviceRequest;
}
void endpointRelease(void* context) noexcept {
    auto* endpoint = static_cast<Endpoint*>(context);
    if (!endpoint) return;
    if (!endpoint->socketClosed.exchange(true, std::memory_order_acq_rel))
        tcpip::kernel_api::closeSocket(endpoint->socketId);
    tcpip::kernel_api::releaseSocketWaitObject(endpoint->socketWaitObject);
    delete endpoint;
}
void endpointLastHandleClosed(void* context) noexcept {
    auto* endpoint = static_cast<Endpoint*>(context);
    if (endpoint) {
        endpoint->closing.store(true, std::memory_order_release);
        tcpip::kernel_api::cancelSocketWaitObject(endpoint->socketWaitObject);
    }
}
Endpoint* endpointFor(const io::FileObject* file) noexcept {
    if (!file || file->deviceObject() != &g_AfdDevice || !file->context()) return nullptr;
    auto* endpoint = static_cast<Endpoint*>(file->context());
    return endpoint->magic == EndpointMagic && endpoint->family == AddressFamilyInet &&
        !endpoint->closing.load(std::memory_order_acquire) ? endpoint : nullptr;
}

NtStatus dispatch(io::DeviceObject*, io::Irp* irp) {
    if (!irp || !irp->fileObject || !irp->systemBuffer) return NtStatus::InvalidParameter;
    auto* endpoint = endpointFor(irp->fileObject);
    if (!endpoint) return NtStatus::InvalidHandle;
    const uint32_t code = irp->byteOffset.lowPart;
    const size_t inputLength = static_cast<uint32_t>(irp->byteOffset.highPart);
    const size_t outputLength = irp->length;
    auto* input = static_cast<const uint8_t*>(irp->systemBuffer);
    auto* output = static_cast<uint8_t*>(irp->userBuffer);
    NtStatus status = NtStatus::InvalidDeviceRequest;
    size_t information = 0;
    if (code == IoctlBind || code == IoctlConnect) {
        if (inputLength != sizeof(SocketAddressV1)) status = NtStatus::InfoLengthMismatch;
        else {
            SocketAddressV1 address{};
            std::memcpy(&address, input, sizeof(address));
            if (address.Size != sizeof(address) || address.AddressFamily != AddressFamilyInet)
                status = NtStatus::InvalidParameter;
            else {
                const bool ok = code == IoctlBind
                    ? tcpip::kernel_api::bindSocket(endpoint->socketId, address.Ipv4AddressNetworkOrder, address.PortHostOrder)
                    : tcpip::kernel_api::connectSocket(endpoint->socketId, address.Ipv4AddressNetworkOrder, address.PortHostOrder);
                status = ok ? NtStatus::Success :
                    (code == IoctlConnect ? NtStatus::PortConnectionRefused : NtStatus::Conflict);
            }
        }
    } else if (code == IoctlListen) {
        if (inputLength != sizeof(uint32_t)) status = NtStatus::InfoLengthMismatch;
        else { int32_t backlog{}; std::memcpy(&backlog, input, sizeof(backlog));
            if (endpoint->type != SocketStream || backlog < 0) status = NtStatus::InvalidParameter;
            else status = tcpip::kernel_api::listenSocket(endpoint->socketId, backlog)
                ? NtStatus::Success : NtStatus::Unsuccessful; }
    } else if (code == IoctlSend || code == IoctlSendTo) {
        if (inputLength < sizeof(DataRequestV1)) status = NtStatus::InfoLengthMismatch;
        else {
            DataRequestV1 request{}; std::memcpy(&request, input, sizeof(request));
            const size_t addressBytes = code == IoctlSendTo ? sizeof(SocketAddressV1) : 0;
            if (request.Size != sizeof(request) || request.Flags != 0 || request.TimeoutMilliseconds != 0 ||
                (code == IoctlSendTo && endpoint->type != SocketDatagram) ||
                request.DataLength > MaximumControlBuffer - sizeof(request) - addressBytes ||
                inputLength < sizeof(request) + addressBytes ||
                request.DataLength != inputLength - sizeof(request) - addressBytes)
                status = NtStatus::InvalidParameter;
            else {
                int sent;
                if (code == IoctlSend) sent = tcpip::kernel_api::sendSocket(endpoint->socketId, input + sizeof(request), request.DataLength);
                else if (inputLength >= sizeof(request) + sizeof(SocketAddressV1)) {
                    SocketAddressV1 address{}; std::memcpy(&address, input + sizeof(request), sizeof(address));
                    if (address.Size != sizeof(address) || address.AddressFamily != AddressFamilyInet) sent = -1;
                    else sent = tcpip::kernel_api::sendToSocket(endpoint->socketId,
                        input + sizeof(request) + sizeof(address), request.DataLength,
                        address.Ipv4AddressNetworkOrder, address.PortHostOrder);
                } else sent = -1;
                if (sent < 0) status = NtStatus::Unsuccessful;
                else { status = NtStatus::Success; information = static_cast<size_t>(sent); }
            }
        }
    } else if (code == IoctlSetReceiveTimeout) {
        if (inputLength != sizeof(uint32_t)) status = NtStatus::InfoLengthMismatch;
        else {
            uint32_t timeout{};
            std::memcpy(&timeout, input, sizeof(timeout));
            if (timeout > 60000) status = NtStatus::InvalidParameter;
            else { endpoint->receiveTimeoutMilliseconds = timeout; status = NtStatus::Success; }
        }
    } else if (code == IoctlQueryNetworkConfiguration) {
        if (inputLength != 0 || outputLength != sizeof(NetworkConfigurationV1) || !output)
            status = NtStatus::InfoLengthMismatch;
        else if (!tcpip::kernel_api::hasAdapter()) status = NtStatus::DeviceNotReady;
        else {
            NetworkConfigurationV1 configuration{};
            configuration.Size = sizeof(configuration);
            configuration.Version = EndpointAbiVersion;
            tcpip::kernel_api::queryNetworkConfiguration(
                &configuration.Ipv4AddressNetworkOrder, &configuration.SubnetMaskNetworkOrder,
                &configuration.GatewayNetworkOrder, &configuration.DnsServerNetworkOrder,
                configuration.MacAddress);
            if (configuration.Ipv4AddressNetworkOrder != 0)
                configuration.Flags |= NetworkConfigurationAddressValid;
            if (configuration.DnsServerNetworkOrder != 0)
                configuration.Flags |= NetworkConfigurationDnsValid;
            std::memcpy(output, &configuration, sizeof(configuration));
            information = sizeof(configuration);
            status = NtStatus::Success;
        }
    } else if (code == IoctlSetNetworkConfiguration) {
        if (inputLength != sizeof(NetworkConfigurationV1)) status = NtStatus::InfoLengthMismatch;
        else {
            NetworkConfigurationV1 configuration{};
            std::memcpy(&configuration, input, sizeof(configuration));
            if (configuration.Size != sizeof(configuration) || configuration.Version != EndpointAbiVersion ||
                configuration.Flags != NetworkConfigurationSet || configuration.Reserved[0] || configuration.Reserved[1])
                status = NtStatus::InvalidParameter;
            else {
                auto* process = ke::KeGetCurrentProcess();
                auto* currentThread = ke::KeGetCurrentThread();
                const auto* token = process ? se::SeGetEffectiveFreestandingToken(
                    process->primaryToken(), currentThread && currentThread->owningThread
                        ? currentThread->owningThread->impersonationToken() : nullptr) : nullptr;
                uint32_t grantedAccess{};
                if (!se::SeAccessCheckFreestanding(token, g_NetworkConfigurationSecurity,
                        se::SE_NETWORK_CONFIG_WRITE, grantedAccess))
                    status = NtStatus::AccessDenied;
                else {
                    NetworkConfigurationV1 current{};
                    tcpip::kernel_api::queryNetworkConfiguration(
                        &current.Ipv4AddressNetworkOrder, &current.SubnetMaskNetworkOrder,
                        &current.GatewayNetworkOrder, &current.DnsServerNetworkOrder, current.MacAddress);
                    if (std::memcmp(current.MacAddress, configuration.MacAddress, sizeof(current.MacAddress)) != 0)
                        status = NtStatus::AccessDenied;
                    else status = tcpip::kernel_api::setNetworkConfiguration(
                        configuration.Ipv4AddressNetworkOrder, configuration.SubnetMaskNetworkOrder,
                        configuration.GatewayNetworkOrder, configuration.DnsServerNetworkOrder)
                        ? NtStatus::Success : NtStatus::InvalidParameter;
                }
            }
        }
    } else if (code == IoctlReceive || code == IoctlReceiveFrom) {
        if (inputLength != sizeof(DataRequestV1) || !output || outputLength == 0 ||
            (code == IoctlReceiveFrom && endpoint->type != SocketDatagram)) status = NtStatus::InvalidParameter;
        else {
            DataRequestV1 request{}; std::memcpy(&request, input, sizeof(request));
            const size_t metadataBytes = code == IoctlReceiveFrom ? sizeof(ReceiveFromResultV1) : 0;
            if (request.Size != sizeof(request) || request.Flags != 0 ||
                request.DataLength > outputLength || outputLength < metadataBytes ||
                request.DataLength > outputLength - metadataBytes || request.DataLength > MaximumControlBuffer)
                status = NtStatus::InvalidParameter;
            else {
                if (ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) status = NtStatus::InvalidDeviceState;
                else {
                    for (;;) {
                        if (endpoint->closing.load(std::memory_order_acquire)) {
                            status = NtStatus::Cancelled;
                            break;
                        }
                        const auto waitStatus = static_cast<NtStatus>(tcpip::kernel_api::waitSocketData(
                            endpoint->socketWaitObject, endpoint->receiveTimeoutMilliseconds));
                        if (waitStatus != NtStatus::Success) { status = waitStatus; break; }
                        if (code == IoctlReceiveFrom) {
                            auto* result = reinterpret_cast<ReceiveFromResultV1*>(output);
                            uint32_t sourceIpNetworkOrder{};
                            uint16_t sourcePort{};
                            const int received = tcpip::kernel_api::recvFromSocket(endpoint->socketId,
                                output + sizeof(*result), request.DataLength,
                                &sourceIpNetworkOrder, &sourcePort);
                            if (received < 0) continue;
                            result->Source.Size = sizeof(SocketAddressV1);
                            result->Source.AddressFamily = AddressFamilyInet;
                            result->Source.Ipv4AddressNetworkOrder = sourceIpNetworkOrder;
                            result->Source.PortHostOrder = sourcePort;
                            result->Source.Reserved = 0;
                            result->DataLength = static_cast<uint32_t>(received);
                            information = sizeof(*result) + static_cast<size_t>(received);
                            status = NtStatus::Success;
                            break;
                        }
                        const int received = tcpip::kernel_api::recvSocket(endpoint->socketId,
                            output, request.DataLength);
                        if (received < 0) continue;
                        information = static_cast<size_t>(received);
                        status = NtStatus::Success;
                        break;
                    }
                }
            }
        }
    }
    irp->ioStatus.status = status;
    irp->ioStatus.information = information;
    return status;
}
} // namespace

NtStatus IoAfdInitialize() noexcept {
    if (g_Initialized) return NtStatus::Success;
    se::SeInitializeNetworkConfigurationAcl(g_NetworkConfigurationSecurity);
    g_AfdDriver.setDispatch(io::IRP_MJ_DEVICE_CONTROL, dispatch);
    g_AfdDevice.driverObject = &g_AfdDriver;
    g_AfdDevice.deviceType = io::DeviceType::Unknown;
    g_AfdDevice.flags = io::DO_BUFFERED_IO;
    g_Initialized = true;
    return NtStatus::Success;
}

NtStatus IoAfdCreateEndpointHandle(Handle* handle, std::wstring_view path, uint32_t desiredAccess,
    const void* eaBuffer, size_t eaLength) noexcept {
    using namespace micant::shared::afd;
    constexpr std::wstring_view endpointPath(L"\\Device\\Afd\\Endpoint");
    if (!handle || path != endpointPath || !eaBuffer ||
        eaLength < offsetof(FullEaInformationV1, EaName) + EndpointEaNameLength + 1 + sizeof(CreateEndpointV1) ||
        eaLength > 256) return NtStatus::InvalidParameter;
    const auto* ea = static_cast<const uint8_t*>(eaBuffer);
    FullEaInformationV1 header{};
    std::memcpy(&header, ea, offsetof(FullEaInformationV1, EaName));
    if (header.NextEntryOffset != 0 || header.Flags != 0 || header.EaNameLength != EndpointEaNameLength ||
        header.EaValueLength != sizeof(CreateEndpointV1) ||
        offsetof(FullEaInformationV1, EaName) + EndpointEaNameLength + 1 + header.EaValueLength != eaLength)
        return NtStatus::InvalidParameter;
    const size_t nameOffset = offsetof(FullEaInformationV1, EaName);
    if (std::memcmp(ea + nameOffset, EndpointEaName, EndpointEaNameLength) != 0 || ea[nameOffset + EndpointEaNameLength] != 0)
        return NtStatus::InvalidParameter;
    CreateEndpointV1 request{};
    std::memcpy(&request, ea + nameOffset + EndpointEaNameLength + 1, sizeof(request));
    if (request.Magic != EndpointMagic || request.Version != EndpointAbiVersion || request.Size != sizeof(request) ||
        request.AddressFamily != AddressFamilyInet ||
        !((request.SocketType == SocketStream && (request.Protocol == 0 || request.Protocol == ProtocolTcp)) ||
          (request.SocketType == SocketDatagram && (request.Protocol == 0 || request.Protocol == ProtocolUdp))) || request.Flags != 0)
        return NtStatus::InvalidParameter;
    NtStatus init = IoAfdInitialize();
    if (!NT_SUCCESS(init)) return init;
    if (!tcpip::kernel_api::isInitialized()) return NtStatus::DeviceNotReady;
    const int id = tcpip::kernel_api::createSocket(request.AddressFamily, request.SocketType, request.Protocol);
    if (id <= 0) return NtStatus::InsufficientResources;
    void* waitObject = tcpip::kernel_api::referenceSocketWaitObject(id);
    if (!waitObject) { tcpip::kernel_api::closeSocket(id); return NtStatus::DeviceNotReady; }
    auto* state = new (std::nothrow_t{}) Endpoint{};
    if (!state) {
        tcpip::kernel_api::closeSocket(id);
        tcpip::kernel_api::releaseSocketWaitObject(waitObject);
        return NtStatus::NoMemory;
    }
    state->socketWaitObject = waitObject;
    state->socketId = id; state->family = request.AddressFamily; state->type = request.SocketType; state->protocol = request.Protocol;
    static constexpr char16_t name[] = u"\\Device\\Afd\\Endpoint";
    io::FileBackend backend{state, 1, std::u16string_view(name, (sizeof(name) / sizeof(name[0])) - 1),
        endpointRead, endpointRelease, false, endpointLastHandleClosed};
    return io::IoCreateDeviceFile(handle, desiredAccess, &g_AfdDevice, backend);
}

bool IoAfdIsEndpointFile(const io::FileObject* fileObject) noexcept { return endpointFor(fileObject) != nullptr; }
io::DeviceObject* IoAfdDeviceObject() noexcept { (void)IoAfdInitialize(); return &g_AfdDevice; }
NtStatus IoAfdDispatchDeviceControl(io::Irp* irp) noexcept { return dispatch(&g_AfdDevice, irp); }

} // namespace micant::km::afdwsk

namespace micant::sys::internal {
void AfdWskInitializeExecutiveSubsystem() noexcept { (void)micant::km::afdwsk::IoAfdInitialize(); }
} // namespace micant::sys::internal
