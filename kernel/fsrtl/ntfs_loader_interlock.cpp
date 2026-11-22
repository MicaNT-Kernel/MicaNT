// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Native Win32 Subsystem & Loader Interlock
// Horizon 368 / Suite 584 - LdrInitializeThunk, ntdll.dll Syscall Trampolines,
// PE32+ Section Mapping directly off NTFS Primary Storage
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Compatible with Dave Cutler's Windows NT architecture, win32metadata,
// and IFS driver architecture. Zero external dependencies.
// ============================================================================

#include "micant/km/drivers/fs/ntfs_loader_interlock.hpp"
#include <iostream>

namespace micant::ntfs::loader {

// ============================================================================
// 1. Synthetic PE32+ Executable Builder Implementation
// ============================================================================

std::vector<uint8_t> SyntheticPeBuilder::buildSyntheticAmd64Pe(
    std::string_view binaryName,
    uint64_t preferredBase,
    std::string_view importDll,
    const std::vector<std::string>& importedFunctions
) {
    (void)binaryName;
    std::vector<uint8_t> peBytes;
    peBytes.resize(0x1000); // Initial allocation for headers and sections

    // 1. DOS Header
    auto* dos = reinterpret_cast<pe::ImageDosHeader*>(peBytes.data());
    dos->e_magic = pe::IMAGE_DOS_SIGNATURE; // 'MZ'
    dos->e_lfanew = 0x80; // Offset to NT Headers

    // 2. NT Headers 64
    auto* nt = reinterpret_cast<pe::ImageNtHeaders64*>(peBytes.data() + dos->e_lfanew);
    nt->signature = pe::IMAGE_NT_SIGNATURE; // 'PE\0\0'

    // File Header
    nt->fileHeader.machine = pe::IMAGE_FILE_MACHINE_AMD64; // 0x8664
    nt->fileHeader.numberOfSections = 4; // .text, .rdata, .data, .pdata
    nt->fileHeader.timeDateStamp = 0x20261122;
    nt->fileHeader.pointerToSymbolTable = 0;
    nt->fileHeader.numberOfSymbols = 0;
    nt->fileHeader.sizeOfOptionalHeader = sizeof(pe::ImageOptionalHeader64);
    nt->fileHeader.characteristics = pe::IMAGE_FILE_EXECUTABLE_IMAGE | pe::IMAGE_FILE_LARGE_ADDRESS_AWARE;

    // Optional Header 64
    nt->optionalHeader.magic = pe::IMAGE_NT_OPTIONAL_HDR64_MAGIC; // 0x020B (PE32+)
    nt->optionalHeader.majorLinkerVersion = 14;
    nt->optionalHeader.minorLinkerVersion = 38;
    nt->optionalHeader.sizeOfCode = 0x1000;
    nt->optionalHeader.sizeOfInitializedData = 0x3000;
    nt->optionalHeader.sizeOfUninitializedData = 0;
    nt->optionalHeader.addressOfEntryPoint = 0x1000; // .text starts at RVA 0x1000
    nt->optionalHeader.baseOfCode = 0x1000;
    nt->optionalHeader.imageBase = preferredBase;
    nt->optionalHeader.sectionAlignment = 0x1000;
    nt->optionalHeader.fileAlignment = 0x200;
    nt->optionalHeader.majorOperatingSystemVersion = 10;
    nt->optionalHeader.minorOperatingSystemVersion = 0;
    nt->optionalHeader.majorImageVersion = 1;
    nt->optionalHeader.minorImageVersion = 0;
    nt->optionalHeader.majorSubsystemVersion = 10;
    nt->optionalHeader.minorSubsystemVersion = 0;
    nt->optionalHeader.win32VersionValue = 0;
    nt->optionalHeader.sizeOfImage = 0x5000; // Headers (0x1000) + 4 sections (0x1000 each)
    nt->optionalHeader.sizeOfHeaders = 0x400; // Aligned to fileAlignment (0x200 * 2)
    nt->optionalHeader.checkSum = 0;
    nt->optionalHeader.subsystem = pe::IMAGE_SUBSYSTEM_WINDOWS_CUI;
    nt->optionalHeader.dllCharacteristics = 0x8160; // HIGH_ENTROPY_VA | DYNAMIC_BASE | NX_COMPAT | TERMINAL_SERVER_AWARE
    nt->optionalHeader.sizeOfStackReserve = 0x100000; // 1 MB
    nt->optionalHeader.sizeOfStackCommit = 0x10000;   // 64 KB
    nt->optionalHeader.sizeOfHeapReserve = 0x100000;
    nt->optionalHeader.sizeOfHeapCommit = 0x10000;
    nt->optionalHeader.numberOfRvaAndSizes = pe::IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

    // Import Directory at RVA 0x2000 (.rdata)
    nt->optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_IMPORT].virtualAddress = 0x2000;
    nt->optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_IMPORT].size = sizeof(pe::ImageImportDescriptor) * 2;

    // IAT Directory at RVA 0x2100 (.rdata)
    nt->optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_IAT].virtualAddress = 0x2100;
    nt->optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_IAT].size = static_cast<uint32_t>((importedFunctions.size() + 1) * sizeof(uint64_t));

    // 3. Section Headers
    auto* sections = reinterpret_cast<pe::ImageSectionHeader*>(
        peBytes.data() + dos->e_lfanew + sizeof(uint32_t) + sizeof(pe::ImageFileHeader) + sizeof(pe::ImageOptionalHeader64)
    );

    // Section 1: .text (Code)
    std::memcpy(sections[0].name, ".text\0\0\0", 8);
    sections[0].misc.virtualSize = 0x1000;
    sections[0].virtualAddress = 0x1000;
    sections[0].sizeOfRawData = 0x200;
    sections[0].pointerToRawData = 0x400;
    sections[0].characteristics = pe::IMAGE_SCN_MEM_EXECUTE | pe::IMAGE_SCN_MEM_READ;

    // Section 2: .rdata (Read-Only Data & Imports)
    std::memcpy(sections[1].name, ".rdata\0\0", 8);
    sections[1].misc.virtualSize = 0x1000;
    sections[1].virtualAddress = 0x2000;
    sections[1].sizeOfRawData = 0x400;
    sections[1].pointerToRawData = 0x600;
    sections[1].characteristics = pe::IMAGE_SCN_MEM_READ;

    // Section 3: .data (Read-Write Data)
    std::memcpy(sections[2].name, ".data\0\0\0", 8);
    sections[2].misc.virtualSize = 0x1000;
    sections[2].virtualAddress = 0x3000;
    sections[2].sizeOfRawData = 0x200;
    sections[2].pointerToRawData = 0xA00;
    sections[2].characteristics = pe::IMAGE_SCN_MEM_READ | pe::IMAGE_SCN_MEM_WRITE;

    // Section 4: .pdata (Exception Handling)
    std::memcpy(sections[3].name, ".pdata\0\0", 8);
    sections[3].misc.virtualSize = 0x1000;
    sections[3].virtualAddress = 0x4000;
    sections[3].sizeOfRawData = 0x200;
    sections[3].pointerToRawData = 0xC00;
    sections[3].characteristics = pe::IMAGE_SCN_MEM_READ;

    // Resize file to encompass raw data of all sections (0xC00 + 0x200 = 0xE00)
    peBytes.resize(0xE00, 0);

    // Populate .text raw data (offset 0x400)
    // 48 31 C0 -> xor rax, rax; C3 -> ret
    uint8_t* textData = peBytes.data() + sections[0].pointerToRawData;
    textData[0] = 0x48;
    textData[1] = 0x31;
    textData[2] = 0xC0;
    textData[3] = 0xC3;

    // Populate .rdata raw data (offset 0x600): Imports layout
    // Offset 0x600 corresponds to RVA 0x2000
    // Import Descriptor (20 bytes) at offset 0x600
    auto* impDesc = reinterpret_cast<pe::ImageImportDescriptor*>(peBytes.data() + sections[1].pointerToRawData);
    impDesc->originalFirstThunk = 0x2080; // ILT RVA
    impDesc->timeDateStamp = 0;
    impDesc->forwarderChain = 0;
    impDesc->name = 0x2040; // DLL Name string RVA
    impDesc->firstThunk = 0x2100; // IAT RVA

    // DLL Name string at RVA 0x2040 (offset 0x640)
    uint8_t* dllNamePtr = peBytes.data() + sections[1].pointerToRawData + (0x2040 - 0x2000);
    std::memcpy(dllNamePtr, importDll.data(), std::min(importDll.size(), size_t(31)));
    dllNamePtr[std::min(importDll.size(), size_t(31))] = '\0';

    // ILT (Import Lookup Table) at RVA 0x2080 (offset 0x680)
    auto* ilt = reinterpret_cast<uint64_t*>(peBytes.data() + sections[1].pointerToRawData + (0x2080 - 0x2000));
    // IAT (Import Address Table) at RVA 0x2100 (offset 0x700)
    auto* iat = reinterpret_cast<uint64_t*>(peBytes.data() + sections[1].pointerToRawData + (0x2100 - 0x2000));

    // Name strings at RVA 0x2200 (offset 0x800)
    uint32_t nameOffsetRva = 0x2200;
    for (size_t i = 0; i < importedFunctions.size(); ++i) {
        ilt[i] = nameOffsetRva;
        iat[i] = nameOffsetRva; // Before binding, IAT points to ILT entry / name RVA

        auto* ibn = reinterpret_cast<pe::ImageImportByName*>(peBytes.data() + sections[1].pointerToRawData + (nameOffsetRva - 0x2000));
        ibn->hint = static_cast<uint16_t>(i);
        std::memcpy(ibn->name, importedFunctions[i].c_str(), importedFunctions[i].size() + 1);

        nameOffsetRva += static_cast<uint32_t>(sizeof(uint16_t) + importedFunctions[i].size() + 1);
        if (nameOffsetRva % 2 != 0) nameOffsetRva++; // Word alignment
    }
    ilt[importedFunctions.size()] = 0; // Terminating null
    iat[importedFunctions.size()] = 0;

    return peBytes;
}

