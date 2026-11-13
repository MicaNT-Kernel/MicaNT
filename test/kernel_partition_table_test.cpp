#include "micant/km/drivers/storage/partition_table.hpp"

namespace {
using namespace micant::storage::partition_table;
constexpr size_t DiskSectors = 100;
uint8_t disk[DiskSectors][512]{};
uint8_t scratch[8192]{};
bool readDisk(void* context, uint64_t lba, uint16_t count, void* destination) noexcept {
    auto* sectors = static_cast<uint8_t (*)[512]>(context);
    if (!destination || !count || lba >= DiskSectors || count > DiskSectors - lba) return false;
    auto* out = static_cast<uint8_t*>(destination);
    for (uint16_t s = 0; s < count; ++s)
        for (size_t b = 0; b < 512; ++b) out[size_t(s) * 512 + b] = sectors[lba + s][b];
    return true;
}
void put32(uint8_t* p, uint32_t v) noexcept { for (unsigned i=0;i<4;++i) p[i]=uint8_t(v>>(i*8)); }
void put64(uint8_t* p, uint64_t v) noexcept { for (unsigned i=0;i<8;++i) p[i]=uint8_t(v>>(i*8)); }
uint32_t crc(const uint8_t* p, size_t n) noexcept { return crc32Update(0xFFFFFFFFu,p,n)^0xFFFFFFFFu; }
void buildHeader(size_t lba, uint64_t backup, uint64_t arrayLba, const uint8_t* entries) noexcept {
    uint8_t* h=disk[lba]; h[0]='E';h[1]='F';h[2]='I';h[3]=' ';h[4]='P';h[5]='A';h[6]='R';h[7]='T';
    put32(h+8,0x00010000);put32(h+12,92);put64(h+24,lba);put64(h+32,backup);
    put64(h+40,34);put64(h+48,97);h[56]=0x41;put64(h+72,arrayLba);put32(h+80,4);put32(h+84,128);put32(h+88,crc(entries,512));
    put32(h+16,0);put32(h+16,crc(h,92));
}
void refreshHeader(size_t headerLba, size_t arrayLba) noexcept {
    uint8_t* h=disk[headerLba];put32(h+88,crc(disk[arrayLba],512));put32(h+16,0);put32(h+16,crc(h,92));
}
void refreshHeaderCrc(size_t headerLba) noexcept {
    uint8_t* h=disk[headerLba];put32(h+16,0);put32(h+16,crc(h,92));
}
void buildGpt() noexcept {
    for (size_t s=0;s<DiskSectors;++s) for(size_t b=0;b<512;++b) disk[s][b]=0;
    auto* m=disk[0];m[510]=0x55;m[511]=0xAA;m[446+4]=0xEE;put32(m+446+8,1);put32(m+446+12,99);
    for(size_t i=0;i<16;++i)disk[2][i]=uint8_t(i+1);
    put64(disk[2]+32,40);put64(disk[2]+40,49);
    for(size_t i=0;i<512;++i)disk[98][i]=disk[2][i];
    buildHeader(1,99,2,disk[2]);buildHeader(99,1,98,disk[98]);
}
bool isStatus(Status actual, Status expected) noexcept { return actual==expected; }
}

extern "C" bool MicantTestPartitionTables() noexcept {
    const Device source{disk,&readDisk,DiskSectors,512,scratch,sizeof(scratch)}; Extent entries[8]{}; Result result{};
    for(size_t s=0;s<DiskSectors;++s)for(size_t b=0;b<512;++b)disk[s][b]=0;
    disk[0][510]=0x55;disk[0][511]=0xAA;
    uint8_t* m=disk[0]+446;m[4]=0x0C;put32(m+8,10);put32(m+12,20);
    if(!isStatus(parseMbr(source,entries,8,result),Status::Success)||result.kind!=TableKind::Mbr||result.count!=1||entries[0].firstLba!=10||entries[0].sectorCount!=20)return false;
    put32(m+8,UINT32_MAX-2);put32(m+12,10);
    if(!isStatus(parseMbr(source,entries,8,result),Status::Corrupt))return false;
    for(size_t s=0;s<DiskSectors;++s)for(size_t b=0;b<512;++b)disk[s][b]=0;
    disk[0][510]=0x55;disk[0][511]=0xAA;m=disk[0]+446;m[4]=0x0F;put32(m+8,30);put32(m+12,50);
    disk[30][510]=0x55;disk[30][511]=0xAA;disk[30][450]=0x0C;put32(disk[30]+454,1);put32(disk[30]+458,10);
    if(!isStatus(parseMbr(source,entries,8,result),Status::Success)||result.count!=1||entries[0].firstLba!=31)return false;
    disk[30][466]=0x0F;put32(disk[30]+470,20);put32(disk[30]+474,30);
    disk[50][510]=0x55;disk[50][511]=0xAA;disk[50][450]=0x0C;put32(disk[50]+454,1);put32(disk[50]+458,5);
    disk[50][466]=0x0F;put32(disk[50]+470,20);put32(disk[50]+474,30);
    if(!isStatus(parseMbr(source,entries,8,result),Status::Corrupt))return false; // repeated EBR is rejected
    buildGpt();
    if(!isStatus(discover(source,entries,8,result),Status::Success)||result.kind!=TableKind::Gpt||result.count!=1||entries[0].firstLba!=40||entries[0].sectorCount!=10)return false;
    // Damaged primary header must recover from the validated backup copy.
    disk[1][16]^=1;
    if(!isStatus(discover(source,entries,8,result),Status::Success)||result.kind!=TableKind::Gpt||result.count!=1)return false;
    disk[0][510]=0;
    if(!isStatus(discover(source,entries,8,result),Status::Success)||result.kind!=TableKind::Gpt||result.count!=1)return false;
    buildGpt();disk[2][0]^=0x80;
    if(!isStatus(discover(source,entries,8,result),Status::Success)||result.kind!=TableKind::Gpt||result.count!=1)return false; // backup remains sound
    disk[98][0]^=0x80;
    if(!isStatus(discover(source,entries,8,result),Status::Corrupt))return false;
    buildGpt();put64(disk[2]+32,UINT64_MAX);put64(disk[98]+32,UINT64_MAX);
    refreshHeader(1,2);refreshHeader(99,98);
    if(!isStatus(discover(source,entries,8,result),Status::Corrupt))return false;
    buildGpt();put64(disk[1]+72,UINT64_MAX);put64(disk[99]+72,UINT64_MAX);
    refreshHeaderCrc(1);refreshHeaderCrc(99);
    if(!isStatus(discover(source,entries,8,result),Status::Corrupt))return false;
    const Device tiny{disk,&readDisk,1,512,scratch,sizeof(scratch)};
    return isStatus(discover(tiny,entries,8,result),Status::InvalidArgument);
}
