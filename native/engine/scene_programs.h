#pragma once
#include "engine/director.h"
#include <QJsonObject>
namespace lordly {
// Edits are attached to physical resources: all existing selector aliases share
// one program, as they do in the loaded ROM.
class ScenePrograms {
  public:
    static ScenePrograms load(const Rom &);
    const Directors &original() const {
        return original_;
    }
    const DirectorResource &resource(quint32 key) const;
    DirectorProgram program(quint32 key) const;
    void setOperand(quint32 key, int command, int operand, quint32 value);
    void insertCommand(quint32 key, int before, quint32 opcode);
    void removeCommand(quint32 key, int command);
    void replace(quint32 key, const QByteArray &);
    void restore(quint32 key);
    QJsonObject json() const;
    void apply(const QJsonObject &);
    const QMap<quint32, QByteArray> &plan() const {
        return changes_;
    }
    int changedPrograms() const {
        return int(changes_.size());
    }
    bool modified() const {
        return !changes_.isEmpty();
    }

  private:
    explicit ScenePrograms(Directors original) : original_(std::move(original)) {}
    Directors original_;
    QMap<quint32, QByteArray> changes_;
};
} // namespace lordly
