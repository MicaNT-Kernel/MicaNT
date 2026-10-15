#pragma once
/**
 * @file winnt_ntdll_ws2_iphlpapi_winhttp_abi.hpp
 * @brief Horizon 113 (Kernel Unit #85): Sovereign WinNT Core User-Mode System32 DLLs
 *        (ntdll.dll, ws2_32.dll, iphlpapi.dll & winhttp.dll) Official PE32+ Exports,
 *        Ring 3 LdrLoadDll / RtlCriticalSection / Syscall Thunks, Winsock 2.2 Socket
 *        & Address Resolution ABI, IP Helper Adapter Enumeration, and WinHTTP Session ABI.
 */

#include <cstdint>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::um::winnt_ntdll_ws2_iphlpapi_winhttp_abi {

inline constexpr uint16_t WINSOCK_VERSION_2_2_OFFICIAL = 0x0202U;
inline constexpr int32_t  AF_INET_OFFICIAL             = 2;
inline constexpr int32_t  AF_INET6_OFFICIAL            = 23;
inline constexpr int32_t  SOCK_STREAM_OFFICIAL         = 1;
inline constexpr int32_t  SOCK_DGRAM_OFFICIAL          = 2;
inline constexpr int32_t  IPPROTO_TCP_OFFICIAL         = 6;
inline constexpr int32_t  IPPROTO_UDP_OFFICIAL         = 17;

struct WinNtOsVersionInfoExW {
    uint32_t dwOSVersionInfoSize{sizeof(WinNtOsVersionInfoExW)};
    uint32_t dwMajorVersion{10};
    uint32_t dwMinorVersion{0};
    uint32_t dwBuildNumber{26100};
    uint32_t dwPlatformId{2}; // VER_PLATFORM_WIN32_NT
    wchar_t  szCSDVersion[128]{};
    uint16_t wServicePackMajor{0};
    uint16_t wServicePackMinor{0};
    uint16_t wSuiteMask{0x0100}; // VER_SUITE_SINGLEUSERTS
    uint8_t  wProductType{1};    // VER_NT_WORKSTATION
    uint8_t  wReserved{0};
};

struct WinNtCriticalSection {
    void*     DebugInfo{nullptr};
    int32_t   LockCount{-1};
    int32_t   RecursionCount{0};
    void*     OwningThread{nullptr};
    void*     LockSemaphore{nullptr};
    uintptr_t SpinCount{0x020007D0ULL};
};

struct WinNtWsaData {
    uint16_t wVersion{WINSOCK_VERSION_2_2_OFFICIAL};
    uint16_t wHighVersion{WINSOCK_VERSION_2_2_OFFICIAL};
    uint16_t iMaxSockets{32767};
    uint16_t iMaxUdpDg{65467};
    char*    lpVendorInfo{nullptr};
    char     szDescription[257]{"MicaNT Winsock 2.2 (96 Gbps WSK)"};
    char     szSystemStatus[129]{"Running"};
};

struct WinNtAddrInfoA {
    int32_t         ai_flags{0};
    int32_t         ai_family{AF_INET_OFFICIAL};
    int32_t         ai_socktype{SOCK_STREAM_OFFICIAL};
    int32_t         ai_protocol{IPPROTO_TCP_OFFICIAL};
    size_t          ai_addrlen{16};
    char*           ai_canonname{nullptr};
    void*           ai_addr{nullptr};
    WinNtAddrInfoA* ai_next{nullptr};
};

