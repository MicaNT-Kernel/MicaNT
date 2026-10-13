#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/executive/mm.hpp"
#include "micant/km/executive/ps.hpp"

namespace micant::section {

// ============================================================================
// 1. Section Access Rights (NT Standard)
// ============================================================================
inline constexpr uint32_t SECTION_QUERY                = 0x0001;
inline constexpr uint32_t SECTION_MAP_WRITE            = 0x0002;
inline constexpr uint32_t SECTION_MAP_READ             = 0x0004;
inline constexpr uint32_t SECTION_MAP_EXECUTE          = 0x0008;
inline constexpr uint32_t SECTION_EXTEND_SIZE          = 0x0010;
inline constexpr uint32_t SECTION_MAP_EXECUTE_EXPLICIT = 0x0020;
inline constexpr uint32_t SECTION_ALL_ACCESS           = 0x001F001F;

// ============================================================================
// 2. Section Allocation Attributes (SEC_*)
// ============================================================================
inline constexpr uint32_t SEC_FILE           = 0x00800000;
inline constexpr uint32_t SEC_IMAGE          = 0x01000000;
inline constexpr uint32_t SEC_PROTECTED_IMAGE= 0x02000000;
inline constexpr uint32_t SEC_RESERVE        = 0x04000000;
inline constexpr uint32_t SEC_COMMIT         = 0x08000000;
inline constexpr uint32_t SEC_NOCACHE        = 0x10000000;
inline constexpr uint32_t SEC_WRITECOMBINE   = 0x40000000;
inline constexpr uint32_t SEC_LARGE_PAGES    = 0x80000000;

// ============================================================================
// 3. Section View Inherit Options (SECTION_INHERIT)
// ============================================================================
enum class SECTION_INHERIT : uint32_t {
    ViewShare = 1,
    ViewUnmap = 2
};
using ViewShare = SECTION_INHERIT;

// ============================================================================
// 4. Section Image Information Architecture (SEC_IMAGE)
// ============================================================================
struct SECTION_IMAGE_INFORMATION {
    void* transferAddress{nullptr};             // Entry point
    uint32_t zeroBits{0};
    size_t maximumStackSize{0x100000};          // 1 MB
    size_t committedStackSize{0x1000};          // 4 KB
    uint32_t subsystemType{3};                 // IMAGE_SUBSYSTEM_WINDOWS_CUI
    uint16_t subsystemMinorVersion{0};
    uint16_t subsystemMajorVersion{6};
    uint32_t imageCharacteristics{0x0022};     // EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
    uint16_t dllCharacteristics{0x8160};       // HIGH_ENTROPY_VA | DYNAMIC_BASE | NX_COMPAT | TERMINAL_SERVER_AWARE
    uint16_t machine{0x8664};                  // IMAGE_FILE_MACHINE_AMD64
    bool imageContainsCode{true};
    bool imageDll{false};
    uint32_t loaderFlags{0};
    uint32_t imageFileSize{0};
    uint32_t checkSum{0};
};

// Forward Declarations
struct CONTROL_AREA;
struct SEGMENT;

// ============================================================================
// 5. Subsection Architecture (SUBSECTION)
// ============================================================================
struct SUBSECTION {
    uint64_t fileOffset{0};
    size_t numberOfPages{0};
    size_t startingPteIndex{0};
    uint32_t protection{mm::PAGE_READWRITE};
    CONTROL_AREA* controlArea{nullptr};
    std::string subsectionName;
};

// ============================================================================
// 6. Prototype Page Table Entry (PROTOTYPE_PTE)
// ============================================================================
struct PROTOTYPE_PTE {
    uint64_t pfn{0};
    uint32_t protection{0};
    bool valid{false};
    bool modified{false};
    bool transition{false};
};

// ============================================================================
// 7. Segment Architecture (SEGMENT)
// ============================================================================
struct SEGMENT {
    CONTROL_AREA* controlArea{nullptr};
    size_t totalNumberOfPtes{0};
    size_t sizeOfSegment{0};
    std::vector<SUBSECTION> subsections;
    std::vector<PROTOTYPE_PTE> prototypePtes;
};

// ============================================================================
// 8. Control Area Architecture (CONTROL_AREA)
// ============================================================================
struct CONTROL_AREA {
    std::shared_ptr<SEGMENT> segment;
    uint32_t numberOfSectionReferences{0};
    uint32_t numberOfMappedViews{0};
    uint32_t numberOfUserReferences{0};
    void* fileObject{nullptr};
    std::wstring backingFilePath;

