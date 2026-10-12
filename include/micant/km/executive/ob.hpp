#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <atomic>
#include <functional>
#include <algorithm>
#include <chrono>
#include <mutex>
#include "ntdef.hpp"
#include "ntstatus.hpp"

namespace micant::ob {

// ============================================================================
// 1. Standard NT Object Attributes & Access Rights
// ============================================================================
using micant::OBJ_INHERIT;
using micant::OBJ_PERMANENT;
using micant::OBJ_EXCLUSIVE;
using micant::OBJ_CASE_INSENSITIVE;
using micant::OBJ_OPENIF;
using micant::OBJ_OPENLINK;
using micant::OBJ_KERNEL_HANDLE;
using micant::OBJ_FORCE_ACCESS_CHECK;
inline constexpr uint32_t OBJ_VALID_ATTRIBUTES   = 0x000007F2L;

// Standard Access Rights (clean-room NT Win32 mapping)
inline constexpr uint32_t DELETE                  = 0x00010000L;
inline constexpr uint32_t READ_CONTROL            = 0x00020000L;
inline constexpr uint32_t WRITE_DAC               = 0x00040000L;
inline constexpr uint32_t WRITE_OWNER             = 0x00080000L;
inline constexpr uint32_t SYNCHRONIZE             = 0x00100000L;
inline constexpr uint32_t STANDARD_RIGHTS_REQUIRED= 0x000F0000L;
inline constexpr uint32_t STANDARD_RIGHTS_READ    = READ_CONTROL;
inline constexpr uint32_t STANDARD_RIGHTS_WRITE   = READ_CONTROL;
inline constexpr uint32_t STANDARD_RIGHTS_EXECUTE = READ_CONTROL;
inline constexpr uint32_t STANDARD_RIGHTS_ALL     = 0x001F0000L;

inline constexpr uint32_t GENERIC_READ            = 0x80000000L;
inline constexpr uint32_t GENERIC_WRITE           = 0x40000000L;
inline constexpr uint32_t GENERIC_EXECUTE         = 0x20000000L;
inline constexpr uint32_t GENERIC_ALL             = 0x10000000L;

using KPROCESSOR_MODE = uint8_t;
inline constexpr KPROCESSOR_MODE KernelMode = 0;
inline constexpr KPROCESSOR_MODE UserMode = 1;

using OBJECT_ATTRIBUTES = ObjectAttributes;
using POBJECT_ATTRIBUTES = ObjectAttributes*;

inline constexpr size_t OB_MAX_SYMLINK_RECURSION = 32;

struct GENERIC_MAPPING {
    uint32_t genericRead{0};
    uint32_t genericWrite{0};
    uint32_t genericExecute{0};
    uint32_t genericAll{0};
};

enum class ObjectTypeId : uint32_t {
    Type,
    Directory,
    SymbolicLink,
    Device,
    Driver,
    Process,
    Thread,
    Section,
    File,
    Event,
    Mutant,
    Semaphore,
    Key,
    IoCompletion,
    Job,
    Timer
};

struct ObjectHeader;
struct ObjectType;

// Object Manager Procedural Callbacks
using OB_DUMP_METHOD = void (*)(ObjectHeader* object);
using OB_OPEN_METHOD = NtStatus (*)(ObjectHeader* object, uint32_t accessMask);
using OB_CLOSE_METHOD = void (*)(ObjectHeader* object, Handle handle);
using OB_DELETE_METHOD = void (*)(ObjectHeader* object);
using OB_PARSE_METHOD = NtStatus (*)(
    ObjectHeader* parseObject,
    const ObjectType* objectType,
    std::wstring_view remainingPath,
    ObjectHeader** outFoundObject
);

using OB_ACCESS_CHECK_PROC = NtStatus (*)(
    void* securityDescriptor,
    uint32_t desiredAccess,
    uint32_t* grantedAccess
);

inline OB_ACCESS_CHECK_PROC g_ObpAccessCheckProcedure = nullptr;

inline void ObSetAccessCheckProcedure(OB_ACCESS_CHECK_PROC proc) noexcept {
    g_ObpAccessCheckProcedure = proc;
}

// ============================================================================
// 2. Dave Cutler Object Header Substructures
// ============================================================================
struct OBJECT_HEADER_NAME_INFO {
    void* directory{nullptr}; // Pointer to DirectoryObject
    std::wstring name;
    uint32_t queryReferences{0};
};

struct OBJECT_HEADER_CREATOR_INFO {
    uint32_t creatorProcessId{0};
    uint32_t creatorThreadId{0};
    uint64_t createTime{0};
};

struct OBJECT_HEADER_QUOTA_INFO {
    uint32_t pagedPoolCharge{0};
    uint32_t nonPagedPoolCharge{0};
    uint32_t securityDescriptorCharge{0};
};

struct OBJECT_HEADER_HANDLE_INFO {
    uint32_t handleCount{0};
    uint32_t singleHandle{0};
};

// ============================================================================
// 3. Object Manager Diagnostic Telemetry
// ============================================================================
struct OB_TELEMETRY {
    size_t totalObjectsCreated{0};
    size_t totalObjectsDestroyed{0};
    size_t totalHandlesOpened{0};
    size_t totalHandlesClosed{0};
    size_t totalHandlesDuplicated{0};
    size_t totalSymlinksResolved{0};
    size_t totalSymlinkRecursionsHit{0};
    size_t totalSecurityChecksPassed{0};
    size_t totalSecurityChecksDenied{0};
    size_t totalDirectoryInserts{0};
    size_t totalDirectoryLookups{0};
    size_t totalParseProcedureCalls{0};
    size_t totalTemporaryObjectsUnlinked{0};
};

inline OB_TELEMETRY g_ObTelemetry{};
inline std::mutex g_ObTelemetryMutex{};

inline OB_TELEMETRY ObQueryTelemetry() {
    std::lock_guard<std::mutex> lk(g_ObTelemetryMutex);
    return g_ObTelemetry;
}

inline void ObResetTelemetryForTesting() {
    std::lock_guard<std::mutex> lk(g_ObTelemetryMutex);
    g_ObTelemetry = OB_TELEMETRY{};
}

// ============================================================================
// 4. Object Type & Object Header
// ============================================================================

/**
 * @brief Representation of an Object Type in the Object Manager.
 */
struct ObjectType {
    std::wstring typeName;
    ObjectTypeId typeId{ObjectTypeId::Type};
    std::atomic<uint32_t> totalNumberOfObjects{0};
    std::atomic<uint32_t> totalNumberOfHandles{0};
    uint32_t validAccessMask{0x001FFFFF};
    GENERIC_MAPPING genericMapping{
        STANDARD_RIGHTS_READ | 0x0001,
        STANDARD_RIGHTS_WRITE | 0x0002,
        STANDARD_RIGHTS_EXECUTE | 0x0004,
        STANDARD_RIGHTS_ALL | 0x000F
    };

