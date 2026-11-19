#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QVector>
#include <optional>
namespace lordly {
struct EventReaction {
    int selector = -1, requestOffset = -1;
    bool readAnswer = false;
    QVector<int> instructions;
    QMap<int, bool> flags;
    QMap<int, quint16> translations;
};
// Resume a mission rule at the scene-completion boundary. Inputs beyond the
// supplied answer and flags written during this invocation remain unknown.
std::optional<EventReaction> storyReaction(const QByteArray &program, int row, int group,
                                          int entry, int requestOffset, int answer,
                                          const QJsonObject &gates);
}
