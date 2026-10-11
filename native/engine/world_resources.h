#pragma once
#include "engine/archive.h"
#include <QMap>
#include <QVector>
#include <optional>

namespace lordly {
struct WorldPlacement {
    quint32 offset = 0, allocation = 0;
};
// The original maizo / spmap / ktenmain-SCINCSV-shopcsv resource directory.
// Index 60 in the site collection remains shopcsv even when new members follow it.
class WorldResources {
  public:
    static constexpr int keyCount = 65, selectorTable = 0x13c550, selectorStride = 12;
    static constexpr int descriptorOffset = 0x27c4500, descriptorSize = 96;
    static constexpr quint32 keyBase = 0x594280, root = 0x2743c78;
    static WorldResources load(const Rom &rom);
    const QVector<LhaMember> &treasureMembers() const {
        return treasure_;
    }
    const QVector<LhaMember> &siteMembers() const {
        return sites_;
    }
    int treasureSelector(int key) const;
    int siteSelector(int key) const;
    QByteArray treasure(const Rom &rom, int key) const;
    QByteArray sites(const Rom &rom, int key) const;
    QByteArray towns(const Rom &rom) const {
        return sites_.first().decode(rom);
    }
    QByteArray shopInventory(const Rom &rom) const {
        return sites_[60].decode(rom);
    }
    std::optional<WorldPlacement> placement() const {
        return placement_;
    }
    QByteArray rebuild(const Rom &rom, const QByteArray &towns,
                       const QMap<int, QByteArray> &treasureEdits,
                       const QMap<int, QByteArray> &siteEdits) const;

  private:
    QVector<LhaMember> treasure_, sites_;
    QByteArray selectors_, baseline_, identity_;
    std::optional<WorldPlacement> placement_;
};
} // namespace lordly
