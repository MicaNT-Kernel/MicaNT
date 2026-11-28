// ============================================================================
// MicaNT: Bare-Metal NVMe 1.4 PCIe Bus-Master DMA Storage Controller Driver
// (nvme_baremetal.hpp)
//
// Description:
//   Pure -ffreestanding -nostdlib ISO C++23 NVMe Express 1.4 PCIe driver
//   compiled directly into ntoskrnl.exe. Zero mocks, zero external libs.
//
//   Implements genuine NVM Express hardware bring-up & block DMA:
//     - Full PCI Configuration Space scan for Class 01:08:02 (NVMe Controller)
//     - 64-bit BAR0/BAR1 MMIO decoding + Bus-Master/Memory Space enablement
//     - Controller Reset (CC.EN=0 -> CSTS.RDY=0), Admin Queue (AQA/ASQ/ACQ),
//       and Controller Enable (CC.EN=1, IOSQES=6, IOCQES=4 -> CSTS.RDY=1)
//     - Hardware Admin Commands over 64-entry ASQ/ACQ + Phase-Bit Doorbells:
//         * Identify Controller (0x06 CNS=1): Model, Serial, Firmware, NN
//         * Identify Namespace 1 (0x06 CNS=0): NSZE LBA count & LBAF sector size
//         * Create I/O Completion Queue (0x05 QID=1) & Submission Queue (0x01 QID=1)
//     - Hardware NVM Read (0x02) & Write (0x01) Bus-Master DMA with PRP1/PRP2
//       and 4 KiB PRP List pages for multi-sector transfers up to 64 KiB/cmd.
//
// Clean-Room ISO C++23. Zero proprietary code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "micant/km/executive/mm_baremetal.hpp"

namespace micant::km::nvme_baremetal {

static inline void outl(uint16_t port, uint32_t val) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
#else
    (void)port; (void)val;
#endif
}

static inline uint32_t inl(uint16_t port) noexcept {
    uint32_t ret = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
#else
    (void)port;
#endif
    return ret;
}

static inline uint32_t pciRead32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) noexcept {
    uint32_t addr = (1u << 31) |
                    (static_cast<uint32_t>(bus) << 16) |
                    (static_cast<uint32_t>(dev) << 11) |
                    (static_cast<uint32_t>(func) << 8) |
                    (reg & 0xFCu);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

static inline void pciWrite32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t val) noexcept {
    uint32_t addr = (1u << 31) |
                    (static_cast<uint32_t>(bus) << 16) |
                    (static_cast<uint32_t>(dev) << 11) |
                    (static_cast<uint32_t>(func) << 8) |
                    (reg & 0xFCu);
    outl(0xCF8, addr);
    outl(0xCFC, val);
}

// --- NVMe 1.4 MMIO Register Offsets ---
inline constexpr uint32_t NVME_REG_CAP   = 0x00; // Controller Capabilities (64-bit)
inline constexpr uint32_t NVME_REG_VS    = 0x08; // Version (32-bit)
inline constexpr uint32_t NVME_REG_INTMS = 0x0C; // Interrupt Mask Set
inline constexpr uint32_t NVME_REG_CC    = 0x14; // Controller Configuration
inline constexpr uint32_t NVME_REG_CSTS  = 0x1C; // Controller Status
inline constexpr uint32_t NVME_REG_AQA   = 0x24; // Admin Queue Attributes
inline constexpr uint32_t NVME_REG_ASQ   = 0x28; // Admin Submission Queue Base (64-bit)
inline constexpr uint32_t NVME_REG_ACQ   = 0x30; // Admin Completion Queue Base (64-bit)
inline constexpr uint32_t NVME_REG_DBS   = 0x1000; // Doorbells Base

// --- NVMe Command Opcodes ---
inline constexpr uint8_t NVME_ADMIN_CREATE_IOSQ = 0x01;
inline constexpr uint8_t NVME_ADMIN_CREATE_IOCQ = 0x05;
inline constexpr uint8_t NVME_ADMIN_IDENTIFY    = 0x06;
inline constexpr uint8_t NVME_NVM_FLUSH         = 0x00;
inline constexpr uint8_t NVME_NVM_WRITE         = 0x01;
inline constexpr uint8_t NVME_NVM_READ          = 0x02;

inline constexpr uint16_t NVME_QUEUE_DEPTH = 64;

