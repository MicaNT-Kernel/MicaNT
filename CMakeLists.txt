cmake_minimum_required(VERSION 3.25)
project(MicaNT VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include Directories
include_directories(
    include
)

# Core Kernel Executable
add_executable(micant_kernel
    kernel/main.cpp
    kernel/syscalls.cpp
    kernel/dispatcher.cpp
)

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
    kernel/syscalls.cpp
    kernel/dispatcher.cpp
)

target_compile_definitions(micant_tests PRIVATE
    _UNICODE
    UNICODE
)

# Standalone Ring 3 Userland Test Application
add_executable(userland_app
    test/userland_app.cpp
    kernel/syscalls.cpp
    kernel/dispatcher.cpp
)

# Standalone Win32 Userland Test Application
add_executable(win32_app
    test/win32_app.cpp
    kernel/syscalls.cpp
    kernel/dispatcher.cpp
)

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
    target_compile_options(micant_kernel PRIVATE -Wall -Wextra -Wpedantic -O2)
    target_compile_options(micant_tests PRIVATE -Wall -Wextra -Wpedantic -O2)
    target_compile_options(userland_app PRIVATE -Wall -Wextra -O2)
    target_compile_options(win32_app PRIVATE -Wall -Wextra -O2)
    target_compile_options(bootx64 PRIVATE -Wall -Wextra -Wpedantic -O2)
elseif (MSVC)
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

