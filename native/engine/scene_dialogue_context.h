#pragma once
#include "engine/rom.h"
#include <QVector>
#include <array>
namespace lordly {
struct DialogueTextEnvironment;
struct DialogueNameOverride {
    quint8 selector;
    QByteArray text; // Complete NUL-terminated native string.
};
struct DialogueNameInputs {
    QByteArray army, guests; // 100 records, respectively 56 and 52 bytes.
    QByteArray groups;       // 25-byte records; member index is byte 2.
    QByteArray fixedName;    // Complete string at native RAM 80196A58.
    QVector<DialogueNameOverride> overrides;
    quint8 currentGroup = 0;
    quint16 defaultTextParameter = 0;
};
class SceneDialogueContext {
  public:
    enum class Kind { Absent, Army, Guest, OwnerWorkspace, GroupWorkspace, Fixed };
    struct Reference {
        Kind kind = Kind::Absent;
        int index = 0;
        bool operator==(const Reference &) const = default;
    };
    explicit SceneDialogueContext(DialogueNameInputs inputs);
    // Empty actor is a known vacant slot; a linked actor needs its matching row.
    void prepare(int mode, const QByteArray &actor, const QByteArray &bodyRow = {});
    const Reference &reference(int index) const;
    QByteArray name(int index) const;
    void supplyTextSources(DialogueTextEnvironment &environment) const;
    const QByteArray &ownerWorkspace() const {
        return owner_;
    }
    const QByteArray &groupWorkspace() const {
        return group_;
    }
    quint16 textParameter() const {
        return textParameter_;
    }
    void setTextParameter(quint32 value) {
        textParameter_ = quint16(value);
    }

  private:
    Reference find(int source) const;
    Reference ownerName(const QByteArray &actor, const QByteArray &row);
    Reference groupName();
    DialogueNameInputs inputs_;
    std::array<Reference, 4> names_;
    QByteArray owner_{64, 0}, group_{64, 0};
    quint16 textParameter_ = 0;
};
} // namespace lordly
