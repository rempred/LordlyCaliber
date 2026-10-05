#pragma once
#include "engine/rom.h"
#include <QImage>
#include <QJsonObject>
#include <QMap>
#include <array>
#include <optional>
namespace lordly {
struct IconPackSpec {
    QString slug, label;
    int count, transparent;
    quint32 retailKey;
    QVector<QPair<int, int>> sites;
};
class IconArtwork {
  public:
    static constexpr int descriptorOffset = 0x27c4800, descriptorSize = 96;
    static const std::array<IconPackSpec, 2> &specs();
    static IconArtwork load(const Rom &);
    static QByteArray canonicalOwner(QByteArray code, qsizetype romStart);
    static void validateRelocations(const Rom &);
    static int packIndex(quint16 item);
    const QByteArray &words(quint16 item) const;
    QImage image(quint16 item) const;
    QVector<quint16> palette(int pack) const;
    QByteArray mapImage(quint16 item, const QImage &, bool existingPalette = true,
                        int alphaThreshold = 128) const;
    void setWords(quint16 item, const QByteArray &);
    void restore(quint16 item);
    bool modified() const {
        return !edits_.isEmpty();
    }
    bool modified(quint16 item) const {
        return edits_.contains(item);
    }
    QJsonObject json() const;
    void apply(const QJsonObject &);
    void importLegacy(const QJsonObject &);
    std::array<QByteArray, 2> decodedPacks() const;
    QByteArray rebuild(const Rom &, const std::array<QByteArray, 2> &compiled) const;

  private:
    struct Pack {
        quint32 key = 0;
        QByteArray source;
    };
    struct Placement {
        quint32 offset = 0, allocation = 0;
        std::array<quint32, 2> relative{}, sizes{};
        QByteArray hash;
    };
    std::array<Pack, 2> packs_;
    std::optional<Placement> placement_;
    QMap<quint16, QByteArray> originals_, edits_;
    QMap<int, quint32> ownerWords_;
    QByteArray descriptor_;
};
} // namespace lordly
