#include "rebuild/effect_preview_runtime.h"
#include "rebuild/compiler.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QTemporaryDir>
namespace lordly {
namespace {
QByteArray source(const QString &name) {
    QFile file(":/game/attacks/" + name);
    if (!file.open(QIODevice::ReadOnly)) throw Error("Missing effect preview source: " + name);
    return file.readAll();
}
struct Runtime {
    QVector<CompiledModule> modules;
    QByteArray trampolines;
    QMap<quint32, quint32> hooks;
    quint32 soundSubmit = 0;
};
Runtime buildRuntime() {
    auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
    if (tools.isEmpty()) tools = QCoreApplication::applicationDirPath() + "/n64-toolchain";
    if (!QDir(tools).exists()) tools = QCoreApplication::applicationDirPath() + "/../../.local/n64-toolchain";
    Compiler compiler(tools);
    compiler.authenticate();
    QTemporaryDir output;
    if (!output.isValid()) throw Error("Cannot prepare the native effect preview");
    Runtime result;
    QMap<QString, quint32> symbols;
    const auto metadata = QJsonDocument::fromJson(source("hooks.json")).object();
    const auto native = metadata["symbols"].toObject();
    for (auto i = native.begin(); i != native.end(); ++i) symbols[i.key()] = quint32(i->toInteger());
    symbols["lc_attack_settings"] = 0x80400000;
    symbols["lc_effect_state"] = 0x80402000;
    symbols["lc_attack_scratch"] = 0x80404100;
    symbols["lc_strike_state"] = 0x80425000;
    symbols["lc_audio_state"] = 0x80427000;
    const auto hooks = metadata["hooks"].toArray();
    for (const auto &value : hooks) {
        const auto h = value.toObject();
        const auto entry = h["entry"].toString();
        if (!entry.startsWith("lc_attack_visual_") && !entry.startsWith("lc_attack_edit_")) continue;
        const auto words = h["words"].toArray();
        const auto address = quint32(h["address"].toInteger());
        QByteArray code(16, 0);
        put32(code, 0, quint32(words[0].toInteger()));
        put32(code, 4, quint32(words[1].toInteger()));
        put32(code, 8, 0x08000000 | ((address + 8) >> 2 & 0x3ffffff));
        if (!h["original"].toString().isEmpty())
            symbols[h["original"].toString()] = 0x80424000 + quint32(result.trampolines.size());
        result.trampolines += code;
    }
    quint32 address = 0x80405000;
    for (const auto &name : {QString("attack-strikes"), QString("attack-audio"), QString("effect-edits"),
                             QString("attack-visual"), QString("attack-strike-driver")}) {
        auto unit = name;
        unit.replace('-', '_');
        const auto prefix = name == "effect-edits" ? source("effect-colors.c") :
                            name == "attack-audio" ? source("sound-parts.c") :
                            name == "attack-visual" ? source("effect-image-draw.c") : QByteArray{};
        auto module = compiler.compileModule(prefix + "\n" + source(name + ".c"), output.path(), unit,
                                            address, {{"battle-state.h", source("battle-state.h")}}, symbols, true);
        for (auto i = module.entries.begin(); i != module.entries.end(); ++i) symbols[i.key()] = i.value();
        address += (quint32(module.bytes.size()) + 15) & ~15U;
        if (address > 0x80424000) throw Error("Effect preview code exceeds its allocation");
        result.modules.append(std::move(module));
    }
    for (const auto &value : hooks) {
        const auto h = value.toObject();
        const auto target = symbols.value(h["entry"].toString());
        if (target) result.hooks[quint32(h["address"].toInteger())] = target;
    }
    result.soundSubmit = symbols.value("lc_audio_submit");
    return result;
}
}
void installEffectPreviewRuntime(EffectMachine &machine, const AttackVisual &visual, int inherited,
                                 const std::function<void(quint32)> &soundSubmit) {
    static QMutex mutex;
    static std::optional<Runtime> cached;
    QMutexLocker lock(&mutex);
    if (!cached) cached = buildRuntime();
    for (const auto &module : cached->modules) machine.mount(module.address, module.bytes, true);
    machine.mount(0x80424000, cached->trampolines, true);
    for (auto i = cached->hooks.begin(); i != cached->hooks.end(); ++i) {
        machine.word(i.key(), 0x08000000 | (i.value() >> 2 & 0x3ffffff));
        machine.word(i.key() + 4, 0);
    }
    const auto config = visual.build(inherited);
    if (config.size() > 0xd0000) throw Error("Effect preview artwork exceeds its allocation");
    machine.mount(0x80400000, QByteArray(0x4200, 0));
    machine.mount(0x80425000, QByteArray(4160, 0));
    machine.mount(0x80427000, QByteArray(9248, 0));
    machine.mount(0x80430000, config);
    machine.byte(0x80400000 + 55 * 32 + 21, 136);
    machine.word(0x80400000 + 55 * 32 + 24, 0x80430000);
    if (soundSubmit) soundSubmit(cached->soundSubmit);
}
}
