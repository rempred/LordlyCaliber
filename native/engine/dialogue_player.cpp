#include "engine/dialogue_player.h"
#include "game/director/text_metrics.h"
#include <algorithm>
namespace lordly {
quint8 revealDialogueText(DialogueTextFrame &frame, const QByteArray &source,
                          const DialogueTextEnvironment &environment, quint16 speed, int budget) {
    if (frame.window.size() != 168 || frame.state.size() != 1144 || budget < 1)
        throw Error("Dialogue reveal requires complete records and a positive budget");
    if (u32(frame.state, 0xc) != (frame.insertion ? 1U : 0U))
        throw Error("Dialogue insertion reference does not match its payload");
    auto nextFrame = frame;
    auto &state = nextFrame.state;
    auto &window = nextFrame.window;
    nextFrame.children.clear();
    nextFrame.sounds.clear();
    auto spend = [&] {
        if (--budget < 0)
            throw Error("Dialogue reveal exceeded its budget");
    };
    auto read = [&](const QByteArray &text, quint32 cursor) {
        spend();
        return u8(text, cursor);
    };
    auto next = [&] {
        const auto cursor = u32(state, 8);
        const auto c = read(source, cursor);
        put32(state, 8, cursor + 1);
        return c;
    };
    auto number = [&] {
        // The dialogue numeric helper always consumes exactly three raw bytes.
        const int a = int(next()) - 48, b = int(next()) - 48, c = int(next()) - 48;
        return quint16(a * 100 + b * 10 + c);
    };
    auto lookup = [&](int key) -> const QByteArray & {
        if (key == -1)
            return environment.longNameSuffix;
        const auto found = environment.substitutions.constFind(key);
        if (found == environment.substitutions.constEnd())
            throw Error(QString("Dialogue substitution byte %1 is unavailable").arg(key));
        return found.value();
    };
    auto clearInsertion = [&] {
        nextFrame.insertion.reset();
        put32(state, 0xc, 0);
    };
    auto insert = [&](int key, quint8 limit, quint32 cursor = 0) {
        const auto &text = lookup(key);
        if (text.isNull() || !limit)
            clearInsertion();
        else {
            nextFrame.insertion = DialogueInsertion{key, cursor};
            put32(state, 0xc, 1);
        }
        put8(state, 0x4c, limit);
        put8(state, 0x4d, 0);
    };
    auto append = [&](quint8 c) {
        const auto used = u16(state, 0x34);
        if (used >= 767)
            throw Error("Dialogue output exceeds its terminated 768-byte buffer");
        put8(state, 0x178 + used, c);
        put16(state, 0x34, used + 1);
    };
    auto line = [&](quint16 initial) {
        if (initial == 1)
            return;
        const auto column = quint16(u16(state, 0x18) + 1);
        put16(state, 0x18, column);
        if (column <= u16(state, 0x1a))
            return;
        append(10);
        const auto row = quint8(u8(state, 0x55) + 1);
        if (row >= 96)
            throw Error("Dialogue line index exceeds its 96-entry table");
        put8(state, 0x55, row);
        put16(state, 0xb8 + row * 2, u16(state, 0x34));
        put16(state, 0x18, initial);
        put16(state, 0x1c, u16(state, 0x1c) + 1);
    };
    auto noCost = [&] { put16(state, 0x10, u16(state, 0x10) + 256); };
    auto stop = [&] { put16(state, 0x10, 256); };
    auto sound = [&] {
        if (!(u8(state, 0x48) & 1))
            nextFrame.sounds.push_back({0x1a, 10});
    };
    put16(state, 0x10, u16(state, 0x10) + speed);
    while (u16(state, 0x10) > 256) {
        spend();
        if (nextFrame.insertion &&
            !read(lookup(nextFrame.insertion->key), nextFrame.insertion->cursor))
            clearInsertion();
        if (nextFrame.insertion) {
            auto &insertion = *nextFrame.insertion;
            const auto &text = lookup(insertion.key);
            const auto c = read(text, insertion.cursor++);
            line(1);
            append(c);
            if (lordly_text_lead_byte(c))
                append(read(text, insertion.cursor++)); // Accepted eacfc is a U16 identity.
            sound();
            put8(state, 0x4d, u8(state, 0x4d) + 1);
            if (u8(state, 0x4d) >= u8(state, 0x4c))
                clearInsertion();
        } else {
            const auto c = read(source, u32(state, 8));
            if (!c) {
                put16(window, 0, u16(window, 0) | 0x1000);
                put8(state, 0x3c, 99);
                stop();
            } else if (lordly_text_lead_byte(c)) {
                line(1);
                append(next());
                append(next());
                sound();
            } else if (c == '{') {
                line(1);
                do {
                    append(next());
                } while (read(source, u32(state, 8)) != '}');
                append(next());
            } else if (c != '@') {
                line(1);
                append(next());
                sound();
            } else {
                next();
                switch (const auto control = next(); control) {
                case 'c':
                case '/':
                case 'n':
                    put16(state, 0x18, 99);
                    line(0);
                    if (control == 'n')
                        noCost();
                    else {
                        if (control == 'c') {
                            put8(state, 0x54, u8(state, 0x55));
                            put8(state, 0x53, u8(state, 0x55));
                            put16(state, 0x36, u16(state, 0x34));
                        } else {
                            put8(state, 0x55, 0);
                            put8(state, 0x54, 0);
                            put8(state, 0x53, 0);
                            put16(state, 0x34, 0);
                            put16(state, 0x36, 0);
                            std::fill(state.begin() + 0x57, state.begin() + 0xb7, 0);
                            std::fill(state.begin() + 0xb8, state.end(), 0);
                        }
                        put16(state, 0x18, 0);
                        put16(state, 0x1c, 0);
                        stop();
                    }
                    break;
                case 'a':
                    put8(state, 0x3c, 3);
                    stop();
                    break;
                case 'p':
                case 'i':
                    put8(state, 0x42,
                         control == 'p'                            ? 1
                         : u16(state, 0x18) + 1 > u16(state, 0x1a) ? 3
                                                                   : 2);
                    put8(state, 0x43, 0);
                    put8(state, 0x3c, 6);
                    stop();
                    break;
                case 'e':
                    nextFrame.children.push_back(
                        {qint16(u16(window, 6)), qint16(u16(window, 8)), number()});
                    break;
                case 'f':
                case 'F':
                    put8(state, 0x4a, control == 'F');
                    put16(state, 0x16, number());
                    if (u16(state, 0x16) != u16(state, 0x14))
                        put8(state, 0x3c, 9);
                    stop();
                    break;
                case 's':
                    put8(window, 0x8a, u8(window, 0x8a) | 1);
                    put8(state, 0x3c, 4);
                    stop();
                    break;
                case 'k':
                    put8(state, 0x3f, 0);
                    put8(state, 0x41, 1);
                    put8(state, 0x3c, 5);
                    stop();
                    break;
                case 'm':
                    put8(window, 0x8a, u8(window, 0x8a) | 0x10);
                    noCost();
                    break;
                case 'g':
                    put8(window, 0x8a, u8(window, 0x8a) ^ 0x20);
                    noCost();
                    break;
                case '1': {
                    put8(state, 0x3c, 7);
                    put8(state, 0x44, 1);
                    const auto choices = countDialogueChoices(source, u32(state, 8), budget);
                    put8(state, 0x45, choices);
                    put8(state, 0x46, u8(state, 0x1d) - quint8(choices));
                    stop();
                    break;
                }
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                    auto cursor = u32(state, 8);
                    seekDialogueChoice(source, cursor, '#', budget);
                    put32(state, 8, cursor);
                    noCost();
                    break;
                }
                case 'w': {
                    const int index = int(next()) - 48;
                    if (index < 0 || index >= 8)
                        throw Error("Dialogue control index exceeds its eight-byte table");
                    nextFrame.controls[index] = quint8(number());
                    noCost();
                    break;
                }
                case 't':
                case 'T': {
                    auto delay = number();
                    if (!delay)
                        delay = 1;
                    else if (control == 't') {
                        if (!speed)
                            throw Error("Dialogue wait divides by zero text speed");
                        delay = quint16((quint32(delay) << 9) / speed);
                    }
                    put16(state, 0x12, delay);
                    put8(state, 0x3c, control == 't' ? 8 : 11);
                    stop();
                    break;
                }
                case '%':
                    insert(next(), 99);
                    noCost();
                    break;
                case '&': {
                    const auto key = next();
                    insert(key, quint8(next() - 48));
                    noCost();
                    break;
                }
                case '!': {
                    const auto key = next();
                    const auto count = scanDialogueSegment(lookup(key), u16(state, 0x1a),
                                                           environment.substitutions, budget)
                                           .count;
                    constexpr quint8 limits[] = {0, 1, 1, 2, 3};
                    insert(key, count < 5 ? limits[count] : 4);
                    noCost();
                    break;
                }
                case 'z': {
                    const auto key = next();
                    const auto &text = lookup(key);
                    quint32 at = 0, last = 0, count = 0;
                    if (!text.isNull())
                        while (const auto ch = read(text, at)) {
                            last = at;
                            at += lordly_text_lead_byte(ch) ? 2 : 1;
                            ++count;
                        }
                    if (quint16(count) >= 2)
                        insert(key, 99, last);
                    else {
                        clearInsertion();
                        put8(state, 0x4c, 99);
                        put8(state, 0x4d, 0);
                    }
                    noCost();
                    break;
                }
                case ':':
                    if (scanDialogueSegment(lookup(next()), u16(state, 0x1a),
                                            environment.substitutions, budget)
                            .count >= 6)
                        insert(-1, 99);
                    noCost();
                    break;
                case '-':
                    put8(window, 0x91, u8(window, 0x91) | 4);
                    noCost();
                    break;
                case 'l':
                    append('{');
                    append('C');
                    append(next());
                    append('}');
                    noCost();
                    break;
                case '=': {
                    const auto count =
                        scanDialogueSegment(source.mid(u32(state, 8)), u16(state, 0x1a),
                                            environment.substitutions, budget)
                            .count;
                    const int spacing = std::max((int(u16(state, 0x1a)) - count) * 7 / 2, 0);
                    const auto token =
                        QString("{X%1}").arg(spacing, 3, 10, QLatin1Char('0')).toLatin1();
                    const auto used = u16(state, 0x34);
                    if (used + token.size() >= 768)
                        throw Error("Centered dialogue exceeds its output buffer");
                    for (int i = 0; i < token.size(); ++i)
                        put8(state, 0x178 + used + i, quint8(token[i]));
                    put8(state, 0x178 + used + token.size(), 0);
                    put16(state, 0x34, used + 6);
                    noCost();
                    break;
                }
                case 'R':
                    put8(state, 0x4e, 0);
                    noCost();
                    break;
                case 'r':
                    put8(state, 0x4e, 1);
                    noCost();
                    break;
                case '<':
                    put8(window, 4, quint8(number()));
                    noCost();
                    break;
                case '#':
                    noCost();
                    break;
                default:
                    break;
                }
            }
        }
        put16(state, 0x10, u16(state, 0x10) - 256);
    }
    put8(state, 0x178 + u16(state, 0x34), 0);
    const auto result = u8(state, 0x3c);
    frame = std::move(nextFrame);
    return result;
}
quint8 advanceDialogueWindow(DialogueTextFrame &frame, const QByteArray &source,
                             const DialogueTextEnvironment &environment, DialogueInput input,
                             const QByteArray &shakePattern, int budget) {
    if (frame.window.size() != 168 || frame.state.size() != 1144 || budget < 1)
        throw Error("Dialogue update requires complete records and a positive budget");
    auto nextFrame = frame;
    auto &state = nextFrame.state;
    auto &window = nextFrame.window;
    nextFrame.children.clear();
    nextFrame.sounds.clear();
    auto historyMarker = [&](quint8 value) {
        const int row = u8(state, 0x54) + u16(state, 0x1e);
        if (row >= 96)
            throw Error("Dialogue history row exceeds its line table");
        const int offset = u16(state, 0xb8 + row * 2);
        if (offset > 768)
            throw Error("Dialogue history marker exceeds its text buffer");
        put8(state, 0x177 + offset, value);
    };
    auto selectionMarker = [&](quint8 value) {
        const int row = u8(state, 0x53) + u8(state, 0x44) + u8(state, 0x46);
        if (row >= 96)
            throw Error("Dialogue selection exceeds its line flags");
        put8(state, 0x57 + row, value);
    };
    auto audio = [&](quint16 sound, quint8 volume) { nextFrame.sounds.push_back({sound, volume}); };
    auto shake = [&] {
        if (!u8(state, 0x41))
            return;
        const auto index = u8(state, 0x3f);
        const auto value = u8(shakePattern, index);
        put8(state, 0x3f, index + 1);
        put8(state, 0x40, value);
        if (value == 0x80) {
            put8(state, 0x41, 0);
            put8(state, 0x40, 0);
        }
    };
    auto cancel = [&] {
        if ((u8(window, 0x91) & 1) && (input.pressed & 0x4000)) {
            put16(window, 0, u16(window, 0) | 0x1000);
            put8(state, 0x3c, 99);
            put8(window, 0x91, u8(window, 0x91) | 2);
        }
    };
    put8(state, 0x40, 0);
    // ROM E6B14 history callback always precedes the state update.
    put8(state, 0x50, 0);
    if (u8(state, 0x4f))
        historyMarker(10);
    const auto phase = u8(state, 0x3c);
    const bool historyInput = (phase == 3 || phase == 6 || (u8(state, 0x52) && phase == 7)) &&
                              (input.history & 16) && u8(state, 0x53) && !u8(state, 0x4e);
    if (historyInput) {
        put8(state, 0x4f, 128);
        if (input.repeat & 0x800) {
            if (u8(state, 0x54)) {
                put8(state, 0x54, u8(state, 0x54) - 1);
                audio(2, 20);
            }
        } else if ((input.repeat & 0x400) && u8(state, 0x54) < u8(state, 0x53)) {
            put8(state, 0x54, u8(state, 0x54) + 1);
            audio(2, 20);
        }
        if (u8(state, 0x44))
            selectionMarker(1);
        if (u8(state, 0x54))
            put8(state, 0x50, u8(state, 0x50) | 1);
        if (u8(state, 0x54) < u8(state, 0x53))
            put8(state, 0x50, u8(state, 0x50) | 2);
    } else if (u8(state, 0x54) < u8(state, 0x53)) {
        put8(state, 0x4f, u8(state, 0x4f) ^ 1);
        if (!(u8(state, 0x4f) & 1)) {
            put8(state, 0x54, u8(state, 0x54) + 1);
            audio(2, 20);
        }
    } else {
        put8(state, 0x4f, 0);
        if (u8(state, 0x44))
            selectionMarker(0);
    }
    if (u8(state, 0x4f))
        historyMarker(0);
    else {
        switch (phase) {
        case 0:
            put8(state, 0x3c, 1);
            break;
        case 1:
            advanceDialoguePosition(state);
            if (!(u16(window, 0) & 0x800)) {
                shake();
                if (u8(state, 0x3e) && !u8(state, 0x41))
                    put8(state, 0x3c, 2);
            }
            break;
        case 2:
            for (;;) {
                if (--budget < 0)
                    throw Error("Dialogue fast-forward exceeded its budget");
                const auto previous = u8(state, 0x3c);
                const auto sounds = nextFrame.sounds;
                const auto children = nextFrame.children;
                const auto result =
                    revealDialogueText(nextFrame, source, environment, input.speed, budget);
                nextFrame.sounds = sounds + nextFrame.sounds;
                nextFrame.children = children + nextFrame.children;
                if (result != previous || u8(state, 0x51) || !(input.pressed & 0x8000))
                    break;
            }
            break;
        case 3:
            if (input.pressed & 0x8000)
                put8(state, 0x3c, 2);
            else
                cancel();
            break;
        case 4:
            if (u8(window, 0x8a) & 2) {
                put8(window, 0x8a, u8(window, 0x8a) & 0xfc);
                put8(state, 0x3c, 2);
            }
            break;
        case 5:
            shake();
            if (!u8(state, 0x41))
                put8(state, 0x3c, 2);
            break;
        case 6:
            put8(state, 0x43, u8(state, 0x43) + 16);
            if (input.pressed & 0x8000) {
                put8(state, 0x3c, 2);
                put8(state, 0x42, 0);
            } else
                cancel();
            break;
        case 7:
            if (input.repeat & 0x800) {
                if (u8(state, 0x44) >= 2) {
                    put8(state, 0x44, u8(state, 0x44) - 1);
                    audio(6, 30);
                }
            } else if (input.repeat & 0x400) {
                if (u8(state, 0x44) < u8(state, 0x45)) {
                    put8(state, 0x44, u8(state, 0x44) + 1);
                    audio(6, 30);
                }
            } else if (input.pressed & 0x8000) {
                if (u8(state, 0x44) >= 2) {
                    auto cursor = u32(state, 8);
                    seekDialogueChoice(source, cursor, u8(state, 0x44) + 48, budget);
                    put32(state, 8, cursor);
                }
                selectionMarker(1);
                put8(state, 0x44, 0);
                put8(state, 0x3c, 2);
                audio(0, 50);
            } else
                cancel();
            break;
        case 8:
            put16(state, 0x12, u16(state, 0x12) - 1);
            if (!u16(state, 0x12) || (!u8(state, 0x51) && (input.pressed & 0x8000)))
                put8(state, 0x3c, 2);
            break;
        case 9: {
            const int alpha = u8(state, 0x49) - 50;
            if (alpha >= 0)
                put8(state, 0x49, alpha);
            else {
                put8(state, 0x49, 0);
                put8(state, 0x3c, phase + 1);
                put16(state, 0x14, u16(state, 0x16));
            }
            break;
        }
        case 10: {
            const int alpha = u8(state, 0x49) + 50;
            put8(state, 0x49, std::min(alpha, 255));
            if (alpha >= 255)
                put8(state, 0x3c, 2);
            break;
        }
        case 11:
            if (input.pressed & 0x8000)
                put8(state, 0x3c, 2);
            else {
                put16(state, 0x12, u16(state, 0x12) - 1);
                if (!u16(state, 0x12))
                    put8(state, 0x3c, 2);
            }
            break;
        case 99:
            put8(state, 0x3e, 0);
            put16(state, 0x28, u16(state, 0x2c));
            put16(state, 0x2a, u16(state, 0x2e));
            put8(state, 0x3c, phase + 1);
            break;
        default:
            if (!u8(state, 0x3e))
                advanceDialoguePosition(state);
            break;
        }
        if (u8(window, 0x8a) & 0x10)
            trackDialoguePointer(window, state);
        positionDialogueBounds(window, state);
    }
    const auto result = u8(state, 0x3c);
    frame = std::move(nextFrame);
    return result;
}
} // namespace lordly