#pragma pack(push, 1)
struct NvmeSqe {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t rsvd2;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
};

struct NvmeCqe {
    uint32_t dw0;
    uint32_t dw1;
    uint16_t sqHead;
    uint16_t sqId;
    uint16_t cid;
    uint16_t status; // Bit 0 = Phase Tag, Bits 15:1 = Status Field
};
#pragma pack(pop)

static_assert(sizeof(NvmeSqe) == 64, "NVMe SQE must be 64 bytes");
static_assert(sizeof(NvmeCqe) == 16, "NVMe CQE must be 16 bytes");

// Page-aligned physical DMA queues & transfer buffers (identity-mapped in Ring 0)
alignas(4096) static NvmeSqe  s_nvmeAdminSq[NVME_QUEUE_DEPTH]{};
alignas(4096) static NvmeCqe  s_nvmeAdminCq[NVME_QUEUE_DEPTH]{};
alignas(4096) static NvmeSqe  s_nvmeIoSq[NVME_QUEUE_DEPTH]{};
alignas(4096) static NvmeCqe  s_nvmeIoCq[NVME_QUEUE_DEPTH]{};
alignas(4096) static uint8_t  s_nvmeIdentifyBuf[4096]{};
alignas(4096) static uint64_t s_nvmePrpList[512]{};
alignas(4096) static uint8_t  s_nvmeDmaBuf[65536]{}; // 64 KiB = 16 contiguous 4 KiB pages

class BareMetalNvmeController {
private:
    bool     m_initialized{false};
    bool     m_online{false};
    uint8_t  m_pciBus{0};
    uint8_t  m_pciDev{0};
    uint8_t  m_pciFunc{0};
    uint16_t m_vendorId{0};
    uint16_t m_deviceId{0};
    uint64_t m_mmioBase{0};
    uint32_t m_version{0};
    uint32_t m_doorbellStride{4};
    uint32_t m_sectorSize{512};
    uint64_t m_totalSectors{0};
    uint32_t m_namespaceCount{0};

    uint16_t m_adminSqTail{0};
    uint16_t m_adminCqHead{0};
    uint8_t  m_adminCqPhase{1};
    uint16_t m_ioSqTail{0};
    uint16_t m_ioCqHead{0};
    uint8_t  m_ioCqPhase{1};
    uint16_t m_nextCid{1};

    uint64_t m_ioReadCmds{0};
    uint64_t m_ioWriteCmds{0};
    uint64_t m_sectorsRead{0};
    uint64_t m_sectorsWritten{0};
    volatile uint32_t m_lock{0};

    char     m_model[48]{};
    char     m_serial[24]{};
    char     m_firmware[12]{};

    uint64_t lockIrqSave() noexcept {
        uint64_t flags = 0;
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
#endif
        while (__atomic_exchange_n(&m_lock, 1u, __ATOMIC_ACQUIRE) != 0u) {
#if defined(__x86_64__)
            __asm__ __volatile__("pause" ::: "memory");
#endif
        }
        return flags;
    }

    void unlockIrqRestore(uint64_t flags) noexcept {
        __atomic_store_n(&m_lock, 0u, __ATOMIC_RELEASE);
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        if (flags & 0x200ULL) __asm__ __volatile__("sti" ::: "memory");
#else
        (void)flags;
#endif
    }

    inline uint32_t rd32(uint32_t off) const noexcept {
        return *reinterpret_cast<const volatile uint32_t*>(m_mmioBase + off);
    }

    inline void wr32(uint32_t off, uint32_t val) const noexcept {
        *reinterpret_cast<volatile uint32_t*>(m_mmioBase + off) = val;
    }

    inline uint64_t rd64(uint32_t off) const noexcept {
        uint64_t lo = rd32(off);
        uint64_t hi = rd32(off + 4);
        return lo | (hi << 32);
    }

    inline void wr64(uint32_t off, uint64_t val) const noexcept {
        wr32(off, static_cast<uint32_t>(val & 0xFFFFFFFFu));
        wr32(off + 4, static_cast<uint32_t>(val >> 32));
    }

    inline void ringSqDoorbell(uint16_t qid, uint16_t tail) const noexcept {
        wr32(NVME_REG_DBS + (2u * qid) * m_doorbellStride, tail);
    }

