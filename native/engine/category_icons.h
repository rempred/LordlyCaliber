#pragma once
#include "engine/rom.h"
#include <QImage>
#include <QJsonObject>
#include <QMap>
namespace lordly {
class Promotion;
class CategoryIcons {
  public:
    static constexpr int descriptorOffset = 0x27c5200, descriptorSize = 64;
    static constexpr int ownerHigh = 0xaed4c, ownerLow = 0xaed54;
    static constexpr int pixelOffset = 0x5128;
    static CategoryIcons load(const Rom &);
    static QByteArray canonicalOwner(QByteArray, qsizetype start);
    static void validateRelocations(const Rom &);
    static QString name(int category);
    QString label(int icon) const;
    QList<int> iconIds() const;
    int create(const QString &, int copyFrom = 0);
    void assignFamily(int family, int icon);
    int familyIcon(int family) const {
        return familyIcons_.value(family);
    }
    bool hasTabRuntime() const {
        return descriptor_.startsWith("LC05CATS") && u32(descriptor_, 20) == 1;
    }
    bool hasFamilyTabs() const {
        return !familyIcons_.isEmpty();
    }
    void validateFamilies(const Promotion &) const;
    QByteArray indices(int category) const;
    QImage image(int category) const;
    QImage image(const QByteArray &) const;
    const QVector<QColor> &palette() const {
        return palette_;
    }
    QByteArray mapImage(const QImage &) const;
    void setIndices(int category, const QByteArray &);
    void restore(int category);
    bool modified() const {
        return json() != baseline_;
    }
    QJsonObject json() const;
    void apply(const QJsonObject &);
    QByteArray decodedPack(const Promotion *promotion = nullptr) const;
    QByteArray rebuild(const Rom &, const QByteArray &compiled) const;

  private:
    QByteArray source_, loadedPack_, descriptor_, sourceContainer_;
    QMap<int, QByteArray> edits_;
    QMap<int, QByteArray> custom_;
    QMap<int, QString> names_;
    QMap<int, int> familyIcons_;
    QJsonObject baseline_;
    QByteArray savedClassTabs_ = QByteArray(256, 0);
    QVector<QColor> palette_;
    quint32 high_ = 0, low_ = 0, key_ = 0, placement_ = 0, allocation_ = 0;
};
} // namespace lordly
