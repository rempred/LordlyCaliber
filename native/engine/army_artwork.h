#pragma once
#include "engine/class_data.h"
#include <QImage>
#include <array>
namespace lordly {
struct ArmyAtlasSpec {
    QString slug, label;
    int width, height, retailCount, capacity, tail;
    quint32 retailKey;
    QVector<int> transparent;
    QVector<QPair<int, int>> sites;
};
struct ArmyClassRoute {
    int lane = 0, model = 0;
};
class ArmyArtwork {
  public:
    static constexpr int descriptorOffset = 0x27c4a00, descriptorSize = 160;
    static constexpr quint32 resourceBase = 0x594280;
    static const std::array<ArmyAtlasSpec, 6> &specs();
    static ArmyArtwork load(const Rom &);
    static QByteArray canonicalOwner(QByteArray, qsizetype romStart);
    static void validateRelocations(const Rom &);
    static int classLane(const ClassAsset &);
    static int classModel(const ClassAsset &);
    QVector<int> consumers(const ClassData &, int atlas, int model) const;
    const QMap<int, ArmyClassRoute> &routes() const {
        return routes_;
    }
    void setRoute(const ClassData &, int classId, int lane, int model);
    void restoreRoute(int classId) {
        routes_.remove(classId);
    }
    void applyRoutes(ClassData &) const;
    void validateRoutes(const ClassData &) const;
    int sourceCount(int atlas) const;
    bool present(int atlas, int model) const;
    QByteArray indices(int atlas, int model) const;
    QByteArray words(int atlas, int model, int palette = 0) const;
    QImage image(int atlas, int model, int palette = 0) const;
    QVector<quint16> palette(int atlas, int ordinal = 0) const;
    QByteArray mapImage(int atlas, int palette, const QImage &, int alphaThreshold = 128,
                        const QByteArray &previousIndices = {}, bool orderedDither = false) const;
    void setIndices(int atlas, int model, const QByteArray &);
    void restore(int atlas, int model);
    bool modified() const {
        return pixelsModified() || !routes_.isEmpty();
    }
    bool pixelsModified() const {
        return !edits_.isEmpty();
    }
    bool modified(int atlas, int model) const {
        return edits_.contains(atlas * 64 + model);
    }
    QJsonObject json() const;
    void apply(const QJsonObject &);
    void importLegacy(const QJsonObject &);
    std::array<QByteArray, 6> decodedPacks() const;
    QByteArray rebuild(const Rom &, const std::array<QByteArray, 6> &) const;

  private:
    struct Pack {
        quint32 key = 0;
        int count = 0;
        QByteArray source;
    };
    struct Placement {
        quint32 offset = 0, allocation = 0;
        std::array<quint32, 6> relative{}, sizes{};
        QByteArray hash;
    };
    static const ArmyAtlasSpec &validate(int atlas);
    static const ArmyAtlasSpec &validate(int atlas, int model);
    std::array<Pack, 6> packs_;
    std::optional<Placement> placement_;
    QMap<int, QByteArray> edits_;
    QMap<int, ArmyClassRoute> routes_;
    QMap<int, quint32> ownerWords_;
    QByteArray descriptor_;
};
} // namespace lordly