    inline void ringCqDoorbell(uint16_t qid, uint16_t head) const noexcept {
        wr32(NVME_REG_DBS + (2u * qid + 1u) * m_doorbellStride, head);
    }

    static void copyTrimmedAscii(char* dst, size_t dstCap, const uint8_t* src, size_t srcLen) noexcept {
        size_t n = (srcLen < dstCap - 1) ? srcLen : (dstCap - 1);
        for (size_t i = 0; i < n; ++i) {
            char c = static_cast<char>(src[i]);
            dst[i] = (c >= 0x20 && c <= 0x7E) ? c : ' ';
        }
        while (n > 0 && dst[n - 1] == ' ') --n;
        dst[n] = '\0';
    }

    bool submitAndWait(
        uint16_t qid,
        NvmeSqe* sq,
        NvmeCqe* cq,
        uint16_t& sqTail,
        uint16_t& cqHead,
        uint8_t& cqPhase,
        NvmeSqe& cmd
    ) noexcept {
        cmd.cid = m_nextCid++;
        if (m_nextCid == 0) m_nextCid = 1;

        sq[sqTail] = cmd;
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        sqTail = static_cast<uint16_t>((sqTail + 1u) % NVME_QUEUE_DEPTH);
        ringSqDoorbell(qid, sqTail);

        // Poll CQ entry for hardware Phase Bit inversion
        for (uint32_t spin = 0; spin < 50000000u; ++spin) {
            uint16_t status = __atomic_load_n(&cq[cqHead].status, __ATOMIC_ACQUIRE);
            if ((status & 0x0001u) == cqPhase) {
                uint16_t sc = static_cast<uint16_t>(status >> 1);
                cqHead = static_cast<uint16_t>((cqHead + 1u) % NVME_QUEUE_DEPTH);
                if (cqHead == 0) {
                    cqPhase ^= 1u;
                }
                ringCqDoorbell(qid, cqHead);
                return (sc == 0);
            }
#if defined(__x86_64__)
            __asm__ __volatile__("pause" ::: "memory");
#endif
        }
        return false;
    }

    bool submitAdmin(NvmeSqe& cmd) noexcept {
        return submitAndWait(0, s_nvmeAdminSq, s_nvmeAdminCq, m_adminSqTail, m_adminCqHead, m_adminCqPhase, cmd);
    }

    bool submitIo(NvmeSqe& cmd) noexcept {
        return submitAndWait(1, s_nvmeIoSq, s_nvmeIoCq, m_ioSqTail, m_ioCqHead, m_ioCqPhase, cmd);
    }

    static void setupPrps(NvmeSqe& cmd, uint64_t dmaPhys, size_t byteLen) noexcept {
        cmd.prp1 = dmaPhys;
        if (byteLen <= 4096) {
            cmd.prp2 = 0;
        } else if (byteLen <= 8192) {
            cmd.prp2 = dmaPhys + 4096ULL;
        } else {
            size_t totalPages = (byteLen + 4095u) / 4096u;
            for (size_t p = 1; p < totalPages && (p - 1) < 512; ++p) {
                s_nvmePrpList[p - 1] = dmaPhys + static_cast<uint64_t>(p) * 4096ULL;
            }
            __atomic_thread_fence(__ATOMIC_SEQ_CST);
            cmd.prp2 = reinterpret_cast<uint64_t>(s_nvmePrpList);
        }
    }

public:
    static BareMetalNvmeController& get() noexcept {
        static BareMetalNvmeController instance;
        return instance;
    }

