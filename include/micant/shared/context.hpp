#pragma once

#include <cstdint>
#include <cstring>

namespace micant {

#ifndef CONTEXT_CONTROL
#define CONTEXT_CONTROL (0x00100000L | 0x1L)
#endif

#ifndef CONTEXT_INTEGER
#define CONTEXT_INTEGER (0x00100000L | 0x2L)
#endif

#ifndef CONTEXT_SEGMENTS
#define CONTEXT_SEGMENTS (0x00100000L | 0x4L)
#endif

#ifndef CONTEXT_FLOATING_POINT
#define CONTEXT_FLOATING_POINT (0x00100000L | 0x8L)
#endif

#ifndef CONTEXT_DEBUG_REGISTERS
#define CONTEXT_DEBUG_REGISTERS (0x00100000L | 0x10L)
#endif

#ifndef CONTEXT_FULL
#define CONTEXT_FULL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT)
#endif

#ifndef CONTEXT_ALL
#define CONTEXT_ALL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS)
#endif

#pragma pack(push, 16)
struct alignas(16) M128A {
    uint64_t Low;
    int64_t  High;
};

struct XMM_SAVE_AREA32 {
    uint16_t ControlWord;
    uint16_t StatusWord;
    uint8_t  TagWord;
    uint8_t  Reserved1;
    uint16_t ErrorOpcode;
    uint32_t ErrorOffset;
    uint16_t ErrorSelector;
    uint16_t Reserved2;
    uint32_t DataOffset;
    uint16_t DataSelector;
    uint16_t Reserved3;
    uint32_t MxCsr;
    uint32_t MxCsr_Mask;
    M128A    FloatRegisters[8];
    M128A    XmmRegisters[16];
    uint8_t  Reserved4[96];
};

struct alignas(16) CONTEXT {
    // Register parameter home addresses.
    uint64_t P1Home;
    uint64_t P2Home;
    uint64_t P3Home;
    uint64_t P4Home;
    uint64_t P5Home;
    uint64_t P6Home;

    // Control flags.
    uint32_t ContextFlags;
    uint32_t MxCsr;

    // Segment Registers and processor flags.
    uint16_t SegCs;
    uint16_t SegDs;
    uint16_t SegEs;
    uint16_t SegFs;
    uint16_t SegGs;
    uint16_t SegSs;
    uint32_t EFlags;

    // Debug registers
    uint64_t Dr0;
    uint64_t Dr1;
    uint64_t Dr2;
    uint64_t Dr3;
    uint64_t Dr6;
    uint64_t Dr7;

    // Integer registers.
    uint64_t Rax;
    uint64_t Rcx;
    uint64_t Rdx;
    uint64_t Rbx;
    uint64_t Rsp;
    uint64_t Rbp;
    uint64_t Rsi;
    uint64_t Rdi;
    uint64_t R8;
    uint64_t R9;
    uint64_t R10;
    uint64_t R11;
    uint64_t R12;
    uint64_t R13;
    uint64_t R14;
    uint64_t R15;

    // Program counter.
    uint64_t Rip;

    // Floating point state.
    union {
        XMM_SAVE_AREA32 FltSave;
        struct {
            M128A Header[2];
            M128A Legacy[8];
            M128A Xmm0;
            M128A Xmm1;
            M128A Xmm2;
            M128A Xmm3;
            M128A Xmm4;
            M128A Xmm5;
            M128A Xmm6;
            M128A Xmm7;
            M128A Xmm8;
            M128A Xmm9;
            M128A Xmm10;
            M128A Xmm11;
            M128A Xmm12;
            M128A Xmm13;
            M128A Xmm14;
            M128A Xmm15;
        };
    };

    // Vector registers.
    M128A VectorRegister[26];
    uint64_t VectorControl;

    // Special debug control registers.
    uint64_t DebugControl;
    uint64_t LastBranchToRip;
    uint64_t LastBranchFromRip;
    uint64_t LastExceptionToRip;
    uint64_t LastExceptionFromRip;

    CONTEXT() noexcept {
        std::memset(this, 0, sizeof(CONTEXT));
    }
};
#pragma pack(pop)

using PCONTEXT = CONTEXT*;
using LPCONTEXT = CONTEXT*;

static_assert(sizeof(CONTEXT) == 1232, "sizeof(CONTEXT) on x64 Windows ABI must be exactly 1232 bytes");

} // namespace micant
