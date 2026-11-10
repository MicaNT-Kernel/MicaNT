#pragma once

#include "micant/km/drivers/input/hidclass.hpp"

namespace micant::km::hid {

// hidparse.sys is a kernel-mode parser library. It exports the documented
// HidP routines and does not create a device object of its own.
inline void InitializeHidParseExports() {
    auto& loader = ldr::DynamicLoader::get();
    loader.registerExport("hidparse.sys", "HidP_GetCaps", reinterpret_cast<void*>(HidP_GetCaps));
    loader.registerExport("hidparse.sys", "HidP_GetButtonCaps", reinterpret_cast<void*>(HidP_GetButtonCaps));
    loader.registerExport("hidparse.sys", "HidP_GetValueCaps", reinterpret_cast<void*>(HidP_GetValueCaps));
    loader.registerExport("hidparse.sys", "HidP_GetUsages", reinterpret_cast<void*>(HidP_GetUsages));
    loader.registerExport("hidparse.sys", "HidP_SetUsages", reinterpret_cast<void*>(HidP_SetUsages));
    loader.registerExport("hidparse.sys", "HidP_GetUsageValue", reinterpret_cast<void*>(HidP_GetUsageValue));
    loader.registerExport("hidparse.sys", "HidP_SetUsageValue", reinterpret_cast<void*>(HidP_SetUsageValue));
}

inline void InitializeHidParseDriver(io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\Hidparse";
}

} // namespace micant::km::hid