    bool initialize() noexcept {
        std::memset(this, 0, sizeof(*this));
        m_sectorSize = 512;
        m_adminCqPhase = 1;
        m_ioCqPhase = 1;
        m_nextCid = 1;

        // 1. Scan PCI buses 0..7 for Class 01, Subclass 08, ProgIF 02 (NVM Express)
        bool found = false;
        for (uint8_t bus = 0; bus < 8 && !found; ++bus) {
            for (uint8_t dev = 0; dev < 32 && !found; ++dev) {
                for (uint8_t fn = 0; fn < 8 && !found; ++fn) {
                    uint32_t vd = pciRead32(bus, dev, fn, 0x00);
                    if (vd == 0xFFFFFFFFu) {
                        if (fn == 0) break;
                        continue;
                    }
                    uint32_t cls = pciRead32(bus, dev, fn, 0x08);
                    if (((cls >> 8) & 0xFFFFFFu) == 0x010802u) {
                        m_pciBus = bus;
                        m_pciDev = dev;
                        m_pciFunc = fn;
                        m_vendorId = static_cast<uint16_t>(vd & 0xFFFFu);
                        m_deviceId = static_cast<uint16_t>((vd >> 16) & 0xFFFFu);
                        found = true;
                    }
                }
            }
        }
        if (!found) return false;

        // 2. Enable Memory Space + Bus-Master DMA + Interrupt Disable (clean polled CQE mode)
        uint32_t pciCmd = pciRead32(m_pciBus, m_pciDev, m_pciFunc, 0x04);
        pciWrite32(m_pciBus, m_pciDev, m_pciFunc, 0x04, (pciCmd | 0x0406u));

        // 3. Decode 64-bit BAR0/BAR1 MMIO physical base & map into canonical high-half MMIO aperture (PML4[500])
        uint32_t bar0Raw = pciRead32(m_pciBus, m_pciDev, m_pciFunc, 0x10);
        uint64_t mmio = static_cast<uint64_t>(bar0Raw & ~0xFULL);
        if ((bar0Raw & 0x06u) == 0x04u) {
            uint32_t bar1Raw = pciRead32(m_pciBus, m_pciDev, m_pciFunc, 0x14);
            mmio |= (static_cast<uint64_t>(bar1Raw) << 32);
        }
        if (mmio == 0) return false;
        uint64_t highHalfMmio = mm_baremetal::BareMetalPaging::mapKernelMmio(mmio, 65536);
        m_mmioBase = (highHalfMmio != 0) ? highHalfMmio : mmio;

        uint64_t cap = rd64(NVME_REG_CAP);
        m_version = rd32(NVME_REG_VS);
        uint32_t dstrd = static_cast<uint32_t>((cap >> 32) & 0x0Fu);
        m_doorbellStride = 4u << dstrd;

        // 4. Mask all controller interrupts & reset controller (CC.EN = 0 -> wait CSTS.RDY = 0)
        wr32(NVME_REG_INTMS, 0xFFFFFFFFu);
        uint32_t cc = rd32(NVME_REG_CC);
        if (cc & 1u) {
            wr32(NVME_REG_CC, cc & ~1u);
            for (uint32_t i = 0; i < 20000000u; ++i) {
                if ((rd32(NVME_REG_CSTS) & 1u) == 0) break;
#if defined(__x86_64__)
                __asm__ __volatile__("pause");
#endif
            }
        }
        if ((rd32(NVME_REG_CSTS) & 1u) != 0) return false;

        // 5. Clear & bind Admin Submission Queue (ASQ) and Admin Completion Queue (ACQ)
        std::memset(s_nvmeAdminSq, 0, sizeof(s_nvmeAdminSq));
        std::memset(s_nvmeAdminCq, 0, sizeof(s_nvmeAdminCq));
        std::memset(s_nvmeIoSq, 0, sizeof(s_nvmeIoSq));
        std::memset(s_nvmeIoCq, 0, sizeof(s_nvmeIoCq));

        uint32_t aqa = ((static_cast<uint32_t>(NVME_QUEUE_DEPTH - 1u)) << 16) |
                       static_cast<uint32_t>(NVME_QUEUE_DEPTH - 1u);
        wr32(NVME_REG_AQA, aqa);
        wr64(NVME_REG_ASQ, reinterpret_cast<uint64_t>(s_nvmeAdminSq));
        wr64(NVME_REG_ACQ, reinterpret_cast<uint64_t>(s_nvmeAdminCq));
        __atomic_thread_fence(__ATOMIC_SEQ_CST);

        // 6. Enable Controller: CSS=0 (NVM), MPS=0 (4KB), AMS=0 (RR), IOSQES=6 (64B), IOCQES=4 (16B), EN=1
        uint32_t ccEnable = (4u << 20) | (6u << 16) | 1u;
        wr32(NVME_REG_CC, ccEnable);
        for (uint32_t i = 0; i < 20000000u; ++i) {
            uint32_t csts = rd32(NVME_REG_CSTS);
            if (csts & 2u) return false; // CFS fatal
            if (csts & 1u) break;        // RDY!
#if defined(__x86_64__)
            __asm__ __volatile__("pause");
#endif
        }
        if ((rd32(NVME_REG_CSTS) & 1u) == 0) return false;

        // 7. Admin Command: Identify Controller (Opcode 0x06, CNS=1)
        std::memset(s_nvmeIdentifyBuf, 0, sizeof(s_nvmeIdentifyBuf));
        NvmeSqe idCtrl{};
        idCtrl.opcode = NVME_ADMIN_IDENTIFY;
        idCtrl.nsid = 0;
        idCtrl.prp1 = reinterpret_cast<uint64_t>(s_nvmeIdentifyBuf);
        idCtrl.cdw10 = 1u; // CNS = Identify Controller
        if (!submitAdmin(idCtrl)) return false;

        copyTrimmedAscii(m_serial, sizeof(m_serial), s_nvmeIdentifyBuf + 4, 20);
        copyTrimmedAscii(m_model, sizeof(m_model), s_nvmeIdentifyBuf + 24, 40);
        copyTrimmedAscii(m_firmware, sizeof(m_firmware), s_nvmeIdentifyBuf + 64, 8);
        std::memcpy(&m_namespaceCount, s_nvmeIdentifyBuf + 516, sizeof(uint32_t));

        // 8. Admin Command: Identify Namespace 1 (Opcode 0x06, NSID=1, CNS=0)
        std::memset(s_nvmeIdentifyBuf, 0, sizeof(s_nvmeIdentifyBuf));
        NvmeSqe idNs{};
        idNs.opcode = NVME_ADMIN_IDENTIFY;
        idNs.nsid = 1;
        idNs.prp1 = reinterpret_cast<uint64_t>(s_nvmeIdentifyBuf);
        idNs.cdw10 = 0u; // CNS = Identify Namespace
        if (!submitAdmin(idNs)) return false;

        std::memcpy(&m_totalSectors, s_nvmeIdentifyBuf + 0, sizeof(uint64_t));
        uint8_t flbas = s_nvmeIdentifyBuf[26] & 0x0Fu;
        uint32_t lbaf = 0;
        std::memcpy(&lbaf, s_nvmeIdentifyBuf + 128 + static_cast<size_t>(flbas) * 4, sizeof(uint32_t));
        uint8_t lbads = static_cast<uint8_t>((lbaf >> 16) & 0xFFu);
        if (lbads >= 9 && lbads <= 12) {
            m_sectorSize = 1u << lbads;
        }
        if (m_totalSectors == 0) return false;

        // 9. Admin Command: Create I/O Completion Queue (QID=1, PC=1, IEN=0)
        NvmeSqe createCq{};
        createCq.opcode = NVME_ADMIN_CREATE_IOCQ;
        createCq.prp1 = reinterpret_cast<uint64_t>(s_nvmeIoCq);
        createCq.cdw10 = (static_cast<uint32_t>(NVME_QUEUE_DEPTH - 1u) << 16) | 1u;
        createCq.cdw11 = 0x0001u; // PC=1 (Physically Contiguous)
        if (!submitAdmin(createCq)) return false;

        // 10. Admin Command: Create I/O Submission Queue (QID=1, CQID=1, PC=1)
        NvmeSqe createSq{};
        createSq.opcode = NVME_ADMIN_CREATE_IOSQ;
        createSq.prp1 = reinterpret_cast<uint64_t>(s_nvmeIoSq);
        createSq.cdw10 = (static_cast<uint32_t>(NVME_QUEUE_DEPTH - 1u) << 16) | 1u;
        createSq.cdw11 = (1u << 16) | 0x0001u; // CQID=1 | PC=1
        if (!submitAdmin(createSq)) return false;

        m_initialized = true;
        m_online = true;
        return true;
    }

