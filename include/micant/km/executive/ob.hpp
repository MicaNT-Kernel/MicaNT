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
#include "ntdef.hpp"
#include "ntstatus.hpp"

namespace micant::ob {

// ============================================================================
// 1. Standard NT Object Attributes & Access Rights
// ============================================================================
inline constexpr uint32_t OBJ_INHERIT             = 0x00000002L;
inline constexpr uint32_t OBJ_PERMANENT           = 0x00000010L;
inline constexpr uint32_t OBJ_EXCLUSIVE           = 0x00000020L;
inline constexpr uint32_t OBJ_CASE_INSENSITIVE   = 0x00000040L;
inline constexpr uint32_t OBJ_OPENIF             = 0x00000080L;
inline constexpr uint32_t OBJ_OPENLINK           = 0x00000100L;
inline constexpr uint32_t OBJ_KERNEL_HANDLE      = 0x00000200L;
inline constexpr uint32_t OBJ_FORCE_ACCESS_CHECK = 0x00000400L;
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
using OB_PARSE_METHOD = NtStatus (*)(ObjectHeader* parseObject, const ObjectType* objectType, std::wstring_view remainingPath, ObjectHeader** outFoundObject);

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
// 2. Directory Object with NT 37-Bucket Hash Chains
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
        object->parentDirectory = this;
        buckets_[bucket].push_back(object);
        allEntries_[key] = object;
        return NtStatus::Success;
    }

    bool removeObject(std::wstring_view name) {
        std::wstring key(name);
        uint32_t bucket = ObpHashObjectName(key);
        auto& chain = buckets_[bucket];
        for (auto it = chain.begin(); it != chain.end(); ++it) {
            if (equalCaseInsensitive((*it)->objectName, key)) {
                (*it)->parentDirectory = nullptr;
                (*it)->releaseReference();
                chain.erase(it);
                allEntries_.erase(key);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] ObjectHeader* lookup(std::wstring_view name) const {
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
// 3. Multi-Level Handle Table Subsystem (EX_HANDLE_TABLE)
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
// 4. Symbolic Link Object
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
// 5. Global Object Namespace & Hierarchical Resolver (ObpLookupObjectName)
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
     * @brief Resolves symbolic links recursively with loop detection.
     */
    [[nodiscard]] std::wstring resolvePath(std::wstring_view path) const {
        std::wstring current = normalizePrefix(path);
        constexpr size_t MAX_SYMLINK_RECURSION = 32;

        for (size_t hop = 0; hop < MAX_SYMLINK_RECURSION; ++hop) {
            // 1. Exact match
            auto it = symbolicLinks_.find(current);
            if (it != symbolicLinks_.end()) {
                current = it->second->getTargetPath();
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
                current = bestReplacement;
                continue;
            }

            break; // No more symbolic links to resolve
        }

        return current;
    }

    [[nodiscard]] std::shared_ptr<DirectoryObject> getDirectory(std::wstring_view path) const {
        auto it = directories_.find(std::wstring(path));
        if (it != directories_.end()) return it->second;
        return nullptr;
    }

    /**
     * @brief Hierarchical object lookup across directories (ObpLookupObjectName).
     */
    [[nodiscard]] ObjectHeader* lookupObject(std::wstring_view fullPath) const {
        std::wstring resolved = resolvePath(fullPath);
        size_t lastSlash = resolved.rfind(L'\\');
        if (lastSlash == std::wstring::npos) return nullptr;

        std::wstring dirPath = (lastSlash == 0) ? L"\\" : resolved.substr(0, lastSlash);
        std::wstring objName = resolved.substr(lastSlash + 1);

        auto dir = getDirectory(dirPath);
        if (!dir) return nullptr;
        return dir->lookup(objName);
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
// 6. Object Reference & Lifetime Management API (Ob*)
// ============================================================================
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

inline void ObDereferenceObject(ObjectHeader* object) {
    if (!object) return;
    object->releaseReference();
}

inline void ObMakeTemporaryObject(ObjectHeader* object) {
    if (!object) return;
    object->attributes &= ~OBJ_PERMANENT;
}

} // namespace micant::ob
