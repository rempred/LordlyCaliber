#include "engine/dialogue_text.h"
#include "game/director/dialogue_layout.h"
#include "game/director/dialogue_prefix.h"
#include "game/director/text_metrics.h"
#include <algorithm>
#include <functional>
#include <optional>
namespace lordly {
namespace {
quint16 measureText(const QByteArray &source, const DialogueFontMetrics &metrics, int &budget) {
    if (source.isNull() || budget < 1)
        throw Error("Native text measurement requires text and a positive budget");
    quint32 at = 0, width = 0, previous = 0, spacing = 0;
    bool alternate = false;
    auto read = [&](quint32 cursor) {
        if (--budget < 0)
            throw Error("Native text measurement exceeded its budget");
        return u8(source, cursor);
    };
    auto metric = [&](quint32 index, bool other) {
        return u8(other ? metrics.alternate : metrics.normal, index);
    };
    auto byteWidth = [&](quint8 c, bool other) { return metric(c < 128 ? c + 16 : c, other); };
    auto number = [&] {
        quint32 value = 0;
        while (true) {
            const auto c = read(++at);
            if (c < '0' || c > '9')
                return value;
            value = value * 10 + c - '0';
        }
    };
    auto findEnd = [&](quint32 cursor) -> std::optional<quint32> {
        while (true) {
            const auto c = read(cursor);
            if (!c)
                return {};
            if (c == 15)
                return cursor;
            ++cursor;
        }
    };
    for (;;) {
        const auto c = read(at);
        if (!c)
            return quint16(width);
        if (c == 14)
            alternate = true;
        else if (c == 15)
            alternate = false;
        else if (c == 10 || c == 13 || c == 12 || c == 8) {
            // The original backspace subtract is immediately cancelled by its
            // shared add, so it has no net measurement effect.
        } else if (c == 9) {
            const auto unit = quint32(quint16(spacing)) + 11;
            const auto tab = quint16(quint16(width) / unit + 1);
            width = ((quint32(tab) + 3) & ~3U) * unit + spacing;
            previous = 0;
        } else if (c == '{') {
            previous = 0;
            const auto kind = read(++at);
            if (kind == 'T' || kind == 't')
                previous = metric(quint32(quint16(number())) - 1, alternate);
            else if (kind == 'H' || kind == 'h')
                spacing = number();
            else
                while (read(++at) != '}') {
                }
            width += previous + spacing;
        } else if (lordly_text_lead_byte(c)) {
            read(++at); // Bounds check the second byte even though its value is ignored.
            previous = 11;
            width += previous + spacing;
        } else if (!alternate) {
            previous = byteWidth(c, false);
            width += previous + spacing;
        } else {
            previous = 0;
            const auto end = findEnd(at);
            if (!end)
                alternate = false;
            else {
                quint32 split = at;
                while (split != *end && read(split) != 16)
                    ++split;
                if (split == *end) {
                    while (at != *end)
                        previous += byteWidth(read(at++), true);
                } else {
                    quint32 left = 0;
                    while (at != split)
                        left += byteWidth(read(at++), true);
                    const auto digit = read(++at);
                    previous = quint8(digit - (digit < 65 ? 48 : digit < 97 ? 55 : 87));
                    ++at;
                    const auto rightEnd = findEnd(at);
                    if (!rightEnd)
                        throw Error("Paired native text has no closing control");
                    while (at != *rightEnd)
                        previous += byteWidth(read(at++), true);
                    previous = std::max(quint8(left), quint8(previous));
                }
                width += previous + spacing;
            }
        }
        ++at;
    }
}
DialogueSegmentScan scanSegment(const QByteArray &source, quint16 columns,
                                const QHash<int, QByteArray> &substitutions, int &budget) {
    if (budget < 1)
        throw Error("Dialogue segment budget must be positive");
    std::function<DialogueSegmentScan(const QByteArray &, int)> scan;
    auto spend = [&] {
        if (--budget < 0)
            throw Error("Dialogue segment scan exceeded its budget");
    };
    auto lookup = [&](int index) -> const QByteArray & {
        const auto found = substitutions.constFind(index);
        if (found == substitutions.constEnd())
            throw Error(QString("Dialogue substitution byte %1 is unavailable").arg(index));
        return found.value();
    };
    scan = [&](const QByteArray &text, int depth) -> DialogueSegmentScan {
        if (text.isNull())
            return {};
        if (depth >= 64)
            throw Error("Dialogue substitution nesting exceeds its limit");
        quint32 at = 0, count = 0;
        auto read = [&]() {
            spend();
            return u8(text, at++);
        };
        auto skip = [&](quint32 bytes) {
            if (bytes > quint64(text.size()) || at > quint64(text.size()) - bytes)
                throw Error("Dialogue segment control exceeds its source");
            at += bytes;
        };
        quint8 stop = read();
        while (stop) {
            if (lordly_text_lead_byte(stop)) {
                skip(1);
                ++count;
            } else if (stop == '{') {
                while (read() != '}') {
                }
                ++count;
            } else if (stop == '@') {
                stop = read();
                if (!stop || stop == 'n' || stop == 'c' || stop == '/')
                    break;
                switch (stop) {
                case '%':
                    count += scan(lookup(read()), depth + 1).count;
                    break;
                case '&': {
                    const auto length = quint8(scan(lookup(read()), depth + 1).count);
                    const int maximum = int(read()) - 48;
                    count += quint32(std::min(int(length), maximum));
                    break;
                }
                case '!': {
                    const auto length = scan(lookup(read()), depth + 1).count;
                    constexpr int limited[] = {0, 1, 1, 2, 3};
                    count += length < 5 ? limited[length] : 4;
                    break;
                }
                case 'z': {
                    const auto &name = lookup(read());
                    if (name.isNull())
                        break;
                    quint32 pos = 0, characters = 0;
                    while (true) {
                        spend();
                        const auto c = u8(name, pos);
                        if (!c)
                            break;
                        pos += lordly_text_lead_byte(c) ? 2 : 1;
                        ++characters;
                    }
                    if (quint16(characters) >= 2)
                        ++count;
                    break;
                }
                case ':':
                    if (scan(lookup(read()), depth + 1).count >= 6)
                        ++count;
                    break;
                case '<':
                case 'F':
                case 'T':
                case 'e':
                case 'f':
                case 't':
                    skip(3);
                    break;
                case 'l':
                    skip(1);
                    break;
                case 'w':
                    skip(4);
                    break;
                default:
                    break;
                }
            } else
                ++count;
            if (quint16(count) >= columns)
                break;
            stop = read();
        }
        return {quint16(count), at - (stop == 0 ? 1U : 0U), stop, true};
    };
    return scan(source, 0);
}
} // namespace
quint16 measureNativeText(const QByteArray &source, const DialogueFontMetrics &metrics,
                          int budget) {
    return measureText(source, metrics, budget);
}
DialogueSegmentScan scanDialogueSegment(const QByteArray &source, quint16 columns,
                                        const QHash<int, QByteArray> &substitutions, int budget) {
    return scanSegment(source, columns, substitutions, budget);
}
quint16 countDialogueLines(const QByteArray &source, quint16 columns,
                           const QHash<int, QByteArray> &substitutions,
                           const QByteArray &longNameSuffix, int budget) {
    if (source.isNull() || budget < 1)
        throw Error("Dialogue line measurement requires text and a positive budget");
    quint32 at = 0, column = 0, lines = 1, maximum = 1;
    auto read = [&](const QByteArray &text, quint32 &cursor) {
        if (--budget < 0)
            throw Error("Dialogue line measurement exceeded its budget");
        return u8(text, cursor++);
    };
    auto next = [&] { return read(source, at); };
    auto skip = [&](const QByteArray &text, quint32 &cursor, quint32 bytes) {
        if (bytes > quint64(text.size()) || cursor > quint64(text.size()) - bytes)
            throw Error("Dialogue line control exceeds its source");
        cursor += bytes;
    };
    auto glyph = [&] {
        ++column;
        if (quint16(column) > columns) {
            column = 1;
            ++lines;
        }
    };
    auto lookup = [&](int index) -> const QByteArray & {
        const auto found = substitutions.constFind(index);
        if (found == substitutions.constEnd())
            throw Error(QString("Dialogue substitution byte %1 is unavailable").arg(index));
        return found.value();
    };
    auto insert = [&](const QByteArray &text, quint32 cursor, quint8 limit) {
        if (text.isNull())
            return;
        quint32 used = 0;
        do {
            const auto c = read(text, cursor);
            if (!c)
                return;
            if (lordly_text_lead_byte(c))
                skip(text, cursor, 1);
            glyph();
        } while (quint8(++used) < limit);
    };
    for (;;) {
        const auto c = next();
        if (!c)
            return std::max(quint16(maximum), quint16(lines));
        if (lordly_text_lead_byte(c)) {
            skip(source, at, 1);
            glyph();
        } else if (c != '@')
            glyph();
        else {
            switch (next()) {
            case 'c':
            case '/':
                maximum = std::max(quint16(maximum), quint16(lines));
                column = 0;
                lines = 1;
                break;
            case 'n':
                column = 100;
                if (columns < 100) {
                    column = 0;
                    ++lines;
                }
                break;
            case '%':
                insert(lookup(next()), 0, 99);
                break;
            case '&': {
                const auto &text = lookup(next());
                const auto limit = next(); // Raw byte, unlike the segment scanner.
                insert(text, 0, limit);
                break;
            }
            case '!': {
                const auto &text = lookup(next());
                const auto length = scanSegment(text, columns, substitutions, budget).count;
                constexpr quint8 limited[] = {0, 1, 1, 2, 3};
                const auto limit = length < 5 ? limited[length] : quint8(4);
                if (limit)
                    insert(text, 0, limit);
                break;
            }
            case 'z': {
                const auto &text = lookup(next());
                if (text.isNull())
                    break;
                quint32 cursor = 0, last = 0, characters = 0;
                for (;;) {
                    const auto begin = cursor;
                    const auto ch = read(text, cursor);
                    if (!ch)
                        break;
                    last = begin;
                    if (lordly_text_lead_byte(ch))
                        skip(text, cursor, 1);
                    ++characters;
                }
                if (quint16(characters) >= 2)
                    insert(text, last, 99);
                break;
            }
            case ':':
                if (scanSegment(lookup(next()), columns, substitutions, budget).count >= 6)
                    insert(longNameSuffix, 0, 99);
                break;
            case '<':
            case 'F':
            case 'T':
            case 'e':
            case 'f':
            case 't':
                skip(source, at, 3);
                break;
            case 'w':
                skip(source, at, 4);
                break;
            case 'l':
                skip(source, at, 1);
                break;
            default:
                break;
            }
        }
    }
}
quint16 measureDialogueWidth(const QByteArray &source, quint16 columns,
                             const QHash<int, QByteArray> &substitutions,
                             const QByteArray &longNameSuffix, const DialogueFontMetrics &metrics,
                             qint32 minimum, int budget) {
    if (budget < 1)
        throw Error("Dialogue width measurement requires a positive budget");
    if (source.isNull())
        return 0;
    quint32 at = 0, width = 0, maximum = 0, line = 0;
    auto read = [&](const QByteArray &text, quint32 cursor) {
        if (--budget < 0)
            throw Error("Dialogue width measurement exceeded its budget");
        return u8(text, cursor);
    };
    auto next = [&] { return read(source, at++); };
    auto skip = [&](quint32 count) {
        if (count > quint64(source.size()) || at > quint64(source.size()) - count)
            throw Error("Dialogue width control exceeds its source");
        at += count;
    };
    auto lookup = [&](int index) -> const QByteArray & {
        const auto found = substitutions.constFind(index);
        if (found == substitutions.constEnd())
            throw Error(QString("Dialogue substitution byte %1 is unavailable").arg(index));
        return found.value();
    };
    auto copy = [&](const QByteArray &text, quint32 start, quint32 count) {
        if (count > 255)
            throw Error("Dialogue token exceeds the native scratch buffer");
        QByteArray result;
        auto c = read(text, start);
        while (c && count--) {
            result += char(c);
            c = read(text, ++start);
        }
        result += '\0';
        return result;
    };
    for (;;) {
        const auto c = next();
        if (!c) {
            maximum = std::max(quint16(maximum), quint16(width));
            return quint16(std::max(qint32(maximum), minimum));
        }
        if (lordly_text_lead_byte(c)) {
            skip(1);
            width += 11;
        } else if (c == '{') {
            quint32 end = at;
            while (read(source, end) && read(source, end) != '}')
                ++end;
            if (read(source, end) == '}') {
                const auto token = copy(source, at - 1, end - at + 2);
                width += measureText(token, metrics, budget);
                at = end + 1;
            }
        } else if (c != '@')
            width += metrics.glyphWidth(c);
        else {
            switch (const auto control = next(); control) {
            case 'n':
            case 'c':
            case '/':
                ++line;
                maximum = std::max(quint16(maximum), quint16(width));
                width = 0;
                if (control != 'n')
                    line = 0;
                break;
            case '%':
                width += measureText(lookup(next()), metrics, budget);
                break;
            case '&': {
                const auto &text = lookup(next());
                const auto length = quint8(next() - 48);
                width += measureText(copy(text, 0, length), metrics, budget);
                break;
            }
            case '!': {
                const auto &text = lookup(next());
                const auto length = scanSegment(text, columns, substitutions, budget).count;
                constexpr quint8 limited[] = {0, 1, 1, 2, 3};
                width +=
                    measureText(copy(text, 0, length < 5 ? limited[length] : 4), metrics, budget);
                break;
            }
            case 'z': {
                const auto &text = lookup(next());
                if (!text.isNull()) {
                    quint32 cursor = 0, last = 0, characters = 0;
                    for (;;) {
                        const auto ch = read(text, cursor);
                        if (!ch)
                            break;
                        last = cursor;
                        cursor += lordly_text_lead_byte(ch) ? 2 : 1;
                        ++characters;
                    }
                    if (quint16(characters) >= 2)
                        width += metrics.glyphWidth(read(text, last));
                }
                skip(1); // This grammar consumes another byte after the operand.
                break;
            }
            case ':': {
                const auto &text = lookup(next());
                quint32 length = 0;
                while (read(text, length))
                    ++length;
                if (length >= 6)
                    width += measureText(longNameSuffix, metrics, budget);
                break;
            }
            case 'i':
                width = quint16(width) + 15U;
                break;
            case 'p':
                // All original style/portrait branches reduce to this amount.
                if (quint16(line) == 2)
                    width = quint16(width) + 15U;
                break;
            case '<':
            case 'F':
            case 'T':
            case 'e':
            case 'f':
            case 't':
                skip(3);
                break;
            case 'w':
                skip(4);
                break;
            case 'l':
                skip(1);
                break;
            default:
                break;
            }
        }
    }
}
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
namespace {
using Measurements = std::pair<quint16, quint16>;
InitializedDialogue
initializeWindow(const QByteArray &window, qint32 resourceSlot, const QByteArray &source,
                 const std::function<Measurements(const QByteArray &)> &measure) {
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
    const auto [measuredWidth, measuredLines] = measure(state);
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
} // namespace
InitializedDialogue initializeDialogueWindow(const QByteArray &window, qint32 resourceSlot,
                                             const QByteArray &source, quint16 measuredWidth,
                                             quint16 measuredLines) {
    return initializeWindow(window, resourceSlot, source, [=](const QByteArray &) {
        return Measurements{measuredWidth, measuredLines};
    });
}
InitializedDialogue initializeDialogueWindow(const QByteArray &window, qint32 resourceSlot,
                                             const QByteArray &source,
                                             const DialogueTextEnvironment &environment) {
    return initializeWindow(window, resourceSlot, source, [&](const QByteArray &state) {
        quint16 width = 0, lines = 0;
        const auto style = u8(state, 0x47);
        auto columns = u16(state, 0x1a);
        if (style != 1) {
            if (!columns) {
                width = measureDialogueWidth(source, columns, environment.substitutions,
                                             environment.longNameSuffix, environment.metrics);
                columns = width / 7U + 1;
            }
            // The native line query runs before columns are clamped to 36.
            if (style != 0 && !u16(state, 0x1e))
                lines = countDialogueLines(source, columns, environment.substitutions,
                                           environment.longNameSuffix);
        }
        return Measurements{width, lines};
    });
}
} // namespace lordly
