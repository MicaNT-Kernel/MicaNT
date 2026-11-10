#pragma once

// ACPI namespace bus enumerator. It creates a PDO for each present namespace
// device below \_SB that exposes _HID, then binds the control-method battery
// function driver to PNP0C0A devices.
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "micant/km/drivers/acpi/cmbatt.hpp"
#include "micant/km/executive/pnp.hpp"
#include "micant/km/hal/acpi.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::km::bus::acpi {

struct AcpiPdoExtension {
    std::wstring hardwareId;
    std::wstring instanceId;
    std::string namespacePath;
};

class AcpiBusEnumerator {
public:
    static AcpiBusEnumerator& Instance() {
        static AcpiBusEnumerator enumerator;
        return enumerator;
    }

    NtStatus initialize() {
        auto& subsystem = micant::acpi::TitanAcpiSubsystem::Instance();
        if (!subsystem.isInitialized()) subsystem.initialize();
        const auto& ns = subsystem.getNamespace();
        if (!ns.resolvePath("\\_SB")) return NtStatus::NoSuchDevice;

        configureDriver();
        auto& ioManager = io::IoManager::get();
        if (!busPdo_) {
            busPdo_ = ioManager.createDevice(&driver_, L"\\Device\\AcpiBus0",
                io::DeviceType::Unknown, this);
            busPdo_->flags &= ~io::DO_DEVICE_INITIALIZING;
        }

        enumerateNamespace(ns);
        if (children_.empty()) return NtStatus::NoSuchDevice;

        auto& pnp = pnp::PnpManager::get();
        pnp.initialize();
        auto root = pnp.getRootNode();
        if (!busDevNode_) {
            busDevNode_ = pnp.createDeviceNode(root.get(), busPdo_.get(), L"ACPI\\PNP0A03", L"0");
            busDevNode_->setDeviceDescription(L"ACPI x64-based PC");
            busDevNode_->setHardwareIds({L"ACPI\\PNP0A03"});
        }
        if (busDevNode_->getState() == pnp::PnpDeviceState::Initialized) {
            NtStatus status = pnp.addDevice(busDevNode_.get(), &driver_);
            if (!NT_SUCCESS(status)) return status;
            status = pnp.startDevice(busDevNode_.get());
            if (!NT_SUCCESS(status)) return status;
        }

        std::vector<std::shared_ptr<pnp::PnpDeviceNode>> enumerated;
        NtStatus status = pnp.enumerateBus(busDevNode_.get(), enumerated);
        if (!NT_SUCCESS(status)) return status;

        auto* battery = findBatteryPdo();
        if (!battery) return NtStatus::NoSuchDevice;
        for (const auto& child : enumerated) {
            if (child->getPdo() != battery) continue;
            batteryDevNode_ = child;
            break;
        }
        if (!batteryDevNode_) return NtStatus::NoSuchDevice;
        batteryDevNode_->setDeviceDescription(L"ACPI Control Method Battery");
        batteryDevNode_->setHardwareIds({L"ACPI\\PNP0C0A"});
        batteryDevNode_->setCompatibleIds({L"PNP0C0A"});
        if (batteryDevNode_->getState() == pnp::PnpDeviceState::Started) {
            (void)micant::km::acpi::cmbatt::ControlMethodBatteryMiniport::RefreshClassDevice();
            return NtStatus::Success;
        }

        micant::km::acpi::cmbatt::InitializeCmbattDriver();
        status = pnp.addDevice(batteryDevNode_.get(),
            &micant::km::acpi::cmbatt::ControlMethodBatteryMiniport::DriverObject());
        if (!NT_SUCCESS(status)) return status;
        return pnp.startDevice(batteryDevNode_.get());
    }

