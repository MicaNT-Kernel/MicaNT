#include <cstdint>
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <span>
#include <memory>
#include <thread>
#include <fstream>
#include <sstream>
#include <cmath>
#include <filesystem>
#include "micant/ntstatus.hpp"
#include "micant/ntdef.hpp"
#include "micant/ob.hpp"
#include "micant/mm.hpp"
#include "micant/pe.hpp"
#include "micant/syscalls.hpp"
#include "micant/dispatcher.hpp"
#include "micant/ps.hpp"
#include "micant/section.hpp"
#include "micant/sync.hpp"
#include "micant/io.hpp"
#include "micant/cm.hpp"
#include "micant/se.hpp"
#include "micant/lpc.hpp"
#include "micant/ke.hpp"
#include "micant/ex.hpp"
#include "micant/trap.hpp"
#include "micant/hal.hpp"
#include "micant/fs.hpp"
#include "micant/ex_work.hpp"
#include "micant/boot.hpp"
#include "micant/probe.hpp"
#include "micant/driver.hpp"
#include "micant/timer.hpp"
#include "micant/lookaside.hpp"
#include "micant/po.hpp"
#include "micant/heap.hpp"
#include "micant/ldr.hpp"
#include "micant/ntdll.hpp"
#include "micant/uefi.hpp"
#include "micant/bootvid.hpp"
#include "micant/csrss.hpp"
#include "micant/conhost.hpp"
#include "micant/kernel32.hpp"
#include "micant/wow64.hpp"
#include "micant/cpu.hpp"
#include "micant/msvcrt.hpp"
#include "micant/advapi32.hpp"
#include "micant/user32.hpp"
#include "micant/ws2_32.hpp"
#include "micant/shell.hpp"
#include "micant/ppl.hpp"
#include "micant/sysguard.hpp"
#include "micant/vbs_hvci.hpp"
#include "micant/dma_guard.hpp"
#include "micant/wsl_lxss.hpp"
#include "micant/sandbox.hpp"
#include "micant/whp.hpp"
#include "micant/wdf.hpp"
#include "micant/conpty.hpp"
#include "micant/usb.hpp"
#include "micant/pci.hpp"
#include "micant/nvme.hpp"
#include "micant/acpi.hpp"
#include "micant/hdaudio.hpp"
#include "micant/wddm.hpp"
#include "micant/storage.hpp"
#include "micant/fat32.hpp"
#include "micant/ndis.hpp"
#include "micant/tcpip.hpp"
#include "micant/iphlpapi.hpp"
#include "micant/arm64.hpp"
#include "micant/npfs.hpp"
#include "micant/scm.hpp"
#include "micant/prismx.hpp"
#include "micant/prism3d.hpp"
#include "micant/prism3d12.hpp"
#include "micant/prism_shader_vm.hpp"
#include "micant/dxgkrnl.hpp"
#include "micant/vulkan.hpp"
#include "micant/emeraldfs.hpp"
#include "micant/daytonamm.hpp"
#include "micant/d3dcompiler.hpp"
#include "micant/prismaudio.hpp"
#include "micant/xinput.hpp"
#include "micant/vanguarddriver.hpp"
#include "micant/aegissandbox.hpp"
#include "micant/polarisdiag.hpp"
#include "micant/cipherksp.hpp"
#include "micant/janusldr.hpp"
#include "micant/dinput.hpp"
#include "micant/prism_viewer.hpp"
#include "micant/d3d9.hpp"
#include "micant/gdi32.hpp"
#include "micant/ole32.hpp"
#include "micant/shell32.hpp"
#include "micant/comctl32.hpp"
#include "micant/winmm.hpp"
#include "micant/dsound.hpp"
#include "micant/version.hpp"
#include "micant/opengl.hpp"
#include "micant/wininet.hpp"
#include "micant/urlmon.hpp"
#include "micant/crypt32.hpp"
#include "micant/sspi.hpp"
#include "micant/rpcrt4.hpp"
#include "micant/oleaut32.hpp"
#include "micant/setupapi.hpp"
#include "micant/wevtapi.hpp"
#include "micant/wbem.hpp"
#include "micant/taskschd.hpp"
#include "micant/bits.hpp"
#include "micant/vss.hpp"
#include "micant/wer.hpp"
#include "micant/dwmapi.hpp"
#include "micant/wasapi.hpp"
#include "micant/cbs.hpp"
#include "micant/wdi.hpp"
#include "micant/pdh.hpp"
#include "micant/etw.hpp"
#include "micant/acl.hpp"
#include "micant/netapi32.hpp"
#include "micant/ldap.hpp"
#include "micant/termsrv.hpp"
#include "micant/winspool.hpp"
#include "micant/mci.hpp"
#include "micant/winscard.hpp"
#include "micant/nla.hpp"
#include "micant/wns.hpp"
#include "micant/location.hpp"
#include "micant/wpd.hpp"
#include "micant/sensors.hpp"
#include "micant/winbio.hpp"
#include "micant/bluetooth.hpp"
#include "micant/bthport.hpp"
#include "micant/wdiwifi.hpp"
#include "micant/usb4.hpp"
#include "micant/npu.hpp"
#include "micant/cxl.hpp"
#include "micant/ucsi.hpp"
#include "micant/bypassio.hpp"
#include "micant/pmem.hpp"
#include "micant/rdma.hpp"
#include "micant/pluton.hpp"
#include "micant/hfi.hpp"
#include "micant/cardmod.hpp"
#include "micant/posix.hpp"
#include "micant/whp.hpp"
#include "micant/dwrite.hpp"
#include "micant/mfplat.hpp"
#include "micant/dshow.hpp"
#include "micant/wmp.hpp"
#include "micant/gdiplus.hpp"
#include "micant/d2d1.hpp"
#include "micant/mfsession.hpp"
#include "micant/evr.hpp"
#include "micant/dxva2.hpp"
#include "micant/d3d11va.hpp"
#include "micant/d3d12video.hpp"
#include "micant/mfreadwrite.hpp"
#include "micant/directstorage.hpp"
#include "micant/ocr.hpp"
#include "micant/wlanapi.hpp"
#include "micant/virtdisk.hpp"
#include "micant/fveapi.hpp"
#include "micant/fwpuclnt.hpp"
#include "micant/feclient.hpp"
#include "micant/wscapi.hpp"
#include "micant/amsi.hpp"
#include "micant/mpengine.hpp"
#include "micant/exploit_guard.hpp"
#include "micant/credguard.hpp"
#include "micant/cet.hpp"
#include "micant/qat.hpp"
#include "micant/tee.hpp"
#include "micant/dsa.hpp"
#include "micant/amx.hpp"
#include "micant/sriov.hpp"
#include "micant/iommu.hpp"
#include "micant/uefi_rt.hpp"
#include "micant/modern_standby.hpp"
#include "micant/wsa.hpp"
#include "micant/touchpad.hpp"
#include "micant/ink.hpp"
#include "micant/spatial_audio.hpp"
#include "micant/hpd.hpp"
#include "micant/cameracx.hpp"
#include "micant/vrr.hpp"
#include "micant/sensorscx.hpp"
#include "micant/mbbcx.hpp"
#include "micant/pmp.hpp"
#include "micant/vmbus.hpp"
#include "micant/vpci.hpp"
#include "micant/vsm.hpp"
#include "micant/hotpatch.hpp"
#include "micant/hyperv.hpp"
#include "micant/refs.hpp"
#include "micant/csvfs.hpp"
#include "micant/wcifs.hpp"
#include "micant/s2d.hpp"
#include "micant/branchcache.hpp"
#include "micant/storage_replica.hpp"
#include "micant/clustering.hpp"
#include "micant/vmms.hpp"
#include "micant/activedirectory.hpp"
#include "micant/grouppolicy.hpp"
#include "micant/remotedesktop.hpp"
#include "micant/nps.hpp"
#include "micant/wsrm.hpp"
#include "micant/wds.hpp"
#include "micant/certsrv.hpp"
#include "micant/dns_server.hpp"
#include "micant/dhcp_server.hpp"
#include "micant/iis_server.hpp"
#include "micant/wsus_server.hpp"
#include "micant/winrm_server.hpp"
#include "micant/ssh.hpp"
#include "micant/rdp.hpp"
#include "micant/input_router.hpp"
#include "micant/satellite_win32.hpp"
#include "micant/boot_event_loop.hpp"
#include "micant/satellite_wiztree.hpp"
#include "micant/satellite_putty.hpp"
#include "unmodified_fixture.hpp"

