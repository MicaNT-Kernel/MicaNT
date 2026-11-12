#pragma once

/**
 * @file ws2_32.hpp
 * @brief Clean-Room Windows Sockets 2 (ws2_32.dll) Bridge & Transport Engine.
 *
 * Implements standard Win32 Winsock 2 APIs backed by the MicaNT kernel TCP/IP stack:
 * - Socket creation, binding, listening, connecting, and closing
 * - TCP streaming (send / recv) and UDP datagrams (sendto / recvfrom)
 * - Network byte order conversions (htons, ntohs, htonl, ntohl)
 * - Address translation (inet_addr, inet_ntoa, inet_pton, inet_ntop)
 *
 * References: Microsoft Learn Windows Sockets 2 (Winsock) & POSIX Sockets.
 */

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include "micant/shared/ntdef.hpp"
#include "micant/um/subsystems/winbase/kernel32.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/native/ntdll.hpp"
#include "micant/shared/afd_socket_abi.hpp"

namespace micant::ws2_32 {

using SOCKET = uintptr_t;
inline constexpr SOCKET INVALID_SOCKET = static_cast<SOCKET>(~0ULL);
inline constexpr int SOCKET_ERROR = -1;

// Standard Address Families
inline constexpr int AF_UNSPEC = 0;
inline constexpr int AF_INET   = 2;
inline constexpr int AF_INET6  = 23;

// Socket Types
inline constexpr int SOCK_STREAM = 1;
inline constexpr int SOCK_DGRAM  = 2;
inline constexpr int SOCK_RAW    = 3;

// Protocols
inline constexpr int IPPROTO_IP   = 0;
inline constexpr int IPPROTO_ICMP = 1;
inline constexpr int IPPROTO_TCP  = 6;
inline constexpr int IPPROTO_UDP  = 17;

// Special IPv4 Addresses
inline constexpr uint32_t INADDR_ANY       = 0x00000000;
inline constexpr uint32_t INADDR_LOOPBACK  = 0x7F000001; // 127.0.0.1 in host order (0x0100007F net)
inline constexpr uint32_t INADDR_BROADCAST = 0xFFFFFFFF;
inline constexpr uint32_t INADDR_NONE      = 0xFFFFFFFF;

// Error Codes
inline constexpr int WSAEWOULDBLOCK     = 10035;
inline constexpr int WSAEINVAL          = 10022;
inline constexpr int WSAENOTSOCK        = 10038;
inline constexpr int WSAECONNREFUSED    = 10061;
inline constexpr int WSAETIMEDOUT       = 10060;
inline constexpr int WSAECONNRESET      = 10054;
inline constexpr int WSAEOPNOTSUPP      = 10045;
inline constexpr int WSAEACCES          = 10013;
inline constexpr int WSAENOBUFS         = 10055;
inline constexpr int WSAEADDRINUSE      = 10048;
inline constexpr int WSANOTINITIALISED  = 10093;
inline constexpr int SOL_SOCKET = 0xFFFF;
inline constexpr int SO_RCVTIMEO = 0x1006;

#pragma pack(push, 1)

struct in_addr {
    union {
        struct { uint8_t s_b1, s_b2, s_b3, s_b4; } S_un_b;
        struct { uint16_t s_w1, s_w2; } S_un_w;
        uint32_t S_addr;
    } S_un;
};

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

struct sockaddr_in {
    int16_t  sin_family; // AF_INET
    uint16_t sin_port;   // Network byte order
    in_addr  sin_addr;   // IPv4 Address
    char     sin_zero[8]{0};
};

struct in6_addr {
    uint8_t Byte[16];
};

struct sockaddr_in6 {
    int16_t  sin6_family;   // AF_INET6
    uint16_t sin6_port;     // Transport level port
    uint32_t sin6_flowinfo; // IPv6 flow information
    in6_addr sin6_addr;     // IPv6 address
    uint32_t sin6_scope_id; // Set of interfaces for a scope
};

struct WSABUF {
    uint32_t len;
    char*    buf;
};
using LPWSABUF = WSABUF*;

struct WSAPOLLFD {
    SOCKET  fd;
    int16_t events;
    int16_t revents;
};
using LPWSAPOLLFD = WSAPOLLFD*;

inline constexpr int16_t POLLRDNORM = 0x0100;
inline constexpr int16_t POLLRDBAND = 0x0200;
inline constexpr int16_t POLLIN     = (POLLRDNORM | POLLRDBAND);
inline constexpr int16_t POLLPRI    = 0x0400;
inline constexpr int16_t POLLWRNORM = 0x0010;
inline constexpr int16_t POLLOUT    = (POLLWRNORM);
inline constexpr int16_t POLLERR    = 0x0001;
inline constexpr int16_t POLLHUP    = 0x0002;
inline constexpr int16_t POLLNVAL   = 0x0004;

// WSA Event Selection Bitmasks
inline constexpr long FD_READ       = 0x01;
inline constexpr long FD_WRITE      = 0x02;
inline constexpr long FD_OOB        = 0x04;
inline constexpr long FD_ACCEPT     = 0x08;
inline constexpr long FD_CONNECT    = 0x10;
inline constexpr long FD_CLOSE      = 0x20;
inline constexpr long FD_QOS        = 0x40;
inline constexpr long FD_GROUP_QOS  = 0x80;
inline constexpr long FD_ROUTING_INTERFACE_CHANGE = 0x100;
inline constexpr long FD_ADDRESS_LIST_CHANGE      = 0x200;

struct WSAOVERLAPPED {
    uintptr_t Internal;
    uintptr_t InternalHigh;
    union {
        struct { uint32_t Offset; uint32_t OffsetHigh; };
        void* Pointer;
    };
    win32::HANDLE hEvent;
};
using LPWSAOVERLAPPED = WSAOVERLAPPED*;

#pragma pack(pop)

struct WSADATA {
    uint16_t wVersion{0x0202};
    uint16_t wHighVersion{0x0202};
    char szDescription[257]{"MicaNT Clean-Room Sockets 2.2"};
    char szSystemStatus[129]{"Running"};
    uint16_t iMaxSockets{32767};
    uint16_t iMaxUdpDg{65467};
    char* lpVendorInfo{nullptr};
};

inline thread_local int g_WsaLastError = 0;
inline uint16_t htons(uint16_t value) noexcept { return static_cast<uint16_t>((value << 8) | (value >> 8)); }
inline uint16_t ntohs(uint16_t value) noexcept { return htons(value); }
inline uint32_t htonl(uint32_t value) noexcept { return __builtin_bswap32(value); }
inline uint32_t ntohl(uint32_t value) noexcept { return htonl(value); }

inline int afdError(micant::NtStatus status) noexcept {
    if (status == micant::NtStatus::Success) return 0;
    if (status == micant::NtStatus::InvalidHandle || status == micant::NtStatus::HandlesClosed) return WSAENOTSOCK;
    if (status == micant::NtStatus::PortConnectionRefused) return WSAECONNREFUSED;
    if (status == micant::NtStatus::Conflict) return WSAEADDRINUSE;
    if (status == micant::NtStatus::InsufficientResources || status == micant::NtStatus::NoMemory) return WSAENOBUFS;
    if (status == micant::NtStatus::NotSupported || status == micant::NtStatus::InvalidDeviceRequest) return WSAEOPNOTSUPP;
    if (status == micant::NtStatus::AccessDenied) return WSAEACCES;
    if (status == micant::NtStatus::InvalidParameter) return WSAEINVAL;
    if (status == micant::NtStatus::Timeout) return WSAETIMEDOUT;
    return WSAECONNRESET;
}

inline micant::NtStatus afdControl(SOCKET socket, uint32_t code, const void* input,
                                   uint32_t inputLength, void* output = nullptr,
                                   uint32_t outputLength = 0,
                                   micant::IoStatusBlock* result = nullptr) noexcept {
    micant::IoStatusBlock local{};
    auto status = ntdll::NtDeviceIoControlFile(static_cast<micant::Handle>(socket), 0, nullptr, nullptr,
        &local, code, input, inputLength, output, outputLength);
    if (result) *result = local;
    return status;
}

inline micant::NtStatus queryNetworkConfiguration(
    SOCKET socket, micant::shared::afd::NetworkConfigurationV1& configuration) noexcept {
    using namespace micant::shared::afd;
    IoStatusBlock iosb{};
    NetworkConfigurationV1 result{};
    const auto status = afdControl(socket, IoctlQueryNetworkConfiguration, nullptr, 0,
                                   &result, sizeof(result), &iosb);
    if (status != micant::NtStatus::Success) return status;
    if (iosb.information != sizeof(result) || result.Size != sizeof(result) ||
        result.Version != EndpointAbiVersion || result.Reserved[0] || result.Reserved[1] ||
        (result.Flags & ~(NetworkConfigurationAddressValid | NetworkConfigurationDnsValid)))
        return micant::NtStatus::Unsuccessful;
    configuration = result;
    return micant::NtStatus::Success;
}

inline micant::NtStatus setNetworkConfiguration(
    SOCKET socket, const micant::shared::afd::NetworkConfigurationV1& configuration) noexcept {
    using namespace micant::shared::afd;
    if (configuration.Size != sizeof(configuration) || configuration.Version != EndpointAbiVersion ||
        configuration.Flags != NetworkConfigurationSet || configuration.Reserved[0] || configuration.Reserved[1])
        return micant::NtStatus::InvalidParameter;
    return afdControl(socket, IoctlSetNetworkConfiguration, &configuration,
                      sizeof(configuration));
}

inline micant::NtStatus queryNetworkConfiguration(
    SOCKET socket, micant::shared::afd::NetworkConfigurationV1* configuration) noexcept {
    if (!configuration) return micant::NtStatus::InvalidParameter;
    return queryNetworkConfiguration(socket, *configuration);
}

inline int WSAStartup(uint16_t /*wVersionRequired*/, WSADATA* lpWSAData) noexcept {
    if (lpWSAData) *lpWSAData = WSADATA{};
    return 0;
}
inline int WSACleanup() noexcept { return 0; }
inline int WSAGetLastError() noexcept { return g_WsaLastError; }
inline void WSASetLastError(int error) noexcept { g_WsaLastError = error; }
inline int unsupportedSocket() noexcept { g_WsaLastError = WSAEOPNOTSUPP; return SOCKET_ERROR; }

inline SOCKET socket(int af, int type, int protocol) noexcept {
    using namespace micant::shared::afd;
    if (af != AF_INET || (type != SOCK_STREAM && type != SOCK_DGRAM) ||
        (protocol && !((type == SOCK_STREAM && protocol == IPPROTO_TCP) || (type == SOCK_DGRAM && protocol == IPPROTO_UDP)))) {
        g_WsaLastError = WSAEINVAL; return INVALID_SOCKET;
    }
    CreateEndpointV1 create{EndpointMagic, EndpointAbiVersion, sizeof(CreateEndpointV1),
        static_cast<uint32_t>(af), static_cast<uint32_t>(type), static_cast<uint32_t>(protocol), 0};
    constexpr size_t eaSize = offsetof(FullEaInformationV1, EaName) + EndpointEaNameLength + 1 + sizeof(create);
    uint8_t ea[eaSize]{};
    auto* header = reinterpret_cast<FullEaInformationV1*>(ea);
    header->NextEntryOffset = 0; header->Flags = 0;
    header->EaNameLength = EndpointEaNameLength; header->EaValueLength = sizeof(create);
    std::memcpy(ea + offsetof(FullEaInformationV1, EaName), EndpointEaName, EndpointEaNameLength);
    std::memcpy(ea + offsetof(FullEaInformationV1, EaName) + EndpointEaNameLength + 1, &create, sizeof(create));
    constexpr wchar_t endpointPath[] = L"\\Device\\Afd\\Endpoint";
    UnicodeString name{endpointPath};
    ObjectAttributes oa{}; oa.objectName = &name; oa.attributes = OBJ_CASE_INSENSITIVE;
    Handle handle{}; IoStatusBlock iosb{};
    const auto status = ntdll::NtCreateFile(&handle, 0x00100003u, &oa, &iosb, nullptr, 0, 0, 1, 0, ea, sizeof(ea));
    if (status != NtStatus::Success) { g_WsaLastError = afdError(status); return INVALID_SOCKET; }
    return static_cast<SOCKET>(handle);
}

inline int bind(SOCKET s, const sockaddr* name, int namelen) noexcept {
    if (s == INVALID_SOCKET || !name || namelen < static_cast<int>(sizeof(sockaddr_in))) { g_WsaLastError=WSAEINVAL; return SOCKET_ERROR; }
    const auto* in=reinterpret_cast<const sockaddr_in*>(name);
    if (in->sin_family != AF_INET) { g_WsaLastError=WSAEINVAL; return SOCKET_ERROR; }
    micant::shared::afd::SocketAddressV1 address{sizeof(address), AF_INET, in->sin_addr.S_un.S_addr, ntohs(in->sin_port), 0};
    auto st=afdControl(s,micant::shared::afd::IoctlBind,&address,sizeof(address));
    if (st!=NtStatus::Success) { g_WsaLastError=afdError(st); return SOCKET_ERROR; } return 0;
}
inline int listen(SOCKET s, int backlog) noexcept {
    if (s==INVALID_SOCKET || backlog<0) { g_WsaLastError=WSAEINVAL; return SOCKET_ERROR; }
    const auto st=afdControl(s,micant::shared::afd::IoctlListen,&backlog,sizeof(backlog));
    if (st!=NtStatus::Success) { g_WsaLastError=afdError(st); return SOCKET_ERROR; } return 0;
}
inline SOCKET accept(SOCKET, sockaddr*, int*) noexcept { g_WsaLastError=WSAEOPNOTSUPP; return INVALID_SOCKET; }
inline int connect(SOCKET s,const sockaddr* name,int namelen) noexcept {
    if(s==INVALID_SOCKET||!name||namelen<static_cast<int>(sizeof(sockaddr_in))){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}
    const auto* in=reinterpret_cast<const sockaddr_in*>(name);
    if(in->sin_family!=AF_INET){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}
    micant::shared::afd::SocketAddressV1 address{sizeof(address),AF_INET,in->sin_addr.S_un.S_addr,ntohs(in->sin_port),0};
    auto st=afdControl(s,micant::shared::afd::IoctlConnect,&address,sizeof(address));
    if(st!=NtStatus::Success){g_WsaLastError=afdError(st);return SOCKET_ERROR;}return 0;
}
inline int send(SOCKET s,const char* buf,int len,int flags) noexcept {
    using namespace micant::shared::afd;
    if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}
    if(flags!=0||(!buf&&len)||len<0||static_cast<uint32_t>(len)>MaximumControlBuffer-sizeof(DataRequestV1)){g_WsaLastError=flags?WSAEOPNOTSUPP:WSAEINVAL;return SOCKET_ERROR;}
    try {
        std::vector<uint8_t> request(sizeof(DataRequestV1)+static_cast<size_t>(len));
        DataRequestV1 header{sizeof(DataRequestV1),0,static_cast<uint32_t>(len),0}; std::memcpy(request.data(),&header,sizeof(header));
        if(len) std::memcpy(request.data()+sizeof(header),buf,static_cast<size_t>(len));
        IoStatusBlock iosb{}; auto st=afdControl(s,IoctlSend,request.data(),request.size(),nullptr,0,&iosb);
        if(st!=NtStatus::Success){g_WsaLastError=afdError(st);return SOCKET_ERROR;} return static_cast<int>(iosb.information);
    } catch (...) { g_WsaLastError=WSAENOBUFS; return SOCKET_ERROR; }
}
inline int recv(SOCKET s,char* buf,int len,int flags) noexcept {
    using namespace micant::shared::afd;
    if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}
    if(flags!=0||(!buf&&len)||len<0||static_cast<uint32_t>(len)>MaximumControlBuffer){g_WsaLastError=flags?WSAEOPNOTSUPP:WSAEINVAL;return SOCKET_ERROR;}
    if(len==0)return 0;
    DataRequestV1 request{sizeof(DataRequestV1),0,static_cast<uint32_t>(len),0}; IoStatusBlock iosb{};
    auto st=afdControl(s,IoctlReceive,&request,sizeof(request),buf,static_cast<uint32_t>(len),&iosb);
    if(st!=NtStatus::Success){g_WsaLastError=afdError(st);return SOCKET_ERROR;} return static_cast<int>(iosb.information);
}
inline int sendto(SOCKET s,const char* buf,int len,int flags,const sockaddr* to,int tolen) noexcept {
    using namespace micant::shared::afd;
    if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}
    if(flags!=0){g_WsaLastError=WSAEOPNOTSUPP;return SOCKET_ERROR;}
    if((!buf&&len)||len<0||!to||tolen<static_cast<int>(sizeof(sockaddr_in))||static_cast<uint32_t>(len)>MaximumControlBuffer-sizeof(DataRequestV1)-sizeof(SocketAddressV1)){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}
    const auto* in=reinterpret_cast<const sockaddr_in*>(to); if(in->sin_family!=AF_INET){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}
    try {
        std::vector<uint8_t> request(sizeof(DataRequestV1)+sizeof(SocketAddressV1)+static_cast<size_t>(len));
        DataRequestV1 header{sizeof(DataRequestV1),0,static_cast<uint32_t>(len),0};
        SocketAddressV1 address{sizeof(address),AF_INET,in->sin_addr.S_un.S_addr,ntohs(in->sin_port),0};
        std::memcpy(request.data(),&header,sizeof(header));std::memcpy(request.data()+sizeof(header),&address,sizeof(address));if(len)std::memcpy(request.data()+sizeof(header)+sizeof(address),buf,len);
        IoStatusBlock iosb{};auto st=afdControl(s,IoctlSendTo,request.data(),request.size(),nullptr,0,&iosb);
        if(st!=NtStatus::Success){g_WsaLastError=afdError(st);return SOCKET_ERROR;}return static_cast<int>(iosb.information);
    } catch (...) { g_WsaLastError=WSAENOBUFS; return SOCKET_ERROR; }
}
inline int recvfrom(SOCKET s,char* buf,int len,int flags,sockaddr* from,int* fromlen) noexcept {
    using namespace micant::shared::afd;
    if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}
    if(flags!=0||(!buf&&len)||len<0||static_cast<uint32_t>(len)>MaximumControlBuffer-sizeof(ReceiveFromResultV1)){
        g_WsaLastError=flags?WSAEOPNOTSUPP:WSAEINVAL;return SOCKET_ERROR;
    }
    if(from&&!fromlen){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}
    if(len==0)return 0;
    try {
        std::vector<uint8_t> output(sizeof(ReceiveFromResultV1)+static_cast<size_t>(len));
        DataRequestV1 request{sizeof(request),0,static_cast<uint32_t>(len),0}; IoStatusBlock iosb{};
        auto status=afdControl(s,IoctlReceiveFrom,&request,sizeof(request),output.data(),output.size(),&iosb);
        if(status!=NtStatus::Success){g_WsaLastError=afdError(status);return SOCKET_ERROR;}
        if(iosb.information<sizeof(ReceiveFromResultV1)){g_WsaLastError=WSAECONNRESET;return SOCKET_ERROR;}
        ReceiveFromResultV1 result{};std::memcpy(&result,output.data(),sizeof(result));
        if(result.Source.Size!=sizeof(SocketAddressV1)||result.Source.AddressFamily!=AddressFamilyInet||
           result.DataLength>static_cast<uint32_t>(len)||iosb.information!=sizeof(result)+result.DataLength){
            g_WsaLastError=WSAECONNRESET;return SOCKET_ERROR;
        }
        std::memcpy(buf,output.data()+sizeof(result),result.DataLength);
        if(from){
            if(*fromlen<static_cast<int>(sizeof(sockaddr_in))){*fromlen=sizeof(sockaddr_in);g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}
            sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.S_un.S_addr=result.Source.Ipv4AddressNetworkOrder;
            address.sin_port=htons(result.Source.PortHostOrder);std::memcpy(from,&address,sizeof(address));*fromlen=sizeof(address);
        } else if(fromlen) *fromlen=0;
        return static_cast<int>(result.DataLength);
    } catch (...) {g_WsaLastError=WSAENOBUFS;return SOCKET_ERROR;}
}
inline int closesocket(SOCKET s) noexcept {
    if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}
    const auto status=ntdll::NtClose(static_cast<Handle>(s));
    if(status!=NtStatus::Success){g_WsaLastError=afdError(status);return SOCKET_ERROR;}return 0;
}

