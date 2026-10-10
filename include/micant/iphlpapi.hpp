#pragma once

/**
 * @file iphlpapi.hpp
 * @brief Clean-Room IP Helper API (iphlpapi.dll) Bridge.
 *
 * Implements standard Windows NT IP Helper network configuration inspection:
 * - GetAdaptersInfo
 * - GetNetworkParams
 * - GetIpForwardTable
 *
 * References: Microsoft Learn IP Helper API Documentation.
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"
#include "tcpip.hpp"

namespace micant::iphlpapi {

inline constexpr uint32_t MAX_ADAPTER_NAME_LENGTH    = 256;
inline constexpr uint32_t MAX_ADAPTER_DESCRIPTION_LENGTH = 128;
inline constexpr uint32_t MAX_ADAPTER_ADDRESS_LENGTH = 8;
inline constexpr uint32_t MAX_HOSTNAME_LEN           = 128;
inline constexpr uint32_t MAX_DOMAIN_NAME_LEN        = 128;
inline constexpr uint32_t MAX_SCOPE_ID_LEN           = 256;

// Adapter Types
inline constexpr uint32_t MIB_IF_TYPE_OTHER     = 1;
inline constexpr uint32_t MIB_IF_TYPE_ETHERNET  = 6;
inline constexpr uint32_t MIB_IF_TYPE_TOKENRING = 9;
inline constexpr uint32_t MIB_IF_TYPE_FDDI      = 15;
inline constexpr uint32_t MIB_IF_TYPE_PPP       = 23;
inline constexpr uint32_t MIB_IF_TYPE_LOOPBACK  = 24;

// Error Codes
inline constexpr uint32_t ERROR_SUCCESS         = 0;
inline constexpr uint32_t ERROR_BUFFER_OVERFLOW = 111;
inline constexpr uint32_t ERROR_INVALID_PARAMETER = 87;
inline constexpr uint32_t ERROR_NO_DATA         = 232;

#pragma pack(push, 4)

struct IP_ADDRESS_STRING {
    char str[16]{0};
};

struct IP_ADDR_STRING {
    IP_ADDR_STRING*   next{nullptr};
    IP_ADDRESS_STRING ipAddress;
    IP_ADDRESS_STRING ipMask;
    uint32_t          context{0};
};

struct IP_ADAPTER_INFO {
    IP_ADAPTER_INFO* next{nullptr};
    uint32_t comboIndex{0};
    char adapterName[MAX_ADAPTER_NAME_LENGTH + 4]{0};
    char description[MAX_ADAPTER_DESCRIPTION_LENGTH + 4]{0};
    uint32_t addressLength{6};
    uint8_t address[MAX_ADAPTER_ADDRESS_LENGTH]{0};
    uint32_t index{1};
    uint32_t type{MIB_IF_TYPE_ETHERNET};
    uint32_t dhcpEnabled{0};
    IP_ADDR_STRING currentIpAddress{};
    IP_ADDR_STRING ipAddressList{};
    IP_ADDR_STRING gatewayList{};
    IP_ADDR_STRING dhcpServer{};
    int haveWins{0};
    IP_ADDR_STRING primaryWinsServer{};
    IP_ADDR_STRING secondaryWinsServer{};
    int64_t leaseObtained{0};
    int64_t leaseExpires{0};
};

struct FIXED_INFO {
    char hostName[MAX_HOSTNAME_LEN + 4]{0};
    char domainName[MAX_DOMAIN_NAME_LEN + 4]{0};
    IP_ADDR_STRING* currentDnsServer{nullptr};
    IP_ADDR_STRING dnsServerList{};
    uint32_t nodeType{1};
    char scopeId[MAX_SCOPE_ID_LEN + 4]{0};
    uint32_t enableRouting{0};
    uint32_t enableProxy{0};
    uint32_t enableDns{1};
};

#pragma pack(pop)

inline uint32_t GetAdaptersInfo(IP_ADAPTER_INFO* pAdapterInfo, uint32_t* pOutBufLen) noexcept {
    if (!pOutBufLen) return ERROR_INVALID_PARAMETER;

    uint32_t requiredSize = sizeof(IP_ADAPTER_INFO);
    if (!pAdapterInfo || *pOutBufLen < requiredSize) {
        *pOutBufLen = requiredSize;
        return ERROR_BUFFER_OVERFLOW;
    }

    auto& net = tcpip::NetworkStack::get();
    auto adapter = net.getAdapter();

    std::memset(pAdapterInfo, 0, sizeof(IP_ADAPTER_INFO));
    pAdapterInfo->next = nullptr;
    pAdapterInfo->comboIndex = 1;
    pAdapterInfo->index = 1;
    pAdapterInfo->type = MIB_IF_TYPE_ETHERNET;
    pAdapterInfo->addressLength = 6;

    if (adapter) {
        auto mac = adapter->getMacAddress();
        std::memcpy(pAdapterInfo->address, mac.bytes, 6);
        std::string name = "{4D36E972-E325-11CE-BFC1-08002BE10318}";
        std::strncpy(pAdapterInfo->adapterName, name.c_str(), sizeof(pAdapterInfo->adapterName) - 1);
        std::string desc = "MicaNT 10-Gigabit Virtual Network Adapter";
        std::strncpy(pAdapterInfo->description, desc.c_str(), sizeof(pAdapterInfo->description) - 1);
    }

    // IP Address List
    std::string ipStr = net.getLocalIp().toString();
    std::string maskStr = net.getSubnetMask().toString();
    std::string gwStr = net.getGateway().toString();

    std::strncpy(pAdapterInfo->ipAddressList.ipAddress.str, ipStr.c_str(), 15);
    std::strncpy(pAdapterInfo->ipAddressList.ipMask.str, maskStr.c_str(), 15);
    std::strncpy(pAdapterInfo->gatewayList.ipAddress.str, gwStr.c_str(), 15);

    *pOutBufLen = requiredSize;
    return ERROR_SUCCESS;
}

inline uint32_t GetNetworkParams(FIXED_INFO* pFixedInfo, uint32_t* pOutBufLen) noexcept {
    if (!pOutBufLen) return ERROR_INVALID_PARAMETER;

    uint32_t requiredSize = sizeof(FIXED_INFO);
    if (!pFixedInfo || *pOutBufLen < requiredSize) {
        *pOutBufLen = requiredSize;
        return ERROR_BUFFER_OVERFLOW;
    }

    auto& net = tcpip::NetworkStack::get();

    std::memset(pFixedInfo, 0, sizeof(FIXED_INFO));
    std::strncpy(pFixedInfo->hostName, "MicaNT-Workstation", sizeof(pFixedInfo->hostName) - 1);
    std::strncpy(pFixedInfo->domainName, "localdomain", sizeof(pFixedInfo->domainName) - 1);
    pFixedInfo->nodeType = 1; // Broadcast node
    pFixedInfo->enableDns = 1;

    std::string dnsStr = net.getDnsServer().toString();
    std::strncpy(pFixedInfo->dnsServerList.ipAddress.str, dnsStr.c_str(), 15);

    *pOutBufLen = requiredSize;
    return ERROR_SUCCESS;
}

struct NET_LUID {
    uint64_t Value;
};
using PNET_LUID = NET_LUID*;
using NETIO_STATUS = uint32_t;

inline NETIO_STATUS ConvertInterfaceGuidToLuid(const GUID* InterfaceGuid, PNET_LUID InterfaceLuid) noexcept {
    if (!InterfaceGuid || !InterfaceLuid) return 87; // ERROR_INVALID_PARAMETER
    InterfaceLuid->Value = (static_cast<uint64_t>(InterfaceGuid->Data1)) | 0x0100000000000000ULL;
    return 0; // NO_ERROR
}

inline NETIO_STATUS ConvertInterfaceLuidToAlias(const NET_LUID* InterfaceLuid, wchar_t* InterfaceAlias, size_t Length) noexcept {
    if (!InterfaceLuid || !InterfaceAlias || Length < 5) return 87;
    std::swprintf(InterfaceAlias, Length, L"eth0");
    return 0;
}

inline uint32_t __stdcall if_nametoindex(const char* /*InterfaceName*/) noexcept {
    return 1;
}

