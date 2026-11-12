#pragma once

// Optional user-mode registrations for hosted driver tooling.
#include "micant/km/hal/pci.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::pci {

inline void InitializePciHostedAdapter() {
    static bool s_registered = false;
    if (s_registered) return;
    s_registered = true;

    // 1. Initialize Subsystem Singleton & Hardware Device Tree
    TitanPciSubsystem::Instance().initialize();

    // 2. Dynamic Loader Exports for pci.sys
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("pci.sys", "PciReadConfigByte", reinterpret_cast<void*>(PciReadConfigByte));
    ldr.registerExport("pci.sys", "PciReadConfigWord", reinterpret_cast<void*>(PciReadConfigWord));
    ldr.registerExport("pci.sys", "PciReadConfigDword", reinterpret_cast<void*>(PciReadConfigDword));
    ldr.registerExport("pci.sys", "PciWriteConfigByte", reinterpret_cast<void*>(PciWriteConfigByte));
    ldr.registerExport("pci.sys", "PciWriteConfigWord", reinterpret_cast<void*>(PciWriteConfigWord));
    ldr.registerExport("pci.sys", "PciWriteConfigDword", reinterpret_cast<void*>(PciWriteConfigDword));
    ldr.registerExport("pci.sys", "PciFindDevice", reinterpret_cast<void*>(PciFindDevice));
    ldr.registerExport("pci.sys", "PciGetBarInfo", reinterpret_cast<void*>(PciGetBarInfo));
    ldr.registerExport("pci.sys", "PciEnableBusMastering", reinterpret_cast<void*>(PciEnableBusMastering));
    ldr.registerExport("pci.sys", "PciEnableMemorySpace", reinterpret_cast<void*>(PciEnableMemorySpace));
    ldr.registerExport("pci.sys", "PciConfigureMsi", reinterpret_cast<void*>(PciConfigureMsi));
    ldr.registerExport("pci.sys", "PciConfigureMsix", reinterpret_cast<void*>(PciConfigureMsix));
    ldr.registerExport("pci.sys", "PciTriggerMsiVector", reinterpret_cast<void*>(PciTriggerMsiVector));
    ldr.registerExport("pci.sys", "PciInjectAerError", reinterpret_cast<void*>(PciInjectAerError));
    ldr.registerExport("pci.sys", "PciClearAerStatus", reinterpret_cast<void*>(PciClearAerStatus));

    // 3. Register PCI Bus Driver in SCM
    auto pciSvc = std::make_shared<scm::ServiceRecord>();
    pciSvc->serviceName = L"pci";
    pciSvc->displayName = L"MicaNT PCI Express Bus Driver";
    pciSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    pciSvc->startType = scm::SERVICE_BOOT_START;
    pciSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    pciSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\pci.sys";
    pciSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(pciSvc);

    // 4. Register Version Database Information
    version::VersionDatabase::Instance().RegisterModule(
        "pci.sys",
        "10.0.22621.1",
        "MicaNT Sovereign PCI Express Bus Driver"
    );
}

} // namespace micant::pci