inline int gethostname(char*, int) noexcept { return unsupportedSocket(); }

inline uint32_t inet_addr(const char* cp) noexcept {
    if (!cp) return INADDR_NONE;
    uint32_t octets[4]{}; const char* cursor=cp;
    for (unsigned i=0;i<4;++i) {
        if (*cursor<'0'||*cursor>'9') return INADDR_NONE;
        unsigned value=0;
        do { value=value*10u+static_cast<unsigned>(*cursor-'0'); if(value>255) return INADDR_NONE; ++cursor; } while(*cursor>='0'&&*cursor<='9');
        octets[i]=value;
        if(i<3) { if(*cursor++!='.') return INADDR_NONE; }
    }
    if(*cursor!='\0') return INADDR_NONE;
    return htonl((octets[0]<<24)|(octets[1]<<16)|(octets[2]<<8)|octets[3]);
}

inline char* inet_ntoa(in_addr in) noexcept {
    static thread_local char buf[32];
    const uint32_t address=ntohl(in.S_un.S_addr);
    std::snprintf(buf,sizeof(buf),"%u.%u.%u.%u",static_cast<unsigned>((address>>24)&255),
        static_cast<unsigned>((address>>16)&255),static_cast<unsigned>((address>>8)&255),static_cast<unsigned>(address&255));
    return buf;
}

