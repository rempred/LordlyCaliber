#include "rebuild/persuasion.h"
namespace lordly {
QByteArray rebuildPersuasion(const Rom &rom, const Persuasion &table, const Compiler &compiler, const QString &directory) {
    const auto prior = Persuasion::load(rom);
    if (!prior.unavailable().isEmpty()) throw Error(prior.unavailable());
    if (!table.unavailable().isEmpty()) throw Error(table.unavailable());
    const auto expected = table.storage();
    const bool reuse = prior.installedOffset() && expected.size() <= prior.installedSize();
    const quint32 offset = reuse ? prior.installedOffset() : quint32((rom.bytes().size() + 15) & ~qsizetype(15));
    const auto expectedCode = table.owner(offset, expected.size());
    const auto data = compiler.compileData(expected, directory, "persuasion_table");
    const auto code = compiler.assembleOwner(table.assembly(offset,expected.size()), directory,
                                              "persuasion_lookup", Persuasion::ownerAddress, {}, true);
    if (data != expected || code != expectedCode) throw Error("Persuasion compilation differs from the bounded source.");
    auto image = rom.bytes();
    if (prior.installedOffset())
        image.replace(prior.installedOffset(), prior.installedSize(), QByteArray(prior.installedSize(),char(-1)));
    if (quint64(offset)+data.size()>quint64(image.size())) image.append(QByteArray(offset+data.size()-image.size(),char(-1)));
    image.replace(offset,data.size(),data); image.replace(Persuasion::ownerOffset,code.size(),code);
    const auto reopened=Persuasion::load(Rom::fromBytes(image));
    if (!reopened.unavailable().isEmpty() || reopened.json()!=table.json()) throw Error("Persuasion export failed readback.");
    return image;
}
}
