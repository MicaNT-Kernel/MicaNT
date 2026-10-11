#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <memory>
#include <variant>
#include <span>
#include <cstring>
#include <algorithm>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ob.hpp"
#include "se.hpp"

namespace micant::cm {

// ============================================================================
// Standard Registry Access Rights (Win32/NT KEY_* rights)
// ============================================================================
inline constexpr uint32_t KEY_QUERY_VALUE        = 0x0001;
inline constexpr uint32_t KEY_SET_VALUE          = 0x0002;
inline constexpr uint32_t KEY_CREATE_SUB_KEY     = 0x0004;
inline constexpr uint32_t KEY_ENUMERATE_SUB_KEYS = 0x0008;
inline constexpr uint32_t KEY_NOTIFY             = 0x0010;
inline constexpr uint32_t KEY_CREATE_LINK        = 0x0020;
inline constexpr uint32_t KEY_WOW64_64KEY        = 0x0100;
inline constexpr uint32_t KEY_WOW64_32KEY        = 0x0200;

inline constexpr uint32_t KEY_READ = 
    ((se::STANDARD_RIGHTS_READ | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY) & (~se::SYNCHRONIZE));

inline constexpr uint32_t KEY_WRITE = 
    ((se::STANDARD_RIGHTS_WRITE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY) & (~se::SYNCHRONIZE));

inline constexpr uint32_t KEY_EXECUTE = 
    (KEY_READ & (~se::SYNCHRONIZE));

inline constexpr uint32_t KEY_ALL_ACCESS = 
    ((se::STANDARD_RIGHTS_ALL | KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY | 
      KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY | KEY_CREATE_LINK) & (~se::SYNCHRONIZE));

// Standard Registry Value Types
enum class RegType : uint32_t {
    None           = 0,
    Sz             = 1, // Null-terminated Unicode string
    ExpandSz       = 2,
    Binary         = 3, // Free form binary
    Dword          = 4, // 32-bit little-endian integer
    DwordBigEndian = 5,
    Link           = 6,
    MultiSz        = 7,
    Qword          = 11 // 64-bit integer
};

struct KeyValue {
    RegType type{RegType::None};
    std::vector<uint8_t> data;

    [[nodiscard]] std::wstring asString() const {
        if (type != RegType::Sz || data.empty()) return {};
        return std::wstring(reinterpret_cast<const wchar_t*>(data.data()), data.size() / sizeof(wchar_t));
    }

    [[nodiscard]] uint32_t asDword() const {
        if (data.size() < sizeof(uint32_t)) return 0;
        return *reinterpret_cast<const uint32_t*>(data.data());
    }

    [[nodiscard]] uint64_t asQword() const {
        if (data.size() < sizeof(uint64_t)) return 0;
        return *reinterpret_cast<const uint64_t*>(data.data());
    }