    static io::DriverObject& DriverObject() { return Instance().driver_; }
    io::DeviceObject* batteryPdo() const noexcept { return findBatteryPdo(); }

private:
    static NtStatus dispatchPnp(io::DeviceObject* device, io::Irp* irp) noexcept {
        if (!device || !irp || !device->deviceExtension) return NtStatus::InvalidParameter;
        auto* self = static_cast<AcpiBusEnumerator*>(device->deviceExtension);
        auto* stack = irp->getCurrentStack();
        if (!stack) return NtStatus::InvalidParameter;
        if (irp->minorFunction == pnp::IRP_MN_QUERY_DEVICE_RELATIONS &&
            stack->parameters.pnp.type == static_cast<uint32_t>(pnp::DEVICE_RELATION_TYPE::BusRelations)) {
            auto* relations = static_cast<pnp::DeviceRelationsList*>(stack->parameters.pnp.argument);
            if (!relations) relations = static_cast<pnp::DeviceRelationsList*>(irp->userBuffer);
            if (!relations) return complete(irp, NtStatus::InvalidParameter);
            relations->objects.clear();
            const auto& ns = micant::acpi::TitanAcpiSubsystem::Instance().getNamespace();
            for (const auto& child : self->children_) {
                const auto sta = ns.evaluate(child.extension->namespacePath + "._STA");
                const bool present = sta.type != micant::acpi::AmlObjectType::Integer ||
                    (sta.integerVal & 1U) != 0;
                if (present) relations->objects.push_back(child.pdo.get());
            }
            return complete(irp, NtStatus::Success);
        }
        if (irp->minorFunction == pnp::IRP_MN_QUERY_ID) {
            auto* extension = static_cast<AcpiPdoExtension*>(device->deviceExtension);
            if (device == self->busPdo_.get()) {
                return queryId(irp, L"ACPI\\PNP0A03", L"0", false);
            }
            switch (static_cast<pnp::BUS_QUERY_ID_TYPE>(stack->parameters.pnp.type)) {
            case pnp::BUS_QUERY_ID_TYPE::BusQueryDeviceID:
            case pnp::BUS_QUERY_ID_TYPE::BusQueryHardwareIDs:
                return queryId(irp, extension->hardwareId, extension->instanceId, true);
            case pnp::BUS_QUERY_ID_TYPE::BusQueryCompatibleIDs: {
                std::wstring compatible = extension->hardwareId;
                if (compatible.starts_with(L"ACPI\\")) compatible.erase(0, 5);
                return queryId(irp, compatible, extension->instanceId, true);
            }
            case pnp::BUS_QUERY_ID_TYPE::BusQueryInstanceID:
                return queryId(irp, {}, extension->instanceId, true);
            default:
                return complete(irp, NtStatus::InvalidParameter);
            }
        }
        if (irp->minorFunction == pnp::IRP_MN_START_DEVICE ||
            irp->minorFunction == pnp::IRP_MN_STOP_DEVICE ||
            irp->minorFunction == pnp::IRP_MN_REMOVE_DEVICE)
            return complete(irp, NtStatus::Success);
        return complete(irp, NtStatus::InvalidDeviceRequest);
    }

    static NtStatus queryId(io::Irp* irp, std::wstring_view id,
                            std::wstring_view instance, bool child) noexcept {
        auto* stack = irp->getCurrentStack();
        auto* out = stack ? static_cast<std::wstring*>(stack->parameters.pnp.argument) : nullptr;
        if (!out) out = static_cast<std::wstring*>(irp->userBuffer);
        if (!out) return complete(irp, NtStatus::InvalidParameter);
        switch (static_cast<pnp::BUS_QUERY_ID_TYPE>(stack->parameters.pnp.type)) {
        case pnp::BUS_QUERY_ID_TYPE::BusQueryDeviceID:
        case pnp::BUS_QUERY_ID_TYPE::BusQueryHardwareIDs:
            *out = id;
            break;
        case pnp::BUS_QUERY_ID_TYPE::BusQueryCompatibleIDs:
            *out = std::wstring(id);
            if (child && out->starts_with(L"ACPI\\")) out->erase(0, 5);
            break;
        case pnp::BUS_QUERY_ID_TYPE::BusQueryInstanceID:
            *out = instance;
            break;
        default:
            return complete(irp, NtStatus::InvalidParameter);
        }
        return complete(irp, NtStatus::Success);
    }

    static NtStatus complete(io::Irp* irp, NtStatus status) noexcept {
        if (irp) { irp->ioStatus.status = status; irp->ioStatus.information = 0; }
        return status;
    }

    void enumerateNamespace(const micant::acpi::AcpiNamespace& ns) {
        if (!children_.empty()) return;
        auto sb = ns.resolvePath("\\_SB");
        if (!sb) return;
        uint32_t index = 0;
        std::unordered_map<std::wstring, uint32_t> instances;
        enumerateNamespaceNode(ns, sb, "\\_SB", instances, index, 0);
    }

