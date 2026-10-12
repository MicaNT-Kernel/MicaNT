#pragma once

/**
 * @file etw.hpp
 * @brief MicaNT Event Tracing for Windows (ETW) Subsystem
 *
 * Clean-room implementation referencing Microsoft's MIT-licensed win32metadata.
 * Implements:
 * - Event Tracing for Windows Controller & Consumer APIs (advapi32.dll)
 * - Native Event Tracing System Calls & Routines (ntdll.dll)
 * - Trace Sessions (NT Kernel Logger, MicaKernelTrace, Circular/Real-Time)
 * - Provider Registration & Event Dispatching (EventRegister, EventWrite, EventWriteString)
 * - Trace Log Management CLI (logman.exe) and Trace Report Generator (tracerpt.exe)
 * - SCM integration with Connected User Experiences and Telemetry (DiagTrack)
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <functional>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ldr.hpp"
#include "scm.hpp"

namespace micant::etw {

// ============================================================================
// 1. ETW Constants, Error Codes, and Control Codes
// ============================================================================

using TRACEHANDLE = uint64_t;
using REGHANDLE   = uint64_t;

inline constexpr uint32_t ERROR_SUCCESS                   = 0;
inline constexpr uint32_t ERROR_INVALID_PARAMETER         = 87;
inline constexpr uint32_t ERROR_ALREADY_EXISTS            = 183;
inline constexpr uint32_t ERROR_MORE_DATA                 = 234;
inline constexpr uint32_t ERROR_NOT_FOUND                 = 1168;
inline constexpr uint32_t ERROR_CANCELLED                 = 1223;
inline constexpr uint32_t ERROR_WMI_GUID_NOT_FOUND        = 4200;
inline constexpr uint32_t ERROR_WMI_ALREADY_ENABLED       = 4201;
inline constexpr uint32_t ERROR_WMI_INSTANCE_NOT_FOUND    = 4202;

inline constexpr TRACEHANDLE INVALID_PROCESSTRACE_HANDLE  = static_cast<TRACEHANDLE>(-1);

// Control codes for EnableTrace / EnableTraceEx2
inline constexpr uint32_t EVENT_CONTROL_CODE_DISABLE_PROVIDER = 0;
inline constexpr uint32_t EVENT_CONTROL_CODE_ENABLE_PROVIDER  = 1;
inline constexpr uint32_t EVENT_CONTROL_CODE_CAPTURE_STATE    = 2;

// Control codes for ControlTrace
inline constexpr uint32_t EVENT_TRACE_CONTROL_QUERY           = 0;
inline constexpr uint32_t EVENT_TRACE_CONTROL_STOP            = 1;
inline constexpr uint32_t EVENT_TRACE_CONTROL_UPDATE          = 2;
inline constexpr uint32_t EVENT_TRACE_CONTROL_FLUSH           = 3;

// Log file modes
inline constexpr uint32_t EVENT_TRACE_FILE_MODE_SEQUENTIAL    = 0x00000001;
inline constexpr uint32_t EVENT_TRACE_FILE_MODE_CIRCULAR      = 0x00000002;
inline constexpr uint32_t EVENT_TRACE_FILE_MODE_APPEND        = 0x00000004;
inline constexpr uint32_t EVENT_TRACE_FILE_MODE_NEWFILE       = 0x00000008;
inline constexpr uint32_t EVENT_TRACE_REAL_TIME_MODE          = 0x00000100;
inline constexpr uint32_t EVENT_TRACE_USE_PAGED_MEMORY        = 0x01000000;

// Logging levels
inline constexpr uint8_t TRACE_LEVEL_NONE                     = 0;
inline constexpr uint8_t TRACE_LEVEL_CRITICAL                 = 1;
inline constexpr uint8_t TRACE_LEVEL_ERROR                    = 2;
inline constexpr uint8_t TRACE_LEVEL_WARNING                  = 3;
inline constexpr uint8_t TRACE_LEVEL_INFORMATION              = 4;
inline constexpr uint8_t TRACE_LEVEL_VERBOSE                  = 5;

// System trace flags (for NT Kernel Logger)
inline constexpr uint32_t EVENT_TRACE_FLAG_PROCESS            = 0x00000001;
inline constexpr uint32_t EVENT_TRACE_FLAG_THREAD             = 0x00000002;
inline constexpr uint32_t EVENT_TRACE_FLAG_IMAGE_LOAD         = 0x00000004;
inline constexpr uint32_t EVENT_TRACE_FLAG_DISK_IO            = 0x00000100;
inline constexpr uint32_t EVENT_TRACE_FLAG_DISK_FILE_IO       = 0x00000200;
inline constexpr uint32_t EVENT_TRACE_FLAG_MEMORY_PAGE_FAULTS = 0x00001000;
inline constexpr uint32_t EVENT_TRACE_FLAG_MEMORY_HARD_FAULTS = 0x00002000;
inline constexpr uint32_t EVENT_TRACE_FLAG_NETWORK_TCPIP      = 0x00010000;
inline constexpr uint32_t EVENT_TRACE_FLAG_REGISTRY           = 0x00020000;
inline constexpr uint32_t EVENT_TRACE_FLAG_SYSTEMCALL         = 0x00080000;
inline constexpr uint32_t EVENT_TRACE_FLAG_ALPC               = 0x00100000;

// Standard Event Trace Types / Opcodes
inline constexpr uint8_t EVENT_TRACE_TYPE_INFO                = 0;
inline constexpr uint8_t EVENT_TRACE_TYPE_START               = 1;
inline constexpr uint8_t EVENT_TRACE_TYPE_END                 = 2;
inline constexpr uint8_t EVENT_TRACE_TYPE_DC_START            = 3;
inline constexpr uint8_t EVENT_TRACE_TYPE_DC_END              = 4;

// Event Header Extended Types & Flags
inline constexpr uint16_t EVENT_HEADER_EXT_TYPE_RELATED_ACTIVITY_ID = 0x0001;
inline constexpr uint16_t EVENT_HEADER_FLAG_EXTENDED_INFO           = 0x0001;
inline constexpr uint16_t EVENT_HEADER_FLAG_CLASSIC_HEADER          = 0x0100;

// ============================================================================
// 2. ETW Standard Structures (evntrace.h, evntprov.h, evntcons.h)
// ============================================================================

struct EVENT_DESCRIPTOR {
    uint16_t Id{0};
    uint8_t  Version{0};
    uint8_t  Channel{0};
    uint8_t  Level{TRACE_LEVEL_INFORMATION};
    uint8_t  Opcode{0};
    uint16_t Task{0};
    uint64_t Keyword{0};
};

struct EVENT_DATA_DESCRIPTOR {
    uint64_t Ptr{0};
    uint32_t Size{0};
    uint32_t Reserved{0};
};

struct EVENT_FILTER_DESCRIPTOR {
    uint64_t Ptr{0};
    uint32_t Size{0};
    uint32_t Type{0};
};

using PENABLECALLBACK = void (__stdcall *)(
    const GUID* SourceId,
    uint32_t IsEnabled,
    uint8_t Level,
    uint64_t MatchAnyKeyword,
    uint64_t MatchAllKeyword,
    void* FilterData,
    void* CallbackContext
);

struct ENABLE_TRACE_PARAMETERS {
    uint32_t Version{1};
    uint32_t EnableProperty{0};
    uint32_t ControlFlags{0};
    GUID     SourceId{};
    EVENT_FILTER_DESCRIPTOR* EnableFilterDesc{nullptr};
    uint32_t FilterDescCount{0};
};

struct WNODE_HEADER {
    uint32_t BufferSize{0};
    uint32_t ProviderId{0};
    uint64_t HistoricalContext{0};
    int64_t  TimeStamp{0};
    GUID     Guid{};
    uint32_t ClientContext{0};
    uint32_t Flags{0};
};

struct EVENT_TRACE_PROPERTIES {
    WNODE_HEADER Wnode{};
    uint32_t BufferSize{64};       // In KB
    uint32_t MinimumBuffers{2};
    uint32_t MaximumBuffers{64};
    uint32_t MaximumFileSize{100}; // In MB
    uint32_t LogFileMode{EVENT_TRACE_REAL_TIME_MODE};
    uint32_t FlushTimer{1};        // In seconds
    uint32_t EnableFlags{0};
    int32_t  AgeLimit{0};
    uint32_t NumberOfBuffers{2};
    uint32_t FreeBuffers{2};
    uint32_t EventsLost{0};
    uint32_t BuffersWritten{0};
    uint32_t LogBuffersLost{0};
    uint32_t RealTimeBuffersLost{0};
    void*    LoggerThreadId{nullptr};
    uint32_t LogFileNameOffset{0};
    uint32_t LoggerNameOffset{0};
};

struct EVENT_HEADER {
    uint16_t         Size{sizeof(EVENT_HEADER)};
    uint16_t         HeaderType{0};
    uint16_t         Flags{0};
    uint16_t         EventProperty{0};
    uint32_t         ThreadId{1};
    uint32_t         ProcessId{1000};
    int64_t          TimeStamp{0};
    GUID             ProviderId{};
    EVENT_DESCRIPTOR EventDescriptor{};
    uint64_t         ProcessorTime{0};
    GUID             ActivityId{};
};

struct ETW_BUFFER_CONTEXT {
    uint8_t  ProcessorNumber{0};
    uint8_t  Alignment{0};
    uint16_t LoggerId{0};
};

struct EVENT_RECORD {
    EVENT_HEADER       EventHeader{};
    ETW_BUFFER_CONTEXT BufferContext{};
    uint16_t           ExtendedDataCount{0};
    uint16_t           UserDataLength{0};
    void*              ExtendedData{nullptr};
    void*              UserData{nullptr};
    void*              UserContext{nullptr};
};

using PEVENT_RECORD_CALLBACK = void (__stdcall *)(EVENT_RECORD* EventRecord);
using PEVENT_TRACE_BUFFER_CALLBACKW = uint32_t (__stdcall *)(void* Logfile);

struct EVENT_TRACE_LOGFILEW {
    wchar_t*                      LogFileName{nullptr};
    wchar_t*                      LoggerName{nullptr};
    int64_t                       CurrentTime{0};
    uint32_t                      BuffersRead{0};
    uint32_t                      LogFileMode{0};
    EVENT_RECORD                  CurrentEvent{};
    ETW_BUFFER_CONTEXT            BufferContext{};
    PEVENT_TRACE_BUFFER_CALLBACKW BufferCallback{nullptr};
    uint32_t                      BufferSize{0};
    uint32_t                      Filled{0};
    uint32_t                      EventsLost{0};
    PEVENT_RECORD_CALLBACK        EventRecordCallback{nullptr};
    uint32_t                      IsKernelTrace{0};
    void*                         Context{nullptr};
};

// ============================================================================
// 3. Well-Known Standard Provider GUIDs
// ============================================================================

// SystemTraceControlGuid: {9E814AAD-3204-11D2-9A82-006008A86939}
inline constexpr GUID SystemTraceControlGuid = {
    0x9E814AAD, 0x3204, 0x11D2, { 0x9A, 0x82, 0x00, 0x60, 0x08, 0xA8, 0x69, 0x39 }
};

// MicaKernelProviderGuid: {D7B54789-53E0-4E1E-9C80-D5C8A79E5321}
inline constexpr GUID MicaKernelProviderGuid = {
    0xD7B54789, 0x53E0, 0x4E1E, { 0x9C, 0x80, 0xD5, 0xC8, 0xA7, 0x9E, 0x53, 0x21 }
};

// SecurityAuditProviderGuid: {8378E585-6A0F-4B6A-B6DD-9A74E5A8E5C3}
inline constexpr GUID SecurityAuditProviderGuid = {
    0x8378E585, 0x6A0F, 0x4B6A, { 0xB6, 0xDD, 0x9A, 0x74, 0xE5, 0xA8, 0xE5, 0xC3 }
};

// NetworkDiagProviderGuid: {437346A2-0DCE-4F55-A370-1B2296FFB13E}
inline constexpr GUID NetworkDiagProviderGuid = {
    0x437346A2, 0x0DCE, 0x4F55, { 0xA3, 0x70, 0x1B, 0x22, 0x96, 0xFF, 0xB1, 0x3E }
};

// StorageProviderGuid: {3256AEF1-E082-4E90-B8B7-91B6EC242A5C}
inline constexpr GUID StorageProviderGuid = {
    0x3256AEF1, 0xE082, 0x4E90, { 0xB8, 0xB7, 0x91, 0xB6, 0xEC, 0x24, 0x2A, 0x5C }
};

// NT Kernel Process Trace GUID: {22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}
inline constexpr GUID ProcessTraceGuid = {
    0x22FB2CD6, 0x0E7B, 0x422B, { 0xA0, 0xC7, 0x2F, 0xAD, 0x1F, 0xD0, 0xE7, 0x16 }
};

// NT Kernel Thread Trace GUID: {3D6FA8D1-FE12-462E-A000-F5B03021D18E}
inline constexpr GUID ThreadTraceGuid = {
    0x3D6FA8D1, 0xFE12, 0x462E, { 0xA0, 0x00, 0xF5, 0xB0, 0x30, 0x21, 0xD1, 0x8E }
};

// NT Kernel Disk I/O Trace GUID: {3D6FA8D4-FE12-462E-A000-F5B03021D18E}
inline constexpr GUID DiskIoTraceGuid = {
    0x3D6FA8D4, 0xFE12, 0x462E, { 0xA0, 0x00, 0xF5, 0xB0, 0x30, 0x21, 0xD1, 0x8E }
};

// NT Kernel Page Fault Trace GUID: {3D6FA8D3-FE12-462E-A000-F5B03021D18E}
inline constexpr GUID PageFaultTraceGuid = {
    0x3D6FA8D3, 0xFE12, 0x462E, { 0xA0, 0x00, 0xF5, 0xB0, 0x30, 0x21, 0xD1, 0x8E }
};

// NT Kernel Image Load Trace GUID: {2CB154FD-9B4C-42E4-B0E8-5144E4E1AD5B}
inline constexpr GUID ImageLoadTraceGuid = {
    0x2CB154FD, 0x9B4C, 0x42E4, { 0xB0, 0xE8, 0x51, 0x44, 0xE4, 0xE1, 0xAD, 0x5B }
};

// NT Kernel ALPC Trace GUID: {45D8CCCD-5384-47CE-B4CE-0320138E5144}
inline constexpr GUID AlpcTraceGuid = {
    0x45D8CCCD, 0x5384, 0x47CE, { 0xB4, 0xCE, 0x03, 0x20, 0x13, 0x8E, 0x51, 0x44 }
};

// NT Kernel Registry Trace GUID: {AE53722E-C863-11D2-8659-00C04FA321A1}
inline constexpr GUID RegistryTraceGuid = {
    0xAE53722E, 0xC863, 0x11D2, { 0x86, 0x59, 0x00, 0xC0, 0x4F, 0xA3, 0x21, 0xA1 }
};

// ============================================================================
// 4. GUID Utilities
// ============================================================================

inline std::string guidToString(const GUID& g) {
    std::ostringstream oss;
    oss << "{" << std::hex << std::uppercase << std::setfill('0')
        << std::setw(8) << g.Data1 << "-"
        << std::setw(4) << g.Data2 << "-"
        << std::setw(4) << g.Data3 << "-";
    for (int i = 0; i < 2; ++i) oss << std::setw(2) << static_cast<int>(g.Data4[i]);
    oss << "-";
    for (int i = 2; i < 8; ++i) oss << std::setw(2) << static_cast<int>(g.Data4[i]);
    oss << "}";
    return oss.str();
}

inline bool stringToGuid(const std::string& str, GUID& out) {
    std::string s = str;
    if (s.size() >= 2 && s.front() == '{' && s.back() == '}') {
        s = s.substr(1, s.size() - 2);
    }
    if (s.size() != 36) return false;

    // Format: 00000000-0000-0000-0000-000000000000
    try {
        out.Data1 = static_cast<uint32_t>(std::stoul(s.substr(0, 8), nullptr, 16));
        out.Data2 = static_cast<uint16_t>(std::stoul(s.substr(9, 4), nullptr, 16));
        out.Data3 = static_cast<uint16_t>(std::stoul(s.substr(14, 4), nullptr, 16));
        for (int i = 0; i < 2; ++i) {
            out.Data4[i] = static_cast<uint8_t>(std::stoul(s.substr(19 + i * 2, 2), nullptr, 16));
        }
        for (int i = 0; i < 6; ++i) {
            out.Data4[2 + i] = static_cast<uint8_t>(std::stoul(s.substr(24 + i * 2, 2), nullptr, 16));
        }
        return true;
    } catch (...) {
        return false;
    }
}

// GUID Comparator for map storage
struct GuidLess {
    bool operator()(const GUID& a, const GUID& b) const noexcept {
        if (a.Data1 != b.Data1) return a.Data1 < b.Data1;
        if (a.Data2 != b.Data2) return a.Data2 < b.Data2;
        if (a.Data3 != b.Data3) return a.Data3 < b.Data3;
        for (int i = 0; i < 8; ++i) {
            if (a.Data4[i] != b.Data4[i]) return a.Data4[i] < b.Data4[i];
        }
        return false;
    }
};

// ============================================================================
// 5. Internal Recorded Event, Buffer Pool and Trace Session Structures
// ============================================================================

namespace details {
inline void safeCopyAnsi(char* dest, size_t destSize, const char* src) noexcept {
    if (!dest || destSize == 0) return;
    if (!src) { dest[0] = '\0'; return; }
    size_t i = 0;
    while (i + 1 < destSize && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

inline void safeCopyWide(wchar_t* dest, size_t destCount, const wchar_t* src) noexcept {
    if (!dest || destCount == 0) return;
    if (!src) { dest[0] = L'\0'; return; }
    size_t i = 0;
    while (i + 1 < destCount && src[i] != L'\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = L'\0';
}
} // namespace details

#pragma pack(push, 1)
struct ETW_PROCESS_EVENT {
    uint32_t ProcessId{0};
    uint32_t ParentProcessId{0};
    uint32_t SessionId{0};
    int32_t  ExitStatus{0};
    uint64_t DirectoryTableBase{0};
    char     ImageFileName[16]{0};
};

struct ETW_THREAD_EVENT {
    uint32_t ProcessId{0};
    uint32_t ThreadId{0};
    uint64_t StackBase{0};
    uint64_t StackLimit{0};
    uint64_t Win32StartAddress{0};
    int32_t  ExitStatus{0};
};

struct ETW_IMAGE_LOAD_EVENT {
    uint64_t ImageBase{0};
    uint64_t ImageSize{0};
    uint32_t ProcessId{0};
    uint32_t ImageChecksum{0};
    uint32_t TimeDateStamp{0};
    wchar_t  FileName[260]{0};
};

struct ETW_DISK_IO_EVENT {
    uint32_t DiskNumber{0};
    uint32_t IrpFlags{0};
    uint32_t TransferSize{0};
    uint32_t Reserved{0};
    uint64_t ByteOffset{0};
    uint64_t FileObject{0};
};

struct ETW_PAGE_FAULT_EVENT {
    uint64_t VirtualAddress{0};
    uint64_t ProgramCounter{0};
    uint32_t ProcessId{0};
    uint32_t FaultType{0}; // 1 = DemandZero, 2 = Transition, 3 = HardFault
};

struct ETW_ALPC_EVENT {
    uint32_t MessageId{0};
    uint32_t SourceProcessId{0};
    uint32_t TargetProcessId{0};
    uint32_t MessageType{0};
    uint32_t DataLength{0};
};

struct ETW_REGISTRY_EVENT {
    uint64_t KeyHandle{0};
    uint32_t Status{0};
    uint32_t OpType{0}; // 1 = Create, 2 = Open, 3 = Query, 4 = Set, 5 = Delete
    wchar_t  KeyName[128]{0};
};

struct EVENT_HEADER_EXTENDED_DATA_ITEM {
    uint16_t Reserved1{0};
    uint16_t ExtType{0};
    uint16_t Reserved2{0};
    uint16_t DataSize{0};
    uint64_t DataPtr{0};
};
#pragma pack(pop)

struct ETW_TELEMETRY {
    uint64_t totalSessionsStarted{0};
    uint64_t totalSessionsStopped{0};
    uint64_t totalEventsWritten{0};
    uint64_t totalEventsDelivered{0};
    uint64_t totalBuffersFlushed{0};
    uint64_t totalProvidersRegistered{0};
    uint64_t totalProvidersUnregistered{0};
    uint64_t totalCircularBufferWraps{0};
    uint64_t totalEventsLost{0};
    uint64_t totalKernelEventsLogged{0};
};

struct RecordedEtwEvent {
    EVENT_HEADER         header{};
    std::vector<uint8_t> data;
    std::wstring         message;
    bool                 hasActivityId{false};
    GUID                 activityId{};
    bool                 hasRelatedActivityId{false};
    GUID                 relatedActivityId{};
};

struct EnabledProviderInfo {
    uint8_t  level{TRACE_LEVEL_INFORMATION};
    uint64_t matchAnyKeyword{0xFFFFFFFFFFFFFFFFULL};
    uint64_t matchAllKeyword{0};
};

struct ETW_BUFFER_HEADER {
    uint32_t BufferSequence{0};
    uint32_t BufferSize{64 * 1024};
    uint32_t FilledBytes{sizeof(ETW_BUFFER_HEADER)};
    int64_t  FirstTimeStamp{0};
    int64_t  LastTimeStamp{0};
    uint32_t EventsCount{0};
    uint16_t LoggerId{0};
    uint8_t  ProcessorNumber{0};
    uint8_t  Alignment{8};
    uint32_t State{0}; // 0 = Free, 1 = Active/Filling, 2 = Flushed/Full
};

struct EtwBuffer {
    ETW_BUFFER_HEADER header{};
    std::vector<RecordedEtwEvent> events;

    void reset(uint32_t seq, uint16_t loggerId, uint32_t bufSizeKb) noexcept {
        header.BufferSequence = seq;
        header.BufferSize = (bufSizeKb == 0 ? 64 : bufSizeKb) * 1024;
        header.FilledBytes = sizeof(ETW_BUFFER_HEADER);
        header.FirstTimeStamp = 0;
        header.LastTimeStamp = 0;
        header.EventsCount = 0;
        header.LoggerId = loggerId;
        header.ProcessorNumber = 0;
        header.Alignment = 8;
        header.State = 0;
        events.clear();
    }

    bool canFit(size_t payloadSize) const noexcept {
        size_t alignedEventSize = (sizeof(EVENT_HEADER) + payloadSize + 7) & ~7ULL;
        return (header.FilledBytes + alignedEventSize <= header.BufferSize);
    }

    void appendEvent(const RecordedEtwEvent& ev) {
        if (events.empty()) {
            header.FirstTimeStamp = ev.header.TimeStamp;
        }
        header.LastTimeStamp = ev.header.TimeStamp;
        size_t alignedEventSize = (sizeof(EVENT_HEADER) + ev.data.size() + 7) & ~7ULL;
        header.FilledBytes += static_cast<uint32_t>(alignedEventSize);
        header.EventsCount++;
        events.push_back(ev);
    }
};

class TraceSession {
public:
    TraceSession(uint16_t id, std::wstring name, const EVENT_TRACE_PROPERTIES& props, std::wstring logFileName)
        : m_id(id), m_name(std::move(name)), m_props(props), m_logFileName(std::move(logFileName)) {
        if (m_props.BufferSize == 0) m_props.BufferSize = 64;
        m_props.NumberOfBuffers = std::max(2u, m_props.MinimumBuffers);
        if (m_props.MaximumBuffers < m_props.NumberOfBuffers) {
            m_props.MaximumBuffers = std::max(m_props.NumberOfBuffers, 32u);
        }
        m_props.FreeBuffers = m_props.NumberOfBuffers;
        m_props.BuffersWritten = 0;
        m_props.EventsLost = 0;
        m_props.RealTimeBuffersLost = 0;

        // Allocate initial active buffer
        EtwBuffer initialBuf;
        initialBuf.reset(++m_sequenceCounter, m_id, m_props.BufferSize);
        initialBuf.header.State = 1; // Active
        m_buffers.push_back(std::move(initialBuf));
        m_currentBufferIndex = 0;
        m_oldestBufferIndex = 0;
        if (m_props.FreeBuffers > 0) m_props.FreeBuffers--;
    }

    uint16_t getId() const noexcept { return m_id; }
    const std::wstring& getName() const noexcept { return m_name; }
    const std::wstring& getLogFileName() const noexcept { return m_logFileName; }
    const EVENT_TRACE_PROPERTIES& getProperties() const noexcept { return m_props; }
    EVENT_TRACE_PROPERTIES& getProperties() noexcept { return m_props; }

    uint32_t getCircularWraps() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_circularWraps;
    }

    uint32_t getCompletedBuffersCount() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_completedBuffersCount;
    }

    void enableProvider(const GUID& guid, uint8_t level, uint64_t anyKw, uint64_t allKw) {
        std::lock_guard<std::mutex> lock(m_mutex);
        EnabledProviderInfo info{ level, anyKw, allKw };
        m_enabledProviders[guid] = info;
    }

    void disableProvider(const GUID& guid) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_enabledProviders.erase(guid);
    }

    bool isProviderEnabled(const GUID& guid, uint8_t level, uint64_t keyword) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_enabledProviders.find(guid);
        if (it == m_enabledProviders.end()) return false;
        if (level > it->second.level && it->second.level != 0) return false;
        if (keyword != 0 && (keyword & it->second.matchAnyKeyword) == 0) return false;
        if ((keyword & it->second.matchAllKeyword) != it->second.matchAllKeyword) return false;
        return true;
    }

    bool recordEvent(const EVENT_HEADER& hdr, const void* pData, size_t size, std::wstring msg = L"",
                     const GUID* pActivityId = nullptr, const GUID* pRelatedActivityId = nullptr,
                     uint64_t* pWrapsOut = nullptr, uint64_t* pLostOut = nullptr, uint64_t* pFlushedOut = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);

        RecordedEtwEvent rec{};
        rec.header = hdr;
        if (pData && size > 0) {
            rec.data.resize(size);
            std::memcpy(rec.data.data(), pData, size);
        }
        rec.message = std::move(msg);
        if (pActivityId) {
            rec.hasActivityId = true;
            rec.activityId = *pActivityId;
            rec.header.ActivityId = *pActivityId;
        }
        if (pRelatedActivityId) {
            rec.hasRelatedActivityId = true;
            rec.relatedActivityId = *pRelatedActivityId;
        }

        if (m_buffers.empty()) {
            EtwBuffer buf;
            buf.reset(++m_sequenceCounter, m_id, m_props.BufferSize);
            buf.header.State = 1;
            m_buffers.push_back(std::move(buf));
            m_currentBufferIndex = 0;
            m_oldestBufferIndex = 0;
        }

        // Check if current buffer can fit the event
        if (!m_buffers[m_currentBufferIndex].canFit(size)) {
            // Mark current buffer full
            m_buffers[m_currentBufferIndex].header.State = 2;
            m_completedBuffersCount++;
            m_props.BuffersWritten = m_completedBuffersCount;
            if (pFlushedOut) (*pFlushedOut)++;

            if (m_buffers.size() < m_props.MaximumBuffers) {
                EtwBuffer newBuf;
                newBuf.reset(++m_sequenceCounter, m_id, m_props.BufferSize);
                newBuf.header.State = 1;
                m_buffers.push_back(std::move(newBuf));
                m_currentBufferIndex = m_buffers.size() - 1;
                m_props.NumberOfBuffers = static_cast<uint32_t>(m_buffers.size());
                if (m_props.FreeBuffers > 0) m_props.FreeBuffers--;
            } else {
                if ((m_props.LogFileMode & EVENT_TRACE_FILE_MODE_CIRCULAR) != 0) {
                    m_circularWraps++;
                    if (pWrapsOut) (*pWrapsOut)++;

                    m_currentBufferIndex = m_oldestBufferIndex;
                    m_oldestBufferIndex = (m_oldestBufferIndex + 1) % m_buffers.size();
                    m_buffers[m_currentBufferIndex].reset(++m_sequenceCounter, m_id, m_props.BufferSize);
                    m_buffers[m_currentBufferIndex].header.State = 1;
                } else {
                    m_props.EventsLost++;
                    if (pLostOut) (*pLostOut)++;
                    return false;
                }
            }
        }

        m_buffers[m_currentBufferIndex].appendEvent(rec);
        m_props.BuffersWritten = std::max(1u, m_completedBuffersCount + 1);
        return true;
    }

    std::vector<RecordedEtwEvent> getEvents() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<RecordedEtwEvent> res;
        if (m_buffers.empty()) return res;
        size_t count = m_buffers.size();
        for (size_t i = 0; i < count; ++i) {
            size_t idx = (m_oldestBufferIndex + i) % count;
            for (const auto& ev : m_buffers[idx].events) {
                res.push_back(ev);
            }
        }
        return res;
    }

    size_t getEventCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t total = 0;
        for (const auto& buf : m_buffers) {
            total += buf.events.size();
        }
        return total;
    }

    std::vector<EtwBuffer> getBuffersCopy() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<EtwBuffer> res;
        if (m_buffers.empty()) return res;
        size_t count = m_buffers.size();
        for (size_t i = 0; i < count; ++i) {
            size_t idx = (m_oldestBufferIndex + i) % count;
            res.push_back(m_buffers[idx]);
        }
        return res;
    }

    void flush() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_buffers.empty() && m_buffers[m_currentBufferIndex].header.EventsCount > 0) {
            m_buffers[m_currentBufferIndex].header.State = 2;
            m_completedBuffersCount++;
            m_props.BuffersWritten = m_completedBuffersCount;
        }
    }

    std::vector<GUID> getEnabledGuids() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<GUID> res;
        for (const auto& [g, _] : m_enabledProviders) res.push_back(g);
        return res;
    }

private:
    uint16_t m_id{1};
    std::wstring m_name;
    EVENT_TRACE_PROPERTIES m_props{};
    std::wstring m_logFileName;
    std::vector<EtwBuffer> m_buffers;
    size_t m_currentBufferIndex{0};
    size_t m_oldestBufferIndex{0};
    uint32_t m_sequenceCounter{0};
    uint32_t m_circularWraps{0};
    uint32_t m_completedBuffersCount{0};
    std::map<GUID, EnabledProviderInfo, GuidLess> m_enabledProviders;
    mutable std::mutex m_mutex;
};

struct RegisteredProvider {
    REGHANDLE        handle{0};
    GUID             guid{};
    PENABLECALLBACK  callback{nullptr};
    void*            context{nullptr};
    bool             registered{false};
};

// ============================================================================
// 6. TraceManager - Central ETW Controller & Dispatch Engine
// ============================================================================

class TraceManager {
public:
    static TraceManager& get() {
        static TraceManager instance;
        return instance;
    }

    TraceManager() {
        initializeKernelLogger();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_providers.clear();
        m_sessions.clear();
        m_sessionByName.clear();
        m_consumers.clear();
        m_nextRegHandle = 0x1000;
        m_nextTraceHandle = 0x2000;
        m_nextSessionId = 1;
        m_telemetry = ETW_TELEMETRY{};
        initializeKernelLoggerUnlocked();
    }

    ETW_TELEMETRY getTelemetry() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
    }

    void resetTelemetry() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry = ETW_TELEMETRY{};
    }

    // Provider APIs
    uint32_t registerProvider(const GUID* providerId, PENABLECALLBACK callback, void* context, REGHANDLE* outHandle) {
        if (!providerId || !outHandle) return ERROR_INVALID_PARAMETER;

        std::lock_guard<std::mutex> lock(m_mutex);
        REGHANDLE h = m_nextRegHandle++;
        RegisteredProvider prov{};
        prov.handle = h;
        prov.guid = *providerId;
        prov.callback = callback;
        prov.context = context;
        prov.registered = true;

        m_providers[h] = prov;
        *outHandle = h;
        m_telemetry.totalProvidersRegistered++;

        // If any active session has already enabled this provider, notify callback
        for (const auto& [_, session] : m_sessions) {
            if (session->isProviderEnabled(*providerId, TRACE_LEVEL_INFORMATION, 0)) {
                if (callback) {
                    callback(providerId, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION,
                             0xFFFFFFFFFFFFFFFFULL, 0, nullptr, context);
                }
                break;
            }
        }

        return ERROR_SUCCESS;
    }

    uint32_t unregisterProvider(REGHANDLE handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_providers.find(handle);
        if (it == m_providers.end()) return ERROR_INVALID_PARAMETER;

        if (it->second.callback) {
            it->second.callback(&it->second.guid, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, nullptr, it->second.context);
        }

        m_providers.erase(it);
        m_telemetry.totalProvidersUnregistered++;
        return ERROR_SUCCESS;
    }

    bool isEventEnabled(REGHANDLE handle, const EVENT_DESCRIPTOR* desc) {
        if (!desc) return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_providers.find(handle);
        if (it == m_providers.end()) return false;

        for (const auto& [_, session] : m_sessions) {
            if (session->isProviderEnabled(it->second.guid, desc->Level, desc->Keyword)) {
                return true;
            }
        }
        return false;
    }

    bool isProviderEnabled(REGHANDLE handle, uint8_t level, uint64_t keyword) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_providers.find(handle);
        if (it == m_providers.end()) return false;

        for (const auto& [_, session] : m_sessions) {
            if (session->isProviderEnabled(it->second.guid, level, keyword)) {
                return true;
            }
        }
        return false;
    }

    uint32_t writeEventTransfer(REGHANDLE handle, const EVENT_DESCRIPTOR* desc,
                                const GUID* activityId, const GUID* relatedActivityId,
                                uint32_t userDataCount, const EVENT_DATA_DESCRIPTOR* userData) {
        if (!desc) return ERROR_INVALID_PARAMETER;

        std::vector<uint8_t> payload;
        if (userDataCount > 0 && userData) {
            size_t totalBytes = 0;
            for (uint32_t i = 0; i < userDataCount; ++i) {
                totalBytes += userData[i].Size;
            }
            payload.resize(totalBytes);
            size_t offset = 0;
            for (uint32_t i = 0; i < userDataCount; ++i) {
                if (userData[i].Ptr && userData[i].Size > 0) {
                    std::memcpy(payload.data() + offset, reinterpret_cast<const void*>(userData[i].Ptr), userData[i].Size);
                    offset += userData[i].Size;
                }
            }
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_providers.find(handle);
        if (it == m_providers.end()) return ERROR_INVALID_PARAMETER;

        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        EVENT_HEADER hdr{};
        hdr.Size = static_cast<uint16_t>(sizeof(EVENT_HEADER) + payload.size());
        hdr.TimeStamp = ts;
        hdr.ProviderId = it->second.guid;
        hdr.EventDescriptor = *desc;
        hdr.ThreadId = 1;
        hdr.ProcessId = 1000;
        if (activityId) {
            hdr.ActivityId = *activityId;
            hdr.Flags |= EVENT_HEADER_FLAG_EXTENDED_INFO;
        }

        for (auto& [_, session] : m_sessions) {
            if (session->isProviderEnabled(it->second.guid, desc->Level, desc->Keyword)) {
                uint64_t wraps = 0, lost = 0, flushed = 0;
                session->recordEvent(hdr, payload.data(), payload.size(), L"", activityId, relatedActivityId, &wraps, &lost, &flushed);
                m_telemetry.totalCircularBufferWraps += wraps;
                m_telemetry.totalEventsLost += lost;
                m_telemetry.totalBuffersFlushed += flushed;
            }
        }

        m_telemetry.totalEventsWritten++;
        return ERROR_SUCCESS;
    }

    uint32_t writeEvent(REGHANDLE handle, const EVENT_DESCRIPTOR* desc, uint32_t userDataCount, const EVENT_DATA_DESCRIPTOR* userData) {
        return writeEventTransfer(handle, desc, nullptr, nullptr, userDataCount, userData);
    }

    uint32_t writeString(REGHANDLE handle, uint8_t level, uint64_t keyword, const wchar_t* str) {
        if (!str) return ERROR_INVALID_PARAMETER;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_providers.find(handle);
        if (it == m_providers.end()) return ERROR_INVALID_PARAMETER;

        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        EVENT_DESCRIPTOR desc{};
        desc.Level = level;
        desc.Keyword = keyword;

        std::wstring ws(str);
        size_t byteLen = (ws.size() + 1) * sizeof(wchar_t);

        EVENT_HEADER hdr{};
        hdr.Size = static_cast<uint16_t>(sizeof(EVENT_HEADER) + byteLen);
        hdr.TimeStamp = ts;
        hdr.ProviderId = it->second.guid;
        hdr.EventDescriptor = desc;

        for (auto& [_, session] : m_sessions) {
            if (session->isProviderEnabled(it->second.guid, level, keyword)) {
                uint64_t wraps = 0, lost = 0, flushed = 0;
                session->recordEvent(hdr, ws.data(), byteLen, ws, nullptr, nullptr, &wraps, &lost, &flushed);
                m_telemetry.totalCircularBufferWraps += wraps;
                m_telemetry.totalEventsLost += lost;
                m_telemetry.totalBuffersFlushed += flushed;
            }
        }

        m_telemetry.totalEventsWritten++;
        return ERROR_SUCCESS;
    }

    // Controller APIs
    uint32_t startTrace(TRACEHANDLE* outHandle, const wchar_t* sessionName, EVENT_TRACE_PROPERTIES* props) {
        if (!outHandle || !sessionName || !props) return ERROR_INVALID_PARAMETER;

        std::wstring name(sessionName);
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_sessionByName.find(name) != m_sessionByName.end()) {
            return ERROR_ALREADY_EXISTS;
        }

        std::wstring logFile;
        if (props->LogFileNameOffset > 0 && props->LogFileNameOffset < props->Wnode.BufferSize) {
            const wchar_t* pFile = reinterpret_cast<const wchar_t*>(reinterpret_cast<const uint8_t*>(props) + props->LogFileNameOffset);
            logFile = pFile;
        }

        TRACEHANDLE h = m_nextTraceHandle++;
        uint16_t id = m_nextSessionId++;

        auto session = std::make_shared<TraceSession>(id, name, *props, logFile);

        // If starting NT Kernel Logger, auto-enable system trace provider
        if (name == L"NT Kernel Logger") {
            session->enableProvider(SystemTraceControlGuid, TRACE_LEVEL_VERBOSE, 0xFFFFFFFFFFFFFFFFULL, 0);
        }

        m_sessions[h] = session;
        m_sessionByName[name] = h;
        *outHandle = h;

        props->Wnode.HistoricalContext = h;
        props->NumberOfBuffers = session->getProperties().NumberOfBuffers;
        props->FreeBuffers = session->getProperties().FreeBuffers;

        m_telemetry.totalSessionsStarted++;
        return ERROR_SUCCESS;
    }

    uint32_t stopTrace(TRACEHANDLE handle, const wchar_t* sessionName, EVENT_TRACE_PROPERTIES* props) {
        std::lock_guard<std::mutex> lock(m_mutex);
        TRACEHANDLE targetHandle = handle;

        if (targetHandle == 0 && sessionName) {
            auto it = m_sessionByName.find(sessionName);
            if (it == m_sessionByName.end()) return ERROR_WMI_INSTANCE_NOT_FOUND;
            targetHandle = it->second;
        }

        auto it = m_sessions.find(targetHandle);
        if (it == m_sessions.end()) return ERROR_WMI_INSTANCE_NOT_FOUND;

        if (props) {
            *props = it->second->getProperties();
            props->Wnode.HistoricalContext = targetHandle;
        }

        m_sessionByName.erase(it->second->getName());
        m_sessions.erase(it);

        m_telemetry.totalSessionsStopped++;
        return ERROR_SUCCESS;
    }

    uint32_t queryTrace(TRACEHANDLE handle, const wchar_t* sessionName, EVENT_TRACE_PROPERTIES* props) {
        if (!props) return ERROR_INVALID_PARAMETER;

        std::lock_guard<std::mutex> lock(m_mutex);
        TRACEHANDLE targetHandle = handle;

        if (targetHandle == 0 && sessionName) {
            auto it = m_sessionByName.find(sessionName);
            if (it == m_sessionByName.end()) return ERROR_WMI_INSTANCE_NOT_FOUND;
            targetHandle = it->second;
        }

        auto it = m_sessions.find(targetHandle);
        if (it == m_sessions.end()) return ERROR_WMI_INSTANCE_NOT_FOUND;

        *props = it->second->getProperties();
        props->Wnode.HistoricalContext = targetHandle;

        return ERROR_SUCCESS;
    }

    uint32_t updateTrace(TRACEHANDLE handle, const wchar_t* sessionName, EVENT_TRACE_PROPERTIES* props) {
        if (!props) return ERROR_INVALID_PARAMETER;

        std::lock_guard<std::mutex> lock(m_mutex);
        TRACEHANDLE targetHandle = handle;

        if (targetHandle == 0 && sessionName) {
            auto it = m_sessionByName.find(sessionName);
            if (it == m_sessionByName.end()) return ERROR_WMI_INSTANCE_NOT_FOUND;
            targetHandle = it->second;
        }

        auto it = m_sessions.find(targetHandle);
        if (it == m_sessions.end()) return ERROR_WMI_INSTANCE_NOT_FOUND;

        auto& currentProps = it->second->getProperties();
        if (props->FlushTimer > 0) currentProps.FlushTimer = props->FlushTimer;
        if (props->MaximumBuffers > 0) currentProps.MaximumBuffers = props->MaximumBuffers;
        if (props->LogFileMode > 0) currentProps.LogFileMode = props->LogFileMode;
        if (props->EnableFlags > 0) currentProps.EnableFlags = props->EnableFlags;

        *props = currentProps;
        return ERROR_SUCCESS;
    }

    uint32_t flushTrace(TRACEHANDLE handle, const wchar_t* sessionName, EVENT_TRACE_PROPERTIES* props) {
        std::lock_guard<std::mutex> lock(m_mutex);
        TRACEHANDLE targetHandle = handle;

        if (targetHandle == 0 && sessionName) {
            auto it = m_sessionByName.find(sessionName);
            if (it == m_sessionByName.end()) return ERROR_WMI_INSTANCE_NOT_FOUND;
            targetHandle = it->second;
        }

        auto it = m_sessions.find(targetHandle);
        if (it == m_sessions.end()) return ERROR_WMI_INSTANCE_NOT_FOUND;

        it->second->flush();
        if (props) {
            *props = it->second->getProperties();
        }

        m_telemetry.totalBuffersFlushed++;
        return ERROR_SUCCESS;
    }

    uint32_t controlTrace(TRACEHANDLE handle, const wchar_t* sessionName, EVENT_TRACE_PROPERTIES* props, uint32_t controlCode) {
        switch (controlCode) {
            case EVENT_TRACE_CONTROL_QUERY:
                return queryTrace(handle, sessionName, props);
            case EVENT_TRACE_CONTROL_STOP:
                return stopTrace(handle, sessionName, props);
            case EVENT_TRACE_CONTROL_UPDATE:
                return updateTrace(handle, sessionName, props);
            case EVENT_TRACE_CONTROL_FLUSH:
                return flushTrace(handle, sessionName, props);
            default:
                return ERROR_INVALID_PARAMETER;
        }
    }

    uint32_t enableTraceEx2(TRACEHANDLE traceHandle, const GUID* providerId, uint32_t controlCode,
                            uint8_t level, uint64_t matchAnyKeyword, uint64_t matchAllKeyword,
                            uint32_t timeout, ENABLE_TRACE_PARAMETERS* enableParams) {
        (void)timeout;
        (void)enableParams;
        if (!providerId) return ERROR_INVALID_PARAMETER;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(traceHandle);
        if (it == m_sessions.end()) return ERROR_WMI_INSTANCE_NOT_FOUND;

        if (controlCode == EVENT_CONTROL_CODE_ENABLE_PROVIDER || controlCode == EVENT_CONTROL_CODE_CAPTURE_STATE) {
            it->second->enableProvider(*providerId, level, matchAnyKeyword, matchAllKeyword);

            // Notify provider if registered
            for (auto& [_, prov] : m_providers) {
                if (prov.guid == *providerId && prov.callback) {
                    prov.callback(providerId, controlCode, level, matchAnyKeyword, matchAllKeyword, nullptr, prov.context);
                }
            }
        } else if (controlCode == EVENT_CONTROL_CODE_DISABLE_PROVIDER) {
            it->second->disableProvider(*providerId);

            for (auto& [_, prov] : m_providers) {
                if (prov.guid == *providerId && prov.callback) {
                    prov.callback(providerId, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, nullptr, prov.context);
                }
            }
        }

        return ERROR_SUCCESS;
    }

    // Consumer APIs
    TRACEHANDLE openTrace(EVENT_TRACE_LOGFILEW* logfile) {
        if (!logfile) return INVALID_PROCESSTRACE_HANDLE;

        std::lock_guard<std::mutex> lock(m_mutex);
        TRACEHANDLE h = m_nextTraceHandle++;
        auto copyLog = std::make_shared<EVENT_TRACE_LOGFILEW>(*logfile);
        m_consumers[h] = copyLog;
        return h;
    }

    uint32_t processTrace(TRACEHANDLE* handleArray, uint32_t handleCount, void* startTime, void* endTime) {
        (void)startTime;
        (void)endTime;
        if (!handleArray || handleCount == 0) return ERROR_INVALID_PARAMETER;

        std::lock_guard<std::mutex> lock(m_mutex);
        for (uint32_t i = 0; i < handleCount; ++i) {
            TRACEHANDLE h = handleArray[i];
            auto it = m_consumers.find(h);
            if (it == m_consumers.end()) continue;

            auto logfile = it->second;
            if (!logfile->EventRecordCallback && !logfile->BufferCallback) continue;

            // Find matching session
            std::shared_ptr<TraceSession> targetSession;
            if (logfile->LoggerName && logfile->LoggerName[0] != L'\0') {
                auto sit = m_sessionByName.find(logfile->LoggerName);
                if (sit != m_sessionByName.end()) {
                    targetSession = m_sessions[sit->second];
                }
            } else if (logfile->LogFileName && logfile->LogFileName[0] != L'\0') {
                for (const auto& [_, s] : m_sessions) {
                    if (s->getLogFileName() == logfile->LogFileName) {
                        targetSession = s;
                        break;
                    }
                }
            }

            if (!targetSession && !m_sessions.empty()) {
                targetSession = m_sessions.begin()->second;
            }

            if (targetSession) {
                auto buffers = targetSession->getBuffersCopy();
                for (auto& buf : buffers) {
                    if (buf.events.empty()) continue;

                    logfile->Filled = buf.header.FilledBytes;
                    logfile->BufferSize = buf.header.BufferSize;
                    logfile->BufferContext.ProcessorNumber = buf.header.ProcessorNumber;
                    logfile->BufferContext.Alignment = buf.header.Alignment;
                    logfile->BufferContext.LoggerId = buf.header.LoggerId;

                    if (logfile->BufferCallback) {
                        logfile->BufferCallback(logfile.get());
                    }

                    if (logfile->EventRecordCallback) {
                        for (auto& ev : buf.events) {
                            EVENT_RECORD rec{};
                            rec.EventHeader = ev.header;
                            rec.BufferContext = logfile->BufferContext;
                            rec.UserDataLength = static_cast<uint16_t>(ev.data.size());
                            rec.UserData = ev.data.empty() ? nullptr : ev.data.data();
                            rec.UserContext = logfile->Context;

                            EVENT_HEADER_EXTENDED_DATA_ITEM extItem{};
                            if (ev.hasRelatedActivityId) {
                                extItem.ExtType = EVENT_HEADER_EXT_TYPE_RELATED_ACTIVITY_ID;
                                extItem.DataSize = sizeof(GUID);
                                extItem.DataPtr = reinterpret_cast<uint64_t>(&ev.relatedActivityId);
                                rec.ExtendedDataCount = 1;
                                rec.ExtendedData = &extItem;
                            }

                            logfile->EventRecordCallback(&rec);
                            logfile->BuffersRead++;
                            m_telemetry.totalEventsDelivered++;
                        }
                    }
                }
            }
        }

        return ERROR_SUCCESS;
    }

    uint32_t closeTrace(TRACEHANDLE traceHandle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_consumers.find(traceHandle);
        if (it == m_consumers.end()) return ERROR_INVALID_PARAMETER;
        m_consumers.erase(it);
        return ERROR_SUCCESS;
    }

    std::vector<std::shared_ptr<TraceSession>> getActiveSessions() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<TraceSession>> res;
        for (const auto& [_, s] : m_sessions) res.push_back(s);
        return res;
    }

    std::shared_ptr<TraceSession> getSessionByName(const std::wstring& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionByName.find(name);
        if (it == m_sessionByName.end()) return nullptr;
        return m_sessions[it->second];
    }

    // Sovereign Kernel-Mode Event Tracing Engine & Routing
    bool isSystemFlagEnabled(uint32_t flag) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [_, s] : m_sessions) {
            if ((s->getProperties().EnableFlags & flag) != 0) return true;
        }
        return false;
    }

    void recordKernelEvent(const GUID& guid, uint32_t flag, const EVENT_HEADER& hdr, const void* pData, size_t size, std::wstring msg = L"") {
        std::lock_guard<std::mutex> lock(m_mutex);
        bool logged = false;
        for (auto& [_, session] : m_sessions) {
            if ((session->getProperties().EnableFlags & flag) != 0 ||
                session->isProviderEnabled(guid, hdr.EventDescriptor.Level, hdr.EventDescriptor.Keyword) ||
                session->isProviderEnabled(SystemTraceControlGuid, hdr.EventDescriptor.Level, 0)) {
                uint64_t wraps = 0, lost = 0, flushed = 0;
                session->recordEvent(hdr, pData, size, msg, nullptr, nullptr, &wraps, &lost, &flushed);
                m_telemetry.totalCircularBufferWraps += wraps;
                m_telemetry.totalEventsLost += lost;
                m_telemetry.totalBuffersFlushed += flushed;
                logged = true;
            }
        }
        if (logged) {
            m_telemetry.totalKernelEventsLogged++;
            m_telemetry.totalEventsWritten++;
        }
    }

    void traceProcess(uint8_t opcode, uint32_t pid, uint32_t parentPid, uint32_t sessionId, int32_t exitStatus, const char* imageName) {
        ETW_PROCESS_EVENT pe{};
        pe.ProcessId = pid;
        pe.ParentProcessId = parentPid;
        pe.SessionId = sessionId;
        pe.ExitStatus = exitStatus;
        pe.DirectoryTableBase = 0x10000000;
        details::safeCopyAnsi(pe.ImageFileName, sizeof(pe.ImageFileName), imageName ? imageName : "unknown");

        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        EVENT_HEADER hdr{};
        hdr.Size = static_cast<uint16_t>(sizeof(EVENT_HEADER) + sizeof(pe));
        hdr.TimeStamp = ts;
        hdr.ProviderId = ProcessTraceGuid;
        hdr.EventDescriptor.Id = 1;
        hdr.EventDescriptor.Opcode = opcode;
        hdr.EventDescriptor.Level = TRACE_LEVEL_INFORMATION;
        hdr.ProcessId = pid;
        hdr.ThreadId = 1;

        recordKernelEvent(ProcessTraceGuid, EVENT_TRACE_FLAG_PROCESS, hdr, &pe, sizeof(pe));
    }

    void traceThread(uint8_t opcode, uint32_t pid, uint32_t tid, uint64_t startAddr, int32_t exitStatus) {
        ETW_THREAD_EVENT te{};
        te.ProcessId = pid;
        te.ThreadId = tid;
        te.Win32StartAddress = startAddr;
        te.ExitStatus = exitStatus;

        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        EVENT_HEADER hdr{};
        hdr.Size = static_cast<uint16_t>(sizeof(EVENT_HEADER) + sizeof(te));
        hdr.TimeStamp = ts;
        hdr.ProviderId = ThreadTraceGuid;
        hdr.EventDescriptor.Id = 2;
        hdr.EventDescriptor.Opcode = opcode;
        hdr.EventDescriptor.Level = TRACE_LEVEL_INFORMATION;
        hdr.ProcessId = pid;
        hdr.ThreadId = tid;

        recordKernelEvent(ThreadTraceGuid, EVENT_TRACE_FLAG_THREAD, hdr, &te, sizeof(te));
    }

    void traceImageLoad(uint32_t pid, uint64_t base, uint64_t size, const wchar_t* path) {
        ETW_IMAGE_LOAD_EVENT ie{};
        ie.ProcessId = pid;
        ie.ImageBase = base;
        ie.ImageSize = size;
        details::safeCopyWide(ie.FileName, sizeof(ie.FileName) / sizeof(wchar_t), path ? path : L"");

        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        EVENT_HEADER hdr{};
        hdr.Size = static_cast<uint16_t>(sizeof(EVENT_HEADER) + sizeof(ie));
        hdr.TimeStamp = ts;
        hdr.ProviderId = ImageLoadTraceGuid;
        hdr.EventDescriptor.Id = 3;
        hdr.EventDescriptor.Opcode = 10; // Load
        hdr.EventDescriptor.Level = TRACE_LEVEL_INFORMATION;
        hdr.ProcessId = pid;
        hdr.ThreadId = 1;

        recordKernelEvent(ImageLoadTraceGuid, EVENT_TRACE_FLAG_IMAGE_LOAD, hdr, &ie, sizeof(ie));
    }

    void traceDiskIo(uint8_t opcode, uint32_t diskNumber, uint64_t byteOffset, uint32_t size) {
        ETW_DISK_IO_EVENT de{};
        de.DiskNumber = diskNumber;
        de.ByteOffset = byteOffset;
        de.TransferSize = size;

        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        EVENT_HEADER hdr{};
        hdr.Size = static_cast<uint16_t>(sizeof(EVENT_HEADER) + sizeof(de));
        hdr.TimeStamp = ts;
        hdr.ProviderId = DiskIoTraceGuid;
        hdr.EventDescriptor.Id = 4;
        hdr.EventDescriptor.Opcode = opcode;
        hdr.EventDescriptor.Level = TRACE_LEVEL_INFORMATION;
        hdr.ProcessId = 4; // System
        hdr.ThreadId = 1;

        recordKernelEvent(DiskIoTraceGuid, EVENT_TRACE_FLAG_DISK_IO, hdr, &de, sizeof(de));
    }

    void tracePageFault(uint32_t pid, uint64_t virtualAddr, uint64_t pc, uint32_t faultType) {
        ETW_PAGE_FAULT_EVENT pfe{};
        pfe.ProcessId = pid;
        pfe.VirtualAddress = virtualAddr;
        pfe.ProgramCounter = pc;
        pfe.FaultType = faultType;

        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        EVENT_HEADER hdr{};
        hdr.Size = static_cast<uint16_t>(sizeof(EVENT_HEADER) + sizeof(pfe));
        hdr.TimeStamp = ts;
        hdr.ProviderId = PageFaultTraceGuid;
        hdr.EventDescriptor.Id = 5;
        hdr.EventDescriptor.Opcode = static_cast<uint8_t>(10 + faultType);
        hdr.EventDescriptor.Level = TRACE_LEVEL_INFORMATION;
        hdr.ProcessId = pid;
        hdr.ThreadId = 1;

        recordKernelEvent(PageFaultTraceGuid, EVENT_TRACE_FLAG_MEMORY_PAGE_FAULTS, hdr, &pfe, sizeof(pfe));
    }

    void traceAlpc(uint8_t opcode, uint32_t msgId, uint32_t srcPid, uint32_t dstPid, uint32_t msgType) {
        ETW_ALPC_EVENT ae{};
        ae.MessageId = msgId;
        ae.SourceProcessId = srcPid;
        ae.TargetProcessId = dstPid;
        ae.MessageType = msgType;

        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        EVENT_HEADER hdr{};
        hdr.Size = static_cast<uint16_t>(sizeof(EVENT_HEADER) + sizeof(ae));
        hdr.TimeStamp = ts;
        hdr.ProviderId = AlpcTraceGuid;
        hdr.EventDescriptor.Id = 6;
        hdr.EventDescriptor.Opcode = opcode;
        hdr.EventDescriptor.Level = TRACE_LEVEL_INFORMATION;
        hdr.ProcessId = srcPid;
        hdr.ThreadId = 1;

        recordKernelEvent(AlpcTraceGuid, EVENT_TRACE_FLAG_ALPC, hdr, &ae, sizeof(ae));
    }

    void traceRegistry(uint8_t opcode, uint64_t keyHandle, uint32_t opType, const wchar_t* keyName, uint32_t status) {
        ETW_REGISTRY_EVENT re{};
        re.KeyHandle = keyHandle;
        re.OpType = opType;
        re.Status = status;
        details::safeCopyWide(re.KeyName, sizeof(re.KeyName) / sizeof(wchar_t), keyName ? keyName : L"");

        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        EVENT_HEADER hdr{};
        hdr.Size = static_cast<uint16_t>(sizeof(EVENT_HEADER) + sizeof(re));
        hdr.TimeStamp = ts;
        hdr.ProviderId = RegistryTraceGuid;
        hdr.EventDescriptor.Id = 7;
        hdr.EventDescriptor.Opcode = opcode;
        hdr.EventDescriptor.Level = TRACE_LEVEL_INFORMATION;
        hdr.ProcessId = 1000;
        hdr.ThreadId = 1;

        recordKernelEvent(RegistryTraceGuid, EVENT_TRACE_FLAG_REGISTRY, hdr, &re, sizeof(re));
    }

private:
    void initializeKernelLogger() {
        std::lock_guard<std::mutex> lock(m_mutex);
        initializeKernelLoggerUnlocked();
    }

    void initializeKernelLoggerUnlocked() {
        EVENT_TRACE_PROPERTIES props{};
        props.Wnode.BufferSize = sizeof(EVENT_TRACE_PROPERTIES);
        props.BufferSize = 64;
        props.MinimumBuffers = 4;
        props.MaximumBuffers = 64;
        props.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
        props.EnableFlags = EVENT_TRACE_FLAG_PROCESS | EVENT_TRACE_FLAG_THREAD | EVENT_TRACE_FLAG_DISK_IO |
                            EVENT_TRACE_FLAG_NETWORK_TCPIP | EVENT_TRACE_FLAG_MEMORY_PAGE_FAULTS |
                            EVENT_TRACE_FLAG_ALPC | EVENT_TRACE_FLAG_REGISTRY;

        TRACEHANDLE h = m_nextTraceHandle++;
        uint16_t id = m_nextSessionId++;
        auto session = std::make_shared<TraceSession>(id, L"NT Kernel Logger", props, L"");
        session->enableProvider(SystemTraceControlGuid, TRACE_LEVEL_VERBOSE, 0xFFFFFFFFFFFFFFFFULL, 0);

        m_sessions[h] = session;
        m_sessionByName[L"NT Kernel Logger"] = h;
    }

    mutable std::mutex m_mutex;
    std::unordered_map<REGHANDLE, RegisteredProvider> m_providers;
    std::unordered_map<TRACEHANDLE, std::shared_ptr<TraceSession>> m_sessions;
    std::unordered_map<std::wstring, TRACEHANDLE> m_sessionByName;
    std::unordered_map<TRACEHANDLE, std::shared_ptr<EVENT_TRACE_LOGFILEW>> m_consumers;
    ETW_TELEMETRY m_telemetry{};

    REGHANDLE   m_nextRegHandle{0x1000};
    TRACEHANDLE m_nextTraceHandle{0x2000};
    uint16_t    m_nextSessionId{1};
};

// ============================================================================
// 7. advapi32.dll Standard ETW Export Bridge
// ============================================================================

inline uint32_t __stdcall StartTraceW(TRACEHANDLE* SessionHandle, const wchar_t* InstanceName, EVENT_TRACE_PROPERTIES* Properties) {
    return TraceManager::get().startTrace(SessionHandle, InstanceName, Properties);
}

inline uint32_t __stdcall StartTraceA(TRACEHANDLE* SessionHandle, const char* InstanceName, EVENT_TRACE_PROPERTIES* Properties) {
    if (!InstanceName) return ERROR_INVALID_PARAMETER;
    std::string s(InstanceName);
    std::wstring ws(s.begin(), s.end());
    return TraceManager::get().startTrace(SessionHandle, ws.c_str(), Properties);
}

inline uint32_t __stdcall StopTraceW(TRACEHANDLE SessionHandle, const wchar_t* InstanceName, EVENT_TRACE_PROPERTIES* Properties) {
    return TraceManager::get().stopTrace(SessionHandle, InstanceName, Properties);
}

inline uint32_t __stdcall StopTraceA(TRACEHANDLE SessionHandle, const char* InstanceName, EVENT_TRACE_PROPERTIES* Properties) {
    std::wstring ws;
    if (InstanceName) {
        std::string s(InstanceName);
        ws.assign(s.begin(), s.end());
    }
    return TraceManager::get().stopTrace(SessionHandle, ws.empty() ? nullptr : ws.c_str(), Properties);
}

inline uint32_t __stdcall QueryTraceW(TRACEHANDLE SessionHandle, const wchar_t* InstanceName, EVENT_TRACE_PROPERTIES* Properties) {
    return TraceManager::get().queryTrace(SessionHandle, InstanceName, Properties);
}

inline uint32_t __stdcall QueryTraceA(TRACEHANDLE SessionHandle, const char* InstanceName, EVENT_TRACE_PROPERTIES* Properties) {
    std::wstring ws;
    if (InstanceName) {
        std::string s(InstanceName);
        ws.assign(s.begin(), s.end());
    }
    return TraceManager::get().queryTrace(SessionHandle, ws.empty() ? nullptr : ws.c_str(), Properties);
}

inline uint32_t __stdcall UpdateTraceW(TRACEHANDLE SessionHandle, const wchar_t* InstanceName, EVENT_TRACE_PROPERTIES* Properties) {
    return TraceManager::get().updateTrace(SessionHandle, InstanceName, Properties);
}

inline uint32_t __stdcall UpdateTraceA(TRACEHANDLE SessionHandle, const char* InstanceName, EVENT_TRACE_PROPERTIES* Properties) {
    std::wstring ws;
    if (InstanceName) {
        std::string s(InstanceName);
        ws.assign(s.begin(), s.end());
    }
    return TraceManager::get().updateTrace(SessionHandle, ws.empty() ? nullptr : ws.c_str(), Properties);
}

inline uint32_t __stdcall FlushTraceW(TRACEHANDLE SessionHandle, const wchar_t* InstanceName, EVENT_TRACE_PROPERTIES* Properties) {
    return TraceManager::get().flushTrace(SessionHandle, InstanceName, Properties);
}

inline uint32_t __stdcall FlushTraceA(TRACEHANDLE SessionHandle, const char* InstanceName, EVENT_TRACE_PROPERTIES* Properties) {
    std::wstring ws;
    if (InstanceName) {
        std::string s(InstanceName);
        ws.assign(s.begin(), s.end());
    }
    return TraceManager::get().flushTrace(SessionHandle, ws.empty() ? nullptr : ws.c_str(), Properties);
}

inline uint32_t __stdcall ControlTraceW(TRACEHANDLE SessionHandle, const wchar_t* InstanceName, EVENT_TRACE_PROPERTIES* Properties, uint32_t ControlCode) {
    return TraceManager::get().controlTrace(SessionHandle, InstanceName, Properties, ControlCode);
}

inline uint32_t __stdcall ControlTraceA(TRACEHANDLE SessionHandle, const char* InstanceName, EVENT_TRACE_PROPERTIES* Properties, uint32_t ControlCode) {
    std::wstring ws;
    if (InstanceName) {
        std::string s(InstanceName);
        ws.assign(s.begin(), s.end());
    }
    return TraceManager::get().controlTrace(SessionHandle, ws.empty() ? nullptr : ws.c_str(), Properties, ControlCode);
}

inline uint32_t __stdcall EnableTraceEx2(TRACEHANDLE TraceHandle, const GUID* ProviderId, uint32_t ControlCode,
                                         uint8_t Level, uint64_t MatchAnyKeyword, uint64_t MatchAllKeyword,
                                         uint32_t Timeout, ENABLE_TRACE_PARAMETERS* EnableParameters) {
    return TraceManager::get().enableTraceEx2(TraceHandle, ProviderId, ControlCode, Level, MatchAnyKeyword, MatchAllKeyword, Timeout, EnableParameters);
}

inline uint32_t __stdcall EventRegister(const GUID* ProviderId, PENABLECALLBACK EnableCallback, void* CallbackContext, REGHANDLE* RegHandle) {
    return TraceManager::get().registerProvider(ProviderId, EnableCallback, CallbackContext, RegHandle);
}

inline uint32_t __stdcall EventUnregister(REGHANDLE RegHandle) {
    return TraceManager::get().unregisterProvider(RegHandle);
}

inline uint8_t __stdcall EventEnabled(REGHANDLE RegHandle, const EVENT_DESCRIPTOR* EventDescriptor) {
    return TraceManager::get().isEventEnabled(RegHandle, EventDescriptor) ? 1 : 0;
}

inline uint8_t __stdcall EventProviderEnabled(REGHANDLE RegHandle, uint8_t Level, uint64_t Keyword) {
    return TraceManager::get().isProviderEnabled(RegHandle, Level, Keyword) ? 1 : 0;
}

inline uint32_t __stdcall EventWrite(REGHANDLE RegHandle, const EVENT_DESCRIPTOR* EventDescriptor, uint32_t UserDataCount, const EVENT_DATA_DESCRIPTOR* UserData) {
    return TraceManager::get().writeEvent(RegHandle, EventDescriptor, UserDataCount, UserData);
}

inline uint32_t __stdcall EventWriteString(REGHANDLE RegHandle, uint8_t Level, uint64_t Keyword, const wchar_t* String) {
    return TraceManager::get().writeString(RegHandle, Level, Keyword, String);
}

inline uint32_t __stdcall EventWriteTransfer(REGHANDLE RegHandle, const EVENT_DESCRIPTOR* EventDescriptor,
                                             const GUID* ActivityId, const GUID* RelatedActivityId,
                                             uint32_t UserDataCount, const EVENT_DATA_DESCRIPTOR* UserData) {
    return TraceManager::get().writeEventTransfer(RegHandle, EventDescriptor, ActivityId, RelatedActivityId, UserDataCount, UserData);
}

inline TRACEHANDLE __stdcall OpenTraceW(EVENT_TRACE_LOGFILEW* Logfile) {
    return TraceManager::get().openTrace(Logfile);
}

inline uint32_t __stdcall ProcessTrace(TRACEHANDLE* HandleArray, uint32_t HandleCount, void* StartTime, void* EndTime) {
    return TraceManager::get().processTrace(HandleArray, HandleCount, StartTime, EndTime);
}

inline uint32_t __stdcall CloseTrace(TRACEHANDLE TraceHandle) {
    return TraceManager::get().closeTrace(TraceHandle);
}

// ============================================================================
// 8. ntdll.dll Native System Event Tracing Export Bridge
// ============================================================================

inline int32_t __stdcall EtwEventRegister(const GUID* ProviderId, PENABLECALLBACK EnableCallback, void* CallbackContext, REGHANDLE* RegHandle) {
    uint32_t r = TraceManager::get().registerProvider(ProviderId, EnableCallback, CallbackContext, RegHandle);
    return (r == ERROR_SUCCESS) ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
}

inline int32_t __stdcall EtwEventUnregister(REGHANDLE RegHandle) {
    uint32_t r = TraceManager::get().unregisterProvider(RegHandle);
    return (r == ERROR_SUCCESS) ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
}

inline uint8_t __stdcall EtwEventEnabled(REGHANDLE RegHandle, const EVENT_DESCRIPTOR* EventDescriptor) {
    return TraceManager::get().isEventEnabled(RegHandle, EventDescriptor) ? 1 : 0;
}

inline int32_t __stdcall EtwEventWrite(REGHANDLE RegHandle, const EVENT_DESCRIPTOR* EventDescriptor, uint32_t UserDataCount, const EVENT_DATA_DESCRIPTOR* UserData) {
    uint32_t r = TraceManager::get().writeEvent(RegHandle, EventDescriptor, UserDataCount, UserData);
    return (r == ERROR_SUCCESS) ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
}

inline int32_t __stdcall EtwEventWriteString(REGHANDLE RegHandle, uint8_t Level, uint64_t Keyword, const wchar_t* String) {
    uint32_t r = TraceManager::get().writeString(RegHandle, Level, Keyword, String);
    return (r == ERROR_SUCCESS) ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
}

inline int32_t __stdcall EtwEventWriteTransfer(REGHANDLE RegHandle, const EVENT_DESCRIPTOR* EventDescriptor,
                                               const GUID* ActivityId, const GUID* RelatedActivityId,
                                               uint32_t UserDataCount, const EVENT_DATA_DESCRIPTOR* UserData) {
    uint32_t r = TraceManager::get().writeEventTransfer(RegHandle, EventDescriptor, ActivityId, RelatedActivityId, UserDataCount, UserData);
    return (r == ERROR_SUCCESS) ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
}

// ============================================================================
// 8b. Sovereign Kernel-Mode System Tracing & Telemetry API
// ============================================================================

inline void EtwTraceProcess(uint8_t opcode, uint32_t pid, uint32_t parentPid, uint32_t sessionId, int32_t exitStatus, const char* imageName) {
    TraceManager::get().traceProcess(opcode, pid, parentPid, sessionId, exitStatus, imageName);
}

inline void EtwTraceThread(uint8_t opcode, uint32_t pid, uint32_t tid, uint64_t startAddr, int32_t exitStatus) {
    TraceManager::get().traceThread(opcode, pid, tid, startAddr, exitStatus);
}

inline void EtwTraceImageLoad(uint32_t pid, uint64_t base, uint64_t size, const wchar_t* path) {
    TraceManager::get().traceImageLoad(pid, base, size, path);
}

inline void EtwTraceDiskIo(uint8_t opcode, uint32_t diskNumber, uint64_t byteOffset, uint32_t size) {
    TraceManager::get().traceDiskIo(opcode, diskNumber, byteOffset, size);
}

inline void EtwTracePageFault(uint32_t pid, uint64_t virtualAddr, uint64_t pc, uint32_t faultType) {
    TraceManager::get().tracePageFault(pid, virtualAddr, pc, faultType);
}

inline void EtwTraceAlpc(uint8_t opcode, uint32_t msgId, uint32_t srcPid, uint32_t dstPid, uint32_t msgType) {
    TraceManager::get().traceAlpc(opcode, msgId, srcPid, dstPid, msgType);
}

inline void EtwTraceRegistry(uint8_t opcode, uint64_t keyHandle, uint32_t opType, const wchar_t* keyName, uint32_t status) {
    TraceManager::get().traceRegistry(opcode, keyHandle, opType, keyName, status);
}

inline ETW_TELEMETRY EtwQueryTelemetry() {
    return TraceManager::get().getTelemetry();
}

inline void EtwResetTelemetryForTesting() {
    TraceManager::get().resetTelemetry();
}

// ============================================================================
// 9. Dynamic Loader & SCM Registration
// ============================================================================

inline void InitializeEtwSubsystemExports() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    auto& ldr = ldr::DynamicLoader::get();

    // 1. advapi32.dll ETW exports
    ldr.registerExport("advapi32.dll", "StartTraceW", reinterpret_cast<void*>(StartTraceW));
    ldr.registerExport("advapi32.dll", "StartTraceA", reinterpret_cast<void*>(StartTraceA));
    ldr.registerExport("advapi32.dll", "StopTraceW", reinterpret_cast<void*>(StopTraceW));
    ldr.registerExport("advapi32.dll", "StopTraceA", reinterpret_cast<void*>(StopTraceA));
    ldr.registerExport("advapi32.dll", "QueryTraceW", reinterpret_cast<void*>(QueryTraceW));
    ldr.registerExport("advapi32.dll", "QueryTraceA", reinterpret_cast<void*>(QueryTraceA));
    ldr.registerExport("advapi32.dll", "UpdateTraceW", reinterpret_cast<void*>(UpdateTraceW));
    ldr.registerExport("advapi32.dll", "UpdateTraceA", reinterpret_cast<void*>(UpdateTraceA));
    ldr.registerExport("advapi32.dll", "FlushTraceW", reinterpret_cast<void*>(FlushTraceW));
    ldr.registerExport("advapi32.dll", "FlushTraceA", reinterpret_cast<void*>(FlushTraceA));
    ldr.registerExport("advapi32.dll", "ControlTraceW", reinterpret_cast<void*>(ControlTraceW));
    ldr.registerExport("advapi32.dll", "ControlTraceA", reinterpret_cast<void*>(ControlTraceA));
    ldr.registerExport("advapi32.dll", "EnableTraceEx2", reinterpret_cast<void*>(EnableTraceEx2));
    ldr.registerExport("advapi32.dll", "EventRegister", reinterpret_cast<void*>(EventRegister));
    ldr.registerExport("advapi32.dll", "EventUnregister", reinterpret_cast<void*>(EventUnregister));
    ldr.registerExport("advapi32.dll", "EventEnabled", reinterpret_cast<void*>(EventEnabled));
    ldr.registerExport("advapi32.dll", "EventProviderEnabled", reinterpret_cast<void*>(EventProviderEnabled));
    ldr.registerExport("advapi32.dll", "EventWrite", reinterpret_cast<void*>(EventWrite));
    ldr.registerExport("advapi32.dll", "EventWriteString", reinterpret_cast<void*>(EventWriteString));
    ldr.registerExport("advapi32.dll", "EventWriteTransfer", reinterpret_cast<void*>(EventWriteTransfer));
    ldr.registerExport("advapi32.dll", "OpenTraceW", reinterpret_cast<void*>(OpenTraceW));
    ldr.registerExport("advapi32.dll", "ProcessTrace", reinterpret_cast<void*>(ProcessTrace));
    ldr.registerExport("advapi32.dll", "CloseTrace", reinterpret_cast<void*>(CloseTrace));

    // 2. ntdll.dll Native ETW exports
    ldr.registerExport("ntdll.dll", "EtwEventRegister", reinterpret_cast<void*>(EtwEventRegister));
    ldr.registerExport("ntdll.dll", "EtwEventUnregister", reinterpret_cast<void*>(EtwEventUnregister));
    ldr.registerExport("ntdll.dll", "EtwEventEnabled", reinterpret_cast<void*>(EtwEventEnabled));
    ldr.registerExport("ntdll.dll", "EtwEventWrite", reinterpret_cast<void*>(EtwEventWrite));
    ldr.registerExport("ntdll.dll", "EtwEventWriteString", reinterpret_cast<void*>(EtwEventWriteString));
    ldr.registerExport("ntdll.dll", "EtwEventWriteTransfer", reinterpret_cast<void*>(EtwEventWriteTransfer));

    // 3. SCM Service: DiagTrack ("Connected User Experiences and Telemetry")
    auto& scm = scm::ServiceControlManager::get();
    auto diagTrackRecord = std::make_shared<scm::ServiceRecord>();
    diagTrackRecord->serviceName = L"DiagTrack";
    diagTrackRecord->displayName = L"Connected User Experiences and Telemetry";
    diagTrackRecord->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    diagTrackRecord->startType = scm::SERVICE_AUTO_START;
    diagTrackRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    diagTrackRecord->svchostGroup = "utcsvc";
    diagTrackRecord->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k utcsvc -p";
    diagTrackRecord->status.dwServiceType = diagTrackRecord->serviceType;
    diagTrackRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    diagTrackRecord->status.dwProcessId = scm::SvcHostManager::get().getOrCreateGroupProcess("utcsvc");
    scm::SvcHostManager::get().assignService("utcsvc", diagTrackRecord->serviceName);
    scm.registerServiceRecord(diagTrackRecord);
}

} // namespace micant::etw