    // Control flags
    bool isImage{false};
    bool isFile{false};
    bool isCommit{true};
    bool isReserve{false};
    bool isNoCache{false};
    bool isWriteCombined{false};

    // Shared physical PFN tracking: page index -> PFN
    std::unordered_map<size_t, uint64_t> sharedPfnMap;

    // Coherent Backing Memory
    std::vector<uint8_t> backingStore;

    mutable std::mutex mutex;
};

// ============================================================================
// 9. Mapped Section View Descriptor (SECTION_VIEW)
// ============================================================================
struct SECTION_VIEW {
    ps::EProcess* process{nullptr};
    uintptr_t baseAddress{0};
    size_t viewSize{0};
    uint64_t sectionOffset{0};
    uint32_t protection{0};
    SECTION_INHERIT inheritDisposition{SECTION_INHERIT::ViewShare};
    bool isCopyOnWrite{false};
    std::vector<uint8_t> privateCowData;
};

// ============================================================================
// 10. Sovereign Section Object (SECTION / SectionObject)
// ============================================================================
class SectionObject {
public:
    SectionObject(
        size_t maximumSize,
        uint32_t allocationAttributes,
        uint32_t pageProtection,
        void* fileObject = nullptr,
        const std::wstring& backingPath = L""
    ) : maximumSize_(maximumSize),
        allocationAttributes_(allocationAttributes),
        pageProtection_(pageProtection)
    {
        controlArea_ = std::make_shared<CONTROL_AREA>();
        segment_ = std::make_shared<SEGMENT>();

        controlArea_->segment = segment_;
        segment_->controlArea = controlArea_.get();
        controlArea_->fileObject = fileObject;
        controlArea_->backingFilePath = backingPath;

        controlArea_->isFile = (allocationAttributes & SEC_FILE) != 0 || (fileObject != nullptr);
        controlArea_->isImage = (allocationAttributes & SEC_IMAGE) != 0;
        controlArea_->isCommit = (allocationAttributes & SEC_COMMIT) != 0;
        controlArea_->isReserve = (allocationAttributes & SEC_RESERVE) != 0;
        controlArea_->isNoCache = (allocationAttributes & SEC_NOCACHE) != 0;
        controlArea_->isWriteCombined = (allocationAttributes & SEC_WRITECOMBINE) != 0;
        controlArea_->numberOfSectionReferences = 1;

        if (maximumSize_ > 0) {
            controlArea_->backingStore.resize(maximumSize_, 0);
        }

        size_t pageCount = (maximumSize_ + mm::PageMask4KB) / mm::PageSize4KB;
        segment_->totalNumberOfPtes = pageCount;
        segment_->sizeOfSegment = maximumSize_;
        segment_->prototypePtes.resize(pageCount);

        if (controlArea_->isCommit) {
            auto& pfnDb = mm::PfnDatabase::get();
            for (size_t i = 0; i < pageCount; ++i) {
                uint64_t pfn = pfnDb.allocatePage(true);
                controlArea_->sharedPfnMap[i] = pfn;
                segment_->prototypePtes[i].pfn = pfn;
                segment_->prototypePtes[i].valid = true;
                segment_->prototypePtes[i].protection = pageProtection_;
            }
        }

        if (controlArea_->isImage) {
            initializeImageSubsections();
        }
    }

