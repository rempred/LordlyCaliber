#pragma once
#include "engine/dialogues.h"
#include <optional>
namespace lordly {
struct DialoguePlacement {
    quint32 offset, allocation;
    QMap<quint32, QPair<quint32, quint32>> members;
};
class DialogueResources {
  public:
    static constexpr quint32 descriptorOffset = 0x27c4600, descriptorSize = 96;
    static std::optional<DialoguePlacement> placement(const Rom &);
    static QByteArray rebuild(const Rom &, const QMap<quint32, QByteArray> &compiledArchives);
};
} // namespace lordly
