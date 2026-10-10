#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

#include "ntstatus.hpp"
#include "scm.hpp"
#include "version.hpp"

namespace micant::wds {

// ============================================================================
// Windows Deployment Services (WDS) & PXE Boot Enums & Constants
// ============================================================================

// Client Architecture Types (RFC 4578 / DHCP Option 93)
enum class ClientArchitecture : uint16_t {
    Intelx86PC      = 0,  // Legacy BIOS x86
    NEC_PC98        = 1,
    Itanium         = 2,
    DEC_Alpha       = 3,
    Arcx86          = 4,
    IntelLeanClient = 5,
    EFI_IA32        = 6,  // UEFI x86
    EFI_x86_64      = 7,  // UEFI x64 (Standard 64-bit PC)
    EFI_Xscale      = 8,
    EFI_BC          = 9,  // EFI Byte Code (EBC)
    EFI_ARM32       = 10, // UEFI ARM32
    EFI_ARM64       = 11  // UEFI ARM64 / AArch64
};

inline const char* ClientArchitectureToString(ClientArchitecture arch) {
    switch (arch) {
        case ClientArchitecture::Intelx86PC: return "Intel_x86_BIOS";
        case ClientArchitecture::EFI_IA32:   return "UEFI_x86";
        case ClientArchitecture::EFI_x86_64: return "UEFI_x64";
        case ClientArchitecture::EFI_BC:     return "EFI_ByteCode";
        case ClientArchitecture::EFI_ARM32:  return "UEFI_ARM32";
        case ClientArchitecture::EFI_ARM64:  return "UEFI_ARM64";
        default: return "Unknown";
    }
}

// TFTP OpCodes (RFC 1350 / RFC 2347)
enum class TftpOpCode : uint16_t {
    RRQ   = 1, // Read Request
    WRQ   = 2, // Write Request
    DATA  = 3, // Data Packet
    ACK   = 4, // Acknowledgment
    ERROR = 5, // Error Packet
    OACK  = 6  // Option Acknowledgment (RFC 2347)
};

// WDS Image Types
enum class ImageType : uint32_t {
    BootImage    = 1, // Windows PE Boot Image (WinPE / boot.wim)
    InstallImage = 2  // Full Operating System Deployment Image (install.wim)
};

inline const char* ImageTypeToString(ImageType t) {
    switch (t) {
        case ImageType::BootImage:    return "Boot_Image (WinPE)";
        case ImageType::InstallImage: return "Install_Image (OS)";
        default: return "Unknown";
    }
}

// ============================================================================
// DHCP / PXE Protocol Structures (RFC 951 / RFC 2131 / RFC 4578)
// ============================================================================

constexpr uint32_t DHCP_MAGIC_COOKIE = 0x63825363; // 99.130.83.99

struct DhcpHeader {
    uint8_t  op{1};        // 1 = BOOTREQUEST, 2 = BOOTREPLY
    uint8_t  htype{1};     // 1 = 10Mb Ethernet
    uint8_t  hlen{6};      // MAC length (6 octets)
    uint8_t  hops{0};
    uint32_t xid{0};       // Transaction ID
    uint16_t secs{0};
    uint16_t flags{0x8000}; // 0x8000 = Broadcast
    uint32_t ciaddr{0};    // Client IP
    uint32_t yiaddr{0};    // Your IP (offered)
    uint32_t siaddr{0};    // Next server IP (TFTP server)
    uint32_t giaddr{0};    // Relay agent IP
    uint8_t  chaddr[16]{}; // Client hardware address (MAC)
    char     sname[64]{};  // Server host name
    char     file[128]{};  // Boot file name (e.g. boot\x64\wdsmgfw.efi)
};

struct DhcpOption {
    uint8_t              code{0};
    std::vector<uint8_t> data;
};

// ============================================================================
// WDS Image Catalog Record
// ============================================================================
struct WdsImageRecord {
    std::string imageId;
    std::string imageName;
    std::string architecture; // "x64", "ARM64", "x86"
    std::string filePath;     // e.g. "sources\boot.wim"
    ImageType   type{ImageType::BootImage};
    uint32_t    imageIndex{1};
    uint64_t    sizeBytes{0};
    std::string osVersion{"10.0.26100.1"};
    bool        enabled{true};
};

// ============================================================================
// Virtual TFTP File Entry
// ============================================================================
struct TftpVirtualFile {
    std::string          fileName;
    std::vector<uint8_t> content;
    std::string          description;
};

// ============================================================================
// DeploymentServicesEngine: Singleton Core Engine
// ============================================================================
class DeploymentServicesEngine {
private:
    mutable std::mutex m_mutex;
    bool m_initialized{false};

