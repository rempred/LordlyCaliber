#pragma once
#include "engine/rom.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
namespace lordly {
// Compiles structured events into the existing VM. Original code never moves.
class EventAuthoring {
  public:
    static bool supported(int row);
    static QMap<int, QString> triggers();
    static QMap<QString, QString> conditions();
    static QMap<QString, QString> actions();
    static QByteArray compile(const QByteArray &, int row, const QJsonArray &trees,
                              const QJsonObject &triggers);
    static QMap<QString, int> entries(const QByteArray &, int row, const QJsonArray &trees,
                                     const QJsonObject &triggers);
    static void validateReferences(const QJsonArray &, const QSet<int> &scenes, const QSet<int> &flags);
};
}
