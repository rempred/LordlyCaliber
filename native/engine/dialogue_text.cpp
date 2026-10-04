#include "engine/dialogue_text.h"
#include "game/director/dialogue_layout.h"
#include "game/director/dialogue_prefix.h"
#include "game/director/text_metrics.h"
namespace lordly {
quint8 DialogueFontMetrics::glyphWidth(quint32 character, bool other) const {
    quint8 width = 0;
    if (lordly_text_glyph_width(
            character, other, fixedIndex, reinterpret_cast<const uint8_t *>(normal.constData()),
            size_t(normal.size()), reinterpret_cast<const uint8_t *>(alternate.constData()),
            size_t(alternate.size()), &width))
        throw Error("Text glyph exceeds its currently selected metric table");
    return width;
}
void parseDialoguePrefix(QByteArray &window, QByteArray &state, const QByteArray &source,
                         quint32 &cursor) {
    if (window.size() != 168 || state.size() != 1144)
        throw Error("Dialogue prefix requires complete window and payload records");
    auto nextWindow = window, nextState = state;
    auto nextCursor = cursor;
    if (lordly_dialogue_prefix(reinterpret_cast<uint8_t *>(nextWindow.data()),
                               reinterpret_cast<uint8_t *>(nextState.data()),
                               reinterpret_cast<const uint8_t *>(source.constData()),
                               size_t(source.size()), &nextCursor))
        throw Error("Dialogue leading control exceeds its current text source");
    window = std::move(nextWindow);
    state = std::move(nextState);
    cursor = nextCursor;
}
InitializedDialogue initializeDialogueWindow(const QByteArray &window, qint32 resourceSlot,
                                             const QByteArray &source, quint16 measuredWidth,
                                             quint16 measuredLines) {
    if (window.size() != 168 || source.indexOf('\0') < 0)
        throw Error("Dialogue initialization requires a complete window and terminated source");
    InitializedDialogue result{window, QByteArray(1144, 0), 0, 0};
    auto &record = result.window;
    auto &state = result.state;
    put8(record, 2, u8(record, 2) | 9);
    put8(record, 3, u8(record, 3) | 1);
    put16(record, 0, u16(record, 0) | 0x200);
    put32(record, 0x14, 0x8019981c);
    put32(record, 0x18, 0x8019985c);
    put32(record, 0x1c, 0x801998a0);
    put16(record, 0x20, 0x478);
    put32(record, 0x24, 0);
    put8(record, 4, 8);
    put16(state, 0, quint16(resourceSlot));
    put16(state, 0x14, 0xffff);
    put8(state, 0x49, 255);
    parseDialoguePrefix(record, state, source, result.cursor);
    put32(state, 8, result.cursor);
    LordlyDialogueLayout geometry{};
    geometry.columns = u16(state, 0x1a);
    geometry.rows = u16(state, 0x1e);
    geometry.portrait = qint16(u16(state, 0x14));
    geometry.style = u8(state, 0x47);
    geometry.portraitSide = u8(state, 0x3d);
    geometry.portraitVariant = u8(state, 0x4a);
    LordlyDialogueOptions options{u8(record, 0x8a),
                                  u8(record, 0x91),
                                  u8(record, 0x8b),
                                  u8(record, 4),
                                  u16(record, 0x92),
                                  u16(record, 0x8c),
                                  u16(record, 0x94),
                                  quint16((u8(record, 0x8e) << 8) | u8(record, 0x90)),
                                  0,
                                  0};
    result.warnings = lordly_dialogue_layout(&geometry, &options, measuredWidth, measuredLines);
    put16(record, 0x28, options.width);
    put16(record, 0x2a, options.height);
    put8(record, 0x8a, options.flags);
    put16(state, 0x14, quint16(geometry.portrait));
    put16(state, 0x1a, geometry.columns);
    put16(state, 0x1e, geometry.rows);
    put16(state, 0x20, geometry.width);
    put16(state, 0x22, geometry.height);
    put16(state, 0x24, geometry.start.x);
    put16(state, 0x26, geometry.start.y);
    put16(state, 0x28, geometry.targetX);
    put16(state, 0x2a, geometry.targetY);
    put16(state, 0x2c, geometry.currentX);
    put16(state, 0x2e, geometry.currentY);
    put16(state, 0x30, quint16(geometry.pointerX));
    put16(state, 0x32, quint16(geometry.pointerY));
    put16(state, 0x38, geometry.steps);
    put8(state, 0x3a, geometry.pointerSide);
    put8(state, 0x3b, geometry.pointerEdge);
    put8(state, 0x3d, geometry.portraitSide);
    put8(state, 0x4a, geometry.portraitVariant);
    return result;
}
} // namespace lordly
