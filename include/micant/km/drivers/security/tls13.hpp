#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "micant/km/drivers/security/cng.hpp"
#include "micant/km/drivers/security/x509.hpp"
#include "micant/shared/tls13_client.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/um/native/ldr.hpp"

namespace micant::km::crypto::tls13 {
using namespace ::micant::shared::tls13;

// Kernel-side Schannel export registration remains an adapter; the actual TLS
// protocol and cryptographic implementation is mode-neutral.
inline void InitializeSchannelSubsystem() {
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("schannel.dll", "10.0.26100.5900", "MicaNT TLS / SSL Security Support Provider");
    verDb.RegisterModule("secur32.dll", "10.0.26100.5900", "MicaNT Security Support Provider Interface");
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("schannel.dll", "SpInitialize", reinterpret_cast<void*>(+[]() { return STATUS_SUCCESS; }));
}

} // namespace micant::km::crypto::tls13
