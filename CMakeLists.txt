cmake_minimum_required(VERSION 3.25)
project(MicaNT VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(MICANT_ENABLE_BAREMETAL "Build the AMD64 freestanding PE boot targets" OFF)
option(MICANT_BOOTSTRAP_RUNTIME_SELFTEST "Halt after kernel C++ lifecycle verification" OFF)

# Canonical Include Root (all headers addressed via micant/<layer>/<header>.hpp)
include_directories(
    include
)

# Explicit modular NT Kernel Executive & Microkernel source manifest.
# Historical sources under old/ are never kernel or driver build inputs.
set(NTOSKRNL_SUBSYSTEM_SOURCES
    kernel/ke/dispatcher.cpp
    kernel/ke/context_switch.cpp
    kernel/ke/wait.cpp
    kernel/win32k/win32k.cpp
    kernel/win32k/win32k_wddm.cpp
    kernel/mm/virtual.cpp
    kernel/io/iomgr.cpp
    kernel/ob/obref.cpp
    kernel/ps/psmgr.cpp
    kernel/cm/ntreg.cpp
    kernel/se/ntsec.cpp
    kernel/lpc/ntlpc.cpp
    kernel/cc/ccmgr.cpp
    kernel/fsrtl/fsrtlmgr.cpp
    kernel/fsrtl/extfs.cpp
    kernel/fsrtl/ntfs.cpp
    kernel/fsrtl/exfat.cpp
    kernel/fsrtl/cdfs.cpp
    kernel/fsrtl/refs.cpp
    kernel/fsrtl/zfs.cpp
    kernel/dbgk/dbgkobj.cpp
    kernel/tm/ktmmgr.cpp
    kernel/verifier/vfmgr.cpp
    kernel/arb/arbmgr.cpp
    kernel/wmi/wmimgr.cpp
    kernel/io/pnpboot.cpp
    kernel/io/pnpresilience.cpp
    kernel/ex/exmgr.cpp
    kernel/hal/halinit.cpp
    kernel/pnp/pnpmgr.cpp
    kernel/po/pomgr.cpp
    kernel/po/pofx_whea.cpp
    kernel/po/bms.cpp
    kernel/po/ppm.cpp
    kernel/po/thermal.cpp
    kernel/io/umdf_msix.cpp
    kernel/ke/zerobsod.cpp
    kernel/io/livehotswap.cpp
    kernel/ex/sysinfo_taskmgr.cpp
    kernel/ps/quotagov.cpp
    kernel/fsrtl/mftzipui.cpp
    kernel/se/srm_ci.cpp
    kernel/io/afd_wsk.cpp
    kernel/ke/kuser_seal.cpp
    kernel/fsrtl/ring1_vfs.cpp
    kernel/io/ring1_dxgk.cpp
    kernel/io/ring1_audio.cpp
    kernel/ps/ring1_smss.cpp
    kernel/io/ring2_wudf.cpp
    kernel/se/ring2_lsa.cpp
    kernel/ps/ring2_scm.cpp
    kernel/ps/ring3_win32_base.cpp
    kernel/ps/ring3_desktop_shell.cpp
    kernel/ps/ring3_real_apps.cpp
    kernel/io/ring3_prismx_3d12.cpp
    kernel/io/ring3_directstorage_d2d.cpp
    kernel/io/ring3_prismaudio_mf.cpp
    kernel/io/ring3_winsock_winhttp_tls.cpp
    kernel/lpc/ring3_com_rpc_shell.cpp
    kernel/se/ring3_bcrypt_crypt32_wintrust.cpp
    kernel/io/ring3_fiber_net_apps.cpp
    kernel/ps/ring3_workstation_apps.cpp
    kernel/ps/ring3_wsl2_hyperv_closure.cpp
    kernel/io/prismx_directx_headers_kmt.cpp
    kernel/io/prismx_dxmath_dxtex_dxmesh.cpp
    kernel/io/prismx_d3d12_translation_on12.cpp
    kernel/io/prismx_dxc_sm68_pix.cpp
    kernel/io/prismx_directstorage_gdeflate.cpp
    kernel/io/prismx_directml_superres.cpp
    kernel/io/prismx_enhanced_barriers_workgraphs.cpp
    kernel/io/prismx_vulkanon12_video_av1.cpp
    kernel/io/prismx_dred_shadercache_zerotdr.cpp
    kernel/io/prismx_live_3d_scene_framegraph.cpp
    kernel/io/prismx_gop_scanout_frame_pacing.cpp
    kernel/io/prismx_game_benchmark_proton_seal.cpp
    kernel/io/prismx_directsr_vrs_rebar_gaming.cpp
    kernel/io/prismx_execute_indirect_sampler_feedback.cpp
    kernel/io/prismx_gameinput_pix_classic_gaming_seal.cpp
    kernel/io/prismx_hlsl_fxc_dxbc_reflection_compiler.cpp
    kernel/io/prismx_dxil_sm69_wave32_simd_vm.cpp
    kernel/io/prismx_spirv16_dxil_cross_translator_seal.cpp
    kernel/se/ring0_anticheat_eac_battleye_callbacks.cpp
    kernel/se/ring0_vanguard_ricochet_hvci_iommu_shield.cpp
    kernel/se/horizon100_gaming_drm_hwid_vault_seal.cpp
    kernel/io/prism_dxgi16_com_hierarchy.cpp
    kernel/io/prism_d3d12_vtable_options21.cpp
    kernel/io/prism_directx_system32_dll_router.cpp
    kernel/io/prism_d3d9ex_com_vtable_abi.cpp
    kernel/io/prism_xaudio2_dinput8_com_abi.cpp
    kernel/io/prism_d2d1_dwrite_dcomp_11dll_seal.cpp
    kernel/io/prism_mfplat_mfreadwrite_cutscene_abi.cpp
    kernel/io/prism_dxcore_adapter_factory_com_abi.cpp
    kernel/io/prism_vulkan1_opengl32_16dll_seal.cpp
    kernel/ps/winnt_kernel32_advapi32_system32_abi.cpp
    kernel/ps/winnt_ole32_dwmapi_winmm_system32_seal.cpp
    kernel/ps/winnt_ntdll_ws2_iphlpapi_winhttp_abi.cpp
    kernel/se/winnt_bcrypt_crypt32_secur32_wintrust_abi.cpp
    kernel/ps/winnt_shell32_dbghelp_version_40dll_iat_seal.cpp
    kernel/ps/winnt_ucrtbase_runtime_abi.cpp
    kernel/ps/winnt_vcruntime140_eh_abi.cpp
    kernel/ps/winnt_msvcp140_pdata_unwind_44dll_seal.cpp
)

add_library(ntoskrnl_core STATIC
    ${NTOSKRNL_SUBSYSTEM_SOURCES}
)

# Win32 user APIs are independent user-mode PE DLLs. Keep their implementation
# out of ntoskrnl_core; each DLL is built from the shared ABI source with only
# its own export surface enabled.
if(WIN32)
    # The window manager has one process-wide state owner. User32 resolves this
    # DLL at runtime so it never embeds a private copy of TitanWin32kSubsystem.
    add_library(micant_win32k SHARED kernel/ps/winnt_win32k_server.cpp)
    target_compile_definitions(micant_win32k PRIVATE BUILD_SYSTEM32_STANDALONE_DLL MICANT_WIN32K_EXPORTS)
    if(MINGW)
        target_link_options(micant_win32k PRIVATE -static)
    endif()
    set_target_properties(micant_win32k PROPERTIES
        OUTPUT_NAME "win32k"
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/system32"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/system32")

    foreach(_dll IN ITEMS USER32 GDI32 IMM32)
        string(TOLOWER "${_dll}" _dll_lower)
        add_library(micant_${_dll_lower} SHARED
            kernel/ps/winnt_user32_gdi32_imm32_system32_abi.cpp)
        add_dependencies(micant_${_dll_lower} micant_win32k)
        target_compile_definitions(micant_${_dll_lower} PRIVATE
            BUILD_SYSTEM32_${_dll}_DLL BUILD_SYSTEM32_STANDALONE_DLL MICANT_WIN32K_IMPORTS)
        target_link_libraries(micant_${_dll_lower} PRIVATE micant_win32k)
        if(MINGW)
            target_link_options(micant_${_dll_lower} PRIVATE -static)
        endif()
        set_target_properties(micant_${_dll_lower} PROPERTIES
            OUTPUT_NAME "${_dll_lower}"
            PREFIX ""
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/system32"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/system32")
    endforeach()
endif()

# Hosted initialization harness. This is not a freestanding kernel image.
add_executable(micant_kernel
    kernel/init/kiinit.cpp
)
target_link_libraries(micant_kernel PRIVATE ntoskrnl_core)

target_compile_definitions(micant_kernel PRIVATE
    MICANT_VERSION_MAJOR=0
    MICANT_VERSION_MINOR=1
    MICANT_VERSION_PATCH=0
    _UNICODE
    UNICODE
)

# Unit Test Runner
add_executable(micant_tests
    test/test_runner.cpp
)
target_link_libraries(micant_tests PRIVATE ntoskrnl_core)

target_compile_definitions(micant_tests PRIVATE
    _UNICODE
    UNICODE
)

# Standalone Ring 3 Userland Test Application
add_executable(userland_app
    test/userland_app.cpp
)
target_link_libraries(userland_app PRIVATE ntoskrnl_core)

# Standalone Win32 Userland Test Application
add_executable(win32_app
    test/win32_app.cpp
)
target_link_libraries(win32_app PRIVATE ntoskrnl_core)

# Standalone PrismX 3D & Vulkan / Direct3D 12 Hardware Verification Harness
add_executable(prismx_3d_cube_app
    test/prismx_3d_cube_app.cpp
)
target_link_libraries(prismx_3d_cube_app PRIVATE ntoskrnl_core)
target_link_options(prismx_3d_cube_app PRIVATE -static)

# Standalone Unmodified Third-Party Windows Test Application (Zero MicaNT headers)
if (MSVC)
    add_executable(unmodified_sample
        test/unmodified_sample.cpp
    )
    target_compile_options(unmodified_sample PRIVATE /O2 /GS-)
    target_link_options(unmodified_sample PRIVATE /SUBSYSTEM:CONSOLE /NODEFAULTLIB /ENTRY:entry)
    target_link_libraries(unmodified_sample PRIVATE kernel32)
    set_target_properties(unmodified_sample PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# Bare-Metal UEFI Bootloader (bootx64)
add_library(bootx64 OBJECT
    boot/bootx64.cpp
)

if (MICANT_ENABLE_BAREMETAL)
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "The freestanding AMD64 PE targets require a Clang/GNU cross-toolchain.")
    endif()

    add_executable(ntoskrnl_core_baremetal
        kernel/init/kiinit.cpp
        kernel/ke/context_switch.cpp
        kernel/hal/amd64_bootstrap.cpp
        kernel/win32k/freestanding.cpp
        kernel/ex/freestanding.cpp
        kernel/ex/libcpp_support.cpp
        kernel/ob/obref.cpp
        test/kernel_context_canary.cpp
        test/kernel_ke_ready_queue_test.cpp
        test/kernel_mm_avl_test.cpp
        tools/explorer_shell/desktop.cpp
        tools/explorer_shell/notification_area.cpp
        tools/explorer_shell/taskbar.cpp
    )
    target_include_directories(ntoskrnl_core_baremetal PRIVATE
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/tools"
    )
    target_compile_definitions(ntoskrnl_core_baremetal PRIVATE MICANT_FREESTANDING)
    if (MICANT_BOOTSTRAP_RUNTIME_SELFTEST)
        target_sources(ntoskrnl_core_baremetal PRIVATE test/kernel_cxx_lifecycle_fixture.cpp)
        target_compile_definitions(ntoskrnl_core_baremetal PRIVATE MICANT_KERNEL_CXX_LIFECYCLE_SELFTEST)
    endif()
    target_compile_options(ntoskrnl_core_baremetal PRIVATE -fno-builtin)

    add_executable(bootx64_efi
        boot/bootx64.cpp
    )
    target_include_directories(bootx64_efi PRIVATE "${CMAKE_SOURCE_DIR}")

    add_library(win32k_boot SHARED EXCLUDE_FROM_ALL kernel/ps/winnt_win32k_server.cpp)
    target_compile_definitions(win32k_boot PRIVATE
        BUILD_SYSTEM32_STANDALONE_DLL MICANT_WIN32K_EXPORTS
    )
    target_compile_options(win32k_boot PRIVATE
        -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
        -ffunction-sections -fdata-sections -O2
    )
    target_link_options(win32k_boot PRIVATE -static -Wl,--gc-sections)
    set_target_properties(win32k_boot PROPERTIES
        PREFIX "" OUTPUT_NAME "win32k"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/system32"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/boot-imports"
    )

    foreach(_client IN ITEMS user32 gdi32 imm32)
        string(TOUPPER "${_client}" _client_upper)
        add_library(${_client}_boot SHARED EXCLUDE_FROM_ALL
            kernel/ps/winnt_user32_gdi32_imm32_system32_abi.cpp
        )
        target_compile_definitions(${_client}_boot PRIVATE
            BUILD_SYSTEM32_STANDALONE_DLL
            "BUILD_SYSTEM32_${_client_upper}_DLL" MICANT_WIN32K_IMPORTS
        )
        target_compile_options(${_client}_boot PRIVATE
            -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
            -ffunction-sections -fdata-sections -O2
        )
        target_link_options(${_client}_boot PRIVATE -static -Wl,--gc-sections)
        target_link_libraries(${_client}_boot PRIVATE win32k_boot)
        set_target_properties(${_client}_boot PROPERTIES
            PREFIX "" OUTPUT_NAME "${_client}"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/system32"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/boot-imports"
        )
    endforeach()

    add_executable(explorer_boot EXCLUDE_FROM_ALL
        tools/explorer_shell_app.cpp
        tools/explorer_shell/desktop.cpp
        tools/explorer_shell/taskbar.cpp
        tools/explorer_shell/start_menu.cpp
        tools/explorer_shell/notification_area.cpp
        tools/explorer_shell/task_switcher.cpp
    )
    target_include_directories(explorer_boot PRIVATE "${CMAKE_SOURCE_DIR}/tools")
    target_compile_options(explorer_boot PRIVATE
        -ffreestanding -fno-builtin -fno-exceptions -fno-rtti
        -fno-stack-protector -mno-red-zone -mno-stack-arg-probe -O2
    )
    target_link_options(explorer_boot PRIVATE
        -nostdlib -Wl,--subsystem,windows -Wl,-e,_start
    )
    target_link_libraries(explorer_boot PRIVATE user32_boot gdi32_boot win32k_boot)
    set_target_properties(explorer_boot PROPERTIES
        OUTPUT_NAME "explorer"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
    )
    add_library(kernel32_boot SHARED EXCLUDE_FROM_ALL
        kernel/ps/winnt_kernel32_advapi32_system32_abi.cpp
    )
    target_compile_definitions(kernel32_boot PRIVATE MICANT_FREESTANDING_KERNEL32)
    target_compile_options(kernel32_boot PRIVATE
        -ffreestanding -fno-builtin -fno-exceptions -fno-rtti
        -fno-threadsafe-statics -fno-use-cxa-atexit -fno-emulated-tls
        -fno-stack-protector -mno-red-zone -O2
    )
    target_link_options(kernel32_boot PRIVATE
        -nostdlib -Wl,--entry,DllMainCRTStartup
    )
    set_target_properties(kernel32_boot PROPERTIES
        PREFIX "" OUTPUT_NAME "kernel32"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/system32"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/boot-imports"
    )
    add_library(ucrtbase_boot SHARED EXCLUDE_FROM_ALL
        kernel/ps/winnt_ucrtbase_runtime_abi.cpp
    )
    target_compile_definitions(ucrtbase_boot PRIVATE MICANT_FREESTANDING_UCRT)
    target_compile_options(ucrtbase_boot PRIVATE
        -ffreestanding -fno-builtin -fno-exceptions -fno-rtti
        -fno-threadsafe-statics -fno-use-cxa-atexit -fno-emulated-tls
        -fno-stack-protector -mno-red-zone -O2
    )
    target_link_options(ucrtbase_boot PRIVATE
        -nostdlib -Wl,--entry,DllMainCRTStartup
    )
    set_target_properties(ucrtbase_boot PROPERTIES
        PREFIX "" OUTPUT_NAME "ucrtbase"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/system32"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/boot-imports"
    )
    add_custom_target(explorer_runtime_boot
        DEPENDS explorer_boot imm32_boot kernel32_boot ucrtbase_boot
    )

    add_executable(virtio_gpu_boot drivers/virtio_gpu/virtio_gpu.cpp)
    set_target_properties(virtio_gpu_boot PROPERTIES
        OUTPUT_NAME "virtio_gpu"
        SUFFIX ".sys"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build"
    )
endif()

if(WIN32 OR MICANT_ENABLE_BAREMETAL)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_custom_target(micant_user_mode_boundary_check)
    foreach(_user_mode_variant IN ITEMS
        micant_win32k micant_user32 micant_gdi32 micant_imm32
        kernel32_boot ucrtbase_boot win32k_boot user32_boot gdi32_boot
        imm32_boot explorer_boot explorer_runtime_boot
    )
        add_custom_target(micant_boundary_${_user_mode_variant}
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/scripts/check_user_mode_boundary.py"
                --root "${CMAKE_SOURCE_DIR}"
                --compiler "${CMAKE_CXX_COMPILER}"
                --target "${_user_mode_variant}"
            VERBATIM
        )
        add_dependencies(micant_user_mode_boundary_check
                         micant_boundary_${_user_mode_variant})
        if(TARGET ${_user_mode_variant})
            add_dependencies(${_user_mode_variant}
                             micant_boundary_${_user_mode_variant})
        endif()
    endforeach()
    if(TARGET explorer_runtime_boot)
        add_dependencies(explorer_runtime_boot micant_boundary_explorer_runtime_boot)
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ntoskrnl_core PRIVATE -Wall -Wextra -Wpedantic -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    target_compile_options(micant_kernel PRIVATE -Wall -Wextra -Wpedantic -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    target_compile_options(micant_tests PRIVATE -Wall -Wextra -Wpedantic -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    target_compile_options(userland_app PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    target_compile_options(win32_app PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    target_compile_options(bootx64 PRIVATE -Wall -Wextra -Wpedantic -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    if (MICANT_ENABLE_BAREMETAL)
        foreach(_target IN ITEMS ntoskrnl_core_baremetal bootx64_efi virtio_gpu_boot)
            target_compile_options(${_target} PRIVATE
                -ffreestanding
                -fno-exceptions
                -fno-rtti
                -fno-threadsafe-statics
                -fno-stack-protector
                -mno-red-zone
                -ffunction-sections
                -fdata-sections
                -O2
            )
        endforeach()
        target_link_options(ntoskrnl_core_baremetal PRIVATE
            -nostdlib
            -Wl,--subsystem,native
            -Wl,-e,KiSystemStartup
            -Wl,--gc-sections
        )
        target_link_options(bootx64_efi PRIVATE
            -nostdlib
            -Wl,--subsystem,efi_application
            -Wl,-e,EfiMain
        )
        target_compile_options(virtio_gpu_boot PRIVATE -fno-builtin -mno-stack-arg-probe)
        target_link_options(virtio_gpu_boot PRIVATE
            -nostdlib -Wl,--subsystem,native -Wl,-e,DriverEntry
        )
    endif()
elseif (MSVC)
    target_compile_options(ntoskrnl_core PRIVATE /W4 /permissive- /O2)
    target_compile_options(micant_kernel PRIVATE /W4 /permissive- /O2)
    target_compile_options(micant_tests PRIVATE /W4 /permissive- /O2)
    target_compile_options(userland_app PRIVATE /W4 /permissive- /O2)
    target_compile_options(win32_app PRIVATE /W4 /permissive- /O2)
    target_compile_options(bootx64 PRIVATE /W4 /permissive- /O2)
endif()

set_target_properties(micant_kernel micant_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/host"
)
foreach(_config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${_config}" _config_upper)
    set_target_properties(micant_kernel micant_tests PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${_config_upper}" "${CMAKE_BINARY_DIR}/host"
    )
endforeach()
set_target_properties(userland_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
set_target_properties(win32_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
set_target_properties(prismx_3d_cube_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if (MICANT_ENABLE_BAREMETAL)
    set_target_properties(ntoskrnl_core_baremetal PROPERTIES
        OUTPUT_NAME "ntoskrnl"
        SUFFIX ".exe"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/baremetal"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build/baremetal"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/build/baremetal"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/build/baremetal"
    )
    set_target_properties(bootx64_efi PROPERTIES
        OUTPUT_NAME "BOOTX64"
        SUFFIX ".EFI"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/esp/EFI/BOOT"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build/esp/EFI/BOOT"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/build/esp/EFI/BOOT"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/build/esp/EFI/BOOT"
    )
endif()