    ~SectionObject() {
        if (controlArea_) {
            auto& pfnDb = mm::PfnDatabase::get();
            for (const auto& [pageIdx, pfn] : controlArea_->sharedPfnMap) {
                pfnDb.freePage(pfn);
            }
            controlArea_->sharedPfnMap.clear();
        }
    }

    [[nodiscard]] size_t getSize() const noexcept { return maximumSize_; }
    [[nodiscard]] uint32_t getAllocationAttributes() const noexcept { return allocationAttributes_; }
    [[nodiscard]] uint32_t getPageProtection() const noexcept { return pageProtection_; }

    [[nodiscard]] uint8_t* getData() noexcept {
        return controlArea_ ? controlArea_->backingStore.data() : nullptr;
    }
    [[nodiscard]] const uint8_t* getData() const noexcept {
        return controlArea_ ? controlArea_->backingStore.data() : nullptr;
    }

    void loadData(const void* src, size_t size) {
        if (!src || size == 0 || !controlArea_) return;
        size_t copySize = std::min(size, maximumSize_);
        std::memcpy(controlArea_->backingStore.data(), src, copySize);
    }

    [[nodiscard]] std::shared_ptr<CONTROL_AREA> getControlArea() const noexcept { return controlArea_; }
    [[nodiscard]] std::shared_ptr<SEGMENT> getSegment() const noexcept { return segment_; }
    [[nodiscard]] const SECTION_IMAGE_INFORMATION& getImageInformation() const noexcept { return imageInfo_; }
    void setImageInformation(const SECTION_IMAGE_INFORMATION& info) noexcept { imageInfo_ = info; }

    [[nodiscard]] size_t getMappedViewCount() const noexcept {
        return controlArea_ ? controlArea_->numberOfMappedViews : 0;
    }

    void addView(const SECTION_VIEW& view) {
        std::lock_guard<std::mutex> lock(mutex_);
        views_.push_back(view);
        if (controlArea_) {
            controlArea_->numberOfMappedViews++;
        }
    }

    bool removeView(ps::EProcess* process, uintptr_t baseAddress) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(views_.begin(), views_.end(), [&](const SECTION_VIEW& v) {
            return v.process == process && v.baseAddress == baseAddress;
        });
        if (it != views_.end()) {
            views_.erase(it);
            if (controlArea_ && controlArea_->numberOfMappedViews > 0) {
                controlArea_->numberOfMappedViews--;
            }
            return true;
        }
        return false;
    }

    SECTION_VIEW* findView(ps::EProcess* process, uintptr_t baseAddress) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& v : views_) {
            if (v.process == process && v.baseAddress == baseAddress) {
                return &v;
            }
        }
        return nullptr;
    }

    const std::vector<SECTION_VIEW>& getMappedViews() const noexcept { return views_; }

    NtStatus read(uint64_t offset, void* buffer, size_t size) {
        if (!buffer || size == 0) return NtStatus::InvalidParameter;
        if (offset + size > maximumSize_) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(controlArea_->mutex);
        std::memcpy(buffer, controlArea_->backingStore.data() + offset, size);
        return NtStatus::Success;
    }

    NtStatus write(uint64_t offset, const void* buffer, size_t size) {
        if (!buffer || size == 0) return NtStatus::InvalidParameter;
        if (offset + size > maximumSize_) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(controlArea_->mutex);
        std::memcpy(controlArea_->backingStore.data() + offset, buffer, size);
        return NtStatus::Success;
    }

