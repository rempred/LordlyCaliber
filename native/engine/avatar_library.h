#pragma once
#include "engine/rom.h"
#include <QImage>
#include <QMap>
#include <QVector>
#include <optional>
namespace lordly {
struct AvatarPlacement {
    quint32 offset = 0, allocation = 0;
    QByteArray hash;
};
struct AvatarBundle {
    QByteArray indices;
    QVector<QByteArray> palettes;
};
// Resource directory and image storage, independent of class-table placement.
class AvatarLibrary {
  public:
    static constexpr quint32 retailKey = 0x1dfb6ea, resourceBase = 0x594280;
    static constexpr int descriptorOffset = 0x27c4900, descriptorSize = 96;
    static const QVector<QPair<int, int>> &sites();
    static AvatarLibrary load(const Rom &);
    static QByteArray canonicalOwner(QByteArray, qsizetype romStart);
    static void validateRelocations(const Rom &);
    quint32 directoryKey() const {
        return key_;
    }
    const QVector<quint32> &members() const {
        return members_;
    }
    const AvatarBundle &bundle(int member) const;
    QByteArray words(quint16 token) const;
    QByteArray palette(quint16 token) const;
    QImage image(quint16 token) const;
    const std::optional<AvatarPlacement> &placement() const {
        return placement_;
    }
    QByteArray storedBundle(int member) const;

  private:
    Rom rom_;
    quint32 key_ = 0;
    QVector<quint32> members_;
    mutable QMap<quint32, AvatarBundle> bundles_;
    std::optional<AvatarPlacement> placement_;
};
} // namespace lordly
