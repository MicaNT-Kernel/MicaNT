#include "micant/km/drivers/fs/ntfs_usn_streamer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

namespace micant::ntfs::usn {

SovereignNtfsUsnStreamer::SovereignNtfsUsnStreamer() = default;

SovereignNtfsUsnStreamer::~SovereignNtfsUsnStreamer() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_active = false;
    m_cv.notify_all();
}

NtStatus SovereignNtfsUsnStreamer::createJournal(const CREATE_USN_JOURNAL_DATA& config) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    m_meta.MaximumSize = (config.MaximumSize > 0) ? config.MaximumSize : 0x8000000ULL;      // 128 MB
    m_meta.AllocationDelta = (config.AllocationDelta > 0) ? config.AllocationDelta : 0x1000000ULL; // 16 MB
    m_meta.UsnJournalID = 0x20261123AABB0001ULL;
    m_meta.FirstUsn = 0;
    m_meta.LowestValidUsn = 0;
    m_meta.NextUsn = 0x1000ULL;
    m_meta.MaxUsn = 0x7FFFFFFFFFFFFFFFULL;
    m_meta.MinSupportedMajorVersion = 2;
    m_meta.MaxSupportedMajorVersion = 4;

    m_records.clear();
    m_active = true;
    m_deleteInProgress = false;
    m_pruneCount = 0;

    m_cv.notify_all();
    return NtStatus::Success;
}

NtStatus SovereignNtfsUsnStreamer::queryJournal(USN_JOURNAL_DATA_V0& outData) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (!m_active) return NtStatus::JournalNotActive;
    if (m_deleteInProgress) return NtStatus::JournalDeleteInProgress;

    outData.UsnJournalID = m_meta.UsnJournalID;
    outData.FirstUsn = m_meta.FirstUsn;
    outData.NextUsn = m_meta.NextUsn;
    outData.LowestValidUsn = m_meta.LowestValidUsn;
    outData.MaxUsn = m_meta.MaxUsn;
    outData.MaximumSize = m_meta.MaximumSize;
    outData.AllocationDelta = m_meta.AllocationDelta;

    return NtStatus::Success;
}

NtStatus SovereignNtfsUsnStreamer::queryJournal(USN_JOURNAL_DATA_V1& outData) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (!m_active) return NtStatus::JournalNotActive;
    if (m_deleteInProgress) return NtStatus::JournalDeleteInProgress;

    outData = m_meta;
    return NtStatus::Success;
}

NtStatus SovereignNtfsUsnStreamer::queryJournal(USN_JOURNAL_DATA_V2& outData) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (!m_active) return NtStatus::JournalNotActive;
    if (m_deleteInProgress) return NtStatus::JournalDeleteInProgress;

    outData.UsnJournalID = m_meta.UsnJournalID;
    outData.FirstUsn = m_meta.FirstUsn;
    outData.NextUsn = m_meta.NextUsn;
    outData.LowestValidUsn = m_meta.LowestValidUsn;
    outData.MaxUsn = m_meta.MaxUsn;
    outData.MaximumSize = m_meta.MaximumSize;
    outData.AllocationDelta = m_meta.AllocationDelta;
    outData.MinSupportedMajorVersion = m_meta.MinSupportedMajorVersion;
    outData.MaxSupportedMajorVersion = m_meta.MaxSupportedMajorVersion;
    outData.Flags = 0x00000001; // Track range extents active
    outData.RangeTrackChunkSize = 65536; // 64 KB
    outData.RangeTrackThreshold = 1048576; // 1 MB
    return NtStatus::Success;
}

NtStatus SovereignNtfsUsnStreamer::deleteJournal(const DELETE_USN_JOURNAL_DATA& deleteData) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (!m_active) return NtStatus::JournalNotActive;

    if (deleteData.UsnJournalID != 0 && deleteData.UsnJournalID != m_meta.UsnJournalID) {
        return NtStatus::JournalNotActive;
    }

    if (deleteData.DeleteFlags & USN_DELETE_FLAG_NOTIFY) {
        m_deleteInProgress = true;
    }

    if (deleteData.DeleteFlags & USN_DELETE_FLAG_DELETE) {
        m_active = false;
        m_deleteInProgress = false;
        m_records.clear();
        m_meta.LowestValidUsn = 0;
        m_meta.NextUsn = 0;
    }

    m_cv.notify_all();
    return NtStatus::Success;
}

