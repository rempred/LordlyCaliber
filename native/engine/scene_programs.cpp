#include "engine/scene_programs.h"
#include "engine/dialogues.h"
#include "engine/legacy_scene_arena.h"
#include "engine/scene_images.h"
#include "engine/scene_import.h"
#include "game/director/control_flow.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QUuid>
#include <optional>
namespace lordly {
namespace {
DirectorProgram blankSceneProgram(const DirectorProgram &initial) {
    const auto &bytes = initial.source;
    if (initial.resourceClass() == 5 && bytes.size() >= 92 && u32(bytes, 0) == 0x80000006 &&
        u32(bytes, 8) == 0x36 && u32(bytes, 40) == 0x35 &&
        bytes.last(20) == QByteArray::fromHex("0000007d800000028000000080000001ff000005"))
        return DirectorProgram::decode(bytes.first(72) + bytes.last(20));
    const auto &commands = initial.commands;
    if ((initial.resourceClass() != 1 && initial.resourceClass() != 8) || commands.isEmpty() ||
        commands.first().opcode != 0x80000006 || commands.first().operands[0] > 32767)
        throw Error(
            "Choose a room setup or a fresh environment scene with an explicit background.");
    for (int i = 1; i < commands.size(); ++i)
        if (commands[i].opcode == 0x80000006 || commands[i].opcode == 0x80000007)
            throw Error("This scene has additional presentation requests; duplicate it instead.");
    if (SceneImages::launch(initial).environment != int(commands.first().operands[0]))
        throw Error("This scene's launch scan does not match its initial environment request.");
    const auto blank = wipeSceneProgram(initial);
    if (blank.commands.size() < 2 || blank.commands[1].opcode != 0x7d)
        throw Error("A new environment scene needs a supported release exit.");
    return blank;
}
} // namespace
ScenePrograms ScenePrograms::load(const Rom &rom) {
    ScenePrograms result(Directors::load(rom));
    const auto placement = SceneProgramResources::placement(rom);
    auto authored = placement ? placement->authored : QVector<AuthoredSceneBuild>{};
    if (!placement || !placement->legacyArenaRetired)
        if (const auto legacy = LegacySceneArena::read(rom))
            authored += legacy->authored;
    for (const auto &scene : authored) {
        if (result.authored_.contains(scene.id))
            throw Error("Native and browser scene allocations claim the same identity");
        const auto key = result.original_.selectors()[scene.selector];
        const auto baseline = result.original_.entries()[key];
        result.authored_[scene.id] = {scene.id, scene.name, scene.selector, baseline,
                                      baseline.program};
    }
    result.originalAuthored_ = result.authored_;
    return result;
}
SceneId ScenePrograms::canonical(SceneId id) const {
    if (id.authoredId.isEmpty() && id.nativeKey)
        for (const auto &scene : authored_)
            if (scene.baseline.key == id.nativeKey)
                return SceneId::authored(scene.id);
    if (id.authoredId.isEmpty() && id.nativeKey)
        for (const auto &scene : originalAuthored_)
            if (scene.baseline.key == id.nativeKey)
                return SceneId::authored(scene.id);
    return id;
}
const DirectorResource &ScenePrograms::resource(SceneId id) const {
    id = canonical(id);
    if (!id.authoredId.isEmpty()) {
        const auto found = authored_.constFind(id.authoredId);
        if (found == authored_.cend())
            throw Error("Authored scene is not present in this project");
        return found->baseline;
    }
    const auto primary = original_.entries().constFind(id.nativeKey);
    if (primary != original_.entries().cend())
        return primary.value();
    const auto continuation = original_.continuations().constFind(id.nativeKey);
    if (continuation != original_.continuations().cend())
        return continuation.value();
    throw Error("Scene resource is not present in the loaded ROM");
}
DirectorProgram ScenePrograms::program(SceneId id) const {
    id = canonical(id);
    if (!id.authoredId.isEmpty()) {
        resource(id);
        return authored_[id.authoredId].current;
    }
    const auto changed = changes_.constFind(id.nativeKey);
    return changed == changes_.cend() ? resource(id).program
                                      : DirectorProgram::decode(changed.value());
}
Directors ScenePrograms::runtimeDirectors() const {
    auto result = original_;
    for (auto it = replacementNames_.cbegin(); it != replacementNames_.cend(); ++it) {
        if (result.resources_.contains(it.key())) result.resources_[it.key()].name = it.value();
        if (result.continuations_.contains(it.key())) result.continuations_[it.key()].name = it.value();
    }
    for (auto it = changes_.cbegin(); it != changes_.cend(); ++it) {
        const auto edited = DirectorProgram::decode(it.value());
        if (result.resources_.contains(it.key()))
            result.resources_[it.key()].program = edited;
        if (result.continuations_.contains(it.key()))
            result.continuations_[it.key()].program = edited;
    }
    for (const auto &scene : originalAuthored_) {
        if (authored_.contains(scene.id))
            continue;
        const auto key = result.selectors_[scene.selector];
        result.selectors_[scene.selector] = 0;
        if (!result.selectors_.contains(key))
            result.resources_.remove(key);
        else
            result.resources_[key].selectors.removeAll(scene.selector);
    }
    for (const auto &scene : authored_) {
        auto resource = scene.baseline;
        if (!resource.key) {
            resource.key = 0xf0000000U + quint32(scene.selector + 1) * 2;
            while (result.resources_.contains(resource.key) ||
                   result.continuations_.contains(resource.key))
                ++resource.key;
        }
        resource.envelope = 0; // Runtime data is not a serialized ROM resource.
        resource.name = scene.name;
        resource.program = scene.current;
        const auto key = resource.key;
        result.resources_[key] = std::move(resource);
        result.selectors_[scene.selector] = key;
    }
    return result;
}
void ScenePrograms::replace(SceneId id, const QByteArray &bytes) {
    id = canonical(id);
    const auto &source = resource(id).program;
    const auto parsed = DirectorProgram::decode(bytes);
    if (parsed.commands.last().opcode != 0x80000001 ||
        parsed.resourceClass() != source.resourceClass())
        throw Error("Scene edits must retain their terminal command and resource class");
    if (!id.authoredId.isEmpty())
        authored_[id.authoredId].current = parsed;
    else if (bytes == source.source)
        changes_.remove(id.nativeKey);
    else
        changes_[id.nativeKey] = bytes;
}
void ScenePrograms::setOperand(SceneId key, int command, int operand, quint32 value) {
    auto next = program(key);
    if (command < 0 || command >= next.commands.size() || operand < 0 ||
        operand >= next.commands[command].operands.size())
        throw Error("Scene operand is outside its command");
    next.commands[command].operands[operand] = value;
    replace(key, next.encode());
}
void ScenePrograms::insertCommand(SceneId key, int before, quint32 opcode) {
    auto next = program(key);
    const auto definition = Directors::grammar().constFind(opcode);
    if (definition == Directors::grammar().cend() || opcode == 0x80000001 || before < 0 ||
        before >= next.commands.size())
        throw Error("Choose a known command and insert before the terminal command");
    next.commands.insert(before, {0, opcode, QVector<quint32>(definition->words - 1, 0)});
    replace(key, next.encode());
}
void ScenePrograms::removeCommand(SceneId key, int command) {
    auto next = program(key);
    if (command < 0 || command >= next.commands.size() - 1)
        throw Error("The terminal scene command cannot be removed");
    next.commands.removeAt(command);
    replace(key, next.encode());
}
void ScenePrograms::restore(SceneId id) {
    id = canonical(id);
    const auto baseline = resource(id).program;
    if (!id.authoredId.isEmpty())
        authored_[id.authoredId].current = baseline;
    else {
        changes_.remove(id.nativeKey);
        replacementNames_.remove(id.nativeKey);
    }
}
QVector<SceneId> ScenePrograms::scenes() const {
    QVector<SceneId> result;
    for (const auto &scene : original_.entries())
        if (canonical(scene.key).authoredId.isEmpty())
            result << SceneId(scene.key);
    for (const auto &scene : authored_)
        result << SceneId::authored(scene.id);
    return result;
}
QVector<int> ScenePrograms::availableSelectors() const {
    QVector<int> result;
    for (int i = 0; i < original_.selectors().size(); ++i) {
        if (original_.selectors()[i] || Directors::reservedSelectors().contains(i))
            continue;
        bool claimed = false;
        for (const auto &scene : authored_)
            claimed |= scene.selector == i;
        if (!claimed)
            result << i;
    }
    return result;
}
bool ScenePrograms::canCreateBlank(SceneId source) const {
    const auto initial = program(source);
    try {
        blankSceneProgram(initial);
        return true;
    } catch (const Error &) {
        return false;
    }
}
SceneId ScenePrograms::createScene(const QString &name, SceneId source, bool blank) {
    if (name.trimmed().isEmpty() || name.size() > 120)
        throw Error("Enter a scene name of 1 to 120 characters");
    const auto free = availableSelectors();
    if (free.isEmpty())
        throw Error("No unclaimed scene selectors remain");
    auto initial = program(source);
    if (blank)
        initial = blankSceneProgram(initial);
    if (initial.resourceClass() < 0)
        throw Error("New scenes require a complete resource class");
    const auto uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
    DirectorResource baseline{0, 0, {free.first()}, name.trimmed(), initial};
    authored_[uuid] = {uuid, name.trimmed(), free.first(), baseline, initial};
    return SceneId::authored(uuid);
}
void ScenePrograms::renameScene(SceneId id, const QString &name) {
    id = canonical(id);
    resource(id);
    if (name.trimmed().isEmpty() || (!id.authoredId.isEmpty() && name.size() > 120))
        throw Error("Enter a scene name (up to 120 characters for a new authored resource)");
    if (!id.authoredId.isEmpty()) authored_[id.authoredId].name = name.trimmed();
    else if (name.trimmed() == resource(id).name) replacementNames_.remove(id.nativeKey);
    else replacementNames_[id.nativeKey] = name.trimmed();
}
QString ScenePrograms::displayName(SceneId id) const {
    id = canonical(id);
    const auto &source = resource(id);
    return !id.authoredId.isEmpty() ? authored_[id.authoredId].name : replacementNames_.value(id.nativeKey, source.name);
}
void ScenePrograms::validateRemovedSelectors(const QSet<int> &removed) const {
    if (removed.isEmpty())
        return;
    auto remaining = scenes();
    for (const auto &entry : original_.continuations())
        if (!remaining.contains(SceneId(entry.key)))
            remaining << SceneId(entry.key);
    for (const auto &id : remaining)
        for (const auto &command : program(id).commands)
            if (command.opcode == 0x80000003 && removed.contains(int(command.operands[0])))
                throw Error(QString("Scene %1, word %2 still launches removed selector %3")
                                .arg(id.label())
                                .arg(command.word)
                                .arg(command.operands[0]));
}
void ScenePrograms::removeScene(SceneId id) {
    id = canonical(id);
    const auto source = resource(id);
    if (id.authoredId.isEmpty())
        throw Error("Only authored scenes can be removed");
    auto next = *this;
    next.authored_.remove(id.authoredId);
    next.validateRemovedSelectors({source.selectors.first()});
    *this = std::move(next);
}
int ScenePrograms::authoredChanges() const {
    int count = 0;
    for (const auto &scene : originalAuthored_)
        count += !authored_.contains(scene.id);
    for (const auto &scene : authored_) {
        const auto found = originalAuthored_.constFind(scene.id);
        count += found == originalAuthored_.cend() || found->name != scene.name ||
                 found->current.source != scene.current.source;
    }
    return count;
}
QVector<AuthoredSceneBuild> ScenePrograms::authoredPlan() const {
    QVector<AuthoredSceneBuild> result;
    for (const auto &scene : authored_)
        result << AuthoredSceneBuild{scene.id, scene.name, scene.selector, scene.current.source};
    return result;
}
void ScenePrograms::validateReferences(const Rom &rom, const Dialogues *dialogues) const {
    const auto dynamic = [](quint32 word) { return (word & 0xffffff00U) == 0x08880000U; };
    auto lookup = [](const QVector<quint32> &keys, quint32 index) {
        if (index >= quint32(keys.size()) || !keys[int(index)])
            throw Error("Reference selects an absent scene resource");
    };
    QSet<int> removed;
    for (const auto &scene : originalAuthored_)
        if (!authored_.contains(scene.id))
            removed.insert(scene.selector);
    validateRemovedSelectors(removed);
    std::optional<Dialogues> fallback;
    QVector<SceneId> editedIds;
    for (auto it = changes_.cbegin(); it != changes_.cend(); ++it)
        editedIds << SceneId(it.key());
    for (const auto &scene : authored_)
        editedIds << SceneId::authored(scene.id);
    for (const auto &id : editedIds) {
        const auto original = !id.authoredId.isEmpty() && !originalAuthored_.contains(id.authoredId)
                                  ? DirectorProgram{}
                                  : resource(id).program;
        const auto edited = program(id);
        auto words = [](const QByteArray &bytes) {
            QVector<quint32> result;
            for (int at = 0; at < bytes.size(); at += 4)
                result << u32(bytes, at);
            return result;
        };
        const auto originalWords = words(original.source), editedWords = words(edited.source);
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
                    !sameReference(command, 0, 1)) {
                    bool added = false;
                    for (const auto &scene : authored_)
                        added |= quint32(scene.selector) == args[0];
                    if (!added)
                        lookup(original_.selectors(), args[0]);
                }
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
                                .arg(id.label())
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
    QJsonArray authored;
    for (const auto &scene : authored_)
        authored << QJsonObject{
            {"id", scene.id},
            {"name", scene.name},
            {"selector", scene.selector},
            {"baseline", QString::fromLatin1(scene.baseline.program.source.toHex())},
            {"program", QString::fromLatin1(scene.current.source.toHex())}};
    QJsonObject result{{"version", 1}, {"programs", entries}, {"authored", authored}};
    QJsonObject names;
    for (auto it = replacementNames_.cbegin(); it != replacementNames_.cend(); ++it) names[QString::number(it.key())] = it.value();
    if (!names.isEmpty()) result["replacementNames"] = names;
    return result;
}
void ScenePrograms::apply(const QJsonObject &value) {
    if (value["version"] != 1 || !value["programs"].isObject())
        throw Error("Invalid saved scene programs");
    auto next = *this;
    next.changes_.clear();
    next.replacementNames_.clear();
    next.authored_ = originalAuthored_;
    if (value.contains("authored")) {
        if (!value["authored"].isArray())
            throw Error("Invalid saved authored scenes");
        QSet<QString> identities;
        auto bytes = [](const QJsonValue &value) {
            const auto hex = value.toString().toLatin1(), result = QByteArray::fromHex(hex);
            if (!value.isString() || result.toHex() != hex.toLower())
                throw Error("Invalid saved authored scene bytes");
            return result;
        };
        for (const auto item : value["authored"].toArray()) {
            const auto object = item.toObject();
            const auto uuid = object["id"].toString(), name = object["name"].toString();
            const auto selector = object["selector"].toInt(-1);
            if (!item.isObject() || object.size() != 5 ||
                !QRegularExpression("^[0-9a-f]{32}$").match(uuid).hasMatch() ||
                identities.contains(uuid) || name.trimmed().isEmpty() || name.size() > 120 ||
                selector < 0 || object["selector"].toDouble() != selector)
                throw Error("Invalid saved authored scene identity");
            const auto baseline = DirectorProgram::decode(bytes(object["baseline"]));
            const auto current = DirectorProgram::decode(bytes(object["program"]));
            if (baseline.resourceClass() < 0 || current.resourceClass() != baseline.resourceClass())
                throw Error("Saved authored scene changed its terminal class");
            if (originalAuthored_.contains(uuid)) {
                const auto &original = originalAuthored_[uuid];
                if (selector != original.selector ||
                    baseline.source != original.baseline.program.source)
                    throw Error("Saved authored scene differs from its source ROM");
                next.authored_[uuid].name = name;
                next.authored_[uuid].current = current;
            } else {
                if (!next.availableSelectors().contains(selector))
                    throw Error("Saved new scene selector is occupied or reserved");
                next.authored_[uuid] = {
                    uuid, name, selector, {0, 0, {selector}, name, baseline}, current};
            }
            identities.insert(uuid);
        }
        for (const auto &scene : originalAuthored_)
            if (!identities.contains(scene.id))
                next.authored_.remove(scene.id);
    }
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
    QSet<int> removed;
    for (const auto &scene : authored_)
        if (!next.authored_.contains(scene.id))
            removed.insert(scene.selector);
    for (const auto &scene : originalAuthored_)
        if (!next.authored_.contains(scene.id))
            removed.insert(scene.selector);
    next.validateRemovedSelectors(removed);
    if (value.contains("replacementNames")) {
        if (!value["replacementNames"].isObject()) throw Error("Invalid replacement scene names");
        const auto names = value["replacementNames"].toObject();
        for (auto it = names.cbegin(); it != names.cend(); ++it) {
            bool valid = false;
            const auto key = it.key().toUInt(&valid);
            if (!valid || QString::number(key) != it.key() || !it.value().isString() || !next.canonical(key).authoredId.isEmpty())
                throw Error("Invalid replacement scene identity");
            next.renameScene(key, it.value().toString());
        }
    }
    *this = std::move(next);
}
} // namespace lordly
