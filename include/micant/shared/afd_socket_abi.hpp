#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

// Private, mode-neutral MicaNT AFD endpoint ABI. Winsock enters through the
// native file/syscall boundary; these declarations are shared by Ring 3 and
// the kernel and contain no Executive or kernel-private types.
namespace micant::shared::afd {

inline constexpr char EndpointDevicePath[] = "\\Device\\Afd\\Endpoint";
inline constexpr char EndpointEaName[] = "MicaAfdEndpoint";
inline constexpr uint8_t EndpointEaNameLength = sizeof(EndpointEaName) - 1;
inline constexpr uint32_t EndpointMagic = 0x31444641; // AFD1
inline constexpr uint16_t EndpointAbiVersion = 1;
// Accommodate an entire IPv4 UDP payload (65507 bytes) together with the
// private request/address headers, while retaining a fixed kernel allocation cap.
inline constexpr uint32_t MaximumControlBuffer = 65568;
inline constexpr uint16_t NetworkConfigurationAddressValid = 0x0001;
inline constexpr uint16_t NetworkConfigurationDnsValid = 0x0002;
inline constexpr uint16_t NetworkConfigurationSet = 0x0001;

inline constexpr uint32_t IoctlBind = 0x00012003;
inline constexpr uint32_t IoctlConnect = 0x00012007;
inline constexpr uint32_t IoctlListen = 0x0001200B;
inline constexpr uint32_t IoctlAccept = 0x0001200F;
inline constexpr uint32_t IoctlSend = 0x0001201F;
inline constexpr uint32_t IoctlReceive = 0x00012023;
inline constexpr uint32_t IoctlSendTo = 0x00012027;
inline constexpr uint32_t IoctlReceiveFrom = 0x0001202B;
inline constexpr uint32_t IoctlSetReceiveTimeout = 0x0001202F;
inline constexpr uint32_t IoctlQueryNetworkConfiguration = 0x00012033;
inline constexpr uint32_t IoctlSetNetworkConfiguration = 0x00012037;
inline constexpr uint32_t IoctlSetSendTimeout = 0x0001203B;
inline constexpr uint32_t IoctlSetConnectTimeout = 0x0001203F;

inline constexpr uint32_t AddressFamilyInet = 2;
inline constexpr uint32_t SocketStream = 1;
inline constexpr uint32_t SocketDatagram = 2;
inline constexpr uint32_t ProtocolTcp = 6;
inline constexpr uint32_t ProtocolUdp = 17;

#pragma pack(push, 1)

struct FullEaInformationV1 {
    uint32_t NextEntryOffset;
    uint8_t Flags;
    uint8_t EaNameLength;
    uint16_t EaValueLength;
    char EaName[1]; // followed by NUL, then EaValueLength bytes
};

struct CreateEndpointV1 {
    uint32_t Magic;
    uint16_t Version;
    uint16_t Size;
    uint32_t AddressFamily;
    uint32_t SocketType;
    uint32_t Protocol;
    uint32_t Flags;
};

struct SocketAddressV1 {
    uint32_t Size;
    uint32_t AddressFamily;
    uint32_t Ipv4AddressNetworkOrder;
    uint16_t PortHostOrder;
    uint16_t Reserved;
};

struct AcceptResultV1 {
    uint32_t Size;
    uintptr_t SocketHandle;
    SocketAddressV1 Remote;
};

struct DataRequestV1 {
    uint32_t Size;
    uint32_t Flags;
    uint32_t DataLength;
    uint32_t TimeoutMilliseconds; // receive timeout; zero means poll
    // Send IOCTLs append DataLength bytes immediately after this header.
};

struct ReceiveFromResultV1 {
    SocketAddressV1 Source;
    uint32_t DataLength;
};

struct NetworkConfigurationV1 {
    uint32_t Size;
    uint16_t Version;
    uint16_t Flags;
    uint32_t Ipv4AddressNetworkOrder;
    uint32_t SubnetMaskNetworkOrder;
    uint32_t GatewayNetworkOrder;
    uint32_t DnsServerNetworkOrder;
    uint8_t MacAddress[6];
    uint8_t Reserved[2];
};

#pragma pack(pop)

static_assert(std::is_standard_layout_v<FullEaInformationV1>);
static_assert(std::is_trivially_copyable_v<FullEaInformationV1>);
static_assert(offsetof(FullEaInformationV1, EaName) == 8);
static_assert(sizeof(CreateEndpointV1) == 24);
static_assert(sizeof(SocketAddressV1) == 16);
static_assert(sizeof(DataRequestV1) == 16);
static_assert(sizeof(NetworkConfigurationV1) == 32);

} // namespace micant::shared::afd
