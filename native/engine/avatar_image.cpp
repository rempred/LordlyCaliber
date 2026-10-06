#include "engine/artwork_dither.h"
#include "engine/avatar_artwork.h"
#include <algorithm>
#include <limits>
namespace lordly {
QByteArray AvatarArtwork::mapImage(const QImage &input, bool reduce, QColor background,
                                   bool dither) {
    if (input.size() != QSize(40, 48) || !background.isValid() || background.alpha() != 255)
        throw Error("Portrait assignment needs 40 by 48 pixels and an opaque background");
    QMap<quint16, int> histogram;
    QByteArray result(3840, 0);
    auto channel = [](int value, int bg, int alpha) {
        return (31 * ((value * alpha + bg * (255 - alpha) + 127) / 255) + 127) / 255;
    };
    for (int y = 0; y < 48; ++y)
        for (int x = 0; x < 40; ++x) {
            const auto c = input.pixelColor(x, y);
            const auto a = c.alpha();
            const auto word = quint16(channel(c.red(), background.red(), a) << 11 |
                                      channel(c.green(), background.green(), a) << 6 |
                                      channel(c.blue(), background.blue(), a) << 1 | 1);
            put16(result, (y * 40 + x) * 2, word);
            ++histogram[word];
        }
    if (histogram.size() <= 80)
        return result;
    if (!reduce)
        throw Error("This edit exceeds 80 colors; use image assignment to reduce the palette");
    struct Color {
        quint16 word;
        int count;
    };
    using Box = QVector<Color>;
    Box all;
    for (auto it = histogram.cbegin(); it != histogram.cend(); ++it)
        all.append({it.key(), it.value()});
    QVector<Box> boxes{all};
    auto channel5 = [](quint16 word, int axis) {
        return int((word >> (axis == 0 ? 11 : axis == 1 ? 6 : 1)) & 31);
    };
    auto score = [&](const Box &box, int axis) {
        quint64 weight = 0, sum = 0, square = 0;
        for (const auto &c : box) {
            const auto v = channel5(c.word, axis);
            weight += c.count;
            sum += v * c.count;
            square += v * v * c.count;
        }
        return (square * weight - sum * sum) * (axis == 0 ? 77 : axis == 1 ? 150 : 29) / weight;
    };
    while (boxes.size() < 80) {
        int selected = -1, axis = 0;
        quint64 best = 0;
        for (int i = 0; i < boxes.size(); ++i)
            if (boxes[i].size() > 1)
                for (int a = 0; a < 3; ++a)
                    if (const auto value = score(boxes[i], a); value > best) {
                        selected = i;
                        axis = a;
                        best = value;
                    }
        if (selected < 0)
            break;
        auto box = boxes[selected];
        std::sort(box.begin(), box.end(), [&](const Color &a, const Color &b) {
            const auto x = channel5(a.word, axis), y = channel5(b.word, axis);
            return x != y ? x < y : a.word < b.word;
        });
        int total = 0;
        for (const auto &c : box)
            total += c.count;
        int cumulative = 0, split = 0;
        do {
            cumulative += box[split++].count;
        } while (split < box.size() - 1 && cumulative * 2 < total);
        boxes[selected] = box.first(split);
        boxes.append(box.sliced(split));
    }
    QVector<quint16> palette;
    for (const auto &box : boxes) {
        int total = 0, sums[3] = {};
        for (const auto &c : box) {
            total += c.count;
            for (int a = 0; a < 3; ++a)
                sums[a] += channel5(c.word, a) * c.count;
        }
        palette.append(quint16(((sums[0] + total / 2) / total) << 11 |
                               ((sums[1] + total / 2) / total) << 6 |
                               ((sums[2] + total / 2) / total) << 1 | 1));
    }
    if (dither) {
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 40; ++x) {
                const auto source = u16(result, (y * 40 + x) * 2);
                double best = std::numeric_limits<double>::max();
                quint16 chosen = 1;
                for (const auto color : palette) {
                    double distance = 0;
                    for (int a = 0; a < 3; ++a) {
                        const auto delta =
                            channel5(source, a) + orderedColorOffset(x, y) - channel5(color, a);
                        distance += delta * delta * (a == 0 ? 77 : a == 1 ? 150 : 29);
                    }
                    if (distance < best) {
                        best = distance;
                        chosen = color;
                    }
                }
                put16(result, (y * 40 + x) * 2, chosen);
            }
        return result;
    }
    QMap<quint16, quint16> mapped;
    for (auto it = histogram.cbegin(); it != histogram.cend(); ++it) {
        int best = std::numeric_limits<int>::max();
        quint16 chosen = 1;
        for (const auto color : palette) {
            int distance = 0;
            for (int a = 0; a < 3; ++a) {
                const auto delta = channel5(it.key(), a) - channel5(color, a);
                distance += delta * delta * (a == 0 ? 77 : a == 1 ? 150 : 29);
            }
            if (distance < best) {
                best = distance;
                chosen = color;
            }
        }
        mapped[it.key()] = chosen;
    }
    for (int i = 0; i < 1920; ++i)
        put16(result, i * 2, mapped[u16(result, i * 2)]);
    return result;
}
} // namespace lordly
