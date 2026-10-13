// ============================================================================
// MicaNT: I/O Filter Manager & Minifilter Driver Architecture (fltmgr.hpp)
// Sovereign System Domain: TitanFltMgr / AegisFilter
//
// Description:
//   Clean-room implementation of the Microsoft Windows Filter Manager Subsystem
//   (fltmgr.sys). Provides altitude-based minifilter stacking and arbitration,
//   pre- and post-operation callback dispatch chains, rich context management
//   (Volume, Instance, File, Stream, and StreamHandle contexts), kernel-to-user
//   bidirectional communication ports (FilterConnectCommunicationPort /
//   FilterSendMessage), and exemplar minifilters for Real-Time Threat Scanning
//   (SentinelScanFlt @ 328000), Transparent Encryption (EmeraldFlt @ 141000),
//   and Filesystem Activity Monitoring (MicaActivityFlt @ 385000).
//
// Clean-Room Engineering Reference & Standards:
//   - Microsoft Windows Filter Manager Architecture Specification
//   - Microsoft File System Minifilter Driver Developer Guide
//   - Standard Altitude Group Allocations for Filter Drivers
//   - ISO/IEC 14882:2023 C++ Standard (Pure ISO C++23)
//   - Zero proprietary, decompiled, or leaked Microsoft source code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <algorithm>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "janusldr.hpp"
#include "scm.hpp"
#include "version.hpp"

namespace micant::fltmgr {

// ============================================================================
// 1. Basic Win32 / NT Types & Constants
// ============================================================================

using BOOL  = int32_t;
using UINT  = uint32_t;
using DWORD = uint32_t;
using WORD  = uint16_t;
using ULONG = uint32_t;
using UCHAR = uint8_t;

inline constexpr BOOL TRUE_VAL  = 1;
inline constexpr BOOL FALSE_VAL = 0;

// Filter Manager Standard Altitude Ranges
inline constexpr const char* ALTITUDE_FSFILTER_TOP              = "400000";
inline constexpr const char* ALTITUDE_ACTIVITY_MONITOR          = "385000";
inline constexpr const char* ALTITUDE_UNDELETE                  = "365000";
inline constexpr const char* ALTITUDE_ANTI_VIRUS                = "328000";
inline constexpr const char* ALTITUDE_REPLICATION               = "265000";
inline constexpr const char* ALTITUDE_CONTINUOUS_BACKUP         = "245000";
inline constexpr const char* ALTITUDE_SECURITY_ENHANCER         = "185000";
inline constexpr const char* ALTITUDE_OPEN_FILE_BACKUP          = "165000";
inline constexpr const char* ALTITUDE_ENCRYPTION                = "141000";
inline constexpr const char* ALTITUDE_COMPRESSION               = "105000";
inline constexpr const char* ALTITUDE_FSFILTER_BOTTOM           = "45000";

// Major Function Codes (IRP_MJ_*)
inline constexpr UCHAR IRP_MJ_CREATE                   = 0x00;
inline constexpr UCHAR IRP_MJ_CREATE_NAMED_PIPE        = 0x01;
inline constexpr UCHAR IRP_MJ_CLOSE                    = 0x02;
inline constexpr UCHAR IRP_MJ_READ                     = 0x03;
inline constexpr UCHAR IRP_MJ_WRITE                    = 0x04;
inline constexpr UCHAR IRP_MJ_QUERY_INFORMATION        = 0x05;
inline constexpr UCHAR IRP_MJ_SET_INFORMATION          = 0x06;
inline constexpr UCHAR IRP_MJ_DIRECTORY_CONTROL        = 0x0C;
inline constexpr UCHAR IRP_MJ_FILE_SYSTEM_CONTROL      = 0x0D;
inline constexpr UCHAR IRP_MJ_DEVICE_CONTROL           = 0x0E;
inline constexpr UCHAR IRP_MJ_SHUTDOWN                 = 0x10;
inline constexpr UCHAR IRP_MJ_CLEANUP                  = 0x12;

// Operation Flags
inline constexpr uint32_t FLTFL_CALLBACK_DATA_DIRTY           = 0x00000001;
inline constexpr uint32_t FLTFL_CALLBACK_DATA_FAST_IO_OPERATION= 0x00000002;
inline constexpr uint32_t FLTFL_CALLBACK_DATA_POST_OPERATION  = 0x00000004;

// Post-Operation Flags
inline constexpr uint32_t FLTFL_POST_OPERATION_DRAINING       = 0x00000001;

// ============================================================================
// 2. Filter Status Enumerations
// ============================================================================

enum class FltPreopCallbackStatus : uint32_t {
    SuccessWithCallback        = 0, // FLT_PREOP_SUCCESS_WITH_CALLBACK
    SuccessNoCallback          = 1, // FLT_PREOP_SUCCESS_NO_CALLBACK
    Pending                    = 2, // FLT_PREOP_PENDING
    Complete                   = 3, // FLT_PREOP_COMPLETE (short-circuit I/O)
    Synchronize                = 4, // FLT_PREOP_SYNCHRONIZE
    DisallowFastIo             = 5  // FLT_PREOP_DISALLOW_FASTIO
};

enum class FltPostopCallbackStatus : uint32_t {
    FinishedProcessing         = 0, // FLT_POSTOP_FINISHED_PROCESSING
    MoreProcessingRequired     = 1  // FLT_POSTOP_MORE_PROCESSING_REQUIRED
};

enum class FltContextType : uint16_t {
    VolumeContext              = 0x0001,
    InstanceContext            = 0x0002,
    FileContext                = 0x0004,
    StreamContext              = 0x0008,
    StreamHandleContext        = 0x0010,
    TransactionContext         = 0x0020
};

// ============================================================================
// 3. Callback Data & Parameters (FLT_CALLBACK_DATA)
// ============================================================================

struct FLT_IO_PARAMETER_BLOCK {
    uint8_t MajorFunction{IRP_MJ_CREATE};
    uint8_t MinorFunction{0};
    uint32_t OperationFlags{0};
    uint64_t TargetFileObject{0};
    std::wstring TargetFilePath{};