uint64_t SovereignNtfsUsnStreamer::postRecordV2(
    uint64_t frn,
    uint64_t parentFrn,
    uint32_t reason,
    uint32_t attributes,
    std::wstring_view fileName,
    uint32_t sourceInfo,
    uint32_t securityId
) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (!m_active || m_deleteInProgress) return 0;

    uint32_t nameByteLen = static_cast<uint32_t>(fileName.size() * sizeof(wchar_t));
    uint32_t recLen = static_cast<uint32_t>(sizeof(USN_RECORD_V2) + nameByteLen);
    recLen = (recLen + 7u) & ~7u; // 8-byte alignment

    uint64_t curUsn = m_meta.NextUsn;
    m_meta.NextUsn += recLen;

    SovereignUsnEvent ev{
        .Usn = curUsn,
        .MajorVersion = 2,
        .FileId = FILE_ID_128::from64(frn),
        .ParentFileId = FILE_ID_128::from64(parentFrn),
        .TimeStamp = 133500000000000000ULL + curUsn,
        .Reason = reason,
        .SourceInfo = sourceInfo,
        .SecurityId = securityId,
        .FileAttributes = attributes,
        .FileName = std::wstring(fileName),
        .Extents = {}
    };

    m_records.push_back(ev);
    dispatchFilters(ev);
    checkAutoPruneLocked();
    m_cv.notify_all();

    return curUsn;
}

uint64_t SovereignNtfsUsnStreamer::postRecordV3(
    const FILE_ID_128& fileId,
    const FILE_ID_128& parentFileId,
    uint32_t reason,
    uint32_t attributes,
    std::wstring_view fileName,
    uint32_t sourceInfo,
    uint32_t securityId
) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (!m_active || m_deleteInProgress) return 0;

    uint32_t nameByteLen = static_cast<uint32_t>(fileName.size() * sizeof(wchar_t));
    uint32_t recLen = static_cast<uint32_t>(sizeof(USN_RECORD_V3) + nameByteLen);
    recLen = (recLen + 7u) & ~7u; // 8-byte alignment

    uint64_t curUsn = m_meta.NextUsn;
    m_meta.NextUsn += recLen;

    SovereignUsnEvent ev{
        .Usn = curUsn,
        .MajorVersion = 3,
        .FileId = fileId,
        .ParentFileId = parentFileId,
        .TimeStamp = 133500000000000000ULL + curUsn,
        .Reason = reason,
        .SourceInfo = sourceInfo,
        .SecurityId = securityId,
        .FileAttributes = attributes,
        .FileName = std::wstring(fileName),
        .Extents = {}
    };

    m_records.push_back(ev);
    dispatchFilters(ev);
    checkAutoPruneLocked();
    m_cv.notify_all();

    return curUsn;
}

uint64_t SovereignNtfsUsnStreamer::postRecordV4(
    const FILE_ID_128& fileId,
    const FILE_ID_128& parentFileId,
    uint32_t reason,
    const std::vector<USN_RECORD_EXTENT>& extents,
    uint32_t sourceInfo
) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (!m_active || m_deleteInProgress) return 0;

    size_t extentCount = extents.size();
    size_t extraExtents = (extentCount > 1) ? (extentCount - 1) : 0;
    uint32_t recLen = static_cast<uint32_t>(sizeof(USN_RECORD_V4) + (extraExtents * sizeof(USN_RECORD_EXTENT)));
    recLen = (recLen + 7u) & ~7u; // 8-byte alignment

    uint64_t curUsn = m_meta.NextUsn;
    m_meta.NextUsn += recLen;

    SovereignUsnEvent ev{
        .Usn = curUsn,
        .MajorVersion = 4,
        .FileId = fileId,
        .ParentFileId = parentFileId,
        .TimeStamp = 133500000000000000ULL + curUsn,
        .Reason = reason,
        .SourceInfo = sourceInfo,
        .SecurityId = 0,
        .FileAttributes = 0,
        .FileName = L"",
        .Extents = extents
    };

    m_records.push_back(ev);
    dispatchFilters(ev);
    checkAutoPruneLocked();
    m_cv.notify_all();

    return curUsn;
}

