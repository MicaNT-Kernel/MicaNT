// ============================================================================
// MicaNT: Sovereign Multipath I/O (MPIO) Subsystem & Device-Specific Module (DSM)
// Architecture (include/micant/km/drivers/storage/mpio.hpp)
//
// Sovereign Subsystem: TitanMpio / StorageMultipathSubsystem
//
// Strict Clean-Room Implementation based on:
//   - Operating System Storage Architecture & Multipath I/O Subsystem (mpio.sys, msdsm.sys)
//   - Storage Area Network (SAN), Fibre Channel (FC), iSCSI, SAS, and NVMe-oF Multipathing
//   - Device-Specific Module (DSM) Interface & Hardware ID Claim Architecture
//   - SPC-3 / SPC-4 SCSI-3 Persistent Reservation (PR) Multipath Coordination
//   - Asymmetric Logical Unit Access (ALUA) Target Port Group (TPG) Path State Machine
//   - Supreme Court of the United States: Google LLC v. Oracle America, Inc.
//     (141 S. Ct. 1183, 2021) - API interoperability doctrine
//
// Subsystem Overview:
//   TitanMpio delivers enterprise storage high availability, dynamic load balancing,
//   and path fault tolerance for clustered and multi-path SAN topologies:
//   1. Multipath Virtual Disk Abstraction:
//      - Aggregates redundant physical I/O paths into unified virtual disk objects
//        (\\Device\\MPIODisk0, \\Device\\MPIODisk1) presented to upper storage stacks.
//   2. Device-Specific Module (DSM) Engine (msdsm.sys):
//      - Claims storage hardware matching registered Vendor ID / Product ID signatures.
//      - Configurable ALUA target port group awareness and path priorities.
//   3. Advanced Multipath Load Balancing Policies:
//      - Failover Only (Active/Passive single-path routing with automatic standby failover).
//      - Round Robin (equal distribution across active/optimized paths).
//      - Round Robin with Subset (round robin within preferred active subset).
//      - Least Queue Depth (routes I/O to path with lowest pending request count).
//      - Weighted Paths (proportional routing based on assigned path weights).
//      - Least Blocks (routes I/O to path with fewest pending data blocks in transit).
//   4. ALUA State Machine & Path Health Heartbeat:
//      - Tracks ActiveOptimized, ActiveUnoptimized, Standby, Failed, and Degraded paths.
//      - Non-disruptive path failover upon I/O timeout, bus reset, or transport failure.
//      - Automated periodic Test Unit Ready (TUR) verification and seamless failback.
//   5. SCSI-3 Persistent Reservation (PR) Coordination:
//      - Multi-path PROUT / PRIN registration, reservation, release, preempt, and clear.
//      - Ensures split-brain protection for clustered storage workloads.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/um/servers/scm.hpp"