inline int getpeername(SOCKET, sockaddr*, int*) noexcept { g_WsaLastError=WSAEOPNOTSUPP; return SOCKET_ERROR; }
inline int getsockname(SOCKET, sockaddr*, int*) noexcept { g_WsaLastError=WSAEOPNOTSUPP; return SOCKET_ERROR; }
inline int setsockopt(SOCKET s,int level,int option,const char* value,int valueLength) noexcept {
    if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}
    if(level!=SOL_SOCKET||option!=SO_RCVTIMEO){g_WsaLastError=WSAEOPNOTSUPP;return SOCKET_ERROR;}
    if(!value||valueLength!=static_cast<int>(sizeof(uint32_t))){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}
    uint32_t timeout{};std::memcpy(&timeout,value,sizeof(timeout));
    if(timeout>60000){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}
    auto status=afdControl(s,micant::shared::afd::IoctlSetReceiveTimeout,&timeout,sizeof(timeout));
    if(status!=NtStatus::Success){g_WsaLastError=afdError(status);return SOCKET_ERROR;}return 0;
}
inline int shutdown(SOCKET, int) noexcept { g_WsaLastError=WSAEOPNOTSUPP; return SOCKET_ERROR; }

struct hostent {
    char*  h_name;
    char** h_aliases;
    int16_t h_addrtype;
    int16_t h_length;
    char** h_addr_list;
};

