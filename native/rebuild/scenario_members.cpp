#include "rebuild/scenario_members.h"
#include "engine/scenario_member_runtime.h"
#include <QFile>
namespace lordly {
QByteArray rebuildScenarioMembers(const Rom &rom, const Compiler &compiler, const QString &directory) {
    if (scenarioMemberRuntimeInstalled(rom)) return rom.bytes();
    const auto owner = scenarioMemberOwner();
    const int at = owner["start"].toInt(), size = owner["size"].toInt();
    const auto classes = ClassPlacement::read(rom);
    auto original = rom.bytes().mid(at, size);
    if (classes) original = classes->canonicalReferences(original, at);
    if (sha256(original) != QByteArray("5985a5dfc866d4effb58c0e412aa76a8e0ae8da0ef19bb8e44a6bf278c2a5e2b"))
        throw Error("Scenario member constructor has an unknown modification");
    QFile file(":/game/scenarios/squad-member.c");
    if (!file.open(QIODevice::ReadOnly)) throw Error("Missing scenario member source");
    const auto source = file.readAll();
    if (sha256(source) != owner["sourceSha256"].toString().toLatin1())
        throw Error("Scenario member source and owner catalog disagree");
    QMap<QString, quint32> symbols;
    const auto definitions = owner["symbols"].toObject();
    for (auto it = definitions.begin(); it != definitions.end(); ++it) symbols[it.key()] = quint32(it.value().toDouble());
    if (classes) symbols["lc_classes"] = classes->layout.records;
    const auto module = compiler.compileModule(source, directory, "scenario_member", quint32(owner["address"].toDouble()), {}, symbols);
    const auto code = module.fitOwner(size);
    auto image = rom.bytes();
    image.replace(at, size, code);
    if (!scenarioMemberRuntimeInstalled(Rom::fromBytes(image)))
        throw Error("Compiled scenario member constructor does not match its declared owner");
    return image;
}
}
