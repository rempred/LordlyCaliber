#include "engine/scene_programs.h"
#include "engine/dialogues.h"
#include "game/director/control_flow.h"
#include <optional>
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
void ScenePrograms::validateReferences(const Rom &rom, const Dialogues *dialogues) const {
    const auto dynamic = [](quint32 word) { return (word & 0xffffff00U) == 0x08880000U; };
    auto lookup = [](const QVector<quint32> &keys, quint32 index) {
        if (index >= quint32(keys.size()) || !keys[int(index)])
            throw Error("Reference selects an absent scene resource");
    };
    std::optional<Dialogues> fallback;
    for (auto it = changes_.cbegin(); it != changes_.cend(); ++it) {
        const auto &original = resource(it.key()).program;
        const auto edited = DirectorProgram::decode(it.value());
        auto words = [](const QByteArray &bytes) {
            QVector<quint32> result;
            for (int at = 0; at < bytes.size(); at += 4)
                result << u32(bytes, at);
            return result;
        };
        const auto originalWords = words(original.source), editedWords = words(it.value());
        auto sameReference = [&](const DirectorCommand &command, int first, int count) {
            for (const auto &prior : original.commands) {
                if (prior.opcode != command.opcode)
                    continue;
                bool same = true;
                for (int i = first; i < first + count; ++i)
                    same &= prior.operands[i] == command.operands[i];
                if (same)
                    return true;
            }
            return false;
        };
        for (const auto &command : edited.commands) {
            const auto &args = command.operands;
            try {
                if (command.opcode == 0x80000003 && !dynamic(args[0]) &&
                    !sameReference(command, 0, 1))
                    lookup(original_.selectors(), args[0]);
                if (command.opcode == 0x99 && !dynamic(args[0]) && !sameReference(command, 0, 1) &&
                    (args[0] & 255) < 254)
                    lookup(original_.continuationSelectors(), args[0] & 255);
                if (command.opcode == 0x59 && !dynamic(args[0])) {
                    int target = -1;
                    const auto now = lordly_director_find_label(
                        editedWords.constData(), int(editedWords.size()), qint32(args[0]), &target);
                    const auto before = lordly_director_find_label(originalWords.constData(),
                                                                   int(originalWords.size()),
                                                                   qint32(args[0]), &target);
                    if (now && (!before || !sameReference(command, 0, 1)))
                        throw Error("Jump label is missing from this scene");
                }
                if (command.opcode == 0xbf && !dynamic(args[1]) && !dynamic(args[2]) &&
                    !sameReference(command, 1, 2)) {
                    const quint16 selector = quint16(args[1]), entry = quint16(args[2]);
                    const auto group = entry & 0xc000;
                    if (group != 0 && group != 0x8000)
                        throw Error("Dialogue reference uses an unresolved archive table");
                    if (!dialogues) {
                        if (!fallback)
                            fallback = Dialogues::load(rom);
                        dialogues = &*fallback;
                    }
                    dialogues->entry(group ? Dialogues::Table::Alternate
                                           : Dialogues::Table::Primary,
                                     selector, entry & 0x3fff);
                }
                if (command.opcode == 0x80000006 && !dynamic(args[0]) &&
                    !sameReference(command, 0, 1) &&
                    (edited.resourceClass() == 4 || edited.resourceClass() == 5)) {
                    constexpr quint32 base = 0x594280, root = base + 0x16b3d18;
                    const auto extent = u32(rom.bytes(), root);
                    if (!extent || extent % 4 || args[0] >= extent / 4)
                        throw Error("Background group selector is outside its table");
                    const auto key = u32(rom.bytes(), root + 4 + args[0] * 4);
                    if (key) {
                        const quint64 at = quint64(key) + base;
                        if (key >= 0x10000000 || at + 4 > quint64(rom.bytes().size()))
                            throw Error("Background group key is outside the ROM");
                        const auto size = u32(rom.bytes(), at);
                        if (size % 4 || size > 4096 || at + 4 + size > quint64(rom.bytes().size()))
                            throw Error("Background group exceeds its resource");
                        for (quint32 i = 0; i < size; i += 4) {
                            const auto member = u32(rom.bytes(), at + 4 + i);
                            if (!member)
                                continue;
                            const quint64 memberAt = quint64(member) + base;
                            if (member >= 0x10000000 ||
                                memberAt + 4 > quint64(rom.bytes().size()) ||
                                memberAt + 4 + u32(rom.bytes(), memberAt) >
                                    quint64(rom.bytes().size()))
                                throw Error("Background group member exceeds its ROM allocation");
                        }
                    }
                }
            } catch (const std::exception &error) {
                throw Error(QString("Scene resource %1, word %2: %3")
                                .arg(it.key(), 8, 16, QChar('0'))
                                .arg(command.word)
                                .arg(QString::fromUtf8(error.what())));
            }
        }
    }
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
