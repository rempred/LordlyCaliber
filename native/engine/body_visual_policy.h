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
    static constexpr quint32 queryAddress = 0x801c425c;
    static const std::array<Block, 5> &blocks();
    static const std::array<Block, 4> &constructionBlocks();
    static QByteArray constructionCode(const Rom &);
    static QByteArray code(const Rom &, bool construction = false);
    static int count(const Rom &);
    static void validate(const Rom &, const AnimationRoutes &);
};
} // namespace lordly
