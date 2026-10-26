#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QVector>

namespace lordly {
struct WeaponTypeWord { quint32 offset, original, installed; };
QVector<WeaponTypeWord> weaponTypeTextWords(const Rom &);
QByteArray canonicalWeaponTypeText(const Rom &, const QByteArray &owner, qsizetype start);
class WeaponTypes {
  public:
    static constexpr int maximumCustom = 230;
    static constexpr int headerSize = 32, labelOffset = 32, groupOffset = 1056;
    static constexpr quint32 helpKeyBase = 0x594280, retailHelpOffset = 0x2377c5a;
    static bool customId(int id) { return id == 6 || (id >= 26 && id <= 254); }
    static bool isWeapon(int id) { return (id >= 1 && id <= 13) || id == 24 || (id >= 26 && id <= 254); }
    static WeaponTypes load(const Rom &, qsizetype metadata = 0, qsizetype limit = 0);
    const QJsonObject &names() const { return names_; }
    void setNames(const QJsonObject &);
    int create(const QString &name, const QSet<int> &used);
    QByteArray build(quint32 address) const;
    const QMap<quint32, QByteArray> &originalOwners() const { return originalOwners_; }
    bool empty() const { return names_.isEmpty(); }

  private:
    QJsonObject names_;
    QMap<quint32, QByteArray> originalOwners_;
    QVector<QByteArray> retailLabels_;
    QByteArray retailHelp_;
};
} // namespace lordly