// ============================================================================
// 2. SovereignNtfsLoaderEngine Implementation
// ============================================================================

NtStatus SovereignNtfsLoaderEngine::createSectionFromNtfs(
    std::wstring_view ntfsPath,
    std::span<const uint8_t> fileBytes,
    NtfsSectionObject& outSection
) noexcept {
    if (fileBytes.empty()) {
        return NtStatus::InvalidParameter;
    }

    pe::ImageNtHeaders64 headers{};
    std::vector<pe::ImageSectionHeader> sectionHeaders;

    NtStatus status = pe::PeLoader::inspect(fileBytes, headers, sectionHeaders);
    if (status != NtStatus::Success) {
        return status;
    }

    outSection.ntfsPath = std::wstring(ntfsPath);
    outSection.headers = headers;
    outSection.sectionHeaders = std::move(sectionHeaders);
    outSection.rawFileBytes.assign(fileBytes.begin(), fileBytes.end());
    outSection.preferredBase = headers.optionalHeader.imageBase;
    outSection.sizeOfImage = headers.optionalHeader.sizeOfImage;
    outSection.entryPointRva = headers.optionalHeader.addressOfEntryPoint;
    outSection.subsystem = headers.optionalHeader.subsystem;
    outSection.machine = headers.fileHeader.machine;
    outSection.sectionAlignment = headers.optionalHeader.sectionAlignment;
    outSection.fileAlignment = headers.optionalHeader.fileAlignment;
    outSection.isValid = true;
    pe::PeLoader::parseImports(fileBytes, headers, outSection.sectionHeaders, outSection.imports);

    return NtStatus::Success;
}

