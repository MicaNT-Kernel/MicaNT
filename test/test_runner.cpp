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
#include "micant/shared/ntstatus.hpp"
#include "micant/shared/ntdef.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/executive/mm.hpp"
#include "micant/um/native/pe.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/core/dispatcher.hpp"
#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/section.hpp"
#include "micant/km/core/sync.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/executive/cm.hpp"
#include "micant/km/executive/se.hpp"
#include "micant/km/executive/lpc.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/executive/ex.hpp"
#include "micant/km/core/trap.hpp"
#include "micant/km/core/kd.hpp"
#include "micant/km/hal/hal.hpp"
#include "micant/km/drivers/fs/fs.hpp"
#include "micant/km/executive/ex_work.hpp"
#include "micant/km/hal/boot.hpp"
#include "micant/km/executive/probe.hpp"
#include "micant/km/executive/driver.hpp"
#include "micant/km/core/timer.hpp"
#include "micant/km/executive/lookaside.hpp"
#include "micant/km/executive/po.hpp"
#include "micant/km/core/ipi.hpp"
#include "micant/km/executive/heap.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/native/ntdll.hpp"
#include "micant/km/hal/uefi.hpp"
#include "micant/km/hal/bootvid.hpp"
#include "micant/km/hal/inbv.hpp"
#include "micant/um/servers/csrss.hpp"
#include "micant/um/servers/conhost.hpp"
#include "micant/um/subsystems/winbase/kernel32.hpp"
#include "micant/um/native/wow64.hpp"
#include "micant/km/hal/cpu.hpp"
#include "micant/ucrt/msvcrt.hpp"
#include "micant/um/subsystems/winbase/advapi32.hpp"
#include "micant/um/subsystems/winuser/user32.hpp"
#include "micant/um/subsystems/net/ws2_32.hpp"
#include "micant/um/subsystems/winuser/shell.hpp"
#include "micant/km/drivers/security/ppl.hpp"
#include "micant/km/drivers/security/sysguard.hpp"
#include "micant/km/drivers/security/vbs_hvci.hpp"
#include "micant/km/hal/dma_guard.hpp"
#include "micant/um/native/wsl_lxss.hpp"
#include "micant/km/drivers/security/sandbox.hpp"
#include "micant/um/subsystems/winbase/whp.hpp"
#include "micant/km/executive/wdf.hpp"
#include "micant/km/executive/pnp.hpp"
#include "micant/um/servers/conpty.hpp"
#include "micant/km/drivers/usb/usb.hpp"
#include "micant/km/hal/pci.hpp"
#include "micant/km/drivers/storage/nvme.hpp"
#include "micant/km/hal/acpi.hpp"
#include "micant/km/drivers/audio/hdaudio.hpp"
#include "micant/km/drivers/video/wddm.hpp"
#include "micant/km/win32k/win32k.hpp"
#include "micant/km/core/ring1_isolation.hpp"
#include "micant/km/executive/fltmgr.hpp"
#include "micant/km/executive/bugcheck.hpp"
#include "micant/km/drivers/usb/xhci.hpp"
#include "micant/km/drivers/input/hidclass.hpp"
#include "micant/km/executive/cc.hpp"
#include "micant/km/executive/fsrtl.hpp"
#include "micant/km/executive/dbgk.hpp"
#include "micant/km/executive/ktm.hpp"
#include "micant/km/executive/verifier.hpp"
#include "micant/km/executive/arbiter.hpp"
#include "micant/km/executive/wmi_driver_compat.hpp"
#include "micant/km/executive/pnp_boot_scheduler.hpp"
#include "micant/km/executive/driver_resilience.hpp"
#include "micant/km/executive/pofx_whea.hpp"
#include "micant/km/executive/umdf_msix_guard.hpp"
#include "micant/km/core/zero_bsod_guard.hpp"
#include "micant/km/executive/live_hotswap_storheal.hpp"
#include "micant/km/executive/sysinfo_taskmgr.hpp"
#include "micant/km/executive/quota_svchost_primes.hpp"
#include "micant/km/drivers/fs/mftsearch_uiguard_zipfldr.hpp"
#include "micant/km/executive/srm_token_ci.hpp"
#include "micant/km/drivers/net/afd_wsk_rio.hpp"
#include "micant/km/core/kuser_upcall_seal.hpp"
#include "micant/km/drivers/fs/ring1_vfs_minifilter_plane.hpp"
#include "micant/km/win32k/ring1_dxgkrnl_flip_plane.hpp"
#include "micant/km/drivers/audio/ring1_wavert_audio_dma.hpp"
#include "micant/km/executive/ring1_smss_csrss_bootstrap.hpp"
#include "micant/km/executive/ring2_wudf_fontdrv_host.hpp"
#include "micant/km/drivers/security/ring2_lsa_vault_keyiso.hpp"
#include "micant/km/executive/ring2_scm_services_broker.hpp"
#include "micant/um/native/ring3_win32_base_bridge.hpp"
#include "micant/um/servers/ring3_winlogon_explorer_dwm.hpp"
#include "micant/um/subsystems/winuser/ring3_real_apps_triad.hpp"
#include "micant/um/subsystems/graphics/ring3_prismx_3d12_shader_tunnel.hpp"
#include "micant/um/subsystems/graphics/ring3_directstorage_d2d_vulkan_tunnel.hpp"
#include "micant/um/subsystems/media/ring3_prismaudio_wasapi_mf_tunnel.hpp"
#include "micant/um/subsystems/net/ring3_winsock_winhttp_tls_tunnel.hpp"
#include "micant/um/subsystems/com/ring3_com_rpc_shell_controls_tunnel.hpp"
#include "micant/um/subsystems/security/ring3_bcrypt_crypt32_wintrust_tunnel.hpp"
#include "micant/um/subsystems/net/ring3_fiber_net_apps_tunnel.hpp"
#include "micant/um/subsystems/winuser/ring3_workstation_apps_tunnel.hpp"
#include "micant/um/servers/ring3_wsl2_hyperv_enterprise_closure.hpp"
#include "micant/um/subsystems/graphics/prismx_directx_headers_kmt_runtime.hpp"
#include "micant/um/subsystems/graphics/prismx_dxmath_dxtex_dxmesh_tk12.hpp"
#include "micant/um/subsystems/graphics/prismx_d3d12_translation_layer_on12.hpp"
#include "micant/km/drivers/storage/storage.hpp"
#include "micant/km/drivers/fs/fat32.hpp"
#include "micant/km/drivers/net/ndis.hpp"
#include "micant/km/drivers/net/tcpip.hpp"
#include "micant/um/subsystems/net/iphlpapi.hpp"
#include "micant/km/hal/arm64.hpp"
#include "micant/km/drivers/fs/npfs.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/graphics/prismx.hpp"
#include "micant/um/subsystems/graphics/prism3d.hpp"
#include "micant/um/subsystems/graphics/prism3d12.hpp"
#include "micant/um/subsystems/graphics/prism_shader_vm.hpp"
#include "micant/km/drivers/video/dxgkrnl.hpp"
#include "micant/um/subsystems/graphics/vulkan.hpp"
#include "micant/km/drivers/fs/emeraldfs.hpp"
#include "micant/ucrt/daytonamm.hpp"
#include "micant/um/subsystems/graphics/d3dcompiler.hpp"
#include "micant/um/subsystems/media/prismaudio.hpp"
#include "micant/um/subsystems/winuser/xinput.hpp"
#include "micant/km/drivers/security/vanguarddriver.hpp"
#include "micant/km/drivers/security/aegissandbox.hpp"
#include "micant/um/subsystems/mgmt/polarisdiag.hpp"
#include "micant/um/subsystems/security/cipherksp.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/subsystems/winuser/dinput.hpp"
#include "micant/um/subsystems/graphics/prism_viewer.hpp"
#include "micant/um/subsystems/graphics/d3d9.hpp"
#include "micant/um/subsystems/winuser/gdi32.hpp"
#include "micant/um/subsystems/com/ole32.hpp"
#include "micant/um/subsystems/winuser/shell32.hpp"
#include "micant/um/subsystems/winuser/comctl32.hpp"
#include "micant/um/subsystems/media/winmm.hpp"
#include "micant/um/subsystems/media/dsound.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/um/subsystems/graphics/opengl.hpp"
#include "micant/um/subsystems/net/wininet.hpp"
#include "micant/um/subsystems/net/urlmon.hpp"
#include "micant/um/subsystems/security/crypt32.hpp"
#include "micant/um/subsystems/security/sspi.hpp"
#include "micant/um/subsystems/com/rpcrt4.hpp"
#include "micant/um/subsystems/com/oleaut32.hpp"
#include "micant/um/subsystems/winbase/setupapi.hpp"
#include "micant/um/subsystems/mgmt/wevtapi.hpp"
#include "micant/um/subsystems/mgmt/wbem.hpp"
#include "micant/um/subsystems/mgmt/taskschd.hpp"
#include "micant/um/subsystems/net/bits.hpp"
#include "micant/um/subsystems/winbase/vss.hpp"
#include "micant/um/subsystems/mgmt/wer.hpp"
#include "micant/um/subsystems/winuser/dwmapi.hpp"
#include "micant/um/subsystems/media/wasapi.hpp"
#include "micant/um/subsystems/mgmt/cbs.hpp"
#include "micant/um/subsystems/mgmt/wdi.hpp"
#include "micant/um/subsystems/mgmt/pdh.hpp"
#include "micant/km/executive/etw.hpp"
#include "micant/um/subsystems/security/acl.hpp"
#include "micant/um/subsystems/net/netapi32.hpp"
#include "micant/um/servers/ldap.hpp"
#include "micant/um/servers/termsrv.hpp"
#include "micant/um/subsystems/winuser/winspool.hpp"
#include "micant/um/subsystems/media/mci.hpp"
#include "micant/um/subsystems/security/winscard.hpp"
#include "micant/um/subsystems/net/nla.hpp"
#include "micant/um/subsystems/net/wns.hpp"
#include "micant/um/subsystems/net/location.hpp"
#include "micant/um/subsystems/winbase/wpd.hpp"
#include "micant/um/subsystems/net/sensors.hpp"
#include "micant/um/subsystems/security/winbio.hpp"
#include "micant/um/subsystems/net/bluetooth.hpp"
#include "micant/km/drivers/input/bthport.hpp"
#include "micant/km/drivers/net/wdiwifi.hpp"
#include "micant/km/drivers/usb/usb4.hpp"
#include "micant/km/drivers/compute/npu.hpp"
#include "micant/km/drivers/bus/cxl.hpp"
#include "micant/km/drivers/usb/ucsi.hpp"
#include "micant/km/drivers/storage/bypassio.hpp"
#include "micant/km/drivers/storage/pmem.hpp"
#include "micant/km/drivers/net/rdma.hpp"
#include "micant/km/drivers/security/pluton.hpp"
#include "micant/km/drivers/compute/hfi.hpp"
#include "micant/um/subsystems/security/cardmod.hpp"
#include "micant/um/native/posix.hpp"
#include "micant/um/subsystems/winbase/whp.hpp"
#include "micant/um/subsystems/graphics/dwrite.hpp"
#include "micant/um/subsystems/media/mfplat.hpp"
#include "micant/um/subsystems/media/dshow.hpp"
#include "micant/um/subsystems/media/wmp.hpp"
#include "micant/um/subsystems/winuser/gdiplus.hpp"
#include "micant/um/subsystems/graphics/d2d1.hpp"
#include "micant/um/subsystems/media/mfsession.hpp"
#include "micant/um/subsystems/media/evr.hpp"
#include "micant/um/subsystems/graphics/dxva2.hpp"
#include "micant/um/subsystems/graphics/d3d11va.hpp"
#include "micant/um/subsystems/graphics/d3d12video.hpp"
#include "micant/um/subsystems/media/mfreadwrite.hpp"
#include "micant/um/subsystems/graphics/directstorage.hpp"
#include "micant/um/subsystems/graphics/ocr.hpp"
#include "micant/um/subsystems/net/wlanapi.hpp"
#include "micant/um/subsystems/winbase/virtdisk.hpp"
#include "micant/um/subsystems/security/fveapi.hpp"
#include "micant/um/subsystems/net/fwpuclnt.hpp"
#include "micant/um/subsystems/security/feclient.hpp"
#include "micant/um/subsystems/security/wscapi.hpp"
#include "micant/um/subsystems/security/amsi.hpp"
#include "micant/um/subsystems/security/mpengine.hpp"
#include "micant/um/subsystems/security/exploit_guard.hpp"
#include "micant/km/drivers/security/credguard.hpp"
#include "micant/km/drivers/security/cet.hpp"
#include "micant/km/drivers/compute/qat.hpp"
#include "micant/km/drivers/security/tee.hpp"
#include "micant/km/drivers/compute/dsa.hpp"
#include "micant/km/drivers/compute/amx.hpp"
#include "micant/km/drivers/bus/sriov.hpp"
#include "micant/km/hal/iommu.hpp"
#include "micant/km/hal/uefi_rt.hpp"
#include "micant/km/drivers/compute/modern_standby.hpp"
#include "micant/um/subsystems/com/wsa.hpp"
#include "micant/um/subsystems/winuser/touchpad.hpp"
#include "micant/um/subsystems/winuser/ink.hpp"
#include "micant/um/subsystems/media/spatial_audio.hpp"
#include "micant/km/drivers/input/hpd.hpp"
#include "micant/km/drivers/input/cameracx.hpp"
#include "micant/km/drivers/video/vrr.hpp"
#include "micant/km/drivers/input/sensorscx.hpp"
#include "micant/km/drivers/net/mbbcx.hpp"
#include "micant/um/subsystems/media/pmp.hpp"
#include "micant/km/drivers/bus/vmbus.hpp"
#include "micant/km/drivers/bus/vpci.hpp"
#include "micant/km/drivers/security/vsm.hpp"
#include "micant/km/drivers/security/hotpatch.hpp"
#include "micant/um/servers/hyperv.hpp"
#include "micant/km/drivers/fs/refs.hpp"
#include "micant/km/drivers/fs/csvfs.hpp"
#include "micant/km/drivers/fs/wcifs.hpp"
#include "micant/km/drivers/storage/s2d.hpp"
#include "micant/um/servers/branchcache.hpp"
#include "micant/km/drivers/storage/storage_replica.hpp"
#include "micant/um/servers/clustering.hpp"
#include "micant/um/servers/vmms.hpp"
#include "micant/um/servers/activedirectory.hpp"
#include "micant/um/subsystems/mgmt/grouppolicy.hpp"
#include "micant/um/servers/remotedesktop.hpp"
#include "micant/um/servers/nps.hpp"
#include "micant/um/servers/wsrm.hpp"
#include "micant/um/subsystems/mgmt/wds.hpp"
#include "micant/um/servers/certsrv.hpp"
#include "micant/um/servers/dns_server.hpp"
#include "micant/um/servers/dhcp_server.hpp"
#include "micant/um/servers/iis_server.hpp"
#include "micant/um/servers/wsus_server.hpp"
#include "micant/um/servers/winrm_server.hpp"
#include "micant/um/servers/ssh.hpp"
#include "micant/um/servers/rdp.hpp"
#include "micant/um/subsystems/winuser/input_router.hpp"
#include "micant/km/hal/boot_event_loop.hpp"
#include "micant/um/subsystems/net/mpr.hpp"
#include "micant/um/subsystems/mgmt/dbghelp.hpp"
#include "micant/um/subsystems/winbase/cfgmgr32.hpp"
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
    if (argc > 1 && std::string(argv[1]) == "--apps") {
        std::cout << "========================================================================\n";
        std::cout << "   MicaNT Retail Application Subsystems Suite (Suites 215 - 228)\n";
        std::cout << "========================================================================\n";
        RUN_TEST(Test_InteractiveWindowManager_InputRouting_Subsystem);
        RUN_TEST(Test_BareMetalEventLoop_WizTreeMFT_Subsystem);
        RUN_TEST(Test_PuTTYTerminal_AnsiWin32_Subsystem);
        RUN_TEST(Test_SumatraPDF_Gdiplus_Subsystem);
        RUN_TEST(Test_Everything_Search_Indexing_Subsystem);
        RUN_TEST(Test_WinMerge_Visual_Diff_Subsystem);
        RUN_TEST(Test_Retail_Ecosystem_100_Percent_Coverage);
        RUN_TEST(Test_Rufus_Storage_And_NtSyscalls_Suite);
        RUN_TEST(Test_SystemInformer_Diagnostics_And_NativeNT_Suite);
        RUN_TEST(Test_qBittorrent_Networking_AsyncIO_And_ICU_Suite);
        RUN_TEST(Test_WinSCP_RemoteFileManagement_And_AsyncNetwork_Suite);
        RUN_TEST(Test_Wireshark_NetworkPacketCapture_And_UCRT_Suite);
        RUN_TEST(Test_FileZilla_FtpSftp_And_SovereignNetworking_Suite);
        RUN_TEST(Test_Tier4_Multimedia_Creative_Workstation_Ecosystem_Suite);
        std::cout << "\n------------------------------------------------------------------------\n";
        std::cout << "Summary: " << g_PassedTests << " Passed, " << g_FailedTests << " Failed\n";
        std::cout << "------------------------------------------------------------------------\n";
        return g_FailedTests;
    }
    if (argc > 1 && (std::string(argv[1]) == "--last" || std::string(argv[1]) == "--suite298")) {
        RUN_TEST(Test_Horizon82_PrismX_D3D12TranslationLayer_D3D11On12_And_D3D9On12_UnifiedFunnel);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite297") {
        RUN_TEST(Test_Horizon81_PrismX_DirectXMath_DirectXTex_DirectXMesh_And_DirectXTK12);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite296") {
        RUN_TEST(Test_Horizon80_PrismX_DirectXHeaders_DXCore_And_WddmD3DKMTScanoutRuntime);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite295") {
        RUN_TEST(Test_Horizon79_Ring3Wsl2Lxss_WhpHypervisor_EnterpriseRdp_And_Complete51UnitClosure);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite294") {
        RUN_TEST(Test_Horizon78_Ring3WorkstationApps_SystemInformer_Everything_Rufus_SumatraPDF_And_WinMerge);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite293") {
        RUN_TEST(Test_Horizon77_Ring3FiberNetApps_PuTTY_Wireshark_qBittorrent_WinSCP_And_FileZilla);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite292") {
        RUN_TEST(Test_Horizon76_Ring3BcryptCrypt32WinTrust_LsaIsoVault_And_ZeroLagCiTunnel);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite291") {
        RUN_TEST(Test_Horizon75_Ring3ComOleRpcShell_RpcSsOxid_And_MftNamespaceTunnel);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite290") {
        RUN_TEST(Test_Horizon74_Ring3WinsockWinHttpTls_AfdFastIo_Rio_And_WskTunnel);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite289") {
        RUN_TEST(Test_Horizon73_Ring3PrismAudio_XAudio2Spatial_WasapiMf_And_WaveRtTunnel);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite288") {
        RUN_TEST(Test_Horizon72_Ring3DirectStorage_GDeflateBypassIo_And_Ring2FontWicSandbox);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite287") {
        RUN_TEST(Test_Horizon71_Ring3PrismX_Prism3D12_ShaderVM_And_DirectFlipMpoTunnel);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite286") {
        RUN_TEST(Test_Horizon70_Ring3RealAppsTriad_NotepadPlusPlus_Vlc_7Zip_And_FourRingSeal);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite285") {
        RUN_TEST(Test_Horizon69_Ring3WinlogonSession_ZeroWebView2Explorer_And_DirectFlipDwm);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite284") {
        RUN_TEST(Test_Horizon68_Ring3Win32BaseDlls_KnownDllsIat_And_ZeroSyscallKuser);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite283") {
        RUN_TEST(Test_Horizon67_Ring2ParallelScm_RpcSsDcomLaunch_And_Ring3HandoverGate);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite282") {
        RUN_TEST(Test_Horizon66_Ring2LsaLogonBroker_LsaIsoCredentialVault_And_KeyIsoDpapi);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite281") {
        RUN_TEST(Test_Horizon65_Ring2WudfDeviceHostPool_FontDrvSandbox_And_SyscallGuard);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite280") {
        RUN_TEST(Test_Horizon64_Ring1SmssBootstrap_KnownDllsPreMap_And_CsrssSuperServer);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite279") {
        RUN_TEST(Test_Horizon63_Ring1WaveRtCyclicDma_Sub2msWasapi_And_ApoDspGuard);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite278") {
        RUN_TEST(Test_Horizon62_Ring1Win32kSandbox_DirectFlipMpo_And_540HzCursorPlane);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite277") {
        RUN_TEST(Test_Horizon61_Ring1VfsMinifilterSandbox_BtreeWof_And_ZeroCopyFastIo);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite276") {
        RUN_TEST(Test_Horizon60_Ring0KuserSharedData_SyscallMsrSeal_And_NtdllUpcallTrampolines);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite275") {
        RUN_TEST(Test_Horizon59_Ring0AfdFastIo_RioZeroCopyRing_And_WskDirectProvider);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite274") {
        RUN_TEST(Test_Horizon58_Ring0SrmAccess_AppContainerSandbox_And_ZeroLagCiEngine);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite273") {
        RUN_TEST(Test_Horizon57_ZeroFreezeUiHookGuard_InstantMftUsnSearch_And_ZeroCopyZipfldrSection);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite272") {
        RUN_TEST(Test_Horizon56_AntiBloatGdiLeakGovernor_SvchostSpinHealer_And_PrimesDragRace);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite271") {
        RUN_TEST(Test_Horizon55_NtQuerySystemInformation_TaskmgrKernelTimes_And_MmcssSmpScheduler);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite270") {
        RUN_TEST(Test_Horizon54_LiveDriverHotSwap_InpageStorageHealer_And_VtDIommuDmaFirewall);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite269") {
        RUN_TEST(Test_Horizon53_ZeroBsodGuard_DpcWatchdog_PowerIrp_HighIrqlFault_And_SafeListRepair);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite268") {
        RUN_TEST(Test_Horizon52_Umdf2Reflector_SafeChannelCrowdStrikeGuard_And_XeonMsixRouter);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite267") {
        RUN_TEST(Test_Horizon51_RuntimePoFx_LiveHotplugRebalance_And_WheaHardwareErrorEngine);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite266") {
        RUN_TEST(Test_Horizon50_AntiVistaMeDriverResilience_PnpRank_LastKnownGood_And_Tdr);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite265") {
        RUN_TEST(Test_Horizon49_BootDriverScheduler_ServiceGroupOrder_StartOverride_And_LiveBusScan);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite264") {
        RUN_TEST(Test_Horizon48_RealNvidiaAndIntelSysDrivers_WmiWpp_And_OpenGpuGspRm);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite263") {
        RUN_TEST(Test_Horizon47_HardwareResourceArbiter_WdkSysLoader_And_InfDriverStaging);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite262") {
        RUN_TEST(Test_Horizon46_DriverVerifier_SpecialPool_IrpVerification_And_FaultInjection);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite261") {
        RUN_TEST(Test_Horizon45_KernelTransactionManager_TwoPhaseCommit_CLFS_And_Ktmw32);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite260") {
        RUN_TEST(Test_Horizon44_UserModeDebuggingExecutive_DebugObject_Dbgk_And_DbgUi);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite259") {
        RUN_TEST(Test_Horizon43_FileSystemRuntimeLibrary_Mcb_FileLock_Oplock_Notify_And_Tunnel);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite258") {
        RUN_TEST(Test_Horizon42_ExecutiveCacheManager_VACB_LazyWriter_And_Pinning);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite257") {
        RUN_TEST(Test_Horizon41_HIDClass_ReportParser_And_KernelInputStack);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite256") {
        RUN_TEST(Test_Horizon40_USB32_xHCI_HostController_And_HubArchitecture);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite255") {
        RUN_TEST(Test_Horizon39_KernelCrashDumpEngine_BugCheck_And_BSOD);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite254") {
        RUN_TEST(Test_Horizon38_FilterManager_Minifilter_Architecture);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite253") {
        RUN_TEST(Test_Horizon37_Ring1_PrivilegedServiceDomain_And_FaultIsolation);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite252") {
        RUN_TEST(Test_Horizon36_KernelToUser_ShadowSSDT_And_Win32k_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite251") {
        RUN_TEST(Test_Horizon35_ACPI_HAL_DeviceTree_And_AML_Interpreter);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite250") {
        RUN_TEST(Test_Horizon34_NDIS6_NetAdapterCx_Networking_Architecture);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite249") {
        RUN_TEST(Test_Horizon33_WDDM32_Display_Driver_Model_And_Graphics_Subsystem);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite248") {
        RUN_TEST(Test_Horizon32_StorPort_Nvme_Storage_Architecture_And_Miniport);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite247") {
        RUN_TEST(Test_Horizon31_WindowsDriverFramework_KMDF_ObjectTree_PnP_Queues_And_Requests);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite246") {
        RUN_TEST(Test_Horizon30_KernelDebugger_KD64_Breakpoints_And_KdpTrapExceptionRouting);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite245") {
        RUN_TEST(Test_Horizon29_PlugAndPlayManager_DeviceTree_IrpStateEngine_And_Interfaces);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite244") {
        RUN_TEST(Test_Horizon28_HardwareAbstractionLayer_Interrupts_APIC_HPET_And_Timers);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite243") {
        RUN_TEST(Test_Horizon27_ExecutiveSynchronization_ERESOURCE_PushLocks_And_BufferProbing);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite242") {
        RUN_TEST(Test_Horizon26_EventTracingForWindows_Sessions_CircularBuffers_And_RealtimeDispatch);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite241") {
        RUN_TEST(Test_Horizon25_SecurityReferenceMonitor_GenericMapping_Inheritance_And_Impersonation);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite240") {
        RUN_TEST(Test_Horizon24_ProcessSubsystem_Quotas_JobObjects_Callbacks_And_Affinity);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite239") {
        RUN_TEST(Test_Horizon23_IoSubsystem_FastIo_Cancellation_And_BufferingModes);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite238") {
        RUN_TEST(Test_Horizon22_ConfigurationManager_HiveRefinement_Coalescing_And_Security);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite237") {
        RUN_TEST(Test_Horizon21_ExecutiveLPC_ALPC_ThreeWayHandshake_And_Impersonation);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite228") {
        RUN_TEST(Test_Tier4_Multimedia_Creative_Workstation_Ecosystem_Suite);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite227") {
        RUN_TEST(Test_FileZilla_FtpSftp_And_SovereignNetworking_Suite);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite226") {
        RUN_TEST(Test_Wireshark_NetworkPacketCapture_And_UCRT_Suite);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite225") {
        RUN_TEST(Test_WinSCP_RemoteFileManagement_And_AsyncNetwork_Suite);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite224") {
        RUN_TEST(Test_qBittorrent_Networking_AsyncIO_And_ICU_Suite);
        return g_FailedTests;
    }
    if (argc > 1 && std::string(argv[1]) == "--suite223") {
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
    RUN_TEST(Test_qBittorrent_Networking_AsyncIO_And_ICU_Suite);
    RUN_TEST(Test_WinSCP_RemoteFileManagement_And_AsyncNetwork_Suite);
    RUN_TEST(Test_Wireshark_NetworkPacketCapture_And_UCRT_Suite);
    RUN_TEST(Test_FileZilla_FtpSftp_And_SovereignNetworking_Suite);
    RUN_TEST(Test_Tier4_Multimedia_Creative_Workstation_Ecosystem_Suite);
    RUN_TEST(Test_Horizon13_SovereignPowerManagement_PoSubsystem);
    RUN_TEST(Test_Horizon14_ExecutiveWorkQueues_And_IoWorkItems);
    RUN_TEST(Test_Horizon15_KernelTimers_DpcWheel_And_ExTimer);
    RUN_TEST(Test_Horizon16_InterProcessorInterrupts_SMP_And_Barriers);
    RUN_TEST(Test_Horizon17_MemoryManager_MDL_ContiguousMemory_And_WorkingSet);
    RUN_TEST(Test_Horizon18_SectionObjects_MemoryMappedFiles_And_ControlAreaArchitecture);
    RUN_TEST(Test_Horizon19_DispatcherDatabase_MultiWait_And_PriorityInheritance);
    RUN_TEST(Test_Horizon20_ObjectManager_Lifecycle_Symlinks_And_Callbacks);
    RUN_TEST(Test_Horizon21_ExecutiveLPC_ALPC_ThreeWayHandshake_And_Impersonation);
    RUN_TEST(Test_Horizon22_ConfigurationManager_HiveRefinement_Coalescing_And_Security);
    RUN_TEST(Test_Horizon23_IoSubsystem_FastIo_Cancellation_And_BufferingModes);
    RUN_TEST(Test_Horizon24_ProcessSubsystem_Quotas_JobObjects_Callbacks_And_Affinity);
    RUN_TEST(Test_Horizon25_SecurityReferenceMonitor_GenericMapping_Inheritance_And_Impersonation);
    RUN_TEST(Test_Horizon26_EventTracingForWindows_Sessions_CircularBuffers_And_RealtimeDispatch);
    RUN_TEST(Test_Horizon27_ExecutiveSynchronization_ERESOURCE_PushLocks_And_BufferProbing);
    RUN_TEST(Test_Horizon28_HardwareAbstractionLayer_Interrupts_APIC_HPET_And_Timers);
    RUN_TEST(Test_Horizon29_PlugAndPlayManager_DeviceTree_IrpStateEngine_And_Interfaces);
    RUN_TEST(Test_Horizon30_KernelDebugger_KD64_Breakpoints_And_KdpTrapExceptionRouting);
    RUN_TEST(Test_Horizon31_WindowsDriverFramework_KMDF_ObjectTree_PnP_Queues_And_Requests);
    RUN_TEST(Test_Horizon32_StorPort_Nvme_Storage_Architecture_And_Miniport);
    RUN_TEST(Test_Horizon33_WDDM32_Display_Driver_Model_And_Graphics_Subsystem);
    RUN_TEST(Test_Horizon34_NDIS6_NetAdapterCx_Networking_Architecture);
    RUN_TEST(Test_Horizon35_ACPI_HAL_DeviceTree_And_AML_Interpreter);
    RUN_TEST(Test_Horizon36_KernelToUser_ShadowSSDT_And_Win32k_Subsystem);
    RUN_TEST(Test_Horizon37_Ring1_PrivilegedServiceDomain_And_FaultIsolation);
    RUN_TEST(Test_Horizon38_FilterManager_Minifilter_Architecture);
    RUN_TEST(Test_Horizon39_KernelCrashDumpEngine_BugCheck_And_BSOD);
    RUN_TEST(Test_Horizon40_USB32_xHCI_HostController_And_HubArchitecture);
    RUN_TEST(Test_Horizon41_HIDClass_ReportParser_And_KernelInputStack);
    RUN_TEST(Test_Horizon42_ExecutiveCacheManager_VACB_LazyWriter_And_Pinning);
    RUN_TEST(Test_Horizon43_FileSystemRuntimeLibrary_Mcb_FileLock_Oplock_Notify_And_Tunnel);
    RUN_TEST(Test_Horizon44_UserModeDebuggingExecutive_DebugObject_Dbgk_And_DbgUi);
    RUN_TEST(Test_Horizon45_KernelTransactionManager_TwoPhaseCommit_CLFS_And_Ktmw32);
    RUN_TEST(Test_Horizon46_DriverVerifier_SpecialPool_IrpVerification_And_FaultInjection);
    RUN_TEST(Test_Horizon47_HardwareResourceArbiter_WdkSysLoader_And_InfDriverStaging);
    RUN_TEST(Test_Horizon48_RealNvidiaAndIntelSysDrivers_WmiWpp_And_OpenGpuGspRm);
    RUN_TEST(Test_Horizon49_BootDriverScheduler_ServiceGroupOrder_StartOverride_And_LiveBusScan);
    RUN_TEST(Test_Horizon50_AntiVistaMeDriverResilience_PnpRank_LastKnownGood_And_Tdr);
    RUN_TEST(Test_Horizon51_RuntimePoFx_LiveHotplugRebalance_And_WheaHardwareErrorEngine);
    RUN_TEST(Test_Horizon52_Umdf2Reflector_SafeChannelCrowdStrikeGuard_And_XeonMsixRouter);
    RUN_TEST(Test_Horizon53_ZeroBsodGuard_DpcWatchdog_PowerIrp_HighIrqlFault_And_SafeListRepair);
    RUN_TEST(Test_Horizon54_LiveDriverHotSwap_InpageStorageHealer_And_VtDIommuDmaFirewall);
    RUN_TEST(Test_Horizon55_NtQuerySystemInformation_TaskmgrKernelTimes_And_MmcssSmpScheduler);
    RUN_TEST(Test_Horizon56_AntiBloatGdiLeakGovernor_SvchostSpinHealer_And_PrimesDragRace);
    RUN_TEST(Test_Horizon57_ZeroFreezeUiHookGuard_InstantMftUsnSearch_And_ZeroCopyZipfldrSection);
    RUN_TEST(Test_Horizon58_Ring0SrmAccess_AppContainerSandbox_And_ZeroLagCiEngine);
    RUN_TEST(Test_Horizon59_Ring0AfdFastIo_RioZeroCopyRing_And_WskDirectProvider);
    RUN_TEST(Test_Horizon60_Ring0KuserSharedData_SyscallMsrSeal_And_NtdllUpcallTrampolines);
    RUN_TEST(Test_Horizon61_Ring1VfsMinifilterSandbox_BtreeWof_And_ZeroCopyFastIo);
    RUN_TEST(Test_Horizon62_Ring1Win32kSandbox_DirectFlipMpo_And_540HzCursorPlane);
    RUN_TEST(Test_Horizon63_Ring1WaveRtCyclicDma_Sub2msWasapi_And_ApoDspGuard);
    RUN_TEST(Test_Horizon64_Ring1SmssBootstrap_KnownDllsPreMap_And_CsrssSuperServer);
    RUN_TEST(Test_Horizon65_Ring2WudfDeviceHostPool_FontDrvSandbox_And_SyscallGuard);
    RUN_TEST(Test_Horizon66_Ring2LsaLogonBroker_LsaIsoCredentialVault_And_KeyIsoDpapi);
    RUN_TEST(Test_Horizon67_Ring2ParallelScm_RpcSsDcomLaunch_And_Ring3HandoverGate);
    RUN_TEST(Test_Horizon68_Ring3Win32BaseDlls_KnownDllsIat_And_ZeroSyscallKuser);
    RUN_TEST(Test_Horizon69_Ring3WinlogonSession_ZeroWebView2Explorer_And_DirectFlipDwm);
    RUN_TEST(Test_Horizon70_Ring3RealAppsTriad_NotepadPlusPlus_Vlc_7Zip_And_FourRingSeal);
    RUN_TEST(Test_Horizon71_Ring3PrismX_Prism3D12_ShaderVM_And_DirectFlipMpoTunnel);
    RUN_TEST(Test_Horizon72_Ring3DirectStorage_GDeflateBypassIo_And_Ring2FontWicSandbox);
    RUN_TEST(Test_Horizon73_Ring3PrismAudio_XAudio2Spatial_WasapiMf_And_WaveRtTunnel);
    RUN_TEST(Test_Horizon74_Ring3WinsockWinHttpTls_AfdFastIo_Rio_And_WskTunnel);
    RUN_TEST(Test_Horizon75_Ring3ComOleRpcShell_RpcSsOxid_And_MftNamespaceTunnel);
    RUN_TEST(Test_Horizon76_Ring3BcryptCrypt32WinTrust_LsaIsoVault_And_ZeroLagCiTunnel);
    RUN_TEST(Test_Horizon77_Ring3FiberNetApps_PuTTY_Wireshark_qBittorrent_WinSCP_And_FileZilla);
    RUN_TEST(Test_Horizon78_Ring3WorkstationApps_SystemInformer_Everything_Rufus_SumatraPDF_And_WinMerge);
    RUN_TEST(Test_Horizon79_Ring3Wsl2Lxss_WhpHypervisor_EnterpriseRdp_And_Complete51UnitClosure);
    RUN_TEST(Test_Horizon80_PrismX_DirectXHeaders_DXCore_And_WddmD3DKMTScanoutRuntime);
    RUN_TEST(Test_Horizon81_PrismX_DirectXMath_DirectXTex_DirectXMesh_And_DirectXTK12);
    RUN_TEST(Test_Horizon82_PrismX_D3D12TranslationLayer_D3D11On12_And_D3D9On12_UnifiedFunnel);

    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << "Summary: " << g_PassedTests << " Passed, " << g_FailedTests << " Failed\n";
    std::cout << "------------------------------------------------------------------------\n";

    return (g_FailedTests == 0) ? 0 : 1;
}


