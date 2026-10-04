#pragma once
#include "engine/dialogue_text.h"
#include <QVector>
#include <array>
#include <optional>
namespace lordly {
struct DialogueInsertion {
    int key; // Raw substitution byte, or -1 for the long-name suffix.
    quint32 cursor = 0;
    bool operator==(const DialogueInsertion &) const = default;
};
struct DialogueChildRequest {
    qint16 x, y;
    quint16 entry;
};
struct DialogueTextFrame {
    QByteArray window, state;
    // Payload +C is 0/1 presence on PC; this typed reference owns its meaning.
    std::optional<DialogueInsertion> insertion;
    std::array<quint8, 8> controls{};
    QVector<DialogueChildRequest> children;
    int glyphSounds = 0; // Requests to sound service (1, 4, 0x1A, 10, 0).
};
// One text-reveal invocation, not a full dialogue/window update. External effects
// are returned as requests. Rejected input leaves the entire frame unchanged.
quint8 revealDialogueText(DialogueTextFrame &frame, const QByteArray &source,
                          const DialogueTextEnvironment &environment, quint16 speed,
                          int budget = 100000);
} // namespace lordly
