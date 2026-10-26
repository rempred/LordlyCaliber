#include "rebuild/weapon_types.h"
#include "engine/item_data.h"
#include "engine/shop_runtime.h"
#include <QFile>
#include <QJsonArray>

namespace lordly {
namespace {
struct Owner { quint32 offset, address; int size; };
const Owner owners[] = {{0x458f0, 0x8016f9f0, 0x44}, {0x45934, 0x8016fa34, 0x11c},
                        {0x19b1b0, 0x80218950, 0x6c}, {0x19b800, 0x80218fa0, 0x2b4}};
void verifyOriginals(const ItemData &data) {
    const auto originals = data.weaponTypes().originalOwners();
    if (originals.size() != 4) throw Error("Incomplete original weapon-type owners");
    const auto catalog = classRelocationCatalog()["owners"].toArray();
    for (const auto &owner : owners) {
        QByteArray expected;
        for (const auto &row : catalog) {
            const auto value = row.toObject();
            if (value["start"].toInteger() == owner.offset)
                expected = value["retailSha256"].toString().toLatin1();
        }
        const auto original = originals.value(owner.offset);
        if (original.size() != owner.size || expected.isEmpty() || sha256(original) != expected)
            throw Error("Custom weapon types require the original equipment classification routines");
    }
}
QMap<quint32, QByteArray> compiledOwners(const ItemData &data, const ClassPlacement &placement,
                                       const Compiler &compiler, const QString &directory) {
    verifyOriginals(data);
    QFile file(":/game/items/weapon-types.c");
    if (!file.open(QIODevice::ReadOnly)) throw Error("Missing custom weapon type source");
    const auto source = file.readAll();
    const auto metadataAddress = placement.layout.items + quint32(data.weaponTypesOffset());
    QMap<QString, quint32> symbols = {{"lc_item_types", placement.layout.items + 16},
        {"lc_type_groups", metadataAddress + WeaponTypes::groupOffset},
        {"lc_is_weapon", owners[0].address}, {"lc_equipment_group", owners[2].address},
        {"D_8021A010", 0x8021a010}, {"D_8021A114", 0x8021a114},
        {"D_8018C414", placement.layout.items + 20}, {"lc_special_price", 0x802194b4}};
    QMap<quint32, QByteArray> result;
    for (int i = 0; i < 4; ++i) {
        const auto &owner = owners[i];
        auto bindings = symbols;
        if (i == 0) bindings.remove("lc_is_weapon");
        if (i == 2) bindings.remove("lc_equipment_group");
        const auto module = compiler.compileModule("#define LC_TYPE_OPERATION " + QByteArray::number(i + 1) +
            "\n" + source, directory, "weapon_type_" + QString::number(i), owner.address, {}, bindings);
        if (module.functionExtent > quint32(owner.size))
            throw Error(QString("Weapon type routine %1 needs %2 bytes; its allocation is %3")
                            .arg(i).arg(module.functionExtent).arg(owner.size));
        result[owner.offset] = module.fitOwner(owner.size);
    }
    return result;
}
void references(QByteArray &image, bool remove) {
    for (const auto &word : weaponTypeTextWords(Rom::fromBytes(image))) {
        if (u32(image, word.offset) != (remove ? word.installed : word.original))
            throw Error("Equipment category text references changed outside their rebuild");
        put32(image, word.offset, remove ? word.original : word.installed);
    }
}
}
QByteArray removeWeaponTypes(const Rom &rom, const Compiler &compiler, const QString &directory) {
    const auto placement = ClassPlacement::read(rom);
    if (!placement || !placement->layout.items) return rom.bytes();
    const auto data = ItemData::load(rom);
    if (data.weaponTypes().empty()) return rom.bytes();
    auto image = rom.bytes();
    const auto code = compiledOwners(data, *placement, compiler, directory);
    for (const auto &owner : owners) {
        if (image.mid(owner.offset, owner.size) != code[owner.offset])
            throw Error("Installed weapon type code changed outside its rebuild");
        image.replace(owner.offset, owner.size,
            placement->placedReferences(data.weaponTypes().originalOwners()[owner.offset], owner.offset));
    }
    references(image, true);
    return image;
}
void rebuildWeaponTypes(const Project &project, const Compiler &compiler, const QString &directory,
                        BuildResult &result) {
    if (project.weaponTypes().empty()) return;
    const auto rom = Rom::fromBytes(result.image);
    const auto placement = ClassPlacement::read(rom);
    if (!placement || !placement->layout.items) throw Error("Weapon types need relocated equipment data");
    const auto data = ItemData::load(rom);
    if (data.weaponTypes().names() != project.weaponTypes().names())
        throw Error("Exported weapon type names differ from the project");
    const auto code = compiledOwners(data, *placement, compiler, directory);
    for (const auto &owner : owners) {
        const auto expected = placement->placedReferences(data.weaponTypes().originalOwners()[owner.offset], owner.offset);
        if (result.image.mid(owner.offset, owner.size) != expected)
            throw Error("Another export owner overlaps equipment classification");
        result.image.replace(owner.offset, owner.size, code[owner.offset]);
    }
    references(result.image, false);
    result.report << QString("Compiled %1 named weapon types, weapon selection, shop categories and category text")
                         .arg(project.weaponTypes().names().size());
}
} // namespace lordly
