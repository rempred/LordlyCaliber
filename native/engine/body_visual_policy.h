#pragma once
#include "engine/animation_routes.h"
namespace lordly {
class BodyVisualPolicyImage {
  public:
    struct Block {
        int offset, size;
        const char *source;
    };
    static constexpr int descriptorOffset = 0x27c5100, descriptorSize = 96;
    static const std::array<Block, 5> &blocks();
    static QByteArray code(const Rom &);
    static int count(const Rom &);
    static void validate(const Rom &, const AnimationRoutes &);
};
} // namespace lordly