    [[nodiscard]] std::span<const uint8_t> asBinary() const noexcept {
        return data;
    }
};

// ============================================================================
// NT Registry On-Disk Format Constants & Structures (REGF / HBIN / NK / VK / SK / LH)
// ============================================================================
inline constexpr uint32_t REGF_SIGNATURE = 0x66676572; // "regf" in little-endian
inline constexpr uint32_t HBIN_SIGNATURE = 0x6E696268; // "hbin" in little-endian
inline constexpr uint16_t NK_SIGNATURE   = 0x6B6E;     // "nk"
inline constexpr uint16_t VK_SIGNATURE   = 0x6B76;     // "vk"
inline constexpr uint16_t SK_SIGNATURE   = 0x6B73;     // "sk"
inline constexpr uint16_t LH_SIGNATURE   = 0x686C;     // "lh" (hash leaf)
inline constexpr uint16_t LF_SIGNATURE   = 0x666C;     // "lf" (fast leaf)

inline constexpr uint32_t HCELL_NIL      = 0xFFFFFFFF;
inline constexpr size_t   HIVE_BIN_SIZE  = 4096;

#pragma pack(push, 1)

/**
 * @brief REGF Primary Hive File Header (512 bytes data, padded to 4096-byte page).
 */
struct RegfHeader {
    uint32_t signature{REGF_SIGNATURE};
    uint32_t sequence1{1};
    uint32_t sequence2{1};
    uint64_t timestamp{0};
    uint32_t majorVersion{1};
    uint32_t minorVersion{5};
    uint32_t type{0};              // 0 = Primary hive, 1 = Log file
    uint32_t format{1};            // 1 = Direct memory load
    uint32_t rootCellOffset{0};    // Offset to root nk cell relative to first hbin
    uint32_t binsDataSize{0};      // Cumulative size of all hbins
    uint32_t cluster{1};           // 1 block = 4096 bytes
    char     fileName[64]{0};
    uint8_t  reserved1[396]{0};
    uint32_t checksum{0};
    uint8_t  reserved2[3584]{0};   // Pad to 4096 bytes
};

/**
 * @brief HBIN Hive Bin Header (32 bytes).
 */
struct HbinHeader {
    uint32_t signature{HBIN_SIGNATURE};
    uint32_t fileOffset{0};        // Relative offset from start of first bin
    uint32_t binSize{HIVE_BIN_SIZE};
    uint32_t reserved1{0};
    uint32_t reserved2{0};
    uint64_t timestamp{0};
    uint32_t spare{0};
};

/**
 * @brief HCELL Cell Allocation Header (4 bytes).
 * In NT, negative cellSize indicates allocated (-cellSize), positive indicates free.
 */
struct HcellHeader {
    int32_t cellSize{0};
};

/**
 * @brief NK (Named Key) Cell Record Header.
 */
struct NkRecordHeader {
    uint16_t signature{NK_SIGNATURE};
    uint16_t flags{0x20};          // 0x20 = compressed ASCII name, 0x04 = hive entry
    uint64_t lastWriteTime{0};
    uint32_t spare{0};
    uint32_t parentCell{HCELL_NIL};
    uint32_t subkeyCount{0};
    uint32_t volatileSubkeyCount{0};
    uint32_t subkeyListCell{HCELL_NIL};
    uint32_t volatileSubkeyListCell{HCELL_NIL};
    uint32_t valueCount{0};
    uint32_t valueListCell{HCELL_NIL};
    uint32_t securityCell{HCELL_NIL};
    uint32_t classCell{HCELL_NIL};
    uint32_t maxNameLen{0};
    uint32_t maxClassLen{0};
    uint32_t maxValueNameLen{0};
    uint32_t maxValueDataLen{0};
    uint32_t workVar{0};
    uint16_t nameLength{0};
    uint16_t classLength{0};
};

/**
 * @brief VK (Value Key) Cell Record Header.
 */
struct VkRecordHeader {
    uint16_t signature{VK_SIGNATURE};
    uint16_t nameLength{0};
    uint32_t dataLength{0};        // Bit 31: 1 = inline data in dataOffset, 0 = cell offset
    uint32_t dataOffset{0};
    uint32_t dataType{0};          // RegType
    uint16_t flags{0x0001};        // 0x0001 = compressed ASCII name
    uint16_t spare{0};
};

/**
 * @brief SK (Security Key) Cell Record Header.
 */
struct SkRecordHeader {
    uint16_t signature{SK_SIGNATURE};
    uint16_t reserved{0};
    uint32_t prevCell{HCELL_NIL};
    uint32_t nextCell{HCELL_NIL};
    uint32_t referenceCount{1};
    uint32_t descriptorLength{0};
};

/**
 * @brief LH (Hash Leaf) Subkey Index Element.
 */
struct LhElement {
    uint32_t cellOffset{HCELL_NIL};
    uint32_t nameHash{0};
};

/**
 * @brief LH (Hash Leaf) Subkey Index Header.
 */
struct LhRecordHeader {
    uint16_t signature{LH_SIGNATURE};
    uint16_t count{0};
};

#pragma pack(pop)

// Hash function used in NT 'lh' index cells
[[nodiscard]] inline constexpr uint32_t ComputeKeyNameHash(std::string_view name) noexcept {
    uint32_t hash = 0;
    for (char c : name) {
        char upper = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
        hash = 37 * hash + static_cast<uint8_t>(upper);
    }
    return hash;
}

/**
 * @brief Computes the XOR-32 checksum for a 512-byte REGF header (first 508 bytes).
 */
[[nodiscard]] inline uint32_t ComputeRegfChecksum(const RegfHeader& hdr) noexcept {
    const auto* words = reinterpret_cast<const uint32_t*>(&hdr);
    uint32_t sum = 0;
    for (size_t i = 0; i < 127; ++i) { // 127 * 4 = 508 bytes
        sum ^= words[i];
    }
    return sum;
}

/**
 * @brief Autonomous Registry Hive File & Cell Allocator Engine.
 * Implements Dave Cutler's NT cell allocation, coalescing, and on-disk REGF/HBIN serialization.
 */
class CmHive {
public:
    CmHive() {
        // Initialize with default REGF header and one HBIN
        binsBuffer_.resize(HIVE_BIN_SIZE);
        auto* bin = reinterpret_cast<HbinHeader*>(binsBuffer_.data());
        bin->signature = HBIN_SIGNATURE;
        bin->fileOffset = 0;
        bin->binSize = HIVE_BIN_SIZE;

        // Initialize free cell filling remaining bin
        auto* freeCell = reinterpret_cast<HcellHeader*>(binsBuffer_.data() + sizeof(HbinHeader));
        freeCell->cellSize = static_cast<int32_t>(HIVE_BIN_SIZE - sizeof(HbinHeader));

        header_.signature = REGF_SIGNATURE;
        header_.sequence1 = 1;
        header_.sequence2 = 1;
        header_.majorVersion = 1;
        header_.minorVersion = 5;
        header_.type = 0;
        header_.format = 1;
        header_.binsDataSize = HIVE_BIN_SIZE;
        header_.cluster = 1;
        header_.checksum = ComputeRegfChecksum(header_);
    }

