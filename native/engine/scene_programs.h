#pragma once
#include "engine/director.h"
#include "engine/scene_program_resources.h"
#include <QJsonObject>
namespace lordly {
class Dialogues;
struct SceneId {
    quint32 nativeKey = 0;
    QString authoredId;
    SceneId() = default;
    SceneId(quint32 key) : nativeKey(key) {}
    static SceneId authored(QString id) {
        SceneId result;
        result.authoredId = std::move(id);
        return result;
    }
    explicit operator bool() const {
        return nativeKey || !authoredId.isEmpty();
    }
    bool operator==(const SceneId &) const = default;
    QString label() const {
        return authoredId.isEmpty() ? QString::number(nativeKey, 16) : authoredId;
    }
};
struct AuthoredScene {
    QString id, name;
    int selector = -1;
    DirectorResource baseline;
    DirectorProgram current;
};
// Edits are attached to physical resources: all existing selector aliases share
// one program, as they do in the loaded ROM.
class ScenePrograms {
  public:
    static ScenePrograms load(const Rom &);
    const Directors &original() const {
        return original_;
    }
    const DirectorResource &resource(SceneId id) const;
    DirectorProgram program(SceneId id) const;
    // Snapshot for PC execution. New authored scenes receive virtual keys;
    // these are resource identities, never ROM addresses or export placements.
    Directors runtimeDirectors() const;
    void setOperand(SceneId id, int command, int operand, quint32 value);
    void insertCommand(SceneId id, int before, quint32 opcode);
    void removeCommand(SceneId id, int command);
    void replace(SceneId id, const QByteArray &);
    void restore(SceneId id);
    void validateReferences(const Rom &, const Dialogues *dialogues = nullptr) const;
    QVector<SceneId> scenes() const;
    QVector<int> availableSelectors() const;
    bool canCreateBlank(SceneId source) const;
    SceneId createScene(const QString &name, SceneId source, bool blank);
    void renameScene(SceneId, const QString &name);
    QString displayName(SceneId) const;
    const QMap<quint32, QString> &replacementNames() const { return replacementNames_; }
    void removeScene(SceneId);
    SceneId canonical(SceneId) const;
    const QMap<QString, AuthoredScene> &authoredScenes() const {
        return authored_;
    }
    QVector<AuthoredSceneBuild> authoredPlan() const;
    QJsonObject json() const;
    void apply(const QJsonObject &);
    const QMap<quint32, QByteArray> &plan() const {
        return changes_;
    }
    int changedPrograms() const {
        QSet<quint32> keys;
        for (auto key : changes_.keys()) keys.insert(key);
        for (auto key : replacementNames_.keys()) keys.insert(key);
        return int(keys.size()) + authoredChanges();
    }
    bool modified() const {
        return !changes_.isEmpty() || !replacementNames_.isEmpty() || authoredChanges() != 0;
    }

  private:
    explicit ScenePrograms(Directors original) : original_(std::move(original)) {}
    int authoredChanges() const;
    void validateRemovedSelectors(const QSet<int> &) const;
    QMap<QString, AuthoredScene> authored_, originalAuthored_;
    Directors original_;
    QMap<quint32, QByteArray> changes_;
    QMap<quint32, QString> replacementNames_;
};
} // namespace lordly