private:
    void initializeImageSubsections() {
        size_t totalPages = segment_->totalNumberOfPtes;
        size_t textPages = std::max<size_t>(1, (totalPages * 40) / 100);
        size_t rdataPages = std::max<size_t>(1, (totalPages * 30) / 100);
        size_t dataPages = totalPages > (textPages + rdataPages) ? totalPages - (textPages + rdataPages) : 1;

        SUBSECTION secText{
            .fileOffset = 0x1000,
            .numberOfPages = textPages,
            .startingPteIndex = 0,
            .protection = mm::PAGE_EXECUTE_READ,
            .controlArea = controlArea_.get(),
            .subsectionName = ".text"
        };
        SUBSECTION secRdata{
            .fileOffset = 0x1000 + textPages * mm::PageSize4KB,
            .numberOfPages = rdataPages,
            .startingPteIndex = textPages,
            .protection = mm::PAGE_READONLY,
            .controlArea = controlArea_.get(),
            .subsectionName = ".rdata"
        };
        SUBSECTION secData{
            .fileOffset = 0x1000 + (textPages + rdataPages) * mm::PageSize4KB,
            .numberOfPages = dataPages,
            .startingPteIndex = textPages + rdataPages,
            .protection = mm::PAGE_READWRITE,
            .controlArea = controlArea_.get(),
            .subsectionName = ".data"
        };

        segment_->subsections.push_back(secText);
        segment_->subsections.push_back(secRdata);
        segment_->subsections.push_back(secData);

        imageInfo_.transferAddress = reinterpret_cast<void*>(0x0000000140001000ULL);
        imageInfo_.imageFileSize = static_cast<uint32_t>(maximumSize_);
    }

    size_t maximumSize_{0};
    uint32_t allocationAttributes_{0};
    uint32_t pageProtection_{0};
    std::shared_ptr<CONTROL_AREA> controlArea_;
    std::shared_ptr<SEGMENT> segment_;
    SECTION_IMAGE_INFORMATION imageInfo_{};
    std::vector<SECTION_VIEW> views_;
    mutable std::mutex mutex_;
};

// ============================================================================
// 11. Section Telemetry & Diagnostics (SECTION_TELEMETRY)
// ============================================================================
struct SECTION_TELEMETRY {
    size_t totalSectionsCreated{0};
    size_t totalActiveSections{0};
    size_t totalMappedViews{0};
    size_t totalSharedPages{0};
    size_t totalCowSplits{0};
    size_t totalFlushedPages{0};
};

// ============================================================================
// 12. Sovereign Section Manager (SectionManager)
// ============================================================================
class SectionManager {
public:
    static SectionManager& get() {
        static SectionManager instance;
        return instance;
    }

