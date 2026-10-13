#pragma once
#include "engine/rom.h"
#include <QVector>
#include <optional>

namespace lordly {
// Reversible, checked record of capacity-owned edits. Models inspect the image
// before these engine patches; the unmodified runtime image remains available
// for save-state interpretation. Unknown bytes are never reconstructed.
struct CapacityPlacement {
    struct Patch { quint32 site; QByteArray before, after; };
    quint32 offset=0, size=0, originalSize=0, base=0, codeAddress=0;
    QVector<Patch> patches;
    static std::optional<CapacityPlacement> read(const Rom &rom);
    Rom canonical(const Rom &rom) const;
    static QByteArray record(const QByteArray &before,const QByteArray &after,
                             quint32 base,quint32 codeAddress);
};
} // namespace lordly
