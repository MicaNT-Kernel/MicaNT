#pragma once

#include "micant/shared/ntstatus.hpp"
#include "micant/shared/win32k_client.h"

namespace micant::win32k::freestanding {

micant::NtStatus dispatchRequest(MicaWin32kRequest& request) noexcept;

} // namespace micant::win32k::freestanding
