#pragma once
#include "engine/rom.h"
#include "engine/attack_visual.h"
#include <QJsonObject>
#include <QMap>
#include <QVector>
#include <optional>

namespace lordly {
struct AttackPlacement {
    static constexpr quint32 descriptorOffset = 0x27c5300, descriptorSize = 96;
    static constexpr quint32 codeCapacity = 0x8000;
    quint32 romOffset = 0, address = 0, size = 0;
    quint32 records = 0, settings = 0, code = 0, codeSize = 0;
    quint32 spellIndexEntry = 0, amountEntry = 0, amountOriginal = 0;
    quint32 hooks = 0, hookCount = 0;
    int count = 0;
    static std::optional<AttackPlacement> read(const Rom &);
    QByteArray descriptor(const QByteArray &) const;
};
struct Attack {
    QByteArray data, name; // Twelve D bytes, and encoded game text without its terminator.
    int nameGroup = 0, behavior = 0, animation = 0;
    bool dynamicName = false;
    // Zero retains the retail targeting; custom masks address nine formation cells.
    int targetPattern = 0, targetSide = 0, targetMask = 0;
    int powerPercent = 100;
    // -1 retains the retail secondary-effect rules. Chance is an explicit percentage.
    int status = -1, condition = 1, chance = 100;
    int afterEffect = -1, afterBasis = 0, afterPercent = 100;
    AttackVisual visual;
    bool operator==(const Attack &) const = default;
};
class AttackData {
  public:
    static constexpr int retailCount = 159, maximumCount = 255;
    static constexpr quint32 retailBase = 0x8018aa7c, retailOffset = 0x6097c;
    static AttackData loadRetail(const Rom &);
    static AttackData load(const Rom &);
    QByteArray build(quint32 address) const;
    static constexpr quint32 recordOffset = 64, settingsOffset = recordOffset + 256 * 16;
    static constexpr quint32 scratchOffset = settingsOffset + 256 * 32;
    static constexpr quint32 namesOffset = scratchOffset + 128;
    const QVector<Attack> &entries() const {
        return entries_;
    }
    const Attack &entry(int id) const;
    void set(int id, const Attack &);
    void rename(int id, const QString &, bool shared);
    int duplicate(int source, const QString &name);
    int create(const QString &name);
    QVector<int> sharingName(int id) const;
    bool modified() const {
        return entries_ != original_;
    }
    QJsonObject json() const;
    void apply(const QJsonObject &);
    static bool isTemplate(int id);
    static QMap<int, int> templateTargets(int id);
    static QString displayName(const Attack &);
    static QMap<int, QString> elements();
    static QMap<int, QString> effects();
    static QMap<int, QString> calculations();
    static QMap<int, QString> statuses();

  private:
    static void validate(const Attack &);
    int nextNameGroup() const;
    QVector<Attack> entries_, original_;
};
} // namespace lordly