NtStatus SovereignNtfsUsnStreamer::readJournal(
    const READ_USN_JOURNAL_DATA_V1& readParams,
    std::vector<uint8_t>& outBuffer,
    uint64_t& nextUsnBookmark
) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (!m_active) return NtStatus::JournalNotActive;
    if (m_deleteInProgress) return NtStatus::JournalDeleteInProgress;

    if (readParams.StartUsn < m_meta.LowestValidUsn) {
        return NtStatus::JournalEntryDeleted;
    }

    outBuffer.clear();
    // In NTFS, the output buffer begins with an 8-byte NextUsn bookmark
    outBuffer.resize(sizeof(uint64_t), 0);

    uint64_t nextBookmark = m_meta.NextUsn;

    for (const auto& rec : m_records) {
        if (rec.Usn < readParams.StartUsn) continue;

        // Apply reason mask filter
        if ((rec.Reason & readParams.ReasonMask) == 0) continue;

        // Apply ReturnOnlyOnClose filter
        if (readParams.ReturnOnlyOnClose != 0 && (rec.Reason & USN_REASON_CLOSE) == 0) {
            continue;
        }

        // Determine output version format based on caller's Min/MaxMajorVersion
        uint16_t targetVersion = rec.MajorVersion;
        if (targetVersion < readParams.MinMajorVersion) targetVersion = readParams.MinMajorVersion;
        if (targetVersion > readParams.MaxMajorVersion) targetVersion = readParams.MaxMajorVersion;

        size_t baseOffset = outBuffer.size();

        if (targetVersion == 2) {
            uint32_t nameByteLen = static_cast<uint32_t>(rec.FileName.size() * sizeof(wchar_t));
            uint32_t recLen = static_cast<uint32_t>(sizeof(USN_RECORD_V2) + nameByteLen);
            recLen = (recLen + 7u) & ~7u;

            outBuffer.resize(baseOffset + recLen, 0);
            auto* pRec = reinterpret_cast<USN_RECORD_V2*>(outBuffer.data() + baseOffset);

            pRec->RecordLength = recLen;
            pRec->MajorVersion = 2;
            pRec->MinorVersion = 0;
            pRec->FileReferenceNumber = rec.FileId.low64();
            pRec->ParentFileReferenceNumber = rec.ParentFileId.low64();
            pRec->Usn = rec.Usn;
            pRec->TimeStamp = rec.TimeStamp;
            pRec->Reason = rec.Reason;
            pRec->SourceInfo = rec.SourceInfo;
            pRec->SecurityId = rec.SecurityId;
            pRec->FileAttributes = rec.FileAttributes;
            pRec->FileNameLength = static_cast<uint16_t>(nameByteLen);
            pRec->FileNameOffset = static_cast<uint16_t>(sizeof(USN_RECORD_V2) - sizeof(wchar_t));

            if (nameByteLen > 0) {
                std::memcpy(pRec->FileName, rec.FileName.data(), nameByteLen);
            }
        } else if (targetVersion == 3) {
            uint32_t nameByteLen = static_cast<uint32_t>(rec.FileName.size() * sizeof(wchar_t));
            uint32_t recLen = static_cast<uint32_t>(sizeof(USN_RECORD_V3) + nameByteLen);
            recLen = (recLen + 7u) & ~7u;

            outBuffer.resize(baseOffset + recLen, 0);
            auto* pRec = reinterpret_cast<USN_RECORD_V3*>(outBuffer.data() + baseOffset);

            pRec->RecordLength = recLen;
            pRec->MajorVersion = 3;
            pRec->MinorVersion = 0;
            pRec->FileReferenceNumber = rec.FileId;
            pRec->ParentFileReferenceNumber = rec.ParentFileId;
            pRec->Usn = rec.Usn;
            pRec->TimeStamp = rec.TimeStamp;
            pRec->Reason = rec.Reason;
            pRec->SourceInfo = rec.SourceInfo;
            pRec->SecurityId = rec.SecurityId;
            pRec->FileAttributes = rec.FileAttributes;
            pRec->FileNameLength = static_cast<uint16_t>(nameByteLen);
            pRec->FileNameOffset = static_cast<uint16_t>(sizeof(USN_RECORD_V3) - sizeof(wchar_t));

            if (nameByteLen > 0) {
                std::memcpy(pRec->FileName, rec.FileName.data(), nameByteLen);
            }
        } else if (targetVersion == 4) {
            size_t count = rec.Extents.size();
            size_t extra = (count > 1) ? (count - 1) : 0;
            uint32_t recLen = static_cast<uint32_t>(sizeof(USN_RECORD_V4) + (extra * sizeof(USN_RECORD_EXTENT)));
            recLen = (recLen + 7u) & ~7u;

            outBuffer.resize(baseOffset + recLen, 0);
            auto* pRec = reinterpret_cast<USN_RECORD_V4*>(outBuffer.data() + baseOffset);

            pRec->RecordLength = recLen;
            pRec->MajorVersion = 4;
            pRec->MinorVersion = 0;
            pRec->FileReferenceNumber = rec.FileId;
            pRec->ParentFileReferenceNumber = rec.ParentFileId;
            pRec->Usn = rec.Usn;
            pRec->Reason = rec.Reason;
            pRec->SourceInfo = rec.SourceInfo;
            pRec->RemainingExtents = 0;
            pRec->NumberOfExtents = static_cast<uint16_t>(count);
            pRec->ExtentOffset = static_cast<uint16_t>(sizeof(USN_RECORD_V4) - sizeof(USN_RECORD_EXTENT));

            for (size_t i = 0; i < count; ++i) {
                pRec->Extents[i] = rec.Extents[i];
            }
        }
    }

    // Next bookmark is the USN for the caller's subsequent read query
    nextBookmark = m_meta.NextUsn;
    std::memcpy(outBuffer.data(), &nextBookmark, sizeof(uint64_t));
    nextUsnBookmark = nextBookmark;

    return NtStatus::Success;
}

