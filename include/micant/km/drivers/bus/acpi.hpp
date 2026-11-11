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
    std::wstring compatibleId;
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
        auto& ns = subsystem.getNamespace();
        if (!ns.resolvePath("\\_SB")) return NtStatus::NoSuchDevice;

        configureDriver();
        auto& ioManager = io::IoManager::get();
        if (!busPdo_) {
            busPdo_ = ioManager.createDevice(&driver_, L"\\Device\\AcpiBus0",
                io::DeviceType::Unknown, this);
            busPdo_->flags &= ~io::DO_DEVICE_INITIALIZING;
        }

        enumerateNamespace(ns);
        registerNotifyHandlers(ns);
        registerGpeMethods(ns);

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
        if (!battery) return NtStatus::Success;
        for (const auto& child : enumerated) {
            if (child->getPdo() != battery) continue;
            batteryDevNode_ = child;
            break;
        }
        if (!batteryDevNode_) return NtStatus::Success;
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
                const std::string staPath = child.extension->namespacePath + "._STA";
                const auto staNode = ns.resolvePath(staPath);
                const auto sta = staNode ? ns.evaluate(staPath) : micant::acpi::AmlValue::MakeInteger(0x0F);
                const bool present = !staNode || sta.type != micant::acpi::AmlObjectType::Integer ||
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
                std::wstring compatible = extension->compatibleId;
                if (compatible.empty()) {
                    compatible = extension->hardwareId;
                    if (compatible.starts_with(L"ACPI\\")) compatible.erase(0, 5);
                }
                return queryId(irp, compatible, extension->instanceId, true);
            }
            case pnp::BUS_QUERY_ID_TYPE::BusQueryInstanceID:
                return queryId(irp, {}, extension->instanceId, true);
            default:
                return complete(irp, NtStatus::InvalidParameter);
            }
        }
        if (irp->minorFunction == pnp::IRP_MN_QUERY_CAPABILITIES) {
            auto* capabilities = static_cast<pnp::DEVICE_CAPABILITIES*>(stack->parameters.pnp.argument);
            if (!capabilities) capabilities = static_cast<pnp::DEVICE_CAPABILITIES*>(irp->userBuffer);
            if (!capabilities || capabilities->size < sizeof(pnp::DEVICE_CAPABILITIES))
                return complete(irp, NtStatus::InvalidParameter);
            capabilities->rawDeviceOK = true;
            capabilities->uniqueId = true;
            if (device != self->busPdo_.get()) {
                const auto* extension = static_cast<AcpiPdoExtension*>(device->deviceExtension);
                const auto& ns = micant::acpi::TitanAcpiSubsystem::Instance().getNamespace();
                const std::string staPath = extension->namespacePath + "._STA";
                const auto staNode = ns.resolvePath(staPath);
                const auto sta = staNode ? ns.evaluate(staPath) : micant::acpi::AmlValue::MakeInteger(0x0F);
                capabilities->hardwareDisabled = staNode && sta.type == micant::acpi::AmlObjectType::Integer &&
                    !(sta.integerVal & 1U);
            }
            return complete(irp, NtStatus::Success);
        }
        if (irp->minorFunction == pnp::IRP_MN_START_DEVICE ||
            irp->minorFunction == pnp::IRP_MN_STOP_DEVICE ||
            irp->minorFunction == pnp::IRP_MN_REMOVE_DEVICE ||
            irp->minorFunction == pnp::IRP_MN_QUERY_STOP_DEVICE ||
            irp->minorFunction == pnp::IRP_MN_CANCEL_STOP_DEVICE ||
            irp->minorFunction == pnp::IRP_MN_QUERY_REMOVE_DEVICE ||
            irp->minorFunction == pnp::IRP_MN_CANCEL_REMOVE_DEVICE ||
            irp->minorFunction == pnp::IRP_MN_SURPRISE_REMOVAL ||
            irp->minorFunction == pnp::IRP_MN_DEVICE_USAGE_NOTIFICATION)
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

    void enumerateNamespace(micant::acpi::AcpiNamespace& ns) {
        if (!children_.empty()) return;
        auto sb = ns.resolvePath("\\_SB");
        if (!sb) return;
        uint32_t index = 0;
        std::unordered_map<std::wstring, uint32_t> instances;
        enumerateNamespaceNode(ns, sb, "\\_SB", instances, index, 0);
    }

    void enumerateNamespaceNode(micant::acpi::AcpiNamespace& ns,
        const std::shared_ptr<micant::acpi::AmlNode>& parent, const std::string& parentPath,
        std::unordered_map<std::wstring, uint32_t>& instances, uint32_t& index, uint32_t depth) {
        if (!parent || depth > 64) return;
        for (const auto& [name, node] : parent->children) {
            if (!node || name == "_HID") continue;
            const std::string path = parentPath + "." + name;
            if (node->value.type == micant::acpi::AmlObjectType::Device) {
                const auto hidIt = node->children.find("_HID");
                if (hidIt != node->children.end() && hidIt->second) {
                        std::string hid = decodeId(hidIt->second->value);
                    if (!hid.empty()) {
                        std::wstring wideHid(hid.begin(), hid.end());
                        const uint32_t instance = instances[wideHid]++;
                        auto extension = std::make_unique<AcpiPdoExtension>();
                        extension->hardwareId = L"ACPI\\" + wideHid;
                            extension->instanceId = decodeUid(ns.evaluate(path + "._UID"));
                            if (extension->instanceId.empty())
                                extension->instanceId = std::to_wstring(instance);
                            const auto cid = ns.evaluate(path + "._CID");
                            if (cid.type == micant::acpi::AmlObjectType::Package) {
                                for (const auto& item : cid.packageElements) {
                                    const auto decoded = decodeId(item);
                                    if (!decoded.empty()) {
                                        extension->compatibleId = L"ACPI\\" +
                                            std::wstring(decoded.begin(), decoded.end());
                                        break;
                                    }
                                }
                            } else {
                                const auto decoded = decodeId(cid);
                                if (!decoded.empty()) extension->compatibleId = L"ACPI\\" +
                                    std::wstring(decoded.begin(), decoded.end());
                            }
                        extension->namespacePath = path;
                        const std::wstring pdoName = L"\\Device\\AcpiPdo" + std::to_wstring(index++);
                        auto pdo = io::IoManager::get().createDevice(&driver_, pdoName,
                            wideHid == L"PNP0C0A" ? io::DeviceType::Battery : io::DeviceType::Unknown,
                            extension.get());
                        pdo->flags &= ~io::DO_DEVICE_INITIALIZING;
                        ns.bindDevicePath(pdo.get(), extension->namespacePath);
                        children_.push_back({std::move(extension), std::move(pdo)});
                    }
                }
            }
            enumerateNamespaceNode(ns, node, path, instances, index, depth + 1);
        }
    }

    void registerNotifyHandlers(micant::acpi::AcpiNamespace& ns) {
        for (const auto& child : children_) {
            if (notifyTokens_.contains(child.pdo.get())) continue;
            const auto pdo = child.pdo.get();
            const auto path = child.extension->namespacePath;
            const auto token = ns.registerNotifyHandler(path,
                [pdo](std::string_view, uint32_t value) {
                    if (value == 0x80 || value == 0x81)
                        (void)micant::km::acpi::cmbatt::ControlMethodBatteryMiniport::HandleFirmwareNotify(
                            pdo, value);
                });
            if (token) notifyTokens_[pdo] = token;
        }
    }

    void registerGpeMethods(micant::acpi::AcpiNamespace& ns) {
        if (gpeMethodsRegistered_) return;
        auto gpeScope = ns.resolvePath("\\_GPE");
        if (!gpeScope) return;
        auto& controller = micant::acpi::TitanAcpiSubsystem::Instance().getGpeController();
        for (const auto& [name, node] : gpeScope->children) {
            if (!node || node->type != micant::acpi::AmlObjectType::Method || name.size() != 4 ||
                (name[1] != 'L' && name[1] != 'E')) continue;
            const int high = hexDigit(name[2]);
            const int low = hexDigit(name[3]);
            if (high < 0 || low < 0) continue;
            const uint32_t gpe = static_cast<uint32_t>((high << 4) | low);
            if (gpe >= 64) continue;
            const std::string path = "\\_GPE." + name;
            controller.registerHandler(gpe, [&ns, path](uint32_t) {
                (void)ns.evaluate(path);
            }, name[1] == 'L');
        }
        gpeMethodsRegistered_ = true;
    }

    static int hexDigit(char value) noexcept {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return -1;
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

    static std::string decodeId(const micant::acpi::AmlValue& value) {
        if (value.type == micant::acpi::AmlObjectType::String) return value.stringVal;
        if (value.type == micant::acpi::AmlObjectType::Integer)
            return eisaIdToString(static_cast<uint32_t>(value.integerVal));
        return {};
    }

    static std::wstring decodeUid(const micant::acpi::AmlValue& value) {
        if (value.type == micant::acpi::AmlObjectType::String)
            return std::wstring(value.stringVal.begin(), value.stringVal.end());
        if (value.type == micant::acpi::AmlObjectType::Integer)
            return std::to_wstring(value.integerVal);
        return {};
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
    std::unordered_map<io::DeviceObject*, uint64_t> notifyTokens_;
    bool gpeMethodsRegistered_{};
    std::shared_ptr<pnp::PnpDeviceNode> busDevNode_;
    std::shared_ptr<pnp::PnpDeviceNode> batteryDevNode_;
};

inline NtStatus InitializeAcpiBusDriver() {
    return AcpiBusEnumerator::Instance().initialize();
}

} // namespace micant::km::bus::acpi