    // Read/Write parameters
    struct {
        uint32_t Length{0};
        uint64_t ByteOffset{0};
        void* Buffer{nullptr};
    } ReadWrite{};

    // Create parameters
    struct {
        uint32_t DesiredAccess{0};
        uint32_t CreateDisposition{0};
        uint32_t ShareAccess{0};
    } Create{};

    // Custom tag/data
    uint64_t ParameterExtra{0};
};

struct FLT_CALLBACK_DATA {
    uint32_t Flags{0};
    uint64_t ThreadId{1000};
    uint32_t RequestorMode{0}; // 0 = KernelMode, 1 = UserMode
    FLT_IO_PARAMETER_BLOCK Iopb{};
    struct {
        NtStatus Status{NtStatus::Success};
        uint64_t Information{0};
    } IoStatus{};
};

// ============================================================================
// 4. Minifilter Context Model
// ============================================================================

class FltContext {
public:
    FltContext(FltContextType type, size_t size, uint32_t tag)
        : m_type(type), m_size(size), m_tag(tag), m_refCount(1) {
        m_data.resize(size, 0);
    }

    FltContextType getType() const noexcept { return m_type; }
    size_t getSize() const noexcept { return m_size; }
    uint32_t getTag() const noexcept { return m_tag; }
    int32_t getRefCount() const noexcept { return m_refCount.load(); }

    void* getData() noexcept { return m_data.data(); }
    const void* getData() const noexcept { return m_data.data(); }

    void addRef() noexcept {
        m_refCount.fetch_add(1, std::memory_order_relaxed);
    }

    bool release() noexcept {
        return m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

private:
    FltContextType m_type;
    size_t m_size;
    uint32_t m_tag;
    std::atomic<int32_t> m_refCount;
    std::vector<uint8_t> m_data;
};

// ============================================================================
// 5. Minifilter Callback Definitions & Registration
// ============================================================================

using PFLT_PRE_OPERATION_CALLBACK = std::function<FltPreopCallbackStatus(
    FLT_CALLBACK_DATA* Data,
    void** CompletionContext
)>;

using PFLT_POST_OPERATION_CALLBACK = std::function<FltPostopCallbackStatus(
    FLT_CALLBACK_DATA* Data,
    void* CompletionContext,
    uint32_t Flags
)>;

struct FLT_OPERATION_REGISTRATION {
    uint8_t MajorFunction;
    uint32_t Flags;
    PFLT_PRE_OPERATION_CALLBACK PreOperation;
    PFLT_POST_OPERATION_CALLBACK PostOperation;
};

struct FLT_REGISTRATION {
    uint16_t Size{sizeof(FLT_REGISTRATION)};
    uint16_t Version{0x0200}; // Filter Manager 2.0
    uint32_t Flags{0};
    std::wstring FilterName{};
    std::string Altitude{"300000"};
    std::vector<FLT_OPERATION_REGISTRATION> OperationRegistration{};
    std::function<NtStatus(uint32_t Flags)> FilterUnloadCallback{nullptr};
};

// ============================================================================
// 6. Communication Port Architecture (Kernel-to-User IPC)
// ============================================================================

using PFLT_CONNECT_NOTIFY = std::function<NtStatus(
    uint32_t ClientPortHandle,
    const void* ConnectionContext,
    uint32_t SizeOfContext,
    void** ConnectionPortCookie
)>;

using PFLT_DISCONNECT_NOTIFY = std::function<void(void* ConnectionCookie)>;

using PFLT_MESSAGE_NOTIFY = std::function<NtStatus(
    void* PortCookie,
    const void* InputBuffer,
    uint32_t InputBufferSize,
    void* OutputBuffer,
    uint32_t OutputBufferSize,
    uint32_t* ReturnOutputBufferLength
)>;

struct FltCommunicationPort {
    uint32_t PortId{0};
    std::wstring PortName{};
    uint32_t FilterHandle{0};
    void* ServerCookie{nullptr};
    PFLT_CONNECT_NOTIFY ConnectNotify{nullptr};
    PFLT_DISCONNECT_NOTIFY DisconnectNotify{nullptr};
    PFLT_MESSAGE_NOTIFY MessageNotify{nullptr};
    uint32_t MaxConnections{1};
    uint32_t CurrentConnections{0};
    bool IsClosed{false};
};

// ============================================================================
// 7. Telemetry Structure
// ============================================================================

struct FLT_TELEMETRY {
    uint32_t activeFiltersCount{0};
    uint32_t activeInstancesCount{0};
    uint32_t activePortsCount{0};
    uint32_t activeContextsCount{0};
    uint64_t totalPreOpsDispatched{0};
    uint64_t totalPostOpsDispatched{0};
    uint64_t totalOpsShortCircuited{0};
    uint64_t totalMalwareBlocked{0};
    uint64_t totalCryptoOpsProcessed{0};
    uint64_t totalPortMessagesSent{0};
};

// ============================================================================
// 8. Registered Minifilter Representation
// ============================================================================

struct RegisteredFilter {
    uint32_t FilterHandle{0};
    FLT_REGISTRATION Registration{};
    double NumericAltitude{300000.0};
    bool IsStarted{false};
};

// ============================================================================
// 9. Sovereign Filter Manager Subsystem (TitanFilterManagerSubsystem)
// ============================================================================

class TitanFilterManagerSubsystem {
public:
    static TitanFilterManagerSubsystem& Instance() noexcept {
        static TitanFilterManagerSubsystem s_instance;
        return s_instance;
    }

