#include "rebuild/encounter_owners.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
namespace lordly {
QByteArray compileEncounterOwner(const Compiler &compiler, const QString &kind,
                                 const QByteArray &definitions, const QString &directory,
                                 const QString &name, const QMap<QString, quint32> &entries) {
    if (kind != "dispatcher" && kind != "rewards") throw Error("Unknown encounter C owner");
    auto read = [](const QString &file) {
        QFile source(":/game/encounters/" + file);
        if (!source.open(QIODevice::ReadOnly)) throw Error("Missing encounter source: " + file);
        return source.readAll();
    };
    const auto owner = QJsonDocument::fromJson(read(kind + ".json")).object();
    auto code = compiler.compileAcceptedOwner(quint32(owner["start"].toInteger()), directory, name);
    if (code.size() != owner["size"].toInt()) throw Error("Encounter C allocation changed");
    QMap<QString, quint32> symbols = entries;
    const auto calls = owner["calls"].toObject();
    for (auto it = calls.begin(); it != calls.end(); ++it) symbols[it.key()] = quint32(it->toInteger());
    int end = 0;
    for (const auto &value : owner["patches"].toArray()) {
        const auto patch = value.toObject();
        const int offset = patch["offset"].toInt(), size = patch["size"].toInt();
        if (offset < end || size <= 0 || offset + size > code.size() || ((offset | size) & 3))
            throw Error("Encounter hook exceeds its qualified C owner");
        QByteArray localLabels;
        const auto labels = patch["symbols"].toObject();
        // This historical assembler cannot encode a PC-relative branch to an
        // undefined symbol. Define same-owner labels relative to this block's
        // section origin, including targets outside the replaced instructions.
        for (auto it = labels.begin(); it != labels.end(); ++it)
            localLabels += QString(".equ %1, . + %2\n").arg(it.key())
                               .arg(it->toInteger() - owner["address"].toInteger() - offset).toLatin1();
        const auto bytes = compiler.assembleOwner(
            QByteArray(".equ LC_CUSTOM_SQUADS,") + (entries.isEmpty() ? "0\n" : "1\n") +
                definitions + localLabels + read(patch["source"].toString()), directory,
            name + "_hook_" + QString::number(offset),
            quint32(owner["address"].toInteger()) + offset, symbols, true);
        if (bytes.size() != size) throw Error("Encounter hook changed instruction positions");
        code.replace(offset, size, bytes);
        end = offset + size;
    }
    return code;
}
}