using namespace micant;

static int g_PassedTests = 0;
static int g_FailedTests = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            g_FailedTests++; \
            return; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[RUNNING] " << #fn << "...\n" << std::flush; \
        int before = g_FailedTests; \
        fn(); \
        if (g_FailedTests == before) { \
            std::cout << "  [PASS] " << #fn << "\n" << std::flush; \
            g_PassedTests++; \
        } \
    } while (0)

// ============================================================================
// Suite 1: Object Manager Tests
// ============================================================================

// ============================================================================
// Modular Test Suites (test/suites/*.hpp)
// ============================================================================
#include "suites/kernel_suites.hpp"
#include "suites/win32_suites.hpp"
#include "suites/multimedia_suites.hpp"
#include "suites/modern_suites.hpp"
#include "suites/hardware_suites.hpp"
#include "suites/server_storage_suites.hpp"

int main(int argc, char* argv[]) {
    if (argc > 1 && (std::string(argv[1]) == "--last" || std::string(argv[1]) == "--suite223")) {
        RUN_TEST(Test_SystemInformer_Diagnostics_And_NativeNT_Suite);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite222") {
        RUN_TEST(Test_Rufus_Storage_And_NtSyscalls_Suite);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite221") {
        RUN_TEST(Test_Retail_Ecosystem_100_Percent_Coverage);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite220") {
        RUN_TEST(Test_WinMerge_Visual_Diff_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite219") {
        RUN_TEST(Test_Everything_Search_Indexing_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite218") {
        RUN_TEST(Test_SumatraPDF_Gdiplus_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite217") {
        RUN_TEST(Test_PuTTYTerminal_AnsiWin32_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite216") {
        RUN_TEST(Test_BareMetalEventLoop_WizTreeMFT_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite215") {
        RUN_TEST(Test_InteractiveWindowManager_InputRouting_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite214") {
        RUN_TEST(Test_WindowsRemoteDesktop_RDP_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite213") {
        RUN_TEST(Test_WindowsOpenSSH_ServerClient_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite212") {
        RUN_TEST(Test_WindowsRemoteManagement_WinRM_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite211") {
        RUN_TEST(Test_WindowsServerUpdateServices_WSUS_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite210") {
        RUN_TEST(Test_WindowsEnterpriseIIS_HttpServer_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite209") {
        RUN_TEST(Test_WindowsEnterpriseDHCP_Server_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite208") {
        RUN_TEST(Test_WindowsEnterpriseDNS_Server_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite207") {
        RUN_TEST(Test_ActiveDirectoryCertificateServices_ADCS_PKI_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite206") {
        RUN_TEST(Test_WindowsDeploymentServices_PXE_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite205") {
        RUN_TEST(Test_WindowsSystemResourceManager_FairShare_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite204") {
        RUN_TEST(Test_WindowsNetworkPolicyServer_RADIUS_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite203") {
        RUN_TEST(Test_WindowsRemoteDesktop_VirtualChannels_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite202") {
        RUN_TEST(Test_WindowsGroupPolicy_Engine_CSE_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite201") {
        RUN_TEST(Test_WindowsActiveDirectory_KerberosKDC_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite200") {
        RUN_TEST(Test_WindowsHyperV_VMMS_VirtualSwitch_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite199") {
        RUN_TEST(Test_WindowsFailoverClustering_PaxosQuorum_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite198") {
        RUN_TEST(Test_WindowsStorageReplica_DisasterRecovery_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite197") {
        RUN_TEST(Test_WindowsDirectAccess_BranchCache_SMBQuic_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite196") {
        RUN_TEST(Test_WindowsDirectStorage_S2D_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite195") {
        RUN_TEST(Test_WindowsContainerStorage_Wcifs_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite194") {
        RUN_TEST(Test_WindowsClusterSharedVolume_CSVFS_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite193") {
        RUN_TEST(Test_WindowsReFS_ResilientFileSystem_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite192") {
        RUN_TEST(Test_WindowsHyperV_NestedVirtualization_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite191") {
        RUN_TEST(Test_WindowsKernelHotpatching_LiveUpdate_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite190") {
        RUN_TEST(Test_WindowsVirtualSecureMode_VBS_HVCI_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite189") {
        RUN_TEST(Test_WindowsVirtualPCI_SRIOV_DDA_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite188") {
        RUN_TEST(Test_WindowsVMBus_SyntheticDriver_Subsystem);
        return g_FailedTests;
    }

    if (argc > 1 && std::string(argv[1]) == "--suite187") {
        RUN_TEST(Test_WindowsProtectedMedia_PAVP_HDCP_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite186") {
        RUN_TEST(Test_WindowsMbbCx_MBIM40_5G_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite185") {
        RUN_TEST(Test_WindowsSensorsCxV2_SensorFusion_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite184") {
        RUN_TEST(Test_WindowsDisplayVRR_AutoHDR_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite183") {
        RUN_TEST(Test_WindowsCameraClassExtension_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite182") {
        RUN_TEST(Test_WindowsHumanPresenceDetection_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite181") {
        RUN_TEST(Test_WindowsSpatialAudio_APO_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite180") {
        RUN_TEST(Test_WindowsInk_PenDigitizer_ISF_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite179") {
        RUN_TEST(Test_WindowsPrecisionTouchpad_DirectManipulation_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite178") {
        RUN_TEST(Test_WindowsSubsystemForAndroid_WSA_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite177") {
        RUN_TEST(Test_ModernStandby_PEP_SleepStudy_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite176") {
        RUN_TEST(Test_WindowsUEFI_RuntimeServices_CapsuleUpdate_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite175") {
        RUN_TEST(Test_HardwareIOMMU_VTd_AMDVi_DMA_Remapping_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite174") {
        RUN_TEST(Test_PCIeSRIOV_PASID_SharedVirtualAddressing_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite173") {
        RUN_TEST(Test_IntelAMX_ArmSME_MatrixAccelerator_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite172") {
        RUN_TEST(Test_IntelDSA_IAA_FastCopy_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite171") {
        RUN_TEST(Test_ConfidentialComputing_TEE_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite170") {
        RUN_TEST(Test_IntelQAT_HardwareOffload_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite169") {
        RUN_TEST(Test_IntelCET_HardwareEnforcedStackProtection_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite168") {
        RUN_TEST(Test_IntelThreadDirector_AMD_CPPC_HeterogeneousScheduling_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite167") {
        RUN_TEST(Test_MicrosoftPluton_SecurityProcessor_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite166") {
        RUN_TEST(Test_RDMA_RoCEv2_InfiniBand_SMBDirect_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite165") {
        RUN_TEST(Test_PersistentMemory_NVDIMM_Optane_DAX_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite164") {
        RUN_TEST(Test_DirectStorage12_BypassIO_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite163") {
        RUN_TEST(Test_USBTypeC_UCSI_PowerDelivery31_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite162") {
        RUN_TEST(Test_ComputeExpressLink_CXL_HeterogeneousMemory_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite161") {
        RUN_TEST(Test_NeuralProcessingUnit_MCDM_DirectML_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite160") {
        RUN_TEST(Test_USB4_Thunderbolt4_ProtocolTunneling_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite159") {
        RUN_TEST(Test_WiFi7_WDI_NetAdapterCx_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite158") {
        RUN_TEST(Test_Bluetooth54_KernelPortDriver_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite157") {
        RUN_TEST(Test_NDIS688_HighSpeedNetworking_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite156") {
        RUN_TEST(Test_WDDM32_GraphicsKernel_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite155") {
        RUN_TEST(Test_IntelHighDefinitionAudio_USBAudio_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite154") {
        RUN_TEST(Test_ACPI_Platform_And_AML_Interpreter_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite153") {
        RUN_TEST(Test_NVMExpress_UniversalFlashStorage_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite152") {
        RUN_TEST(Test_PCIExpress_PCIe_Bus_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite151") {
        RUN_TEST(Test_UniversalSerialBus_USB_xHCI_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite150") {
        RUN_TEST(Test_WindowsPseudoConsole_ConPTY_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite149") {
        RUN_TEST(Test_WindowsDriverFrameworks_WDF_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite148") {
        RUN_TEST(Test_WindowsPackageManager_AppInstaller_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite147") {
        RUN_TEST(Test_WindowsHypervisorPlatform_Viridian_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite146") {
        RUN_TEST(Test_WindowsSandbox_LightweightContainer_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite145") {
        RUN_TEST(Test_WindowsSubsystemForLinux_LXSS_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite144") {
        RUN_TEST(Test_WindowsKernelDMA_Protection_IOMMU_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite143") {
        RUN_TEST(Test_WindowsVBS_HVCI_MemoryIntegrity_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite142") {
        RUN_TEST(Test_WindowsSystemGuard_SecureLaunch_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite141") {
        RUN_TEST(Test_WindowsProtectedProcessLight_ELAM_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite140") {
        RUN_TEST(Test_WindowsCredentialGuard_SentinelCredGuard_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite139") {
        RUN_TEST(Test_WindowsExploitGuard_SentinelGuard_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite138") {
        RUN_TEST(Test_WindowsDefender_AegisDefender_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite137") {
        RUN_TEST(Test_WindowsAMSI_SentinelScan_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite136") {
        RUN_TEST(Test_WindowsSecurityCenter_WSC_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite135") {
        RUN_TEST(Test_WindowsEncryptingFileSystem_EFS_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite134") {
        RUN_TEST(Test_WindowsCodeIntegrity_WDAC_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite133") {
        RUN_TEST(Test_WindowsAuthenticode_WinTrust_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite132") {
        RUN_TEST(Test_WindowsFilteringPlatform_Firewall_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite131") {
        RUN_TEST(Test_WindowsBitLocker_FVE_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite130") {
        RUN_TEST(Test_WindowsVirtualDisk_Storage_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite129") {
        RUN_TEST(Test_WindowsNativeWifi_WLAN_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite128") {
        RUN_TEST(Test_WindowsWebAuthn_FIDO2_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite127") {
        RUN_TEST(Test_WindowsMachineLearning_WinML_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite126") {
        RUN_TEST(Test_WindowsMedia_OCR_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite125") {
        RUN_TEST(Test_WindowsSpeech_SAPI_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite124") {
        RUN_TEST(Test_WindowsSpellCheck_Linguistic_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite123") {
        RUN_TEST(Test_WindowsTextServices_IME_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite122") {
        RUN_TEST(Test_WindowsDirect2D1_3_Typography_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite121") {
        RUN_TEST(Test_WindowsAppModel_Lifecycle_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite120") {
        RUN_TEST(Test_WindowsPointerDevice_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite119") {
        RUN_TEST(Test_WindowsColorSystem_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite118") {
        RUN_TEST(Test_WindowsUIComposition_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite117") {
        RUN_TEST(Test_WindowsDirectComposition_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite116") {
        RUN_TEST(Test_WindowsDirectML_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite115") {
        RUN_TEST(Test_WindowsDirectStorage_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite114") {
        RUN_TEST(Test_WindowsDirectX_Raytracing_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite113") {
        RUN_TEST(Test_WindowsMediaFoundation_CaptureEngine_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite112") {
        RUN_TEST(Test_WindowsMediaFoundation_SourceReader_SinkWriter_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite111") {
        RUN_TEST(Test_WindowsDirect3D12_Video_Acceleration_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite110") {
        RUN_TEST(Test_WindowsDirect3D11_Video_Acceleration_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite109") {
        RUN_TEST(Test_WindowsDXVA2_Hardware_Acceleration_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite108") {
        RUN_TEST(Test_WindowsEnhancedVideoRenderer_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite107") {
        RUN_TEST(Test_WindowsMediaFoundation_Topology_And_Session_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite106") {
        RUN_TEST(Test_WindowsDirect2D_Hardware_Rendering_Subsystem);
        return g_FailedTests;
    }

    std::cout << "========================================================================\n";
    std::cout << "                   MicaNT Executive Unit Test Suite                     \n";
    std::cout << "========================================================================\n\n";

    RUN_TEST(Test_ObjectManager_DirectoryAndHandles);
    RUN_TEST(Test_MemoryManager_VADAllocation);
    RUN_TEST(Test_PeLoader_ValidAndCorruptedHeaders);
    RUN_TEST(Test_SyscallDispatcher_DispatchFlow);
    RUN_TEST(Test_ProcessAndSectionManager);
    RUN_TEST(Test_SynchronizationPrimitives);
    RUN_TEST(Test_IoAndCompletionPorts);
    RUN_TEST(Test_ConfigurationManager_HiveAndValues);
    RUN_TEST(Test_SecurityReferenceMonitor_AccessCheck);
    RUN_TEST(Test_Alpc_MessageRendezvous);
    RUN_TEST(Test_KernelCore_IrqlSpinLockAndScheduler);
    RUN_TEST(Test_ExecutivePools_AllocationAndIrql);
    RUN_TEST(Test_TrapEngine_PageFaultAndBugCheck);
    RUN_TEST(Test_HardwareAbstractionLayer_KPCRAndTimers);
    RUN_TEST(Test_VirtualFileSystem_Fat32AndFileObjects);
    RUN_TEST(Test_ExecutiveWorkQueues_Dispatch);
    RUN_TEST(Test_BootContract_LoaderParameterBlock);
    RUN_TEST(Test_KernelStressAndConcurrencyHardening);
    RUN_TEST(Test_DriverModel_DriverEntryAndDeviceIoControl);
    RUN_TEST(Test_KernelTimers_DpcAndDelayExecution);
    RUN_TEST(Test_WaitMultipleObjects_MultiHandleSync);
    RUN_TEST(Test_LookasideLists_FastAllocAndTelemetry);
    RUN_TEST(Test_PowerManagement_IrpAndShutdown);
    RUN_TEST(Test_Ntdll_SyscallStubsAndPebTeb);
    RUN_TEST(Test_UserlandHeap_RtlAllocateAndCoalescing);
    RUN_TEST(Test_UefiBootloader_GopAndMemoryMap);
    RUN_TEST(Test_BootVid_FramebufferAndSplashRenderer);
    RUN_TEST(Test_Csrss_ProcessRegistrationAndAlpc);
    RUN_TEST(Test_Conhost_ScreenBufferAndFramebufferBlit);
    RUN_TEST(Test_Kernel32_Win32ApiParity);
    RUN_TEST(Test_Wow64_PebTebAndHeavensGate);
    RUN_TEST(Test_Wow64_SyscallThunkingAndFsRedirection);
    RUN_TEST(Test_PeLoader_DynamicImportBindingAndUnmodifiedBinary);
    RUN_TEST(Test_Cpu_GdtTssAndRing3HardwareTransitions);
    RUN_TEST(Test_Execution_UnmodifiedThirdPartyBinary);
    RUN_TEST(Test_ExpandedWin32AndNtSystemCalls);
    RUN_TEST(Test_MsvcrtBridge_And_CommandShell);
    RUN_TEST(Test_StorageAndFat32FileSystem);
    RUN_TEST(Test_NdisAndTcpIpNetworkStack);
    RUN_TEST(Test_Arm64HardwareArchitectureAndSyscall);
    RUN_TEST(Test_NamedPipesAndMailslotsIpc);
    RUN_TEST(Test_NtfsFileSystemAndMasterFileTable);
    RUN_TEST(Test_ServiceControlManager_And_SvcHost);
    RUN_TEST(Test_Lsass_Winlogon_And_SamDatabase);
    RUN_TEST(Test_PrismX_And_Prism3D_GraphicsSubsystem);
    RUN_TEST(Test_VulkanLoader_And_PrismVK_Subsystem);
    RUN_TEST(Test_Prism3D12_And_ProgrammableShaderVM);
    RUN_TEST(Test_EmeraldFS_And_DaytonaMM_Subsystems);
    RUN_TEST(Test_DirectX_DynamicLoader_And_DXBC_Container);
    RUN_TEST(Test_DirectX_PrismAudio_And_XInput_Subsystems);
    RUN_TEST(Test_VanguardDriver_DeviceStack_And_PnP_Subsystem);
    RUN_TEST(Test_AegisSandbox_JobObjects_And_ProcessContainment);
    RUN_TEST(Test_PolarisDiag_CrashDump_And_MinidumpWriter);
    RUN_TEST(Test_CipherKSP_CryptographicServices_And_AES);
    RUN_TEST(Test_JanusLDR_DelayLoadThunks_And_SxSManifest);
    RUN_TEST(Test_User32_WindowManager_SwapchainPresentation_And_DirectInput);
    RUN_TEST(Test_PrismX_Interactive3DViewer_And_CameraPipeline);
    RUN_TEST(Test_Direct3D9_Runtime_And_FixedFunctionPipeline);
    RUN_TEST(Test_Gdi32_And_Ole32_Win32Foundation);
    RUN_TEST(Test_Shell32_And_ComCtl32_Win32Controls);
    RUN_TEST(Test_Windows_CMD_And_BatchExecutionEngine);
    RUN_TEST(Test_Direct3D9_ProgrammableShaders_And_D3DX9Math);
    RUN_TEST(Test_WinMM_DirectSound_And_VersionInfo);
    RUN_TEST(Test_OpenGL_And_WGL_Subsystem);
    RUN_TEST(Test_WinINet_And_URLMon_Subsystems);
    RUN_TEST(Test_CryptoAPI_And_CNG_Subsystems);
    RUN_TEST(Test_SSPI_And_Schannel_Subsystems);
    RUN_TEST(Test_RPC_Runtime_And_NDR_Subsystem);
    RUN_TEST(Test_OLE_Automation_And_SafeArray_Subsystem);
    RUN_TEST(Test_SetupApi_DeviceInstallation_And_INF_Subsystem);
    RUN_TEST(Test_StructuredStorage_CompoundFile_And_Persistence_Subsystem);
    RUN_TEST(Test_WindowsEventLog_And_WevtApi_Subsystem);
    RUN_TEST(Test_WMI_WindowsManagementInstrumentation_Subsystem);
    RUN_TEST(Test_WindowsTaskScheduler_Subsystem);
    RUN_TEST(Test_WindowsBITS_Subsystem);
    RUN_TEST(Test_WindowsVSS_VolumeShadowCopy_Subsystem);
    RUN_TEST(Test_WindowsWER_ErrorReporting_Subsystem);
    RUN_TEST(Test_WindowsDWM_DesktopWindowManager_Subsystem);
    RUN_TEST(Test_WindowsWASAPI_CoreAudioEngine_Subsystem);
    RUN_TEST(Test_WindowsCBS_DISM_Servicing_Subsystem);
    RUN_TEST(Test_WindowsWDI_DiagnosticsInfrastructure_Subsystem);
    RUN_TEST(Test_WindowsPDH_PerformanceMonitor_Subsystem);
    RUN_TEST(Test_WindowsETW_EventTracing_Subsystem);
    RUN_TEST(Test_WindowsACL_SecurityAuditing_Subsystem);
    RUN_TEST(Test_WindowsNetAPI32_NetworkManagement_Subsystem);
    RUN_TEST(Test_WindowsLDAP_ActiveDirectory_Subsystem);
    RUN_TEST(Test_WindowsRDP_TerminalServices_Subsystem);
    RUN_TEST(Test_WindowsPrinting_Spooler_Subsystem);
    RUN_TEST(Test_WindowsMCI_AudioWave_Subsystem);
    RUN_TEST(Test_WindowsSmartCard_PCSC_Subsystem);
    RUN_TEST(Test_WindowsNLA_NetworkListService_Subsystem);
    RUN_TEST(Test_WindowsWNS_PushNotification_Subsystem);
    RUN_TEST(Test_WindowsLocation_Geolocation_Subsystem);
    RUN_TEST(Test_WindowsWPD_PortableDevices_Subsystem);
    RUN_TEST(Test_WindowsSensors_Subsystem);
    RUN_TEST(Test_WindowsBiometrics_Subsystem);
    RUN_TEST(Test_WindowsBluetooth_Subsystem);
    RUN_TEST(Test_WindowsSmartCardMinidriver_Subsystem);
    RUN_TEST(Test_WindowsPOSIX_Subsystem);
    RUN_TEST(Test_WindowsHypervisor_Platform_Subsystem);
    RUN_TEST(Test_WindowsDirectWrite_Uniscribe_Subsystem);
    RUN_TEST(Test_WindowsMediaFoundation_Subsystem);
    RUN_TEST(Test_WindowsDirectShow_FilterGraph_Subsystem);
    RUN_TEST(Test_WindowsMediaPlayer_ActiveMovie_Subsystem);
    RUN_TEST(Test_WindowsGdiPlus_Imaging_Subsystem);
    RUN_TEST(Test_WindowsDirect2D_Hardware_Rendering_Subsystem);
    RUN_TEST(Test_WindowsMediaFoundation_Topology_And_Session_Subsystem);
    RUN_TEST(Test_WindowsEnhancedVideoRenderer_Subsystem);
    RUN_TEST(Test_WindowsDXVA2_Hardware_Acceleration_Subsystem);
    RUN_TEST(Test_WindowsDirect3D11_Video_Acceleration_Subsystem);
    RUN_TEST(Test_WindowsDirect3D12_Video_Acceleration_Subsystem);
    RUN_TEST(Test_WindowsMediaFoundation_SourceReader_SinkWriter_Subsystem);
    RUN_TEST(Test_WindowsMediaFoundation_CaptureEngine_Subsystem);
    RUN_TEST(Test_WindowsDirectX_Raytracing_Subsystem);
    RUN_TEST(Test_WindowsDirectStorage_Subsystem);
    RUN_TEST(Test_WindowsDirectML_Subsystem);
    RUN_TEST(Test_WindowsDirectComposition_Subsystem);
    RUN_TEST(Test_WindowsUIComposition_Subsystem);
    RUN_TEST(Test_WindowsColorSystem_Subsystem);
    RUN_TEST(Test_WindowsPointerDevice_Subsystem);
    RUN_TEST(Test_WindowsAppModel_Lifecycle_Subsystem);
    RUN_TEST(Test_WindowsDirect2D1_3_Typography_Subsystem);
    RUN_TEST(Test_WindowsTextServices_IME_Subsystem);
    RUN_TEST(Test_WindowsSpellCheck_Linguistic_Subsystem);
    RUN_TEST(Test_WindowsSpeech_SAPI_Subsystem);
    RUN_TEST(Test_WindowsMedia_OCR_Subsystem);
    RUN_TEST(Test_WindowsMachineLearning_WinML_Subsystem);
    RUN_TEST(Test_WindowsWebAuthn_FIDO2_Subsystem);
    RUN_TEST(Test_WindowsNativeWifi_WLAN_Subsystem);
    RUN_TEST(Test_WindowsVirtualDisk_Storage_Subsystem);
    RUN_TEST(Test_WindowsBitLocker_FVE_Subsystem);
    RUN_TEST(Test_WindowsFilteringPlatform_Firewall_Subsystem);
    RUN_TEST(Test_WindowsAuthenticode_WinTrust_Subsystem);
    RUN_TEST(Test_WindowsCodeIntegrity_WDAC_Subsystem);
    RUN_TEST(Test_WindowsEncryptingFileSystem_EFS_Subsystem);
    RUN_TEST(Test_WindowsSecurityCenter_WSC_Subsystem);
    RUN_TEST(Test_WindowsAMSI_SentinelScan_Subsystem);
    RUN_TEST(Test_WindowsDefender_AegisDefender_Subsystem);
    RUN_TEST(Test_WindowsExploitGuard_SentinelGuard_Subsystem);
    RUN_TEST(Test_WindowsCredentialGuard_SentinelCredGuard_Subsystem);
    RUN_TEST(Test_WindowsProtectedProcessLight_ELAM_Subsystem);
    RUN_TEST(Test_WindowsSystemGuard_SecureLaunch_Subsystem);
    RUN_TEST(Test_WindowsVBS_HVCI_MemoryIntegrity_Subsystem);
    RUN_TEST(Test_WindowsKernelDMA_Protection_IOMMU_Subsystem);
    RUN_TEST(Test_WindowsSubsystemForLinux_LXSS_Subsystem);
    RUN_TEST(Test_WindowsSandbox_LightweightContainer_Subsystem);
    RUN_TEST(Test_WindowsHypervisorPlatform_Viridian_Subsystem);
    RUN_TEST(Test_WindowsPackageManager_AppInstaller_Subsystem);
    RUN_TEST(Test_WindowsDriverFrameworks_WDF_Subsystem);
    RUN_TEST(Test_WindowsPseudoConsole_ConPTY_Subsystem);
    RUN_TEST(Test_UniversalSerialBus_USB_xHCI_Subsystem);
    RUN_TEST(Test_PCIExpress_PCIe_Bus_Subsystem);
    RUN_TEST(Test_NVMExpress_UniversalFlashStorage_Subsystem);
    RUN_TEST(Test_ACPI_Platform_And_AML_Interpreter_Subsystem);
    RUN_TEST(Test_IntelHighDefinitionAudio_USBAudio_Subsystem);
    RUN_TEST(Test_WDDM32_GraphicsKernel_Subsystem);
    RUN_TEST(Test_NDIS688_HighSpeedNetworking_Subsystem);
    RUN_TEST(Test_Bluetooth54_KernelPortDriver_Subsystem);
    RUN_TEST(Test_WiFi7_WDI_NetAdapterCx_Subsystem);
    RUN_TEST(Test_USB4_Thunderbolt4_ProtocolTunneling_Subsystem);
    RUN_TEST(Test_NeuralProcessingUnit_MCDM_DirectML_Subsystem);
    RUN_TEST(Test_ComputeExpressLink_CXL_HeterogeneousMemory_Subsystem);
    RUN_TEST(Test_USBTypeC_UCSI_PowerDelivery31_Subsystem);
    RUN_TEST(Test_DirectStorage12_BypassIO_Subsystem);
    RUN_TEST(Test_PersistentMemory_NVDIMM_Optane_DAX_Subsystem);
    RUN_TEST(Test_RDMA_RoCEv2_InfiniBand_SMBDirect_Subsystem);
    RUN_TEST(Test_MicrosoftPluton_SecurityProcessor_Subsystem);
    RUN_TEST(Test_IntelThreadDirector_AMD_CPPC_HeterogeneousScheduling_Subsystem);
    RUN_TEST(Test_IntelCET_HardwareEnforcedStackProtection_Subsystem);
    RUN_TEST(Test_IntelQAT_HardwareOffload_Subsystem);
    RUN_TEST(Test_ConfidentialComputing_TEE_Subsystem);
    RUN_TEST(Test_IntelDSA_IAA_FastCopy_Subsystem);
    RUN_TEST(Test_IntelAMX_ArmSME_MatrixAccelerator_Subsystem);
    RUN_TEST(Test_PCIeSRIOV_PASID_SharedVirtualAddressing_Subsystem);
    RUN_TEST(Test_HardwareIOMMU_VTd_AMDVi_DMA_Remapping_Subsystem);
    RUN_TEST(Test_WindowsUEFI_RuntimeServices_CapsuleUpdate_Subsystem);
    RUN_TEST(Test_ModernStandby_PEP_SleepStudy_Subsystem);
    RUN_TEST(Test_WindowsSubsystemForAndroid_WSA_Subsystem);
    RUN_TEST(Test_WindowsPrecisionTouchpad_DirectManipulation_Subsystem);
    RUN_TEST(Test_WindowsInk_PenDigitizer_ISF_Subsystem);
    RUN_TEST(Test_WindowsSpatialAudio_APO_Subsystem);
    RUN_TEST(Test_WindowsHumanPresenceDetection_Subsystem);
    RUN_TEST(Test_WindowsCameraClassExtension_Subsystem);
    RUN_TEST(Test_WindowsDisplayVRR_AutoHDR_Subsystem);
    RUN_TEST(Test_WindowsSensorsCxV2_SensorFusion_Subsystem);
    RUN_TEST(Test_WindowsMbbCx_MBIM40_5G_Subsystem);
    RUN_TEST(Test_WindowsProtectedMedia_PAVP_HDCP_Subsystem);
    RUN_TEST(Test_WindowsVMBus_SyntheticDriver_Subsystem);
    RUN_TEST(Test_WindowsVirtualPCI_SRIOV_DDA_Subsystem);
    RUN_TEST(Test_WindowsVirtualSecureMode_VBS_HVCI_Subsystem);
    RUN_TEST(Test_WindowsKernelHotpatching_LiveUpdate_Subsystem);
    RUN_TEST(Test_WindowsHyperV_NestedVirtualization_Subsystem);
    RUN_TEST(Test_WindowsReFS_ResilientFileSystem_Subsystem);
    RUN_TEST(Test_WindowsClusterSharedVolume_CSVFS_Subsystem);
    RUN_TEST(Test_WindowsContainerStorage_Wcifs_Subsystem);
    RUN_TEST(Test_WindowsDirectStorage_S2D_Subsystem);
    RUN_TEST(Test_WindowsDirectAccess_BranchCache_SMBQuic_Subsystem);
    RUN_TEST(Test_WindowsStorageReplica_DisasterRecovery_Subsystem);
    RUN_TEST(Test_WindowsFailoverClustering_PaxosQuorum_Subsystem);
    RUN_TEST(Test_WindowsHyperV_VMMS_VirtualSwitch_Subsystem);
    RUN_TEST(Test_WindowsActiveDirectory_KerberosKDC_Subsystem);
    RUN_TEST(Test_WindowsGroupPolicy_Engine_CSE_Subsystem);
    RUN_TEST(Test_WindowsRemoteDesktop_VirtualChannels_Subsystem);
    RUN_TEST(Test_WindowsNetworkPolicyServer_RADIUS_Subsystem);
    RUN_TEST(Test_WindowsSystemResourceManager_FairShare_Subsystem);
    RUN_TEST(Test_WindowsDeploymentServices_PXE_Subsystem);
    RUN_TEST(Test_ActiveDirectoryCertificateServices_ADCS_PKI_Subsystem);
    RUN_TEST(Test_WindowsEnterpriseDNS_Server_Subsystem);
    RUN_TEST(Test_WindowsEnterpriseDHCP_Server_Subsystem);
    RUN_TEST(Test_WindowsEnterpriseIIS_HttpServer_Subsystem);
    RUN_TEST(Test_WindowsServerUpdateServices_WSUS_Subsystem);
    RUN_TEST(Test_WindowsRemoteManagement_WinRM_Subsystem);
    RUN_TEST(Test_WindowsOpenSSH_ServerClient_Subsystem);
    RUN_TEST(Test_WindowsRemoteDesktop_RDP_Subsystem);
    RUN_TEST(Test_InteractiveWindowManager_InputRouting_Subsystem);
    RUN_TEST(Test_BareMetalEventLoop_WizTreeMFT_Subsystem);
    RUN_TEST(Test_PuTTYTerminal_AnsiWin32_Subsystem);
    RUN_TEST(Test_SumatraPDF_Gdiplus_Subsystem);
    RUN_TEST(Test_Everything_Search_Indexing_Subsystem);
    RUN_TEST(Test_WinMerge_Visual_Diff_Subsystem);
    RUN_TEST(Test_Retail_Ecosystem_100_Percent_Coverage);
    RUN_TEST(Test_Rufus_Storage_And_NtSyscalls_Suite);
    RUN_TEST(Test_SystemInformer_Diagnostics_And_NativeNT_Suite);

    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << "Summary: " << g_PassedTests << " Passed, " << g_FailedTests << " Failed\n";
    std::cout << "------------------------------------------------------------------------\n";

    return (g_FailedTests == 0) ? 0 : 1;
}


