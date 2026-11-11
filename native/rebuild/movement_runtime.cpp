#include "rebuild/movement_runtime.h"
#include "engine/capacity_placement.h"
#include "engine/class_data.h"
#include "engine/mission_tables.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <bit>
namespace lordly {
namespace {
QByteArray read(const QString &p) {
    QFile f(":/" + p);
    if (!f.open(QIODevice::ReadOnly))
        throw Error("Missing movement source: " + p);
    return f.readAll();
}
void align(QByteArray &b) {
    while (b.size() % 16)
        b.append(char(0));
}
} // namespace
MovementPlacement buildMovementRuntime(const MovementInput &input, quint32 address, quint32 offset,
                                       const Compiler &compiler, const QString &dir,
                                       const std::function<quint32(quint32)> &relocate) {
    MovementPlacement p;
    p.address = address;
    p.offset = offset;
    p.model = input.types.json();
    auto &b = p.data;
    b = QByteArray(64, 0);
    b.replace(0, 8, "LCMOVDAT");
    const int rows = input.types.entries().lastKey() + 1;
    const auto initial = quint32(b.size());
    b += QByteArray(rows * 64, char(-1));
    b.replace(initial, 64, input.source.bytes().mid(0x13fa58, 64));
    for (auto i = input.types.entries().begin(); i != input.types.entries().end(); ++i)
        for (int t = 0; t < 32; ++t)
            put16(b, initial + i.key() * 64 + t * 2, quint16(i->costs[t]));
    const auto working = quint32(b.size());
    b += b.mid(initial, rows * 64);
    const auto kinds = quint32(b.size());
    b += QByteArray(256, char(7));
    for (auto i = input.types.entries().begin(); i != input.types.entries().end(); ++i)
        put8(b, kinds + i.key(), quint8(i->behavior));
    const auto speeds = quint32(b.size());
    b += QByteArray(1024, 0);
    for (int i = 0; i < 256; i++)
        put32(b, speeds + i * 4, std::bit_cast<quint32>(35.f));
    for (auto i = input.types.entries().begin(); i != input.types.entries().end(); ++i)
        put32(b, speeds + i.key() * 4, std::bit_cast<quint32>(float(i->baseSpeed)));
    const auto priority = quint32(b.size());
    b += input.types.priorityBytes();
    align(b);
    const auto curve = quint32(b.size());
    for (float v : input.types.curve()) {
        auto at = b.size();
        b += QByteArray(4, 0);
        put32(b, at, std::bit_cast<quint32>(v));
    }
    const auto json = QJsonDocument(p.model).toJson(QJsonDocument::Compact);
    put32(b, 8, b.size());
    put32(b, 12, json.size());
    b += json;
    align(b);
    put32(b, 24, initial);
    put32(b, 28, working);
    put32(b, 32, speeds);
    put32(b, 36, kinds);
    put32(b, 40, priority);
    put32(b, 44, rows);
    put32(b, 48, input.types.entries().size());
    put32(b, 52, curve);
    const QByteArray config =
        QByteArray("#define LC_MOVE_COUNT ") + QByteArray::number(input.types.entries().size()) +
        "\n#define LC_MOVE_ROWS " + QByteArray::number(rows) + "\n#define LC_MOVE_CLASSES " +
        QByteArray::number(input.classCount) + "\n";
    auto policy = compiler.compileModule(
        read("game/movement/policy.c") + "\n" + read("game/movement/save-cache.c"), dir,
        "movement_policy", address + b.size(),
        {{"movement_config.h", config}, {"save-cache.h", read("game/movement/save-cache.h")}},
        {{"lc_movement_kinds", address + kinds},
         {"lc_movement_speeds", address + speeds},
         {"lc_movement_priority", address + priority},
         {"lc_movement_sources", relocate(0x801971f0)},
         {"lc_movement_groups", relocate(0x801969b8)},
         {"lc_movement_characters", relocate(0x80193bc0)},
         {"lc_movement_records", relocate(0x80187c14)}},
        true);
    b += policy.bytes;
    align(b);
    const auto catalog =
        QJsonDocument::fromJson(read("game/movement/owners.json")).object()["owners"].toArray();
    QSet<QString> variants;
    for (const auto &v : catalog)
        if (v.toObject()["capacity"].toBool())
            variants.insert(v.toObject()["name"].toString());
    struct Hook {
        quint32 start, target;
    };
    QVector<Hook> hooks;
    const auto placement = ClassPlacement::read(input.source);
    quint32 rawMovement = 0;
    for (const auto &value : catalog) {
        const auto o = value.toObject();
        const auto name = o["name"].toString();
        if (o["capacity"].toBool() != (input.capacity && variants.contains(name)))
            continue;
        const auto source = read(o["source"].toString());
        if (sha256(source) != o["sha256"].toString().toLatin1())
            throw Error("Movement C source has changed: " + name);
        QMap<QString, QByteArray> headers{
            {"movement_config.h", QByteArray("#define LC_MOVE_COUNT ") +
                                      QByteArray::number(input.types.entries().size()) +
                                      "\n#define LC_MOVE_ROWS " + QByteArray::number(rows) + "\n"}};
        for (const auto &hv : o["headers"].toArray()) {
            const auto h = hv.toObject();
            const auto data = read(h["file"].toString());
            if (sha256(data).toUpper() != h["sha256"].toString().toLatin1().toUpper())
                throw Error("Movement header has changed");
            auto key = h["path"].toString();
            if (key.startsWith("include/"))
                key.remove(0, 8);
            headers[key] = data;
            headers[key.section('/', -1)] = data;
        }
        QMap<QString, quint32> bindings;
        const auto symbols = o["bindings"].toObject();
        for (auto i = symbols.begin(); i != symbols.end(); ++i) {
            auto a = quint32(i->toDouble());
            if (a >= 0x801f36d8 && a < 0x801f38d8)
                a = address + working + a - 0x801f36d8;
            else if (a >= 0x801eb318 && a < 0x801eb518)
                a = address + initial + a - 0x801eb318;
            else if (a == 0x80186fbc)
                a = address + priority;
            else if (a == 0x801e862c)
                a = address + curve;
            else
                a = relocate(a);
            bindings[i.key()] = a;
        }
        bindings["lc_movement_refresh_saved"] = policy.entries.value("lc_movement_refresh_saved");
        bindings["lc_movement_kind"] = policy.entries.value("lc_movement_kind");
        bindings["lc_movement_factor"] = policy.entries.value("lc_movement_factor");
        // Capacity's maintained variant refers to its explicit expanded data.
        if (input.capacity) {
            bindings["lc_enemy_records"] = relocate(0x80195560);
            bindings["lc_enemy_sources"] = relocate(0x801971d0);
            bindings["lc_enemy_types"] = relocate(0x80195560) + 0x3400;
        }
        if (input.capacity) {
            headers["enemy-capacity.h"] = read("game/scenarios/enemy-capacity.h");
            for (auto i = input.bindings.begin(); i != input.bindings.end(); ++i)
                if (!bindings.contains(i.key()))
                    bindings[i.key()] = i.value();
        }
        // A capacity module can export this same function. Its old entry is not
        // an external symbol of the movement replacement being compiled here.
        for (const auto &part : o["parts"].toArray()) {
            const auto own =
                QString("func_%1").arg(part.toObject()["start"].toInteger(), 8, 16, QChar('0'));
            for (auto i = bindings.begin(); i != bindings.end();)
                if (i.key().compare(own, Qt::CaseInsensitive) == 0)
                    i = bindings.erase(i);
                else
                    ++i;
        }
        bindings["lc_movement_class_records"] = relocate(0x80187c18);
        if (name.compare("func_000bd154", Qt::CaseInsensitive) == 0) {
            if (!rawMovement)
                throw Error("Missing raw movement lookup");
            bindings["func_0016DBD8"] = rawMovement;
        }
        const auto code = compiler.compileModule(source, dir, "movement_" + name,
                                                 address + b.size(), headers, bindings, true);
        if (code.entries.contains("lc_movement_raw"))
            rawMovement = code.entries["lc_movement_raw"];
        for (const auto &pv : o["parts"].toArray()) {
            const auto part = pv.toObject();
            auto start = quint32(part["start"].toDouble()), size = quint32(part["size"].toDouble());
            auto original = MissionTables::canonicalCode(input.source, start, size);
            if (placement)
                original = placement->canonicalReferences(original, start);
            if (sha256(original) != part["sha256"].toString().toLatin1())
                throw Error("Unrecognized movement owner at z64 " + QString::number(start, 16));
            const auto wanted = QString("func_%1").arg(start, 8, 16, QChar('0'));
            quint32 entry = 0;
            for (auto i = code.entries.begin(); i != code.entries.end(); ++i)
                if (i.key().compare(wanted, Qt::CaseInsensitive) == 0)
                    entry = i.value();
            if (entry < code.address || entry >= code.address + code.bytes.size())
                throw Error("Missing movement entry: " + wanted);
            hooks.append({start, entry});
        }
        b += code.bytes;
        align(b);
    }
    p.patches = b.size();
    p.patchCount = hooks.size() * 2;
    put32(b, 16, p.patches);
    put32(b, 20, p.patchCount);
    for (const auto &h : hooks)
        for (int i = 0; i < 2; i++) {
            const auto at = b.size();
            b += QByteArray(12, 0);
            put32(b, at, h.start + i * 4);
            put32(b, at + 4, u32(input.source.bytes(), h.start + i * 4));
            put32(b, at + 8, i ? 0 : 0x08000000U | ((h.target >> 2) & 0x3ffffff));
        }
    // Keep the pre-install directory too, so unwrapping restores the exact class
    // allocation checksummed by an earlier capacity exporter.
    put32(b, 56, b.size());
    for (quint32 i = 0; i < p.patchCount; ++i) {
        auto at = b.size();
        const auto word = u32(b, p.patches + i * 12 + 4);
        b += QByteArray(4, 0);
        put32(b, at, word);
    }
    align(b);
    p.size = b.size();
    if (quint64(address) + p.size > 0x80400000 || p.size > 0x80000)
        throw Error("Movement runtime exceeds permanent game storage");
    return p;
}
void installMovementRuntime(QByteArray &image, const MovementPlacement &built) {
    auto p = built;
    const auto cls = ClassPlacement::read(Rom::fromBytes(image));
    if (!cls)
        throw Error("Movement runtime needs its class allocation");
    if (image.mid(p.descriptorOffset, p.descriptorSize) != QByteArray(p.descriptorSize, char(-1)))
        throw Error("Movement descriptor overlaps unrelated data");
    p.originalDataCrc = u32(image, ClassPlacement::descriptorRomOffset + 52);
    p.originalClassHeaderCrc = u32(image, ClassPlacement::descriptorRomOffset + 124);
    // Other supported exporters can relocate the original entry operands or
    // install an expanded-capacity entry. Preserve those exact words for reopening.
    for (quint32 i = 0; i < p.patchCount; i++) {
        const auto a = p.patches + i * 12, at = u32(p.data, a);
        put32(p.data, a + 4, u32(image, at));
        put32(image, at, u32(p.data, a + 8));
    }
    image.replace(p.offset, p.size, p.data);
    image.replace(p.descriptorOffset, p.descriptorSize, p.descriptor());
    const auto data = image.mid(cls->dataRomOffset, cls->layout.end - cls->layout.indexBase);
    const auto code = image.mid(ClassPlacement::codeRomOffset, cls->codeBytes);
    image.replace(ClassPlacement::descriptorRomOffset, ClassPlacement::descriptorSize,
                  cls->descriptor(data, code));
}
} // namespace lordly
