#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QVector>
#include <array>

namespace lordly {
// Combat leader bonuses, separate from encounter persuasion bonuses.
class LeaderAffinity {
  public:
    static constexpr int offset = 0x2347b8, size = 0x218;
    static constexpr quint32 address = 0x801f14e8;
    using Bonuses = std::array<int, 4>;
    static LeaderAffinity load(const Rom &);
    static Bonuses retail(int leader);
    static QVector<int> members(int group);
    static QString groupName(int group);
    static int capacity();
    Bonuses bonuses(int leader) const { return rows_.value(leader, Bonuses{}); }
    QString unavailable() const { return unavailable_; }
    bool modified() const { return rows_ != original_; }
    void set(int leader, int group, int strength);
    QJsonObject json() const;
    void apply(const QJsonObject &);
    QByteArray image() const;
    QByteArray assembly() const;

  private:
    QMap<int, Bonuses> rows_, original_;
    QByteArray owner_;
    QString unavailable_;
};
}
