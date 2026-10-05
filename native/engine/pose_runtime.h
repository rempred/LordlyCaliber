#pragma once
#include "engine/rom.h"
namespace lordly {
class PoseRuntimeImage {
  public:
    static constexpr int poseOffset = 0x29e218, poseSize = 1268;
    static constexpr quint32 poseAddress = 0x8022e9e8;
    static constexpr int readerOffset = 0x204f34, readerSize = 376;
    static constexpr quint32 readerAddress = 0x801c1aa4;
    static constexpr int widthsOffset = 0x212370, widthsSize = 48;
    static constexpr int descriptorOffset = 0x27c5000, descriptorSize = 160;
    static int count(const Rom &);
    static void validate(const Rom &, const QByteArray &mappings, bool wideRequired = false);
};
} // namespace lordly