    NtStatus createSection(
        Handle& outSectionHandle,
        ob::HandleTable& handleTable,
        size_t maximumSize,
        uint32_t allocationAttributes,
        uint32_t pageProtection,
        std::shared_ptr<SectionObject>& outSection,
        const std::wstring& sectionName = L""
    ) {
        if (maximumSize == 0) {
            return NtStatus::InvalidParameter;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        outSection = std::make_shared<SectionObject>(maximumSize, allocationAttributes, pageProtection);
        activeSections_.push_back(outSection);
        telemetry_.totalSectionsCreated++;
        telemetry_.totalActiveSections++;
        telemetry_.totalSharedPages += outSection->getControlArea()->sharedPfnMap.size();

        // Wrap in ObjectHeader for Object Manager namespace integration
        auto header = std::make_unique<ob::ObjectHeader>();
        header->objectName = sectionName.empty() ? L"\\BaseNamedObjects\\Section" : sectionName;

        NtStatus status = handleTable.createHandle(header.get(), outSectionHandle);
        if (NT_SUCCESS(status)) {
            header.release(); // Retain reference in handle table
        }
        return status;
    }

    NtStatus createFileBackedSection(
        Handle& outSectionHandle,
        ob::HandleTable& handleTable,
        void* fileObject,
        const std::wstring& filePath,
        size_t maximumSize,
        uint32_t allocationAttributes,
        uint32_t pageProtection,
        std::shared_ptr<SectionObject>& outSection,
        const std::wstring& sectionName = L""
    ) {
        if (maximumSize == 0) return NtStatus::InvalidParameter;
        uint32_t attrs = allocationAttributes | SEC_FILE;
        std::lock_guard<std::mutex> lock(mutex_);
        outSection = std::make_shared<SectionObject>(maximumSize, attrs, pageProtection, fileObject, filePath);
        activeSections_.push_back(outSection);
        telemetry_.totalSectionsCreated++;
        telemetry_.totalActiveSections++;
        telemetry_.totalSharedPages += outSection->getControlArea()->sharedPfnMap.size();

        auto header = std::make_unique<ob::ObjectHeader>();
        header->objectName = sectionName.empty() ? L"\\BaseNamedObjects\\FileBackedSection" : sectionName;
        NtStatus status = handleTable.createHandle(header.get(), outSectionHandle);
        if (NT_SUCCESS(status)) {
            header.release();
        }
        return status;
    }

    NtStatus mapViewOfSection(
        std::shared_ptr<SectionObject> section,
        ps::EProcess& targetProcess,
        uintptr_t& baseAddress,
        size_t& viewSize,
        uint32_t allocationType,
        uint32_t protect,
        uint64_t sectionOffset = 0,
        SECTION_INHERIT inherit = SECTION_INHERIT::ViewShare
    ) {
        if (!section) return NtStatus::InvalidHandle;

        if (viewSize == 0 || viewSize > (section->getSize() - sectionOffset)) {
            viewSize = section->getSize() - sectionOffset;
        }

        uint32_t effectiveProtect = (protect != 0) ? protect : section->getPageProtection();
        bool isCow = (effectiveProtect == mm::PAGE_WRITECOPY || effectiveProtect == mm::PAGE_EXECUTE_WRITECOPY);

        // Map coherent view into target process address space using shared physical PFNs
        NtStatus status = targetProcess.getAddressSpace().mapSectionView(
            baseAddress,
            viewSize,
            effectiveProtect,
            section->getControlArea()->sharedPfnMap,
            section.get(),
            sectionOffset
        );

        if (!NT_SUCCESS(status)) {
            status = targetProcess.getAddressSpace().allocate(
                baseAddress,
                viewSize,
                allocationType ? allocationType : (mm::MEM_COMMIT | mm::MEM_RESERVE),
                effectiveProtect
            );
        }

        if (NT_SUCCESS(status)) {
            SECTION_VIEW view{
                .process = &targetProcess,
                .baseAddress = baseAddress,
                .viewSize = viewSize,
                .sectionOffset = sectionOffset,
                .protection = effectiveProtect,
                .inheritDisposition = inherit,
                .isCopyOnWrite = isCow,
                .privateCowData = {}
            };
            section->addView(view);
            std::lock_guard<std::mutex> lock(mutex_);
            telemetry_.totalMappedViews++;
        }

        return status;
    }

    NtStatus unmapViewOfSection(ps::EProcess& targetProcess, uintptr_t baseAddress) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sec : activeSections_) {
            if (sec && sec->removeView(&targetProcess, baseAddress)) {
                if (telemetry_.totalMappedViews > 0) telemetry_.totalMappedViews--;
                break;
            }
        }
        return targetProcess.getAddressSpace().free(baseAddress, 0, mm::MEM_RELEASE);
    }

    NtStatus readView(
        ps::EProcess& process,
        uintptr_t baseAddress,
        uint64_t viewOffset,
        void* buffer,
        size_t size
    ) {
        if (!buffer || size == 0) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sec : activeSections_) {
            if (!sec) continue;
            auto* view = sec->findView(&process, baseAddress);
            if (view) {
                if (viewOffset + size > view->viewSize) return NtStatus::AccessViolation;
                if (!view->privateCowData.empty()) {
                    std::memcpy(buffer, view->privateCowData.data() + viewOffset, size);
                } else {
                    return sec->read(view->sectionOffset + viewOffset, buffer, size);
                }
                return NtStatus::Success;
            }
        }
        return NtStatus::NotFound;
    }

    NtStatus writeView(
        ps::EProcess& process,
        uintptr_t baseAddress,
        uint64_t viewOffset,
        const void* buffer,
        size_t size
    ) {
        if (!buffer || size == 0) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sec : activeSections_) {
            if (!sec) continue;
            auto* view = sec->findView(&process, baseAddress);
            if (view) {
                if (viewOffset + size > view->viewSize) return NtStatus::AccessViolation;
                if (view->protection == mm::PAGE_READONLY || view->protection == mm::PAGE_EXECUTE_READ) {
                    return NtStatus::AccessViolation;
                }

                if (view->isCopyOnWrite) {
                    if (view->privateCowData.empty()) {
                        view->privateCowData.resize(view->viewSize);
                        std::memcpy(view->privateCowData.data(), sec->getData() + view->sectionOffset, view->viewSize);
                        telemetry_.totalCowSplits++;
                    }
                    std::memcpy(view->privateCowData.data() + viewOffset, buffer, size);
                    return NtStatus::Success;
                }

                return sec->write(view->sectionOffset + viewOffset, buffer, size);
            }
        }
        return NtStatus::NotFound;
    }

    NtStatus flushVirtualMemory(
        ps::EProcess& process,
        uintptr_t baseAddress,
        size_t regionSize
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sec : activeSections_) {
            if (!sec) continue;
            auto* view = sec->findView(&process, baseAddress);
            if (view) {
                size_t flushLen = regionSize == 0 ? view->viewSize : std::min(regionSize, view->viewSize);
                telemetry_.totalFlushedPages += (flushLen + mm::PageMask4KB) / mm::PageSize4KB;
                return NtStatus::Success;
            }
        }
        return NtStatus::NotFound;
    }

    [[nodiscard]] SECTION_TELEMETRY getTelemetry() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return telemetry_;
    }

    void resetForTesting() {
        std::lock_guard<std::mutex> lock(mutex_);
        activeSections_.clear();
        telemetry_ = SECTION_TELEMETRY{};
    }

