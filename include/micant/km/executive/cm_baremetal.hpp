// ============================================================================
// MicaNT: Sovereign Freestanding Ring-0 NT Configuration Manager & Binary
//         Registry Hive Engine (cm_baremetal.hpp)
//
// Description:
//   Pure -ffreestanding -nostdlib ISO C++23 Configuration Manager compiled
//   directly into ntoskrnl.exe.
//
//   Implements authentic Windows NT binary Registry Hive (`regf` v1.3 / `hbin` /
//   `nk` / `lf` / `vk`) loading, querying, creation, mutation, deletion,
//   enumeration, binary serialization (with sequence bump & XOR-32 header
//   checksum at 0x1FC), and live write-back persistence directly onto
//   C:\Windows\System32\config\SYSTEM and SOFTWARE over NTFS 3.1 AHCI DMA!
//
// Clean-Room ISO C++23. Zero mocks, zero third-party dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "micant/km/drivers/fs/ntfs_baremetal.hpp"

namespace micant::km::cm_baremetal {

inline constexpr uint64_t HKEY_CLASSES_ROOT_VAL   = 0x80000000ULL;
inline constexpr uint64_t HKEY_CURRENT_USER_VAL   = 0x80000001ULL;
inline constexpr uint64_t HKEY_LOCAL_MACHINE_VAL  = 0x80000002ULL;
inline constexpr uint64_t HKEY_USERS_VAL          = 0x80000003ULL;

inline constexpr uint64_t REG_KEY_HANDLE_TAG      = 0x50000000ULL;

inline constexpr uint32_t CM_REG_NONE             = 0;
inline constexpr uint32_t CM_REG_SZ               = 1;
inline constexpr uint32_t CM_REG_EXPAND_SZ        = 2;
inline constexpr uint32_t CM_REG_BINARY           = 3;
inline constexpr uint32_t CM_REG_DWORD            = 4;
inline constexpr uint32_t CM_REG_QWORD            = 11;

inline constexpr uint32_t CM_REG_CREATED_NEW_KEY     = 1;
inline constexpr uint32_t CM_REG_OPENED_EXISTING_KEY = 2;

inline constexpr size_t MAX_HIVES         = 2;   // 0 = SYSTEM, 1 = SOFTWARE
inline constexpr size_t MAX_KEYS_PER_HIVE = 64;
inline constexpr size_t MAX_VALS_PER_KEY  = 16;
inline constexpr size_t MAX_KEY_HANDLES   = 64;
inline constexpr size_t MAX_HIVE_BYTES    = 32768; // 32 KB max binary hive image

struct CmValue {
    bool     inUse;
    char     name[48];
    uint32_t type;
    uint32_t dataSize;
    uint8_t  data[160];
};

struct CmKeyNode {
    bool     inUse;
    char     name[48];
    int32_t  parentIdx;
    uint32_t valueCount;
    CmValue  values[MAX_VALS_PER_KEY];
};

struct CmHive {
    bool      loaded;
    char      hiveName[16];       // "SYSTEM" or "SOFTWARE"
    char      ntfsPath[64];       // "\\Windows\\System32\\config\\SYSTEM"
    uint32_t  sequence;
    uint32_t  headerChecksum;
    uint32_t  rawSizeBytes;
    uint32_t  flushCount;
    uint32_t  keyCount;
    uint32_t  totalValuesLoaded;
    CmKeyNode keys[MAX_KEYS_PER_HIVE];
};

struct CmKeyHandle {
    bool    inUse;
    uint8_t hiveIdx;
    int32_t keyIdx; // -1 = virtual HKLM root
};

// Scratch 32 KB hive read/write buffer in kernel BSS
alignas(4096) static uint8_t s_hiveIoBuf[MAX_HIVE_BYTES];

class BareMetalConfigManager {
private:
    CmHive      m_hives[MAX_HIVES];
    CmKeyHandle m_handles[MAX_KEY_HANDLES];
    bool        m_initialized;

    static inline char toLower(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    }

    static bool strCaseEq(const char* a, const char* b) noexcept {
        size_t i = 0;
        while (a[i] && b[i]) {
            if (toLower(a[i]) != toLower(b[i])) return false;
            ++i;
        }
        return a[i] == '\0' && b[i] == '\0';
    }

    static void copyTrunc(char* dst, size_t cap, const char* src) noexcept {
        if (!dst || cap == 0) return;
        size_t i = 0;
        if (src) {
            while (src[i] && i + 1 < cap) { dst[i] = src[i]; ++i; }
        }
        dst[i] = '\0';
    }

