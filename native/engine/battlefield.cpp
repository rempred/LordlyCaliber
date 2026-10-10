#include "engine/battlefield.h"
#include "engine/archive.h"
#include "engine/color_combiner.h"
#include "engine/image.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QVector3D>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <optional>

namespace lordly {
namespace {
using Matrix = std::array<double, 16>;
Matrix identity() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
Matrix multiply(const Matrix &a, const Matrix &b) {
    Matrix c{};
    for (int r = 0; r < 4; ++r)
        for (int col = 0; col < 4; ++col)
            for (int j = 0; j < 4; ++j)
                c[r * 4 + col] += a[r * 4 + j] * b[j * 4 + col];
    return c;
}
double real(const QByteArray &b, int at) {
    const double v = std::bit_cast<float>(u32(b, at));
    if (!std::isfinite(v) || std::abs(v) > 1e8)
        throw Error("Invalid battlefield transform");
    return v;
}
quint32 le32(const QByteArray &b, int at) {
    return u8(b, at) | (quint32(u8(b, at + 1)) << 8) | (quint32(u8(b, at + 2)) << 16) |
           (quint32(u8(b, at + 3)) << 24);
}
QByteArray resource(const Rom &rom, quint32 key) {
    const quint64 at = quint64(key) + 0x594280;
    if (key & 1 || at + 4 > quint64(rom.bytes().size()))
        throw Error("Invalid battlefield resource");
    const quint32 size = u32(rom.bytes(), qsizetype(at));
    if (!size || size > 16 * 1024 * 1024 || at + 4 + size > quint64(rom.bytes().size()))
        throw Error("Battlefield resource exceeds ROM");
    return rom.bytes().mid(qsizetype(at + 4), size);
}
QByteArray member(const Rom &rom, quint32 key, int index) {
    if (index < 0 || index > 128)
        throw Error("Invalid battlefield member");
    const auto bytes = resource(rom, key);
    qsizetype at = 0;
    for (int i = 0; i <= index; ++i) {
        if (at + 9 > bytes.size())
            throw Error("Missing battlefield member");
        const int mode = u8(bytes, at);
        const quint32 stored = le32(bytes, int(at + 1)), size = le32(bytes, int(at + 5));
        if ((mode != 0x80 && mode != 0x85) || !stored || !size || size > 8 * 1024 * 1024 ||
            quint64(at) + 9 + stored > quint64(bytes.size()))
            throw Error("Damaged battlefield member");
        if (i == index) {
            const auto packed = bytes.mid(at + 9, stored);
            if (mode == 0x80) {
                if (stored != size)
                    throw Error("Battlefield stored size mismatch");
                return packed;
            }
            return decodeLh5(packed, size);
        }
        at += 9 + stored;
    }
    throw Error("Missing battlefield member");
}
const QJsonObject &recipes() {
    static const auto result = [] {
        QFile f(":/game/scenarios/field-materials.json");
        if (!f.open(QIODevice::ReadOnly))
            throw Error("Missing battlefield materials");
        return QJsonDocument::fromJson(f.readAll()).object();
    }();
    return result;
}
struct Tile {
    int format = 0, size = 2, line = 0, tmem = 0, palette = 0, cs = 0, ct = 0, ms = 0, mt = 0,
        ss = 0, st = 0;
    int left = 0, top = 0, right = 0, bottom = 0;
};
struct Load {
    quint32 address = 0;
    int format = 0, size = 0, width = 1, left = 0, top = 0;
};
struct Texture {
    RasterImage image;
    Tile tile;
    RenderColor sample(double s, double t) const {
        if (image.rgba.isEmpty())
            return {255, 255, 255, 255};
        auto coordinate = [](double value, int shift, int origin, int mask, int flags, int extent) {
            value = shift <= 10 ? std::ldexp(value, -shift) : std::ldexp(value, 16 - shift);
            value -= origin / 4.;
            if (!std::isfinite(value) || std::abs(value) > 1e9)
                throw Error("Battlefield texture coordinate exceeds bounds");
            int p = int(std::floor(value));
            if (flags & 2)
                p = std::clamp(p, 0, extent - 1);
            else if (mask) {
                const int period = 1 << mask;
                const int block = int(std::floor(double(p) / period));
                p -= block * period;
                if ((flags & 1) && (block & 1))
                    p = period - 1 - p;
            }
            return (p % extent + extent) % extent;
        };
        const int x = coordinate(s, tile.ss, tile.left, tile.ms, tile.cs, image.width);
        const int y = coordinate(t, tile.st, tile.top, tile.mt, tile.ct, image.height);
        const auto *p = reinterpret_cast<const unsigned char *>(image.rgba.constData()) +
                        (y * image.width + x) * 4;
        return {p[0], p[1], p[2], p[3]};
    }
};
struct Material {
    std::shared_ptr<Texture> first, second;
    quint32 high = 0xfc127e24, low = 0xfffff9fc, geometry = 0, other = 0;
    bool two = false, texture = false;
    double scaleS = 1, scaleT = 1;
    RenderColor primitive{255, 255, 255, 255}, environment{255, 255, 255, 255};
};
struct Vertex {
    QVector3D position;
    QPointF uv;
    RenderColor shade{};
};
struct Triangle {
    std::array<Vertex, 3> vertices;
    int material;
};
RenderColor rgba(quint32 word) {
    return {quint8(word >> 24), quint8(word >> 16), quint8(word >> 8), quint8(word)};
}
struct Builder {
    const Rom &rom;
    QVector<quint16> sites;
    QByteArray model, geometry, textures;
    QMap<quint32, QByteArray> external;
    QMap<QString, quint32> bindings;
    QVector<Triangle> triangles;
    QVector<Material> materials;
    QMap<QString, std::shared_ptr<Texture>> textureCache;
    QSet<int> buildingSites;
    QStringList warnings;
    std::array<Tile, 8> tiles;
    QMap<int, Load> loads, palettes;
    quint32 image = 0, combineHigh = 0xfc127e24, combineLow = 0xfffff9fc, geometryMode = 0x220404,
            otherLow = 0, otherHigh = 0;
    int imageFormat = 0, imageSize = 2, imageWidth = 1, textureTile = 0;
    double scaleS = 1, scaleT = 1;
    bool textureOn = false, dirty = true;
    RenderColor primitive{255, 255, 255, 255}, environment{255, 255, 255, 255};
    std::array<std::optional<Vertex>, 64> vertices;
    Matrix matrix = identity();
    QVector<Matrix> stack;
    int instructions = 0, activeMaterial = -1;
    quint32 bindNext = 0x04000000;
    Builder(const Rom &r, QVector<quint16> s) : rom(r), sites(std::move(s)) {}
    QByteArray bytes(quint32 address, int size) const {
        if (size < 0 || size > 8 * 1024 * 1024)
            throw Error("Battlefield read exceeds budget");
        const QByteArray *source = nullptr;
        quint64 at = address & 0xffffff;
        if (address >> 24 == 2)
            source = &geometry;
        else if (address >> 24 == 3)
            source = &textures;
        else if (address >= 0x8019ee50 && address < 0x801ad290) {
            source = &rom.bytes();
            at = quint64(address) - 0x800b32a0;
        } else if (address >= 0x80197b70 && address < 0x8019ee30) {
            source = &rom.bytes();
            at = quint64(address) - 0x800b3280;
        } else {
            auto found = external.upperBound(address);
            if (found != external.begin()) {
                --found;
                source = &found.value();
                at = address - found.key();
            }
        }
        if (!source || at + quint64(size) > quint64(source->size()))
            throw Error(
                QString("Unresolved battlefield address %1").arg(address, 8, 16, QChar('0')));
        return source->mid(qsizetype(at), size);
    }
    quint32 bind(const QJsonObject &r) {
        const auto name = QString::fromUtf8(QJsonDocument(r).toJson(QJsonDocument::Compact));
        if (bindings.contains(name))
            return bindings[name];
        const auto root = resource(rom, quint32(r["root"].toInteger()));
        const int sel = r["selector"].toInt();
        if (sel < 0 || sel >= root.size() / 4)
            throw Error("Battlefield material selector exceeds root");
        const quint32 key = u32(root, sel * 4);
        QByteArray data;
        if (r["mode"].toInt() == -10) {
            const auto packed = resource(rom, key);
            const quint32 size = u32(packed, 0);
            if (size > 4 * 1024 * 1024)
                throw Error("Battlefield texture exceeds budget");
            data = decodeLzss(packed.mid(4), size);
        } else
            data = member(rom, key, r["member"].toInt());
        const quint32 address = bindNext;
        bindNext += quint32((data.size() + 15) & ~15);
        if (bindNext > 0x05000000)
            throw Error("Battlefield textures exceed memory budget");
        external.insert(address, std::move(data));
        bindings[name] = address;
        return address;
    }
    void recipe(const QString &name) {
        const auto r = recipes()["recipes"].toObject()[name].toObject();
        if (r.isEmpty())
            throw Error("Unsupported battlefield material: " + name);
        const auto sources = r["resources"].toArray();
        for (const auto &value : r["commands"].toArray()) {
            const auto c = value.toArray();
            const auto a = quint32(c[0].toInteger());
            quint32 b = 0;
            if (c[1].isObject()) {
                const auto ref = c[1].toObject();
                if (ref["identity"].toBool()) {
                    b = 0x06000000;
                    if (!external.contains(b)) {
                        QByteArray id(64, 0);
                        for (int i = 0; i < 4; ++i)
                            put16(id, i * 10, 1);
                        external[b] = id;
                    }
                } else {
                    const int i = ref["resource"].toInt(-1);
                    if (i < 0 || i >= sources.size())
                        throw Error("Invalid battlefield recipe");
                    b = bind(sources[i].toObject()) + quint32(ref["offset"].toInteger());
                }
            } else
                b = quint32(c[1].toInteger());
            command(a, b, 0);
        }
    }
    std::shared_ptr<Texture> texture(int tileId) {
        const auto tile = tiles[tileId];
        if (!loads.contains(tile.tmem))
            return {};
        const auto source = loads[tile.tmem];
        const auto palette = palettes.value(256);
        QByteArray key;
        auto field = [&](quint32 v) {
            const int at = int(key.size());
            key.resize(at + 4);
            put32(key, at, v);
        };
        for (int v :
             {tile.format, tile.size, tile.line, tile.tmem, tile.palette, tile.cs, tile.ct, tile.ms,
              tile.mt, tile.ss, tile.st, tile.left, tile.top, tile.right, tile.bottom})
            field(quint32(v));
        field(source.address);
        field(palette.address);
        const QString name = QString::fromLatin1(key.toHex());
        if (textureCache.contains(name))
            return textureCache[name];
        auto result = std::make_shared<Texture>();
        result->tile = tile;
        // Wrapped tiles use their masks for their extent. Animated water updates
        // tile origins with zero lower bounds; those commands do not make it 1x1.
        const int width =
            !(tile.cs & 2) && tile.ms ? 1 << tile.ms : (tile.right - tile.left) / 4 + 1;
        const int height =
            !(tile.ct & 2) && tile.mt ? 1 << tile.mt : (tile.bottom - tile.top) / 4 + 1;
        if (width < 1 || height < 1 || width > 512 || height > 512)
            throw Error("Invalid battlefield texture dimensions");
        const int stride = tile.line ? tile.line * 8 : ((width * (4 << tile.size) + 7) / 8);
        const int required = (width * (4 << tile.size) + 7) / 8;
        if (stride < required)
            throw Error("Battlefield texture stride is too short");
        const auto data = bytes(source.address, stride * height);
        QMap<int, quint16> pal;
        result->image = {width, height, QByteArray(width * height * 4, 0)};
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                RenderColor color{};
                if (tile.format == 0 && tile.size == 2) {
                    const quint16 c = u16(data, y * stride + x * 2);
                    color = {quint8((c >> 11) * 255 / 31), quint8(((c >> 6) & 31) * 255 / 31),
                             quint8(((c >> 1) & 31) * 255 / 31), quint8((c & 1) * 255)};
                } else if (tile.format == 0 && tile.size == 3)
                    color = rgba(u32(data, y * stride + x * 4));
                else if (tile.format == 2 && (tile.size == 0 || tile.size == 1)) {
                    int index = u8(data, y * stride + (tile.size ? x : x / 2));
                    if (!tile.size)
                        index = ((index >> (x % 2 ? 0 : 4)) & 15) + tile.palette * 16;
                    // Some retail resources omit unused trailing TLUT entries even
                    // when LoadTLUT requests 256. Read only entries the pixels use.
                    if (!pal.contains(index))
                        pal[index] = u16(bytes(palette.address + index * 2, 2), 0);
                    const quint16 c = pal[index];
                    color = {quint8((c >> 11) * 255 / 31), quint8(((c >> 6) & 31) * 255 / 31),
                             quint8(((c >> 1) & 31) * 255 / 31), quint8((c & 1) * 255)};
                } else if (tile.format == 4 && (tile.size == 0 || tile.size == 1)) {
                    int c = u8(data, y * stride + (tile.size ? x : x / 2));
                    if (!tile.size)
                        c = ((c >> (x % 2 ? 0 : 4)) & 15) * 17;
                    color = {quint8(c), quint8(c), quint8(c), quint8(c)};
                } else if (tile.format == 3 &&
                           (tile.size == 0 || tile.size == 1 || tile.size == 2)) {
                    const int c = tile.size == 2 ? u16(data, y * stride + x * 2)
                                                 : u8(data, y * stride + (tile.size ? x : x / 2));
                    const int nibble = (c >> (x % 2 ? 0 : 4)) & 15;
                    const int intensity = tile.size == 2   ? c >> 8
                                          : tile.size == 1 ? (c >> 4) * 17
                                                           : (nibble >> 1) * 255 / 7;
                    const int alpha = tile.size == 2   ? c & 255
                                      : tile.size == 1 ? (c & 15) * 17
                                                       : (nibble & 1) * 255;
                    color = {quint8(intensity), quint8(intensity), quint8(intensity),
                             quint8(alpha)};
                } else
                    throw Error(QString("Unsupported battlefield texture %1/%2")
                                    .arg(tile.format)
                                    .arg(tile.size));
                for (int c = 0; c < 4; ++c)
                    put8(result->image.rgba, (y * width + x) * 4 + c, color[c]);
            }
        textureCache[name] = result;
        return result;
    }
    int material() {
        if (!dirty)
            return activeMaterial;
        Material m;
        m.high = combineHigh;
        m.low = combineLow;
        m.two = ((otherHigh >> 20) & 3) == 1;
        m.texture = textureOn;
        m.geometry = geometryMode;
        m.other = otherLow;
        m.scaleS = scaleS;
        m.scaleT = scaleT;
        m.primitive = primitive;
        m.environment = environment;
        if (textureOn) {
            m.first = texture(textureTile);
            if (m.two)
                m.second = texture((textureTile + 1) & 7);
        }
        ColorCombiner(m.high, m.low, m.two).evaluate({});
        materials << m;
        activeMaterial = int(materials.size() - 1);
        dirty = false;
        return activeMaterial;
    }
    void triangle(quint32 word) {
        Triangle t;
        t.material = material();
        for (int i = 0; i < 3; ++i) {
            const int index = int(word >> (16 - i * 8) & 255) / 2;
            if (index >= 64 || !vertices[index])
                throw Error("Battlefield triangle references a missing vertex");
            t.vertices[i] = *vertices[index];
        }
        if (triangles.size() >= 200000)
            throw Error("Battlefield exceeds triangle budget");
        triangles << t;
    }
    void walk(quint32 address, int depth = 0) {
        if (depth > 32)
            throw Error("Battlefield display-list recursion");
        for (;; address += 8) {
            if (++instructions > 500000)
                throw Error("Battlefield command budget exhausted");
            const auto c = bytes(address, 8);
            const quint32 a = u32(c, 0), b = u32(c, 4), op = a >> 24;
            if (op == 0xdf)
                return;
            command(a, b, depth);
            if (op == 0xde && (a & 0x10000))
                return;
        }
    }
    void command(quint32 a, quint32 b, int depth) {
        const int op = int(a >> 24);
        switch (op) {
        case 0xde:
            walk(b, depth + 1);
            return;
        case 0xda: {
            if (a & 4)
                throw Error("Unexpected projection matrix in battlefield asset");
            const auto data = bytes(b, 64);
            Matrix m;
            for (int i = 0; i < 16; ++i)
                m[i] = qint16(u16(data, i * 2)) + u16(data, 32 + i * 2) / 65536.;
            if (!(a & 1)) {
                if (stack.size() >= 32)
                    throw Error("Battlefield matrix stack overflow");
                stack << matrix;
            }
            matrix = a & 2 ? m : multiply(m, matrix);
            return;
        }
        case 0xd8:
            if (b % 64 || b / 64 > quint32(stack.size()))
                throw Error("Battlefield matrix stack underflow");
            for (quint32 i = 0; i < b / 64; ++i)
                matrix = stack.takeLast();
            return;
        case 1: {
            const int count = int(a >> 12 & 255), first = int(a >> 1 & 127) - count;
            if (count < 1 || first < 0 || first + count > 64)
                throw Error("Invalid battlefield vertex cache load");
            const auto data = bytes(b, count * 16);
            for (int i = 0; i < count; ++i) {
                const double x = qint16(u16(data, i * 16)), y = qint16(u16(data, i * 16 + 2)),
                             z = qint16(u16(data, i * 16 + 4));
                Vertex v;
                double p[3];
                for (int j = 0; j < 3; ++j) {
                    p[j] = x * matrix[j] + y * matrix[4 + j] + z * matrix[8 + j] + matrix[12 + j];
                    if (!std::isfinite(p[j]) || std::abs(p[j]) > 1e6)
                        throw Error("Battlefield vertex exceeds world bounds");
                }
                v.position = {float(p[0]), float(p[1]), float(p[2])};
                v.uv = {qint16(u16(data, i * 16 + 8)) / 32., qint16(u16(data, i * 16 + 10)) / 32.};
                if (geometryMode & 0x20000) {
                    const double nx = qint8(u8(data, i * 16 + 12)),
                                 ny = qint8(u8(data, i * 16 + 13)),
                                 nz = qint8(u8(data, i * 16 + 14));
                    QVector3D n(float(nx * matrix[0] + ny * matrix[4] + nz * matrix[8]),
                                float(nx * matrix[1] + ny * matrix[5] + nz * matrix[9]),
                                float(nx * matrix[2] + ny * matrix[6] + nz * matrix[10]));
                    n.normalize();
                    const int light =
                        int(255 *
                            std::clamp(.65 + .35 * QVector3D::dotProduct(
                                                       n, QVector3D(-.3f, .9f, -.3f).normalized()),
                                       .25, 1.));
                    v.shade = {quint8(light), quint8(light), quint8(light), u8(data, i * 16 + 15)};
                } else
                    for (int c = 0; c < 4; ++c)
                        v.shade[c] = u8(data, i * 16 + 12 + c);
                vertices[first + i] = v;
            }
            return;
        }
        case 5:
            triangle(a);
            return;
        case 6:
        case 7:
            triangle(a);
            triangle(b);
            return;
        case 0xfd:
            image = b;
            imageFormat = int(a >> 21 & 7);
            imageSize = int(a >> 19 & 3);
            imageWidth = int(a & 4095) + 1;
            break;
        case 0xf5: {
            auto &t = tiles[b >> 24 & 7];
            t.format = int(a >> 21 & 7);
            t.size = int(a >> 19 & 3);
            t.line = int(a >> 9 & 511);
            t.tmem = int(a & 511);
            t.palette = int(b >> 20 & 15);
            t.ct = int(b >> 18 & 3);
            t.mt = int(b >> 14 & 15);
            t.st = int(b >> 10 & 15);
            t.cs = int(b >> 8 & 3);
            t.ms = int(b >> 4 & 15);
            t.ss = int(b & 15);
            break;
        }
        case 0xf2: {
            auto &t = tiles[b >> 24 & 7];
            t.left = int(a >> 12 & 4095);
            t.top = int(a & 4095);
            t.right = int(b >> 12 & 4095);
            t.bottom = int(b & 4095);
            break;
        }
        case 0xf3:
        case 0xf4:
            loads[tiles[b >> 24 & 7].tmem] = {image,      imageFormat,         imageSize,
                                              imageWidth, int(a >> 12 & 4095), int(a & 4095)};
            break;
        case 0xf0:
            palettes[tiles[b >> 24 & 7].tmem] = {image, imageFormat, imageSize, imageWidth, 0, 0};
            break;
        case 0xfa:
            primitive = rgba(b);
            break;
        case 0xfb:
            environment = rgba(b);
            break;
        case 0xfc:
            combineHigh = a;
            combineLow = b;
            break;
        case 0xd9:
            geometryMode = (geometryMode & (a & 0xffffff)) | b;
            break;
        case 0xd7:
            textureOn = a & 2;
            textureTile = int(a >> 8 & 7);
            scaleS = (b >> 16) / 65536.;
            scaleT = (b & 65535) / 65536.;
            break;
        case 0xe2:
        case 0xe3: {
            const int count = int(a & 255) + 1, shift = 32 - int(a >> 8 & 255) - count;
            if (count > 32 || shift < 0)
                throw Error("Invalid battlefield render mode");
            const quint32 mask = count == 32 ? 0xffffffffu : ((1u << count) - 1) << shift;
            auto &mode = op == 0xe2 ? otherLow : otherHigh;
            mode = (mode & ~mask) | (b & mask);
            break;
        }
        case 0:
        case 0xe6:
        case 0xe7:
        case 0xe8:
        case 0xe9:
        case 0xdb:
        case 0xdc:
        case 0xdf:
        case 0xed:
        case 0xf8:
        case 0xf9:
            return;
        default:
            throw Error(
                QString("Unsupported battlefield drawing command %1").arg(op, 2, 16, QChar('0')));
        }
        dirty = true;
    }
};
} // namespace
struct Battlefield::Data {
    QVector<Triangle> triangles;
    QVector<Material> materials;
    QStringList notices;
    int buildings = 0;
    quint32 key = 0;
};
Battlefield Battlefield::load(const Rom &rom, int scenario, const QVector<quint16> &siteFlags) {
    rom.requireSupported();
    if (rom.revision() || scenario < 1 || scenario > 64)
        throw Error("Battlefield preview requires a revision-0 scenario");
    const int modelId = qint8(u8(rom.bytes(), 0x654a0 + scenario * 9 + 3));
    if (modelId < 0 || modelId >= 69)
        throw Error("Scenario has no battlefield model");
    const int row = 0x65710 + modelId * 12;
    const quint32 flags = u32(rom.bytes(), row + 8), selection = u32(rom.bytes(), row);
    const auto root = resource(rom, 0x1e39dce);
    if (selection >= quint32(root.size() / 4))
        throw Error("Battlefield model exceeds its collection");
    Builder b(rom, siteFlags);
    const quint32 key = u32(root, selection * 4);
    b.model = member(rom, key, 0);
    if (b.model.size() < 52)
        throw Error("Truncated battlefield header");
    const quint32 geo = (u32(b.model, 48) + 15) & ~15u;
    if (geo >= quint32(b.model.size()))
        throw Error("Invalid battlefield geometry offset");
    b.geometry = b.model.mid(geo);
    const quint32 textureSize = u32(b.model, 16);
    if (textureSize > 8 * 1024 * 1024)
        throw Error("Battlefield texture allocation exceeds budget");
    const auto tr = resource(rom, 0x203c53e);
    for (quint32 at = u32(b.model, 12); quint32(b.textures.size()) < textureSize; at += 2) {
        const int index = u16(b.model, at);
        if (index >= tr.size() / 4)
            throw Error("Invalid battlefield texture reference");
        const auto packed = resource(rom, u32(tr, index * 4));
        const quint32 size = u32(packed, 0);
        if (size > textureSize - quint32(b.textures.size()))
            throw Error("Battlefield texture blocks exceed allocation");
        b.textures += decodeLzss(packed.mid(4), size);
    }
    if (flags & 0x2000)
        b.recipe("grass");
    if (flags & 0x10000)
        b.recipe("snow");
    if (flags & 0x4000)
        b.warnings << "This map's additional ground effect is not yet drawn.";
    // Original field wordcode, z64 EF59C..EFD28. Full-map view retains every
    // frustum chunk. Initial visibility flags follow the map constructor.
    QByteArray visibility(16, char(-1));
    if (modelId == 23 || modelId == 24)
        put8(visibility, 0, quint8(u8(visibility, 0) & ~4));
    if (modelId == 43 || modelId == 44) {
        put8(visibility, 0, quint8(u8(visibility, 0) & ~64));
        put8(visibility, 1, quint8(u8(visibility, 1) & 0xfa));
    }
    if (modelId == 42 || modelId == 3) {
        put8(visibility, 0, quint8(u8(visibility, 0) & ~64));
        put8(visibility, 1, quint8(u8(visibility, 1) & 0xaa));
        put8(visibility, 2, quint8(u8(visibility, 2) & 0xfa));
    }
    constexpr int widths[] = {0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 3, 3, 1,
                              1, 0, 1, 1, 1, 2, 2, 3, 1, 2, 2, 3, 2};
    QMap<int, Matrix> saved;
    QVector<Matrix> parents;
    Matrix current = identity();
    double alpha = 1;
    int commands = 0;
    for (quint32 at = u32(b.model, 0);;) {
        if (++commands > 20000)
            throw Error("Battlefield wordcode budget exhausted");
        const quint32 op = u32(b.model, at);
        at += 4;
        if (!op)
            break;
        if (op > 25)
            throw Error("Unknown battlefield wordcode");
        std::array<quint32, 3> args{};
        for (int i = 0; i < widths[op]; ++i) {
            args[i] = u32(b.model, at);
            at += 4;
        }
        if (op == 1 || op == 18 || op == 19 || op == 20 || op == 22 || op == 23 || op == 24) {
            const int bit = op == 19 || op == 23   ? int(args[1])
                            : op == 20 || op == 24 ? int(args[2])
                                                   : -1;
            if (bit >= 128)
                throw Error("Battlefield visibility flag exceeds bitmap");
            if (bit >= 0 && !(u8(visibility, bit / 8) & (1 << (bit % 8))))
                continue;
            if (op >= 22 && alpha == 0)
                continue;
            b.walk(args[0]);
        } else if (op == 2)
            b.recipe(QString("preset%1").arg(args[0]));
        else if (op == 25) {
            if (args[0] == 0) {
                const int site = int(args[1]);
                b.buildingSites.insert(site);
                const int side = site > 0 && site <= siteFlags.size() ? siteFlags[site - 1] & 6 : 0;
                b.recipe(side == 2 ? "ally" : side == 4 ? "enemy" : "neutral");
            } else
                b.warnings << "Animated map effects are omitted from this overview.";
        } else if (op == 3) {
            if (!saved.contains(int(args[0])))
                throw Error("Battlefield references an uninitialized transform");
            b.matrix = saved[int(args[0])];
        } else if (op == 5 || op == 6) {
            Matrix next;
            const quint64 off = quint64(u32(b.model, 4)) + quint64(args[0]) * 64;
            if (off + 64 > quint64(b.model.size()))
                throw Error("Battlefield transform exceeds member");
            for (int i = 0; i < 16; ++i)
                next[i] = real(b.model, int(off) + i * 4);
            current = op == 6 ? next : multiply(next, current);
        } else if (op == 7)
            saved[int(args[0])] = current;
        else if (op == 8) {
            if (parents.size() > 32)
                throw Error("Battlefield transform stack overflow");
            parents << current;
        } else if (op == 9) {
            if (parents.isEmpty())
                throw Error("Battlefield transform stack underflow");
            current = parents.takeLast();
        } else if (op == 21) {
            const quint64 off = quint64(u32(b.model, 36)) + quint64(args[0]) * 40;
            if (off + 40 > quint64(b.model.size()))
                throw Error("Battlefield animated transform exceeds member");
            const auto p = int(off);
            Matrix scale = identity(), translate = identity();
            for (int i = 0; i < 3; ++i) {
                scale[i * 5] = real(b.model, p + 24 + i * 4);
                translate[12 + i] = real(b.model, p + i * 4);
            }
            Matrix rotate = identity();
            for (int axis = 0; axis < 3; ++axis) {
                const double angle = real(b.model, p + 12 + axis * 4) * 3.141592653589793 / 180.;
                Matrix r = identity();
                const int a = (axis + 1) % 3, c = (axis + 2) % 3;
                r[a * 4 + a] = r[c * 4 + c] = std::cos(angle);
                r[a * 4 + c] = std::sin(angle);
                r[c * 4 + a] = -std::sin(angle);
                rotate = multiply(rotate, r);
            }
            current = multiply(multiply(multiply(scale, rotate), translate), current);
            alpha = real(b.model, p + 36);
        } else if (op == 4)
            b.warnings << "An additional animated map effect is not drawn.";
        // 10..17 are visibility/culling storage; host overview has no frustum.
    }
    if (b.triangles.isEmpty())
        throw Error("Battlefield contains no drawable geometry");
    Battlefield result;
    result.data_ = std::make_shared<Data>();
    result.data_->triangles = std::move(b.triangles);
    result.data_->materials = std::move(b.materials);
    b.warnings.removeDuplicates();
    result.data_->notices = b.warnings;
    result.data_->buildings = b.buildingSites.size();
    result.data_->key = key;
    return result;
}
QImage Battlefield::render(QRectF bounds, int pixels) const {
    if (!data_ || !bounds.isValid() || pixels < 64 || pixels > 4096)
        throw Error("Invalid battlefield preview extent");
    QImage out(pixels, pixels, QImage::Format_ARGB32);
    out.fill(QColor("#18232b"));
    QVector<float> depth(pixels * pixels, -std::numeric_limits<float>::infinity());
    QVector<ColorCombiner> combiners;
    for (const auto &m : data_->materials)
        combiners << ColorCombiner(m.high, m.low, m.two);
    const double sx = pixels / bounds.width(), sy = pixels / bounds.height();
    auto edge = [](QPointF a, QPointF b, QPointF p) {
        return (b.x() - a.x()) * (p.y() - a.y()) - (b.y() - a.y()) * (p.x() - a.x());
    };
    for (const auto &t : data_->triangles) {
        const auto &m = data_->materials[t.material];
        std::array<QPointF, 3> p;
        for (int i = 0; i < 3; ++i)
            p[i] = {(t.vertices[i].position.x() - bounds.left()) * sx,
                    (t.vertices[i].position.z() - bounds.top()) * sy};
        const double area = edge(p[0], p[1], p[2]);
        if (std::abs(area) < 1e-8)
            continue;
        if ((m.geometry & 0x400) && area > 0)
            continue;
        if ((m.geometry & 0x200) && area < 0)
            continue;
        const int left = std::max(0, int(std::floor(std::min({p[0].x(), p[1].x(), p[2].x()})))),
                  right = std::min(pixels - 1,
                                   int(std::ceil(std::max({p[0].x(), p[1].x(), p[2].x()}))));
        const int top = std::max(0, int(std::floor(std::min({p[0].y(), p[1].y(), p[2].y()})))),
                  bottom = std::min(pixels - 1,
                                    int(std::ceil(std::max({p[0].y(), p[1].y(), p[2].y()}))));
        for (int y = top; y <= bottom; ++y) {
            auto *line = reinterpret_cast<QRgb *>(out.scanLine(y));
            for (int x = left; x <= right; ++x) {
                const QPointF q(x + .5, y + .5);
                const double a = edge(p[1], p[2], q) / area, b = edge(p[2], p[0], q) / area,
                             c = 1 - a - b;
                if (a < -1e-8 || b < -1e-8 || c < -1e-8)
                    continue;
                const double z = a * t.vertices[0].position.y() + b * t.vertices[1].position.y() +
                                 c * t.vertices[2].position.y();
                auto &prior = depth[y * pixels + x];
                if ((m.other & 0x10) && z < prior - 1e-5)
                    continue;
                CombineInputs in;
                in.primitive = m.primitive;
                in.environment = m.environment;
                for (int channel = 0; channel < 4; ++channel)
                    in.shade[channel] = quint8(std::clamp(a * t.vertices[0].shade[channel] +
                                                              b * t.vertices[1].shade[channel] +
                                                              c * t.vertices[2].shade[channel],
                                                          0., 255.));
                const double s = (a * t.vertices[0].uv.x() + b * t.vertices[1].uv.x() +
                                  c * t.vertices[2].uv.x()) *
                                 m.scaleS,
                             v = (a * t.vertices[0].uv.y() + b * t.vertices[1].uv.y() +
                                  c * t.vertices[2].uv.y()) *
                                 m.scaleT;
                in.texel0 = m.first ? m.first->sample(s, v) : RenderColor{255, 255, 255, 255};
                in.texel1 = m.second ? m.second->sample(s, v) : in.texel0;
                const auto color = combiners[t.material].evaluate(in);
                const int alpha = color[3];
                if (!alpha)
                    continue;
                if (alpha == 255)
                    line[x] = qRgb(color[0], color[1], color[2]);
                else {
                    const auto old = line[x];
                    line[x] = qRgb((color[0] * alpha + qRed(old) * (255 - alpha)) / 255,
                                   (color[1] * alpha + qGreen(old) * (255 - alpha)) / 255,
                                   (color[2] * alpha + qBlue(old) * (255 - alpha)) / 255);
                }
                if (m.other & 0x20)
                    prior = float(z);
            }
        }
    }
    return out;
}
int Battlefield::triangleCount() const {
    return data_ ? int(data_->triangles.size()) : 0;
}
int Battlefield::buildingCount() const {
    return data_ ? data_->buildings : 0;
}
QStringList Battlefield::notices() const {
    return data_ ? data_->notices : QStringList{};
}
quint32 Battlefield::resourceKey() const {
    return data_ ? data_->key : 0;
}
} // namespace lordly
