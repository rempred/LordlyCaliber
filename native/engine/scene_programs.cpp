#include "engine/scene_programs.h"
namespace lordly {
ScenePrograms ScenePrograms::load(const Rom &rom) {
    return ScenePrograms(Directors::load(rom));
}
const DirectorResource &ScenePrograms::resource(quint32 key) const {
    const auto primary = original_.entries().constFind(key);
    if (primary != original_.entries().cend())
        return primary.value();
    const auto continuation = original_.continuations().constFind(key);
    if (continuation != original_.continuations().cend())
        return continuation.value();
    throw Error("Scene resource is not present in the loaded ROM");
}
DirectorProgram ScenePrograms::program(quint32 key) const {
    const auto changed = changes_.constFind(key);
    return changed == changes_.cend() ? resource(key).program
                                      : DirectorProgram::decode(changed.value());
}
void ScenePrograms::replace(quint32 key, const QByteArray &bytes) {
    const auto &source = resource(key).program;
    const auto parsed = DirectorProgram::decode(bytes);
    if (parsed.commands.last().opcode != 0x80000001 ||
        parsed.resourceClass() != source.resourceClass())
        throw Error("Scene edits must retain their terminal command and resource class");
    if (bytes == source.source)
        changes_.remove(key);
    else
        changes_[key] = bytes;
}
void ScenePrograms::setOperand(quint32 key, int command, int operand, quint32 value) {
    auto next = program(key);
    if (command < 0 || command >= next.commands.size() || operand < 0 ||
        operand >= next.commands[command].operands.size())
        throw Error("Scene operand is outside its command");
    next.commands[command].operands[operand] = value;
    replace(key, next.encode());
}
void ScenePrograms::insertCommand(quint32 key, int before, quint32 opcode) {
    auto next = program(key);
    const auto definition = Directors::grammar().constFind(opcode);
    if (definition == Directors::grammar().cend() || opcode == 0x80000001 || before < 0 ||
        before >= next.commands.size())
        throw Error("Choose a known command and insert before the terminal command");
    next.commands.insert(before, {0, opcode, QVector<quint32>(definition->words - 1, 0)});
    replace(key, next.encode());
}
void ScenePrograms::removeCommand(quint32 key, int command) {
    auto next = program(key);
    if (command < 0 || command >= next.commands.size() - 1)
        throw Error("The terminal scene command cannot be removed");
    next.commands.removeAt(command);
    replace(key, next.encode());
}
void ScenePrograms::restore(quint32 key) {
    resource(key);
    changes_.remove(key);
}
QJsonObject ScenePrograms::json() const {
    QJsonObject entries;
    for (auto it = changes_.cbegin(); it != changes_.cend(); ++it)
        entries[QString::number(it.key())] = QString::fromLatin1(it.value().toHex());
    return {{"version", 1}, {"programs", entries}};
}
void ScenePrograms::apply(const QJsonObject &value) {
    if (value["version"] != 1 || !value["programs"].isObject())
        throw Error("Invalid saved scene programs");
    auto next = *this;
    next.changes_.clear();
    const auto programs = value["programs"].toObject();
    for (auto it = programs.cbegin(); it != programs.cend(); ++it) {
        bool valid = false;
        const auto key = it.key().toUInt(&valid);
        const auto hex = it.value().toString().toLatin1(), bytes = QByteArray::fromHex(hex);
        if (!valid || QString::number(key) != it.key() || !it.value().isString() ||
            bytes.toHex() != hex.toLower())
            throw Error("Invalid saved scene program bytes");
        next.replace(key, bytes);
    }
    *this = std::move(next);
}
} // namespace lordly