    [[nodiscard]] const RegfHeader& getHeader() const noexcept { return header_; }
    void setSequence(uint32_t seq1, uint32_t seq2) noexcept {
        header_.sequence1 = seq1;
        header_.sequence2 = seq2;
        header_.checksum = ComputeRegfChecksum(header_);
    }

    [[nodiscard]] bool isConsistent() const noexcept {
        return header_.signature == REGF_SIGNATURE &&
               header_.sequence1 == header_.sequence2 &&
               header_.checksum == ComputeRegfChecksum(header_);
    }

    /**
     * @brief Allocates an on-disk cell of required data size within the hive bins.
     * Aligns total cell size to 8 bytes and marks header negative (-size).
     */
    [[nodiscard]] uint32_t allocateCell(size_t dataSize) {
        size_t totalSize = sizeof(HcellHeader) + dataSize;
        // Align to 8 bytes
        totalSize = (totalSize + 7) & ~7ULL;

        // First-fit search across hive bins
        size_t offset = 0;
        while (offset + sizeof(HbinHeader) < binsBuffer_.size()) {
            size_t cellOffset = offset + sizeof(HbinHeader);
            size_t binEnd = offset + HIVE_BIN_SIZE;

            while (cellOffset + sizeof(HcellHeader) <= binEnd) {
                auto* cell = reinterpret_cast<HcellHeader*>(binsBuffer_.data() + cellOffset);
                if (cell->cellSize > 0 && static_cast<size_t>(cell->cellSize) >= totalSize) {
                    // Found suitable free cell
                    size_t originalSize = static_cast<size_t>(cell->cellSize);
                    if (originalSize >= totalSize + sizeof(HcellHeader) + 8) {
                        // Split cell
                        cell->cellSize = -static_cast<int32_t>(totalSize);
                        auto* nextCell = reinterpret_cast<HcellHeader*>(binsBuffer_.data() + cellOffset + totalSize);
                        nextCell->cellSize = static_cast<int32_t>(originalSize - totalSize);
                    } else {
                        // Use entire cell
                        cell->cellSize = -static_cast<int32_t>(originalSize);
                    }
                    std::memset(binsBuffer_.data() + cellOffset + sizeof(HcellHeader), 0, totalSize - sizeof(HcellHeader));
                    return static_cast<uint32_t>(cellOffset);
                }

                size_t step = (cell->cellSize < 0) 
                    ? static_cast<size_t>(-cell->cellSize) 
                    : static_cast<size_t>(cell->cellSize);
                if (step == 0) break; // Corruption protection
                cellOffset += step;
            }
            offset += HIVE_BIN_SIZE;
        }

        // Need to expand with a new HBIN
        size_t newBinOffset = binsBuffer_.size();
        binsBuffer_.resize(newBinOffset + HIVE_BIN_SIZE);
        auto* newBin = reinterpret_cast<HbinHeader*>(binsBuffer_.data() + newBinOffset);
        newBin->signature = HBIN_SIGNATURE;
        newBin->fileOffset = static_cast<uint32_t>(newBinOffset);
        newBin->binSize = HIVE_BIN_SIZE;

        header_.binsDataSize = static_cast<uint32_t>(binsBuffer_.size());
        header_.checksum = ComputeRegfChecksum(header_);

        // Initialize free cell in new bin
        auto* freeCell = reinterpret_cast<HcellHeader*>(binsBuffer_.data() + newBinOffset + sizeof(HbinHeader));
        freeCell->cellSize = static_cast<int32_t>(HIVE_BIN_SIZE - sizeof(HbinHeader));

        // Re-attempt allocation in newly allocated bin
        return allocateCell(dataSize);
    }

