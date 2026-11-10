#include <cassert>
#include <memory>
#include <vector>

#include "micant/km/drivers/storage/storage.hpp"

int main() {
    using namespace micant;
    using namespace micant::storage;

    RamDiskDevice disk(L"\\Device\\PartMgrTest", 2 * 1024 * 1024);
    std::vector<MbrPartitionEntry> entries(1);
    entries[0].bootIndicator = 0x80;
    entries[0].partitionType = MBR_TYPE_FAT32_LBA;
    entries[0].startLba = 8;
    entries[0].sectorCount = 100;
    assert(PartitionManager::writeMbr(disk, entries) == NtStatus::Success);

    std::vector<MbrPartitionEntry> discovered;
    assert(PartitionManager::parseMbr(disk, discovered) == NtStatus::Success);
    assert(discovered.size() == 1);
    assert(discovered[0].startLba == entries[0].startLba);
    assert(discovered[0].sectorCount == entries[0].sectorCount);

    entries[0].startLba = static_cast<uint32_t>(disk.getTotalBlocks() - 1);
    entries[0].sectorCount = 2;
    assert(PartitionManager::writeMbr(disk, entries) == NtStatus::Success);
    assert(PartitionManager::parseMbr(disk, discovered) == NtStatus::UnrecognizedVolume);
    assert(discovered.empty());

    entries.resize(2);
    entries[0].startLba = 8;
    entries[0].sectorCount = 100;
    entries[1] = entries[0];
    entries[1].bootIndicator = 0;
    entries[1].startLba = 50;
    assert(PartitionManager::writeMbr(disk, entries) == NtStatus::Success);
    assert(PartitionManager::parseMbr(disk, discovered) == NtStatus::UnrecognizedVolume);
    assert(discovered.empty());

    return 0;
}
