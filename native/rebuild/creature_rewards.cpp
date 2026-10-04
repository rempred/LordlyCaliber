#include "rebuild/creature_rewards.h"
#include <QFile>
namespace lordly {
QByteArray rebuildCreatureRewards(const Rom &rom, const CreatureRewards &rewards,
                                  const Compiler &compiler, const QString &directory) {
    const auto prior = CreatureRewards::placement(rom);
    if (!prior &&
        rom.bytes().mid(CreatureRewards::descriptorOffset, CreatureRewards::descriptorSize) !=
            QByteArray(CreatureRewards::descriptorSize, char(-1)))
        throw Error("Creature reward descriptor allocation is occupied");
    const auto offset = prior ? *prior : quint32((rom.bytes().size() + 15) & ~qsizetype(15));
    if (offset < 0x2800000 || quint64(offset) + CreatureRewards::dataSize > 0x4000000)
        throw Error("Creature rewards exceed the cartridge address range");
    const auto rows = rewards.bytes();
    const auto data = compiler.compileData(rows, directory, "creature_reward_rows");
    if (data != rows)
        throw Error("Compiled creature rewards differ from their authored rows");
    QFile file(":/game/encounters/rewards.s");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing maintained creature reward source");
    const auto code = compiler.assembleOwner(
        CreatureRewards::definitions(offset) + file.readAll(), directory, "creature_reward_owner",
        CreatureRewards::ownerAddress, CreatureRewards::services(), true);
    if (code.size() != CreatureRewards::ownerSize)
        throw Error("Creature reward source exceeds its original owner");
    auto image = rom.bytes();
    if (qsizetype(offset) + data.size() > image.size())
        image.append(QByteArray(offset + data.size() - image.size(), char(-1)));
    image.replace(offset, data.size(), data);
    image.replace(CreatureRewards::ownerOffset, code.size(), code);
    QByteArray header(CreatureRewards::descriptorSize, 0);
    header.replace(0, 8, "LC05REWD");
    put32(header, 8, 1);
    put32(header, 12, offset);
    put32(header, 16, CreatureRewards::dataSize);
    put32(header, 20, CreatureRewards::count);
    put32(header, 24, CreatureRewards::rowSize);
    put32(header, 28, crc32(data));
    put32(header, 32, crc32(code));
    put32(header, 60, crc32(header.first(60)));
    image.replace(CreatureRewards::descriptorOffset, header.size(), header);
    if (CreatureRewards::load(Rom::fromBytes(image)).json() != rewards.json())
        throw Error("Creature reward rebuild failed readback");
    return image;
}
} // namespace lordly
