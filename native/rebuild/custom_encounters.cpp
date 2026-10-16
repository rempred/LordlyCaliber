#include "rebuild/encounter_owners.h"
#include "engine/art_resource_links.h"
#include "rebuild/custom_encounters.h"
#include "engine/class_data.h"
#include "engine/creature_rewards.h"
#include "engine/scenario_rates.h"
#include <QFile>
#include <QJsonArray>
namespace lordly {
QByteArray removeCustomInstallation(const Rom &rom, const CustomPlacement &previous,
                                    const Compiler &compiler, const QString &directory) {
    const auto actual = CustomPlacement::read(rom);
    if (!actual || actual->dataOffset != previous.dataOffset ||
        actual->dataSize != previous.dataSize || actual->entries != previous.entries)
        throw Error("Custom encounter removal disagrees with its installed allocation");
    auto image = rebuildCustomCallers(rom, compiler, directory, {}, actual);
    image.replace(CustomPlacement::descriptorOffset, CustomPlacement::descriptorSize,
                  QByteArray(CustomPlacement::descriptorSize, char(-1)));
    if (quint64(previous.dataOffset) + previous.dataSize == quint64(image.size()))
        image.truncate(previous.dataOffset);
    else
        image.replace(previous.dataOffset, previous.dataSize,
                      QByteArray(previous.dataSize, char(-1)));
    return image;
}
QByteArray installCustomEncounters(const Rom &rom, const CustomSquads &squads,
                                   const CustomRuntimeBuild &runtime, const Compiler &compiler,
                                   const QString &directory,
                                   const std::optional<CustomPlacement> &previous,
                                   const QByteArray &poolPairs, int poolSlots) {
    const auto at = runtime.indexRomOffset;
    auto expected = squads.bytes(at, poolSlots);
    if (poolSlots > 10) {
        if (poolPairs.size() != 45 * poolSlots * 2) throw Error("Expanded encounter pairs are incomplete");
        expected += poolPairs;
        while (expected.size() % 4) expected.append(char(0));
    }
    if (expected.isEmpty())
        throw Error("Cannot install an empty custom squad library");
    const auto classes = ClassPlacement::read(rom);
    if (!classes || classes->residentAddress != runtime.code.address ||
        classes->residentBytes != runtime.bytes.size() ||
        rom.bytes().mid(runtime.code.address - ClassPlacement::residentDelta,
                        runtime.bytes.size()) != runtime.bytes ||
        runtime.stateAddress != runtime.code.address + runtime.code.bytes.size())
        throw Error("Custom encounter runtime was not reserved and linked by the class rebuild");
    squads.validateFormations(ClassData::load(rom));
    if (rom.bytes().mid(CustomPlacement::descriptorOffset, CustomPlacement::descriptorSize) !=
        QByteArray(CustomPlacement::descriptorSize, char(-1)))
        throw Error("Custom encounter descriptor allocation is occupied");
    if (at < quint64(rom.bytes().size()) &&
        (!previous || at != previous->dataOffset || expected.size() > previous->dataSize ||
         rom.bytes().mid(at, expected.size()) != QByteArray(expected.size(), char(-1))))
        throw Error("Custom squad library would overwrite unrelated cartridge data");
    const auto classEnd =
        quint64(classes->dataRomOffset) + classes->layout.end - classes->layout.indexBase;
    if (at < classEnd && quint64(at) + expected.size() > classes->dataRomOffset)
        throw Error("Custom squad library overlaps rebuilt classes");
    const auto data = compiler.compileData(expected, directory, "custom_squad_library");
    if (data != expected)
        throw Error("Compiled custom squads disagree with their source");
    CustomPlacement placed;
    placed.slotCount = poolSlots;
    placed.dataOffset = at;
    placed.dataSize = quint32(data.size());
    placed.profileCount = quint32(squads.profiles().size());
    placed.codeAddress = runtime.code.address;
    placed.codeBytes = quint32(runtime.code.bytes.size());
    placed.residentBytes = quint32(runtime.bytes.size());
    placed.stateAddress = runtime.stateAddress;
    for (const auto &name : customRuntimeEntries())
        placed.entries[name] = runtime.code.entries.value(name);
    auto image = rebuildCustomCallers(rom, compiler, directory, placed.entries);
    if (quint64(at) + data.size() > quint64(image.size()))
        image.append(QByteArray(at + data.size() - image.size(), char(-1)));
    image.replace(at, data.size(), data);
    image.replace(CustomPlacement::descriptorOffset, CustomPlacement::descriptorSize,
                  placed.descriptor(data, runtime.bytes));
    const auto reopened = Rom::fromBytes(image);
    verifyCustomCallers(reopened, CustomPlacement::read(reopened));
    if (CustomSquads::load(reopened, classes->layout.count).json() != squads.json())
        throw Error("Custom squad export failed readback");
    return image;
}
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
        const auto at = owner["start"].toInt(), size = owner["size"].toInt();
        QByteArray code;
        if (owner["acceptedC"].toBool()) {
            code = compiler.compileAcceptedOwner(at, directory, "custom_caller_" + name);
            // Card-message's final word is the next physical entry, outside
            // its accepted C allocation. The caller verifier authenticated it.
            if (code.size() < size) code += rom.bytes().mid(at + code.size(), size - code.size());
            if (!entries.isEmpty())
                for (const auto &s : owner["sites"].toArray()) {
                    const auto site = s.toObject();
                    const int offset = site["offset"].toInt();
                    const auto original = site["original"].toArray();
                    if (offset < 0 || offset + 8 > code.size() || original.size() != 2 ||
                        u32(code, offset) != quint32(original[0].toInteger()) ||
                        u32(code, offset + 4) != quint32(original[1].toInteger()) || site["keepDelay"].toBool())
                        throw Error("C custom encounter hook no longer matches its qualified site");
                    const auto patch = compiler.assembleOwner(
                        "jal " + site["entry"].toString().toLatin1() + "\nnop\n", directory,
                        "custom_hook_" + name + QString::number(offset),
                        quint32(owner["address"].toInteger()) + offset, symbols, true);
                    if (patch.size() != 8) throw Error("Custom encounter call exceeds its hook");
                    code.replace(offset, 8, patch);
                }
        } else {
            QFile file(":/game/encounters/custom-owners/" + name + ".s");
            if (!file.open(QIODevice::ReadOnly)) throw Error("Missing custom caller source: " + name);
            code = compiler.assembleOwner(QByteArray(".equ LC_CUSTOM_SQUADS,") +
                                               (entries.isEmpty() ? "0\n" : "1\n") + file.readAll(),
                                           directory, "custom_caller_" + name,
                                           quint32(owner["address"].toDouble()), symbols, true);
        }
        if (code.size() != size)
            throw Error("Custom caller source exceeds its original function");
        if (classes)
            code = classes->placedReferences(code, at);
        image.replace(at, size, retainArtReferences(rom, code, at));
    }
    auto compile = [&](const QString &name, const QByteArray &definitions, int offset, int size) {
        const auto code = compileEncounterOwner(compiler, name, definitions, directory,
                                                 "custom_shared_" + name, entries);
        if (code.size() != size)
            throw Error("Shared custom encounter caller exceeds its original function");
        image.replace(offset, size, retainArtReferences(rom, code, offset));
    };
    compile("dispatcher",
            scenario ? ScenarioRates::definitions(scenario->offset) : rate.assemblyDefinitions(),
            EncounterRate::romOffset,
            EncounterRate::codeSize);
    compile("rewards", CreatureRewards::definitions(*reward), CreatureRewards::ownerOffset, CreatureRewards::ownerSize);
    return image;
}
QMap<QString, quint32> customEncounterServices() {
    return {{"lc_staged_scenario", 0x8018f481},
            {"lc_character_records", 0x80195560},
            {"lc_source_records", 0x801971f0},
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
                                      quint32 address, quint32 capacity, quint32 indexRomOffset, int poolSlots, int profileCount,
                                      const QMap<QString,quint32> &capacitySymbols) {
    if (poolSlots < 10 || poolSlots > 32 || profileCount < 0 || profileCount > 256 * poolSlots ||
        address < ClassPlacement::residentBegin || address % 16 || capacity % 16 ||
        capacity < CustomRuntimeBuild::stateSize ||
        quint64(address) + capacity > ClassPlacement::residentEnd || indexRomOffset % 16 ||
        indexRomOffset < 0x2800000 || quint64(indexRomOffset) + 256 * poolSlots * 4 + profileCount * 128 +
            (poolSlots > 10 ? ((45 * poolSlots * 2 + 3) & ~3) : 0) > 0x4000000)
        throw Error("Custom encounter runtime requires bounded resident and ROM allocations");
    auto read = [](const QString &name) {
        QFile file(":/game/encounters/" + name);
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing maintained custom encounter source: " + name);
        return file.readAll();
    };
    auto source = QString("#define LC_POOL_SLOTS %1\n#define LC_POOL_PAIRS %2U\n")
                      .arg(poolSlots).arg(poolSlots > 10 ? indexRomOffset + 256 * poolSlots * 4 + profileCount * 128 : 0)
                      .toLatin1() + read("custom.c");
    if(!capacitySymbols.isEmpty())source="#define LC_ENEMY_CAPACITY 1\n"+source;
    // One translation unit gives C and register adapters ordinary linked symbols.
    // Keep the editable assembly readable as a separate resource, not encoded bytes.
    auto adapter = read("custom-sites.s");
    adapter.replace("\\", "\\\\");
    adapter.replace("\"", "\\\"");
    adapter.replace("\r", "");
    adapter.replace("\n", "\\n\"\n\"");
    source += "\nasm(\"" + adapter + "\");\n";
    auto symbols = customEncounterServices();
    if(!capacitySymbols.isEmpty()) {
        symbols.insert(capacitySymbols);
        symbols["lc_character_records"]=capacitySymbols.value("lc_enemy_characters");
        symbols["lc_source_records"]=capacitySymbols.value("lc_capacity_sources");
    }
    symbols["lc_custom_state"] = address + capacity - CustomRuntimeBuild::stateSize;
    symbols["lc_custom_index"] = indexRomOffset;
    QMap<QString, QByteArray> headers{{"custom.h", read("custom.h")}};
    if(!capacitySymbols.isEmpty()) {
        QFile header(":/game/scenarios/enemy-capacity.h");
        if(!header.open(QIODevice::ReadOnly))throw Error("Missing capacity encounter header");
        headers["enemy-capacity.h"]=header.readAll();
    }
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
    return {code, bytes, address + size, indexRomOffset};
}
} // namespace lordly