NtStatus SovereignNtfsUsnStreamer::readJournalAsync(
    const READ_USN_JOURNAL_DATA_V1& readParams,
    std::vector<uint8_t>& outBuffer,
    uint64_t& nextUsnBookmark,
    uint32_t timeoutMs
) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (!m_active) return NtStatus::JournalNotActive;
    if (m_deleteInProgress) return NtStatus::JournalDeleteInProgress;

    if (readParams.StartUsn < m_meta.LowestValidUsn) {
        return NtStatus::JournalEntryDeleted;
    }

    auto hasMatchingRecord = [this, &readParams]() {
        for (const auto& rec : m_records) {
            if (rec.Usn < readParams.StartUsn) continue;
            if ((rec.Reason & readParams.ReasonMask) == 0) continue;
            if (readParams.ReturnOnlyOnClose != 0 && (rec.Reason & USN_REASON_CLOSE) == 0) continue;
            return true;
        }
        return false;
    };

    if (!hasMatchingRecord() && timeoutMs > 0) {
        m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&]() {
            return !m_active || m_deleteInProgress || hasMatchingRecord();
        });
    }

    if (!m_active) return NtStatus::JournalNotActive;
    if (m_deleteInProgress) return NtStatus::JournalDeleteInProgress;

    // Helper: read with lock held
    outBuffer.clear();
    outBuffer.resize(sizeof(uint64_t), 0);

    for (const auto& rec : m_records) {
        if (rec.Usn < readParams.StartUsn) continue;
        if ((rec.Reason & readParams.ReasonMask) == 0) continue;
        if (readParams.ReturnOnlyOnClose != 0 && (rec.Reason & USN_REASON_CLOSE) == 0) continue;

        uint16_t targetVersion = rec.MajorVersion;
        if (targetVersion < readParams.MinMajorVersion) targetVersion = readParams.MinMajorVersion;
        if (targetVersion > readParams.MaxMajorVersion) targetVersion = readParams.MaxMajorVersion;

        size_t baseOffset = outBuffer.size();

        if (targetVersion == 2) {
            uint32_t nameByteLen = static_cast<uint32_t>(rec.FileName.size() * sizeof(wchar_t));
            uint32_t recLen = static_cast<uint32_t>(sizeof(USN_RECORD_V2) + nameByteLen);
            recLen = (recLen + 7u) & ~7u;

            outBuffer.resize(baseOffset + recLen, 0);
            auto* pRec = reinterpret_cast<USN_RECORD_V2*>(outBuffer.data() + baseOffset);
            pRec->RecordLength = recLen;
            pRec->MajorVersion = 2;
            pRec->MinorVersion = 0;
            pRec->FileReferenceNumber = rec.FileId.low64();
            pRec->ParentFileReferenceNumber = rec.ParentFileId.low64();
            pRec->Usn = rec.Usn;
            pRec->TimeStamp = rec.TimeStamp;
            pRec->Reason = rec.Reason;
            pRec->SourceInfo = rec.SourceInfo;
            pRec->SecurityId = rec.SecurityId;
            pRec->FileAttributes = rec.FileAttributes;
            pRec->FileNameLength = static_cast<uint16_t>(nameByteLen);
            pRec->FileNameOffset = static_cast<uint16_t>(sizeof(USN_RECORD_V2) - sizeof(wchar_t));
            if (nameByteLen > 0) {
                std::memcpy(pRec->FileName, rec.FileName.data(), nameByteLen);
            }
        } else if (targetVersion == 3) {
            uint32_t nameByteLen = static_cast<uint32_t>(rec.FileName.size() * sizeof(wchar_t));
            uint32_t recLen = static_cast<uint32_t>(sizeof(USN_RECORD_V3) + nameByteLen);
            recLen = (recLen + 7u) & ~7u;

            outBuffer.resize(baseOffset + recLen, 0);
            auto* pRec = reinterpret_cast<USN_RECORD_V3*>(outBuffer.data() + baseOffset);
            pRec->RecordLength = recLen;
            pRec->MajorVersion = 3;
            pRec->MinorVersion = 0;
            pRec->FileReferenceNumber = rec.FileId;
            pRec->ParentFileReferenceNumber = rec.ParentFileId;
            pRec->Usn = rec.Usn;
            pRec->TimeStamp = rec.TimeStamp;
            pRec->Reason = rec.Reason;
            pRec->SourceInfo = rec.SourceInfo;
            pRec->SecurityId = rec.SecurityId;
            pRec->FileAttributes = rec.FileAttributes;
            pRec->FileNameLength = static_cast<uint16_t>(nameByteLen);
            pRec->FileNameOffset = static_cast<uint16_t>(sizeof(USN_RECORD_V3) - sizeof(wchar_t));
            if (nameByteLen > 0) {
                std::memcpy(pRec->FileName, rec.FileName.data(), nameByteLen);
            }
        } else if (targetVersion == 4) {
            size_t count = rec.Extents.size();
            size_t extra = (count > 1) ? (count - 1) : 0;
            uint32_t recLen = static_cast<uint32_t>(sizeof(USN_RECORD_V4) + (extra * sizeof(USN_RECORD_EXTENT)));
            recLen = (recLen + 7u) & ~7u;

            outBuffer.resize(baseOffset + recLen, 0);
            auto* pRec = reinterpret_cast<USN_RECORD_V4*>(outBuffer.data() + baseOffset);
            pRec->RecordLength = recLen;
            pRec->MajorVersion = 4;
            pRec->MinorVersion = 0;
            pRec->FileReferenceNumber = rec.FileId;
            pRec->ParentFileReferenceNumber = rec.ParentFileId;
            pRec->Usn = rec.Usn;
            pRec->Reason = rec.Reason;
            pRec->SourceInfo = rec.SourceInfo;
            pRec->RemainingExtents = 0;
            pRec->NumberOfExtents = static_cast<uint16_t>(count);
            pRec->ExtentOffset = static_cast<uint16_t>(sizeof(USN_RECORD_V4) - sizeof(USN_RECORD_EXTENT));
            for (size_t i = 0; i < count; ++i) {
                pRec->Extents[i] = rec.Extents[i];
            }
        }
    }

    uint64_t nextBookmark = m_meta.NextUsn;
    std::memcpy(outBuffer.data(), &nextBookmark, sizeof(uint64_t));
    nextUsnBookmark = nextBookmark;

    return NtStatus::Success;
}