    static uint16_t rd16(const uint8_t* p) noexcept {
        uint16_t v = 0;
        std::memcpy(&v, p, 2);
        return v;
    }

    static uint32_t rd32(const uint8_t* p) noexcept {
        uint32_t v = 0;
        std::memcpy(&v, p, 4);
        return v;
    }

    static int32_t rdi32(const uint8_t* p) noexcept {
        int32_t v = 0;
        std::memcpy(&v, p, 4);
        return v;
    }

    static void wr16(uint8_t* p, uint16_t v) noexcept { std::memcpy(p, &v, 2); }
    static void wr32(uint8_t* p, uint32_t v) noexcept { std::memcpy(p, &v, 4); }
    static void wri32(uint8_t* p, int32_t v) noexcept { std::memcpy(p, &v, 4); }
    static void wr64(uint8_t* p, uint64_t v) noexcept { std::memcpy(p, &v, 8); }

    static uint32_t computeRegfChecksum(const uint8_t* base4k) noexcept {
        uint32_t crc = 0;
        for (size_t off = 0; off < 0x1FC; off += 4) {
            crc ^= rd32(base4k + off);
        }
        if (crc == 0) crc = 1;
        else if (crc == 0xFFFFFFFFu) crc = 0xFFFFFFFEu;
        return crc;
    }