    void enumerateNamespaceNode(const micant::acpi::AcpiNamespace& ns,
        const std::shared_ptr<micant::acpi::AmlNode>& parent, const std::string& parentPath,
        std::unordered_map<std::wstring, uint32_t>& instances, uint32_t& index, uint32_t depth) {
        if (!parent || depth > 64) return;
        for (const auto& [name, node] : parent->children) {
            if (!node || name == "_HID") continue;
            const std::string path = parentPath + "." + name;
            if (node->value.type == micant::acpi::AmlObjectType::Device) {
                const auto hidIt = node->children.find("_HID");
                if (hidIt != node->children.end() && hidIt->second) {
                    std::string hid;
                    const auto& hidValue = hidIt->second->value;
                    if (hidValue.type == micant::acpi::AmlObjectType::String)
                        hid = hidValue.stringVal;
                    else if (hidValue.type == micant::acpi::AmlObjectType::Integer)
                        hid = eisaIdToString(static_cast<uint32_t>(hidValue.integerVal));
                    if (!hid.empty()) {
                        const auto sta = ns.evaluate(path + "._STA");
                        if (sta.type != micant::acpi::AmlObjectType::Integer || (sta.integerVal & 1U)) {
                            std::wstring wideHid(hid.begin(), hid.end());
                            const uint32_t instance = instances[wideHid]++;
                            auto extension = std::make_unique<AcpiPdoExtension>();
                            extension->hardwareId = L"ACPI\\" + wideHid;
                            extension->instanceId = std::to_wstring(instance);
                            extension->namespacePath = path;
                            const std::wstring pdoName = L"\\Device\\AcpiPdo" + std::to_wstring(index++);
                            auto pdo = io::IoManager::get().createDevice(&driver_, pdoName,
                                wideHid == L"PNP0C0A" ? io::DeviceType::Battery : io::DeviceType::Unknown,
                                extension.get());
                            pdo->flags &= ~io::DO_DEVICE_INITIALIZING;
                            children_.push_back({std::move(extension), std::move(pdo)});
                        }
                    }
                }
            }
            enumerateNamespaceNode(ns, node, path, instances, index, depth + 1);
        }
    }

    static std::string eisaIdToString(uint32_t id) {
        const uint16_t encoded = static_cast<uint16_t>(id >> 16);
        const char a = static_cast<char>(((encoded >> 10) & 0x1f) + '@');
        const char b = static_cast<char>(((encoded >> 5) & 0x1f) + '@');
        const char c = static_cast<char>((encoded & 0x1f) + '@');
        constexpr char hex[] = "0123456789ABCDEF";
        std::string out{a, b, c, hex[(id >> 12) & 0xF], hex[(id >> 8) & 0xF],
            hex[(id >> 4) & 0xF], hex[id & 0xF]};
        return out;
    }

    io::DeviceObject* findBatteryPdo() const noexcept {
        for (const auto& child : children_)
            if (child.extension->hardwareId == L"ACPI\\PNP0C0A") return child.pdo.get();
        return nullptr;
    }

    struct ChildDevice {
        std::unique_ptr<AcpiPdoExtension> extension;
        std::shared_ptr<io::DeviceObject> pdo;
    };

    void configureDriver() {
        driver_.driverName = L"\\Driver\\Acpi";
        driver_.setDispatch(io::IRP_MJ_PNP, dispatchPnp);
        auto service = std::make_shared<scm::ServiceRecord>();
        service->serviceName = L"acpi";
        service->displayName = L"Microsoft ACPI Driver";
        service->serviceType = scm::SERVICE_KERNEL_DRIVER;
        service->startType = scm::SERVICE_BOOT_START;
        service->errorControl = scm::SERVICE_ERROR_CRITICAL;
        service->binaryPath = L"\\SystemRoot\\System32\\drivers\\acpi.sys";
        service->status.dwCurrentState = scm::SERVICE_RUNNING;
        scm::ServiceControlManager::get().registerServiceRecord(service);
        version::VersionDatabase::Instance().RegisterModule("acpi.sys", "10.0.26100.1", "Microsoft ACPI Driver");
    }

    io::DriverObject driver_{};
    std::shared_ptr<io::DeviceObject> busPdo_;
    std::vector<ChildDevice> children_;
    std::shared_ptr<pnp::PnpDeviceNode> busDevNode_;
    std::shared_ptr<pnp::PnpDeviceNode> batteryDevNode_;
};

inline NtStatus InitializeAcpiBusDriver() {
    return AcpiBusEnumerator::Instance().initialize();
}

} // namespace micant::km::bus::acpi
