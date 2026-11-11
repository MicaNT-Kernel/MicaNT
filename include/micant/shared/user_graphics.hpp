#pragma once

#include <cstdint>

namespace micant::um {

#pragma pack(push, 1)
struct UserVideoInfo64 {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t pitch{0};
    uint32_t bpp{32};
    uint32_t pixelFormat{1};
    uint32_t reserved{0};
    uint64_t physicalBase{0};
};

struct UserPresentSurfaceParams64 {
    uint64_t surfaceBufferVa{0};
    uint32_t dstX{0};
    uint32_t dstY{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t pitch{0};
    uint32_t reserved{0};
};

struct UserPresentCompositionParams64 {
    uint64_t baseSurfaceBufferVa{0};
    uint64_t layersVa{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t pitch{0};
    uint32_t layerCount{0};
};
#pragma pack(pop)

static_assert(sizeof(UserVideoInfo64) == 32);
static_assert(sizeof(UserPresentSurfaceParams64) == 32);
static_assert(sizeof(UserPresentCompositionParams64) == 32);

} // namespace micant::um