    void initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return;

        m_nextFilterHandle = 1;
        m_nextPortHandle = 100;
        m_nextClientHandle = 500;
        m_filters.clear();
        m_ports.clear();
        m_clientToPortMap.clear();
        m_fileContexts.clear();
        m_streamContexts.clear();

        // Seed Default Exemplar Minifilters
        registerDefaultMinifilters();

        m_initialized = true;
    }

    void resetForTesting() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_filters.clear();
        m_ports.clear();
        m_clientToPortMap.clear();
        m_fileContexts.clear();
        m_streamContexts.clear();
        m_activityLog.clear();
        m_telemetry = FLT_TELEMETRY{};
        m_initialized = false;
        initialize();
    }

    // --- Minifilter Lifecycle ---

    NtStatus registerFilter(const FLT_REGISTRATION& reg, uint32_t* outFilterHandle) {
        if (!outFilterHandle) return NtStatus::InvalidParameter;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        uint32_t handle = m_nextFilterHandle++;
        RegisteredFilter rf{};
        rf.FilterHandle = handle;
        rf.Registration = reg;
        try {
            rf.NumericAltitude = std::stod(reg.Altitude);
        } catch (...) {
            rf.NumericAltitude = 300000.0;
        }
        rf.IsStarted = false;

        m_filters[handle] = std::move(rf);
        *outFilterHandle = handle;
        m_telemetry.activeFiltersCount = static_cast<uint32_t>(m_filters.size());
        return NtStatus::Success;
    }

