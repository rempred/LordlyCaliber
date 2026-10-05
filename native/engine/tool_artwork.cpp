#include "engine/tool_artwork.h"
#include "engine/resource.h"
#include <QMap>
#include <QPoint>
#include <QSet>
#include <QVector>
#include <algorithm>
namespace lordly {
namespace {
using Grid = QVector<QByteArray>;
bool body(char value) {
    return value == 12 || value == 13;
}
Grid word(const QByteArray &pack, int offset, int stride, int y, int width, bool header) {
    Grid result(8, QByteArray(width, 0));
    for (int row = 0; row < 8; ++row)
        for (int x = 0; x < width; ++x) {
            const auto byte = u8(pack, offset + (row + y) * stride / 2 + x / 2);
            int index = (x & 1) ? (byte & 15) : (byte >> 4);
            // Header and War Funds exchange their cream/outline palette entries.
            if (header && (index == 12 || index == 13))
                index ^= 1;
            result[row][x] = char(index >= 12 && index <= 14 ? index : 0);
        }
    return result;
}
Grid letter(const Grid &word, int ordinal, int expectedLetters) {
    QVector<QPair<int, int>> spans;
    const int width = word[0].size();
    auto cream = [&](int x) {
        return std::any_of(word.begin(), word.end(), [x](const auto &row) { return row[x] == 13; });
    };
    for (int x = 0; x < width;) {
        if (!cream(x)) {
            ++x;
            continue;
        }
        const int start = x++;
        while (x < width && cream(x))
            ++x;
        spans.append({start, x});
    }
    if (spans.size() != expectedLetters || ordinal < 0 || ordinal >= spans.size())
        throw Error("The Army UI lettering no longer has the expected separate letter shapes");
    const auto [start, end] = spans[ordinal];
    if (start < 1 || end >= width)
        throw Error("The Army UI lettering is missing its outline margin");
    Grid result(8, QByteArray(end - start + 2, 0));
    for (int y = 0; y < 8; ++y)
        for (int x = start - 1; x <= end; ++x)
            if (body(word[y][x]))
                result[y][x - start + 1] = word[y][x];
    return result;
}
QPoint shadowOffset(const Grid &word) {
    QSet<QPoint> bodies, shadows;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < word[y].size(); ++x) {
            if (body(word[y][x]))
                bodies.insert({x, y});
            if (word[y][x] == 14)
                shadows.insert({x, y});
        }
    QPoint best(1, 1);
    double score = -1;
    for (int dx : {1, 2})
        for (int dy : {1, 2}) {
            QSet<QPoint> cast;
            for (const auto &point : bodies)
                cast.insert(point + QPoint(dx, dy));
            cast.subtract(bodies);
            if (cast.isEmpty())
                continue;
            const auto combined = cast | shadows;
            const double overlap = double((cast & shadows).size()) / combined.size();
            if (overlap > score) {
                score = overlap;
                best = {dx, dy};
            }
        }
    return best;
}
Grid compose(const QString &text, const QMap<QChar, Grid> &letters, QPoint shadow) {
    int width = 0;
    for (const auto c : text)
        width += letters[c][0].size() - 1;
    width = (width + 1 + shadow.x() + 1) & ~1;
    if (width > 46)
        throw Error("The composed Chaos Frame label exceeds its 48-pixel sheet");
    Grid result(8, QByteArray(width, 0));
    int left = 0;
    for (const auto c : text) {
        const auto &cell = letters[c];
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < cell[y].size(); ++x)
                if (cell[y][x] == 13 || (cell[y][x] == 12 && !result[y][left + x]))
                    result[y][left + x] = cell[y][x];
        left += cell[0].size() - 1;
    }
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < width; ++x) {
            const int sx = x + shadow.x(), sy = y + shadow.y();
            if (body(result[y][x]) && sx < width && sy < 8 && !result[sy][sx])
                result[sy][sx] = 14;
        }
    return result;
}
} // namespace
QByteArray chaosFrameTitle(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Chaos Frame artwork requires US revision zero");
    // Army UI loader: LUI a0 / JAL resource loader / ORI a0,a0 delay slot.
    // Retail key 01DC873E identifies the 23496-byte shared UI texture pack.
    const auto high = u32(rom.bytes(), 0xaed4c), low = u32(rom.bytes(), 0xaed54);
    if ((high >> 16) != 0x3c04 || (low >> 16) != 0x3484)
        throw Error("Army UI texture resource operands use an unsupported instruction layout");
    const quint32 key = ((high & 65535) << 16) | (low & 65535);
    if (!key || key >= 0x10000000 || (key & 1))
        throw Error("Invalid Army UI texture resource key");
    const qsizetype at = 0x594280 + key;
    const auto stored = u32(rom.bytes(), at), decoded = u32(rom.bytes(), at + 4);
    if (stored > 1024 * 1024 || decoded != 23496)
        throw Error("Army UI texture pack has an unsupported size");
    const auto pack = Resource::load(rom, at, qsizetype(stored) + 4).decoded;
    const auto war = word(pack, 0x2828, 48, 0, 23, false);
    const auto funds = word(pack, 0x2828, 48, 8, 34, false);
    const auto character = word(pack, 0x40f8, 64, 16, 58, true);
    const auto soldier = word(pack, 0x40f8, 64, 24, 43, true);
    QMap<QChar, Grid> letters;
    for (const auto &[c, index] : {std::pair{'C', 0}, {'H', 1}, {'A', 2}, {'R', 3}, {'E', 7}})
        letters[c] = letter(character, index, 9);
    letters['S'] = letter(soldier, 0, 7);
    letters['O'] = letter(soldier, 1, 7);
    letters['F'] = letter(funds, 0, 5);
    auto m = letter(war, 0, 3);
    int first = 8, last = -1;
    for (int y = 0; y < 8; ++y)
        if (std::any_of(m[y].begin(), m[y].end(), body)) {
            first = std::min(first, y);
            last = y;
        }
    if (last < first)
        throw Error("The Army UI W glyph is empty");
    std::reverse(m.begin() + first, m.begin() + last + 1);
    letters['M'] = m;
    const auto shadow = shadowOffset(character);
    const auto chaos = compose("CHAOS", letters, shadow), frame = compose("FRAME", letters, shadow);
    QByteArray result(384, 0);
    for (int row = 0; row < 16; ++row) {
        const auto &line = row < 8 ? chaos[row] : frame[row - 8];
        for (int x = 0; x < line.size(); ++x) {
            const int column = x + 2, pixel = row * 24 + column / 2;
            result[pixel] =
                char(quint8(result[pixel]) | (quint8(line[x]) << ((column & 1) ? 0 : 4)));
        }
    }
    return result;
}
} // namespace lordly
