#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QVector>
namespace lordly {
struct GameRule {
    QString key, group, label, help;
    int minimum, maximum, step, retail;
    bool percent = false;
};
struct RuleBuild {
    QString name;
    int offset, size;
    quint32 address;
    QByteArray source, expected;
    QMap<QString, quint32> symbols;
    bool data = false;
};
class GameRules {
  public:
    static GameRules load(const Rom &);
    static QVector<GameRule> definitions();
    int value(const QString &key) const { return values_.value(key); }
    int loaded(const QString &key) const { return originals_.value(key); }
    QString unavailable(const QString &group) const { return unavailable_.value(group); }
    bool modified() const { return values_ != originals_; }
    QJsonObject json() const;
    void apply(const QJsonObject &);
    QVector<RuleBuild> builds() const;
  private:
    QMap<QString, int> values_, originals_;
    QMap<QString, QString> unavailable_;
    QMap<QString, QByteArray> owners_;
};
}