inline hostent* gethostbyname(const char*) noexcept { g_WsaLastError=WSAEOPNOTSUPP; return nullptr; }
inline int WSAAsyncSelect(SOCKET,void*,unsigned int,long) noexcept { return unsupportedSocket(); }
inline win32::HANDLE WINAPI WSACreateEvent() noexcept { return win32::CreateEventW(nullptr,1,0,nullptr); }
inline win32::BOOL WINAPI WSACloseEvent(win32::HANDLE e) noexcept { return win32::CloseHandle(e); }
inline win32::BOOL WINAPI WSASetEvent(win32::HANDLE e) noexcept { return win32::SetEvent(e); }
inline win32::DWORD WINAPI WSAWaitForMultipleEvents(win32::DWORD n,const win32::HANDLE* e,win32::BOOL all,win32::DWORD ms,win32::BOOL alertable) noexcept {
    return win32::WaitForMultipleObjects(n,e,all,ms);
}
inline win32::HANDLE WINAPI WSAAsyncGetHostByName(win32::HWND,unsigned int,const char*,char*,int) noexcept {g_WsaLastError=WSAEOPNOTSUPP;return nullptr;}
inline int WINAPI WSACancelAsyncRequest(win32::HANDLE) noexcept {return unsupportedSocket();}
inline int WINAPI WSAEnumNetworkEvents(SOCKET,win32::HANDLE,void*) noexcept {return unsupportedSocket();}
inline int WINAPI WSAEventSelect(SOCKET,win32::HANDLE,long) noexcept {return unsupportedSocket();}
inline int WINAPI WSAPoll(WSAPOLLFD*,uint32_t,int) noexcept {return unsupportedSocket();}
inline void* WINAPI getservbyname(const char*,const char*) noexcept {g_WsaLastError=WSAEOPNOTSUPP;return nullptr;}
inline int WINAPI getsockopt(SOCKET,int,int,char*,int*) noexcept {return unsupportedSocket();}
inline int WINAPI ioctlsocket(SOCKET,long,uint32_t*) noexcept {return unsupportedSocket();}
inline int WINAPI select(int,void*,void*,void*,const void*) noexcept {return unsupportedSocket();}
inline int WINAPI __WSAFDIsSet(SOCKET,void*) noexcept {g_WsaLastError=WSAEOPNOTSUPP;return 0;}
inline const char* WINAPI inet_ntop(int af,const void* src,char* dst,size_t size) noexcept {
    if(af!=AF_INET||!src||!dst||size<16){g_WsaLastError=WSAEINVAL;return nullptr;}
    in_addr addr{};std::memcpy(&addr,src,sizeof(addr));std::snprintf(dst,size,"%s",inet_ntoa(addr));return dst;
}
inline int WINAPI inet_pton(int af,const char* src,void* dst) noexcept {
    if(af!=AF_INET||!src||!dst){g_WsaLastError=WSAEINVAL;return -1;}
    const uint32_t address=inet_addr(src);if(address==INADDR_NONE&&std::strcmp(src,"255.255.255.255")!=0)return 0;
    std::memcpy(dst,&address,sizeof(address));return 1;
}
inline int WINAPI WSAConnect(SOCKET s,const void* name,int length,void*,void*,void*,void*) noexcept {return connect(s,static_cast<const sockaddr*>(name),length);}
inline SOCKET WINAPI WSAAccept(SOCKET,sockaddr*,int*,void*,uintptr_t) noexcept {g_WsaLastError=WSAEOPNOTSUPP;return INVALID_SOCKET;}
inline int WINAPI WSAHtonl(SOCKET s,uint32_t value,uint32_t* out) noexcept {if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}if(!out){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}*out=htonl(value);return 0;}
inline int WINAPI WSANtohl(SOCKET s,uint32_t value,uint32_t* out) noexcept {if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}if(!out){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}*out=ntohl(value);return 0;}
inline int WINAPI WSANtohs(SOCKET s,uint16_t value,uint16_t* out) noexcept {if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}if(!out){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}*out=ntohs(value);return 0;}
inline int WINAPI WSASend(SOCKET s,void* buffers,uint32_t count,uint32_t* sent,uint32_t flags,void* ov,void* completion) noexcept {
    if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}
    if(ov||completion||flags||(count&&!buffers))return unsupportedSocket();const auto* list=static_cast<const WSABUF*>(buffers);uint64_t total=0;
    for(uint32_t i=0;i<count;++i){if(list[i].len>INT32_MAX)return unsupportedSocket();int n=send(s,list[i].buf,static_cast<int>(list[i].len),flags);if(n==SOCKET_ERROR)return SOCKET_ERROR;total+=static_cast<uint32_t>(n);if(static_cast<uint32_t>(n)!=list[i].len)break;}
    if(sent)*sent=static_cast<uint32_t>(total);return 0;
}
inline int WINAPI WSARecv(SOCKET s,void* buffers,uint32_t count,uint32_t* received,uint32_t* flags,void* ov,void* completion) noexcept {
    if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}
    if(ov||completion||(flags&&*flags)||(count&&!buffers))return unsupportedSocket();auto* list=static_cast<WSABUF*>(buffers);uint64_t total=0;
    for(uint32_t i=0;i<count;++i){if(list[i].len>INT32_MAX)return unsupportedSocket();int n=recv(s,list[i].buf,static_cast<int>(list[i].len),0);if(n==SOCKET_ERROR)return SOCKET_ERROR;total+=static_cast<uint32_t>(n);if(static_cast<uint32_t>(n)!=list[i].len)break;}
    if(received)*received=static_cast<uint32_t>(total);if(flags)*flags=0;return 0;
}
inline int WINAPI WSASendTo(SOCKET s,void* buffers,uint32_t count,uint32_t* sent,uint32_t flags,const void* to,int len,void* ov,void* completion) noexcept {
    if(s==INVALID_SOCKET){g_WsaLastError=WSAENOTSOCK;return SOCKET_ERROR;}
    if(ov||completion||count!=1||!buffers)return unsupportedSocket();const auto* b=static_cast<const WSABUF*>(buffers);
    if(b->len>INT32_MAX){g_WsaLastError=WSAEINVAL;return SOCKET_ERROR;}
    int n=sendto(s,b->buf,static_cast<int>(b->len),flags,static_cast<const sockaddr*>(to),len);if(n==SOCKET_ERROR)return SOCKET_ERROR;if(sent)*sent=static_cast<uint32_t>(n);return 0;
}
inline int WINAPI WSARecvFrom(SOCKET s,LPWSABUF buffers,uint32_t bufferCount,uint32_t* bytesReceived,
    uint32_t* flags,sockaddr* from,int* fromLength,void* overlapped,void* completionRoutine) noexcept {
    if (overlapped || completionRoutine || !bytesReceived || !flags || !buffers || bufferCount != 1)
        return unsupportedSocket();
    if (*flags != 0) { g_WsaLastError=WSAEOPNOTSUPP; return SOCKET_ERROR; }
    if (buffers[0].len > 0x7FFFFFFFu) { g_WsaLastError=WSAEINVAL; return SOCKET_ERROR; }
    const int result=recvfrom(s,buffers[0].buf,static_cast<int>(buffers[0].len),0,from,fromLength);
    if(result==SOCKET_ERROR)return result;
    *bytesReceived=static_cast<uint32_t>(result);*flags=0;return 0;
}
inline int WINAPI WSAIoctl(SOCKET,uint32_t,void*,uint32_t,void*,uint32_t,uint32_t*,void*,void*) noexcept{return unsupportedSocket();}
inline SOCKET WINAPI WSASocketA(int af,int type,int protocol,void*,uint32_t,uint32_t) noexcept{return socket(af,type,protocol);}
inline SOCKET WINAPI WSASocketW(int af,int type,int protocol,void*,uint32_t,uint32_t) noexcept{return socket(af,type,protocol);}
struct MicaAddrInfo {int ai_flags;int ai_family;int ai_socktype;int ai_protocol;size_t ai_addrlen;char* ai_canonname;void* ai_addr;MicaAddrInfo* ai_next;};
inline int WINAPI getaddrinfo(const char*,const char*,const void*,void** result) noexcept {if(result)*result=nullptr;g_WsaLastError=WSAEOPNOTSUPP;return WSAEOPNOTSUPP;}
inline void WINAPI freeaddrinfo(void* value) noexcept {auto* ai=static_cast<MicaAddrInfo*>(value);while(ai){auto* next=ai->ai_next;delete[] static_cast<uint8_t*>(ai->ai_addr);delete ai;ai=next;}}
inline int WINAPI getnameinfo(const void*,int,char*,uint32_t,char*,uint32_t,int) noexcept {return WSAEOPNOTSUPP;}
inline int WINAPI WSAStringToAddressW(wchar_t*,int,void*,void*,int*) noexcept{return unsupportedSocket();}
inline int WINAPI WSAAddressToStringW(void*,uint32_t,void*,wchar_t*,uint32_t*) noexcept{return unsupportedSocket();}
inline win32::BOOL WINAPI AcceptEx(SOCKET,SOCKET,void*,uint32_t,uint32_t,uint32_t,uint32_t*,void*) noexcept {g_WsaLastError=WSAEOPNOTSUPP;return 0;}
inline void WINAPI GetAcceptExSockaddrs(void*,uint32_t,uint32_t,uint32_t,void** local,int* localLength,void** remote,int* remoteLength) noexcept {if(local)*local=nullptr;if(localLength)*localLength=0;if(remote)*remote=nullptr;if(remoteLength)*remoteLength=0;}

