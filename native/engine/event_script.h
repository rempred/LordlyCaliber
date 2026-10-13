#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QVector>
namespace lordly {
struct EventStep {
    int offset = 0, length = 2, flag = -1, target = -1;
    bool check = false, value = false;
    QString text;
};
struct EventHandler {
    int group = 0, entry = 0;
    QVector<EventStep> steps;
};
class EventScript {
  public:
    static QVector<EventHandler> decode(const QByteArray &);
    static QMap<int, EventStep> flagSteps(const QByteArray &);
    static QByteArray apply(const QByteArray &, const QJsonObject &edits);
    static QString flagName(int flag);
    static QString groupName(int group);
};
} // namespace lordly
