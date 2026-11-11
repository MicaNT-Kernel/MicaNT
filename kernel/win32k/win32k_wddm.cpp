#include "micant/km/drivers/video/wddm.hpp"
#include "micant/km/win32k/win32k.hpp"

namespace micant::win32k {

bool TitanWin32kSubsystem::blitToWddmPrimary(
    HDC hdcSrc, int32_t nXSrc, int32_t nYSrc,
    int32_t nWidth, int32_t nHeight) {
    auto& wddmSubsystem = micant::wddm::TitanWddmSubsystem::Instance();
    if (!wddmSubsystem.getPrimaryAdapter()) return false;

    const HDC screenDc = getGdi().getScreenDC();
    if (!screenDc) return false;
    getGdi().bitBlt(screenDc, 0, 0, nWidth, nHeight,
                    hdcSrc, nXSrc, nYSrc, SRCCOPY);
    return true;
}

} // namespace micant::win32k
