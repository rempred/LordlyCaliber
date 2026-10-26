#include "rebuild/attack_runtime.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace lordly {
namespace {
quint32 jump(quint32 address) {
    return 0x08000000u | ((address >> 2) & 0x3ffffff);
}
QByteArray resource(const QString &path) {
    QFile file(":/game/attacks/" + path);
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing attack runtime resource: " + path);
    return file.readAll();
}
QByteArray effectTargets(QByteArray bytes, bool install) {
    const auto sites =
        QJsonDocument::fromJson(resource("effect-targets.json")).object()["sites"].toArray();
    for (const auto &value : sites) {
        const auto site = value.toObject();
        const auto at = site["rom"].toInt();
        const auto original = QByteArray::fromHex(site["original"].toString().toLatin1());
        const auto patched = QByteArray::fromHex(site["patched"].toString().toLatin1());
        const auto current = bytes.mid(at, original.size());
        if (original.isEmpty() || original.size() != patched.size() ||
            (current != original && current != patched))
            throw Error(
                "An effect target-list reader contains unrecognized code; it was preserved");
        bytes.replace(at, original.size(), install ? patched : original);
    }
    return bytes;
}
QByteArray hooks(const Rom &rom, const AttackPlacement &placement, bool remove) {
    auto bytes = rom.bytes();
    if (!placement.codeSize)
        return bytes;
    const auto catalog =
        QJsonDocument::fromJson(resource("hooks.json")).object()["hooks"].toArray();
    if (placement.hookCount < 15 || placement.hookCount > quint32(catalog.size()))
        throw Error("Unrecognized attack hook directory");
    const auto directory = placement.romOffset + placement.hooks - placement.address;
    for (int i = 0; i < int(placement.hookCount); ++i) {
        const auto s = catalog[i].toObject();
        const auto words = s["words"].toArray();
        const auto at = quint32(s["rom"].toInteger()), first = quint32(words[0].toInteger()),
                   second = quint32(words[1].toInteger());
        const auto row = directory + i * 24, target = u32(bytes, row + 12);
        if (u32(bytes, row) != at || u32(bytes, row + 4) != first ||
            u32(bytes, row + 8) != second || target < placement.code ||
            target >= placement.code + placement.codeSize || (target & 3))
            throw Error("Invalid attack hook entry");
        if (u32(bytes, at) != (remove ? jump(target) : first) ||
            u32(bytes, at + 4) != (remove ? 0 : second))
            throw Error(
                QString("An attack hook overlaps unrecognized code at ROM %1").arg(at, 0, 16));
        put32(bytes, at, remove ? first : jump(target));
        put32(bytes, at + 4, remove ? second : 0);
    }
    return bytes;
}
QByteArray references(const Rom &rom, quint32 before, quint32 after) {
    QFile file(":/game/attacks/relocations.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing attack relocation catalog");
    const auto catalog = QJsonDocument::fromJson(file.readAll()).object();
    auto bytes = rom.bytes();
    QMap<quint32, quint32> patches;
    for (const auto &entry : catalog["owners"].toArray()) {
        const auto owner = entry.toObject();
        const auto start = quint32(owner["start"].toInteger());
        for (const auto &value : owner["references"].toArray()) {
            const auto ref = value.toObject();
            const auto hi = start + quint32(ref["high"].toInt()),
                       lo = start + quint32(ref["low"].toInt());
            const auto high = quint32(ref["highWord"].toInteger()),
                       low = quint32(ref["lowWord"].toInteger());
            const auto add = quint32(ref["addend"].toInt());
            const quint32 carry = low >> 26 == 13 ? 0 : 0x8000;
            const auto highBefore = (high & 0xffff0000) | ((before + add + carry) >> 16);
            const auto lowBefore = (low & 0xffff0000) | ((before + add) & 65535);
            if (u32(bytes, hi) != highBefore || u32(bytes, lo) != lowBefore)
                throw Error(
                    "An attack table reader has unrecognized instructions; its code was preserved");
            const QPair<quint32, quint32> words[] = {
                {hi, (high & 0xffff0000) | ((after + add + carry) >> 16)},
                {lo, (low & 0xffff0000) | ((after + add) & 65535)}};
            for (const auto &[at, word] : words) {
                if (patches.contains(at) && patches[at] != word)
                    throw Error("Attack references share incompatible address instructions");
                patches[at] = word;
            }
        }
    }
    for (auto it = patches.begin(); it != patches.end(); ++it)
        put32(bytes, it.key(), it.value());
    return bytes;
}
} // namespace
QByteArray compileAttackRuntime(const AttackData &attacks, AttackPlacement &p,
                                const Compiler &compiler, const QString &directory) {
    auto bytes = attacks.build(p.address);
    p.code = p.address + quint32(bytes.size());
    const auto config = QJsonDocument::fromJson(resource("hooks.json")).object();
    const auto sites = config["hooks"].toArray();
    QMap<QString, quint32> symbols;
    const auto native = config["symbols"].toObject();
    for (auto it = native.begin(); it != native.end(); ++it)
        symbols[it.key()] = quint32(it->toInteger());
    symbols["lc_attack_settings"] = p.settings;
    symbols["lc_attack_records"] = p.records;
    symbols["D_8018AA80"] = p.records + 4;
    symbols["lc_attack_scratch"] = p.address + AttackData::scratchOffset;
    for (const auto &value : sites) {
        const auto site = value.toObject();
        const auto words = site["words"].toArray();
        QByteArray trampoline(16, 0);
        put32(trampoline, 0, quint32(words[0].toInteger()));
        put32(trampoline, 4, quint32(words[1].toInteger()));
        put32(trampoline, 8, jump(quint32(site["address"].toInteger()) + 8));
        if (!site["original"].toString().isEmpty())
            symbols[site["original"].toString()] = p.address + quint32(bytes.size());
        bytes += trampoline;
    }
    p.amountOriginal = symbols.value("lc_original_amount");
    p.hooks = p.address + quint32(bytes.size());
    p.hookCount = quint32(sites.size());
    const auto directoryOffset = bytes.size();
    bytes += QByteArray((sites.size() * 24 + 15) & ~15, 0);
    QMap<QString, quint32> entries;
    for (const auto &name :
         {QString("attack-runtime"), QString("attack-visual"), QString("attack-generic"),
          QString("attack-select"), QString("attack-combined"), QString("attack-pedra")}) {
        auto unit = name;
        unit.replace('-', '_');
        const auto module = compiler.compileModule(
            (name == "attack-visual" ? resource("effect-image-draw.c") + "\n" : QByteArray{}) +
                resource(name + ".c"),
            directory, unit, p.address + quint32(bytes.size()), {}, symbols, true);
        bytes += module.bytes;
        bytes += QByteArray((16 - bytes.size() % 16) % 16, 0);
        for (auto it = module.entries.begin(); it != module.entries.end(); ++it) {
            entries[it.key()] = it.value();
            symbols[it.key()] = it.value();
        }
    }
    for (int i = 0; i < sites.size(); ++i) {
        const auto site = sites[i].toObject();
        const auto words = site["words"].toArray();
        const auto target = entries.value(site["entry"].toString());
        if (!target)
            throw Error("Missing compiled attack entry: " + site["entry"].toString());
        const auto at = directoryOffset + i * 24;
        put32(bytes, at, quint32(site["rom"].toInteger()));
        put32(bytes, at + 4, quint32(words[0].toInteger()));
        put32(bytes, at + 8, quint32(words[1].toInteger()));
        put32(bytes, at + 12, target);
        put32(bytes, at + 16, symbols.value(site["original"].toString()));
    }
    p.amountEntry = entries.value("lc_attack_amount");
    p.spellIndexEntry = entries.value("lc_attack_spell_index");
    if (!p.amountEntry || !p.spellIndexEntry)
        throw Error("Missing compiled attack entry points");
    const auto used = p.address + quint32(bytes.size()) - p.code;
    if (used > AttackPlacement::codeCapacity)
        throw Error("Attack runtime exceeds its code allocation");
    bytes += QByteArray(AttackPlacement::codeCapacity - used, 0);
    p.codeSize = AttackPlacement::codeCapacity;
    p.size = quint32(bytes.size());
    if (quint64(p.address) + p.size > 0x80400000)
        throw Error("Attack code exceeds permanent memory");
    return bytes;
}
QByteArray removeAttackReferences(const Rom &rom) {
    const auto placement = AttackPlacement::read(rom);
    return placement
               ? references(Rom::fromBytes(effectTargets(hooks(rom, *placement, true), false)),
                            placement->records, AttackData::retailBase)
               : rom.bytes();
}
QByteArray installAttackReferences(const Rom &rom, const AttackPlacement &placement) {
    // Whole-function replacements must not silently bypass an unknown patch.
    // Original-byte wrappers retain the original implementation after their entry.
    const auto catalog =
        QJsonDocument::fromJson(resource("hooks.json")).object()["hooks"].toArray();
    for (const auto &value : catalog) {
        const auto site = value.toObject();
        if (!site["original"].toString().isEmpty())
            continue;
        const int at = site["rom"].toInt(), size = site["size"].toInt();
        if (size < 8 || sha256(rom.bytes().mid(at, size)) != site["sha256"].toString().toLatin1())
            throw Error(
                QString("Cannot replace unrecognized attack code at ROM %1; its code was preserved")
                    .arg(at, 0, 16));
    }
    return effectTargets(
        hooks(Rom::fromBytes(references(rom, AttackData::retailBase, placement.records)), placement,
              false),
        true);
}
} // namespace lordly
