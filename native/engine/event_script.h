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
    bool researched = false;
    QString flagLabel;
};
struct EventHandler {
    int group = 0, entry = 0;
    QVector<EventStep> steps;
    QString title;
};
struct EventParameter {
    QString key, label;
    int value = 0, minimum = 0, maximum = 255;
    QMap<int, QString> choices;
    // Each part addresses an instruction's operand byte; masks retain other bits.
    // The comparison field has no operand parts: its validated relation nibble
    // is stored separately under comparison:<offset> by parameterEdit.
    struct Part { int offset, mask = 255, shift = 0, valueShift = 0; };
    QVector<Part> parts;
    QString kind;
};
class EventScript {
  public:
    static QVector<EventHandler> decode(const QByteArray &, int eventRow = -1);
    // Presentation only: authenticated retail descriptions, with current-byte
    // decoding after edits. Export validation continues to use decode/apply.
    static QVector<EventHandler> display(const QByteArray &, int eventRow, bool originalSceneContext = true);
    static QString rowName(int eventRow);
    static QMap<int, EventStep> flagSteps(const QByteArray &);
    static QMap<int, EventStep> branchSteps(const QByteArray &);
    static QByteArray apply(const QByteArray &, const QJsonObject &edits);
    static QString flagName(int flag, int eventRow = -1, int offset = -1);
    static QString groupName(int group);
    static QSet<int> instructionOffsets(int row);
    static QVector<EventParameter> parameters(const QByteArray &, int row, int offset);
    static QJsonObject parameterEdit(const QByteArray &, int row, int offset, const QString &key, int value);
    static QByteArray applyOperands(const QByteArray &, int row, const QJsonObject &);
};
} // namespace lordly