    // Server Configuration
    std::string m_serverIp{"192.168.1.50"};
    std::string m_serverNetmask{"255.255.255.0"};
    std::string m_serverName{"MICANT-WDS-01"};

    // Catalog & Boot Files
    std::map<std::string, WdsImageRecord> m_images; // ID -> Image
    std::map<std::string, TftpVirtualFile> m_tftpFiles; // Lowercase FileName -> File

    // Metrics
    mutable std::atomic<uint64_t> m_totalPxeRequests{0};
    mutable std::atomic<uint64_t> m_totalPxeOffers{0};
    mutable std::atomic<uint64_t> m_totalTftpRequests{0};
    mutable std::atomic<uint64_t> m_totalTftpBytesServed{0};
    mutable std::atomic<uint64_t> m_totalUnattendGenerated{0};

    DeploymentServicesEngine() {
        initialize();
    }

public:
    static DeploymentServicesEngine& instance() {
        static DeploymentServicesEngine s_instance;
        return s_instance;
    }

    DeploymentServicesEngine(const DeploymentServicesEngine&) = delete;
    DeploymentServicesEngine& operator=(const DeploymentServicesEngine&) = delete;

    void initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return;

        // 1. Populate Default Virtual Boot Files in TFTP Root
        // x64 UEFI Bootloader (wdsmgfw.efi)
        TftpVirtualFile fX64;
        fX64.fileName = "boot\\x64\\wdsmgfw.efi";
        fX64.content.resize(1048576, 0x90); // 1 MB synthetic EFI binary
        fX64.content[0] = 'M'; fX64.content[1] = 'Z'; // PE stub
        fX64.description = "WDS x64 UEFI Network Boot Manager";
        m_tftpFiles["boot\\x64\\wdsmgfw.efi"] = fX64;

        // ARM64 UEFI Bootloader (wdsmgfw.efi)
        TftpVirtualFile fArm;
        fArm.fileName = "boot\\arm64\\wdsmgfw.efi";
        fArm.content.resize(1048576, 0x1F); // 1 MB synthetic ARM64 EFI binary
        fArm.content[0] = 'M'; fArm.content[1] = 'Z';
        fArm.description = "WDS ARM64 UEFI Network Boot Manager";
        m_tftpFiles["boot\\arm64\\wdsmgfw.efi"] = fArm;

        // Legacy BIOS NBP (wdsnbp.com)
        TftpVirtualFile fNbp;
        fNbp.fileName = "boot\\x86\\wdsnbp.com";
        fNbp.content.resize(32768, 0xCD); // 32 KB x86 NBP
        fNbp.description = "WDS x86 Legacy BIOS Network Bootstrap Program";
        m_tftpFiles["boot\\x86\\wdsnbp.com"] = fNbp;

        // Boot.sdi (RAMDisk Wrapper)
        TftpVirtualFile fSdi;
        fSdi.fileName = "boot\\boot.sdi";
        fSdi.content.resize(3145728, 0xAA); // 3 MB System Deployment Image
        fSdi.description = "System Deployment Image (SDI) RAMDisk Driver";
        m_tftpFiles["boot\\boot.sdi"] = fSdi;

        // Dynamic BCD
        TftpVirtualFile fBcd;
        fBcd.fileName = "boot\\bcd";
        fBcd.content = generateBcdStoreContent();
        fBcd.description = "Boot Configuration Data (BCD) Network Store";
        m_tftpFiles["boot\\bcd"] = fBcd;

        // 2. Register Default Boot Images
        WdsImageRecord imgX64Pe;
        imgX64Pe.imageId = "IMG-WINPE-X64";
        imgX64Pe.imageName = "Microsoft Windows PE 11 (x64) Network Boot Image";
        imgX64Pe.architecture = "x64";
        imgX64Pe.filePath = "sources\\boot_x64.wim";
        imgX64Pe.type = ImageType::BootImage;
        imgX64Pe.imageIndex = 1;
        imgX64Pe.sizeBytes = 524288000; // 500 MB
        m_images[imgX64Pe.imageId] = imgX64Pe;

