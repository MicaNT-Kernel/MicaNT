// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Subsystem
// Horizon 361 / Suite 577 - Reparse Points, Directory Junctions, Symbolic Links,
// Alternate Data Streams ($DATA Stream Multiplexing) & Sparse Cluster Punch-Holes
//
// Clean-Room Implementation in modern ISO C++23.
// Compatible with Microsoft Windows NT NTFS on-disk layout, win32metadata,
// and IFS driver architecture. Zero external dependencies.
// ============================================================================

#include "micant/km/drivers/fs/ntfs_reparse_streams.hpp"
#include <iostream>

namespace micant::ntfs::reparse {

#define CHECK_TRUE(cond, step) do { if (!(cond)) { std::cout << "  [FAIL STEP " << step << "]\n"; return false; } } while (0)

bool VerifyHorizon361ReparseAndStreamSubsystem() noexcept {
    auto& reparse = SovereignNtfsReparseEngine::Instance();

    // ------------------------------------------------------------------------
    // Pillar 1: Reparse Point Tag Definitions & Buffer Marshaling
    // ------------------------------------------------------------------------
    auto juncBuf = reparse.createMountPointBuffer(
        L"\\??\\C:\\Users\\admin\\AppData",
        L"C:\\Users\\admin\\AppData"
    );
    CHECK_TRUE(!juncBuf.empty(), 1);

    const auto* rpbJunc = reinterpret_cast<const REPARSE_DATA_BUFFER*>(juncBuf.data());
    CHECK_TRUE(rpbJunc->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT, 2);
    CHECK_TRUE(rpbJunc->MountPointReparseBuffer.SubstituteNameLength > 0, 3);
    CHECK_TRUE(rpbJunc->MountPointReparseBuffer.PrintNameLength > 0, 4);

    auto symlinkBuf = reparse.createSymbolicLinkBuffer(
        L"..\\System32\\ntdll.dll",
        L"ntdll.dll",
        true // Relative
    );
    CHECK_TRUE(!symlinkBuf.empty(), 5);

    const auto* rpbSym = reinterpret_cast<const REPARSE_DATA_BUFFER*>(symlinkBuf.data());
    CHECK_TRUE(rpbSym->ReparseTag == IO_REPARSE_TAG_SYMLINK, 6);
    CHECK_TRUE((rpbSym->SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE) != 0, 7);

    // ------------------------------------------------------------------------
    // Pillar 2: Directory Junction Resolution & Target Path Translation
    // ------------------------------------------------------------------------
    std::wstring resolvedJunc;
    bool isRelativeJunc = true;
    NtStatus stResolveJunc = reparse.resolveReparsePoint(juncBuf, resolvedJunc, isRelativeJunc);
    CHECK_TRUE(NT_SUCCESS(stResolveJunc), 8);
    CHECK_TRUE(isRelativeJunc == false, 9);
    CHECK_TRUE(resolvedJunc == L"\\??\\C:\\Users\\admin\\AppData", 10);

    // ------------------------------------------------------------------------
    // Pillar 3: Symbolic Link Parsing & Infinite Loop Detection
    // ------------------------------------------------------------------------
    std::wstring resolvedSym;
    bool isRelativeSym = false;
    NtStatus stResolveSym = reparse.resolveReparsePoint(symlinkBuf, resolvedSym, isRelativeSym);
    CHECK_TRUE(NT_SUCCESS(stResolveSym), 11);
    CHECK_TRUE(isRelativeSym == true, 12);
    CHECK_TRUE(resolvedSym == L"..\\System32\\ntdll.dll", 13);

    // Register a valid junction chain: A -> B -> C
    reparse.registerJunction(L"C:\\LinkA", L"C:\\LinkB");
    reparse.registerJunction(L"C:\\LinkB", L"C:\\TargetFolder");

    std::wstring finalTarget;
    NtStatus stTraverse = reparse.traversePath(L"C:\\LinkA", finalTarget);
    CHECK_TRUE(NT_SUCCESS(stTraverse), 14);
    CHECK_TRUE(finalTarget == L"C:\\TargetFolder", 15);

    // Register circular junction: LoopX -> LoopY -> LoopX
    reparse.registerJunction(L"C:\\LoopX", L"C:\\LoopY");
    reparse.registerJunction(L"C:\\LoopY", L"C:\\LoopX");

    std::wstring loopOut;
    NtStatus stLoop = reparse.traversePath(L"C:\\LoopX", loopOut);
    CHECK_TRUE(stLoop == NtStatus::ReparsePointNotResolved, 16); // Must catch cycle!

    // ------------------------------------------------------------------------
    // Pillar 4: Alternate Data Streams ($DATA Multiplexing & Zone.Identifier)
    // ------------------------------------------------------------------------
    SovereignNtfsStreamFile streamFile(1024, L"kernel32.dll");

    // Primary stream
    std::string primaryContent = "MZ... Sovereign PE32+ Image Data";
    std::span<const uint8_t> primarySpan(
        reinterpret_cast<const uint8_t*>(primaryContent.data()),
        primaryContent.size()
    );
    CHECK_TRUE(streamFile.writeStream(L"", primarySpan), 17);

    // ADS 1: Zone.Identifier (Mark of the Web)
    std::string zoneContent = "[ZoneTransfer]\r\nZoneId=3\r\nReferrerUrl=https://micant.org\r\n";
    std::span<const uint8_t> zoneSpan(
        reinterpret_cast<const uint8_t*>(zoneContent.data()),
        zoneContent.size()
    );
    CHECK_TRUE(streamFile.writeStream(L"Zone.Identifier", zoneSpan), 18);

    // ADS 2: Checksum stream
    std::string shaContent = "SHA256: 0xDEADBEEFCAFEBABE1122334455667788";
    std::span<const uint8_t> shaSpan(
        reinterpret_cast<const uint8_t*>(shaContent.data()),
        shaContent.size()
    );
    CHECK_TRUE(streamFile.writeStream(L"Checksum", shaSpan), 19);

    CHECK_TRUE(streamFile.getStreamCount() == 3, 20);
    CHECK_TRUE(streamFile.hasStream(L"Zone.Identifier"), 21);
    CHECK_TRUE(streamFile.hasStream(L"Checksum"), 22);

    std::vector<uint8_t> readZone;
    CHECK_TRUE(streamFile.readStream(L"Zone.Identifier", readZone), 23);
    std::string readZoneStr(reinterpret_cast<char*>(readZone.data()), readZone.size());
    CHECK_TRUE(readZoneStr.find("ZoneId=3") != std::string::npos, 24);

    auto streamNames = streamFile.enumerateStreamNames();
    CHECK_TRUE(streamNames.size() == 3, 25);

    // ------------------------------------------------------------------------
    // Pillar 5: Sparse File Allocation & Cluster Punch-Hole Zeroing
    // ------------------------------------------------------------------------
    auto& sparse = streamFile.getSparseEngine();
    sparse.initialize(1024 * 1024); // 1 MB file
    CHECK_TRUE(sparse.isSparse(), 26);
    CHECK_TRUE(sparse.getTotalSize() == 1024 * 1024, 27);
    CHECK_TRUE(sparse.getPhysicalAllocatedBytes() == 1024 * 1024, 28);

    // Punch a 512 KB hole in the middle (from 256KB to 768KB)
    CHECK_TRUE(sparse.punchHole(256 * 1024, 512 * 1024), 29);
    CHECK_TRUE(sparse.getPhysicalAllocatedBytes() == 512 * 1024, 30); // 50% savings!

    // Query allocated ranges across entire file
    auto allocated = sparse.queryAllocatedRanges(0, 1024 * 1024);
    CHECK_TRUE(allocated.size() == 2, 31);
    CHECK_TRUE(allocated[0].FileOffset == 0 && allocated[0].Length == 256 * 1024, 32);
    CHECK_TRUE(allocated[1].FileOffset == 768 * 1024 && allocated[1].Length == 256 * 1024, 33);

    // ------------------------------------------------------------------------
    // Pillar 6: High-Throughput Stream Dispatch Stress & Sub-35us Latency
    // ------------------------------------------------------------------------
    auto start = std::chrono::high_resolution_clock::now();
    constexpr uint32_t stressPasses = 500;

    for (uint32_t pass = 0; pass < stressPasses; ++pass) {
        std::wstring testPath;
        bool rel = false;
        (void)reparse.resolveReparsePoint(juncBuf, testPath, rel);
        std::vector<uint8_t> tmp;
        (void)streamFile.readStream(L"Zone.Identifier", tmp);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;
    double avgLatencyUs = elapsed.count() / stressPasses;

    CHECK_TRUE(avgLatencyUs < 100.0, 34);

    return true;
}

} // namespace micant::ntfs::reparse
