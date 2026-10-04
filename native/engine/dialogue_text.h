#pragma once
#include "engine/rom.h"
#include <QHash>
namespace lordly {
struct DialogueFontMetrics {
    QByteArray normal, alternate;
    qint16 fixedIndex = -1;
    quint8 glyphWidth(quint32 character, bool other = false) const;
};
struct DialogueSegmentScan {
    quint16 count = 0;
    quint32 cursor = 0;
    quint8 stop = 0;
    bool published = false; // A known null string leaves the native cursor globals untouched.
};
// Substitution keys are raw operand bytes, not parsed decimal numbers. A missing
// key is unavailable context; a null QByteArray is a known null native pointer.
DialogueSegmentScan scanDialogueSegment(const QByteArray &source, quint16 columns,
                                        const QHash<int, QByteArray> &substitutions,
                                        int budget = 100000);
// Pure measurement: helper cursor publications are local to this operation.
quint16 countDialogueLines(const QByteArray &source, quint16 columns,
                           const QHash<int, QByteArray> &substitutions,
                           const QByteArray &longNameSuffix, int budget = 100000);
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
