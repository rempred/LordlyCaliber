#pragma once
#include "engine/director.h"
namespace lordly {
struct SceneActionField {
    QString name;
    int commandOffset = 0, operand = 0, scale = 1;
    qint64 minimum = -2147483648LL, maximum = 2147483647LL;
    qint64 bias = 0;
    QString text(quint32 word) const;
    quint32 parse(const QString &) const;
    bool canonical(quint32 word) const;
};
struct SceneAction {
    int command = 0, commandCount = 1;
    QString name;
    QVector<SceneActionField> fields;
};
struct SceneActionPosition {
    int command = 0;
    QString label;
    bool canWait = false;
    QString waitReason;
};
class SceneActions {
  public:
    static QVector<SceneAction> inspect(const DirectorProgram &);
    static QVector<SceneActionPosition> positions(const DirectorProgram &);
    static QMap<quint32, QString> kinds();
    static DirectorProgram insert(const DirectorProgram &, int before, quint32 kind,
                                  int ticks = 30);
    static DirectorProgram remove(const DirectorProgram &, int actionCommand);
    static DirectorProgram edit(const DirectorProgram &, int actionCommand, int field,
                                const QString &value);
};
} // namespace lordly