    bool readSectors(uint64_t startLba, uint16_t count, void* dst) noexcept {
        if (!m_online || !dst || count == 0) return false;
        uint64_t flags = lockIrqSave();
        auto* out = static_cast<uint8_t*>(dst);
        uint64_t curLba = startLba;
        uint32_t rem = count;
        uint32_t maxSecPerCmd = static_cast<uint32_t>(sizeof(s_nvmeDmaBuf) / m_sectorSize);
        if (maxSecPerCmd == 0) maxSecPerCmd = 1;

        while (rem > 0) {
            uint32_t batch = (rem < maxSecPerCmd) ? rem : maxSecPerCmd;
            size_t bytes = static_cast<size_t>(batch) * m_sectorSize;

            NvmeSqe cmd{};
            cmd.opcode = NVME_NVM_READ;
            cmd.nsid = 1;
            setupPrps(cmd, reinterpret_cast<uint64_t>(s_nvmeDmaBuf), bytes);
            cmd.cdw10 = static_cast<uint32_t>(curLba & 0xFFFFFFFFu);
            cmd.cdw11 = static_cast<uint32_t>(curLba >> 32);
            cmd.cdw12 = batch - 1u; // 0-based NLB

            if (!submitIo(cmd)) {
                unlockIrqRestore(flags);
                return false;
            }
            std::memcpy(out, s_nvmeDmaBuf, bytes);
            out += bytes;
            curLba += batch;
            rem -= batch;
            m_ioReadCmds++;
            m_sectorsRead += batch;
        }
        unlockIrqRestore(flags);
        return true;
    }