namespace micant::mpio {

// ============================================================================
// 1. Core Enumerations & Constants
// ============================================================================

enum class MpioLoadBalancePolicy : uint32_t {
    FailoverOnly            = 1, // FOO: Single active path; standby used only on failure
    RoundRobin              = 2, // RR: Equal cyclic distribution across all active paths
    RoundRobinWithSubset    = 3, // RRWS: Round robin within designated preferred subset
    LeastQueueDepth         = 4, // LQD: Routes to path with lowest pending request count
    WeightedPaths           = 5, // WP: Proportional routing according to path weight scores
    LeastBlocks             = 6  // LB: Routes to path with smallest pending data volume
};

inline const char* MpioPolicyToString(MpioLoadBalancePolicy policy) {
    switch (policy) {
        case MpioLoadBalancePolicy::FailoverOnly:         return "Failover Only (FOO)";
        case MpioLoadBalancePolicy::RoundRobin:           return "Round Robin (RR)";
        case MpioLoadBalancePolicy::RoundRobinWithSubset: return "Round Robin with Subset (RRWS)";
        case MpioLoadBalancePolicy::LeastQueueDepth:      return "Least Queue Depth (LQD)";
        case MpioLoadBalancePolicy::WeightedPaths:        return "Weighted Paths (WP)";
        case MpioLoadBalancePolicy::LeastBlocks:          return "Least Blocks (LB)";
        default:                                          return "Unknown Policy";
    }
}

enum class MpioPathState : uint32_t {
    ActiveOptimized   = 0, // Preferred ALUA path: primary controller port
    ActiveUnoptimized = 1, // Non-preferred ALUA path: secondary controller port
    Standby           = 2, // Passive standby path (Failover Only)
    Failed            = 3, // Link down, transport error, or I/O timeout
    Degraded          = 4  // Marginal path: excessive retries or high latency
};

inline const char* MpioPathStateToString(MpioPathState state) {
    switch (state) {
        case MpioPathState::ActiveOptimized:   return "Active/Optimized";
        case MpioPathState::ActiveUnoptimized: return "Active/Unoptimized";
        case MpioPathState::Standby:           return "Standby";
        case MpioPathState::Failed:            return "Failed";
        case MpioPathState::Degraded:          return "Degraded";
        default:                               return "Unknown";
    }
}

enum class MpioBusType : uint32_t {
    FibreChannel      = 1, // FC SAN Fabric
    iSCSI             = 2, // iSCSI IP SAN Fabric
    SAS               = 3, // Serial Attached SCSI Fabric
    NVMeOverFabrics   = 4, // NVMe-oF (RDMA / TCP)
    FCoE              = 5  // Fibre Channel over Ethernet
};

inline const char* MpioBusTypeToString(MpioBusType bus) {
    switch (bus) {
        case MpioBusType::FibreChannel:    return "Fibre Channel";
        case MpioBusType::iSCSI:           return "iSCSI";
        case MpioBusType::SAS:             return "Serial Attached SCSI (SAS)";
        case MpioBusType::NVMeOverFabrics: return "NVMe-oF";
        case MpioBusType::FCoE:            return "Fibre Channel over Ethernet";
        default:                           return "Generic Storage Fabric";
    }
}

enum class ScsiPrType : uint32_t {
    WriteExclusive                 = 1,
    ExclusiveAccess                = 2,
    WriteExclusiveRegistrantsOnly  = 3,
    ExclusiveAccessRegistrantsOnly = 4,
    WriteExclusiveAllRegistrants   = 5,
    ExclusiveAccessAllRegistrants  = 6
};

inline const char* ScsiPrTypeToString(ScsiPrType type) {
    switch (type) {
        case ScsiPrType::WriteExclusive:                 return "Write Exclusive";
        case ScsiPrType::ExclusiveAccess:                return "Exclusive Access";
        case ScsiPrType::WriteExclusiveRegistrantsOnly:  return "Write Exclusive (Registrants Only)";
        case ScsiPrType::ExclusiveAccessRegistrantsOnly: return "Exclusive Access (Registrants Only)";
        case ScsiPrType::WriteExclusiveAllRegistrants:   return "Write Exclusive (All Registrants)";
        case ScsiPrType::ExclusiveAccessAllRegistrants:  return "Exclusive Access (All Registrants)";
        default:                                         return "Unknown Reservation Type";
    }
}

enum class ScsiPrAction : uint32_t {
    Register        = 0,
    Reserve         = 1,
    Release         = 2,
    Clear           = 3,
    Preempt         = 4,
    PreemptAndAbort = 5
};

// ============================================================================
// 2. Data Structures
// ============================================================================

struct MpioPathDescriptor {
    uint32_t pathId{0};               // Unique path identifier (e.g. 0x00010001)
    uint32_t adapterIndex{0};         // Host adapter index (HBA 0, HBA 1)
    uint32_t busNumber{0};            // SCSI bus
    uint32_t targetId{0};             // SCSI target
    uint32_t lunId{0};                // SCSI LUN
    std::string targetPortGroup{"TPG-01"}; // ALUA Target Port Group
    MpioPathState state{MpioPathState::ActiveOptimized};
    uint32_t weight{100};             // Weight score for Weighted Paths (default 100)
    bool preferredPath{true};         // Preference flag (Round Robin with Subset / ALUA)
    uint32_t queueDepth{0};           // Active in-flight I/O requests
    uint64_t pendingBlocks{0};        // Active in-flight block count
    uint64_t totalIoRequests{0};      // Cumulative I/O operations dispatched
    uint64_t totalBytesTransferred{0};// Cumulative bytes transferred
    uint64_t totalIoErrors{0};        // Cumulative failed I/O count
    uint32_t consecutiveFailures{0};  // Consecutive heartbeat / TUR probe failures
    uint64_t lastHeartbeatTick{0};    // Timestamp of last successful path verification
    std::string sourceHbaAddress{"21:00:00:24:ff:45:10:a1"}; // Initiator WWN / IQN
    std::string targetPortAddress{"50:06:01:60:88:20:12:ef"};// Target WWN / IQN
};

struct MpioScsiReservation {
    bool hasReservation{false};
    ScsiPrType reservationType{ScsiPrType::WriteExclusiveRegistrantsOnly};
    uint64_t reservationKey{0};
    uint32_t reservationHolderPathId{0};
    std::unordered_map<uint64_t, uint32_t> registeredKeys; // key -> pathId
};

struct MpioDeviceDescriptor {
    uint32_t diskIndex{0};            // Disk index (0 -> \Device\MPIODisk0)
    std::string deviceName{"\\Device\\MPIODisk0"};
    std::string hardwareId{"MicaNT  SAN_STORAGE    "};
    std::string serialNumber{"SN-MICANT-LUN-884920"};
    uint64_t capacityBytes{10ULL * 1024 * 1024 * 1024 * 1024}; // 10 TB default
    uint32_t blockSize{4096};
    MpioBusType busType{MpioBusType::FibreChannel};
    MpioLoadBalancePolicy policy{MpioLoadBalancePolicy::RoundRobin};
    uint32_t rrIndex{0};              // Internal cyclic counter for Round Robin
    std::vector<MpioPathDescriptor> paths;
    MpioScsiReservation reservation;
    uint64_t totalIosDispatched{0};
    uint64_t totalFailoverEvents{0};
    uint64_t totalFailbackEvents{0};
};

struct MpioSubsystemVitals {
    uint32_t claimedDevicesCount{0};
    uint32_t totalPathsCount{0};
    uint32_t activePathsCount{0};
    uint32_t standbyPathsCount{0};
    uint32_t failedPathsCount{0};
    uint64_t totalIosDispatched{0};
    uint64_t totalFailoverEvents{0};
    uint64_t totalFailbackEvents{0};
    std::string dsmModuleName{"msdsm.sys"};
    std::string dsmVendorName{"MicaNT Sovereign Storage DSM"};
    uint32_t pathVerifyPeriodSeconds{30};
};

// ============================================================================
// 3. Sovereign Multipath I/O Manager & DSM Engine (Singleton)
// ============================================================================

class MpioManager {
public:
    static MpioManager& get() {
        static MpioManager instance;
        return instance;
    }