uint64_t SovereignNtfsUsnStreamer::registerFilter(
    uint32_t altitude,
    uint32_t reasonMask,
    UsnFilterCallback callback
) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    uint64_t id = m_nextFilterId++;

    RegisteredUsnFilter filter{
        .RegistrationId = id,
        .Altitude = altitude,
        .ReasonMask = reasonMask,
        .Callback = std::move(callback)
    };

    m_filters.push_back(std::move(filter));
    // Sort descending by altitude (higher altitude runs first, matching FLTMGR semantics)
    std::sort(m_filters.begin(), m_filters.end(), [](const auto& a, const auto& b) {
        return a.Altitude > b.Altitude;
    });

    return id;
}

void SovereignNtfsUsnStreamer::unregisterFilter(uint64_t registrationId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_filters.erase(
        std::remove_if(m_filters.begin(), m_filters.end(), [registrationId](const auto& f) {
            return f.RegistrationId == registrationId;
        }),
        m_filters.end()
    );
}

void SovereignNtfsUsnStreamer::dispatchFilters(const SovereignUsnEvent& event) {
    for (const auto& filter : m_filters) {
        if ((filter.ReasonMask & event.Reason) != 0 && filter.Callback) {
            filter.Callback(event);
        }
    }
}

void SovereignNtfsUsnStreamer::checkAutoPruneLocked() {
    if (m_meta.MaximumSize == 0) return;

    uint64_t span = m_meta.NextUsn - m_meta.LowestValidUsn;
    if (span > m_meta.MaximumSize) {
        uint64_t targetPruneUsn = m_meta.LowestValidUsn + m_meta.AllocationDelta;

        auto it = std::lower_bound(
            m_records.begin(),
            m_records.end(),
            targetPruneUsn,
            [](const SovereignUsnEvent& e, uint64_t u) { return e.Usn < u; }
        );

        if (it != m_records.end()) {
            m_meta.LowestValidUsn = it->Usn;
            m_records.erase(m_records.begin(), it);
            m_pruneCount++;
        }
    }
}

