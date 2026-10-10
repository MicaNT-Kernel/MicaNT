#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include "ntdef.hpp"

namespace micant::ob {

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
    Job
};

struct ObjectHeader;

/**
 * @brief Representation of an Object Type in the Object Manager.
 */
struct ObjectType {
    std::wstring typeName;
    ObjectTypeId typeId;
    uint32_t totalNumberOfObjects{0};
    uint32_t totalNumberOfHandles{0};
    uint32_t validAccessMask{0x001FFFFF};
};

/**
 * @brief Object Header prepended to every allocated kernel object body.
 */
struct ObjectHeader {
    std::atomic<int32_t> pointerCount{1};
    std::atomic<int32_t> handleCount{0};
    const ObjectType* type{nullptr};
    std::wstring objectName;
    ObjectHeader* parentDirectory{nullptr};
    uint32_t attributes{0};

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
};

/**
 * @brief Directory Object representing folders in the NT Object Namespace.
 */
class DirectoryObject {
public:
    explicit DirectoryObject(std::wstring name) : name_(std::move(name)) {}

    NtStatus insertObject(std::wstring_view name, ObjectHeader* object) {
        if (!object) return NtStatus::InvalidParameter;
        std::wstring key(name);
        if (entries_.contains(key)) {
            return NtStatus::ObjectNameCollision;
        }
        object->addReference();
        entries_[key] = object;
        return NtStatus::Success;
    }

    [[nodiscard]] ObjectHeader* lookup(std::wstring_view name) const {
        auto it = entries_.find(std::wstring(name));
        if (it != entries_.end()) {
            return it->second;
        }
        return nullptr;
    }

    [[nodiscard]] const std::wstring& getName() const noexcept { return name_; }
    [[nodiscard]] size_t getEntryCount() const noexcept { return entries_.size(); }

private:
    std::wstring name_;
    std::unordered_map<std::wstring, ObjectHeader*> entries_;
};

/**
 * @brief Per-Process Handle Table.
 * Maps opaque userland `Handle` (values 4, 8, 12...) to ObjectHeader pointers.
 */
class HandleTable {
public:
    static constexpr size_t MaxHandles = 4096;

    HandleTable() {
        table_.resize(MaxHandles, nullptr);
    }

    NtStatus createHandle(ObjectHeader* object, Handle& outHandle) {
        if (!object) return NtStatus::InvalidParameter;

        // NT Handles start at 4 and increment by 4
        for (size_t i = 1; i < table_.size(); ++i) {
            if (table_[i] == nullptr) {
                object->addReference();
                object->handleCount.fetch_add(1, std::memory_order_relaxed);
                table_[i] = object;
                outHandle = static_cast<Handle>(i * 4);
                return NtStatus::Success;
            }
        }
        return NtStatus::NoMemory;
    }

    [[nodiscard]] ObjectHeader* lookup(Handle handle) const {
        size_t index = static_cast<size_t>(handle) / 4;
        if (index == 0 || index >= table_.size()) return nullptr;
        return table_[index];
    }

    NtStatus closeHandle(Handle handle) {
        size_t index = static_cast<size_t>(handle) / 4;
        if (index == 0 || index >= table_.size() || table_[index] == nullptr) {
            return NtStatus::InvalidHandle;
        }

        ObjectHeader* obj = table_[index];
        table_[index] = nullptr;
        obj->handleCount.fetch_sub(1, std::memory_order_relaxed);
        obj->releaseReference();
        return NtStatus::Success;
    }

private:
    std::vector<ObjectHeader*> table_;
};

/**
 * @brief Symbolic Link Object in the NT Object Namespace (\DosDevices\C: -> \Device\HarddiskVolume1).
 */
class SymbolicLinkObject {
public:
    explicit SymbolicLinkObject(std::wstring targetPath) : targetPath_(std::move(targetPath)) {}
    [[nodiscard]] const std::wstring& getTargetPath() const noexcept { return targetPath_; }
    void setTargetPath(std::wstring targetPath) noexcept { targetPath_ = std::move(targetPath); }

private:
    std::wstring targetPath_;
};

/**
 * @brief Global Object Manager Namespace Root & Path Resolution.
 */
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

        // Canonical Windows NT Symbolic Links
        createSymbolicLink(L"\\DosDevices\\NUL", L"\\Device\\Null");
        createSymbolicLink(L"\\DosDevices\\CON", L"\\Device\\Console");
        createSymbolicLink(L"\\DosDevices\\PIPE", L"\\Device\\NamedPipe");
        createSymbolicLink(L"\\DosDevices\\MAILSLOT", L"\\Device\\Mailslot");
        createSymbolicLink(L"\\DosDevices\\PhysicalDrive0", L"\\Device\\Harddisk0");
    }

    bool createDirectory(std::wstring_view fullPath) {
        std::wstring path(fullPath);
        if (directories_.contains(path)) return true;
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

    [[nodiscard]] std::wstring resolvePath(std::wstring_view path) const {
        std::wstring current(path);
        // Canonical NT prefix normalization (\??\ and \\.\ -> \DosDevices\)
        if (current.starts_with(L"\\??\\")) {
            current = L"\\DosDevices\\" + current.substr(4);
        } else if (current.starts_with(L"\\\\.\\")) {
            current = L"\\DosDevices\\" + current.substr(4);
        }

        // Check exact match in symbolic links
        auto it = symbolicLinks_.find(current);
        if (it != symbolicLinks_.end()) {
            return it->second->getTargetPath();
        }
        // Check prefix match (e.g. \DosDevices\C:\Windows -> \Device\HarddiskVolume1\Windows)
        for (const auto& [link, target] : symbolicLinks_) {
            if (current.starts_with(link)) {
                return target->getTargetPath() + current.substr(link.length());
            }
        }
        return current;
    }

    [[nodiscard]] std::shared_ptr<DirectoryObject> getDirectory(std::wstring_view path) const {
        auto it = directories_.find(std::wstring(path));
        if (it != directories_.end()) return it->second;
        return nullptr;
    }

    [[nodiscard]] size_t getDirectoryCount() const noexcept { return directories_.size(); }
    [[nodiscard]] size_t getSymbolicLinkCount() const noexcept { return symbolicLinks_.size(); }

private:
    ObjectNamespace() = default;
    bool initialized_{false};
    std::unordered_map<std::wstring, std::shared_ptr<DirectoryObject>> directories_;
    std::unordered_map<std::wstring, std::shared_ptr<SymbolicLinkObject>> symbolicLinks_;
};

} // namespace micant::ob