    NtStatus startFiltering(uint32_t filterHandle) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_filters.find(filterHandle);
        if (it == m_filters.end()) return NtStatus::NotFound;
        it->second.IsStarted = true;
        m_telemetry.activeInstancesCount++;
        return NtStatus::Success;
    }

    NtStatus unregisterFilter(uint32_t filterHandle) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_filters.find(filterHandle);
        if (it == m_filters.end()) return NtStatus::NotFound;

        if (it->second.Registration.FilterUnloadCallback) {
            it->second.Registration.FilterUnloadCallback(0);
        }

        m_filters.erase(it);
        m_telemetry.activeFiltersCount = static_cast<uint32_t>(m_filters.size());
        if (m_telemetry.activeInstancesCount > 0) m_telemetry.activeInstancesCount--;
        return NtStatus::Success;
    }

    // --- I/O Operation Dispatch Engine ---

    NtStatus dispatchIoOperation(FLT_CALLBACK_DATA* data) {
        if (!data) return NtStatus::InvalidParameter;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        // 1. Collect started filters and sort descending by altitude for Pre-Operation
        std::vector<RegisteredFilter*> sortedFilters;
        for (auto& [h, f] : m_filters) {
            if (f.IsStarted) {
                sortedFilters.push_back(&f);
            }
        }

        std::sort(sortedFilters.begin(), sortedFilters.end(), [](const RegisteredFilter* a, const RegisteredFilter* b) {
            return a->NumericAltitude > b->NumericAltitude; // Highest altitude executes first
        });

        struct PostOpEntry {
            RegisteredFilter* Filter;
            PFLT_POST_OPERATION_CALLBACK Callback;
            void* CompletionContext;
        };
        std::vector<PostOpEntry> postOpStack;

        uint8_t major = data->Iopb.MajorFunction;

        // 2. Pre-Operation Traversal (Top-Down)
        for (auto* filter : sortedFilters) {
            for (const auto& op : filter->Registration.OperationRegistration) {
                if (op.MajorFunction == major && op.PreOperation) {
                    m_telemetry.totalPreOpsDispatched++;
                    void* compCtx = nullptr;
                    FltPreopCallbackStatus preStatus = op.PreOperation(data, &compCtx);

                    if (preStatus == FltPreopCallbackStatus::Complete) {
                        // Short-circuit: do not proceed to lower filters or filesystem!
                        m_telemetry.totalOpsShortCircuited++;
                        return data->IoStatus.Status;
                    }

                    if (preStatus == FltPreopCallbackStatus::SuccessWithCallback ||
                        preStatus == FltPreopCallbackStatus::Synchronize) {
                        if (op.PostOperation) {
                            postOpStack.push_back({filter, op.PostOperation, compCtx});
                        }
                    }
                    break;
                }
            }
        }

        // 3. Simulated Underlying File System / Storage Execution
        if (data->IoStatus.Status == NtStatus::Success) {
            if (major == IRP_MJ_READ || major == IRP_MJ_WRITE) {
                data->IoStatus.Information = data->Iopb.ReadWrite.Length;
            }
        }

        // 4. Post-Operation Traversal (Bottom-Up Unwinding: Reverse order of Pre-Op)
        for (auto it = postOpStack.rbegin(); it != postOpStack.rend(); ++it) {
            m_telemetry.totalPostOpsDispatched++;
            FltPostopCallbackStatus postStatus = it->Callback(data, it->CompletionContext, 0);
            (void)postStatus;
        }

        return data->IoStatus.Status;
    }

    // --- Context Management Engine ---

    NtStatus allocateContext(uint32_t filterHandle, FltContextType type, size_t size,
                            std::shared_ptr<FltContext>& outCtx) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_filters.find(filterHandle) == m_filters.end()) return NtStatus::NotFound;
        outCtx = std::make_shared<FltContext>(type, size, 'CTX1');
        m_telemetry.activeContextsCount++;
        return NtStatus::Success;
    }

    NtStatus setFileContext(uint64_t fileObjectId, std::shared_ptr<FltContext> ctx) {
        if (!ctx) return NtStatus::InvalidParameter;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_fileContexts[fileObjectId] = ctx;
        return NtStatus::Success;
    }

    NtStatus getFileContext(uint64_t fileObjectId, std::shared_ptr<FltContext>& outCtx) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_fileContexts.find(fileObjectId);
        if (it == m_fileContexts.end()) return NtStatus::NotFound;
        outCtx = it->second;
        return NtStatus::Success;
    }

    NtStatus setStreamContext(const std::wstring& streamKey, std::shared_ptr<FltContext> ctx) {
        if (!ctx) return NtStatus::InvalidParameter;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_streamContexts[streamKey] = ctx;
        return NtStatus::Success;
    }

    NtStatus getStreamContext(const std::wstring& streamKey, std::shared_ptr<FltContext>& outCtx) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_streamContexts.find(streamKey);
        if (it == m_streamContexts.end()) return NtStatus::NotFound;
        outCtx = it->second;
        return NtStatus::Success;
    }

    // --- Communication Port Engine ---

    NtStatus createCommunicationPort(uint32_t filterHandle, const std::wstring& portName,
                                    void* serverCookie, PFLT_CONNECT_NOTIFY connectNotify,
                                    PFLT_DISCONNECT_NOTIFY disconnectNotify,
                                    PFLT_MESSAGE_NOTIFY messageNotify,
                                    uint32_t maxConnections, uint32_t* outPortHandle) {
        if (!outPortHandle) return NtStatus::InvalidParameter;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        uint32_t portId = m_nextPortHandle++;
        FltCommunicationPort port{};
        port.PortId = portId;
        port.PortName = portName;
        port.FilterHandle = filterHandle;
        port.ServerCookie = serverCookie;
        port.ConnectNotify = connectNotify;
        port.DisconnectNotify = disconnectNotify;
        port.MessageNotify = messageNotify;
        port.MaxConnections = (maxConnections == 0) ? 1 : maxConnections;
        port.CurrentConnections = 0;
        port.IsClosed = false;

        m_ports[portId] = std::move(port);
        *outPortHandle = portId;
        m_telemetry.activePortsCount = static_cast<uint32_t>(m_ports.size());
        return NtStatus::Success;
    }

    NtStatus connectCommunicationPort(const std::wstring& portName, const void* connCtx,
                                      uint32_t connCtxSize, uint32_t* outClientHandle) {
        if (!outClientHandle) return NtStatus::InvalidParameter;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        for (auto& [id, port] : m_ports) {
            if (port.PortName == portName && !port.IsClosed) {
                if (port.CurrentConnections >= port.MaxConnections) {
                    return NtStatus::InsufficientResources;
                }

                uint32_t clientHandle = m_nextClientHandle++;
                void* portCookie = nullptr;
                if (port.ConnectNotify) {
                    NtStatus st = port.ConnectNotify(clientHandle, connCtx, connCtxSize, &portCookie);
                    if (st != NtStatus::Success) return st;
                }

                port.CurrentConnections++;
                m_clientToPortMap[clientHandle] = id;
                *outClientHandle = clientHandle;
                return NtStatus::Success;
            }
        }

        return NtStatus::NotFound;
    }

    NtStatus sendPortMessage(uint32_t clientHandle, const void* inBuf, uint32_t inSize,
                             void* outBuf, uint32_t outSize, uint32_t* outReturned) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_clientToPortMap.find(clientHandle);
        if (it == m_clientToPortMap.end()) return NtStatus::InvalidHandle;

        auto portIt = m_ports.find(it->second);
        if (portIt == m_ports.end() || portIt->second.IsClosed) return NtStatus::PortConnectionRefused;

        if (!portIt->second.MessageNotify) return NtStatus::NotImplemented;

        m_telemetry.totalPortMessagesSent++;
        return portIt->second.MessageNotify(
            portIt->second.ServerCookie,
            inBuf, inSize, outBuf, outSize, outReturned
        );
    }

    NtStatus closePort(uint32_t portId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_ports.find(portId);
        if (it == m_ports.end()) return NtStatus::NotFound;

        it->second.IsClosed = true;
        if (it->second.DisconnectNotify) {
            it->second.DisconnectNotify(it->second.ServerCookie);
        }
        m_ports.erase(it);
        m_telemetry.activePortsCount = static_cast<uint32_t>(m_ports.size());
        return NtStatus::Success;
    }

    NtStatus closeClientHandle(uint32_t clientHandle) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_clientToPortMap.find(clientHandle);
        if (it == m_clientToPortMap.end()) return NtStatus::InvalidHandle;

        auto portIt = m_ports.find(it->second);
        if (portIt != m_ports.end() && portIt->second.CurrentConnections > 0) {
            portIt->second.CurrentConnections--;
        }
        m_clientToPortMap.erase(it);
        return NtStatus::Success;
    }

    // --- Exemplar Minifilter Accessors & Telemetry ---

    uint32_t getSentinelScanFilterHandle() const noexcept { return m_sentinelFilterHandle; }
    uint32_t getEmeraldFilterHandle() const noexcept { return m_emeraldFilterHandle; }
    uint32_t getActivityFilterHandle() const noexcept { return m_activityFilterHandle; }

    const std::vector<std::string>& getActivityLog() const noexcept { return m_activityLog; }

    FLT_TELEMETRY getTelemetry() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_telemetry;
    }

    void incrementMalwareBlocked() noexcept {
        m_telemetry.totalMalwareBlocked++;
    }

    void incrementCryptoOps() noexcept {
        m_telemetry.totalCryptoOpsProcessed++;
    }

    void logActivity(const std::string& record) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_activityLog.push_back(record);
    }

