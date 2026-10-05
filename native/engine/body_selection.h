#pragma once
#include "engine/rom.h"
namespace lordly {
class BodySelectionImage {
  public:
    static constexpr int offset = 0x2a9364, size = 1272;
    static constexpr quint32 address = 0x80239c04;
    static constexpr int descriptorOffset = 0x27c4f00, descriptorSize = 96;
    // -1: foreign code/header; 0: original or owned baseline; positive: map count.
    static int count(const Rom &);
    static void validate(const Rom &, const QByteArray &mappings);
};
} // namespace lordly
