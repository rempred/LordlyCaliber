#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QVector>
#include <array>

namespace lordly {
struct LeaderAffinityGroup {
    QString name;
    QVector<int> members;
    bool operator==(const LeaderAffinityGroup &) const = default;
};
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
    Bonuses bonuses(int leader) const;
    int strength(int leader, int group) const;
    const QVector<LeaderAffinityGroup> &groups() const { return groups_; }
    QList<int> leaders(int group) const;
    int groupFor(int member) const;
    void addGroup(const QString &, const QVector<int> &, int classCount);
    void editGroup(int group, const QString &, const QVector<int> &, int classCount);
    void removeGroup(int group);
    QString unavailable() const { return unavailable_; }
    bool modified() const { return json() != original_; }
    bool extended() const { return extended_; }
    void set(int leader, int group, int strength);
    QJsonObject json() const;
    void apply(const QJsonObject &);
    QByteArray image() const;
    QByteArray assembly() const;
    QByteArray runtimeData() const;
    QByteArray extendedOwner(quint32 romOffset, quint32 address, const QByteArray &, quint32 entry) const;
    void validateClasses(int classCount) const;

  private:
    QMap<int, QVector<int>> rows_;
    QVector<LeaderAffinityGroup> groups_;
    QJsonObject original_;
    QByteArray owner_;
    QString unavailable_;
    bool extended_ = false;
};
}