        WdsImageRecord imgArmPe;
        imgArmPe.imageId = "IMG-WINPE-ARM64";
        imgArmPe.imageName = "Microsoft Windows PE 11 (ARM64) Network Boot Image";
        imgArmPe.architecture = "ARM64";
        imgArmPe.filePath = "sources\\boot_arm64.wim";
        imgArmPe.type = ImageType::BootImage;
        imgArmPe.imageIndex = 1;
        imgArmPe.sizeBytes = 471859200; // 450 MB
        m_images[imgArmPe.imageId] = imgArmPe;

        // Register default install image
        WdsImageRecord imgWin11;
        imgWin11.imageId = "IMG-WIN11-ENT-X64";
        imgWin11.imageName = "Windows 11 Enterprise 24H2 (x64) Production Golden Image";
        imgWin11.architecture = "x64";
        imgWin11.filePath = "sources\\install_win11_ent.wim";
        imgWin11.type = ImageType::InstallImage;
        imgWin11.imageIndex = 1;
        imgWin11.sizeBytes = 4831838208ULL; // 4.5 GB
        m_images[imgWin11.imageId] = imgWin11;

        // SCM & VersionDatabase registration
        registerScmServices();
        registerVersionDatabase();

        m_initialized = true;
    }

    void registerScmServices() {
        auto& scm = micant::scm::ServiceControlManager::get();

        // 1. WDSServer (Windows Deployment Services)
        auto recWds = std::make_shared<micant::scm::ServiceRecord>();
        recWds->serviceName = L"WDSServer";
        recWds->displayName = L"Windows Deployment Services";
        recWds->serviceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recWds->startType = micant::scm::SERVICE_AUTO_START;
        recWds->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recWds->binaryPath = L"C:\\Windows\\System32\\wdssvc.dll";
        recWds->status.dwServiceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recWds->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        recWds->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN;
        scm.registerServiceRecord(recWds);

        // 2. BINLSVC (Boot Information Negotiation Layer)
        auto recBinl = std::make_shared<micant::scm::ServiceRecord>();
        recBinl->serviceName = L"BINLSVC";
        recBinl->displayName = L"Boot Information Negotiation Layer";
        recBinl->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recBinl->startType = micant::scm::SERVICE_AUTO_START;
        recBinl->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recBinl->binaryPath = L"C:\\Windows\\System32\\binlsvc.dll";
        recBinl->status.dwServiceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recBinl->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        scm.registerServiceRecord(recBinl);

        // 3. WdsTftp (Windows Deployment Services TFTP Server)
        auto recTftp = std::make_shared<micant::scm::ServiceRecord>();
        recTftp->serviceName = L"WdsTftp";
        recTftp->displayName = L"Windows Deployment Services TFTP Server";
        recTftp->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recTftp->startType = micant::scm::SERVICE_AUTO_START;
        recTftp->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recTftp->binaryPath = L"C:\\Windows\\System32\\wdstftp.dll";
        recTftp->status.dwServiceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recTftp->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        scm.registerServiceRecord(recTftp);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("wdssvc.dll", "10.0.26100.1", "Windows Deployment Services Server Core");
        db.RegisterModule("wdsmgfw.efi", "10.0.26100.1", "Windows Deployment Services UEFI Boot Manager");
        db.RegisterModule("wdsclient.dll", "10.0.26100.1", "Windows Deployment Services Client Library");
        db.RegisterModule("wdstftp.dll", "10.0.26100.1", "Windows Deployment Services TFTP Server");
        db.RegisterModule("wdsutil.exe", "10.0.26100.1", "Windows Deployment Services Management CLI");
    }

    // ========================================================================
    // Image Catalog Management
    // ========================================================================