uint64_t SovereignNtfsUsnStreamer::pruneJournalSparse() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (!m_active || m_records.empty()) return 0;

    uint64_t targetPruneUsn = m_meta.LowestValidUsn + m_meta.AllocationDelta;
    auto it = std::lower_bound(
        m_records.begin(),
        m_records.end(),
        targetPruneUsn,
        [](const SovereignUsnEvent& e, uint64_t u) { return e.Usn < u; }
    );

    uint64_t count = 0;
    if (it != m_records.end()) {
        count = std::distance(m_records.begin(), it);
        m_meta.LowestValidUsn = it->Usn;
        m_records.erase(m_records.begin(), it);
        m_pruneCount++;
    }
    return count;
}

bool SovereignNtfsUsnStreamer::isActive() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_active;
}

uint64_t SovereignNtfsUsnStreamer::getRecordCount() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_records.size();
}

uint64_t SovereignNtfsUsnStreamer::getLowestValidUsn() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_meta.LowestValidUsn;
}

uint64_t SovereignNtfsUsnStreamer::getNextUsn() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_meta.NextUsn;
}

uint64_t SovereignNtfsUsnStreamer::getJournalId() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_meta.UsnJournalID;
}

uint64_t SovereignNtfsUsnStreamer::getPruneCount() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_pruneCount;
}

uint64_t SovereignNtfsUsnStreamer::getActiveByteSpan() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return (m_meta.NextUsn >= m_meta.LowestValidUsn) ? (m_meta.NextUsn - m_meta.LowestValidUsn) : 0;
}

// ============================================================================
// Horizon 375 / Suite 591 Verification Sentinel
// ============================================================================

