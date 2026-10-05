#pragma once
#include "engine/rom.h"
namespace lordly {
class BodyMarkerImage {
  public:
    static constexpr int offset = 0x205fd8, size = 220;
    static constexpr quint32 address = 0x801c2b48, widthsAddress = 0x801ceee0;
    // -1: foreign instructions; 0: original; 1: source-built wide-frame support.
    static int mode(const Rom &);
};
} // namespace lordly
