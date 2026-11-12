#pragma once

// Optional user-mode registrations for hosted driver tooling.
#include "micant/km/drivers/net/ndis.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::ndis {

inline void InitializeNdisHostedAdapter() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    TitanNdisSubsystem::Instance().initialize();

    // 2. Register Driver Exports in JanusLDR Dynamic Loader
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("ndis.sys", "NdisMRegisterMiniportDriver", reinterpret_cast<void*>(NdisMRegisterMiniportDriver));
    ldr.registerExport("ndis.sys", "NdisMDeregisterMiniportDriver", reinterpret_cast<void*>(NdisMDeregisterMiniportDriver));
    ldr.registerExport("ndis.sys", "NdisMSetMiniportAttributes", reinterpret_cast<void*>(NdisMSetMiniportAttributes));
    ldr.registerExport("ndis.sys", "NdisAllocateNetBufferListPool", reinterpret_cast<void*>(NdisAllocateNetBufferListPool));
    ldr.registerExport("ndis.sys", "NdisFreeNetBufferListPool", reinterpret_cast<void*>(NdisFreeNetBufferListPool));
    ldr.registerExport("ndis.sys", "NdisAllocateNetBufferList", reinterpret_cast<void*>(NdisAllocateNetBufferList));
    ldr.registerExport("ndis.sys", "NdisFreeNetBufferList", reinterpret_cast<void*>(NdisFreeNetBufferList));
    ldr.registerExport("ndis.sys", "NdisMIndicateReceiveNetBufferLists", reinterpret_cast<void*>(NdisMIndicateReceiveNetBufferLists));
    ldr.registerExport("ndis.sys", "NdisMSendNetBufferListsComplete", reinterpret_cast<void*>(NdisMSendNetBufferListsComplete));
    ldr.registerExport("ndis.sys", "NdisQueryAdapterInformation", reinterpret_cast<void*>(NdisQueryAdapterInformation));
    ldr.registerExport("ndis.sys", "NdisMResetMiniport", reinterpret_cast<void*>(NdisMResetMiniport));

    ldr.registerExport("razzlenet.sys", "RazzleNetInitializeMiniport", reinterpret_cast<void*>(RazzleNetInitializeMiniport));
    ldr.registerExport("razzlenet.sys", "RazzleNetTransmitPacket", reinterpret_cast<void*>(RazzleNetTransmitPacket));

    ldr.registerExport("netadaptercx.sys", "NetAdapterCreate", reinterpret_cast<void*>(NetAdapterCreate));
    ldr.registerExport("netadaptercx.sys", "NetAdapterStart", reinterpret_cast<void*>(NetAdapterStart));
    ldr.registerExport("netadaptercx.sys", "NetAdapterStop", reinterpret_cast<void*>(NetAdapterStop));
    ldr.registerExport("netadaptercx.sys", "NetTxQueueCreate", reinterpret_cast<void*>(NetTxQueueCreate));
    ldr.registerExport("netadaptercx.sys", "NetRxQueueCreate", reinterpret_cast<void*>(NetRxQueueCreate));
    ldr.registerExport("netadaptercx.sys", "NetTxQueueNotifyMoreCompletedPacketsAvailable", reinterpret_cast<void*>(NetTxQueueNotifyMoreCompletedPacketsAvailable));
    ldr.registerExport("netadaptercx.sys", "NetRxQueueNotifyMoreReceivedPacketsAvailable", reinterpret_cast<void*>(NetRxQueueNotifyMoreReceivedPacketsAvailable));

    ldr.registerExport("e1000e.sys", "E1000eInitializeMiniport", reinterpret_cast<void*>(E1000eInitializeMiniport));
    ldr.registerExport("e1000e.sys", "E1000eSendNetBufferLists", reinterpret_cast<void*>(E1000eSendNetBufferLists));
    ldr.registerExport("e1000e.sys", "E1000eReturnNetBufferLists", reinterpret_cast<void*>(E1000eReturnNetBufferLists));
    ldr.registerExport("e1000e.sys", "E1000eOidRequest", reinterpret_cast<void*>(E1000eOidRequest));

    ldr.registerExport("netio.sys", "NetioRegisterClient", reinterpret_cast<void*>(NetioRegisterClient));
    ldr.registerExport("netio.sys", "NetioDeregisterClient", reinterpret_cast<void*>(NetioDeregisterClient));

    // 3. Register Core Network Drivers in SCM
    auto ndisSvc = std::make_shared<scm::ServiceRecord>();
    ndisSvc->serviceName = L"ndis";
    ndisSvc->displayName = L"Network Driver Interface Specification (NDIS 6.88)";
    ndisSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    ndisSvc->startType = scm::SERVICE_BOOT_START;
    ndisSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    ndisSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\ndis.sys";
    ndisSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(ndisSvc);

    auto razzleSvc = std::make_shared<scm::ServiceRecord>();
    razzleSvc->serviceName = L"razzlenet";
    razzleSvc->displayName = L"RazzleNet 10GbE/100GbE PCIe Miniport Driver";
    razzleSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    razzleSvc->startType = scm::SERVICE_SYSTEM_START;
    razzleSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    razzleSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\razzlenet.sys";
    razzleSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(razzleSvc);

    auto netadapterSvc = std::make_shared<scm::ServiceRecord>();
    netadapterSvc->serviceName = L"netadaptercx";
    netadapterSvc->displayName = L"Network Adapter WDF Class Extension (NetAdapterCx)";
    netadapterSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    netadapterSvc->startType = scm::SERVICE_BOOT_START;
    netadapterSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    netadapterSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\netadaptercx.sys";
    netadapterSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(netadapterSvc);

    auto e1000Svc = std::make_shared<scm::ServiceRecord>();
    e1000Svc->serviceName = L"e1000e";
    e1000Svc->displayName = L"Intel PRO/1000 Gigabit PCIe Network Driver";
    e1000Svc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    e1000Svc->startType = scm::SERVICE_BOOT_START;
    e1000Svc->errorControl = scm::SERVICE_ERROR_NORMAL;
    e1000Svc->binaryPath = L"C:\\Windows\\System32\\drivers\\e1000e.sys";
    e1000Svc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(e1000Svc);

    auto netioSvc = std::make_shared<scm::ServiceRecord>();
    netioSvc->serviceName = L"netio";
    netioSvc->displayName = L"Network I/O Subsystem Driver";
    netioSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    netioSvc->startType = scm::SERVICE_BOOT_START;
    netioSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    netioSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\netio.sys";
    netioSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(netioSvc);

    // 4. Register Version Database Information
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("ndis.sys", "10.0.22621.1", "MicaNT NDIS 6.88 Subsystem Driver");
    verDb.RegisterModule("razzlenet.sys", "10.0.22621.1", "MicaNT RazzleNet 10GbE/100GbE PCIe Miniport Driver");
    verDb.RegisterModule("netadaptercx.sys", "10.0.22621.1", "MicaNT NetAdapter WDF Class Extension");
    verDb.RegisterModule("e1000e.sys", "10.0.22621.1", "MicaNT Intel Gigabit PCIe Miniport Driver");
    verDb.RegisterModule("netio.sys", "10.0.22621.1", "MicaNT Network I/O Subsystem Driver");
}

} // namespace micant::ndis
