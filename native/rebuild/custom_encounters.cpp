#include "rebuild/custom_encounters.h"
#include "engine/class_data.h"
#include "engine/creature_rewards.h"
#include "engine/scenario_rates.h"
#include <QFile>
#include <QJsonArray>
namespace lordly {
QByteArray rebuildCustomCallers(const Rom &rom, const Compiler &compiler, const QString &directory,
                                const QMap<QString, quint32> &entries,
                                const std::optional<CustomPlacement> &previous) {
    verifyCustomCallers(rom, previous);
    const auto rate = EncounterRate::load(rom);
    const auto scenario = ScenarioRates::placement(rom);
    const auto reward = CreatureRewards::placement(rom);
    if (!reward)
        throw Error("Custom encounters require rebuilt class rewards");
    auto image = rom.bytes();
    const auto classes = ClassPlacement::read(rom);
    for (const auto &v : customCallerCatalog()["owners"].toArray()) {
        const auto owner = v.toObject(), calls = owner["calls"].toObject();
        const auto name = owner["name"].toString();
        QMap<QString, quint32> symbols = entries;
        for (auto it = calls.begin(); it != calls.end(); ++it)
            symbols[it.key()] = quint32(it.value().toDouble());
        if (!entries.isEmpty())
            for (const auto &s : owner["sites"].toArray())
                if (!entries.contains(s.toObject()["entry"].toString()))
                    throw Error("Missing custom caller entry");
        QFile file(":/game/encounters/custom-owners/" + name + ".s");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing custom caller source: " + name);
        const auto at = owner["start"].toInt(), size = owner["size"].toInt();
        auto code = compiler.assembleOwner(QByteArray(".equ LC_CUSTOM_SQUADS,") +
                                               (entries.isEmpty() ? "0\n" : "1\n") + file.readAll(),
                                           directory, "custom_caller_" + name,
                                           quint32(owner["address"].toDouble()), symbols, true);
        if (code.size() != size)
            throw Error("Custom caller source exceeds its original function");
        if (classes)
            code = classes->placedReferences(code, at);
        image.replace(at, size, code);
    }
    auto compile = [&](const QString &name, const QByteArray &definitions, quint32 address,
                       QMap<QString, quint32> symbols, int offset, int size) {
        QFile file(":/game/encounters/" + name + ".s");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing shared encounter source: " + name);
        for (auto it = entries.begin(); it != entries.end(); ++it)
            symbols[it.key()] = it.value();
        const auto code = compiler.assembleOwner(
            QByteArray(".equ LC_CUSTOM_SQUADS,") + (entries.isEmpty() ? "0\n" : "1\n") +
                definitions + file.readAll(),
            directory, "custom_shared_" + name, address, symbols, true);
        if (code.size() != size)
            throw Error("Shared custom encounter caller exceeds its original function");
        image.replace(offset, size, code);
    };
    compile("dispatcher",
            scenario ? ScenarioRates::definitions(scenario->offset) : rate.assemblyDefinitions(),
            EncounterRate::ramAddress, EncounterRate::serviceAddresses(), EncounterRate::romOffset,
            EncounterRate::codeSize);
    compile("rewards", CreatureRewards::definitions(*reward), CreatureRewards::ownerAddress,
            CreatureRewards::services(), CreatureRewards::ownerOffset, CreatureRewards::ownerSize);
    return image;
}
QMap<QString, quint32> customEncounterServices() {
    return {{"lc_staged_scenario", 0x8018f481},
            {"lc_character_records", 0x80195560},
            {"lc_dma_read", 0x8009da50},
            {"lc_random", 0x80092a60},
            {"lc_character_create", 0x801d0840},
            {"lc_actor_at_index", 0x801c8fe8},
            {"lc_persuasion_filter_a", 0x801c8e30},
            {"lc_persuasion_filter_b", 0x801c8e9c},
            {"lc_persuasion_filter_c", 0x801c8e64},
            {"lc_persuasion_filter_d", 0x801c8e80},
            {"lc_persuasion_eligible", 0x801c8b68},
            {"lc_persuasion_random", 0x8009c7cc},
            {"lc_selection_success", 0x801e7a0c},
            {"lc_materializer_class_one", 0x801d15a8},
            {"lc_materializer_generic", 0x801d1600},
            {"lc_materializer_epilogue", 0x801d1694},
            {"lc_message_index", 0x800e7ab8},
            {"lc_persuasion_epilogue", 0x801d18e8},
            {"lc_persuasion_helper", 0x801d1480},
            {"lc_persuasion_target", 0x801c940c},
            {"lc_retreat_epilogue", 0x801d1a48},
            {"lc_round_mode", 0x801c8afc},
            {"lc_menu_iterator", 0x801d96a0},
            {"lc_menu_context", 0x801ce8c0}};
}
CustomRuntimeBuild buildCustomRuntime(const Compiler &compiler, const QString &directory,
                                      quint32 address, quint32 capacity, quint32 indexRomOffset) {
    if (address < ClassPlacement::residentBegin || address % 16 || capacity % 16 ||
        capacity < CustomRuntimeBuild::stateSize ||
        quint64(address) + capacity > ClassPlacement::residentEnd || indexRomOffset % 16 ||
        indexRomOffset < 0x2800000 || quint64(indexRomOffset) + 256 * 10 * 4 > 0x4000000)
        throw Error("Custom encounter runtime requires bounded resident and ROM allocations");
    auto read = [](const QString &name) {
        QFile file(":/game/encounters/" + name);
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing maintained custom encounter source: " + name);
        return file.readAll();
    };
    auto source = read("custom.c");
    // One translation unit gives C and register adapters ordinary linked symbols.
    // Keep the editable assembly readable as a separate resource, not encoded bytes.
    auto adapter = read("custom-sites.s");
    adapter.replace("\\", "\\\\");
    adapter.replace("\"", "\\\"");
    adapter.replace("\r", "");
    adapter.replace("\n", "\\n\"\n\"");
    source += "\nasm(\"" + adapter + "\");\n";
    auto symbols = customEncounterServices();
    symbols["lc_custom_state"] = address + capacity - CustomRuntimeBuild::stateSize;
    symbols["lc_custom_index"] = indexRomOffset;
    const QMap<QString, QByteArray> headers{{"custom.h", read("custom.h")}};
    auto code =
        compiler.compileModule(source, directory, "custom_encounters", address, headers, symbols);
    const auto size = (quint32(code.bytes.size()) + 15) & ~15U;
    if (size + CustomRuntimeBuild::stateSize > capacity)
        throw Error("Custom encounter code and state exceed the reserved resident allocation");
    symbols["lc_custom_state"] = address + size;
    code =
        compiler.compileModule(source, directory, "custom_encounters", address, headers, symbols);
    if (code.bytes.size() > size)
        throw Error("Relinking custom encounter state changed its code allocation");
    for (const auto &entry :
         {"lc_custom_select", "lc_custom_materialize", "lc_custom_reward", "lc_custom_persuade",
          "lc_custom_retreat", "lc_selection_site", "lc_materializer_site", "lc_card_message_site",
          "lc_card_text_site", "lc_persuasion_site", "lc_talk_target_site", "lc_retreat_site",
          "lc_fight_budget_site", "lc_talk_budget_site", "lc_round_site", "lc_menu_iterator_site"})
        if (!code.entries.contains(QString::fromLatin1(entry)))
            throw Error("Missing linked custom encounter entry: " + QString::fromLatin1(entry));
    const auto bytes =
        code.bytes + QByteArray(size + CustomRuntimeBuild::stateSize - code.bytes.size(), 0);
    return {code, bytes, address + size};
}
} // namespace lordly
