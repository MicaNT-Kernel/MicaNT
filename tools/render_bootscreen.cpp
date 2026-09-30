#include <iostream>
#include <fstream>
#include "micant/bootvid.hpp"

int main() {
    std::cout << "[MicaNT Bootvid] Rendering high-resolution boot splash...\n";

    // 1024x768 Framebuffer
    constexpr uint32_t WIDTH = 1024;
    constexpr uint32_t HEIGHT = 768;

    micant::bootvid::BootVideoDriver driver;
    if (!driver.initializeVirtual(WIDTH, HEIGHT)) {
        std::cerr << "Failed to initialize virtual framebuffer\n";
        return 1;
    }

    // Render the boot splash
    driver.renderBootSplash("Starting Executive Services...", 0.65f);

    // Extract pixels into BmpImage
    micant::bootvid::BmpImage img;
    img.width = WIDTH;
    img.height = HEIGHT;
    img.bpp = 32;
    img.pixels.resize(WIDTH * HEIGHT);

    for (uint32_t y = 0; y < HEIGHT; ++y) {
        for (uint32_t x = 0; x < WIDTH; ++x) {
            img.setPixel(x, y, driver.getPixel(x, y));
        }
    }

    // Encode to BMP
    auto bmpBytes = micant::bootvid::BmpCodec::encode(img);
    if (bmpBytes.empty()) {
        std::cerr << "Failed to encode BMP\n";
        return 1;
    }

    std::ofstream outFile("micant_bootscreen.bmp", std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(bmpBytes.data()), bmpBytes.size());
    outFile.close();

    std::cout << "[MicaNT Bootvid] Successfully exported micant_bootscreen.bmp (" 
              << bmpBytes.size() << " bytes, " << WIDTH << "x" << HEIGHT << ")\n";

    return 0;
}