inline char* __stdcall if_indextoname(uint32_t /*InterfaceIndex*/, char* InterfaceName) noexcept {
    if (InterfaceName) {
        std::strcpy(InterfaceName, "eth0");
    }
    return InterfaceName;
}

inline uint32_t WINAPI GetAdaptersAddresses(uint32_t /*Family*/, uint32_t /*Flags*/, void* /*Reserved*/, void* AdapterAddresses, uint32_t* SizePointer) noexcept {
    if (!SizePointer) return 87; // ERROR_INVALID_PARAMETER
    if (*SizePointer < 256 || !AdapterAddresses) {
        *SizePointer = 512;
        return 111; // ERROR_BUFFER_OVERFLOW
    }
    std::memset(AdapterAddresses, 0, *SizePointer);
    return 0; // NO_ERROR
}

inline uint32_t WINAPI ConvertInterfaceNameToLuidW(const wchar_t* /*InterfaceName*/, void* InterfaceLuid) noexcept {
    if (InterfaceLuid) *reinterpret_cast<uint64_t*>(InterfaceLuid) = 0x10001;
    return 0;
}

inline uint32_t WINAPI ConvertInterfaceIndexToLuid(uint32_t /*InterfaceIndex*/, void* InterfaceLuid) noexcept {
    if (InterfaceLuid) *reinterpret_cast<uint64_t*>(InterfaceLuid) = 0x10001;
    return 0;
}