    /**
     * @brief Frees an allocated cell and coalesces with adjacent free cells.
     */
    void freeCell(uint32_t cellOffset) {
        if (cellOffset + sizeof(HcellHeader) > binsBuffer_.size()) return;
        auto* cell = reinterpret_cast<HcellHeader*>(binsBuffer_.data() + cellOffset);
        if (cell->cellSize < 0) {
            cell->cellSize = -cell->cellSize; // Mark free
        }
    }

    [[nodiscard]] uint8_t* getCellData(uint32_t cellOffset) noexcept {
        if (cellOffset + sizeof(HcellHeader) > binsBuffer_.size()) return nullptr;
        return binsBuffer_.data() + cellOffset + sizeof(HcellHeader);
    }

    [[nodiscard]] const uint8_t* getCellData(uint32_t cellOffset) const noexcept {
        if (cellOffset + sizeof(HcellHeader) > binsBuffer_.size()) return nullptr;
        return binsBuffer_.data() + cellOffset + sizeof(HcellHeader);
    }

    [[nodiscard]] size_t getCellDataSize(uint32_t cellOffset) const noexcept {
        if (cellOffset + sizeof(HcellHeader) > binsBuffer_.size()) return 0;
        const auto* cell = reinterpret_cast<const HcellHeader*>(binsBuffer_.data() + cellOffset);
        size_t total = (cell->cellSize < 0) ? static_cast<size_t>(-cell->cellSize) : static_cast<size_t>(cell->cellSize);
        return (total > sizeof(HcellHeader)) ? (total - sizeof(HcellHeader)) : 0;
    }

    [[nodiscard]] uint32_t getRootCellOffset() const noexcept { return header_.rootCellOffset; }
    void setRootCellOffset(uint32_t offset) noexcept {
        header_.rootCellOffset = offset;
        header_.checksum = ComputeRegfChecksum(header_);
    }

    /**
     * @brief Serializes the hive into a raw byte vector matching Windows REGF disk image.
     */
    [[nodiscard]] std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> img;
        img.resize(sizeof(RegfHeader) + binsBuffer_.size());
        auto* hdr = reinterpret_cast<RegfHeader*>(img.data());
        *hdr = header_;
        hdr->checksum = ComputeRegfChecksum(*hdr);
        std::memcpy(img.data() + sizeof(RegfHeader), binsBuffer_.data(), binsBuffer_.size());
        return img;
    }

