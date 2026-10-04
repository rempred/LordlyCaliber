#pragma once
#include "engine/rom.h"
namespace lordly {
struct DialogueFontMetrics {
    QByteArray normal, alternate;
    qint16 fixedIndex = -1;
    quint8 glyphWidth(quint32 character, bool other = false) const;
};
// Text coordinates are archive-relative byte offsets, never host/native pointers.
// The leading-control grammar is distinct from the body and metric grammars.
void parseDialoguePrefix(QByteArray &window, QByteArray &state, const QByteArray &source,
                         quint32 &cursor);
struct InitializedDialogue {
    QByteArray window, state;
    quint32 cursor;
    unsigned warnings;
};
// Measurements come from the separate native width/line grammars, not QString
// length. Payload text/cursor pointers are source-relative offsets on the host.
InitializedDialogue initializeDialogueWindow(const QByteArray &window, qint32 resourceSlot,
                                             const QByteArray &source, quint16 measuredWidth,
                                             quint16 measuredLines);
} // namespace lordly