inline uint32_t WINAPI ConvertInterfaceLuidToIndex(const void* /*InterfaceLuid*/, uint32_t* InterfaceIndex) noexcept {
    if (InterfaceIndex) *InterfaceIndex = 1;
    return 0;
}

inline uint32_t WINAPI ConvertInterfaceLuidToGuid(const void* /*InterfaceLuid*/, void* InterfaceGuid) noexcept {
    if (InterfaceGuid) std::memset(InterfaceGuid, 0, 16);
    return 0;
}

inline uint32_t WINAPI ConvertInterfaceLuidToNameW(const void* /*InterfaceLuid*/, wchar_t* InterfaceName, size_t Length) noexcept {
    if (InterfaceName && Length > 4) std::memcpy(InterfaceName, L"eth0", 10);
    return 0;
}

inline uint32_t WINAPI NotifyUnicastIpAddressChange(uint16_t /*Family*/, void* /*Callback*/, void* /*CallerContext*/, uint8_t /*InitialNotification*/, void** NotificationHandle) noexcept {
    if (NotificationHandle) *NotificationHandle = reinterpret_cast<void*>(0x5001);
    return 0;
}

inline uint32_t WINAPI CancelMibChangeNotify2(void* /*NotificationHandle*/) noexcept {
    return 0;
}

inline void InitializeIpHlpApiSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("iphlpapi.dll", "GetAdaptersInfo", reinterpret_cast<void*>(GetAdaptersInfo));
    ldr.registerExport("iphlpapi.dll", "GetNetworkParams", reinterpret_cast<void*>(GetNetworkParams));
    ldr.registerExport("iphlpapi.dll", "ConvertInterfaceGuidToLuid", reinterpret_cast<void*>(ConvertInterfaceGuidToLuid));
    ldr.registerExport("iphlpapi.dll", "ConvertInterfaceLuidToAlias", reinterpret_cast<void*>(ConvertInterfaceLuidToAlias));
    ldr.registerExport("iphlpapi.dll", "if_nametoindex", reinterpret_cast<void*>(if_nametoindex));
    ldr.registerExport("iphlpapi.dll", "if_indextoname", reinterpret_cast<void*>(if_indextoname));
    ldr.registerExport("iphlpapi.dll", "GetAdaptersAddresses", reinterpret_cast<void*>(GetAdaptersAddresses));
    ldr.registerExport("iphlpapi.dll", "ConvertInterfaceNameToLuidW", reinterpret_cast<void*>(ConvertInterfaceNameToLuidW));
    ldr.registerExport("iphlpapi.dll", "ConvertInterfaceIndexToLuid", reinterpret_cast<void*>(ConvertInterfaceIndexToLuid));
    ldr.registerExport("iphlpapi.dll", "ConvertInterfaceLuidToIndex", reinterpret_cast<void*>(ConvertInterfaceLuidToIndex));
    ldr.registerExport("iphlpapi.dll", "ConvertInterfaceLuidToGuid", reinterpret_cast<void*>(ConvertInterfaceLuidToGuid));
    ldr.registerExport("iphlpapi.dll", "ConvertInterfaceLuidToNameW", reinterpret_cast<void*>(ConvertInterfaceLuidToNameW));
    ldr.registerExport("iphlpapi.dll", "NotifyUnicastIpAddressChange", reinterpret_cast<void*>(NotifyUnicastIpAddressChange));
    ldr.registerExport("iphlpapi.dll", "CancelMibChangeNotify2", reinterpret_cast<void*>(CancelMibChangeNotify2));
}

inline void InitializeIpHelperApi() {
    InitializeIpHlpApiSubsystemExports();
}

} // namespace micant::iphlpapi