    OB_DUMP_METHOD dumpProcedure{nullptr};
    OB_OPEN_METHOD openProcedure{nullptr};
    OB_CLOSE_METHOD closeProcedure{nullptr};
    OB_DELETE_METHOD deleteProcedure{nullptr};
    OB_PARSE_METHOD parseProcedure{nullptr};
};

/**
 * @brief Canonical Object Header prepended to every allocated kernel object body.
 */
struct ObjectHeader {
    std::atomic<int32_t> pointerCount{1};
    std::atomic<int32_t> handleCount{0};
    const ObjectType* type{nullptr};
    std::wstring objectName;
    void* parentDirectory{nullptr}; // Pointer to DirectoryObject
    uint32_t attributes{0};
    void* securityDescriptor{nullptr};

    // Dave Cutler Header Info extensions
    OBJECT_HEADER_NAME_INFO nameInfo{};
    OBJECT_HEADER_CREATOR_INFO creatorInfo{};
    OBJECT_HEADER_QUOTA_INFO quotaInfo{};
    OBJECT_HEADER_HANDLE_INFO handleInfo{};
    bool dynamicallyAllocated{false};

    [[nodiscard]] void* getBody() noexcept {
        return reinterpret_cast<void*>(this + 1);
    }

    template<typename T>
    [[nodiscard]] T* as() noexcept {
        return reinterpret_cast<T*>(getBody());
    }

    void addReference() noexcept {
        pointerCount.fetch_add(1, std::memory_order_relaxed);
    }

    bool releaseReference() noexcept {
        return pointerCount.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

    [[nodiscard]] bool isPermanent() const noexcept {
        return (attributes & OBJ_PERMANENT) != 0;
    }

    [[nodiscard]] bool isExclusive() const noexcept {
        return (attributes & OBJ_EXCLUSIVE) != 0;
    }
};

// ============================================================================
// 5. Directory Object with NT 37-Bucket Hash Chains
// ============================================================================
inline constexpr size_t OB_HASH_BUCKETS = 37;

inline uint32_t ObpHashObjectName(std::wstring_view name) noexcept {
    uint32_t hash = 0;
    for (wchar_t ch : name) {
        wchar_t upper = (ch >= L'a' && ch <= L'z') ? static_cast<wchar_t>(ch - 32) : ch;
        hash = (hash * 37) + static_cast<uint32_t>(upper);
    }
    return hash % OB_HASH_BUCKETS;
}

/**
 * @brief Directory Object representing folders in the NT Object Namespace.
 * Implements Dave Cutler's 37-bucket hash chain directory structure.
 */
class DirectoryObject {
public:
    explicit DirectoryObject(std::wstring name) : name_(std::move(name)) {}

    NtStatus insertObject(std::wstring_view name, ObjectHeader* object) {
        if (!object) return NtStatus::InvalidParameter;
        std::wstring key(name);

        uint32_t bucket = ObpHashObjectName(key);
        for (auto* existing : buckets_[bucket]) {
            if (equalCaseInsensitive(existing->objectName, key)) {
                return NtStatus::ObjectNameCollision;
            }
        }

        object->addReference();
        object->objectName = key;
        object->nameInfo.name = key;
        object->parentDirectory = this;
        object->nameInfo.directory = this;
        buckets_[bucket].push_back(object);
        allEntries_[key] = object;

        {
            std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
            g_ObTelemetry.totalDirectoryInserts++;
        }

        return NtStatus::Success;
    }