inline void InitializeWs2_32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // ws2_32.dll Named Exports
    ldr.registerExport("ws2_32.dll", "WSAStartup", reinterpret_cast<void*>(WSAStartup));
    ldr.registerExport("ws2_32.dll", "WSACleanup", reinterpret_cast<void*>(WSACleanup));
    ldr.registerExport("ws2_32.dll", "WSAGetLastError", reinterpret_cast<void*>(WSAGetLastError));
    ldr.registerExport("ws2_32.dll", "WSASetLastError", reinterpret_cast<void*>(WSASetLastError));
    ldr.registerExport("ws2_32.dll", "socket", reinterpret_cast<void*>(socket));
    ldr.registerExport("ws2_32.dll", "bind", reinterpret_cast<void*>(bind));
    ldr.registerExport("ws2_32.dll", "listen", reinterpret_cast<void*>(listen));
    ldr.registerExport("ws2_32.dll", "accept", reinterpret_cast<void*>(accept));
    ldr.registerExport("ws2_32.dll", "connect", reinterpret_cast<void*>(connect));
    ldr.registerExport("ws2_32.dll", "send", reinterpret_cast<void*>(send));
    ldr.registerExport("ws2_32.dll", "recv", reinterpret_cast<void*>(recv));
    ldr.registerExport("ws2_32.dll", "sendto", reinterpret_cast<void*>(sendto));
    ldr.registerExport("ws2_32.dll", "recvfrom", reinterpret_cast<void*>(recvfrom));
    ldr.registerExport("ws2_32.dll", "closesocket", reinterpret_cast<void*>(closesocket));
    ldr.registerExport("ws2_32.dll", "gethostname", reinterpret_cast<void*>(gethostname));
    ldr.registerExport("ws2_32.dll", "inet_addr", reinterpret_cast<void*>(inet_addr));
    ldr.registerExport("ws2_32.dll", "inet_ntoa", reinterpret_cast<void*>(inet_ntoa));
    ldr.registerExport("ws2_32.dll", "htons", reinterpret_cast<void*>(htons));
    ldr.registerExport("ws2_32.dll", "ntohs", reinterpret_cast<void*>(ntohs));
    ldr.registerExport("ws2_32.dll", "htonl", reinterpret_cast<void*>(htonl));
    ldr.registerExport("ws2_32.dll", "ntohl", reinterpret_cast<void*>(ntohl));
    ldr.registerExport("ws2_32.dll", "getpeername", reinterpret_cast<void*>(getpeername));
    ldr.registerExport("ws2_32.dll", "getsockname", reinterpret_cast<void*>(getsockname));
    ldr.registerExport("ws2_32.dll", "setsockopt", reinterpret_cast<void*>(setsockopt));
    ldr.registerExport("ws2_32.dll", "shutdown", reinterpret_cast<void*>(shutdown));
    ldr.registerExport("ws2_32.dll", "gethostbyname", reinterpret_cast<void*>(gethostbyname));
    ldr.registerExport("ws2_32.dll", "WSAAsyncSelect", reinterpret_cast<void*>(WSAAsyncSelect));
    ldr.registerExport("ws2_32.dll", "select", reinterpret_cast<void*>(select));
    ldr.registerExport("ws2_32.dll", "ioctlsocket", reinterpret_cast<void*>(ioctlsocket));
    ldr.registerExport("ws2_32.dll", "getsockopt", reinterpret_cast<void*>(getsockopt));
    ldr.registerExport("ws2_32.dll", "__WSAFDIsSet", reinterpret_cast<void*>(__WSAFDIsSet));
    ldr.registerExport("ws2_32.dll", "AcceptEx", reinterpret_cast<void*>(AcceptEx));
    ldr.registerExport("ws2_32.dll", "GetAcceptExSockaddrs", reinterpret_cast<void*>(GetAcceptExSockaddrs));
    ldr.registerExport("ws2_32.dll", "WSACreateEvent", reinterpret_cast<void*>(WSACreateEvent));
    ldr.registerExport("ws2_32.dll", "WSACloseEvent", reinterpret_cast<void*>(WSACloseEvent));
    ldr.registerExport("ws2_32.dll", "WSASetEvent", reinterpret_cast<void*>(WSASetEvent));
    ldr.registerExport("ws2_32.dll", "WSAWaitForMultipleEvents", reinterpret_cast<void*>(WSAWaitForMultipleEvents));
    ldr.registerExport("ws2_32.dll", "WSAConnect", reinterpret_cast<void*>(WSAConnect));
    ldr.registerExport("ws2_32.dll", "WSAAccept", reinterpret_cast<void*>(WSAAccept));
    ldr.registerExport("ws2_32.dll", "WSAHtonl", reinterpret_cast<void*>(WSAHtonl));
    ldr.registerExport("ws2_32.dll", "WSANtohl", reinterpret_cast<void*>(WSANtohl));
    ldr.registerExport("ws2_32.dll", "WSANtohs", reinterpret_cast<void*>(WSANtohs));
    ldr.registerExport("ws2_32.dll", "WSASend", reinterpret_cast<void*>(WSASend));
    ldr.registerExport("ws2_32.dll", "WSARecv", reinterpret_cast<void*>(WSARecv));
    ldr.registerExport("ws2_32.dll", "WSASendTo", reinterpret_cast<void*>(WSASendTo));
    ldr.registerExport("ws2_32.dll", "WSARecvFrom", reinterpret_cast<void*>(WSARecvFrom));
    ldr.registerExport("ws2_32.dll", "WSAIoctl", reinterpret_cast<void*>(WSAIoctl));
    ldr.registerExport("ws2_32.dll", "WSASocketA", reinterpret_cast<void*>(WSASocketA));
    ldr.registerExport("ws2_32.dll", "WSASocketW", reinterpret_cast<void*>(WSASocketW));
    ldr.registerExport("ws2_32.dll", "getaddrinfo", reinterpret_cast<void*>(getaddrinfo));
    ldr.registerExport("ws2_32.dll", "freeaddrinfo", reinterpret_cast<void*>(freeaddrinfo));
    ldr.registerExport("ws2_32.dll", "getnameinfo", reinterpret_cast<void*>(getnameinfo));
    ldr.registerExport("ws2_32.dll", "WSAStringToAddressW", reinterpret_cast<void*>(WSAStringToAddressW));
    ldr.registerExport("ws2_32.dll", "WSAAddressToStringW", reinterpret_cast<void*>(WSAAddressToStringW));
    ldr.registerExport("ws2_32.dll", "WSAAsyncGetHostByName", reinterpret_cast<void*>(WSAAsyncGetHostByName));
    ldr.registerExport("ws2_32.dll", "WSACancelAsyncRequest", reinterpret_cast<void*>(WSACancelAsyncRequest));
    ldr.registerExport("ws2_32.dll", "WSAEnumNetworkEvents", reinterpret_cast<void*>(WSAEnumNetworkEvents));
    ldr.registerExport("ws2_32.dll", "WSAEventSelect", reinterpret_cast<void*>(WSAEventSelect));
    ldr.registerExport("ws2_32.dll", "WSAPoll", reinterpret_cast<void*>(WSAPoll));
    ldr.registerExport("ws2_32.dll", "getservbyname", reinterpret_cast<void*>(getservbyname));
    ldr.registerExport("ws2_32.dll", "inet_ntop", reinterpret_cast<void*>(inet_ntop));
    ldr.registerExport("ws2_32.dll", "inet_pton", reinterpret_cast<void*>(inet_pton));

    // ws2_32.dll Numeric Ordinal Exports
    ldr.registerExportOrdinal("ws2_32.dll", 1, reinterpret_cast<void*>(accept));
    ldr.registerExportOrdinal("ws2_32.dll", 2, reinterpret_cast<void*>(bind));
    ldr.registerExportOrdinal("ws2_32.dll", 3, reinterpret_cast<void*>(closesocket));
    ldr.registerExportOrdinal("ws2_32.dll", 4, reinterpret_cast<void*>(connect));
    ldr.registerExportOrdinal("ws2_32.dll", 5, reinterpret_cast<void*>(getpeername));
    ldr.registerExportOrdinal("ws2_32.dll", 6, reinterpret_cast<void*>(getsockname));
    ldr.registerExportOrdinal("ws2_32.dll", 7, reinterpret_cast<void*>(getsockopt));
    ldr.registerExportOrdinal("ws2_32.dll", 8, reinterpret_cast<void*>(htonl));
    ldr.registerExportOrdinal("ws2_32.dll", 9, reinterpret_cast<void*>(htons));
    ldr.registerExportOrdinal("ws2_32.dll", 10, reinterpret_cast<void*>(inet_addr));
    ldr.registerExportOrdinal("ws2_32.dll", 11, reinterpret_cast<void*>(inet_ntoa));
    ldr.registerExportOrdinal("ws2_32.dll", 12, reinterpret_cast<void*>(ioctlsocket));
    ldr.registerExportOrdinal("ws2_32.dll", 13, reinterpret_cast<void*>(listen));
    ldr.registerExportOrdinal("ws2_32.dll", 14, reinterpret_cast<void*>(ntohl));
    ldr.registerExportOrdinal("ws2_32.dll", 15, reinterpret_cast<void*>(ntohs));
    ldr.registerExportOrdinal("ws2_32.dll", 16, reinterpret_cast<void*>(recv));
    ldr.registerExportOrdinal("ws2_32.dll", 17, reinterpret_cast<void*>(recvfrom));
    ldr.registerExportOrdinal("ws2_32.dll", 18, reinterpret_cast<void*>(select));
    ldr.registerExportOrdinal("ws2_32.dll", 19, reinterpret_cast<void*>(send));
    ldr.registerExportOrdinal("ws2_32.dll", 20, reinterpret_cast<void*>(sendto));
    ldr.registerExportOrdinal("ws2_32.dll", 21, reinterpret_cast<void*>(setsockopt));
    ldr.registerExportOrdinal("ws2_32.dll", 22, reinterpret_cast<void*>(shutdown));
    ldr.registerExportOrdinal("ws2_32.dll", 23, reinterpret_cast<void*>(socket));
    ldr.registerExportOrdinal("ws2_32.dll", 51, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("ws2_32.dll", 52, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("ws2_32.dll", 55, reinterpret_cast<void*>(getservbyname));
    ldr.registerExportOrdinal("ws2_32.dll", 57, reinterpret_cast<void*>(gethostname));
    ldr.registerExportOrdinal("ws2_32.dll", 101, reinterpret_cast<void*>(WSAAsyncSelect));
    ldr.registerExportOrdinal("ws2_32.dll", 102, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("ws2_32.dll", 103, reinterpret_cast<void*>(WSAAsyncGetHostByName));
    ldr.registerExportOrdinal("ws2_32.dll", 108, reinterpret_cast<void*>(WSACancelAsyncRequest));
    ldr.registerExportOrdinal("ws2_32.dll", 111, reinterpret_cast<void*>(WSAGetLastError));
    ldr.registerExportOrdinal("ws2_32.dll", 112, reinterpret_cast<void*>(WSASetLastError));
    ldr.registerExportOrdinal("ws2_32.dll", 115, reinterpret_cast<void*>(WSAStartup));
    ldr.registerExportOrdinal("ws2_32.dll", 116, reinterpret_cast<void*>(WSACleanup));
    ldr.registerExportOrdinal("ws2_32.dll", 151, reinterpret_cast<void*>(__WSAFDIsSet));
    ldr.registerExportOrdinal("ws2_32.dll", 1141, reinterpret_cast<void*>(WSAIoctl));
    ldr.registerExportOrdinal("ws2_32.dll", 1142, reinterpret_cast<void*>(WSASocketA));

    // wsock32.dll Named Exports
    ldr.registerExport("wsock32.dll", "WSAStartup", reinterpret_cast<void*>(WSAStartup));
    ldr.registerExport("wsock32.dll", "WSACleanup", reinterpret_cast<void*>(WSACleanup));
    ldr.registerExport("wsock32.dll", "WSAGetLastError", reinterpret_cast<void*>(WSAGetLastError));
    ldr.registerExport("wsock32.dll", "WSASetLastError", reinterpret_cast<void*>(WSASetLastError));
    ldr.registerExport("wsock32.dll", "socket", reinterpret_cast<void*>(socket));
    ldr.registerExport("wsock32.dll", "bind", reinterpret_cast<void*>(bind));
    ldr.registerExport("wsock32.dll", "listen", reinterpret_cast<void*>(listen));
    ldr.registerExport("wsock32.dll", "accept", reinterpret_cast<void*>(accept));
    ldr.registerExport("wsock32.dll", "connect", reinterpret_cast<void*>(connect));
    ldr.registerExport("wsock32.dll", "send", reinterpret_cast<void*>(send));
    ldr.registerExport("wsock32.dll", "recv", reinterpret_cast<void*>(recv));
    ldr.registerExport("wsock32.dll", "sendto", reinterpret_cast<void*>(sendto));
    ldr.registerExport("wsock32.dll", "recvfrom", reinterpret_cast<void*>(recvfrom));
    ldr.registerExport("wsock32.dll", "closesocket", reinterpret_cast<void*>(closesocket));
    ldr.registerExport("wsock32.dll", "gethostname", reinterpret_cast<void*>(gethostname));
    ldr.registerExport("wsock32.dll", "inet_addr", reinterpret_cast<void*>(inet_addr));
    ldr.registerExport("wsock32.dll", "inet_ntoa", reinterpret_cast<void*>(inet_ntoa));
    ldr.registerExport("wsock32.dll", "htons", reinterpret_cast<void*>(htons));
    ldr.registerExport("wsock32.dll", "ntohs", reinterpret_cast<void*>(ntohs));
    ldr.registerExport("wsock32.dll", "htonl", reinterpret_cast<void*>(htonl));
    ldr.registerExport("wsock32.dll", "ntohl", reinterpret_cast<void*>(ntohl));
    ldr.registerExport("wsock32.dll", "getpeername", reinterpret_cast<void*>(getpeername));
    ldr.registerExport("wsock32.dll", "getsockname", reinterpret_cast<void*>(getsockname));
    ldr.registerExport("wsock32.dll", "setsockopt", reinterpret_cast<void*>(setsockopt));
    ldr.registerExport("wsock32.dll", "shutdown", reinterpret_cast<void*>(shutdown));
    ldr.registerExport("wsock32.dll", "gethostbyname", reinterpret_cast<void*>(gethostbyname));
    ldr.registerExport("wsock32.dll", "WSAAsyncSelect", reinterpret_cast<void*>(WSAAsyncSelect));
    ldr.registerExport("wsock32.dll", "select", reinterpret_cast<void*>(select));
    ldr.registerExport("wsock32.dll", "ioctlsocket", reinterpret_cast<void*>(ioctlsocket));
    ldr.registerExport("wsock32.dll", "getsockopt", reinterpret_cast<void*>(getsockopt));
    ldr.registerExport("wsock32.dll", "__WSAFDIsSet", reinterpret_cast<void*>(__WSAFDIsSet));
    ldr.registerExport("wsock32.dll", "AcceptEx", reinterpret_cast<void*>(AcceptEx));
    ldr.registerExport("wsock32.dll", "GetAcceptExSockaddrs", reinterpret_cast<void*>(GetAcceptExSockaddrs));
    ldr.registerExport("wsock32.dll", "WSACreateEvent", reinterpret_cast<void*>(WSACreateEvent));
    ldr.registerExport("wsock32.dll", "WSACloseEvent", reinterpret_cast<void*>(WSACloseEvent));
    ldr.registerExport("wsock32.dll", "WSASetEvent", reinterpret_cast<void*>(WSASetEvent));
    ldr.registerExport("wsock32.dll", "WSAWaitForMultipleEvents", reinterpret_cast<void*>(WSAWaitForMultipleEvents));
    ldr.registerExport("wsock32.dll", "WSAConnect", reinterpret_cast<void*>(WSAConnect));
    ldr.registerExport("wsock32.dll", "WSAAccept", reinterpret_cast<void*>(WSAAccept));
    ldr.registerExport("wsock32.dll", "WSAHtonl", reinterpret_cast<void*>(WSAHtonl));
    ldr.registerExport("wsock32.dll", "WSANtohl", reinterpret_cast<void*>(WSANtohl));
    ldr.registerExport("wsock32.dll", "WSANtohs", reinterpret_cast<void*>(WSANtohs));
    ldr.registerExport("wsock32.dll", "WSASend", reinterpret_cast<void*>(WSASend));
    ldr.registerExport("wsock32.dll", "WSARecv", reinterpret_cast<void*>(WSARecv));
    ldr.registerExport("wsock32.dll", "WSASendTo", reinterpret_cast<void*>(WSASendTo));
    ldr.registerExport("wsock32.dll", "WSARecvFrom", reinterpret_cast<void*>(WSARecvFrom));
    ldr.registerExport("wsock32.dll", "WSAIoctl", reinterpret_cast<void*>(WSAIoctl));
    ldr.registerExport("wsock32.dll", "WSASocketA", reinterpret_cast<void*>(WSASocketA));
    ldr.registerExport("wsock32.dll", "WSASocketW", reinterpret_cast<void*>(WSASocketW));
    ldr.registerExport("wsock32.dll", "getaddrinfo", reinterpret_cast<void*>(getaddrinfo));
    ldr.registerExport("wsock32.dll", "freeaddrinfo", reinterpret_cast<void*>(freeaddrinfo));
    ldr.registerExport("wsock32.dll", "getnameinfo", reinterpret_cast<void*>(getnameinfo));
    ldr.registerExport("wsock32.dll", "WSAStringToAddressW", reinterpret_cast<void*>(WSAStringToAddressW));
    ldr.registerExport("wsock32.dll", "WSAAddressToStringW", reinterpret_cast<void*>(WSAAddressToStringW));
    ldr.registerExport("wsock32.dll", "WSAAsyncGetHostByName", reinterpret_cast<void*>(WSAAsyncGetHostByName));
    ldr.registerExport("wsock32.dll", "WSACancelAsyncRequest", reinterpret_cast<void*>(WSACancelAsyncRequest));
    ldr.registerExport("wsock32.dll", "WSAEnumNetworkEvents", reinterpret_cast<void*>(WSAEnumNetworkEvents));
    ldr.registerExport("wsock32.dll", "WSAEventSelect", reinterpret_cast<void*>(WSAEventSelect));
    ldr.registerExport("wsock32.dll", "WSAPoll", reinterpret_cast<void*>(WSAPoll));
    ldr.registerExport("wsock32.dll", "getservbyname", reinterpret_cast<void*>(getservbyname));
    ldr.registerExport("wsock32.dll", "inet_ntop", reinterpret_cast<void*>(inet_ntop));
    ldr.registerExport("wsock32.dll", "inet_pton", reinterpret_cast<void*>(inet_pton));

    // wsock32.dll Numeric Ordinal Exports
    ldr.registerExportOrdinal("wsock32.dll", 1, reinterpret_cast<void*>(accept));
    ldr.registerExportOrdinal("wsock32.dll", 2, reinterpret_cast<void*>(bind));
    ldr.registerExportOrdinal("wsock32.dll", 3, reinterpret_cast<void*>(closesocket));
    ldr.registerExportOrdinal("wsock32.dll", 4, reinterpret_cast<void*>(connect));
    ldr.registerExportOrdinal("wsock32.dll", 5, reinterpret_cast<void*>(getpeername));
    ldr.registerExportOrdinal("wsock32.dll", 6, reinterpret_cast<void*>(getsockname));
    ldr.registerExportOrdinal("wsock32.dll", 7, reinterpret_cast<void*>(getsockopt));
    ldr.registerExportOrdinal("wsock32.dll", 8, reinterpret_cast<void*>(htonl));
    ldr.registerExportOrdinal("wsock32.dll", 9, reinterpret_cast<void*>(htons));
    ldr.registerExportOrdinal("wsock32.dll", 10, reinterpret_cast<void*>(inet_addr));
    ldr.registerExportOrdinal("wsock32.dll", 11, reinterpret_cast<void*>(inet_ntoa));
    ldr.registerExportOrdinal("wsock32.dll", 12, reinterpret_cast<void*>(ioctlsocket));
    ldr.registerExportOrdinal("wsock32.dll", 13, reinterpret_cast<void*>(listen));
    ldr.registerExportOrdinal("wsock32.dll", 14, reinterpret_cast<void*>(ntohl));
    ldr.registerExportOrdinal("wsock32.dll", 15, reinterpret_cast<void*>(ntohs));
    ldr.registerExportOrdinal("wsock32.dll", 16, reinterpret_cast<void*>(recv));
    ldr.registerExportOrdinal("wsock32.dll", 17, reinterpret_cast<void*>(recvfrom));
    ldr.registerExportOrdinal("wsock32.dll", 18, reinterpret_cast<void*>(select));
    ldr.registerExportOrdinal("wsock32.dll", 19, reinterpret_cast<void*>(send));
    ldr.registerExportOrdinal("wsock32.dll", 20, reinterpret_cast<void*>(sendto));
    ldr.registerExportOrdinal("wsock32.dll", 21, reinterpret_cast<void*>(setsockopt));
    ldr.registerExportOrdinal("wsock32.dll", 22, reinterpret_cast<void*>(shutdown));
    ldr.registerExportOrdinal("wsock32.dll", 23, reinterpret_cast<void*>(socket));
    ldr.registerExportOrdinal("wsock32.dll", 51, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("wsock32.dll", 52, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("wsock32.dll", 55, reinterpret_cast<void*>(getservbyname));
    ldr.registerExportOrdinal("wsock32.dll", 57, reinterpret_cast<void*>(gethostname));
    ldr.registerExportOrdinal("wsock32.dll", 101, reinterpret_cast<void*>(WSAAsyncSelect));
    ldr.registerExportOrdinal("wsock32.dll", 102, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("wsock32.dll", 103, reinterpret_cast<void*>(WSAAsyncGetHostByName));
    ldr.registerExportOrdinal("wsock32.dll", 108, reinterpret_cast<void*>(WSACancelAsyncRequest));
    ldr.registerExportOrdinal("wsock32.dll", 111, reinterpret_cast<void*>(WSAGetLastError));
    ldr.registerExportOrdinal("wsock32.dll", 112, reinterpret_cast<void*>(WSASetLastError));
    ldr.registerExportOrdinal("wsock32.dll", 115, reinterpret_cast<void*>(WSAStartup));
    ldr.registerExportOrdinal("wsock32.dll", 116, reinterpret_cast<void*>(WSACleanup));
    ldr.registerExportOrdinal("wsock32.dll", 151, reinterpret_cast<void*>(__WSAFDIsSet));
    ldr.registerExportOrdinal("wsock32.dll", 1141, reinterpret_cast<void*>(WSAIoctl));
    ldr.registerExportOrdinal("wsock32.dll", 1142, reinterpret_cast<void*>(WSASocketA));

}

} // namespace micant::ws2_32