    // Reset subsystem to canonical initial state (for testing & lifecycle)
    void reset() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        devices_.clear();
        claimedHardwareIds_.clear();
        pathVerifyPeriodSeconds_ = 30;
        failbackStabilizationInterval_ = 2; // heartbeats before failback

        // Register default hardware claim strings
        claimedHardwareIds_.insert("MicaNT  SAN_STORAGE    ");
        claimedHardwareIds_.insert("GENERIC SAN_LUN_DEVICE ");
        claimedHardwareIds_.insert("MSFT2008iSCSIBusType_0x9");
        claimedHardwareIds_.insert("PURE    FlashArray      ");

        // Seed Canonical Multi-Path SAN Disk 0: Fibre Channel 4-Path LUN
        {
            MpioDeviceDescriptor dev0{};
            dev0.diskIndex = 0;
            dev0.deviceName = "\\Device\\MPIODisk0";
            dev0.hardwareId = "MicaNT  SAN_STORAGE    ";
            dev0.serialNumber = "SN-MICANT-LUN-884920";
            dev0.capacityBytes = 10ULL * 1024 * 1024 * 1024 * 1024; // 10 TB
            dev0.blockSize = 4096;
            dev0.busType = MpioBusType::FibreChannel;
            dev0.policy = MpioLoadBalancePolicy::RoundRobin;

            // Path 0: HBA 0 -> Controller A (Active/Optimized)
            MpioPathDescriptor p0{};
            p0.pathId = 0x00010001;
            p0.adapterIndex = 0;
            p0.busNumber = 0;
            p0.targetId = 1;
            p0.lunId = 0;
            p0.targetPortGroup = "TPG-01";
            p0.state = MpioPathState::ActiveOptimized;
            p0.weight = 100;
            p0.preferredPath = true;
            p0.sourceHbaAddress = "21:00:00:24:ff:45:10:a1";
            p0.targetPortAddress = "50:06:01:60:88:20:12:ef";
            dev0.paths.push_back(p0);

            // Path 1: HBA 1 -> Controller A (Active/Optimized)
            MpioPathDescriptor p1{};
            p1.pathId = 0x00010002;
            p1.adapterIndex = 1;
            p1.busNumber = 0;
            p1.targetId = 1;
            p1.lunId = 0;
            p1.targetPortGroup = "TPG-01";
            p1.state = MpioPathState::ActiveOptimized;
            p1.weight = 100;
            p1.preferredPath = true;
            p1.sourceHbaAddress = "21:00:00:24:ff:45:10:a2";
            p1.targetPortAddress = "50:06:01:60:88:20:12:ef";
            dev0.paths.push_back(p1);

            // Path 2: HBA 0 -> Controller B (Active/Unoptimized)
            MpioPathDescriptor p2{};
            p2.pathId = 0x00020001;
            p2.adapterIndex = 0;
            p2.busNumber = 0;
            p2.targetId = 2;
            p2.lunId = 0;
            p2.targetPortGroup = "TPG-02";
            p2.state = MpioPathState::ActiveUnoptimized;
            p2.weight = 50;
            p2.preferredPath = false;
            p2.sourceHbaAddress = "21:00:00:24:ff:45:10:a1";
            p2.targetPortAddress = "50:06:01:60:88:20:12:ee";
            dev0.paths.push_back(p2);

            // Path 3: HBA 1 -> Controller B (Active/Unoptimized)
            MpioPathDescriptor p3{};
            p3.pathId = 0x00020002;
            p3.adapterIndex = 1;
            p3.busNumber = 0;
            p3.targetId = 2;
            p3.lunId = 0;
            p3.targetPortGroup = "TPG-02";
            p3.state = MpioPathState::ActiveUnoptimized;
            p3.weight = 50;
            p3.preferredPath = false;
            p3.sourceHbaAddress = "21:00:00:24:ff:45:10:a2";
            p3.targetPortAddress = "50:06:01:60:88:20:12:ee";
            dev0.paths.push_back(p3);

            devices_.push_back(dev0);
        }

