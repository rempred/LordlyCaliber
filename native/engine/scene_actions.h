#pragma once
#include "engine/director.h"
#include <optional>
namespace lordly {
struct SceneActionField {
    QString name;
    int commandOffset = 0, operand = 0, scale = 1;
    qint64 minimum = -2147483648LL, maximum = 2147483647LL;
    qint64 bias = 0;
    QVector<QPair<int, int>> mirrors;
    QVector<qint64> choices;
    QString text(quint32 word) const;
    quint32 parse(const QString &) const;
    bool canonical(quint32 word) const;
};
struct SceneAction {
    int command = 0, commandCount = 1;
    QString name;
    QVector<SceneActionField> fields;
    int actorOperand = -1;
};
struct SceneActionPosition {
    int command = 0;
    QString label;
    bool canWait = false;
    QString waitReason;
};
class SceneActions {
  public:
    // Authoring choice; encodes the reuse form of Director 0x63.
    static constexpr quint32 RotateSprite = 0x10063;
    static constexpr quint32 CreatePathSprite = 0x20063, OrientSprite = 0x30063;
    static QVector<SceneAction> inspect(const DirectorProgram &);
    static QVector<SceneActionPosition> positions(const DirectorProgram &);
    static QMap<quint32, QString> kinds();
    static DirectorProgram insert(const DirectorProgram &, int before, quint32 kind,
                                  int ticks = 30);
    static DirectorProgram move(const DirectorProgram &, int actionCommand, int before);
    static DirectorProgram remove(const DirectorProgram &, int actionCommand);
    static DirectorProgram edit(const DirectorProgram &, int actionCommand, int field,
                                const QString &value);
    // Resource group/entry from an unambiguous preceding constructor in the
    // same linear action section. Unknown control flow returns no inference.
    static std::optional<QPair<int, int>> ribbonSource(const DirectorProgram &, int before,
                                                       quint32 slot);
    static std::optional<int> ribbonConstructor(const DirectorProgram &, int before, quint32 slot);
};
} // namespace lordly
