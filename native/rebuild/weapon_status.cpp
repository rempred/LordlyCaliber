#include "rebuild/weapon_status.h"
#include "engine/weapon_status.h"
#include <QFile>
#include <QJsonDocument>

namespace lordly {
void rebuildWeaponStatus(const Project &project, const Compiler &compiler,
                         const QString &directory, BuildResult &result) {
    bool needed = false;
    for (const auto &item : project.records(Table::Items))
        if (WeaponStatus::isWeapon(item.bytes) &&
            (u8(item.bytes, WeaponStatus::offset) & WeaponStatus::poison)) needed = true;
    if (!needed) return; // Existing installed code is preserved when the last Poison is removed.
    if (project.rom().revision() != 0)
        throw Error("Poison weapons require the US revision 0 ROM");
    auto read = [](const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) throw Error("Missing weapon status source: " + path);
        return file.readAll();
    };
    const auto manifest = QJsonDocument::fromJson(read(":/game/items/weapon-hit.json")).object();
    const auto offset = manifest["romOffset"].toString().toUInt(nullptr, 0);
    const auto address = manifest["ramAddress"].toString().toUInt(nullptr, 0);
    const auto size = manifest["size"].toInt();
    if (!offset || !address || size <= 0 || quint64(offset) + size > quint64(result.image.size()))
        throw Error("Invalid weapon status allocation");
    const auto original = result.image.mid(offset, size);
    const auto hash = QString::fromLatin1(sha256(original));
    if (hash != manifest["retailSha256"].toString() && hash != manifest["compiledSha256"].toString())
        throw Error("Cannot add Poison: this ROM has an unrecognized physical-hit routine. Its code has been preserved.");
    QMap<QString, quint32> symbols;
    const auto bindings = manifest["symbols"].toObject();
    for (auto it = bindings.begin(); it != bindings.end(); ++it)
        symbols[it.key()] = it.value().toString().toUInt(nullptr, 0);
    const auto module = compiler.compileModule(read(":/game/items/weapon-hit.c"), directory,
                                               "weapon_hit", address, {}, symbols);
    const auto code = module.fitOwner(size);
    // The linker inserts three leading NOPs to align this single function.
    // The original entry executes those NOPs and falls into the compiled body.
    const auto entry = module.entries.value("func_0022A964", 0);
    if (entry < address || entry - address > quint32(code.size()) ||
        code.first(entry - address) != QByteArray(entry - address, 0) ||
        sha256(code) != manifest["compiledSha256"].toString().toLatin1())
        throw Error("Compiled weapon status routine differs from its tested allocation");
    result.image.replace(offset, size, code);
    result.report << "Compiled Poison weapon hits; retained Sleep, Paralysis, Petrification and immunity rules";
}
} // namespace lordly