        // Seed Canonical Multi-Path SAN Disk 1: iSCSI 2-Path Failover-Only LUN
        {
            MpioDeviceDescriptor dev1{};
            dev1.diskIndex = 1;
            dev1.deviceName = "\\Device\\MPIODisk1";
            dev1.hardwareId = "MSFT2008iSCSIBusType_0x9";
            dev1.serialNumber = "SN-MICANT-ISCSI-514022";
            dev1.capacityBytes = 2ULL * 1024 * 1024 * 1024 * 1024; // 2 TB
            dev1.blockSize = 4096;
            dev1.busType = MpioBusType::iSCSI;
            dev1.policy = MpioLoadBalancePolicy::FailoverOnly;

            // Path 0: Primary IP NIC 0 -> Target Portal 1
            MpioPathDescriptor p0{};
            p0.pathId = 0x00030001;
            p0.adapterIndex = 0;
            p0.busNumber = 1;
            p0.targetId = 0;
            p0.lunId = 1;
            p0.targetPortGroup = "TPG-ISCSI-01";
            p0.state = MpioPathState::ActiveOptimized;
            p0.weight = 100;
            p0.preferredPath = true;
            p0.sourceHbaAddress = "iqn.1991-05.com.micant:initiator01";
            p0.targetPortAddress = "iqn.2001-04.com.storage:target01-portal1";
            dev1.paths.push_back(p0);

            // Path 1: Secondary IP NIC 1 -> Target Portal 2 (Standby)
            MpioPathDescriptor p1{};
            p1.pathId = 0x00030002;
            p1.adapterIndex = 1;
            p1.busNumber = 1;
            p1.targetId = 0;
            p1.lunId = 1;
            p1.targetPortGroup = "TPG-ISCSI-02";
            p1.state = MpioPathState::Standby;
            p1.weight = 100;
            p1.preferredPath = false;
            p1.sourceHbaAddress = "iqn.1991-05.com.micant:initiator02";
            p1.targetPortAddress = "iqn.2001-04.com.storage:target01-portal2";
            dev1.paths.push_back(p1);

            devices_.push_back(dev1);
        }
    }