    int32_t findChildKey(const CmHive& hive, int32_t parentIdx, const char* name) const noexcept {
        for (uint32_t i = 0; i < hive.keyCount; ++i) {
            if (hive.keys[i].inUse && hive.keys[i].parentIdx == parentIdx &&
                strCaseEq(hive.keys[i].name, name)) {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }

    int32_t allocKeyNode(CmHive& hive, int32_t parentIdx, const char* name) noexcept {
        if (hive.keyCount >= MAX_KEYS_PER_HIVE) return -1;
        int32_t idx = static_cast<int32_t>(hive.keyCount++);
        auto& k = hive.keys[idx];
        std::memset(&k, 0, sizeof(k));
        k.inUse = true;
        k.parentIdx = parentIdx;
        copyTrunc(k.name, sizeof(k.name), name);
        return idx;
    }

    // Recursively parse binary NT `nk` -> `lf`/`lh`/`li` -> `vk` cells from hbin area
    void parseNkCell(CmHive& hive, const uint8_t* hbin, size_t hbinSize, uint32_t cellOff, int32_t parentIdx) noexcept {
        if (cellOff == 0xFFFFFFFFu || static_cast<size_t>(cellOff) + 80 > hbinSize) return;
        const uint8_t* cell = hbin + cellOff;
        int32_t csize = rdi32(cell);
        if (csize >= 0) return; // Not allocated
        size_t absSize = static_cast<size_t>(-csize);
        if (static_cast<size_t>(cellOff) + absSize > hbinSize || absSize < 80) return;

        if (cell[4] != 'n' || cell[5] != 'k') return;
        uint16_t flags = rd16(cell + 6);
        uint32_t subkeyCount = rd32(cell + 24);
        uint32_t subkeyListOff = rd32(cell + 32);
        uint32_t valueCount = rd32(cell + 40);
        uint32_t valueListOff = rd32(cell + 44);
        uint16_t nameLen = rd16(cell + 76);

        char kname[48]{};
        if (static_cast<size_t>(80) + nameLen <= absSize) {
            if ((flags & 0x0020u) != 0) { // KEY_COMP_NAME (ASCII)
                size_t n = (nameLen < sizeof(kname) - 1) ? nameLen : (sizeof(kname) - 1);
                for (size_t i = 0; i < n; ++i) kname[i] = static_cast<char>(cell[80 + i]);
                kname[n] = '\0';
            } else { // UTF-16LE
                size_t chars = nameLen / 2;
                size_t n = (chars < sizeof(kname) - 1) ? chars : (sizeof(kname) - 1);
                for (size_t i = 0; i < n; ++i) kname[i] = static_cast<char>(cell[80 + i * 2]);
                kname[n] = '\0';
            }
        }

        int32_t myIdx = allocKeyNode(hive, parentIdx, kname);
        if (myIdx < 0) return;
        auto& node = hive.keys[myIdx];

        // Parse Value List -> `vk` value cells
        if (valueCount > 0 && valueListOff != 0xFFFFFFFFu &&
            static_cast<size_t>(valueListOff) + 4 + valueCount * 4 <= hbinSize) {
            const uint8_t* vl = hbin + valueListOff;
            for (uint32_t vi = 0; vi < valueCount && node.valueCount < MAX_VALS_PER_KEY; ++vi) {
                uint32_t vkOff = rd32(vl + 4 + vi * 4);
                if (vkOff == 0xFFFFFFFFu || static_cast<size_t>(vkOff) + 24 > hbinSize) continue;
                const uint8_t* vk = hbin + vkOff;
                int32_t vkSize = rdi32(vk);
                if (vkSize >= 0) continue;
                size_t vkAbs = static_cast<size_t>(-vkSize);
                if (static_cast<size_t>(vkOff) + vkAbs > hbinSize || vkAbs < 24) continue;
                if (vk[4] != 'v' || vk[5] != 'k') continue;

                uint16_t vNameLen = rd16(vk + 6);
                uint32_t rawDataLen = rd32(vk + 8);
                uint32_t dataOff = rd32(vk + 12);
                uint32_t dataType = rd32(vk + 16);
                uint16_t vFlags = rd16(vk + 20);

                auto& val = node.values[node.valueCount++];
                std::memset(&val, 0, sizeof(val));
                val.inUse = true;
                val.type = dataType;

                if (vNameLen > 0 && static_cast<size_t>(24) + vNameLen <= vkAbs) {
                    if ((vFlags & 0x0001u) != 0) {
                        size_t n = (vNameLen < sizeof(val.name) - 1) ? vNameLen : (sizeof(val.name) - 1);
                        for (size_t k = 0; k < n; ++k) val.name[k] = static_cast<char>(vk[24 + k]);
                        val.name[n] = '\0';
                    } else {
                        size_t chars = vNameLen / 2;
                        size_t n = (chars < sizeof(val.name) - 1) ? chars : (sizeof(val.name) - 1);
                        for (size_t k = 0; k < n; ++k) val.name[k] = static_cast<char>(vk[24 + k * 2]);
                        val.name[n] = '\0';
                    }
                }

                if ((rawDataLen & 0x80000000u) != 0) {
                    uint32_t inlineLen = rawDataLen & 0x7FFFFFFFu;
                    if (inlineLen > 4) inlineLen = 4;
                    val.dataSize = inlineLen;
                    std::memcpy(val.data, &dataOff, inlineLen);
                } else if (rawDataLen > 0 && dataOff != 0xFFFFFFFFu &&
                           static_cast<size_t>(dataOff) + 4 <= hbinSize) {
                    const uint8_t* dc = hbin + dataOff;
                    int32_t dSize = rdi32(dc);
                    if (dSize < 0) {
                        size_t dAbs = static_cast<size_t>(-dSize);
                        size_t avail = (dAbs > 4 && static_cast<size_t>(dataOff) + dAbs <= hbinSize) ? (dAbs - 4) : 0;
                        size_t copyLen = rawDataLen;
                        if (copyLen > avail) copyLen = avail;
                        if (copyLen > sizeof(val.data)) copyLen = sizeof(val.data);
                        std::memcpy(val.data, dc + 4, copyLen);
                        val.dataSize = static_cast<uint32_t>(copyLen);
                    }
                }
                hive.totalValuesLoaded++;
            }
        }

        // Parse Subkey List (`lf`, `lh`, `li`) -> recursive `nk`
        if (subkeyCount > 0 && subkeyListOff != 0xFFFFFFFFu &&
            static_cast<size_t>(subkeyListOff) + 8 <= hbinSize) {
            const uint8_t* sl = hbin + subkeyListOff;
            int32_t slSize = rdi32(sl);
            if (slSize < 0) {
                uint16_t cnt = rd16(sl + 6);
                bool hasHint = ((sl[4] == 'l' && (sl[5] == 'f' || sl[5] == 'h')));
                size_t stride = hasHint ? 8 : 4;
                for (uint16_t si = 0; si < cnt; ++si) {
                    size_t entryPos = static_cast<size_t>(subkeyListOff) + 8 + si * stride;
                    if (entryPos + 4 > hbinSize) break;
                    uint32_t childNkOff = rd32(hbin + entryPos);
                    parseNkCell(hive, hbin, hbinSize, childNkOff, myIdx);
                }
            }
        }
    }

    // Binary `regf` v1.3 + `hbin` serializer helper
    struct HiveWriter {
        uint8_t* hbin;
        size_t   cap;
        size_t   pos{32}; // 32-byte hbin header

        uint32_t allocCell(const void* payload, size_t payloadLen) noexcept {
            size_t rawLen = 4 + payloadLen;
            size_t aligned = (rawLen + 7) & ~7ULL;
            if (pos + aligned + 4 > cap) return 0xFFFFFFFFu;
            uint32_t off = static_cast<uint32_t>(pos);
            wri32(hbin + pos, -static_cast<int32_t>(aligned));
            if (payload && payloadLen > 0) {
                std::memcpy(hbin + pos + 4, payload, payloadLen);
            }
            if (aligned > rawLen) {
                std::memset(hbin + pos + rawLen, 0, aligned - rawLen);
            }
            pos += aligned;
            return off;
        }
    };

    uint32_t serializeKeyRecursive(const CmHive& hive, int32_t keyIdx, uint32_t parentOff, HiveWriter& w) const noexcept {
        const auto& k = hive.keys[keyIdx];
        size_t nameLen = 0;
        while (k.name[nameLen] && nameLen < 47) ++nameLen;

        // Reserve nk cell first so children know exact parentOff
        uint8_t blankNk[128]{};
        uint32_t nkOff = w.allocCell(blankNk, 76 + nameLen);
        if (nkOff == 0xFFFFFFFFu) return 0xFFFFFFFFu;

        // 1. Serialize children recursively
        uint32_t childOffs[MAX_KEYS_PER_HIVE]{};
        char childHints[MAX_KEYS_PER_HIVE][4]{};
        uint32_t childCnt = 0;
        for (uint32_t i = 0; i < hive.keyCount; ++i) {
            if (hive.keys[i].inUse && hive.keys[i].parentIdx == keyIdx) {
                uint32_t co = serializeKeyRecursive(hive, static_cast<int32_t>(i), nkOff, w);
                if (co != 0xFFFFFFFFu) {
                    childOffs[childCnt] = co;
                    for (int h = 0; h < 4; ++h) {
                        childHints[childCnt][h] = hive.keys[i].name[h] ? hive.keys[i].name[h] : '\0';
                        if (!hive.keys[i].name[h]) break;
                    }
                    childCnt++;
                }
            }
        }

        uint32_t subkeyListOff = 0xFFFFFFFFu;
        if (childCnt > 0) {
            uint8_t lfBuf[4 + MAX_KEYS_PER_HIVE * 8]{};
            lfBuf[0] = 'l'; lfBuf[1] = 'f';
            wr16(lfBuf + 2, static_cast<uint16_t>(childCnt));
            for (uint32_t ci = 0; ci < childCnt; ++ci) {
                wr32(lfBuf + 4 + ci * 8, childOffs[ci]);
                std::memcpy(lfBuf + 4 + ci * 8 + 4, childHints[ci], 4);
            }
            subkeyListOff = w.allocCell(lfBuf, 4 + childCnt * 8);
        }

        // 2. Serialize `vk` value cells + value list cell
        uint32_t valOffs[MAX_VALS_PER_KEY]{};
        uint32_t valCnt = 0;
        for (uint32_t vi = 0; vi < k.valueCount && vi < MAX_VALS_PER_KEY; ++vi) {
            const auto& v = k.values[vi];
            if (!v.inUse) continue;
            size_t vnLen = 0;
            while (v.name[vnLen] && vnLen < 47) ++vnLen;

            uint32_t rawDataLen = 0;
            uint32_t dataOff = 0xFFFFFFFFu;
            if (v.dataSize <= 4 && v.type == CM_REG_DWORD) {
                rawDataLen = 0x80000000u | v.dataSize;
                uint32_t inlineVal = 0;
                std::memcpy(&inlineVal, v.data, v.dataSize);
                dataOff = inlineVal;
            } else if (v.dataSize > 0) {
                rawDataLen = v.dataSize;
                dataOff = w.allocCell(v.data, v.dataSize);
            }

            uint8_t vkBuf[80]{};
            vkBuf[0] = 'v'; vkBuf[1] = 'k';
            wr16(vkBuf + 2, static_cast<uint16_t>(vnLen));
            wr32(vkBuf + 4, rawDataLen);
            wr32(vkBuf + 8, dataOff);
            wr32(vkBuf + 12, v.type);
            wr16(vkBuf + 16, vnLen ? 0x0001u : 0x0000u);
            wr16(vkBuf + 18, 0);
            if (vnLen > 0) std::memcpy(vkBuf + 20, v.name, vnLen);

            uint32_t vo = w.allocCell(vkBuf, 20 + vnLen);
            if (vo != 0xFFFFFFFFu) valOffs[valCnt++] = vo;
        }

        uint32_t valListOff = 0xFFFFFFFFu;
        if (valCnt > 0) {
            valListOff = w.allocCell(valOffs, valCnt * 4);
        }

        // 3. Patch nk header in-place at nkOff + 4
        uint8_t* nk = w.hbin + nkOff + 4;
        nk[0] = 'n'; nk[1] = 'k';
        wr16(nk + 2, (parentOff == 0xFFFFFFFFu) ? 0x002Cu : 0x0020u);
        wr64(nk + 4, 133742400000000000ULL);
        wr32(nk + 12, 0);
        wr32(nk + 16, parentOff);
        wr32(nk + 20, childCnt);
        wr32(nk + 24, 0);
        wr32(nk + 28, subkeyListOff);
        wr32(nk + 32, 0xFFFFFFFFu);
        wr32(nk + 36, valCnt);
        wr32(nk + 40, valListOff);
        wr32(nk + 44, 0xFFFFFFFFu);
        wr32(nk + 48, 0xFFFFFFFFu);
        wr16(nk + 72, static_cast<uint16_t>(nameLen));
        wr16(nk + 74, 0);
        if (nameLen > 0) std::memcpy(nk + 76, k.name, nameLen);
        return nkOff;
    }

    bool loadSingleHive(size_t hiveIdx, const char* name, const char* ntfsPath) noexcept {
        auto& ntfs = micant::km::ntfs_baremetal::BareMetalNtfsVolume::get();
        if (!ntfs.isMounted()) return false;

        size_t bytesRead = 0;
        if (!ntfs.readFile(ntfsPath, s_hiveIoBuf, sizeof(s_hiveIoBuf), bytesRead) || bytesRead < 4096 + 32) {
            return false;
        }
        // Validate authentic Windows NT `regf` magic & XOR-32 header checksum at 0x1FC
        if (std::memcmp(s_hiveIoBuf, "regf", 4) != 0) return false;
        uint32_t storedCrc = rd32(s_hiveIoBuf + 0x1FC);
        uint32_t calcCrc = computeRegfChecksum(s_hiveIoBuf);
        if (storedCrc != calcCrc) return false;

        uint32_t seq1 = rd32(s_hiveIoBuf + 4);
        uint32_t rootCellOff = rd32(s_hiveIoBuf + 36);
        uint32_t binsSize = rd32(s_hiveIoBuf + 40);
        if (4096ULL + binsSize > bytesRead) {
            binsSize = static_cast<uint32_t>(bytesRead - 4096);
        }
        if (binsSize < 32 || std::memcmp(s_hiveIoBuf + 4096, "hbin", 4) != 0) return false;

        auto& h = m_hives[hiveIdx];
        std::memset(&h, 0, sizeof(h));
        copyTrunc(h.hiveName, sizeof(h.hiveName), name);
        copyTrunc(h.ntfsPath, sizeof(h.ntfsPath), ntfsPath);
        h.sequence = seq1;
        h.headerChecksum = storedCrc;
        h.rawSizeBytes = static_cast<uint32_t>(bytesRead);
        parseNkCell(h, s_hiveIoBuf + 4096, binsSize, rootCellOff, -1);
        h.loaded = (h.keyCount > 0);
        return h.loaded;
    }

    int allocHandle(uint8_t hiveIdx, int32_t keyIdx) noexcept {
        for (size_t i = 1; i < MAX_KEY_HANDLES; ++i) {
            if (!m_handles[i].inUse) {
                m_handles[i].inUse = true;
                m_handles[i].hiveIdx = hiveIdx;
                m_handles[i].keyIdx = keyIdx;
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool resolveHiveAndStartKey(
        uint64_t rootHandle,
        const char* subKeyPath,
        uint8_t& outHiveIdx,
        int32_t& outStartKeyIdx,
        const char*& outRelPath
    ) const noexcept {
        outRelPath = subKeyPath ? subKeyPath : "";
        while (*outRelPath == '\\') ++outRelPath;

        if ((rootHandle & REG_KEY_HANDLE_TAG) != 0) {
            size_t hIdx = static_cast<size_t>(rootHandle & ~REG_KEY_HANDLE_TAG);
            if (hIdx == 0 || hIdx >= MAX_KEY_HANDLES || !m_handles[hIdx].inUse) return false;
            outHiveIdx = m_handles[hIdx].hiveIdx;
            outStartKeyIdx = m_handles[hIdx].keyIdx;
            return outHiveIdx < MAX_HIVES && m_hives[outHiveIdx].loaded && outStartKeyIdx >= 0;
        }

        // Predefined root (HKLM = 0x80000002, or optional "\Registry\Machine\" prefix)
        const char* p = outRelPath;
        auto stripPrefix = [&](const char* pref) -> bool {
            size_t n = 0;
            while (pref[n]) {
                if (toLower(p[n]) != toLower(pref[n])) return false;
                ++n;
            }
            if (p[n] == '\0' || p[n] == '\\') {
                p += n;
                if (*p == '\\') ++p;
                return true;
            }
            return false;
        };
        stripPrefix("Registry\\Machine");
        stripPrefix("HKLM");
        stripPrefix("HKEY_LOCAL_MACHINE");

        if (stripPrefix("SYSTEM")) {
            outHiveIdx = 0;
            outStartKeyIdx = 0;
            outRelPath = p;
            return m_hives[0].loaded;
        }
        if (stripPrefix("SOFTWARE")) {
            outHiveIdx = 1;
            outStartKeyIdx = 0;
            outRelPath = p;
            return m_hives[1].loaded;
        }
        return false;
    }

public:
    static BareMetalConfigManager& get() noexcept {
        static BareMetalConfigManager instance;
        return instance;
    }

    bool initializeFromNtfs() noexcept {
        std::memset(m_handles, 0, sizeof(m_handles));
        std::memset(m_hives, 0, sizeof(m_hives));
        m_initialized = false;
        bool sysOk = loadSingleHive(0, "SYSTEM", "\\Windows\\System32\\config\\SYSTEM");
        bool swOk  = loadSingleHive(1, "SOFTWARE", "\\Windows\\System32\\config\\SOFTWARE");
        m_initialized = (sysOk || swOk);
        return m_initialized;
    }

    /**
     * @brief Serialize hive into an authentic binary Windows NT `regf` v1.3 + `hbin` image,
     *        bump Sequence1/Sequence2, recompute XOR-32 header checksum at 0x1FC, and
     *        write it directly back to NTFS 3.1 via AHCI Bus-Master DMA!
     */
    bool flushHiveToNtfs(uint8_t hiveIdx) noexcept {
        if (hiveIdx >= MAX_HIVES || !m_hives[hiveIdx].loaded) return false;
        auto& ntfs = micant::km::ntfs_baremetal::BareMetalNtfsVolume::get();
        if (!ntfs.isMounted()) return false;

        auto& h = m_hives[hiveIdx];
        std::memset(s_hiveIoBuf, 0, sizeof(s_hiveIoBuf));

        HiveWriter w{s_hiveIoBuf + 4096, sizeof(s_hiveIoBuf) - 4096, 32};
        uint32_t rootOff = serializeKeyRecursive(h, 0, 0xFFFFFFFFu, w);
        if (rootOff == 0xFFFFFFFFu) return false;

        size_t binSize = ((w.pos + 4 + 4095) / 4096) * 4096;
        if (binSize < 8192) binSize = 8192;
        if (4096 + binSize > sizeof(s_hiveIoBuf)) return false;

        size_t rem = binSize - w.pos;
        if (rem >= 4) {
            wri32(w.hbin + w.pos, static_cast<int32_t>(rem)); // Trailing free cell
        }
        std::memcpy(w.hbin, "hbin", 4);
        wr32(w.hbin + 4, 0);
        wr32(w.hbin + 8, static_cast<uint32_t>(binSize));
        wr64(w.hbin + 20, 133742400000000000ULL);

        // Build 4096-byte `regf` base block
        uint32_t nextSeq = h.sequence + 1;
        std::memcpy(s_hiveIoBuf, "regf", 4);
        wr32(s_hiveIoBuf + 4, nextSeq);   // Sequence1
        wr32(s_hiveIoBuf + 8, nextSeq);   // Sequence2 (cleanly synced)
        wr64(s_hiveIoBuf + 12, 133742400000000000ULL);
        wr32(s_hiveIoBuf + 20, 1);        // Major 1
        wr32(s_hiveIoBuf + 24, 3);        // Minor 3
        wr32(s_hiveIoBuf + 28, 0);        // Type 0
        wr32(s_hiveIoBuf + 32, 1);        // Format 1
        wr32(s_hiveIoBuf + 36, rootOff);  // RootCellOffset (0x20)
        wr32(s_hiveIoBuf + 40, static_cast<uint32_t>(binSize));
        wr32(s_hiveIoBuf + 44, 1);        // ClusteringFactor

        // UTF-16LE Hive Name at +0x30
        for (size_t i = 0; h.hiveName[i] && i < 30; ++i) {
            wr16(s_hiveIoBuf + 48 + i * 2, static_cast<uint16_t>(static_cast<unsigned char>(h.hiveName[i])));
        }
        uint32_t crc = computeRegfChecksum(s_hiveIoBuf);
        wr32(s_hiveIoBuf + 0x1FC, crc);

        size_t totalBytes = 4096 + binSize;
        if (!ntfs.writeFile(h.ntfsPath, s_hiveIoBuf, totalBytes)) {
            return false;
        }
        h.sequence = nextSeq;
        h.headerChecksum = crc;
        h.rawSizeBytes = static_cast<uint32_t>(totalBytes);
        h.flushCount++;
        return true;
    }

    bool openOrCreateKey(
        uint64_t rootHandle,
        const char* subKeyPath,
        bool createMissing,
        uint64_t& outHandle,
        uint32_t& outDisposition
    ) noexcept {
        outHandle = 0;
        outDisposition = CM_REG_OPENED_EXISTING_KEY;
        uint8_t hiveIdx = 0;
        int32_t curKeyIdx = -1;
        const char* rel = nullptr;
        if (!resolveHiveAndStartKey(rootHandle, subKeyPath, hiveIdx, curKeyIdx, rel)) {
            return false;
        }
        auto& h = m_hives[hiveIdx];
        bool createdAny = false;

        while (*rel) {
            while (*rel == '\\') ++rel;
            if (*rel == '\0') break;
            char seg[48]{};
            size_t slen = 0;
            while (*rel && *rel != '\\' && slen + 1 < sizeof(seg)) {
                seg[slen++] = *rel++;
            }
            seg[slen] = '\0';
            while (*rel && *rel != '\\') ++rel;

            int32_t nextIdx = findChildKey(h, curKeyIdx, seg);
            if (nextIdx < 0) {
                if (!createMissing) return false;
                nextIdx = allocKeyNode(h, curKeyIdx, seg);
                if (nextIdx < 0) return false;
                createdAny = true;
            }
            curKeyIdx = nextIdx;
        }

        if (createdAny) {
            outDisposition = CM_REG_CREATED_NEW_KEY;
            flushHiveToNtfs(hiveIdx);
        }
        int hIdx = allocHandle(hiveIdx, curKeyIdx);
        if (hIdx < 0) return false;
        outHandle = REG_KEY_HANDLE_TAG | static_cast<uint64_t>(hIdx);
        return true;
    }

    bool queryValue(
        uint64_t hKey,
        const char* valueName,
        uint32_t& outType,
        void* outBuf,
        uint32_t& inOutLen
    ) const noexcept {
        if ((hKey & REG_KEY_HANDLE_TAG) == 0) return false;
        size_t hIdx = static_cast<size_t>(hKey & ~REG_KEY_HANDLE_TAG);
        if (hIdx == 0 || hIdx >= MAX_KEY_HANDLES || !m_handles[hIdx].inUse) return false;
        const auto& h = m_hives[m_handles[hIdx].hiveIdx];
        const auto& k = h.keys[m_handles[hIdx].keyIdx];
        const char* target = valueName ? valueName : "";

        for (uint32_t i = 0; i < k.valueCount; ++i) {
            if (k.values[i].inUse && strCaseEq(k.values[i].name, target)) {
                outType = k.values[i].type;
                uint32_t need = k.values[i].dataSize;
                if (!outBuf) {
                    inOutLen = need;
                    return true;
                }
                uint32_t cp = (inOutLen < need) ? inOutLen : need;
                if (cp > 0) std::memcpy(outBuf, k.values[i].data, cp);
                inOutLen = need;
                return true;
            }
        }
        return false;
    }

    bool setValue(
        uint64_t hKey,
        const char* valueName,
        uint32_t type,
        const void* data,
        uint32_t dataLen
    ) noexcept {
        if ((hKey & REG_KEY_HANDLE_TAG) == 0) return false;
        size_t hIdx = static_cast<size_t>(hKey & ~REG_KEY_HANDLE_TAG);
        if (hIdx == 0 || hIdx >= MAX_KEY_HANDLES || !m_handles[hIdx].inUse) return false;
        uint8_t hiveIdx = m_handles[hIdx].hiveIdx;
        auto& h = m_hives[hiveIdx];
        auto& k = h.keys[m_handles[hIdx].keyIdx];
        const char* target = valueName ? valueName : "";

        CmValue* slot = nullptr;
        for (uint32_t i = 0; i < k.valueCount; ++i) {
            if (k.values[i].inUse && strCaseEq(k.values[i].name, target)) {
                slot = &k.values[i];
                break;
            }
        }
        if (!slot) {
            for (uint32_t i = 0; i < k.valueCount; ++i) {
                if (!k.values[i].inUse) { slot = &k.values[i]; break; }
            }
            if (!slot) {
                if (k.valueCount >= MAX_VALS_PER_KEY) return false;
                slot = &k.values[k.valueCount++];
            }
            std::memset(slot, 0, sizeof(*slot));
            slot->inUse = true;
            copyTrunc(slot->name, sizeof(slot->name), target);
        }

        slot->type = type;
        uint32_t cp = (dataLen > sizeof(slot->data)) ? static_cast<uint32_t>(sizeof(slot->data)) : dataLen;
        if (cp > 0 && data) std::memcpy(slot->data, data, cp);
        slot->dataSize = cp;

        return flushHiveToNtfs(hiveIdx);
    }

    bool deleteValue(uint64_t hKey, const char* valueName) noexcept {
        if ((hKey & REG_KEY_HANDLE_TAG) == 0) return false;
        size_t hIdx = static_cast<size_t>(hKey & ~REG_KEY_HANDLE_TAG);
        if (hIdx == 0 || hIdx >= MAX_KEY_HANDLES || !m_handles[hIdx].inUse) return false;
        uint8_t hiveIdx = m_handles[hIdx].hiveIdx;
        auto& h = m_hives[hiveIdx];
        auto& k = h.keys[m_handles[hIdx].keyIdx];
        const char* target = valueName ? valueName : "";

        for (uint32_t i = 0; i < k.valueCount; ++i) {
            if (k.values[i].inUse && strCaseEq(k.values[i].name, target)) {
                k.values[i].inUse = false;
                return flushHiveToNtfs(hiveIdx);
            }
        }
        return false;
    }

    bool enumValue(
        uint64_t hKey,
        uint32_t index,
        char* outName,
        size_t nameCap,
        uint32_t& outType,
        void* outData,
        uint32_t& inOutDataLen
    ) const noexcept {
        if ((hKey & REG_KEY_HANDLE_TAG) == 0) return false;
        size_t hIdx = static_cast<size_t>(hKey & ~REG_KEY_HANDLE_TAG);
        if (hIdx == 0 || hIdx >= MAX_KEY_HANDLES || !m_handles[hIdx].inUse) return false;
        const auto& h = m_hives[m_handles[hIdx].hiveIdx];
        const auto& k = h.keys[m_handles[hIdx].keyIdx];

        uint32_t seen = 0;
        for (uint32_t i = 0; i < k.valueCount; ++i) {
            if (!k.values[i].inUse) continue;
            if (seen == index) {
                copyTrunc(outName, nameCap, k.values[i].name);
                outType = k.values[i].type;
                uint32_t need = k.values[i].dataSize;
                if (outData && inOutDataLen > 0) {
                    uint32_t cp = (inOutDataLen < need) ? inOutDataLen : need;
                    if (cp > 0) std::memcpy(outData, k.values[i].data, cp);
                }
                inOutDataLen = need;
                return true;
            }
            seen++;
        }
        return false;
    }

    bool enumSubKey(uint64_t hKey, uint32_t index, char* outName, size_t nameCap) const noexcept {
        if ((hKey & REG_KEY_HANDLE_TAG) == 0) return false;
        size_t hIdx = static_cast<size_t>(hKey & ~REG_KEY_HANDLE_TAG);
        if (hIdx == 0 || hIdx >= MAX_KEY_HANDLES || !m_handles[hIdx].inUse) return false;
        const auto& h = m_hives[m_handles[hIdx].hiveIdx];
        int32_t parentIdx = m_handles[hIdx].keyIdx;

        uint32_t seen = 0;
        for (uint32_t i = 0; i < h.keyCount; ++i) {
            if (h.keys[i].inUse && h.keys[i].parentIdx == parentIdx) {
                if (seen == index) {
                    copyTrunc(outName, nameCap, h.keys[i].name);
                    return true;
                }
                seen++;
            }
        }
        return false;
    }

    bool flushKey(uint64_t hKey) noexcept {
        if ((hKey & REG_KEY_HANDLE_TAG) == 0) return false;
        size_t hIdx = static_cast<size_t>(hKey & ~REG_KEY_HANDLE_TAG);
        if (hIdx == 0 || hIdx >= MAX_KEY_HANDLES || !m_handles[hIdx].inUse) return false;
        return flushHiveToNtfs(m_handles[hIdx].hiveIdx);
    }

    bool closeKey(uint64_t hKey) noexcept {
        if ((hKey & REG_KEY_HANDLE_TAG) == 0) return false;
        size_t hIdx = static_cast<size_t>(hKey & ~REG_KEY_HANDLE_TAG);
        if (hIdx == 0 || hIdx >= MAX_KEY_HANDLES || !m_handles[hIdx].inUse) return false;
        m_handles[hIdx].inUse = false;
        return true;
    }

    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] const CmHive* getHive(size_t idx) const noexcept {
        return (idx < MAX_HIVES) ? &m_hives[idx] : nullptr;
    }
};

} // namespace micant::km::cm_baremetal