private:
    TitanFilterManagerSubsystem() = default;

    void registerDefaultMinifilters() {
        // --------------------------------------------------------------------
        // Minifilter 1: SentinelScanFlt (Real-Time Anti-Virus Scanner @ 328000)
        // --------------------------------------------------------------------
        FLT_REGISTRATION avReg{};
        avReg.FilterName = L"SentinelScanFlt";
        avReg.Altitude = ALTITUDE_ANTI_VIRUS;

        // Register CREATE & WRITE operations
        FLT_OPERATION_REGISTRATION createOp{};
        createOp.MajorFunction = IRP_MJ_CREATE;
        createOp.PreOperation = [this](FLT_CALLBACK_DATA* Data, void**) -> FltPreopCallbackStatus {
            // Check file path or intent
            if (Data->Iopb.TargetFilePath.find(L"malware.exe") != std::wstring::npos ||
                Data->Iopb.TargetFilePath.find(L"eicar.com") != std::wstring::npos) {
                incrementMalwareBlocked();
                Data->IoStatus.Status = NtStatus::AccessDenied;
                return FltPreopCallbackStatus::Complete; // Block!
            }
            return FltPreopCallbackStatus::SuccessWithCallback;
        };
        createOp.PostOperation = [](FLT_CALLBACK_DATA*, void*, uint32_t) -> FltPostopCallbackStatus {
            return FltPostopCallbackStatus::FinishedProcessing;
        };

        FLT_OPERATION_REGISTRATION writeOp{};
        writeOp.MajorFunction = IRP_MJ_WRITE;
        writeOp.PreOperation = [this](FLT_CALLBACK_DATA* Data, void**) -> FltPreopCallbackStatus {
            if (Data->Iopb.ReadWrite.Buffer && Data->Iopb.ReadWrite.Length >= 8) {
                const char* buf = reinterpret_cast<const char*>(Data->Iopb.ReadWrite.Buffer);
                // Scan for EICAR signature or malware byte marker
                if (std::string_view(buf, Data->Iopb.ReadWrite.Length).find("EICAR-STANDARD-ANTIVIRUS-TEST-FILE") != std::string_view::npos ||
                    std::string_view(buf, Data->Iopb.ReadWrite.Length).find("MALWARE_PAYLOAD") != std::string_view::npos) {
                    incrementMalwareBlocked();
                    Data->IoStatus.Status = NtStatus::AccessDenied;
                    return FltPreopCallbackStatus::Complete; // Short-circuit write!
                }
            }
            return FltPreopCallbackStatus::SuccessNoCallback;
        };

        avReg.OperationRegistration.push_back(createOp);
        avReg.OperationRegistration.push_back(writeOp);
        registerFilter(avReg, &m_sentinelFilterHandle);
        startFiltering(m_sentinelFilterHandle);

        // Minifilter 1 Communication Port
        uint32_t avPortHandle = 0;
        createCommunicationPort(
            m_sentinelFilterHandle,
            L"\\SentinelScanPort",
            nullptr,
            [](uint32_t, const void*, uint32_t, void**) { return NtStatus::Success; },
            [](void*) {},
            [this](void*, const void* inBuf, uint32_t inSize, void* outBuf, uint32_t outSize, uint32_t* retLen) -> NtStatus {
                if (!inBuf || inSize == 0) return NtStatus::InvalidParameter;
                // Echo scan request with verdict
                if (outBuf && outSize >= sizeof(uint32_t)) {
                    uint32_t verdict = (std::string_view(reinterpret_cast<const char*>(inBuf), inSize).find("VIRUS") != std::string_view::npos) ? 1 : 0;
                    if (verdict == 1) incrementMalwareBlocked();
                    std::memcpy(outBuf, &verdict, sizeof(uint32_t));
                    if (retLen) *retLen = sizeof(uint32_t);
                }
                return NtStatus::Success;
            },
            10,
            &avPortHandle
        );

        // --------------------------------------------------------------------
        // Minifilter 2: EmeraldFlt (Transparent File Encryption @ 141000)
        // --------------------------------------------------------------------
        FLT_REGISTRATION encReg{};
        encReg.FilterName = L"EmeraldFlt";
        encReg.Altitude = ALTITUDE_ENCRYPTION;

        FLT_OPERATION_REGISTRATION encWriteOp{};
        encWriteOp.MajorFunction = IRP_MJ_WRITE;
        encWriteOp.PreOperation = [this](FLT_CALLBACK_DATA* Data, void** compCtx) -> FltPreopCallbackStatus {
            if (Data->Iopb.ReadWrite.Buffer && Data->Iopb.ReadWrite.Length > 0) {
                // If the file is tagged for transparent encryption (.efs or .crypt)
                if (Data->Iopb.TargetFilePath.find(L".efs") != std::wstring::npos ||
                    Data->Iopb.TargetFilePath.find(L".crypt") != std::wstring::npos) {
                    incrementCryptoOps();
                    // Simple reversible cryptographic XOR transformation with key 0x5A
                    uint8_t* p = reinterpret_cast<uint8_t*>(Data->Iopb.ReadWrite.Buffer);
                    for (size_t i = 0; i < Data->Iopb.ReadWrite.Length; ++i) {
                        p[i] ^= 0x5A;
                    }
                    if (compCtx) *compCtx = reinterpret_cast<void*>(0x1337);
                }
            }
            return FltPreopCallbackStatus::SuccessWithCallback;
        };
        encWriteOp.PostOperation = [](FLT_CALLBACK_DATA*, void*, uint32_t) -> FltPostopCallbackStatus {
            return FltPostopCallbackStatus::FinishedProcessing;
        };

        FLT_OPERATION_REGISTRATION encReadOp{};
        encReadOp.MajorFunction = IRP_MJ_READ;
        encReadOp.PreOperation = [](FLT_CALLBACK_DATA* Data, void** compCtx) -> FltPreopCallbackStatus {
            if (Data->Iopb.TargetFilePath.find(L".efs") != std::wstring::npos ||
                Data->Iopb.TargetFilePath.find(L".crypt") != std::wstring::npos) {
                if (compCtx) *compCtx = reinterpret_cast<void*>(0x1337);
                return FltPreopCallbackStatus::SuccessWithCallback;
            }
            return FltPreopCallbackStatus::SuccessNoCallback;
        };
        encReadOp.PostOperation = [this](FLT_CALLBACK_DATA* Data, void* compCtx, uint32_t) -> FltPostopCallbackStatus {
            if (compCtx == reinterpret_cast<void*>(0x1337) && Data->Iopb.ReadWrite.Buffer) {
                incrementCryptoOps();
                // Decrypt in Post-Op before returning to user
                uint8_t* p = reinterpret_cast<uint8_t*>(Data->Iopb.ReadWrite.Buffer);
                for (size_t i = 0; i < Data->IoStatus.Information; ++i) {
                    p[i] ^= 0x5A;
                }
            }
            return FltPostopCallbackStatus::FinishedProcessing;
        };

        encReg.OperationRegistration.push_back(encWriteOp);
        encReg.OperationRegistration.push_back(encReadOp);
        registerFilter(encReg, &m_emeraldFilterHandle);
        startFiltering(m_emeraldFilterHandle);

        // --------------------------------------------------------------------
        // Minifilter 3: MicaActivityFlt (Filesystem Activity Monitor @ 385000)
        // --------------------------------------------------------------------
        FLT_REGISTRATION actReg{};
        actReg.FilterName = L"MicaActivityFlt";
        actReg.Altitude = ALTITUDE_ACTIVITY_MONITOR;

        FLT_OPERATION_REGISTRATION actCreateOp{};
        actCreateOp.MajorFunction = IRP_MJ_CREATE;
        actCreateOp.PreOperation = [this](FLT_CALLBACK_DATA* Data, void**) -> FltPreopCallbackStatus {
            logActivity("CREATE: " + std::string(Data->Iopb.TargetFilePath.begin(), Data->Iopb.TargetFilePath.end()));
            return FltPreopCallbackStatus::SuccessNoCallback;
        };

        FLT_OPERATION_REGISTRATION actWriteOp{};
        actWriteOp.MajorFunction = IRP_MJ_WRITE;
        actWriteOp.PreOperation = [this](FLT_CALLBACK_DATA* Data, void**) -> FltPreopCallbackStatus {
            logActivity("WRITE: " + std::to_string(Data->Iopb.ReadWrite.Length) + " bytes");
            return FltPreopCallbackStatus::SuccessNoCallback;
        };

        actReg.OperationRegistration.push_back(actCreateOp);
        actReg.OperationRegistration.push_back(actWriteOp);
        registerFilter(actReg, &m_activityFilterHandle);
        startFiltering(m_activityFilterHandle);
    }

    mutable std::recursive_mutex m_mutex;
    bool m_initialized{false};
    uint32_t m_nextFilterHandle{1};
    uint32_t m_nextPortHandle{100};
    uint32_t m_nextClientHandle{500};

    uint32_t m_sentinelFilterHandle{0};
    uint32_t m_emeraldFilterHandle{0};
    uint32_t m_activityFilterHandle{0};

    std::map<uint32_t, RegisteredFilter> m_filters;
    std::unordered_map<uint32_t, FltCommunicationPort> m_ports;
    std::unordered_map<uint32_t, uint32_t> m_clientToPortMap; // ClientHandle -> PortId
    std::unordered_map<uint64_t, std::shared_ptr<FltContext>> m_fileContexts;
    std::unordered_map<std::wstring, std::shared_ptr<FltContext>> m_streamContexts;
    std::vector<std::string> m_activityLog;

    FLT_TELEMETRY m_telemetry{};
};