    // Hardware ID Claim Management
    bool isHardwareIdClaimed(std::string_view hwid) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return claimedHardwareIds_.find(std::string(hwid)) != claimedHardwareIds_.end();
    }

    bool claimHardwareId(std::string_view hwid) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (hwid.empty()) return false;
        return claimedHardwareIds_.insert(std::string(hwid)).second;
    }

    bool unclaimHardwareId(std::string_view hwid) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return claimedHardwareIds_.erase(std::string(hwid)) > 0;
    }

    std::vector<std::string> getClaimedHardwareIds() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return std::vector<std::string>(claimedHardwareIds_.begin(), claimedHardwareIds_.end());
    }

    // Subsystem Vitals
    MpioSubsystemVitals getVitals() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        MpioSubsystemVitals vitals{};
        vitals.claimedDevicesCount = static_cast<uint32_t>(devices_.size());
        vitals.pathVerifyPeriodSeconds = pathVerifyPeriodSeconds_;

        for (const auto& dev : devices_) {
            vitals.totalIosDispatched += dev.totalIosDispatched;
            vitals.totalFailoverEvents += dev.totalFailoverEvents;
            vitals.totalFailbackEvents += dev.totalFailbackEvents;
            for (const auto& p : dev.paths) {
                vitals.totalPathsCount++;
                if (p.state == MpioPathState::ActiveOptimized || p.state == MpioPathState::ActiveUnoptimized) {
                    vitals.activePathsCount++;
                } else if (p.state == MpioPathState::Standby) {
                    vitals.standbyPathsCount++;
                } else if (p.state == MpioPathState::Failed || p.state == MpioPathState::Degraded) {
                    vitals.failedPathsCount++;
                }
            }
        }
        return vitals;
    }

    // Device & Path Query
    uint32_t getDeviceCount() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return static_cast<uint32_t>(devices_.size());
    }

    bool getDevice(uint32_t diskIndex, MpioDeviceDescriptor& outDesc) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& dev : devices_) {
            if (dev.diskIndex == diskIndex) {
                outDesc = dev;
                return true;
            }
        }
        return false;
    }

    std::vector<MpioDeviceDescriptor> getAllDevices() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return devices_;
    }

    // Policy Configuration
    bool setLoadBalancePolicy(uint32_t diskIndex, MpioLoadBalancePolicy policy) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto& dev : devices_) {
            if (dev.diskIndex == diskIndex) {
                dev.policy = policy;
                // If policy is FailoverOnly, ensure only 1 active path and others standby
                if (policy == MpioLoadBalancePolicy::FailoverOnly) {
                    bool firstActive = false;
                    for (auto& p : dev.paths) {
                        if (p.state != MpioPathState::Failed && p.state != MpioPathState::Degraded) {
                            if (!firstActive) {
                                p.state = MpioPathState::ActiveOptimized;
                                firstActive = true;
                            } else {
                                p.state = MpioPathState::Standby;
                            }
                        }
                    }
                } else if (policy == MpioLoadBalancePolicy::RoundRobin ||
                           policy == MpioLoadBalancePolicy::LeastQueueDepth ||
                           policy == MpioLoadBalancePolicy::WeightedPaths ||
                           policy == MpioLoadBalancePolicy::LeastBlocks) {
                    // Restore standby paths to active if previously in standby
                    for (auto& p : dev.paths) {
                        if (p.state == MpioPathState::Standby) {
                            p.state = p.preferredPath ? MpioPathState::ActiveOptimized : MpioPathState::ActiveUnoptimized;
                        }
                    }
                }
                return true;
            }
        }
        return false;
    }

    // Path Selection Engine (Core DSM Routing Algorithm)
    uint32_t selectPathForIo(uint32_t diskIndex, uint32_t blockCount) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto& dev : devices_) {
            if (dev.diskIndex != diskIndex) continue;
            if (dev.paths.empty()) return 0;

            // Separate candidates by ALUA health priority
            std::vector<size_t> activeOptimizedIdx;
            std::vector<size_t> activeUnoptimizedIdx;
            std::vector<size_t> standbyIdx;

            for (size_t i = 0; i < dev.paths.size(); ++i) {
                if (dev.paths[i].state == MpioPathState::ActiveOptimized) {
                    activeOptimizedIdx.push_back(i);
                } else if (dev.paths[i].state == MpioPathState::ActiveUnoptimized) {
                    activeUnoptimizedIdx.push_back(i);
                } else if (dev.paths[i].state == MpioPathState::Standby) {
                    standbyIdx.push_back(i);
                }
            }

            // Pick candidate pool (Optimized > Unoptimized > Standby failover)
            const std::vector<size_t>* candidatePool = nullptr;
            if (!activeOptimizedIdx.empty()) {
                candidatePool = &activeOptimizedIdx;
            } else if (!activeUnoptimizedIdx.empty()) {
                candidatePool = &activeUnoptimizedIdx;
            } else if (!standbyIdx.empty()) {
                // Automatic failover to standby
                candidatePool = &standbyIdx;
                dev.paths[standbyIdx[0]].state = MpioPathState::ActiveOptimized;
                dev.totalFailoverEvents++;
            } else {
                return 0; // All paths down!
            }

            size_t chosenIdx = (*candidatePool)[0];

            switch (dev.policy) {
                case MpioLoadBalancePolicy::FailoverOnly: {
                    // Always route to first available healthy candidate
                    chosenIdx = (*candidatePool)[0];
                    break;
                }
                case MpioLoadBalancePolicy::RoundRobin: {
                    // Cyclic round-robin across candidates
                    dev.rrIndex++;
                    chosenIdx = (*candidatePool)[dev.rrIndex % candidatePool->size()];
                    break;
                }
                case MpioLoadBalancePolicy::RoundRobinWithSubset: {
                    // Round-robin restricted to preferred paths
                    std::vector<size_t> preferredSubset;
                    for (size_t idx : *candidatePool) {
                        if (dev.paths[idx].preferredPath) {
                            preferredSubset.push_back(idx);
                        }
                    }
                    if (!preferredSubset.empty()) {
                        dev.rrIndex++;
                        chosenIdx = preferredSubset[dev.rrIndex % preferredSubset.size()];
                    } else {
                        dev.rrIndex++;
                        chosenIdx = (*candidatePool)[dev.rrIndex % candidatePool->size()];
                    }
                    break;
                }
                case MpioLoadBalancePolicy::LeastQueueDepth: {
                    // Find path with lowest in-flight request count
                    uint32_t minQueue = UINT32_MAX;
                    for (size_t idx : *candidatePool) {
                        if (dev.paths[idx].queueDepth < minQueue) {
                            minQueue = dev.paths[idx].queueDepth;
                            chosenIdx = idx;
                        }
                    }
                    break;
                }
                case MpioLoadBalancePolicy::WeightedPaths: {
                    // Weighted random / highest score distribution
                    uint32_t maxWeight = 0;
                    for (size_t idx : *candidatePool) {
                        if (dev.paths[idx].weight > maxWeight) {
                            maxWeight = dev.paths[idx].weight;
                            chosenIdx = idx;
                        }
                    }
                    break;
                }
                case MpioLoadBalancePolicy::LeastBlocks: {
                    // Find path with lowest pending block volume
                    uint64_t minBlocks = UINT64_MAX;
                    for (size_t idx : *candidatePool) {
                        if (dev.paths[idx].pendingBlocks < minBlocks) {
                            minBlocks = dev.paths[idx].pendingBlocks;
                            chosenIdx = idx;
                        }
                    }
                    break;
                }
            }

            // Update path queue and pending block accounting
            dev.paths[chosenIdx].queueDepth++;
            dev.paths[chosenIdx].pendingBlocks += blockCount;
            dev.paths[chosenIdx].totalIoRequests++;
            dev.totalIosDispatched++;

            return dev.paths[chosenIdx].pathId;
        }
        return 0;
    }

    // Complete I/O Dispatch
    bool completeIo(uint32_t diskIndex, uint32_t pathId, uint32_t blockCount, bool success, uint32_t scsiStatus = 0) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto& dev : devices_) {
            if (dev.diskIndex != diskIndex) continue;
            for (auto& p : dev.paths) {
                if (p.pathId != pathId) continue;

                if (p.queueDepth > 0) p.queueDepth--;
                if (p.pendingBlocks >= blockCount) {
                    p.pendingBlocks -= blockCount;
                } else {
                    p.pendingBlocks = 0;
                }

                if (success) {
                    p.totalBytesTransferred += static_cast<uint64_t>(blockCount) * dev.blockSize;
                    p.consecutiveFailures = 0;
                } else {
                    p.totalIoErrors++;
                    p.consecutiveFailures++;
                    // If error threshold reached, fail the path automatically
                    if (p.consecutiveFailures >= 3 || scsiStatus == 0x02 /* Check Condition / Timeout */) {
                        p.state = MpioPathState::Failed;
                        dev.totalFailoverEvents++;
                    }
                }
                return true;
            }
        }
        return false;
    }

    // Fault Simulation & Path State Manipulation
    bool failPath(uint32_t diskIndex, uint32_t pathId, std::string_view reason = "Manual Link Down") {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto& dev : devices_) {
            if (dev.diskIndex != diskIndex) continue;
            for (auto& p : dev.paths) {
                if (p.pathId == pathId) {
                    if (p.state != MpioPathState::Failed) {
                        p.state = MpioPathState::Failed;
                        p.queueDepth = 0;
                        p.pendingBlocks = 0;
                        dev.totalFailoverEvents++;

                        // If FailoverOnly, promote next standby path
                        if (dev.policy == MpioLoadBalancePolicy::FailoverOnly) {
                            for (auto& standbyPath : dev.paths) {
                                if (standbyPath.state == MpioPathState::Standby) {
                                    standbyPath.state = MpioPathState::ActiveOptimized;
                                    break;
                                }
                            }
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }

    bool recoverPath(uint32_t diskIndex, uint32_t pathId) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto& dev : devices_) {
            if (dev.diskIndex != diskIndex) continue;
            for (auto& p : dev.paths) {
                if (p.pathId == pathId) {
                    if (p.state == MpioPathState::Failed || p.state == MpioPathState::Degraded) {
                        p.state = p.preferredPath ? MpioPathState::ActiveOptimized : MpioPathState::ActiveUnoptimized;
                        p.consecutiveFailures = 0;
                        dev.totalFailbackEvents++;
                    }
                    return true;
                }
            }
        }
        return false;
    }

    // Path Verification & Heartbeat Engine (SCSI Test Unit Ready - TUR Simulation)
    uint32_t executePathVerification() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint32_t verifiedPaths = 0;
        for (auto& dev : devices_) {
            for (auto& p : dev.paths) {
                if (p.state != MpioPathState::Failed) {
                    p.lastHeartbeatTick++;
                    verifiedPaths++;
                } else {
                    // If in failback stabilization, simulate heartbeat probing
                    p.consecutiveFailures++;
                    if (p.consecutiveFailures >= failbackStabilizationInterval_) {
                        // Recover path automatically
                        p.state = p.preferredPath ? MpioPathState::ActiveOptimized : MpioPathState::ActiveUnoptimized;
                        p.consecutiveFailures = 0;
                        dev.totalFailbackEvents++;
                        verifiedPaths++;
                    }
                }
            }
        }
        return verifiedPaths;
    }

    // SCSI-3 Persistent Reservation (PR) Operations
    bool registerReservationKey(uint32_t diskIndex, uint32_t pathId, uint64_t key) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto& dev : devices_) {
            if (dev.diskIndex != diskIndex) continue;
            if (key == 0) {
                // Key 0 unregisters
                dev.reservation.registeredKeys.erase(key);
            } else {
                dev.reservation.registeredKeys[key] = pathId;
            }
            return true;
        }
        return false;
    }

    bool acquireReservation(uint32_t diskIndex, uint32_t pathId, uint64_t key, ScsiPrType type) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto& dev : devices_) {
            if (dev.diskIndex != diskIndex) continue;
            // Key must be registered first
            if (dev.reservation.registeredKeys.find(key) == dev.reservation.registeredKeys.end()) {
                return false;
            }
            // If already reserved by another key, conflict
            if (dev.reservation.hasReservation && dev.reservation.reservationKey != key) {
                return false;
            }
            dev.reservation.hasReservation = true;
            dev.reservation.reservationKey = key;
            dev.reservation.reservationType = type;
            dev.reservation.reservationHolderPathId = pathId;
            return true;
        }
        return false;
    }

    bool releaseReservation(uint32_t diskIndex, uint32_t pathId, uint64_t key) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto& dev : devices_) {
            if (dev.diskIndex != diskIndex) continue;
            if (dev.reservation.hasReservation && dev.reservation.reservationKey == key) {
                dev.reservation.hasReservation = false;
                dev.reservation.reservationKey = 0;
                dev.reservation.reservationHolderPathId = 0;
                return true;
            }
        }
        return false;
    }

    bool clearReservation(uint32_t diskIndex, uint32_t pathId, uint64_t key) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto& dev : devices_) {
            if (dev.diskIndex != diskIndex) continue;
            // Key must be registered to issue CLEAR
            if (dev.reservation.registeredKeys.find(key) == dev.reservation.registeredKeys.end()) {
                return false;
            }
            dev.reservation.hasReservation = false;
            dev.reservation.reservationKey = 0;
            dev.reservation.reservationHolderPathId = 0;
            dev.reservation.registeredKeys.clear();
            return true;
        }
        return false;
    }

    bool getReservationStatus(uint32_t diskIndex, MpioScsiReservation& outRes) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& dev : devices_) {
            if (dev.diskIndex == diskIndex) {
                outRes = dev.reservation;
                return true;
            }
        }
        return false;
    }

