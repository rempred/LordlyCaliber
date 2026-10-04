#pragma once
#include "engine/resource.h"
#include <optional>

namespace lordly {
// ROM resource keys are relative to the game's resource header base, not RAM.
struct ClassTextPlacement {
    quint32 offset = 0, allocation = 0, decodedSize = 0, count = 0;
    static constexpr quint32 descriptorOffset = 0x2378854, descriptorSize = 64;
    static constexpr quint32 keyBase = 0x594280, retailKey = descriptorOffset - keyBase;
    static std::optional<ClassTextPlacement> read(const Rom &rom);
    QByteArray canonicalReferences(const QByteArray &owner, qsizetype start) const;
};
// Compiled authored text is moved only when it outgrows the retail resource or
// already has an owned placement. Original help entries remain in the bank.
QByteArray linkClassText(const Rom &rom, const QByteArray &compiled, int count);
void verifyClassDescriptionRouting(const QByteArray &image, quint32 entry);
void installClassDescriptionRouting(QByteArray &image, quint32 previousEntry, quint32 nextEntry);
} // namespace lordly