// ============================================================================
// 10. Filter Manager Standard C ABI Exports
// ============================================================================

extern "C" {

inline NtStatus WINAPI FltRegisterFilter(
    const FLT_REGISTRATION* Registration,
    uint32_t* RetFilter
) {
    if (!Registration || !RetFilter) return NtStatus::InvalidParameter;
    return TitanFilterManagerSubsystem::Instance().registerFilter(*Registration, RetFilter);
}

inline NtStatus WINAPI FltStartFiltering(uint32_t Filter) {
    return TitanFilterManagerSubsystem::Instance().startFiltering(Filter);
}

inline NtStatus WINAPI FltUnregisterFilter(uint32_t Filter) {
    return TitanFilterManagerSubsystem::Instance().unregisterFilter(Filter);
}

inline NtStatus WINAPI FltDispatchOperation(FLT_CALLBACK_DATA* Data) {
    return TitanFilterManagerSubsystem::Instance().dispatchIoOperation(Data);
}

inline NtStatus WINAPI FltAllocateContext(
    uint32_t Filter,
    FltContextType ContextType,
    size_t ContextSize,
    uint32_t PoolType,
    void** RetContext
) {
    (void)PoolType;
    if (!RetContext) return NtStatus::InvalidParameter;
    std::shared_ptr<FltContext> ctx;
    NtStatus st = TitanFilterManagerSubsystem::Instance().allocateContext(Filter, ContextType, ContextSize, ctx);
    if (st == NtStatus::Success) {
        *RetContext = ctx.get();
        ctx->addRef(); // Retain reference for caller
    }
    return st;
}

inline void WINAPI FltReleaseContext(void* Context) {
    if (Context) {
        auto* ctx = reinterpret_cast<FltContext*>(Context);
        ctx->release();
    }
}

inline NtStatus WINAPI FltCreateCommunicationPort(
    uint32_t Filter,
    const wchar_t* PortName,
    void* ServerCookie,
    PFLT_CONNECT_NOTIFY ConnectNotify,
    PFLT_DISCONNECT_NOTIFY DisconnectNotify,
    PFLT_MESSAGE_NOTIFY MessageNotify,
    uint32_t MaxConnections,
    uint32_t* RetServerPort
) {
    if (!PortName || !RetServerPort) return NtStatus::InvalidParameter;
    return TitanFilterManagerSubsystem::Instance().createCommunicationPort(
        Filter, PortName, ServerCookie, ConnectNotify, DisconnectNotify,
        MessageNotify, MaxConnections, RetServerPort
    );
}

inline NtStatus WINAPI FltCloseCommunicationPort(uint32_t ServerPort) {
    return TitanFilterManagerSubsystem::Instance().closePort(ServerPort);
}

inline NtStatus WINAPI FilterConnectCommunicationPort(
    const wchar_t* PortName,
    uint32_t Options,
    const void* ConnectionContext,
    uint32_t SizeOfContext,
    uint32_t* RetPortHandle
) {
    (void)Options;
    if (!PortName || !RetPortHandle) return NtStatus::InvalidParameter;
    return TitanFilterManagerSubsystem::Instance().connectCommunicationPort(
        PortName, ConnectionContext, SizeOfContext, RetPortHandle
    );
}

inline NtStatus WINAPI FilterSendMessage(
    uint32_t PortHandle,
    const void* InBuffer,
    uint32_t InBufferSize,
    void* OutBuffer,
    uint32_t OutBufferSize,
    uint32_t* BytesReturned
) {
    return TitanFilterManagerSubsystem::Instance().sendPortMessage(
        PortHandle, InBuffer, InBufferSize, OutBuffer, OutBufferSize, BytesReturned
    );
}

inline NtStatus WINAPI FilterClose(uint32_t PortHandle) {
    return TitanFilterManagerSubsystem::Instance().closeClientHandle(PortHandle);
}

inline BOOL WINAPI FltGetTelemetry(FLT_TELEMETRY* outTelemetry) {
    if (!outTelemetry) return FALSE_VAL;
    *outTelemetry = TitanFilterManagerSubsystem::Instance().getTelemetry();
    return TRUE_VAL;
}

} // extern "C"

