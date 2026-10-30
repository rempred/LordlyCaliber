#include "engine/effect_raster.h"
#include "game/director/scene_matrix.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace lordly {
namespace {
using Matrix = std::array<float, 16>;
struct Vertex {
    double x = 0, y = 0, z = 0, inverseW = 1, s = 0, t = 0;
    std::array<double, 4> color{255, 255, 255, 255};
    bool valid = false;
};
struct ImageSource {
    quint32 address = 0;
    int format = 0, size = 0, width = 1, rowBytes = 0;
};
struct Tile {
    bool configured = false;
    int format = 0, size = 0, line = 0, tmem = 0, palette = 0;
    int maskS = 0, maskT = 0, shiftS = 0, shiftT = 0;
    bool clampS = false, clampT = false, mirrorS = false, mirrorT = false;
    int left = 0, top = 0, width = 1, height = 1;
};
class Raster {
  public:
    Raster(const QByteArray &bytes, QSize size, double scale)
        : ram(bytes), viewport(size), zoom(scale) {
        if (ram.isEmpty() || ram.size() > 32 * 1024 * 1024 || size.width() < 1 ||
            size.height() < 1 || size.width() > 2048 || size.height() > 2048 ||
            !std::isfinite(scale) || scale <= 0 || scale > 16)
            throw Error("Effect drawing input exceeds its preview budget");
        result.image = QImage(size, QImage::Format_RGBA8888);
        result.image.fill(Qt::transparent);
        depth.resize(size.width() * size.height(), std::numeric_limits<double>::infinity());
        lordly_matrix_identity(model.data());
        lordly_matrix_identity(projection.data());
        palette.fill({255, 255, 255, 255});
    }
    EffectRaster::Result run(quint32 begin, quint32 end) {
        if (end < begin || end - begin > 1024 * 1024 || (end - begin) % 8)
            throw Error("Invalid effect drawing-list extent");
        const auto start = physical(begin, end - begin);
        list(start, start + (end - begin), 0);
        return result;
    }