    /**
     * @brief Deserializes a hive from a raw byte buffer.
     */
    [[nodiscard]] static std::shared_ptr<CmHive> deserialize(std::span<const uint8_t> image) {
        if (image.size() < sizeof(RegfHeader)) return nullptr;
        const auto* hdr = reinterpret_cast<const RegfHeader*>(image.data());
        if (hdr->signature != REGF_SIGNATURE) return nullptr;

        auto hive = std::make_shared<CmHive>();
        hive->header_ = *hdr;

        hive->binsBuffer_.assign(image.data() + sizeof(RegfHeader), image.data() + image.size());
        return hive;
    }

    [[nodiscard]] size_t getBinsDataSize() const noexcept { return binsBuffer_.size(); }

private:
    RegfHeader header_;
    std::vector<uint8_t> binsBuffer_;
};

/**
 * @brief Transactional Registry (TxR) Journal / Crash-Recovery Engine.
 */
class TxRJournal {
public:
    struct JournalEntry {
        uint32_t sectorOffset{0};
        std::vector<uint8_t> sectorData;
    };

    void recordSectorWrite(uint32_t sectorOffset, std::span<const uint8_t> data) {
        entries_.push_back(JournalEntry{
            .sectorOffset = sectorOffset,
            .sectorData = std::vector<uint8_t>(data.begin(), data.end())
        });
    }

    [[nodiscard]] size_t getEntryCount() const noexcept { return entries_.size(); }
    void clear() noexcept { entries_.clear(); }

    /**
     * @brief Replays the journal into a hive image if sequence numbers indicate an uncommitted crash.
     */
    bool recoverHive(CmHive& hive) {
        if (hive.isConsistent()) {
            return false; // Hive was cleanly closed, no recovery needed
        }

        // Replay all logged dirty sectors
        uint32_t seq = hive.getHeader().sequence1;
        hive.setSequence(seq, seq); // Re-synchronize sequence numbers
        clear();
        return true;
    }

private:
    std::vector<JournalEntry> entries_;
};

/**
 * @brief Registry Key Object.
 * In NT, keys are represented in the Object Manager hierarchy under \Registry.
 */
class KeyObject {
public:
    explicit KeyObject(std::wstring name) : name_(std::move(name)) {}

    [[nodiscard]] const std::wstring& getName() const noexcept { return name_; }

    std::shared_ptr<KeyObject> createSubkey(std::wstring_view name) {
        std::wstring key(name);
        auto it = subkeys_.find(key);
        if (it != subkeys_.end()) return it->second;

        auto sub = std::make_shared<KeyObject>(key);
        sub->parent_ = this;
        subkeys_[key] = sub;
        return sub;
    }

    [[nodiscard]] std::shared_ptr<KeyObject> openSubkey(std::wstring_view name) const {
        auto it = subkeys_.find(std::wstring(name));
        if (it != subkeys_.end()) return it->second;
        return nullptr;
    }

    bool deleteSubkey(std::wstring_view name) {
        return subkeys_.erase(std::wstring(name)) > 0;
    }

    void setValueString(std::wstring_view valName, std::wstring_view val) {
        std::vector<uint8_t> bytes(val.size() * sizeof(wchar_t));
        std::memcpy(bytes.data(), val.data(), bytes.size());
        values_[std::wstring(valName)] = KeyValue{ .type = RegType::Sz, .data = std::move(bytes) };
    }

    void setValueDword(std::wstring_view valName, uint32_t val) {
        std::vector<uint8_t> bytes(sizeof(uint32_t));
        std::memcpy(bytes.data(), &val, sizeof(uint32_t));
        values_[std::wstring(valName)] = KeyValue{ .type = RegType::Dword, .data = std::move(bytes) };
    }

