#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QVector>

namespace lordly {
struct PersuasionGroup {
    QString name;
    QVector<int> members;
};
// Starting Talk score. Health, temperament and custom fixed-chance overrides
// remain owned by the existing Talk resolver.
class Persuasion {
  public:
    static constexpr int ownerOffset = 0x214750, ownerSize = 0x13c;
    static constexpr quint32 ownerAddress = 0x801d1480;
    static Persuasion load(const Rom &);
    const QVector<PersuasionGroup> &groups() const { return groups_; }
    QVector<PersuasionGroup> leaderGroups() const;
    QList<int> leaders() const { return rows_.keys(); }
    int score(int leader, int group) const;
    int groupFor(int member) const;
    QString unavailable() const { return unavailable_; }
    bool modified() const { return json() != original_; }
    void setScore(int leader, int group, int score);
    void addLeader(int leader);
    void setGroupScore(int leaderGroup, int targetGroup, int score);
    void editLeaderGroup(int group, const QString &name, const QVector<int> &members, int classCount);
    void addGroup(const QString &name, const QVector<int> &members, int copyGroup, int classCount);
    void editGroup(int group, const QString &name, const QVector<int> &members, int classCount);
    QJsonObject json() const;
    void apply(const QJsonObject &, int classCount);
    QByteArray runtime() const;
    QByteArray owner(quint32 resourceOffset, quint32 allocation) const;
    QByteArray assembly(quint32 resourceOffset, quint32 allocation) const;
    QByteArray storage() const;
    quint32 installedOffset() const { return installedOffset_; }
    quint32 installedSize() const { return installedSize_; }

  private:
    QVector<PersuasionGroup> groups_;
    QVector<PersuasionGroup> leaderGroups_;
    QVector<int> fallback_;
    QMap<int, QVector<int>> rows_;
    QJsonObject original_;
    QString unavailable_;
    quint32 installedOffset_ = 0, installedSize_ = 0;
};
}
