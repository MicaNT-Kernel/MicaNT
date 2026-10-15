cmake_minimum_required(VERSION 3.25)
project(MicaNT VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Canonical Include Root (all headers addressed via micant/<layer>/<header>.hpp)
include_directories(
    include
)

# Modular NT Kernel Executive & Microkernel Subsystem Library (base/ntos)
set(NTOSKRNL_SUBSYSTEM_SOURCES
    kernel/ke/dispatcher.cpp
    kernel/ke/wait.cpp
    kernel/mm/virtual.cpp
    kernel/io/iomgr.cpp
    kernel/ob/obref.cpp
    kernel/ps/psmgr.cpp
    kernel/cm/ntreg.cpp
    kernel/se/ntsec.cpp
    kernel/lpc/ntlpc.cpp
    kernel/cc/ccmgr.cpp
    kernel/fsrtl/fsrtlmgr.cpp
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
)

add_library(ntoskrnl_core STATIC
    ${NTOSKRNL_SUBSYSTEM_SOURCES}
)

# Core Kernel Executable (ntoskrnl / kiinit)
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

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ntoskrnl_core PRIVATE -Wall -Wextra -Wpedantic -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    target_compile_options(micant_kernel PRIVATE -Wall -Wextra -Wpedantic -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    target_compile_options(micant_tests PRIVATE -Wall -Wextra -Wpedantic -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    target_compile_options(userland_app PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    target_compile_options(win32_app PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
    target_compile_options(bootx64 PRIVATE -Wall -Wextra -Wpedantic -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-unused-parameter -Wno-missing-designated-field-initializers -O2)
elseif (MSVC)
    target_compile_options(ntoskrnl_core PRIVATE /W4 /permissive- /O2)
    target_compile_options(micant_kernel PRIVATE /W4 /permissive- /O2)
    target_compile_options(micant_tests PRIVATE /W4 /permissive- /O2)
    target_compile_options(userland_app PRIVATE /W4 /permissive- /O2)
    target_compile_options(win32_app PRIVATE /W4 /permissive- /O2)
    target_compile_options(bootx64 PRIVATE /W4 /permissive- /O2)
endif()

set_target_properties(micant_kernel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
set_target_properties(micant_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
set_target_properties(userland_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
set_target_properties(win32_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
