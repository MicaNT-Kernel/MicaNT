cmake_minimum_required(VERSION 3.25)
project(MicaNT VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include Directories
include_directories(
    include
)

# Core Executable
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

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(micant_kernel PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O2
    )
elseif (MSVC)
    target_compile_options(micant_kernel PRIVATE
        /W4
        /permissive-
        /O2
    )
endif()

set_target_properties(micant_kernel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