    bool removeObject(std::wstring_view name) {
        std::wstring key(name);
        uint32_t bucket = ObpHashObjectName(key);
        auto& chain = buckets_[bucket];
        for (auto it = chain.begin(); it != chain.end(); ++it) {
            if (equalCaseInsensitive((*it)->objectName, key)) {
                (*it)->parentDirectory = nullptr;
                (*it)->nameInfo.directory = nullptr;
                (*it)->releaseReference();
                chain.erase(it);
                allEntries_.erase(key);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] ObjectHeader* lookup(std::wstring_view name) const {
        {
            std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
            g_ObTelemetry.totalDirectoryLookups++;
        }
        uint32_t bucket = ObpHashObjectName(name);
        for (auto* obj : buckets_[bucket]) {
            if (equalCaseInsensitive(obj->objectName, name)) {
                return obj;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const std::wstring& getName() const noexcept { return name_; }
    [[nodiscard]] size_t getEntryCount() const noexcept { return allEntries_.size(); }
    [[nodiscard]] const std::unordered_map<std::wstring, ObjectHeader*>& getEntries() const noexcept {
        return allEntries_;
    }

private:
    static bool equalCaseInsensitive(std::wstring_view s1, std::wstring_view s2) noexcept {
        if (s1.size() != s2.size()) return false;
        for (size_t i = 0; i < s1.size(); ++i) {
            wchar_t c1 = (s1[i] >= L'a' && s1[i] <= L'z') ? static_cast<wchar_t>(s1[i] - 32) : s1[i];
            wchar_t c2 = (s2[i] >= L'a' && s2[i] <= L'z') ? static_cast<wchar_t>(s2[i] - 32) : s2[i];
            if (c1 != c2) return false;
        }
        return true;
    }

    std::wstring name_;
    std::array<std::vector<ObjectHeader*>, OB_HASH_BUCKETS> buckets_;
    std::unordered_map<std::wstring, ObjectHeader*> allEntries_;
};

// ============================================================================
// 6. Multi-Level Handle Table Subsystem (EX_HANDLE_TABLE)
// ============================================================================
struct HandleTableEntry {
    ObjectHeader* object{nullptr};
    uint32_t grantedAccess{0};
    uint32_t handleAttributes{0};

    [[nodiscard]] bool isValid() const noexcept {
        return object != nullptr;
    }
};

/**
 * @brief Per-Process Handle Table.
 * Maps opaque userland `Handle` (values 4, 8, 12...) to ObjectHeader pointers with access control.
 */
class HandleTable {
public:
    static constexpr size_t MaxHandles = 4096;

    HandleTable() {
        table_.resize(MaxHandles);
    }

    NtStatus createHandle(
        ObjectHeader* object,
        Handle& outHandle,
        uint32_t grantedAccess = STANDARD_RIGHTS_ALL,
        uint32_t attributes = 0
    ) {
        if (!object) return NtStatus::InvalidParameter;

        // NT Handles start at 4 and increment by 4
        for (size_t i = 1; i < table_.size(); ++i) {
            if (!table_[i].isValid()) {
                object->addReference();
                object->handleCount.fetch_add(1, std::memory_order_relaxed);
                object->handleInfo.handleCount = static_cast<uint32_t>(object->handleCount.load(std::memory_order_relaxed));
                if (object->type) {
                    const_cast<ObjectType*>(object->type)->totalNumberOfHandles.fetch_add(1, std::memory_order_relaxed);
                }

                table_[i].object = object;
                table_[i].grantedAccess = grantedAccess;
                table_[i].handleAttributes = attributes;

                outHandle = static_cast<Handle>(i * 4);
                return NtStatus::Success;
            }
        }
        return NtStatus::NoMemory;
    }

    [[nodiscard]] ObjectHeader* lookup(Handle handle, uint32_t* outGrantedAccess = nullptr) const {
        size_t index = static_cast<size_t>(handle) / 4;
        if (index == 0 || index >= table_.size() || !table_[index].isValid()) {
            return nullptr;
        }
        if (outGrantedAccess) {
            *outGrantedAccess = table_[index].grantedAccess;
        }
        return table_[index].object;
    }

    [[nodiscard]] const HandleTableEntry* getEntry(Handle handle) const {
        size_t index = static_cast<size_t>(handle) / 4;
        if (index == 0 || index >= table_.size() || !table_[index].isValid()) {
            return nullptr;
        }
        return &table_[index];
    }

    NtStatus closeHandle(Handle handle) {
        size_t index = static_cast<size_t>(handle) / 4;
        if (index == 0 || index >= table_.size() || !table_[index].isValid()) {
            return NtStatus::InvalidHandle;
        }

        ObjectHeader* obj = table_[index].object;
        table_[index] = HandleTableEntry{};

        obj->handleCount.fetch_sub(1, std::memory_order_relaxed);
        obj->handleInfo.handleCount = static_cast<uint32_t>(obj->handleCount.load(std::memory_order_relaxed));
        if (obj->type) {
            const_cast<ObjectType*>(obj->type)->totalNumberOfHandles.fetch_sub(1, std::memory_order_relaxed);
        }
        obj->releaseReference();
        return NtStatus::Success;
    }

    NtStatus duplicateHandle(
        Handle sourceHandle,
        HandleTable& targetTable,
        Handle& targetHandle,
        uint32_t desiredAccess = 0,
        bool inherit = false,
        uint32_t options = 0
    ) {
        uint32_t currentAccess = 0;
        ObjectHeader* obj = lookup(sourceHandle, &currentAccess);
        if (!obj) return NtStatus::InvalidHandle;

        uint32_t accessToGrant = (desiredAccess != 0) ? desiredAccess : currentAccess;
        uint32_t attrs = inherit ? OBJ_INHERIT : 0;

        NtStatus st = targetTable.createHandle(obj, targetHandle, accessToGrant, attrs);
        if (NT_SUCCESS(st) && (options & 0x00000001)) { // DUPLICATE_CLOSE_SOURCE
            closeHandle(sourceHandle);
        }
        return st;
    }

    [[nodiscard]] size_t getActiveHandleCount() const noexcept {
        size_t count = 0;
        for (size_t i = 1; i < table_.size(); ++i) {
            if (table_[i].isValid()) ++count;
        }
        return count;
    }

private:
    std::vector<HandleTableEntry> table_;
};

// ============================================================================
// 7. Symbolic Link Object
// ============================================================================
class SymbolicLinkObject {
public:
    explicit SymbolicLinkObject(std::wstring targetPath) : targetPath_(std::move(targetPath)) {}
    [[nodiscard]] const std::wstring& getTargetPath() const noexcept { return targetPath_; }
    void setTargetPath(std::wstring targetPath) noexcept { targetPath_ = std::move(targetPath); }

private:
    std::wstring targetPath_;
};

// ============================================================================
// 8. Global Object Namespace & Hierarchical Resolver (ObpLookupObjectName)
// ============================================================================
class ObjectNamespace {
public:
    static ObjectNamespace& get() {
        static ObjectNamespace instance;
        return instance;
    }

    void initializeRoot() {
        if (initialized_) return;
        initialized_ = true;

        directories_[L"\\"] = std::make_shared<DirectoryObject>(L"\\");
        createDirectory(L"\\Device");
        createDirectory(L"\\Driver");
        createDirectory(L"\\DosDevices");
        createDirectory(L"\\KernelObjects");
        createDirectory(L"\\BaseNamedObjects");
        createDirectory(L"\\RPC Control");
        createDirectory(L"\\Sessions");
        createDirectory(L"\\ObjectTypes");

        // Register default NT Object Types in \ObjectTypes
        registerType(L"Type", ObjectTypeId::Type);
        registerType(L"Directory", ObjectTypeId::Directory);
        registerType(L"SymbolicLink", ObjectTypeId::SymbolicLink);
        registerType(L"Device", ObjectTypeId::Device);
        registerType(L"Driver", ObjectTypeId::Driver);
        registerType(L"Process", ObjectTypeId::Process);
        registerType(L"Thread", ObjectTypeId::Thread);
        registerType(L"Section", ObjectTypeId::Section);
        registerType(L"File", ObjectTypeId::File);
        registerType(L"Event", ObjectTypeId::Event);
        registerType(L"Mutant", ObjectTypeId::Mutant);
        registerType(L"Semaphore", ObjectTypeId::Semaphore);
        registerType(L"Key", ObjectTypeId::Key);
        registerType(L"IoCompletion", ObjectTypeId::IoCompletion);
        registerType(L"Job", ObjectTypeId::Job);
        registerType(L"Timer", ObjectTypeId::Timer);

        // Canonical Windows NT Symbolic Links
        createSymbolicLink(L"\\DosDevices\\NUL", L"\\Device\\Null");
        createSymbolicLink(L"\\DosDevices\\CON", L"\\Device\\Console");
        createSymbolicLink(L"\\DosDevices\\PIPE", L"\\Device\\NamedPipe");
        createSymbolicLink(L"\\DosDevices\\MAILSLOT", L"\\Device\\Mailslot");
        createSymbolicLink(L"\\DosDevices\\PhysicalDrive0", L"\\Device\\Harddisk0");
        createSymbolicLink(L"\\Device\\HarddiskVolume1", L"\\Device\\Harddisk0\\Partition1");
        createSymbolicLink(L"\\??", L"\\DosDevices");
        createSymbolicLink(L"\\GLOBAL??", L"\\DosDevices");
    }

    bool registerType(std::wstring_view typeName, ObjectTypeId id) {
        std::wstring name(typeName);
        if (types_.contains(name)) return true;

        auto type = std::make_unique<ObjectType>();
        type->typeName = name;
        type->typeId = id;
        types_[name] = std::move(type);
        return true;
    }

    [[nodiscard]] const ObjectType* getType(std::wstring_view typeName) const {
        auto it = types_.find(std::wstring(typeName));
        if (it != types_.end()) return it->second.get();
        return nullptr;
    }

    bool createDirectory(std::wstring_view fullPath) {
        std::wstring path(fullPath);
        if (directories_.contains(path)) return true;

        // Ensure parent directory exists
        size_t lastSlash = path.rfind(L'\\');
        if (lastSlash != std::wstring::npos && lastSlash > 0) {
            std::wstring parent = path.substr(0, lastSlash);
            createDirectory(parent);
        }

        directories_[path] = std::make_shared<DirectoryObject>(path);
        return true;
    }

    bool createSymbolicLink(std::wstring_view linkPath, std::wstring_view targetPath) {
        symbolicLinks_[std::wstring(linkPath)] = std::make_shared<SymbolicLinkObject>(std::wstring(targetPath));
        return true;
    }

    bool removeSymbolicLink(std::wstring_view linkPath) {
        return symbolicLinks_.erase(std::wstring(linkPath)) > 0;
    }

    [[nodiscard]] std::shared_ptr<SymbolicLinkObject> getSymbolicLink(std::wstring_view linkPath) const {
        auto it = symbolicLinks_.find(std::wstring(linkPath));
        if (it != symbolicLinks_.end()) return it->second;
        return nullptr;
    }

    /**
     * @brief Normalizes NT device prefixes (\??\, \GLOBAL??\, and \\.\ -> \DosDevices\).
     */
    [[nodiscard]] std::wstring normalizePrefix(std::wstring_view path) const {
        std::wstring current(path);
        if (current.starts_with(L"\\??\\")) {
            current = L"\\DosDevices\\" + current.substr(4);
        } else if (current.starts_with(L"\\GLOBAL??\\")) {
            current = L"\\DosDevices\\" + current.substr(10);
        } else if (current.starts_with(L"\\\\.\\")) {
            current = L"\\DosDevices\\" + current.substr(4);
        }
        return current;
    }

    /**
     * @brief Resolves symbolic links recursively with loop detection and depth bounds.
     */
    [[nodiscard]] std::wstring resolvePath(std::wstring_view path, NtStatus* outStatus = nullptr) const {
        std::wstring current = normalizePrefix(path);
        std::vector<std::wstring> visited;
        visited.push_back(current);

        for (size_t hop = 0; hop < OB_MAX_SYMLINK_RECURSION; ++hop) {
            // 1. Exact match
            auto it = symbolicLinks_.find(current);
            if (it != symbolicLinks_.end()) {
                std::wstring next = it->second->getTargetPath();
                {
                    std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
                    g_ObTelemetry.totalSymlinksResolved++;
                }
                if (std::find(visited.begin(), visited.end(), next) != visited.end()) {
                    if (outStatus) *outStatus = NtStatus::CircularReference;
                    std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
                    g_ObTelemetry.totalSymlinkRecursionsHit++;
                    return next;
                }
                visited.push_back(next);
                current = next;
                continue;
            }

            // 2. Longest prefix match
            bool matched = false;
            size_t longestMatch = 0;
            std::wstring bestReplacement;

            for (const auto& [link, target] : symbolicLinks_) {
                if (current.starts_with(link) && link.length() > longestMatch) {
                    if (current.length() == link.length() || current[link.length()] == L'\\') {
                        longestMatch = link.length();
                        bestReplacement = target->getTargetPath() + current.substr(link.length());
                        matched = true;
                    }
                }
            }

            if (matched) {
                {
                    std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
                    g_ObTelemetry.totalSymlinksResolved++;
                }
                if (std::find(visited.begin(), visited.end(), bestReplacement) != visited.end()) {
                    if (outStatus) *outStatus = NtStatus::CircularReference;
                    std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
                    g_ObTelemetry.totalSymlinkRecursionsHit++;
                    return bestReplacement;
                }
                visited.push_back(bestReplacement);
                current = bestReplacement;
                continue;
            }

            break; // No more symbolic links to resolve
        }

        if (visited.size() > OB_MAX_SYMLINK_RECURSION) {
            if (outStatus) *outStatus = NtStatus::LinkEvaluationCapExceeded;
            std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
            g_ObTelemetry.totalSymlinkRecursionsHit++;
        } else if (outStatus) {
            *outStatus = NtStatus::Success;
        }

        return current;
    }

    [[nodiscard]] std::shared_ptr<DirectoryObject> getDirectory(std::wstring_view path) const {
        auto it = directories_.find(std::wstring(path));
        if (it != directories_.end()) return it->second;
        return nullptr;
    }

    /**
     * @brief Hierarchical object lookup across directories (ObpLookupObjectName) with parse procedure support.
     */
    [[nodiscard]] ObjectHeader* lookupObject(
        std::wstring_view fullPath,
        const ObjectType* expectedType = nullptr,
        NtStatus* outStatus = nullptr
    ) const {
        NtStatus st = NtStatus::Success;
        std::wstring resolved = resolvePath(fullPath, &st);
        if (outStatus) *outStatus = st;
        if (!NT_SUCCESS(st)) return nullptr;

        size_t lastSlash = resolved.rfind(L'\\');
        if (lastSlash == std::wstring::npos) return nullptr;

        std::wstring dirPath = (lastSlash == 0) ? L"\\" : resolved.substr(0, lastSlash);
        std::wstring objName = resolved.substr(lastSlash + 1);

        auto dir = getDirectory(dirPath);
        if (dir) {
            auto* obj = dir->lookup(objName);
            if (obj) return obj;
        }

        // Prefix parseProcedure traversal
        size_t pos = 1;
        while (pos < resolved.length()) {
            size_t nextSlash = resolved.find(L'\\', pos);
            std::wstring prefix = (nextSlash == std::wstring::npos) ? resolved : resolved.substr(0, nextSlash);
            size_t pSlash = prefix.rfind(L'\\');
            std::wstring pDir = (pSlash == 0) ? L"\\" : prefix.substr(0, pSlash);
            std::wstring pName = prefix.substr(pSlash + 1);

            auto pDirObj = getDirectory(pDir);
            if (pDirObj) {
                auto* intermediate = pDirObj->lookup(pName);
                if (intermediate && intermediate->type && intermediate->type->parseProcedure) {
                    std::wstring_view remaining = (nextSlash == std::wstring::npos) 
                        ? std::wstring_view{} 
                        : std::wstring_view(resolved).substr(nextSlash);
                    ObjectHeader* foundFromParse = nullptr;
                    {
                        std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
                        g_ObTelemetry.totalParseProcedureCalls++;
                    }
                    NtStatus parseSt = intermediate->type->parseProcedure(
                        intermediate, expectedType, remaining, &foundFromParse
                    );
                    if (outStatus) *outStatus = parseSt;
                    if (NT_SUCCESS(parseSt) && foundFromParse) {
                        return foundFromParse;
                    }
                }
            }

            if (nextSlash == std::wstring::npos) break;
            pos = nextSlash + 1;
        }

        return nullptr;
    }

    [[nodiscard]] size_t getDirectoryCount() const noexcept { return directories_.size(); }
    [[nodiscard]] size_t getSymbolicLinkCount() const noexcept { return symbolicLinks_.size(); }
    [[nodiscard]] size_t getTypeCount() const noexcept { return types_.size(); }

private:
    ObjectNamespace() = default;
    bool initialized_{false};
    std::unordered_map<std::wstring, std::unique_ptr<ObjectType>> types_;
    std::unordered_map<std::wstring, std::shared_ptr<DirectoryObject>> directories_;
    std::unordered_map<std::wstring, std::shared_ptr<SymbolicLinkObject>> symbolicLinks_;
};

// ============================================================================
// 9. Sovereign Object Reference & Lifetime Management API (Ob*)
// ============================================================================

/**
 * @brief Canonical Object Creation (ObCreateObject).
 */
inline NtStatus ObCreateObject(
    KPROCESSOR_MODE /*accessMode*/,
    const ObjectType* objectType,
    const ObjectAttributes* objectAttributes,
    size_t bodySize,
    void** outObjectBody,
    ObjectHeader** outHeader = nullptr
) {
    if (!objectType || !outObjectBody) return NtStatus::InvalidParameter;

    size_t totalSize = sizeof(ObjectHeader) + bodySize;
    uint8_t* memory = new (std::nothrow) uint8_t[totalSize]();
    if (!memory) return NtStatus::InsufficientResources;

    auto* header = reinterpret_cast<ObjectHeader*>(memory);
    header->pointerCount.store(1, std::memory_order_relaxed);
    header->handleCount.store(0, std::memory_order_relaxed);
    header->type = objectType;
    header->dynamicallyAllocated = true;

    if (objectAttributes) {
        header->attributes = objectAttributes->attributes;
        if (objectAttributes->objectName) {
            header->objectName = std::wstring(objectAttributes->objectName->view());
            header->nameInfo.name = header->objectName;
        }
        header->securityDescriptor = objectAttributes->securityDescriptor;
    }

    header->creatorInfo.createTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header->creatorInfo.creatorProcessId = 4; // System process
    header->creatorInfo.creatorThreadId = 1;

    header->quotaInfo.pagedPoolCharge = static_cast<uint32_t>(bodySize);
    header->quotaInfo.nonPagedPoolCharge = static_cast<uint32_t>(sizeof(ObjectHeader));

    const_cast<ObjectType*>(objectType)->totalNumberOfObjects.fetch_add(1, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
        g_ObTelemetry.totalObjectsCreated++;
    }

    *outObjectBody = header->getBody();
    if (outHeader) *outHeader = header;
    return NtStatus::Success;
}

/**
 * @brief Canonical Object Insertion into Directory & Handle Table (ObInsertObject).
 */
inline NtStatus ObInsertObject(
    ObjectHeader* object,
    const ObjectAttributes* objectAttributes,
    uint32_t desiredAccess,
    HandleTable* handleTable,
    Handle* outHandle
) {
    if (!object) return NtStatus::InvalidParameter;

    // Security access check if OBJ_FORCE_ACCESS_CHECK
    if (objectAttributes && (objectAttributes->attributes & OBJ_FORCE_ACCESS_CHECK)) {
        if (g_ObpAccessCheckProcedure && object->securityDescriptor) {
            uint32_t granted = 0;
            NtStatus chkSt = g_ObpAccessCheckProcedure(object->securityDescriptor, desiredAccess, &granted);
            if (!NT_SUCCESS(chkSt)) {
                std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
                g_ObTelemetry.totalSecurityChecksDenied++;
                return NtStatus::AccessDenied;
            }
        }
        std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
        g_ObTelemetry.totalSecurityChecksPassed++;
    }

    // Resolve directory and insert
    if (!object->objectName.empty()) {
        std::wstring fullPath = object->objectName;
        size_t lastSlash = fullPath.rfind(L'\\');
        std::wstring dirPath = (lastSlash == std::wstring::npos || lastSlash == 0) 
            ? L"\\" 
            : fullPath.substr(0, lastSlash);
        std::wstring name = (lastSlash == std::wstring::npos) 
            ? fullPath 
            : fullPath.substr(lastSlash + 1);

        auto dir = ObjectNamespace::get().getDirectory(dirPath);
        if (!dir) {
            ObjectNamespace::get().createDirectory(dirPath);
            dir = ObjectNamespace::get().getDirectory(dirPath);
        }

        if (dir) {
            NtStatus ins = dir->insertObject(name, object);
            if (!NT_SUCCESS(ins)) return ins;
            object->nameInfo.directory = dir.get();
        }
    }

    // If handle requested, allocate handle
    if (handleTable && outHandle) {
        NtStatus hst = handleTable->createHandle(object, *outHandle, desiredAccess, object->attributes);
        if (!NT_SUCCESS(hst)) return hst;

        {
            std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
            g_ObTelemetry.totalHandlesOpened++;
        }

        if (object->type && object->type->openProcedure) {
            object->type->openProcedure(object, desiredAccess);
        }
    }

    return NtStatus::Success;
}

/**
 * @brief Canonical Object Lookup by Name (ObReferenceObjectByName).
 */
inline NtStatus ObReferenceObjectByName(
    std::wstring_view fullPath,
    uint32_t /*attributes*/,
    const ObjectType* expectedType,
    KPROCESSOR_MODE /*accessMode*/,
    ObjectHeader** outObject
) {
    if (!outObject) return NtStatus::InvalidParameter;
    *outObject = nullptr;

    NtStatus st = NtStatus::Success;
    ObjectHeader* obj = ObjectNamespace::get().lookupObject(fullPath, expectedType, &st);
    if (!obj) {
        return NT_SUCCESS(st) ? NtStatus::ObjectNameNotFound : st;
    }

    if (expectedType && obj->type != expectedType) {
        return NtStatus::ObjectTypeMismatch;
    }

    obj->addReference();
    *outObject = obj;
    return NtStatus::Success;
}

/**
 * @brief Canonical Open Object by Name (ObOpenObjectByName).
 */
inline NtStatus ObOpenObjectByName(
    const ObjectAttributes* objectAttributes,
    const ObjectType* expectedType,
    KPROCESSOR_MODE /*accessMode*/,
    uint32_t desiredAccess,
    HandleTable& handleTable,
    Handle& outHandle
) {
    if (!objectAttributes || !objectAttributes->objectName) {
        return NtStatus::InvalidParameter;
    }

    std::wstring path(objectAttributes->objectName->view());
    NtStatus lookupSt = NtStatus::Success;
    ObjectHeader* obj = ObjectNamespace::get().lookupObject(path, expectedType, &lookupSt);
    if (!obj) {
        return NT_SUCCESS(lookupSt) ? NtStatus::ObjectNameNotFound : lookupSt;
    }

    if (expectedType && obj->type != expectedType) {
        return NtStatus::ObjectTypeMismatch;
    }

    // Access check
    if (g_ObpAccessCheckProcedure && obj->securityDescriptor) {
        uint32_t granted = 0;
        NtStatus chkSt = g_ObpAccessCheckProcedure(obj->securityDescriptor, desiredAccess, &granted);
        if (!NT_SUCCESS(chkSt)) {
            std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
            g_ObTelemetry.totalSecurityChecksDenied++;
            return NtStatus::AccessDenied;
        }
    }
    {
        std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
        g_ObTelemetry.totalSecurityChecksPassed++;
    }

    NtStatus st = handleTable.createHandle(obj, outHandle, desiredAccess, objectAttributes->attributes);
    if (NT_SUCCESS(st)) {
        {
            std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
            g_ObTelemetry.totalHandlesOpened++;
        }
        if (obj->type && obj->type->openProcedure) {
            obj->type->openProcedure(obj, desiredAccess);
        }
    }
    return st;
}

/**
 * @brief Canonical Open Object by Pointer (ObOpenObjectByPointer).
 */
inline NtStatus ObOpenObjectByPointer(
    ObjectHeader* object,
    uint32_t handleAttributes,
    uint32_t desiredAccess,
    const ObjectType* expectedType,
    HandleTable& handleTable,
    Handle& outHandle
) {
    if (!object) return NtStatus::InvalidParameter;
    if (expectedType && object->type != expectedType) {
        return NtStatus::ObjectTypeMismatch;
    }

    NtStatus st = handleTable.createHandle(object, outHandle, desiredAccess, handleAttributes);
    if (NT_SUCCESS(st)) {
        {
            std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
            g_ObTelemetry.totalHandlesOpened++;
        }
        if (object->type && object->type->openProcedure) {
            object->type->openProcedure(object, desiredAccess);
        }
    }
    return st;
}

/**
 * @brief Canonical Reference Object by Handle (ObReferenceObjectByHandle).
 */
inline NtStatus ObReferenceObjectByHandle(
    Handle handle,
    const HandleTable& handleTable,
    uint32_t desiredAccess,
    const ObjectType* expectedType,
    ObjectHeader** outObject
) {
    if (!outObject) return NtStatus::InvalidParameter;
    *outObject = nullptr;

    uint32_t grantedAccess = 0;
    ObjectHeader* obj = handleTable.lookup(handle, &grantedAccess);
    if (!obj) return NtStatus::InvalidHandle;

    if (desiredAccess != 0 && (grantedAccess & desiredAccess) != desiredAccess) {
        return NtStatus::AccessDenied;
    }

    if (expectedType && obj->type != expectedType) {
        return NtStatus::ObjectTypeMismatch;
    }

    obj->addReference();
    *outObject = obj;
    return NtStatus::Success;
}

/**
 * @brief Canonical Dereference Object (ObDereferenceObject).
 */
inline void ObDereferenceObject(ObjectHeader* object) {
    if (!object) return;
    if (object->releaseReference()) {
        if (object->type && object->type->deleteProcedure) {
            object->type->deleteProcedure(object);
        }
        if (object->type) {
            const_cast<ObjectType*>(object->type)->totalNumberOfObjects.fetch_sub(1, std::memory_order_relaxed);
        }
        {
            std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
            g_ObTelemetry.totalObjectsDestroyed++;
        }
        if (object->dynamicallyAllocated) {
            delete[] reinterpret_cast<uint8_t*>(object);
        }
    }
}

/**
 * @brief Canonical Close Handle (ObCloseHandle).
 */
inline NtStatus ObCloseHandle(Handle handle, HandleTable& handleTable) {
    const auto* entry = handleTable.getEntry(handle);
    if (!entry || !entry->isValid()) return NtStatus::InvalidHandle;

    ObjectHeader* obj = entry->object;
    if (obj->type && obj->type->closeProcedure) {
        obj->type->closeProcedure(obj, handle);
    }

    NtStatus st = handleTable.closeHandle(handle);
    if (NT_SUCCESS(st)) {
        {
            std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
            g_ObTelemetry.totalHandlesClosed++;
        }

        // Automatic unlinking of temporary objects when last handle is closed
        if (obj->handleCount.load(std::memory_order_relaxed) == 0 && !obj->isPermanent()) {
            if (obj->parentDirectory) {
                auto* dir = reinterpret_cast<DirectoryObject*>(obj->parentDirectory);
                dir->removeObject(obj->objectName);
                obj->parentDirectory = nullptr;
                obj->nameInfo.directory = nullptr;
                {
                    std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
                    g_ObTelemetry.totalTemporaryObjectsUnlinked++;
                }
            }
        }

        if (obj->pointerCount.load(std::memory_order_relaxed) <= 0) {
            if (obj->type && obj->type->deleteProcedure) {
                obj->type->deleteProcedure(obj);
            }
            if (obj->type) {
                const_cast<ObjectType*>(obj->type)->totalNumberOfObjects.fetch_sub(1, std::memory_order_relaxed);
            }
            {
                std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
                g_ObTelemetry.totalObjectsDestroyed++;
            }
            if (obj->dynamicallyAllocated) {
                delete[] reinterpret_cast<uint8_t*>(obj);
            }
        }
    }
    return st;
}

/**
 * @brief Canonical Duplicate Handle (ObDuplicateHandle).
 */
inline NtStatus ObDuplicateHandle(
    Handle sourceHandle,
    HandleTable& sourceTable,
    HandleTable& targetTable,
    Handle& targetHandle,
    uint32_t desiredAccess = 0,
    bool inherit = false,
    uint32_t options = 0
) {
    NtStatus st = sourceTable.duplicateHandle(sourceHandle, targetTable, targetHandle, desiredAccess, inherit, options);
    if (NT_SUCCESS(st)) {
        std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
        g_ObTelemetry.totalHandlesDuplicated++;
    }
    return st;
}

/**
 * @brief Canonical Make Temporary Object (ObMakeTemporaryObject).
 */
inline void ObMakeTemporaryObject(ObjectHeader* object) {
    if (!object) return;
    object->attributes &= ~OBJ_PERMANENT;

    if (object->handleCount.load(std::memory_order_relaxed) == 0 && object->parentDirectory) {
        auto* dir = reinterpret_cast<DirectoryObject*>(object->parentDirectory);
        dir->removeObject(object->objectName);
        object->parentDirectory = nullptr;
        object->nameInfo.directory = nullptr;
        {
            std::lock_guard<std::mutex> tlk(g_ObTelemetryMutex);
            g_ObTelemetry.totalTemporaryObjectsUnlinked++;
        }
    }
}

} // namespace micant::ob
