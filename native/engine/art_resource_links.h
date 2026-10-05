#pragma once
#include "engine/rom.h"
namespace lordly {
// Fingerprints compare stable instructions while these independently validated
// resource-key operands follow the current artwork allocation.
QByteArray canonicalArtOwner(QByteArray code, qsizetype romStart);
void validateArtResourceLinks(const Rom &);
void validateArtOwnerLinks(const Rom &, qsizetype romStart, qsizetype size);
} // namespace lordly