NtStatus SovereignNtfsLoaderEngine::mapViewOfSection(
    const NtfsSectionObject& section,
    MappedProcessImage& outMappedImage,
    uint64_t desiredBase
) noexcept {
    if (!section.isValid || section.sizeOfImage == 0) {
        return NtStatus::InvalidParameter;
    }

    uint64_t targetBase = (desiredBase != 0) ? desiredBase : section.preferredBase;
    outMappedImage.virtualBase = targetBase;
    outMappedImage.imageSize = section.sizeOfImage;
    outMappedImage.entryPointAddress = targetBase + section.entryPointRva;
    outMappedImage.mappedBuffer.clear();
    outMappedImage.mappedBuffer.resize(section.sizeOfImage, 0);
    outMappedImage.vads.clear();
    outMappedImage.relocationDelta = static_cast<int64_t>(targetBase - section.preferredBase);
    outMappedImage.isRelocated = (outMappedImage.relocationDelta != 0);
    outMappedImage.iatBound = false;
    outMappedImage.boundSymbolsCount = 0;

    // 1. Copy Headers
    uint32_t headerSize = std::min<uint32_t>(section.headers.optionalHeader.sizeOfHeaders, static_cast<uint32_t>(section.rawFileBytes.size()));
    if (headerSize > 0) {
        std::memcpy(outMappedImage.mappedBuffer.data(), section.rawFileBytes.data(), headerSize);
    }

    // 2. Map Each Section with Dave Cutler Memory Protections
    for (const auto& sec : section.sectionHeaders) {
        MappedSectionVad vad;
        vad.name = std::string(sec.getName());
        vad.virtualAddress = targetBase + sec.virtualAddress;
        vad.virtualSize = sec.misc.virtualSize ? sec.misc.virtualSize : sec.sizeOfRawData;
        vad.rawDataSize = sec.sizeOfRawData;
        vad.pointerToRawData = sec.pointerToRawData;
        vad.characteristics = sec.characteristics;

        // Compute Memory Protection
        if (sec.characteristics & pe::IMAGE_SCN_MEM_EXECUTE) {
            vad.protection = PAGE_EXECUTE_READ;
        } else if (sec.characteristics & pe::IMAGE_SCN_MEM_WRITE) {
            vad.protection = PAGE_READWRITE;
        } else {
            vad.protection = PAGE_READONLY;
        }

        if (sec.pointerToRawData + sec.sizeOfRawData <= section.rawFileBytes.size() &&
            sec.virtualAddress + sec.sizeOfRawData <= section.sizeOfImage) {
            std::memcpy(
                outMappedImage.mappedBuffer.data() + sec.virtualAddress,
                section.rawFileBytes.data() + sec.pointerToRawData,
                sec.sizeOfRawData
            );
        }

        outMappedImage.vads.push_back(std::move(vad));
    }

    return NtStatus::Success;
}