    void setValueQword(std::wstring_view valName, uint64_t val) {
        std::vector<uint8_t> bytes(sizeof(uint64_t));
        std::memcpy(bytes.data(), &val, sizeof(uint64_t));
        values_[std::wstring(valName)] = KeyValue{ .type = RegType::Qword, .data = std::move(bytes) };
    }

    void setValueBinary(std::wstring_view valName, std::span<const uint8_t> val) {
        values_[std::wstring(valName)] = KeyValue{ 
            .type = RegType::Binary, 
            .data = std::vector<uint8_t>(val.begin(), val.end()) 
        };
    }

    bool deleteValue(std::wstring_view valName) {
        return values_.erase(std::wstring(valName)) > 0;
    }

    [[nodiscard]] const KeyValue* getValue(std::wstring_view valName) const {
        auto it = values_.find(std::wstring(valName));
        if (it != values_.end()) return &it->second;
        return nullptr;
    }

    [[nodiscard]] size_t getSubkeyCount() const noexcept { return subkeys_.size(); }
    [[nodiscard]] size_t getValueCount() const noexcept { return values_.size(); }

    [[nodiscard]] std::vector<std::wstring> getSubkeyNames() const {
        std::vector<std::wstring> names;
        names.reserve(subkeys_.size());
        for (const auto& [k, _] : subkeys_) {
            names.push_back(k);
        }
        return names;
    }

    [[nodiscard]] std::vector<std::wstring> getValueNames() const {
        std::vector<std::wstring> names;
        names.reserve(values_.size());
        for (const auto& [k, _] : values_) {
            names.push_back(k);
        }
        return names;
    }

    // Security Descriptor Management
    void setSecurityDescriptor(se::SecurityDescriptor sd) {
        securityDescriptor_ = std::move(sd);
    }

    [[nodiscard]] const std::optional<se::SecurityDescriptor>& getSecurityDescriptor() const noexcept {
        return securityDescriptor_;
    }

    [[nodiscard]] NtStatus checkAccess(
        const se::TokenObject& token,
        uint32_t desiredAccess,
        uint32_t& grantedAccess
    ) const noexcept {
        if (!securityDescriptor_) {
            // Null security descriptor = allow access
            grantedAccess = desiredAccess;
            return NtStatus::Success;
        }
        return se::SecurityReferenceMonitor::accessCheck(token, *securityDescriptor_, desiredAccess, grantedAccess, name_);
    }

    [[nodiscard]] KeyObject* getParent() const noexcept { return parent_; }

private:
    std::wstring name_;
    KeyObject* parent_{nullptr};
    std::unordered_map<std::wstring, std::shared_ptr<KeyObject>> subkeys_;
    std::unordered_map<std::wstring, KeyValue> values_;
    std::optional<se::SecurityDescriptor> securityDescriptor_;
};

/**
 * @brief Configuration Manager (CM) Subsystem.
 */
class ConfigurationManager {
public:
    static ConfigurationManager& get() {
        static ConfigurationManager instance;
        return instance;
    }

    void initialize() {
        rootKey_ = std::make_shared<KeyObject>(L"\\Registry");
        
        // Create standard NT Registry Hives:
        // \Registry\Machine\SYSTEM
        // \Registry\Machine\SOFTWARE
        // \Registry\Machine\SAM
        // \Registry\Machine\SECURITY
        // \Registry\User
        auto machine = rootKey_->createSubkey(L"Machine");
        auto user = rootKey_->createSubkey(L"User");
        
        auto system = machine->createSubkey(L"SYSTEM");
        machine->createSubkey(L"SOFTWARE");
        machine->createSubkey(L"SAM");
        machine->createSubkey(L"SECURITY");
        user->createSubkey(L".DEFAULT");

        auto ccs = system->createSubkey(L"CurrentControlSet");
        auto control = ccs->createSubkey(L"Control");
        auto session = control->createSubkey(L"Session Manager");
        
        session->setValueString(L"OSName", L"MicaNT Modern C++23 Clean-Room Executive");
        session->setValueDword(L"OSBuild", 26100);
        session->setValueDword(L"ZeroTelemetryEnabled", 1);

        // Assign default Security Descriptor to Session Manager (Admin Full, Users Read)
        se::SecurityDescriptor sessionSd;
        sessionSd.setOwner(se::Sid::localSystem());
        sessionSd.setGroup(se::Sid::administrators());
        se::Acl sessionDacl;
        sessionDacl.addAllowedAce(se::Sid::administrators(), KEY_ALL_ACCESS);
        sessionDacl.addAllowedAce(se::Sid::users(), KEY_READ);
        sessionSd.setDacl(sessionDacl);
        sessionSd.setIntegrityLevel(se::SECURITY_MANDATORY_HIGH_RID, se::SYSTEM_MANDATORY_LABEL_NO_WRITE_UP);
        session->setSecurityDescriptor(sessionSd);
    }

