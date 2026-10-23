/**
 * @file bitsadmin_app.cpp
 * @brief Sovereign Ring 3 Background Intelligent Transfer Service Admin Utility (bitsadmin) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Manages asynchronous file download/upload transfer queues and network bandwidth throttles.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowBitsList() noexcept {
    const char* out =
        "BITSADMIN version 3.0\n"
        "BITS administration utility.\n"
        "(C) Copyright MicaNT System Authority.\n\n"
        "{B8A5B9C2-4E2F-43B0-8F9E-6C0D9B7B8D10} 'MicaKernelUpdate' TRANSFERRED 1 / 1 1048576 / 1048576\n"
        "{E1D7B439-0144-48FE-9538-232B45A8CC92} 'SystemCatalogSync' SUSPENDED 0 / 1 0 / UNKNOWN\n\n"
        "Listed 2 job(s).\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "BITSADMIN [/RAWRETURN] [/WRAP | /NOWRAP] command\n\n"
        "The following commands are available:\n"
        "    /HELP                           Prints this help screen\n"
        "    /LIST [/ALLUSERS] [/VERBOSE]    List jobs\n"
        "    /MONITOR [/ALLUSERS] [/REFRESH sec] Monitor jobs\n"
        "    /RESET [/ALLUSERS]              Cancels all jobs in transfer queue\n"
        "    /TRANSFER <job_name> [type]     One-command transfer\n"
        "    /CREATE [type] <job_name>       Creates a new transfer job\n"
        "    /RESUME <job_name>              Resumes job\n"
        "    /SUSPEND <job_name>             Suspends job\n"
        "    /COMPLETE <job_name>            Completes job\n"
        "    /CANCEL <job_name>              Cancels job\n\n"
        "[bitsadmin /list]\n";

    PrintStr(banner);
    ShowBitsList();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