NtStatus SovereignNtfsLoaderEngine::initializeUserProcess(
    const NtfsSectionObject& section,
    std::wstring_view commandLine,
    NativeProcessContext& outProcess,
    uint64_t desiredBase
) noexcept {
    if (!section.isValid) {
        return NtStatus::InvalidParameter;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    outProcess.processId = ++m_nextProcessId;
    outProcess.threadId = ++m_nextThreadId;
    outProcess.imagePath = section.ntfsPath;
    outProcess.commandLine = std::wstring(commandLine);
    outProcess.imports = section.imports;

    // Map section view into virtual memory
    NtStatus status = mapViewOfSection(section, outProcess.image, desiredBase);
    if (status != NtStatus::Success) {
        return status;
    }

    // Initialize PEB
    outProcess.peb.imageBaseAddress = outProcess.image.virtualBase;
    outProcess.peb.numberOfProcessors = 8;
    outProcess.peb.ntGlobalFlag = 0;
    outProcess.peb.processHeap = 0x00007FF800200000ULL;

    // Initialize RTL_USER_PROCESS_PARAMETERS
    outProcess.processParams.imagePathName = UnicodeString(outProcess.imagePath.c_str());
    outProcess.processParams.commandLine = UnicodeString(outProcess.commandLine.c_str());
    outProcess.processParams.currentDirectory = UnicodeString(L"C:\\Windows\\System32");
    outProcess.peb.processParameters = reinterpret_cast<uint64_t>(&outProcess.processParams);

    // Initialize TEB
    outProcess.teb.ntTib.self = reinterpret_cast<uint64_t>(&outProcess.teb);
    outProcess.teb.ntTib.stackBase = outProcess.stackBase;
    outProcess.teb.ntTib.stackLimit = outProcess.stackLimit;
    outProcess.teb.processEnvironmentBlock = reinterpret_cast<uint64_t>(&outProcess.peb);

    // Thread Context setup: initial entry point is LdrInitializeThunk in ntdll.dll
    outProcess.rip = 0x00007FF800010000ULL; // ntdll!LdrInitializeThunk
    outProcess.rsp = outProcess.stackBase - 0x28; // Stack top 16-byte aligned with shadow space
    outProcess.rcx = reinterpret_cast<uint64_t>(&outProcess.peb);
    outProcess.rdx = 0; // ContextRecord
    outProcess.state = ProcessLifecycleState::ThreadsCreated;

    return NtStatus::Success;
}

NtStatus SovereignNtfsLoaderEngine::executeLdrInitializeThunk(
    NativeProcessContext& procCtx,
    const std::unordered_map<std::string, void*>& systemExports
) noexcept {
    if (procCtx.state != ProcessLifecycleState::ThreadsCreated) {
        return NtStatus::InvalidDeviceState;
    }

    // 1. Establish PEB Loader Data
    auto* ldrData = new ldr::PebLdrData();
    ldrData->initialize();
    procCtx.peb.ldr = reinterpret_cast<uint64_t>(ldrData);

    // 2. Register ntdll.dll as primary module
    auto ntdllEntry = std::make_shared<ldr::LdrDataTableEntry>();
    ntdllEntry->dllBase = 0x00007FF800000000ULL;
    ntdllEntry->entryPoint = 0x00007FF800010000ULL;
    ntdllEntry->sizeOfImage = 0x200000;
    ntdllEntry->storedFullName = L"C:\\Windows\\System32\\ntdll.dll";
    ntdllEntry->storedBaseName = L"ntdll.dll";
    ntdllEntry->flags = 0x00000004;
    ldrData->inLoadOrderModuleList.insertTail(&ntdllEntry->inLoadOrderLinks);

    // 3. Register Application Executable Module
    auto appEntry = std::make_shared<ldr::LdrDataTableEntry>();
    appEntry->dllBase = procCtx.image.virtualBase;
    appEntry->entryPoint = procCtx.image.entryPointAddress;
    appEntry->sizeOfImage = static_cast<uint32_t>(procCtx.image.imageSize);
    appEntry->storedFullName = procCtx.imagePath;
    appEntry->flags = 0x00000004;
    ldrData->inLoadOrderModuleList.insertTail(&appEntry->inLoadOrderLinks);

    // 4. Resolve Imports and Bind IAT
    for (const auto& lib : procCtx.imports) {
        for (const auto& sym : lib.symbols) {
            void* fnAddr = nullptr;
            if (!sym.name.empty()) {
                auto it = systemExports.find(sym.name);
                if (it != systemExports.end()) {
                    fnAddr = it->second;
                } else {
                    // Fallback simulated address
                    fnAddr = reinterpret_cast<void*>(0x00007FF800050000ULL + (procCtx.image.boundSymbolsCount * 0x100));
                }
            } else if (sym.isOrdinal) {
                fnAddr = reinterpret_cast<void*>(0x00007FF800050000ULL + sym.ordinal);
            }

            if (fnAddr && sym.iatRva + sizeof(uint64_t) <= procCtx.image.mappedBuffer.size()) {
                auto* iatSlot = reinterpret_cast<uint64_t*>(procCtx.image.mappedBuffer.data() + sym.iatRva);
                *iatSlot = reinterpret_cast<uint64_t>(fnAddr);
                procCtx.image.boundSymbolsCount++;
            }
        }
    }

    procCtx.image.iatBound = true;

    // 5. Transfer execution control to the application entry point
    procCtx.rip = procCtx.image.entryPointAddress;
    procCtx.state = ProcessLifecycleState::ActiveRunning;

    return NtStatus::Success;
}

NtStatus SovereignNtfsLoaderEngine::terminateProcess(
    NativeProcessContext& procCtx,
    uint32_t exitCode
) noexcept {
    procCtx.exitCode = exitCode;
    procCtx.state = ProcessLifecycleState::Terminated;
    return NtStatus::Success;
}

// ============================================================================
// 3. Verification Suite (Suite 584)
// ============================================================================

bool VerifyHorizon368NtfsLoaderSubsystem() noexcept {
    std::cout << "\n[TEST] Running Suite 584: Horizon 368 - Sovereign MicaNT NTFS Native Win32 Subsystem & Loader Interlock...\n";

    // ------------------------------------------------------------------------
    // Pillar 1: PE32+ Executable Parsing & Integrity on NTFS Primary Storage
    // ------------------------------------------------------------------------
    auto peBytes = SyntheticPeBuilder::buildSyntheticAmd64Pe(
        "smss.exe",
        0x0000000140000000ULL,
        "ntdll.dll",
        {"NtTerminateProcess", "NtWriteFile"}
    );

    if (peBytes.size() < 0xE00) {
        std::cerr << "  [-] Step 1 Failed: Synthetic PE size is insufficient.\n";
        return false;
    }

    NtfsSectionObject secObj;
    NtStatus createStatus = SovereignNtfsLoaderEngine::Instance().createSectionFromNtfs(
        L"\\Device\\HarddiskVolume1\\Windows\\System32\\smss.exe",
        peBytes,
        secObj
    );

    if (createStatus != NtStatus::Success || !secObj.isValid) {
        std::cerr << "  [-] Step 2 Failed: createSectionFromNtfs failed.\n";
        return false;
    }

    if (secObj.machine != pe::IMAGE_FILE_MACHINE_AMD64 ||
        secObj.headers.optionalHeader.magic != pe::IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        secObj.entryPointRva != 0x1000 ||
        secObj.sizeOfImage != 0x5000 ||
        secObj.sectionHeaders.size() != 4) {
        std::cerr << "  [-] Step 3 Failed: PE headers validation failed.\n";
        return false;
    }

    std::cout << "  Horizon 368 Architectural Validation Complete:\n";
    std::cout << "    Pillar 1: PE32+ Executable Parsing & NTFS Section Creation OK\n";

    // ------------------------------------------------------------------------
    // Pillar 2: Direct PE32+ Section Object Mapping into Process Memory
    // ------------------------------------------------------------------------
    MappedProcessImage mappedImage;
    NtStatus mapStatus = SovereignNtfsLoaderEngine::Instance().mapViewOfSection(secObj, mappedImage);
    if (mapStatus != NtStatus::Success || mappedImage.mappedBuffer.size() != 0x5000) {
        std::cerr << "  [-] Step 4 Failed: mapViewOfSection failed.\n";
        return false;
    }

    if (mappedImage.vads.size() != 4) {
        std::cerr << "  [-] Step 5 Failed: Mapped VAD count mismatch.\n";
        return false;
    }

    // Verify VAD Memory Protections
    bool textOk = false, rdataOk = false, dataOk = false, pdataOk = false;
    for (const auto& vad : mappedImage.vads) {
        if (vad.name == ".text" && vad.protection == PAGE_EXECUTE_READ && vad.isExecutable() && !vad.isWritable()) textOk = true;
        if (vad.name == ".rdata" && vad.protection == PAGE_READONLY && !vad.isExecutable() && !vad.isWritable()) rdataOk = true;
        if (vad.name == ".data" && vad.protection == PAGE_READWRITE && !vad.isExecutable() && vad.isWritable()) dataOk = true;
        if (vad.name == ".pdata" && vad.protection == PAGE_READONLY && !vad.isExecutable() && !vad.isWritable()) pdataOk = true;
    }

    if (!textOk || !rdataOk || !dataOk || !pdataOk) {
        std::cerr << "  [-] Step 6 Failed: Section memory protections / VAD verification failed.\n";
        return false;
    }

    // Test Relocated Base Mapping
    MappedProcessImage rebasedImage;
    NtStatus rebaseStatus = SovereignNtfsLoaderEngine::Instance().mapViewOfSection(secObj, rebasedImage, 0x0000000240000000ULL);
    if (rebaseStatus != NtStatus::Success || !rebasedImage.isRelocated || rebasedImage.relocationDelta != 0x0000000100000000ULL) {
        std::cerr << "  [-] Step 7 Failed: Rebased image mapping calculation failed.\n";
        return false;
    }

    std::cout << "    Pillar 2: Direct PE32+ Section Mapping (MmMapViewOfSection) OK\n";

    // ------------------------------------------------------------------------
    // Pillar 3: Native Win32 Subsystem Process Initializer (NtCreateUserProcess)
    // ------------------------------------------------------------------------
    NativeProcessContext procCtx;
    NtStatus initProcStatus = SovereignNtfsLoaderEngine::Instance().initializeUserProcess(
        secObj,
        L"smss.exe -boot",
        procCtx
    );

    if (initProcStatus != NtStatus::Success || procCtx.processId == 0 || procCtx.threadId == 0) {
        std::cerr << "  [-] Step 8 Failed: initializeUserProcess failed.\n";
        return false;
    }

    if (procCtx.peb.imageBaseAddress != 0x0000000140000000ULL ||
        procCtx.peb.processParameters == 0 ||
        procCtx.teb.ntTib.self != reinterpret_cast<uint64_t>(&procCtx.teb) ||
        procCtx.teb.processEnvironmentBlock != reinterpret_cast<uint64_t>(&procCtx.peb)) {
        std::cerr << "  [-] Step 9 Failed: PEB/TEB linkage verification failed.\n";
        return false;
    }

    if (procCtx.rip != 0x00007FF800010000ULL || procCtx.state != ProcessLifecycleState::ThreadsCreated) {
        std::cerr << "  [-] Step 10 Failed: Initial thread context setup failed.\n";
        return false;
    }

    std::cout << "    Pillar 3: Native Process & Initial Thread Creation (NtCreateUserProcess) OK\n";

    // ------------------------------------------------------------------------
    // Pillar 4: Sovereign ntdll.dll Fast Syscall Trampoline Generation
    // ------------------------------------------------------------------------
    SovereignSyscallEngine::Instance().initialize();
    if (SovereignSyscallEngine::Instance().getRegisteredCount() < 12) {
        std::cerr << "  [-] Step 11 Failed: SovereignSyscallEngine registration count low.\n";
        return false;
    }

    const auto* stubUserProc = SovereignSyscallEngine::Instance().getStub(SSN_NtCreateUserProcess);
    const auto* stubTermProc = SovereignSyscallEngine::Instance().getStubByName("NtTerminateProcess");
    const auto* stubMapView  = SovereignSyscallEngine::Instance().getStub(SSN_NtMapViewOfSection);

    if (!stubUserProc || !stubTermProc || !stubMapView) {
        std::cerr << "  [-] Step 12 Failed: Syscall stub lookup failed.\n";
        return false;
    }

    if (!stubUserProc->verifyEncoding() || !stubTermProc->verifyEncoding() || !stubMapView->verifyEncoding()) {
        std::cerr << "  [-] Step 13 Failed: x64 11-byte syscall stub opcode verification failed.\n";
        return false;
    }

    // Execute through software syscall bridge
    NtStatus callStatus = SovereignSyscallEngine::Instance().executeSyscall(SSN_NtMapViewOfSection, 0, 0, 0, 0);
    if (callStatus != NtStatus::Success || SovereignSyscallEngine::Instance().getDispatchCount() == 0) {
        std::cerr << "  [-] Step 14 Failed: Syscall dispatch execution failed.\n";
        return false;
    }

    std::cout << "    Pillar 4: Sovereign ntdll.dll Fast Syscall Trampolines (11-byte x64) OK\n";

    // ------------------------------------------------------------------------
    // Pillar 5: LdrInitializeThunk Interlock & IAT Binding
    // ------------------------------------------------------------------------
    std::unordered_map<std::string, void*> systemExports = {
        {"NtTerminateProcess", reinterpret_cast<void*>(0x00007FF800051234ULL)},
        {"NtWriteFile",        reinterpret_cast<void*>(0x00007FF800055678ULL)}
    };

    NtStatus ldrStatus = SovereignNtfsLoaderEngine::Instance().executeLdrInitializeThunk(procCtx, systemExports);
    if (ldrStatus != NtStatus::Success || !procCtx.image.iatBound || procCtx.image.boundSymbolsCount < 2) {
        std::cerr << "  [-] Step 15 Failed: executeLdrInitializeThunk / IAT binding failed.\n";
        return false;
    }

    // Verify IAT memory slots in mapped image
    auto* iatBase = reinterpret_cast<const uint64_t*>(procCtx.image.mappedBuffer.data() + 0x2100);
    if (iatBase[0] != 0x00007FF800051234ULL || iatBase[1] != 0x00007FF800055678ULL) {
        std::cerr << "  [-] Step 16 Failed: Patched IAT function pointer mismatch.\n";
        return false;
    }

    // Verify execution transfer to Application Entry Point
    if (procCtx.rip != 0x0000000140001000ULL || procCtx.state != ProcessLifecycleState::ActiveRunning) {
        std::cerr << "  [-] Step 17 Failed: Execution transfer to EntryPoint failed.\n";
        return false;
    }

    // Terminate process
    SovereignNtfsLoaderEngine::Instance().terminateProcess(procCtx, 0);
    if (procCtx.state != ProcessLifecycleState::Terminated || procCtx.exitCode != 0) {
        std::cerr << "  [-] Step 18 Failed: terminateProcess failed.\n";
        return false;
    }

    std::cout << "    Pillar 5: LdrInitializeThunk Interlock & IAT Binding OK\n";

    // ------------------------------------------------------------------------
    // Pillar 6: High-Throughput Native Loader & Syscall Benchmark
    // ------------------------------------------------------------------------
    constexpr size_t benchmarkIterations = 100;
    auto tStart = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < benchmarkIterations; ++i) {
        NativeProcessContext benchProc;
        SovereignNtfsLoaderEngine::Instance().initializeUserProcess(secObj, L"benchmark.exe", benchProc);
        SovereignNtfsLoaderEngine::Instance().executeLdrInitializeThunk(benchProc, systemExports);
        SovereignSyscallEngine::Instance().executeSyscall(SSN_NtTerminateProcess, 0, 0, 0, 0);
        SovereignNtfsLoaderEngine::Instance().terminateProcess(benchProc, 0);
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    double totalUs = std::chrono::duration<double, std::micro>(tEnd - tStart).count();
    double avgUs = totalUs / static_cast<double>(benchmarkIterations);

    if (avgUs > 150.0) {
        std::cerr << "  [-] Step 19 Failed: Average process load/init latency exceeded (" << avgUs << " us).\n";
        return false;
    }

    std::cout << "    Pillar 6: High-Throughput Native Loader Benchmark (" << avgUs << " us/process) OK\n";
    std::cout << "[TEST] Suite 584: Horizon 368 - Sovereign MicaNT NTFS Native Win32 Subsystem & Loader PASSED.\n";

    return true;
}

} // namespace micant::ntfs::loader