  private:
    const QByteArray &ram;
    QSize viewport;
    double zoom;
    EffectRaster::Result result;
    std::vector<double> depth;
    std::array<quint32, 16> segments{};
    std::array<Vertex, 80> vertices{};
    std::array<Tile, 8> tiles{};
    std::array<ImageSource, 512> textures{};
    std::array<RenderColor, 256> palette{};
    Matrix model{}, projection{};
    QVector<Matrix> stack;
    bool hasProjection = false, twoCycles = false, textureEnabled = true;
    bool copyCycle = false;
    bool zBuffer = false;
    quint32 lowerMode = 0;
    double primitiveDepth = 0;
    double viewportScaleX = 1, viewportScaleY = 1, viewportOffsetX = 0, viewportOffsetY = 0;
    int activeTile = 0;
    double scaleS = 1, scaleT = 1;
    quint32 combineHigh = 0xfcffffff, combineLow = 0xfffdf6fb;
    RenderColor primitive{255, 255, 255, 255}, environment{};
    ImageSource image;
    quint32 physical(quint32 address, quint32 size = 1) const {
        quint32 resolved = address;
        if (address >> 28 == 0 && (address >> 24))
            resolved = segments[(address >> 24) & 15] + (address & 0xffffff);
        resolved &= 0x1fffffff;
        if (quint64(resolved) + size > quint64(ram.size()))
            throw Error("Effect drawing references memory outside the preview");
        return resolved;
    }
    quint8 byte(quint32 a) const {
        return u8(ram, physical(a));
    }
    quint16 half(quint32 a) const {
        return u16(ram, physical(a, 2));
    }
    quint32 word(quint32 a) const {
        return u32(ram, physical(a, 4));
    }
    static RenderColor rgba16(quint16 color) {
        auto channel = [](int n) { return quint8((n << 3) | (n >> 2)); };
        return {channel(color >> 11), channel((color >> 6) & 31), channel((color >> 1) & 31),
                quint8(color & 1 ? 255 : 0)};
    }
    static RenderColor rgba32(quint32 color) {
        return {quint8(color >> 24), quint8(color >> 16), quint8(color >> 8), quint8(color)};
    }
    static int coordinate(double value, int extent, int mask, bool clamp, bool mirror, int shift) {
        value = shift <= 10 ? value / double(1 << shift) : value * double(1 << (16 - shift));
        if (!std::isfinite(value) || std::abs(value) > 1e8)
            throw Error("Invalid effect texture coordinate");
        int at = int(std::floor(value));
        if (clamp)
            return std::clamp(at, 0, extent - 1);
        const int period = mask ? (1 << mask) : extent;
        int wrapped = ((at % period) + period) % period;
        if (mirror && (int(std::floor(double(at) / period)) & 1))
            wrapped = period - 1 - wrapped;
        return wrapped % extent;
    }
    RenderColor sample(int index, double s, double t) const {
        const auto &tile = tiles[index & 7];
        if (tile.width < 1 || tile.height < 1 || tile.width > 1024 || tile.height > 1024)
            throw Error("Invalid effect texture tile dimensions");
        const auto &source = textures[tile.tmem];
        if (!textureEnabled || !tile.configured || !source.address)
            return {255, 255, 255, 255};
        if (tile.width > 4096 || tile.height > 4096 || tile.width < 1 || tile.height < 1)
            throw Error("Effect texture exceeds its dimensions budget");
        const int x = coordinate(s - tile.left / 4., tile.width, tile.maskS, tile.clampS,
                                 tile.mirrorS, tile.shiftS);
        const int y = coordinate(t - tile.top / 4., tile.height, tile.maskT, tile.clampT,
                                 tile.mirrorT, tile.shiftT);
        const int bits = 4 << tile.size;
        const int rowBytes = source.rowBytes ? source.rowBytes
                             : tile.line     ? tile.line * 8 * (tile.size == 3 ? 2 : 1)
                                             : (tile.width * bits + 7) / 8;
        const quint32 at = source.address + y * rowBytes + (x * bits) / 8;
        if (tile.format == 0 && tile.size == 2)
            return rgba16(half(at));
        if (tile.format == 0 && tile.size == 3)
            return rgba32(word(at));
        if (tile.format == 2 && (tile.size == 0 || tile.size == 1)) {
            const auto value = byte(at);
            return palette[tile.size ? value
                                     : tile.palette * 16 + (x & 1 ? value & 15 : value >> 4)];
        }
        if (tile.format == 3) {
            if (tile.size == 2) {
                const auto v = half(at);
                return {quint8(v >> 8), quint8(v >> 8), quint8(v >> 8), quint8(v)};
            }
            const auto v = byte(at);
            if (tile.size == 1)
                return {quint8((v >> 4) * 17), quint8((v >> 4) * 17), quint8((v >> 4) * 17),
                        quint8((v & 15) * 17)};
            if (tile.size == 0) {
                const int n = x & 1 ? v & 15 : v >> 4;
                const auto i = quint8((n >> 1) * 255 / 7);
                return {i, i, i, quint8(n & 1 ? 255 : 0)};
            }
        }
        if (tile.format == 4 && tile.size <= 1) {
            const auto v = byte(at);
            const auto i = tile.size ? v : quint8((x & 1 ? v & 15 : v >> 4) * 17);
            return {i, i, i, i};
        }
        throw Error(
            QString("Unsupported effect texture format %1/%2").arg(tile.format).arg(tile.size));
    }
    void rememberTexture(int index) {
        const auto &tile = tiles[index & 7];
        const auto &source = textures[tile.tmem];
        if (!textureEnabled || !tile.configured || !source.address) return;
        const int bits = 4 << tile.size;
        const int rowBytes = source.rowBytes ? source.rowBytes : tile.line ? tile.line * 8 * (tile.size == 3 ? 2 : 1)
                                                                                      : (tile.width * bits + 7) / 8;
        EffectRaster::TextureUse use{physical(source.address), tile.format, bits, rowBytes};
        if (!result.textures.contains(use)) result.textures.append(use);
    }
    void vertex(quint32 address, int first, int count) {
        if (first < 0 || count < 1 || first + count > int(vertices.size()))
            throw Error("Effect vertex command exceeds its cache");
        physical(address, count * 16);
        for (int i = 0; i < count; ++i) {
            const auto at = address + i * 16;
            const double x = qint16(half(at)), y = qint16(half(at + 2)), z = qint16(half(at + 4));
            std::array<double, 4> p{};
            for (int c = 0; c < 4; ++c)
                p[c] = x * model[c] + y * model[4 + c] + z * model[8 + c] + model[12 + c];
            auto &v = vertices[first + i];
            if (hasProjection) {
                std::array<double, 4> q{};
                for (int c = 0; c < 4; ++c)
                    for (int k = 0; k < 4; ++k)
                        q[c] += p[k] * projection[k * 4 + c];
                if (std::abs(q[3]) < 1e-8) {
                    v.valid = false;
                    continue;
                }
                v.inverseW = 1. / q[3];
                v.x = (q[0] * v.inverseW + 1) * viewport.width() / 2.;
                v.y = (1 - q[1] * v.inverseW) * viewport.height() / 2.;
                v.z = q[2] * v.inverseW;
            } else {
                v.inverseW = 1;
                v.x = viewport.width() / 2. + p[0] * zoom;
                v.y = viewport.height() * .68 - p[1] * zoom + p[2] * zoom * .2;
                v.z = p[2];
            }
            v.x = (v.x - viewport.width() / 2.) * viewportScaleX + viewport.width() / 2. + viewportOffsetX;
            v.y = (v.y - viewport.height() / 2.) * viewportScaleY + viewport.height() / 2. + viewportOffsetY;
            v.s = qint16(half(at + 8)) / 32. * scaleS;
            v.t = qint16(half(at + 10)) / 32. * scaleT;
            for (int c = 0; c < 4; ++c)
                v.color[c] = byte(at + 12 + c);
            v.valid = std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }
    }
    void triangle(int a, int b, int c) {
        rememberTexture(activeTile);
        if (twoCycles) rememberTexture((activeTile + 1) & 7);
        if (a >= int(vertices.size()) || b >= int(vertices.size()) || c >= int(vertices.size()))
            throw Error("Effect triangle references an invalid vertex");
        const auto &p = vertices[a], &q = vertices[b], &r = vertices[c];
        if (!p.valid || !q.valid || !r.valid)
            return;
        const double area = (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
        if (std::abs(area) < 1e-8)
            return;
        const int left =
            int(std::clamp(std::floor(std::min({p.x, q.x, r.x})), 0., double(viewport.width())));
        const int right =
            int(std::clamp(std::ceil(std::max({p.x, q.x, r.x})), 0., double(viewport.width())));
        const int top =
            int(std::clamp(std::floor(std::min({p.y, q.y, r.y})), 0., double(viewport.height())));
        const int bottom =
            int(std::clamp(std::ceil(std::max({p.y, q.y, r.y})), 0., double(viewport.height())));
        ColorCombiner combiner(combineHigh, combineLow, twoCycles);
        for (int y = top; y < bottom; ++y)
            for (int x = left; x < right; ++x) {
                const double u =
                    ((q.x - x - .5) * (r.y - y - .5) - (q.y - y - .5) * (r.x - x - .5)) / area;
                const double v =
                    ((r.x - x - .5) * (p.y - y - .5) - (r.y - y - .5) * (p.x - x - .5)) / area;
                const double w = 1 - u - v;
                if (u < 0 || v < 0 || w < 0)
                    continue;
                const double z = lowerMode & 4 ? primitiveDepth : u * p.z + v * q.z + w * r.z;
                const int at = y * viewport.width() + x;
                if (zBuffer && z > depth[at])
                    continue;
                const double reciprocal = u * p.inverseW + v * q.inverseW + w * r.inverseW;
                if (std::abs(reciprocal) < 1e-10)
                    continue;
                const auto interpolate = [&](double x0, double x1, double x2) {
                    return (u * x0 * p.inverseW + v * x1 * q.inverseW + w * x2 * r.inverseW) /
                           reciprocal;
                };
                const double s = interpolate(p.s, q.s, r.s), t = interpolate(p.t, q.t, r.t);
                CombineInputs inputs{sample(activeTile, s, t),
                                     sample((activeTile + 1) & 7, s, t),
                                     primitive,
                                     environment,
                                     {}};
                for (int k = 0; k < 4; ++k)
                    inputs.shade[k] = quint8(
                        std::clamp(interpolate(p.color[k], q.color[k], r.color[k]), 0., 255.));
                const auto color = combiner.evaluate(inputs);
                auto *pixel = result.image.scanLine(y) + x * 4;
                const double alpha = color[3] / 255., oldAlpha = pixel[3] / 255.,
                             outAlpha = alpha + oldAlpha * (1 - alpha);
                if (outAlpha > 0)
                    for (int k = 0; k < 3; ++k)
                        pixel[k] = quint8(std::clamp(
                            (color[k] * alpha + pixel[k] * oldAlpha * (1 - alpha)) / outAlpha, 0.,
                            255.));
                pixel[3] = quint8(outAlpha * 255);
                if (zBuffer && color[3])
                    depth[at] = z;
            }
        ++result.triangles;
    }
    void list(quint32 begin, quint32 end, int nesting) {
        if (nesting > 32)
            throw Error("Effect drawing-list nesting exceeds its budget");
        for (quint32 pc = begin; pc < end; pc += 8) {
            if (++result.commands > 100000)
                throw Error("Effect drawing exceeds its command budget");
            const auto high = word(pc), low = word(pc + 4), op = high >> 24;
            if (op == 0xdf)
                return;
            if (op == 0xde) {
                const auto at = physical(low, 8);
                // A no-push branch can skip inline data in the current list.
                // Keep its original extent; following it as a sub-list would
                // run past the submitted frame looking for a terminator.
                if ((high & 0x10000) && at >= begin && at < end) {
                    pc = at - 8;
                    continue;
                }
                list(at, quint32(ram.size()) & ~7U, nesting + 1);
                if (high & 0x10000)
                    return;
            } else if (op == 0xdb && ((high >> 16) & 255) == 6) {
                segments[(high & 65535) / 4 & 15] = low;
            } else if (op == 0xdc && (high & 255) == 8) {
                const auto at = physical(low, 16);
                viewportScaleX = qint16(half(at)) / 640.;
                viewportScaleY = qint16(half(at + 2)) / 480.;
                viewportOffsetX = (qint16(half(at + 8)) - 640) * viewport.width() / 1280.;
                viewportOffsetY = (qint16(half(at + 10)) - 480) * viewport.height() / 960.;
            } else if (op == 0xda) {
                const auto at = physical(low, 64);
                Matrix incoming;
                lordly_matrix_unpack(incoming.data(),
                                     reinterpret_cast<const uint8_t *>(ram.constData() + at));
                auto &destination = high & 4 ? projection : model;
                if (high & 4)
                    hasProjection = true;
                else if (!(high & 1)) {
                    if (stack.size() >= 32)
                        throw Error("Effect matrix stack overflow");
                    stack.push_back(model);
                }
                if (high & 2)
                    destination = incoming;
                else {
                    Matrix product;
                    lordly_matrix_multiply(product.data(), incoming.data(), destination.data());
                    destination = product;
                }
            } else if (op == 0xd8) {
                int count = int(low / 64);
                if (count < 1 || count > stack.size())
                    throw Error("Effect matrix stack underflow");
                while (count--)
                    model = stack.takeLast();
            } else if (op == 1) {
                const int count = (high >> 12) & 255;
                vertex(low, int((high >> 1) & 127) - count, count);
            } else if (op == 5) {
                triangle((high >> 16 & 255) / 2, (high >> 8 & 255) / 2, (high & 255) / 2);
            } else if (op == 6 || op == 7) {
                triangle((high >> 16 & 255) / 2, (high >> 8 & 255) / 2, (high & 255) / 2);
                triangle((low >> 16 & 255) / 2, (low >> 8 & 255) / 2, (low & 255) / 2);
            } else if (op == 0xd7) {
                textureEnabled = high & 2;
                activeTile = (high >> 8) & 7;
                scaleS = (low >> 16) / 65536.;
                scaleT = (low & 65535) / 65536.;
            } else if (op == 0xd9) {
                zBuffer = ((high & 1) ? zBuffer : false) || (low & 1);
            } else if (op == 0xfd) {
                image = {physical(low), int(high >> 21 & 7), int(high >> 19 & 3),
                         int(high & 4095) + 1};
            } else if (op == 0xf5) {
                auto &tile = tiles[low >> 24 & 7];
                tile.configured = true;
                tile.format = high >> 21 & 7;
                tile.size = high >> 19 & 3;
                tile.line = high >> 9 & 511;
                tile.tmem = high & 511;
                tile.palette = low >> 20 & 15;
                tile.clampS = low & 0x200;
                tile.mirrorS = low & 0x100;
                tile.maskS = low >> 4 & 15;
                tile.shiftS = low & 15;
                tile.clampT = low & 0x80000;
                tile.mirrorT = low & 0x40000;
                tile.maskT = low >> 14 & 15;
                tile.shiftT = low >> 10 & 15;
            } else if (op == 0xf2) {
                auto &tile = tiles[low >> 24 & 7];
                tile.left = high >> 12 & 4095;
                tile.top = high & 4095;
                tile.width = ((low >> 12 & 4095) - tile.left) / 4 + 1;
                tile.height = ((low & 4095) - tile.top) / 4 + 1;
            } else if (op == 0xf3 || op == 0xf4) {
                auto source = image;
                if (op == 0xf4) {
                    const int x = (high >> 12 & 4095) / 4, y = (high & 4095) / 4,
                              bits = 4 << image.size;
                    source.rowBytes = (image.width * bits + 7) / 8;
                    source.address += y * source.rowBytes + x * bits / 8;
                }
                textures[tiles[low >> 24 & 7].tmem] = source;
            } else if (op == 0xf0) {
                const int count = int(low >> 14 & 1023) + 1;
                if (count > 256)
                    throw Error("Effect palette exceeds256 colors");
                const int first = std::clamp((tiles[low >> 24 & 7].tmem - 256) * 4, 0, 255);
                if (first + count > 256)
                    throw Error("Effect palette load exceeds its bank");
                for (int i = 0; i < count; ++i)
                    palette[first + i] = rgba16(half(image.address + i * 2));
            } else if (op == 0xfa) {
                primitive = rgba32(low);
            } else if (op == 0xfb) {
                environment = rgba32(low);
            } else if (op == 0xfc) {
                combineHigh = high;
                combineLow = low;
            } else if (op == 0xe4 || op == 0xe5) {
                if (pc + 24 > end || word(pc + 8) >> 24 != 0xe1 || word(pc + 16) >> 24 != 0xf1)
                    throw Error("Incomplete effect texture rectangle");
                const auto st = word(pc + 12), delta = word(pc + 20);
                pc += 16;
                result.commands += 2;
                const double sx = qint16(st >> 16) / 32., sy = qint16(st & 65535) / 32.;
                const double dx = qint16(delta >> 16) / 1024. / (copyCycle ? 4 : 1),
                             dy = qint16(delta & 65535) / 1024.;
                const double left = (low >> 12 & 4095) / 4., top = (low & 4095) / 4.;
                const double right = (high >> 12 & 4095) / 4. + (copyCycle ? 1 : 0),
                             bottom = (high & 4095) / 4. + (copyCycle ? 1 : 0);
                const int tile = low >> 24 & 7;
                rememberTexture(tile);
                if (twoCycles) rememberTexture((tile + 1) & 7);
                ColorCombiner combiner(combineHigh, combineLow, twoCycles);
                for (int y = std::max(0, int(std::ceil(top * viewport.height() / 240.)));
                     y <
                     std::min(viewport.height(), int(std::ceil(bottom * viewport.height() / 240.)));
                     ++y)
                    for (int x = std::max(0, int(std::ceil(left * viewport.width() / 320.)));
                         x < std::min(viewport.width(),
                                      int(std::ceil(right * viewport.width() / 320.)));
                         ++x) {
                        const double px = (x + .5) * 320. / viewport.width() - left,
                                     py = (y + .5) * 240. / viewport.height() - top;
                        const double s = sx + (op == 0xe5 ? py : px) * dx,
                                     t = sy + (op == 0xe5 ? px : py) * dy;
                        const auto tex = sample(tile, s, t);
                        const auto color = copyCycle
                                               ? tex
                                               : combiner.evaluate({tex,
                                                                    sample((tile + 1) & 7, s, t),
                                                                    primitive,
                                                                    environment,
                                                                    {255, 255, 255, 255}});
                        auto *pixel = result.image.scanLine(y) + x * 4;
                        const double a = color[3] / 255., old = pixel[3] / 255.,
                                     out = a + old * (1 - a);
                        if (out > 0)
                            for (int k = 0; k < 3; ++k)
                                pixel[k] = quint8(std::clamp(
                                    (color[k] * a + pixel[k] * old * (1 - a)) / out, 0., 255.));
                        pixel[3] = quint8(out * 255);
                    }
                result.triangles += 2;
            } else if (op == 0xe3) {
                const int shift = 32 - int(high >> 8 & 255) - int(high & 255) - 1;
                if (shift == 20) {
                    twoCycles = (low & 0x300000) == 0x100000;
                    copyCycle = (low & 0x300000) == 0x200000;
                }
            } else if (op == 0xee) {
                primitiveDepth = (low >> 16) / 32767. - 1.;
            } else if (op == 0xe2) {
                const int count = int(high & 255) + 1, shift = 32 - int(high >> 8 & 255) - count;
                if (count < 1 || count > 32 || shift < 0 || shift > 31)
                    throw Error("Invalid effect rendering-mode command");
                const quint32 mask = count == 32 ? 0xffffffffU : ((1U << count) - 1) << shift;
                lowerMode = (lowerMode & ~mask) | (low & mask);
            } else if (op == 0xe7 || op == 0xe6 || op == 0xe8 || op == 0xed || op == 0xdc ||
                       op == 0xdb || op == 0 || op == 0xd6 || op == 0xe9) {
                // Synchronization, blender/coverage, viewport/light configuration
                // and no-ops do not change this bounded desktop composition.
            } else
                throw Error(
                    QString("Unsupported effect drawing command %1").arg(op, 2, 16, QChar('0')));
        }
        if (nesting)
            throw Error("Effect drawing list has no terminator");
    }
};
} // namespace
EffectRaster::Result EffectRaster::draw(const QByteArray &memory, quint32 begin, quint32 end,
                                        QSize viewport, double zoom) {
    return Raster(memory, viewport, zoom).run(begin, end);
}
} // namespace lordly