    bool writeSectors(uint64_t startLba, uint16_t count, const void* src) noexcept {
        if (!m_online || !src || count == 0) return false;
        uint64_t flags = lockIrqSave();
        const auto* in = static_cast<const uint8_t*>(src);
        uint64_t curLba = startLba;
        uint32_t rem = count;
        uint32_t maxSecPerCmd = static_cast<uint32_t>(sizeof(s_nvmeDmaBuf) / m_sectorSize);
        if (maxSecPerCmd == 0) maxSecPerCmd = 1;

        while (rem > 0) {
            uint32_t batch = (rem < maxSecPerCmd) ? rem : maxSecPerCmd;
            size_t bytes = static_cast<size_t>(batch) * m_sectorSize;
            std::memcpy(s_nvmeDmaBuf, in, bytes);
            __atomic_thread_fence(__ATOMIC_SEQ_CST);

            NvmeSqe cmd{};
            cmd.opcode = NVME_NVM_WRITE;
            cmd.nsid = 1;
            setupPrps(cmd, reinterpret_cast<uint64_t>(s_nvmeDmaBuf), bytes);
            cmd.cdw10 = static_cast<uint32_t>(curLba & 0xFFFFFFFFu);
            cmd.cdw11 = static_cast<uint32_t>(curLba >> 32);
            cmd.cdw12 = batch - 1u;

            if (!submitIo(cmd)) {
                unlockIrqRestore(flags);
                return false;
            }
            in += bytes;
            curLba += batch;
            rem -= batch;
            m_ioWriteCmds++;
            m_sectorsWritten += batch;
        }
        unlockIrqRestore(flags);
        return true;
    }

    [[nodiscard]] bool        isInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] bool        isOnline() const noexcept { return m_online; }
    [[nodiscard]] uint8_t     getPciBus() const noexcept { return m_pciBus; }
    [[nodiscard]] uint8_t     getPciDev() const noexcept { return m_pciDev; }
    [[nodiscard]] uint8_t     getPciFunc() const noexcept { return m_pciFunc; }
    [[nodiscard]] uint16_t    getVendorId() const noexcept { return m_vendorId; }
    [[nodiscard]] uint16_t    getDeviceId() const noexcept { return m_deviceId; }
    [[nodiscard]] uint32_t    getVersion() const noexcept { return m_version; }
    [[nodiscard]] uint32_t    getSectorSize() const noexcept { return m_sectorSize; }
    [[nodiscard]] uint64_t    getTotalSectors() const noexcept { return m_totalSectors; }
    [[nodiscard]] uint32_t    getNamespaceCount() const noexcept { return m_namespaceCount; }
    [[nodiscard]] uint64_t    getIoReadCmds() const noexcept { return m_ioReadCmds; }
    [[nodiscard]] uint64_t    getIoWriteCmds() const noexcept { return m_ioWriteCmds; }
    [[nodiscard]] uint64_t    getSectorsRead() const noexcept { return m_sectorsRead; }
    [[nodiscard]] uint64_t    getSectorsWritten() const noexcept { return m_sectorsWritten; }
    [[nodiscard]] const char* getModel() const noexcept { return m_model; }
    [[nodiscard]] const char* getSerial() const noexcept { return m_serial; }
    [[nodiscard]] const char* getFirmware() const noexcept { return m_firmware; }
};

} // namespace micant::km::nvme_baremetal