// --- 1. ntdll.dll official exports (18 functions) ---
NTSTATUS Ntdll_RtlGetVersion(WinNtOsVersionInfoExW* lpVersionInformation) noexcept;
uint32_t Ntdll_RtlNtStatusToDosError(NTSTATUS status) noexcept;
void     Ntdll_RtlInitUnicodeString(UNICODE_STRING* destinationString, const wchar_t* sourceString) noexcept;
NTSTATUS Ntdll_RtlInitializeCriticalSection(WinNtCriticalSection* lpCriticalSection) noexcept;
NTSTATUS Ntdll_RtlEnterCriticalSection(WinNtCriticalSection* lpCriticalSection) noexcept;
NTSTATUS Ntdll_RtlLeaveCriticalSection(WinNtCriticalSection* lpCriticalSection) noexcept;
NTSTATUS Ntdll_RtlDeleteCriticalSection(WinNtCriticalSection* lpCriticalSection) noexcept;
uint16_t Ntdll_RtlCaptureStackBackTrace(uint32_t framesToSkip, uint32_t framesToCapture, void** backTrace, uint32_t* backTraceHash) noexcept;
void*    Ntdll_RtlAllocateHeap(void* heapHandle, uint32_t flags, size_t size) noexcept;
int32_t  Ntdll_RtlFreeHeap(void* heapHandle, uint32_t flags, void* baseAddress) noexcept;
NTSTATUS Ntdll_LdrLoadDll(const wchar_t* searchPath, const uint32_t* dllCharacteristics, const UNICODE_STRING* dllName, void** baseAddress) noexcept;
NTSTATUS Ntdll_LdrGetProcedureAddress(void* baseAddress, const void* procedureName, uint32_t procedureNumber, void** procedureAddress) noexcept;
NTSTATUS Ntdll_NtClose(void* handle) noexcept;
NTSTATUS Ntdll_NtWaitForSingleObject(void* handle, uint8_t alertable, const int64_t* timeout) noexcept;
NTSTATUS Ntdll_NtDelayExecution(uint8_t alertable, const int64_t* delayInterval) noexcept;
NTSTATUS Ntdll_NtQuerySystemInformation(uint32_t systemInformationClass, void* systemInformation, uint32_t systemInformationLength, uint32_t* returnLength) noexcept;
NTSTATUS Ntdll_NtAllocateVirtualMemory(void* processHandle, void** baseAddress, uintptr_t zeroBits, size_t* regionSize, uint32_t allocationType, uint32_t protect) noexcept;
NTSTATUS Ntdll_NtFreeVirtualMemory(void* processHandle, void** baseAddress, size_t* regionSize, uint32_t freeType) noexcept;

// --- 2. ws2_32.dll official exports (18 functions) ---
int32_t   Ws2_WSAStartup(uint16_t wVersionRequested, WinNtWsaData* lpWSAData) noexcept;
int32_t   Ws2_WSACleanup() noexcept;
int32_t   Ws2_WSAGetLastError() noexcept;
void      Ws2_WSASetLastError(int32_t iError) noexcept;
uintptr_t Ws2_socket(int32_t af, int32_t type, int32_t protocol) noexcept;
uintptr_t Ws2_WSASocketW(int32_t af, int32_t type, int32_t protocol, void* lpProtocolInfo, uint32_t g, uint32_t dwFlags) noexcept;
int32_t   Ws2_bind(uintptr_t s, const void* name, int32_t namelen) noexcept;
int32_t   Ws2_connect(uintptr_t s, const void* name, int32_t namelen) noexcept;
int32_t   Ws2_listen(uintptr_t s, int32_t backlog) noexcept;
uintptr_t Ws2_accept(uintptr_t s, void* addr, int32_t* addrlen) noexcept;
int32_t   Ws2_send(uintptr_t s, const char* buf, int32_t len, int32_t flags) noexcept;
int32_t   Ws2_recv(uintptr_t s, char* buf, int32_t len, int32_t flags) noexcept;
int32_t   Ws2_sendto(uintptr_t s, const char* buf, int32_t len, int32_t flags, const void* to, int32_t tolen) noexcept;
int32_t   Ws2_recvfrom(uintptr_t s, char* buf, int32_t len, int32_t flags, void* from, int32_t* fromlen) noexcept;
int32_t   Ws2_setsockopt(uintptr_t s, int32_t level, int32_t optname, const char* optval, int32_t optlen) noexcept;
int32_t   Ws2_closesocket(uintptr_t s) noexcept;
int32_t   Ws2_getaddrinfo(const char* pNodeName, const char* pServiceName, const WinNtAddrInfoA* pHints, WinNtAddrInfoA** ppResult) noexcept;
void      Ws2_freeaddrinfo(WinNtAddrInfoA* pAddrInfo) noexcept;

