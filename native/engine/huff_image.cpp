#include "engine/background_image.h"
#include "engine/huff_tables.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <numbers>
namespace lordly {
namespace {
class Bits {
  public:
    Bits(const QByteArray &data, qsizetype offset) : data_(data), bit_(offset * 8) {}
    int get(int count) {
        if (count < 0 || count > 16 || bit_ + count > data_.size() * 8)
            throw Error("Truncated HUFF entropy stream");
        int result = 0;
        for (int i = 0; i < count; ++i, ++bit_)
            result = result * 2 + ((u8(data_, bit_ / 8) >> (7 - bit_ % 8)) & 1);
        return result;
    }
    int signedBits(int size) {
        if (!size)
            return 0;
        const int value = get(size);
        return value >= (1 << (size - 1)) ? value : value - ((1 << size) - 1);
    }

  private:
    const QByteArray &data_;
    qsizetype bit_;
};
struct Huffman {
    std::array<int, 17> first{}, count{}, offset{};
    QVector<int> values;
    template <size_t N>
    Huffman(const std::array<int, 16> &bits, const std::array<int, N> &symbols) {
        int code = 0, index = 0;
        for (int length = 1; length <= 16; ++length) {
            first[length] = code;
            offset[length] = index;
            count[length] = bits[length - 1];
            code = (code + count[length]) * 2;
            index += count[length];
        }
        if (index != N)
            throw Error("Invalid HUFF table");
        for (int value : symbols)
            values << value;
    }
    int symbol(Bits &bits) const {
        int code = 0;
        for (int length = 1; length <= 16; ++length) {
            code = code * 2 + bits.get(1);
            if (code >= first[length] && code - first[length] < count[length])
                return values[offset[length] + code - first[length]];
        }
        throw Error("Invalid HUFF entropy code");
    }
};
const std::array<int, 64> zigzag{0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
                                 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
                                 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
                                 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
std::array<double, 64> inverse(const std::array<double, 64> &block) {
    static const auto basis = [] {
        std::array<double, 64> b{};
        for (int x = 0; x < 8; ++x)
            for (int u = 0; u < 8; ++u)
                b[x * 8 + u] = (u ? 1. : std::numbers::sqrt2 / 2.) *
                               std::cos((2 * x + 1) * u * std::numbers::pi / 16.);
        return b;
    }();
    std::array<double, 64> intermediate{}, output{};
    for (int v = 0; v < 8; ++v)
        for (int x = 0; x < 8; ++x)
            for (int u = 0; u < 8; ++u)
                intermediate[v * 8 + x] += block[v * 8 + u] * basis[x * 8 + u];
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            for (int v = 0; v < 8; ++v)
                output[y * 8 + x] += intermediate[v * 8 + x] * basis[y * 8 + v];
    // IDCT divide-by-four and the native RSP coefficient scale of four cancel.
    return output;
}
} // namespace
RasterImage decodeHuffImage(const QByteArray &source, bool embedded) {
    const int base = embedded ? 0 : 4;
    if (source.size() < base + 14 ||
        (!embedded && quint64(u32(source, 0)) + 4 != quint64(source.size())) ||
        u32(source, base) != 0x4855fe00 || source.mid(base + 8, 4) != "HUFF")
        throw Error("Invalid HUFF image container");
    const int width = u16(source, base + 4), height = u16(source, base + 6),
              blocks = u16(source, base + 12);
    if (!width || !height || width % 16 || height % 16 || width > 8192 || height > 8192 ||
        quint64(width) * height > 4194304 || blocks != width / 16 * (height / 16))
        throw Error("HUFF dimensions or macroblock count are invalid");
    static const Huffman dcY(huff::dcLuma_bits, huff::dcLuma_values),
        dcC(huff::dcChroma_bits, huff::dcChroma_values),
        acY(huff::acLuma_bits, huff::acLuma_values),
        acC(huff::acChroma_bits, huff::acChroma_values);
    Bits bits(source, base + 14);
    std::array<int, 3> dc{};
    QVector<double> yPlane(width * height, 0), cbPlane(width * height / 4, 0),
        crPlane(width * height / 4, 0);
    for (int macro = 0; macro < blocks; ++macro)
        for (int b = 0; b < 6; ++b) {
            const bool luma = b < 4;
            const int component = luma ? 0 : b - 3;
            const auto &dcTable = luma ? dcY : dcC, &acTable = luma ? acY : acC;
            std::array<double, 64> natural{};
            dc[component] += bits.signedBits(dcTable.symbol(bits));
            natural[0] = std::bit_cast<qint16>(quint16(dc[component]));
            int coefficient = 1;
            while (coefficient < 64) {
                const int symbol = acTable.symbol(bits);
                if (!symbol)
                    break;
                if (symbol == 0xf0) {
                    coefficient += 16;
                    if (coefficient > 64)
                        throw Error("HUFF zero run exceeds a block");
                    continue;
                }
                coefficient += symbol >> 4;
                if (coefficient >= 64)
                    throw Error("HUFF AC run exceeds a block");
                const int index = zigzag[coefficient++];
                natural[(index & 7) * 8 + (index >> 3)] = bits.signedBits(symbol & 15);
            }
            const auto pixels = inverse(natural);
            auto &plane = luma ? yPlane : b == 4 ? cbPlane : crPlane;
            const int stride = luma ? width : width / 2;
            const int mx = macro % (width / 16), my = macro / (width / 16);
            const int ox = luma ? mx * 16 + (b & 1) * 8 : mx * 8;
            const int oy = luma ? my * 16 + (b >> 1) * 8 : my * 8;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    plane[(oy + y) * stride + ox + x] = pixels[y * 8 + x];
        }
    RasterImage result{width, height, QByteArray(width * height * 4, 0)};
    auto channel = [](double value) {
        return quint8(std::clamp(std::floor(value + .5), 0., 255.));
    };
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x, chroma = (y / 2) * (width / 2) + x / 2;
            const double luminance = yPlane[pixel], cb = cbPlane[chroma] - 128.,
                         cr = crPlane[chroma] - 128.;
            put8(result.rgba, pixel * 4, channel(luminance + 1.402 * cr));
            put8(result.rgba, pixel * 4 + 1, channel(luminance - .344136 * cb - .714136 * cr));
            put8(result.rgba, pixel * 4 + 2, channel(luminance + 1.772 * cb));
            put8(result.rgba, pixel * 4 + 3, 255);
        }
    return result;
}
} // namespace lordly
