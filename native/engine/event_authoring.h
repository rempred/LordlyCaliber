#pragma once
#include "engine/rom.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QVector>
namespace lordly {
struct EventActionType {
    QString type, name, group;
};
// Compiles structured events into the existing VM. Original code never moves.
class EventAuthoring {
  public:
    static bool supported(int row);
    static bool newMissionSlot(int row);
    static QJsonArray missionTemplates(int battlefield, int location, int completionFlag,
                                       int openingScene = -1, int victoryScene = -1);
    static QMap<int, QString> triggers();
    static QMap<QString, QString> conditions();
    static QMap<QString, QString> actions();
    static QVector<EventActionType> actionTypes();
    static QByteArray compile(const QByteArray &, int row, const QJsonArray &trees,
                              const QJsonObject &triggers);
    static QMap<QString, int> entries(const QByteArray &, int row, const QJsonArray &trees,
                                     const QJsonObject &triggers);
    static QVector<int> stepPath(const QJsonObject &tree,int relativeOffset);
    static QByteArray compileCondition(const QJsonObject &,int group,int origin,
                                      int yesSelector,int noSelector);
    static void validateReferences(const QJsonArray &, const QSet<int> &scenes, const QSet<int> &flags);
};
}