    bool registerImage(const WdsImageRecord& img) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_images[img.imageId] = img;
        return true;
    }

    bool getImage(const std::string& id, WdsImageRecord* outImg) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_images.find(id);
        if (it == m_images.end()) return false;
        if (outImg) *outImg = it->second;
        return true;
    }

    std::vector<WdsImageRecord> getAllImages() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<WdsImageRecord> res;
        for (const auto& [id, img] : m_images) {
            res.push_back(img);
        }
        return res;
    }

    // ========================================================================
    // Architecture Arbitration & Bootfile Resolution
    // ========================================================================

    std::string resolveBootFileForArchitecture(ClientArchitecture arch) const {
        switch (arch) {
            case ClientArchitecture::EFI_x86_64:
                return "boot\\x64\\wdsmgfw.efi";
            case ClientArchitecture::EFI_ARM64:
                return "boot\\arm64\\wdsmgfw.efi";
            case ClientArchitecture::EFI_IA32:
                return "boot\\x86\\wdsmgfw.efi";
            case ClientArchitecture::Intelx86PC:
            default:
                return "boot\\x86\\wdsnbp.com";
        }
    }

    // ========================================================================
    // PXE / DHCP Negotiation Engine (RFC 2131 / RFC 4578)
    // ========================================================================

    bool processPxeRequest(const uint8_t* inPacket, size_t inLen, std::vector<uint8_t>& outOffer) {
        m_totalPxeRequests.fetch_add(1, std::memory_order_relaxed);

        if (!inPacket || inLen < sizeof(DhcpHeader) + 4) {
            return false;
        }

        const auto* inHdr = reinterpret_cast<const DhcpHeader*>(inPacket);
        if (inHdr->op != 1) return false; // Must be BOOTREQUEST

        // Check magic cookie
        size_t cookieOffset = sizeof(DhcpHeader);
        uint32_t cookie = (static_cast<uint32_t>(inPacket[cookieOffset]) << 24) |
                          (static_cast<uint32_t>(inPacket[cookieOffset + 1]) << 16) |
                          (static_cast<uint32_t>(inPacket[cookieOffset + 2]) << 8) |
                          static_cast<uint32_t>(inPacket[cookieOffset + 3]);
        if (cookie != DHCP_MAGIC_COOKIE) return false;

        // Parse Options
        ClientArchitecture clientArch = ClientArchitecture::Intelx86PC;
        bool isPxeClient = false;
        uint8_t msgType = 1; // Discover

        size_t optOffset = cookieOffset + 4;
        while (optOffset < inLen) {
            uint8_t optCode = inPacket[optOffset];
            if (optCode == 255) break; // End
            if (optCode == 0) { optOffset++; continue; } // Pad
            if (optOffset + 1 >= inLen) break;
            uint8_t optLen = inPacket[optOffset + 1];
            if (optOffset + 2 + optLen > inLen) break;

            const uint8_t* optData = &inPacket[optOffset + 2];

            if (optCode == 53 && optLen >= 1) { // DHCP Message Type
                msgType = optData[0];
            } else if (optCode == 60) { // Vendor Class Identifier
                std::string vendor(reinterpret_cast<const char*>(optData), optLen);
                if (vendor.find("PXEClient") != std::string::npos) {
                    isPxeClient = true;
                }
            } else if (optCode == 93 && optLen >= 2) { // Client Architecture
                uint16_t rawArch = (static_cast<uint16_t>(optData[0]) << 8) | static_cast<uint16_t>(optData[1]);
                clientArch = static_cast<ClientArchitecture>(rawArch);
            }
            optOffset += (2 + optLen);
        }

        if (!isPxeClient && msgType != 1 && msgType != 3) {
            // Not a PXE request
            return false;
        }

        // Generate DHCP/BINL Offer/Ack Packet
        DhcpHeader outHdr{};
        outHdr.op = 2; // BOOTREPLY
        outHdr.htype = inHdr->htype;
        outHdr.hlen = inHdr->hlen;
        outHdr.xid = inHdr->xid;
        outHdr.flags = inHdr->flags;
        std::memcpy(outHdr.chaddr, inHdr->chaddr, 16);

        // Next Server IP (TFTP Server) -> 192.168.1.50
        outHdr.siaddr = 0xC0A80132; // 192.168.1.50 in network byte order
        std::strncpy(outHdr.sname, m_serverName.c_str(), sizeof(outHdr.sname) - 1);

        // Resolve Bootfile name based on ClientArchitecture
        std::string bootFile = resolveBootFileForArchitecture(clientArch);
        std::strncpy(outHdr.file, bootFile.c_str(), sizeof(outHdr.file) - 1);

        outOffer.resize(sizeof(DhcpHeader));
        std::memcpy(outOffer.data(), &outHdr, sizeof(DhcpHeader));

        // Append Magic Cookie
        uint8_t cookieBytes[4] = { 0x63, 0x82, 0x53, 0x63 };
        outOffer.insert(outOffer.end(), cookieBytes, cookieBytes + 4);

        // Option 53: Message Type (2 = Offer, or 5 = Ack)
        uint8_t optMsgType = (msgType == 3) ? 5 : 2;
        appendDhcpOption(outOffer, 53, { optMsgType });

        // Option 54: Server Identifier (192.168.1.50)
        appendDhcpOption(outOffer, 54, { 192, 168, 1, 50 });

        // Option 60: Vendor Class ("PXEClient")
        std::string vClass = "PXEClient";
        appendDhcpOption(outOffer, 60, std::vector<uint8_t>(vClass.begin(), vClass.end()));

        // Option 66: TFTP Server Name / IP string
        appendDhcpOption(outOffer, 66, std::vector<uint8_t>(m_serverIp.begin(), m_serverIp.end()));

        // Option 67: Bootfile name
        appendDhcpOption(outOffer, 67, std::vector<uint8_t>(bootFile.begin(), bootFile.end()));

        // Option 93: Architecture echo
        uint16_t archVal = static_cast<uint16_t>(clientArch);
        appendDhcpOption(outOffer, 93, { static_cast<uint8_t>((archVal >> 8) & 0xFF), static_cast<uint8_t>(archVal & 0xFF) });

        // Option 255: End
        outOffer.push_back(255);

        m_totalPxeOffers.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // ========================================================================
    // TFTP Server & Windowed Transfer Engine (RFC 1350 / RFC 2347 / RFC 7440)
    // ========================================================================

    struct TftpSessionParams {
        std::string fileName;
        uint32_t    blockSize{512};   // blksize option (default 512, negotiated e.g. 1456)
        uint64_t    transferSize{0};  // tsize option
        uint32_t    timeoutSec{3};    // timeout option
        uint32_t    windowSize{1};    // windowsize option (RFC 7440, default 1)
        bool        optionsRequested{false};
    };

    bool processTftpRrq(const std::string& requestPath, uint32_t clientBlkSize,
                        uint32_t clientWindowSize, TftpSessionParams* outParams,
                        std::vector<uint8_t>& outOackPacket) {
        m_totalTftpRequests.fetch_add(1, std::memory_order_relaxed);

        std::string normPath = normalizePath(requestPath);
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_tftpFiles.find(normPath);
        if (it == m_tftpFiles.end()) return false;

        TftpSessionParams params{};
        params.fileName = normPath;
        params.transferSize = it->second.content.size();
        params.blockSize = (clientBlkSize >= 512 && clientBlkSize <= 16384) ? clientBlkSize : 1456;
        params.windowSize = (clientWindowSize >= 1 && clientWindowSize <= 32) ? clientWindowSize : 4;
        params.optionsRequested = (clientBlkSize > 0 || clientWindowSize > 0);

        if (outParams) *outParams = params;

        // Construct TFTP OACK Packet (OpCode 6 + null-terminated option-value pairs)
        outOackPacket.clear();
        outOackPacket.push_back(0);
        outOackPacket.push_back(static_cast<uint8_t>(TftpOpCode::OACK)); // Opcode 6

        appendTftpOption(outOackPacket, "blksize", std::to_string(params.blockSize));
        appendTftpOption(outOackPacket, "tsize", std::to_string(params.transferSize));
        appendTftpOption(outOackPacket, "windowsize", std::to_string(params.windowSize));

        m_totalTftpBytesServed.fetch_add(params.transferSize, std::memory_order_relaxed);
        return true;
    }

    bool getTftpFileBlock(const std::string& requestPath, uint32_t blockNumber,
                          uint32_t blockSize, std::vector<uint8_t>& outBlockData,
                          bool* isLastBlock) const {
        std::string normPath = normalizePath(requestPath);
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_tftpFiles.find(normPath);
        if (it == m_tftpFiles.end() || blockNumber == 0 || blockSize == 0) return false;

        size_t offset = static_cast<size_t>(blockNumber - 1) * blockSize;
        const auto& content = it->second.content;

        if (offset >= content.size()) {
            outBlockData.clear();
            if (isLastBlock) *isLastBlock = true;
            return true;
        }

        size_t available = content.size() - offset;
        size_t toCopy = std::min<size_t>(available, blockSize);

        outBlockData.assign(&content[offset], &content[offset + toCopy]);
        if (isLastBlock) *isLastBlock = (toCopy < blockSize || offset + toCopy == content.size());
        return true;
    }

    std::vector<TftpVirtualFile> getAllTftpFiles() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<TftpVirtualFile> list;
        for (const auto& [name, f] : m_tftpFiles) {
            list.push_back(f);
        }
        return list;
    }

    // ========================================================================
    // Dynamic BCD Network Store Generator
    // ========================================================================

    std::vector<uint8_t> generateBcdStoreContent() const {
        std::string bcdDesc = "MicaNT Clean-Room Network BCD Store\n"
                              "Objects:\n"
                              "  {bootmgr} -> \\boot\\x64\\wdsmgfw.efi\n"
                              "  {ramdisk} -> [boot]\\boot\\boot.sdi\n"
                              "  {default} -> [ramdisk]\\sources\\boot.wim\n";
        return std::vector<uint8_t>(bcdDesc.begin(), bcdDesc.end());
    }

    // ========================================================================
    // Automated Unattend XML Answer File Generator
    // ========================================================================

    std::string generateUnattendXml(const std::string& computerName,
                                   const std::string& adminPassword,
                                   const std::string& domain = "") const {
        m_totalUnattendGenerated.fetch_add(1, std::memory_order_relaxed);

        std::ostringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
           << "<unattend xmlns=\"urn:schemas-microsoft-com:unattend\">\n"
           << "  <settings pass=\"windowsPE\">\n"
           << "    <component name=\"Microsoft-Windows-Setup\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\">\n"
           << "      <DiskConfiguration>\n"
           << "        <Disk wcm:action=\"add\">\n"
           << "          <DiskID>0</DiskID>\n"
           << "          <WillWipeDisk>true</WillWipeDisk>\n"
           << "          <CreatePartitions>\n"
           << "            <CreatePartition wcm:action=\"add\">\n"
           << "              <Order>1</Order>\n"
           << "              <Type>EFI</Type>\n"
           << "              <Size>512</Size>\n"
           << "            </CreatePartition>\n"
           << "            <CreatePartition wcm:action=\"add\">\n"
           << "              <Order>2</Order>\n"
           << "              <Type>MSR</Type>\n"
           << "              <Size>128</Size>\n"
           << "            </CreatePartition>\n"
           << "            <CreatePartition wcm:action=\"add\">\n"
           << "              <Order>3</Order>\n"
           << "              <Type>Primary</Type>\n"
           << "              <Extend>true</Extend>\n"
           << "            </CreatePartition>\n"
           << "          </CreatePartitions>\n"
           << "        </Disk>\n"
           << "      </DiskConfiguration>\n"
           << "    </component>\n"
           << "  </settings>\n"
           << "  <settings pass=\"specialize\">\n"
           << "    <component name=\"Microsoft-Windows-Shell-Setup\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\">\n"
           << "      <ComputerName>" << (computerName.empty() ? "MICANT-WS" : computerName) << "</ComputerName>\n"
           << "      <TimeZone>Pacific Standard Time</TimeZone>\n"
           << "    </component>\n";

        if (!domain.empty()) {
            ss << "    <component name=\"Microsoft-Windows-UnattendedJoin\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\">\n"
               << "      <Identification>\n"
               << "        <JoinDomain>" << domain << "</JoinDomain>\n"
               << "        <UnsecureJoin>true</UnsecureJoin>\n"
               << "      </Identification>\n"
               << "    </component>\n";
        }

        ss << "  </settings>\n"
           << "  <settings pass=\"oobeSystem\">\n"
           << "    <component name=\"Microsoft-Windows-Shell-Setup\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\">\n"
           << "      <UserAccounts>\n"
           << "        <AdministratorPassword>\n"
           << "          <Value>" << (adminPassword.empty() ? "Password123!" : adminPassword) << "</Value>\n"
           << "          <PlainText>true</PlainText>\n"
           << "        </AdministratorPassword>\n"
           << "      </UserAccounts>\n"
           << "    </component>\n"
           << "  </settings>\n"
           << "</unattend>\n";

        return ss.str();
    }

    // Metrics getters
    uint64_t getTotalPxeRequests() const { return m_totalPxeRequests.load(std::memory_order_relaxed); }
    uint64_t getTotalPxeOffers() const { return m_totalPxeOffers.load(std::memory_order_relaxed); }
    uint64_t getTotalTftpRequests() const { return m_totalTftpRequests.load(std::memory_order_relaxed); }
    uint64_t getTotalTftpBytesServed() const { return m_totalTftpBytesServed.load(std::memory_order_relaxed); }
    uint64_t getTotalUnattendGenerated() const { return m_totalUnattendGenerated.load(std::memory_order_relaxed); }

