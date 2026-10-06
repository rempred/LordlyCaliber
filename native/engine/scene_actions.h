#pragma once
#include "engine/director.h"
namespace lordly {
struct SceneActionField {
    QString name;
    int commandOffset = 0, operand = 0, scale = 1;
    qint64 minimum = -2147483648LL, maximum = 2147483647LL;
    QString text(quint32 word) const;
    quint32 parse(const QString &) const;
    bool canonical(quint32 word) const;
};
struct SceneAction {
    int command = 0, commandCount = 1;
    QString name;
    QVector<SceneActionField> fields;
};
class SceneActions {
  public:
    static QVector<SceneAction> inspect(const DirectorProgram &);
    static DirectorProgram edit(const DirectorProgram &, int actionCommand, int field,
                                const QString &value);
};
} // namespace lordly