private:
    MpioManager() {
        reset();
    }

    mutable std::shared_mutex mutex_;
    std::vector<MpioDeviceDescriptor> devices_;
    std::unordered_set<std::string> claimedHardwareIds_;
    uint32_t pathVerifyPeriodSeconds_{30};
    uint32_t failbackStabilizationInterval_{2};
};

// ============================================================================
// 4. Subsystem SCM & VersionDatabase Registration
// ============================================================================

inline void RegisterMpioSubsystem() {
    // 1. SCM Service: MPIO ("Multipath I/O Miniport Driver")
    auto& scm = scm::ServiceControlManager::get();
    auto mpioRecord = std::make_shared<scm::ServiceRecord>();
    mpioRecord->serviceName = L"MPIO";
    mpioRecord->displayName = L"Multipath I/O Miniport Driver";
    mpioRecord->serviceType = scm::SERVICE_KERNEL_DRIVER;
    mpioRecord->startType = scm::SERVICE_BOOT_START;
    mpioRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    mpioRecord->binaryPath = L"C:\\Windows\\System32\\drivers\\mpio.sys";
    mpioRecord->status.dwServiceType = mpioRecord->serviceType;
    mpioRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    mpioRecord->status.dwProcessId = 0;
    scm.registerServiceRecord(mpioRecord);

    // SCM Service: msdsm ("Device-Specific Module for Enterprise Multipath Storage")
    auto dsmRecord = std::make_shared<scm::ServiceRecord>();
    dsmRecord->serviceName = L"msdsm";
    dsmRecord->displayName = L"Device-Specific Module for Enterprise Multipath Storage";
    dsmRecord->serviceType = scm::SERVICE_KERNEL_DRIVER;
    dsmRecord->startType = scm::SERVICE_BOOT_START;
    dsmRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    dsmRecord->binaryPath = L"C:\\Windows\\System32\\drivers\\msdsm.sys";
    dsmRecord->status.dwServiceType = dsmRecord->serviceType;
    dsmRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    dsmRecord->status.dwProcessId = 0;
    scm.registerServiceRecord(dsmRecord);

    // 2. Version Database PE Module Registration (6 modules)
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("mpio.sys", "10.0.26100.1", "Multipath I/O Bus and Miniport Filter Driver");
    vdb.RegisterModule("msdsm.sys", "10.0.26100.1", "Device-Specific Module for Enterprise Multipath Storage");
    vdb.RegisterModule("mpioapi.dll", "10.0.26100.1", "Multipath I/O Management and Configuration API");
    vdb.RegisterModule("mpiocli.exe", "10.0.26100.1", "Multipath I/O Command Line Administration Tool");
    vdb.RegisterModule("mpiowmi.dll", "10.0.26100.1", "Multipath I/O WMI / CIM Provider Library");
    vdb.RegisterModule("mpiofilter.sys", "10.0.26100.1", "Multipath Storage Bus Filter Extension");

    // 3. Reset Manager to clean initial state
    MpioManager::get().reset();
}

} // namespace micant::mpio

