#include "rebuild/scenario_rates.h"
#include <QFile>
namespace lordly {
QByteArray rebuildScenarioRates(const Rom &rom, const ScenarioRates &rates,
                                const EncounterRate &global, const Compiler &compiler,
                                const QString &directory) {
    const auto prior = ScenarioRates::placement(rom);
    EncounterRate::load(rom);
    if (!rates.hasOverrides()) {
        QFile file(":/game/encounters/dispatcher.s");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing maintained encounter dispatcher");
        const auto code = compiler.assembleOwner(
            global.assemblyDefinitions() + file.readAll(), directory, "global_only_dispatcher",
            EncounterRate::ramAddress, EncounterRate::serviceAddresses(), true);
        if (code.size() != EncounterRate::codeSize)
            throw Error("Global dispatcher exceeds its original function");
        auto image = rom.bytes();
        image.replace(EncounterRate::romOffset, code.size(), code);
        if (prior) {
            image.replace(ScenarioRates::descriptorOffset, ScenarioRates::descriptorSize,
                          QByteArray(ScenarioRates::descriptorSize, char(-1)));
            if (qsizetype(prior->offset) + ScenarioRates::dataSize == image.size())
                image.truncate(prior->offset);
            else
                image.replace(prior->offset, ScenarioRates::dataSize,
                              QByteArray(ScenarioRates::dataSize, char(-1)));
        }
        if (EncounterRate::load(Rom::fromBytes(image)).json() != global.json())
            throw Error("Global-only rebuild failed readback");
        return image;
    }
    if (!prior && rom.bytes().mid(ScenarioRates::descriptorOffset, ScenarioRates::descriptorSize) !=
                      QByteArray(ScenarioRates::descriptorSize, char(-1)))
        throw Error("Scenario-rate descriptor allocation is occupied");
    const auto offset = prior ? prior->offset : quint32((rom.bytes().size() + 15) & ~qsizetype(15));
    if (offset < 0x2800000 || quint64(offset) + ScenarioRates::dataSize > 0x4000000)
        throw Error("Scenario rates exceed the cartridge address range");
    const auto expected = rates.bytes(global);
    const auto data = compiler.compileData(expected, directory, "scenario_rates");
    if (data != expected)
        throw Error("Compiled scenario rates disagree with the project");
    QFile file(":/game/encounters/dispatcher.s");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing maintained encounter dispatcher");
    const auto code = compiler.assembleOwner(
        ScenarioRates::definitions(offset) + file.readAll(), directory, "scenario_dispatcher",
        EncounterRate::ramAddress, EncounterRate::serviceAddresses(), true);
    if (code.size() != EncounterRate::codeSize)
        throw Error("Scenario dispatcher exceeds its original function");
    auto image = rom.bytes();
    if (qsizetype(offset) + data.size() > image.size())
        image.append(QByteArray(offset + data.size() - image.size(), char(-1)));
    image.replace(offset, data.size(), data);
    image.replace(EncounterRate::romOffset, code.size(), code);
    QByteArray header(ScenarioRates::descriptorSize, 0);
    header.replace(0, 8, "LC05RATE");
    put32(header, 8, 1);
    put32(header, 12, offset);
    put32(header, 16, ScenarioRates::dataSize);
    put32(header, 20, ScenarioRates::count);
    put32(header, 24, ScenarioRates::rowSize);
    put32(header, 28, crc32(data));
    put32(header, 32, crc32(code));
    put32(header, 36, global.divisor());
    put32(header, 40, global.normalPassCount());
    put32(header, 44, global.alternatePassCount());
    put32(header, 60, crc32(header.first(60)));
    image.replace(ScenarioRates::descriptorOffset, header.size(), header);
    const auto rebuilt = Rom::fromBytes(image);
    if (ScenarioRates::load(rebuilt).json() != rates.json() ||
        EncounterRate::load(rebuilt).json() != global.json())
        throw Error("Scenario-rate rebuild failed readback");
    return image;
}
} // namespace lordly