// --- 3. iphlpapi.dll official exports (6 functions) ---
uint32_t Iphlpapi_GetAdaptersAddresses(uint32_t family, uint32_t flags, void* reserved, void* adapterAddresses, uint32_t* sizePointer) noexcept;
uint32_t Iphlpapi_GetAdaptersInfo(void* adapterInfo, uint32_t* sizePointer) noexcept;
uint32_t Iphlpapi_GetBestInterfaceEx(const void* destAddr, uint32_t* bestIfIndex) noexcept;
uint32_t Iphlpapi_GetIfTable2(void** table) noexcept;
void     Iphlpapi_FreeMibTable(void* memory) noexcept;
uint32_t Iphlpapi_GetNetworkParams(void* fixedInfo, uint32_t* pOutBufLen) noexcept;

// --- 4. winhttp.dll official exports (10 functions) ---
void*   WinHttp_WinHttpOpen(const wchar_t* pszAgentW, uint32_t dwAccessType, const wchar_t* pszProxyW, const wchar_t* pszProxyBypassW, uint32_t dwFlags) noexcept;
void*   WinHttp_WinHttpConnect(void* hSession, const wchar_t* pswzServerName, uint16_t nServerPort, uint32_t dwReserved) noexcept;
void*   WinHttp_WinHttpOpenRequest(void* hConnect, const wchar_t* pwszVerb, const wchar_t* pwszObjectName, const wchar_t* pwszVersion, const wchar_t* pwszReferrer, const wchar_t** ppwszAcceptTypes, uint32_t dwFlags) noexcept;
int32_t WinHttp_WinHttpSendRequest(void* hRequest, const wchar_t* lpszHeaders, uint32_t dwHeadersLength, const void* lpOptional, uint32_t dwOptionalLength, uint32_t dwTotalLength, uintptr_t dwContext) noexcept;
int32_t WinHttp_WinHttpReceiveResponse(void* hRequest, void* lpReserved) noexcept;
int32_t WinHttp_WinHttpQueryHeaders(void* hRequest, uint32_t dwInfoLevel, const wchar_t* pwszName, void* lpBuffer, uint32_t* lpdwBufferLength, uint32_t* lpdwIndex) noexcept;
int32_t WinHttp_WinHttpQueryDataAvailable(void* hRequest, uint32_t* lpdwNumberOfBytesAvailable) noexcept;
int32_t WinHttp_WinHttpReadData(void* hRequest, void* lpBuffer, uint32_t dwNumberOfBytesToRead, uint32_t* lpdwNumberOfBytesRead) noexcept;
int32_t WinHttp_WinHttpSetOption(void* hInternet, uint32_t dwOption, const void* lpBuffer, uint32_t dwBufferLength) noexcept;
int32_t WinHttp_WinHttpCloseHandle(void* hInternet) noexcept;

struct Horizon113NtdllWs2IphlpapiWinHttpAuditReport {
    uint32_t ntdllExportCount{0};
    uint32_t ws2ExportCount{0};
    uint32_t iphlpapiExportCount{0};
    uint32_t winhttpExportCount{0};
    uint32_t buildNumberReported{0};
    bool     ntdllCriticalSectionAndHeapVerified{false};
    bool     winsock22TcpRoundTripVerified{false};
    bool     iphlpapiAdapterQueryVerified{false};
    bool     winhttpHttpsRequestVerified{false};
};

class WinNtNtdllWs2IphlpapiWinHttpCoordinator {
public:
    static WinNtNtdllWs2IphlpapiWinHttpCoordinator& instance() noexcept {
        static WinNtNtdllWs2IphlpapiWinHttpCoordinator s_inst;
        return s_inst;
    }

    void initialize() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

    [[nodiscard]] Horizon113NtdllWs2IphlpapiWinHttpAuditReport runNtdllWs2IphlpapiWinHttpAudit() noexcept;

private:
    WinNtNtdllWs2IphlpapiWinHttpCoordinator() = default;
    bool m_initialized{false};
    Horizon113NtdllWs2IphlpapiWinHttpAuditReport m_cachedReport{};
};

void InitializeWinNtNtdllWs2IphlpapiWinHttpSubsystem() noexcept;

} // namespace micant::um::winnt_ntdll_ws2_iphlpapi_winhttp_abi
