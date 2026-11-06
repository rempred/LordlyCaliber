#pragma once
#include "engine/rom.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
namespace lordly {
class ConsumableEffects;
class ConsumableTree {
public:
    static bool supported(int item);
    static QJsonArray retail(int item, const ConsumableEffects &effects);
    static QMap<QString, QString> actions();
    static QMap<QString, QString> conditions();
    static QString label(const QJsonObject &node);
    static QString retailUse(int item);
    static QJsonObject action(const QString &type);
    static QJsonObject condition(const QString &type);
    static void validate(const QJsonArray &tree);
    // Fixed-width nodes, forward branches only. Runtime never follows editor text.
    static QByteArray compile(const QJsonArray &tree);
};
}