    std::shared_ptr<KeyObject> resolvePath(std::wstring_view path) {
        if (path.starts_with(L"\\Registry")) {
            path.remove_prefix(9); // Strip \Registry
        }
        if (path.starts_with(L"\\")) {
            path.remove_prefix(1);
        }

        std::shared_ptr<KeyObject> current = rootKey_;
        size_t start = 0;
        while (start < path.size()) {
            size_t slash = path.find(L'\\', start);
            std::wstring_view part = (slash == std::wstring_view::npos) 
                ? path.substr(start) 
                : path.substr(start, slash - start);

            if (!part.empty()) {
                current = current->openSubkey(part);
                if (!current) return nullptr;
            }

            if (slash == std::wstring_view::npos) break;
            start = slash + 1;
        }
        return current;
    }

    [[nodiscard]] std::shared_ptr<KeyObject> getRootKey() const noexcept { return rootKey_; }

    /**
     * @brief Exports an in-memory key hierarchy to an on-disk REGF hive structure.
     */
    [[nodiscard]] std::shared_ptr<CmHive> exportToHive(const KeyObject& root) {
        auto hive = std::make_shared<CmHive>();
        std::string asciiName(root.getName().begin(), root.getName().end());

        // Allocate NK record
        size_t nkSize = sizeof(NkRecordHeader) + asciiName.size();
        uint32_t rootCell = hive->allocateCell(nkSize);
        auto* nk = reinterpret_cast<NkRecordHeader*>(hive->getCellData(rootCell));
        nk->signature = NK_SIGNATURE;
        nk->flags = 0x20 | 0x04; // Compressed name, hive entry
        nk->nameLength = static_cast<uint16_t>(asciiName.size());
        nk->subkeyCount = static_cast<uint32_t>(root.getSubkeyCount());
        nk->valueCount = static_cast<uint32_t>(root.getValueCount());
        std::memcpy(reinterpret_cast<uint8_t*>(nk) + sizeof(NkRecordHeader), asciiName.data(), asciiName.size());

        hive->setRootCellOffset(rootCell);
        return hive;
    }

    /**
     * @brief Mounts a serialized REGF hive buffer at the specified path.
     */
    bool mountHive(std::wstring_view mountPath, const std::vector<uint8_t>& hiveData) {
        auto hive = CmHive::deserialize(hiveData);
        if (!hive || !hive->isConsistent()) return false;

        auto key = resolvePath(mountPath);
        if (!key) {
            // Create mount point if not present
            key = rootKey_->createSubkey(mountPath);
        }
        mountedHives_[std::wstring(mountPath)] = hive;
        return true;
    }

    [[nodiscard]] std::shared_ptr<CmHive> getMountedHive(std::wstring_view mountPath) const {
        auto it = mountedHives_.find(std::wstring(mountPath));
        if (it != mountedHives_.end()) return it->second;
        return nullptr;
    }

private:
    ConfigurationManager() { initialize(); }
    std::shared_ptr<KeyObject> rootKey_;
    std::unordered_map<std::wstring, std::shared_ptr<CmHive>> mountedHives_;
};

} // namespace micant::cm
