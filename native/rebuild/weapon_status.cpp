#include "rebuild/weapon_status.h"
#include "engine/weapon_status.h"
#include "engine/status_rules.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

namespace lordly {
QByteArray compileStatusRecovery(const RuleBuild &rule, const Compiler &compiler,
                                 const QString &directory) {
    QByteArray code;
    if (rule.offset == 0x22a964) {
        const auto canonical = canonicalWeaponRecovery(rule.expected);
        const auto hash = sha256(canonical);
        auto read = [](const QString &path) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) throw Error("Missing recovery C source: " + path);
            return file.readAll();
        };
        auto manifest = QJsonDocument::fromJson(read(":/game/items/weapon-hit.json")).object();
        if (hash == manifest["retailSha256"].toString().toLatin1())
            code = compiler.compileAcceptedOwner(rule.offset, directory, "recovery_retail_hit");
        else {
            QString source = "weapon-hit.c";
            if (hash != manifest["compiledSha256"].toString().toLatin1()) {
                // Keep the earlier Poison-only export editable without silently
                // changing its game behavior when only recovery is adjusted.
                manifest = QJsonDocument::fromJson(read(":/game/items/weapon-hit-v1.json")).object();
                source = "weapon-hit-v1.c";
            }
            if (hash != manifest["compiledSha256"].toString().toLatin1())
                throw Error("Missing qualified C for this physical-hit routine");
            QMap<QString, quint32> symbols;
            const auto bindings = manifest["symbols"].toObject();
            for (auto it = bindings.begin(); it != bindings.end(); ++it)
                symbols[it.key()] = it->toString().toUInt(nullptr, 0);
            code = compiler.compileModule(read(":/game/items/" + source), directory,
                                           "recovery_weapon_hit", rule.address, {}, symbols).fitOwner(rule.size);
            if (sha256(code) != hash) throw Error("Compiled physical-hit recovery owner changed");
        }
    } else if (rule.offset == 0x22edd4 || rule.offset == 0x22ec08)
        code = compiler.compileAcceptedOwner(rule.offset, directory, "recovery_" + rule.name);
    else throw Error("Unknown compiled recovery owner");
    return withStatusRecovery(code, rule.name, rule.recoveryChance);
}
void rebuildWeaponStatus(const Project &project, const Compiler &compiler,
                         const QString &directory, BuildResult &result) {
    bool needed = false;
    for (const auto &item : project.records(Table::Items))
        if (WeaponStatus::isWeapon(item.bytes) &&
            (u8(item.bytes, WeaponStatus::offset) & WeaponStatus::extensionMask)) needed = true;
    if (!needed) return; // Preserve installed code when the last extended status is removed.
    if (project.rom().revision() != 0)
        throw Error("Poison and Power weapons require the US revision 0 ROM");
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
    int wakeChance=-1;
    const auto hash = QString::fromLatin1(sha256(canonicalWeaponRecovery(original,&wakeChance)));
    if (hash != manifest["retailSha256"].toString() && hash != manifest["compiledSha256"].toString() &&
        !manifest["priorSha256"].toArray().contains(hash))
        throw Error("Cannot add this status: the ROM has an unrecognized physical-hit routine. Its code has been preserved.");
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
    result.image.replace(offset, size, withWeaponRecovery(code,wakeChance));
    result.report << "Compiled Poison, Power Up and Power Down weapon hits; retained existing ailments and immunity rules";
}
} // namespace lordly