private:
    static std::string normalizePath(const std::string& p) {
        std::string s = p;
        for (auto& c : s) {
            if (c == '/') c = '\\';
            else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    }

    static void appendDhcpOption(std::vector<uint8_t>& buf, uint8_t code, const std::vector<uint8_t>& data) {
        buf.push_back(code);
        buf.push_back(static_cast<uint8_t>(data.size()));
        buf.insert(buf.end(), data.begin(), data.end());
    }

    static void appendTftpOption(std::vector<uint8_t>& buf, const std::string& key, const std::string& val) {
        buf.insert(buf.end(), key.begin(), key.end());
        buf.push_back(0);
        buf.insert(buf.end(), val.begin(), val.end());
        buf.push_back(0);
    }
};

// ============================================================================
// Clean-Room Win32 C ABI Exports (wdssvc.dll / wdstftp.dll)
// ============================================================================

extern "C" {

inline int32_t MicaWdsInitialize(void** ppEngine) {
    if (!ppEngine) return 0;
    auto& eng = DeploymentServicesEngine::instance();
    *ppEngine = &eng;
    return 1;
}

inline int32_t MicaWdsShutdown(void* pEngine) {
    if (!pEngine) return 0;
    return 1;
}

inline int32_t MicaWdsRegisterImage(void* pEngine, const char* name, const char* arch,
                                    const char* path, uint32_t type) {
    if (!pEngine || !name || !arch || !path) return 0;
    auto* eng = reinterpret_cast<DeploymentServicesEngine*>(pEngine);
    WdsImageRecord rec;
    rec.imageId = "IMG-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    rec.imageName = name;
    rec.architecture = arch;
    rec.filePath = path;
    rec.type = static_cast<ImageType>(type);
    rec.sizeBytes = 104857600; // 100 MB default
    return eng->registerImage(rec) ? 1 : 0;
}

inline int32_t MicaWdsProcessPxeRequest(void* pEngine, const uint8_t* inDhcp, uint32_t inLen,
                                       uint8_t* outDhcp, uint32_t maxLen, uint32_t* pOutLen) {
    if (!pEngine || !inDhcp || !outDhcp || !pOutLen) return 0;
    auto* eng = reinterpret_cast<DeploymentServicesEngine*>(pEngine);
    std::vector<uint8_t> offer;
    if (!eng->processPxeRequest(inDhcp, inLen, offer)) return 0;
    if (offer.size() > maxLen) return 0;
    std::memcpy(outDhcp, offer.data(), offer.size());
    *pOutLen = static_cast<uint32_t>(offer.size());
    return 1;
}

inline int32_t MicaWdsTftpServeFile(void* pEngine, const char* fileName, uint32_t blkSize,
                                    uint32_t windowSize, uint32_t* pTotalBlocks) {
    if (!pEngine || !fileName || !pTotalBlocks) return 0;
    auto* eng = reinterpret_cast<DeploymentServicesEngine*>(pEngine);
    DeploymentServicesEngine::TftpSessionParams params{};
    std::vector<uint8_t> oack;
    if (!eng->processTftpRrq(fileName, blkSize, windowSize, &params, oack)) return 0;
    uint32_t actualBlk = (params.blockSize > 0) ? params.blockSize : 512;
    *pTotalBlocks = static_cast<uint32_t>((params.transferSize + actualBlk - 1) / actualBlk);
    return 1;
}

inline int32_t MicaWdsGenerateUnattendXml(void* pEngine, const char* computerName,
                                         const char* adminPass, char* outXml,
                                         uint32_t maxLen, uint32_t* pOutLen) {
    if (!pEngine || !outXml || !pOutLen) return 0;
    auto* eng = reinterpret_cast<DeploymentServicesEngine*>(pEngine);
    std::string xml = eng->generateUnattendXml(computerName ? computerName : "", adminPass ? adminPass : "");
    if (xml.size() + 1 > maxLen) return 0;
    std::memcpy(outXml, xml.c_str(), xml.size() + 1);
    *pOutLen = static_cast<uint32_t>(xml.size());
    return 1;
}

} // extern "C"

} // namespace micant::wds
