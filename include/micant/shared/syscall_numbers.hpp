#pragma once

// MicaNT's native service profile; usable by both syscall entry and NTDLL.
#define MICANT_SSN_NT_RAISE_EXCEPTION 0x0160

// Internal MicaNT profile assignments, not Windows service indices.
#define MICANT_SSN_NT_OPEN_PROCESS 0x016A
#define MICANT_SSN_NT_OPEN_THREAD 0x016B

#define MICANT_SSN_NT_READ_VIRTUAL_MEMORY 0x016C
#define MICANT_SSN_NT_FLUSH_INSTRUCTION_CACHE 0x016D