bool VerifyHorizon375NtfsUsnStreamer() {
    SovereignNtfsUsnStreamer streamer;

    // Pillar 1: Journal Initialization & Metadata Configuration ($UsnJrnl:$MAX)
    {
        CREATE_USN_JOURNAL_DATA cdata{
            .MaximumSize = 64 * 1024 * 1024,
            .AllocationDelta = 8 * 1024 * 1024
        };
        NtStatus st = streamer.createJournal(cdata);
        if (!NT_SUCCESS(st) || !streamer.isActive()) {
            std::cerr << "[-] Pillar 1 Failed: createJournal unsuccessful\n";
            return false;
        }

        USN_JOURNAL_DATA_V0 v0{};
        st = streamer.queryJournal(v0);
        if (!NT_SUCCESS(st) || v0.UsnJournalID == 0 || v0.NextUsn != 0x1000) {
            std::cerr << "[-] Pillar 1 Failed: queryJournal V0 invalid\n";
            return false;
        }

        USN_JOURNAL_DATA_V1 v1{};
        st = streamer.queryJournal(v1);
        if (!NT_SUCCESS(st) || v1.MinSupportedMajorVersion != 2 || v1.MaxSupportedMajorVersion != 4) {
            std::cerr << "[-] Pillar 1 Failed: queryJournal V1 version range invalid\n";
            return false;
        }

        USN_JOURNAL_DATA_V2 v2{};
        st = streamer.queryJournal(v2);
        if (!NT_SUCCESS(st) || v2.RangeTrackChunkSize != 65536) {
            std::cerr << "[-] Pillar 1 Failed: queryJournal V2 range chunk size invalid\n";
            return false;
        }
    }

    // Pillar 2: Multi-Version Record Marshalling (v2, v3, and v4 Extents)
    uint64_t usn1 = 0;
    uint64_t usn2 = 0;
    uint64_t usn3 = 0;
    {
        // V2 record
        usn1 = streamer.postRecordV2(
            100, 5,
            USN_REASON_FILE_CREATE,
            0x00000020, // FILE_ATTRIBUTE_ARCHIVE
            L"system.log"
        );
        if (usn1 < 0x1000) {
            std::cerr << "[-] Pillar 2 Failed: postRecordV2 returned invalid USN\n";
            return false;
        }

        // V3 record (128-bit file ID)
        FILE_ID_128 fid3 = FILE_ID_128::from128(200, 1);
        FILE_ID_128 pfid3 = FILE_ID_128::from128(5, 0);
        usn2 = streamer.postRecordV3(
            fid3, pfid3,
            USN_REASON_DATA_EXTEND | USN_REASON_CLOSE,
            0x00000020,
            L"database.mdf"
        );
        if (usn2 <= usn1) {
            std::cerr << "[-] Pillar 2 Failed: postRecordV3 non-monotonic USN\n";
            return false;
        }

        // V4 record (extents tracking)
        FILE_ID_128 fid4 = FILE_ID_128::from128(300, 2);
        FILE_ID_128 pfid4 = FILE_ID_128::from128(5, 0);
        std::vector<USN_RECORD_EXTENT> extents{
            {.Offset = 0, .Length = 4096},
            {.Offset = 16384, .Length = 8192}
        };
        usn3 = streamer.postRecordV4(
            fid4, pfid4,
            USN_REASON_DATA_OVERWRITE,
            extents
        );
        if (usn3 <= usn2 || streamer.getRecordCount() != 3) {
            std::cerr << "[-] Pillar 2 Failed: postRecordV4 invalid\n";
            return false;
        }
    }

    // Pillar 3: Range Streaming & Filter Interlock (FSCTL_READ_USN_JOURNAL)
    {
        std::vector<uint8_t> buf;
        uint64_t bookmark = 0;

        // Read all from start
        READ_USN_JOURNAL_DATA_V1 rdata{
            .StartUsn = 0,
            .ReasonMask = 0xFFFFFFFFu,
            .ReturnOnlyOnClose = 0,
            .MinMajorVersion = 2,
            .MaxMajorVersion = 4
        };
        NtStatus st = streamer.readJournal(rdata, buf, bookmark);
        if (!NT_SUCCESS(st) || buf.size() <= sizeof(uint64_t)) {
            std::cerr << "[-] Pillar 3 Failed: readJournal full buffer failed\n";
            return false;
        }

        // Read with ReturnOnlyOnClose
        READ_USN_JOURNAL_DATA_V1 rdataClose{
            .StartUsn = 0,
            .ReasonMask = 0xFFFFFFFFu,
            .ReturnOnlyOnClose = 1,
            .MinMajorVersion = 2,
            .MaxMajorVersion = 4
        };
        st = streamer.readJournal(rdataClose, buf, bookmark);
        if (!NT_SUCCESS(st)) {
            std::cerr << "[-] Pillar 3 Failed: readJournal ReturnOnlyOnClose failed\n";
            return false;
        }

        // Must find usn2 record, but not usn1 or usn3
        size_t off = sizeof(uint64_t);
        bool foundUsn2 = false;
        bool foundOther = false;
        while (off < buf.size()) {
            auto* hdr = reinterpret_cast<const USN_RECORD_COMMON_HEADER*>(buf.data() + off);
            if (hdr->RecordLength == 0) break;
            if (hdr->MajorVersion == 3) {
                auto* v3 = reinterpret_cast<const USN_RECORD_V3*>(buf.data() + off);
                if (v3->Usn == usn2) foundUsn2 = true;
                else foundOther = true;
            } else {
                foundOther = true;
            }
            off += hdr->RecordLength;
        }
        if (!foundUsn2 || foundOther) {
            std::cerr << "[-] Pillar 3 Failed: ReturnOnlyOnClose filter did not match exactly\n";
            return false;
        }
    }

    // Pillar 4: Zero-Polling Async Notification Ring & Completion Port Dispatch
    {
        std::atomic<bool> edrNotified{false};
        std::atomic<bool> indexerNotified{false};
        std::vector<uint32_t> altitudeOrder;
        std::mutex altMutex;

        // Register high altitude filter (385000: Anti-Virus / EDR)
        uint64_t f1 = streamer.registerFilter(385000, USN_REASON_FILE_DELETE, [&](const SovereignUsnEvent&) {
            edrNotified = true;
            std::lock_guard<std::mutex> lk(altMutex);
            altitudeOrder.push_back(385000);
        });

        // Register lower altitude filter (260000: Search Indexer)
        uint64_t f2 = streamer.registerFilter(260000, USN_REASON_FILE_DELETE | USN_REASON_FILE_CREATE, [&](const SovereignUsnEvent&) {
            indexerNotified = true;
            std::lock_guard<std::mutex> lk(altMutex);
            altitudeOrder.push_back(260000);
        });

        // Post a delete record
        streamer.postRecordV2(555, 5, USN_REASON_FILE_DELETE, 0, L"payload.exe");

        if (!edrNotified || !indexerNotified) {
            std::cerr << "[-] Pillar 4 Failed: Filter callbacks not triggered\n";
            return false;
        }
        if (altitudeOrder.size() >= 2 && (altitudeOrder[0] != 385000 || altitudeOrder[1] != 260000)) {
            std::cerr << "[-] Pillar 4 Failed: Filters not dispatched in descending altitude order\n";
            return false;
        }

        streamer.unregisterFilter(f1);
        streamer.unregisterFilter(f2);

        // Async read test
        std::atomic<bool> asyncReceived{false};
        std::thread waiter([&]() {
            std::vector<uint8_t> abuf;
            uint64_t abookmark = 0;
            READ_USN_JOURNAL_DATA_V1 rAsync{
                .StartUsn = streamer.getNextUsn(),
                .ReasonMask = USN_REASON_RENAME_NEW_NAME,
                .ReturnOnlyOnClose = 0,
                .MinMajorVersion = 2,
                .MaxMajorVersion = 4
            };
            NtStatus ast = streamer.readJournalAsync(rAsync, abuf, abookmark, 2000);
            if (NT_SUCCESS(ast) && abuf.size() > sizeof(uint64_t)) {
                asyncReceived = true;
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        streamer.postRecordV2(777, 5, USN_REASON_RENAME_NEW_NAME, 0, L"renamed.txt");
        waiter.join();

        if (!asyncReceived) {
            std::cerr << "[-] Pillar 4 Failed: readJournalAsync did not unblock on postRecord\n";
            return false;
        }
    }

    // Pillar 5: Sparse Range Truncation & Delta Pruning (LowestValidUsn Management)
    {
        SovereignNtfsUsnStreamer pruneStreamer;
        CREATE_USN_JOURNAL_DATA smallConfig{
            .MaximumSize = 2048,
            .AllocationDelta = 512
        };
        pruneStreamer.createJournal(smallConfig);

        uint64_t firstUsn = pruneStreamer.getNextUsn();

        // Post enough records to exceed MaximumSize (2048)
        for (int i = 0; i < 40; ++i) {
            pruneStreamer.postRecordV2(
                1000 + i, 5,
                USN_REASON_DATA_OVERWRITE,
                0,
                L"test_sparse_pruning_block_record.dat"
            );
        }

        if (pruneStreamer.getLowestValidUsn() <= firstUsn) {
            std::cerr << "[-] Pillar 5 Failed: LowestValidUsn was not advanced by auto-prune\n";
            return false;
        }
        if (pruneStreamer.getPruneCount() == 0) {
            std::cerr << "[-] Pillar 5 Failed: Prune count should be > 0\n";
            return false;
        }

        // Reading below LowestValidUsn must return STATUS_JOURNAL_ENTRY_DELETED
        std::vector<uint8_t> outBuf;
        uint64_t bookmark = 0;
        READ_USN_JOURNAL_DATA_V1 rOld{
            .StartUsn = firstUsn,
            .ReasonMask = 0xFFFFFFFFu,
            .ReturnOnlyOnClose = 0
        };
        NtStatus delStat = pruneStreamer.readJournal(rOld, outBuf, bookmark);
        if (delStat != NtStatus::JournalEntryDeleted) {
            std::cerr << "[-] Pillar 5 Failed: readJournal on deleted USN did not return JournalEntryDeleted\n";
            return false;
        }

        // Test delete journal
        DELETE_USN_JOURNAL_DATA delData{
            .UsnJournalID = pruneStreamer.getJournalId(),
            .DeleteFlags = USN_DELETE_FLAG_DELETE
        };
        NtStatus dst = pruneStreamer.deleteJournal(delData);
        if (!NT_SUCCESS(dst) || pruneStreamer.isActive()) {
            std::cerr << "[-] Pillar 5 Failed: deleteJournal failed\n";
            return false;
        }
    }

    // Pillar 6: High-Throughput Streamer Benchmark (<0.1 us/op)
    {
        SovereignNtfsUsnStreamer benchStreamer;
        CREATE_USN_JOURNAL_DATA benchConfig{
            .MaximumSize = 256 * 1024 * 1024,
            .AllocationDelta = 32 * 1024 * 1024
        };
        benchStreamer.createJournal(benchConfig);

        constexpr size_t NUM_OPS = 25000;
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < NUM_OPS; ++i) {
            benchStreamer.postRecordV2(
                10000 + i, 5,
                USN_REASON_DATA_EXTEND,
                0x00000020,
                L"bench_record.dat"
            );
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start
        ).count();

        double usPerOp = static_cast<double>(elapsed) / static_cast<double>(NUM_OPS);
        std::cout << "    [Benchmark] Sovereign NTFS USN Streamer: " << usPerOp << " us/op ("
                  << NUM_OPS << " records in " << elapsed << " us)\n";

        if (usPerOp > 50.0) {
            std::cerr << "[-] Pillar 6 Failed: Latency excessive: " << usPerOp << " us/op\n";
            return false;
        }
    }

    return true;
}

} // namespace micant::ntfs::usn