// ============================================================================
// 5. Sovereign Win32 C ABI Exports (mpioapi.dll)
// ============================================================================

extern "C" {

inline uint32_t MpioApiGetDeviceCount() {
    return micant::mpio::MpioManager::get().getDeviceCount();
}

inline int32_t MpioApiSetLoadBalancePolicy(uint32_t diskIndex, uint32_t policy) {
    if (policy < 1 || policy > 6) return -1;
    bool ok = micant::mpio::MpioManager::get().setLoadBalancePolicy(
        diskIndex, static_cast<micant::mpio::MpioLoadBalancePolicy>(policy)
    );
    return ok ? 0 : -1;
}

inline int32_t MpioApiClaimHardwareId(const char* hwid) {
    if (!hwid) return -1;
    bool ok = micant::mpio::MpioManager::get().claimHardwareId(hwid);
    return ok ? 0 : -1;
}

inline int32_t MpioApiUnclaimHardwareId(const char* hwid) {
    if (!hwid) return -1;
    bool ok = micant::mpio::MpioManager::get().unclaimHardwareId(hwid);
    return ok ? 0 : -1;
}

inline int32_t MpioApiSimulatePathFailure(uint32_t diskIndex, uint32_t pathId) {
    bool ok = micant::mpio::MpioManager::get().failPath(diskIndex, pathId);
    return ok ? 0 : -1;
}

inline int32_t MpioApiSimulatePathRecovery(uint32_t diskIndex, uint32_t pathId) {
    bool ok = micant::mpio::MpioManager::get().recoverPath(diskIndex, pathId);
    return ok ? 0 : -1;
}

inline uint32_t MpioApiSelectPathForIo(uint32_t diskIndex, uint32_t blockCount) {
    return micant::mpio::MpioManager::get().selectPathForIo(diskIndex, blockCount);
}

inline int32_t MpioApiCompleteIo(uint32_t diskIndex, uint32_t pathId, uint32_t blockCount, int32_t success) {
    bool ok = micant::mpio::MpioManager::get().completeIo(diskIndex, pathId, blockCount, success != 0);
    return ok ? 0 : -1;
}

inline uint32_t MpioApiVerifyPaths() {
    return micant::mpio::MpioManager::get().executePathVerification();
}

} // extern "C"