private:
    SectionManager() = default;

    std::vector<std::shared_ptr<SectionObject>> activeSections_;
    SECTION_TELEMETRY telemetry_{};
    mutable std::mutex mutex_;
};

// ============================================================================
// 13. Canonical NT Memory Manager Section APIs (Mm* & Nt*)
// ============================================================================

inline NtStatus MmCreateSection(
    std::shared_ptr<SectionObject>& section,
    uint32_t desiredAccess,
    size_t maximumSize,
    uint32_t sectionPageProtection,
    uint32_t allocationAttributes,
    void* fileObject = nullptr
) {
    (void)desiredAccess;
    Handle dummyHandle = 0;
    ob::HandleTable dummyTable;
    return SectionManager::get().createSection(
        dummyHandle,
        dummyTable,
        maximumSize,
        allocationAttributes,
        sectionPageProtection,
        section
    );
}

inline NtStatus MmMapViewOfSection(
    std::shared_ptr<SectionObject> section,
    ps::EProcess& targetProcess,
    uintptr_t& baseAddress,
    size_t& viewSize,
    uint32_t allocationType,
    uint32_t protect
) {
    return SectionManager::get().mapViewOfSection(
        section,
        targetProcess,
        baseAddress,
        viewSize,
        allocationType,
        protect
    );
}

inline NtStatus MmUnmapViewOfSection(
    ps::EProcess& targetProcess,
    uintptr_t baseAddress
) {
    return SectionManager::get().unmapViewOfSection(targetProcess, baseAddress);
}

inline NtStatus MmFlushVirtualMemory(
    ps::EProcess& targetProcess,
    uintptr_t baseAddress,
    size_t regionSize
) {
    return SectionManager::get().flushVirtualMemory(targetProcess, baseAddress, regionSize);
}

} // namespace micant::section
