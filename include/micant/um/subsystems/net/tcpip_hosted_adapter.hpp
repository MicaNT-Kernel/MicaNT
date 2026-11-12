#pragma once

// Optional user-mode service and export registration for hosted TCP/IP tools.
#include "micant/km/drivers/net/tcpip.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::tcpip {

inline void InitializeTcpipHostedAdapter() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    NetworkStack::get().initialize();

    // 1. Register SCM Driver Record
    auto& scm = scm::ServiceControlManager::get();
    auto tcpipSvc = std::make_shared<scm::ServiceRecord>();
    tcpipSvc->serviceName = L"tcpip";
    tcpipSvc->displayName = L"TCP/IP Protocol Driver";
    tcpipSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    tcpipSvc->startType = scm::SERVICE_BOOT_START;
    tcpipSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    tcpipSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\tcpip.sys";
    tcpipSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(tcpipSvc);

    // 2. Register Version Database Record
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("tcpip.sys", "10.0.26100.5900", "MicaNT Clean-Room TCP/IP Dual-Stack Protocol Driver");

    // 3. Register JanusLDR Dynamic Exports
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("tcpip.sys", "TcpipInitialize", reinterpret_cast<void*>(InitializeTcpipSubsystem));
}

} // namespace micant::tcpip
