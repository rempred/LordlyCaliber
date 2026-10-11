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
enum class SceneActionTiming { None, Counter, SharedCounter, Completion };
struct SceneAction {
    int command = 0, commandCount = 1;
    QString name;
    QVector<SceneActionField> fields;
    int actorOperand = -1;
    SceneActionTiming timing = SceneActionTiming::None;
    bool waitsForCompletion = false;
};
struct SceneActionPosition {
    int command = 0;
    QString label;
    bool canWait = false;
    QString waitReason;
};
struct SceneChoice {
    int command = 0;
    int firstBegin = 0, firstEnd = 0, secondBegin = 0, secondEnd = 0, end = 0;
    int option(int index) const {
        return index >= secondBegin && index <= secondEnd ? 2
               : index >= firstBegin && index <= firstEnd ? 1
                                                          : 0;
    }
    bool control(int index) const {
        return (index >= command && index < firstBegin) ||
               (index >= firstEnd && index < secondBegin) || (index >= secondEnd && index < end);
    }
};
class SceneActions {
  public:
    // Authoring choice; encodes the reuse form of Director 0x63.
    static constexpr quint32 RotateSprite = 0x10063;
    static constexpr quint32 CreatePathSprite = 0x20063, OrientSprite = 0x30063;
    static constexpr quint32 Choice = 0x100bf;
    static QVector<SceneChoice> choices(const DirectorProgram &);
    static QByteArray choiceText(const QString &first, const QString &second);
    static std::optional<QPair<QString, QString>> choiceOptions(const QByteArray &);
    static QVector<SceneAction> inspect(const DirectorProgram &);
    static QVector<SceneActionPosition> positions(const DirectorProgram &);
    static QMap<quint32, QString> kinds();
    static DirectorProgram insert(const DirectorProgram &, int before, quint32 kind,
                                  int ticks = 30);
    static DirectorProgram move(const DirectorProgram &, int actionCommand, int before);
    static DirectorProgram remove(const DirectorProgram &, int actionCommand);
    static DirectorProgram setCompletion(const DirectorProgram &, int actionCommand, bool wait);
    static DirectorProgram edit(const DirectorProgram &, int actionCommand, int field,
                                const QString &value);
    // Resource group/entry from an unambiguous preceding constructor in the
    // same linear action section. Unknown control flow returns no inference.
    static std::optional<QPair<int, int>> ribbonSource(const DirectorProgram &, int before,
                                                       quint32 slot);
    static std::optional<int> ribbonConstructor(const DirectorProgram &, int before, quint32 slot);
};
} // namespace lordly