// C++ Helper outside extern "C" to avoid Clang -Wreturn-type-c-linkage warning
inline FLT_TELEMETRY FltQueryTelemetry() noexcept {
    return TitanFilterManagerSubsystem::Instance().getTelemetry();
}

// ============================================================================
// 11. Subsystem Export Registration (JanusLDR, SCM, Version)
// ============================================================================

inline void InitializeFilterManagerSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // Register fltmgr.sys exports
    ldr.registerExport("fltmgr.sys", "FltRegisterFilter", reinterpret_cast<void*>(FltRegisterFilter));
    ldr.registerExport("fltmgr.sys", "FltStartFiltering", reinterpret_cast<void*>(FltStartFiltering));
    ldr.registerExport("fltmgr.sys", "FltUnregisterFilter", reinterpret_cast<void*>(FltUnregisterFilter));
    ldr.registerExport("fltmgr.sys", "FltDispatchOperation", reinterpret_cast<void*>(FltDispatchOperation));
    ldr.registerExport("fltmgr.sys", "FltAllocateContext", reinterpret_cast<void*>(FltAllocateContext));
    ldr.registerExport("fltmgr.sys", "FltReleaseContext", reinterpret_cast<void*>(FltReleaseContext));
    ldr.registerExport("fltmgr.sys", "FltCreateCommunicationPort", reinterpret_cast<void*>(FltCreateCommunicationPort));
    ldr.registerExport("fltmgr.sys", "FltCloseCommunicationPort", reinterpret_cast<void*>(FltCloseCommunicationPort));
    ldr.registerExport("fltmgr.sys", "FilterConnectCommunicationPort", reinterpret_cast<void*>(FilterConnectCommunicationPort));
    ldr.registerExport("fltmgr.sys", "FilterSendMessage", reinterpret_cast<void*>(FilterSendMessage));
    ldr.registerExport("fltmgr.sys", "FilterClose", reinterpret_cast<void*>(FilterClose));
    ldr.registerExport("fltmgr.sys", "FltGetTelemetry", reinterpret_cast<void*>(FltGetTelemetry));

    // Register Service in SCM
    auto fltSvc = std::make_shared<scm::ServiceRecord>();
    fltSvc->serviceName = L"fltmgr";
    fltSvc->displayName = L"Sovereign I/O Filter Manager and Minifilter Driver Coordinator";
    fltSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    fltSvc->startType = scm::SERVICE_BOOT_START;
    fltSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    fltSvc->binaryPath = L"\\SystemRoot\\System32\\drivers\\fltmgr.sys";
    fltSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(fltSvc);

    // Register in VersionDatabase
    version::VersionDatabase::Instance().RegisterModule(
        "fltmgr.sys", "10.0.26100.1", "Sovereign I/O Filter Manager Subsystem");

    // Initialize the Subsystem singleton instance
    TitanFilterManagerSubsystem::Instance().initialize();
}

} // namespace micant::fltmgr

// Forward into parent micant namespace
namespace micant {
    using fltmgr::TitanFilterManagerSubsystem;
    using fltmgr::FltContext;
    using fltmgr::FltContextType;
    using fltmgr::FltPreopCallbackStatus;
    using fltmgr::FltPostopCallbackStatus;
    using fltmgr::FLT_CALLBACK_DATA;
    using fltmgr::FLT_IO_PARAMETER_BLOCK;
    using fltmgr::FLT_REGISTRATION;
    using fltmgr::FLT_OPERATION_REGISTRATION;
    using fltmgr::FLT_TELEMETRY;
    using fltmgr::FltQueryTelemetry;
    using fltmgr::FltGetTelemetry;
    using fltmgr::InitializeFilterManagerSubsystemExports;
} // namespace micant
