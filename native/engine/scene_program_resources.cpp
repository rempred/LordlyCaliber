#include "engine/scene_program_resources.h"
#include "engine/legacy_scene_arena.h"
#include "engine/promotion.h"
#include "engine/scene_events.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
namespace lordly {
namespace {
constexpr quint32 base = 0x594280;
constexpr quint32 eventDirectoryBytes = 4 + SceneEvents::rowCapacity * 4;
constexpr quint32 retailEventBytes = SceneEvents::retailRowCount * 4;
// Resident event-state loader, world-program loader and the two saved-row paths.
// Each pair constructs the key for the same generic resource directory reader.
struct EventDirectoryReference {
    quint32 high, low;
};
constexpr EventDirectoryReference eventReferences[] = {
    {0x47f40, 0x47f44}, {0x480c4, 0x480cc}, {0x483a4, 0x483ac}, {0x48448, 0x48458}};
constexpr quint32 retailHigh = 0x3c05003a, retailLow = 0x34a55668;
constexpr quint32 expandedKey = SceneProgramResources::expandedEventDirectory - base;
constexpr quint32 expandedHigh = 0x3c050000 | (expandedKey >> 16),
                  expandedLow = 0x34a50000 | (expandedKey & 0xffff);
void checkEventReferences(const QByteArray &image, bool expanded) {
    for (const auto &ref : eventReferences) {
        if (u32(image, ref.high) != (expanded ? expandedHigh : retailHigh) ||
            u32(image, ref.low) != (expanded ? expandedLow : retailLow))
            throw Error("The native event directory loader has unrecognized changes");
    }
}
void changeEventDirectory(QByteArray &image, bool expanded) {
    checkEventReferences(image, !expanded);
    if (expanded && image.mid(SceneProgramResources::expandedEventDirectory, eventDirectoryBytes) !=
                        QByteArray(eventDirectoryBytes, char(-1)))
        throw Error("The expanded event directory allocation is occupied");
    for (const auto &ref : eventReferences) {
        put32(image, ref.high, expanded ? expandedHigh : retailHigh);
        put32(image, ref.low, expanded ? expandedLow : retailLow);
    }
    image.replace(SceneProgramResources::expandedEventDirectory, eventDirectoryBytes,
                  QByteArray(eventDirectoryBytes, expanded ? 0 : char(-1)));
    if (expanded)
        put32(image, SceneProgramResources::expandedEventDirectory, SceneEvents::rowCapacity * 4);
}
quint32 align(quint32 value) {
    return (value + 15U) & ~15U;
}
QVector<quint32> referenceWords(const Rom &rom) {
    QVector<quint32> result;
    const int eventCount = SceneProgramResources::eventRowCount(rom);
    for (int row = 0; row < SceneEvents::retailRowCount; ++row)
        result << SceneProgramResources::eventWord(row);
    for (const quint32 root : {0x1e24c1eU, 0x1f3ca84U}) {
        const auto extent = u32(rom.bytes(), root);
        if (!extent || extent % 4 || extent > 262144 ||
            quint64(root) + 4 + extent > quint64(rom.bytes().size()))
            throw Error("Scene resource directory exceeds the ROM");
        for (quint32 at = root + 4; at < root + 4 + extent; at += 4)
            result << at;
    }
    for (int row = SceneEvents::firstCustomRow; row < eventCount; ++row)
        result << SceneProgramResources::eventWord(row);
    return result;
}
QVector<quint32> references(const Rom &rom, quint32 key) {
    QVector<quint32> result;
    for (const auto word : referenceWords(rom))
        if (u32(rom.bytes(), word) == key)
            result << word;
    return result;
}
} // namespace
quint32 SceneProgramResources::eventWord(int row) {
    if (row < 0 || row >= SceneEvents::rowCapacity)
        throw Error("Invalid event directory row");
    return (row < SceneEvents::retailRowCount ? SceneEvents::directory : expandedEventDirectory) +
           4 + quint32(row) * 4;
}
int SceneProgramResources::eventRowCount(const Rom &rom) {
    const auto &image = rom.bytes();
    if (u32(image, SceneEvents::directory) != retailEventBytes)
        throw Error("Event directory differs from the supported layout");
    const auto header = image.mid(descriptorOffset, descriptorSize);
    if (header.first(8) != "LC05SCPT" || u32(header, 8) != 7)
        return SceneEvents::retailRowCount;
    const auto directory = image.mid(expandedEventDirectory, eventDirectoryBytes);
    if (u32(header, 92) != crc32(header.first(92)) ||
        directory.size() != eventDirectoryBytes || u32(directory, 0) != SceneEvents::rowCapacity * 4 ||
        u32(header, 36) != crc32(directory) ||
        directory.mid(4, retailEventBytes) != image.mid(SceneEvents::directory + 4, retailEventBytes))
        throw Error("The expanded event directory is damaged");
    checkEventReferences(image, true);
    return SceneEvents::rowCapacity;
}
std::optional<SceneProgramPlacement> SceneProgramResources::placement(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Scene rebuild requires US revision zero");
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (header == QByteArray(descriptorSize, char(-1)))
        return {};
    const auto version = u32(header, 8);
    const int reserved = version == 1 ? 28 : version == 7 ? 40 : 36;
    if (header.size() != descriptorSize || header.first(8) != "LC05SCPT" ||
        (version < 1 || version > 7) ||
        header.mid(reserved, 92 - reserved) != QByteArray(92 - reserved, 0) ||
        u32(header, 92) != crc32(header.first(92)))
        throw Error("Scene allocation descriptor is damaged or occupied");
    SceneProgramPlacement result{u32(header, 12), u32(header, 16), {}};
    const auto count = u32(header, 20);
    if ((!count && u32(header, 8) < 4) || count > 65536 || result.offset < 0x2800000 ||
        result.offset % 16 || !result.allocation || result.allocation % 16 ||
        quint64(result.offset) + result.allocation > 0x4000000 ||
        quint64(result.offset) + result.allocation > quint64(rom.bytes().size()) ||
        count * 20 > result.allocation)
        throw Error("Scene allocation exceeds the cartridge");
    const auto data = rom.bytes().mid(result.offset, result.allocation);
    if (crc32(data) != u32(header, 24))
        throw Error("Scene allocation payload is damaged");
    const auto allowed = referenceWords(rom);
    const QSet<quint32> allowedSet(allowed.begin(), allowed.end());
    QSet<quint32> claimed;
    quint32 referenceCursor = count * 20, previous = 0;
    for (quint32 i = 0; i < count; ++i) {
        const auto owner = u32(data, i * 20), offset = u32(data, i * 20 + 4),
                   size = u32(data, i * 20 + 8), refs = u32(data, i * 20 + 12),
                   length = u32(data, i * 20 + 16);
        if ((i && owner <= previous) || refs != referenceCursor || !length ||
            quint64(refs) + quint64(length) * 4 > quint64(data.size()) || size < 8 ||
            quint64(offset) + size > result.allocation || u32(data, offset) + quint64(4) != size)
            throw Error("Scene allocation has invalid members");
        SceneProgramMember member{offset, size, {}};
        for (quint32 r = 0; r < length; ++r) {
            const auto word = u32(data, refs + r * 4);
            if (!allowedSet.contains(word) || claimed.contains(word) || (!r && word != owner) ||
                (r && word <= member.references.last()) ||
                u32(rom.bytes(), word) != result.offset + offset - base)
                throw Error("Scene allocation resource links differ from their owner");
            member.references << word;
            claimed.insert(word);
        }
        result.members[owner] = std::move(member);
        referenceCursor += length * 4;
        previous = owner;
    }
    quint32 metadataEnd = referenceCursor;
    if (u32(header, 8) >= 2) {
        const auto offset = u32(header, 28), size = u32(header, 32);
        if (offset != referenceCursor || !size || size > 1024 * 1024 ||
            quint64(offset) + size > quint64(data.size()))
            throw Error("Authored scene metadata exceeds its allocation");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(data.mid(offset, size), &error);
        if (error.error != QJsonParseError::NoError ||
            (u32(header, 8) == 2 ? !document.isArray() : !document.isObject()))
            throw Error("Authored scene metadata is invalid");
        QSet<QString> identities;
        QSet<int> selectors;
        const auto tableBytes = u32(rom.bytes(), 0x1f3ca84);
        const auto manifest = document.object();
        if (u32(header, 8) >= 3 &&
            (manifest.size() != (u32(header, 8) >= 5 ? 4 : u32(header, 8) == 4 ? 3 : 2) || !manifest["authored"].isArray() ||
             !manifest["eventBindings"].isObject()))
            throw Error("Invalid scene allocation metadata");
        if (u32(header, 8) >= 4) {
            if (!manifest["legacyArenaRetired"].isBool())
                throw Error("Invalid browser arena retirement metadata");
            result.legacyArenaRetired = manifest["legacyArenaRetired"].toBool();
        }
        if (u32(header, 8) >= 5) {
            if (!manifest["eventFlags"].isObject()) throw Error("Invalid event flag metadata");
            result.eventFlags = manifest["eventFlags"].toObject();
            result.eventRows = SceneEvents::flagRows(result.eventFlags);
        }
        if (!count && !result.legacyArenaRetired)
            throw Error("An empty scene allocation has no retired browser arena");
        for (const auto value :
             u32(header, 8) == 2 ? document.array() : manifest["authored"].toArray()) {
            const auto entry = value.toObject();
            const auto id = entry["id"].toString(), name = entry["name"].toString();
            const auto selector = entry["selector"].toInt(-1);
            if (!value.isObject() || entry.size() != 3 ||
                !QRegularExpression("^[0-9a-f]{32}$").match(id).hasMatch() ||
                identities.contains(id) || name.trimmed().isEmpty() || name.size() > 120 ||
                selector < 0 || entry["selector"].toDouble() != selector ||
                quint32(selector) >= tableBytes / 4 || selectors.contains(selector) ||
                Directors::reservedSelectors().contains(selector))
                throw Error("Authored scene identity is invalid or duplicated");
            const auto owner = 0x1f3ca88U + quint32(selector) * 4;
            if (!result.members.contains(owner) ||
                result.members[owner].references != QVector<quint32>{owner})
                throw Error("Authored scene does not exclusively own its selector");
            identities.insert(id);
            selectors.insert(selector);
            const auto &member = result.members[owner];
            const auto decoded = decodeLzss(data.mid(member.offset + 8, member.size - 8),
                                            u32(data, member.offset + 4));
            if (DirectorProgram::decode(decoded).resourceClass() < 0)
                throw Error("Authored scene lacks its resource class");
            result.authored << AuthoredSceneBuild{id, name, selector, decoded};
        }
        const auto bindings = manifest["eventBindings"].toObject();
        for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
            const auto &site = SceneEvents::site(it.key());
            const auto target = it.value().toInt(-1);
            if (target < 0 || target > 65535 || it.value().toDouble() != target ||
                target == site.selector || quint32(target) >= tableBytes / 4 ||
                !u32(rom.bytes(), 0x1f3ca88 + target * 4))
                throw Error("Invalid event target in scene allocation");
            result.eventBindings[it.key()] = target;
            result.eventRows.insert(site.row);
        }
        for (const auto eventRow : result.eventRows) {
            const auto owner = eventWord(eventRow);
            if (!result.members.contains(owner) ||
                result.members[owner].references != QVector<quint32>{owner})
                throw Error("Event row does not exclusively own its resource link");
            const auto &member = result.members[owner];
            const auto decoded = decodeLzss(data.mid(member.offset + 8, member.size - 8),
                                            u32(data, member.offset + 4));
            if (decoded != SceneEvents::bindRow(SceneEvents::flagRow(SceneEvents::retailRow(rom, eventRow), eventRow, result.eventFlags), eventRow,
                                                result.eventBindings))
                throw Error("Event bindings differ from the rebuilt program");
        }
        metadataEnd += size;
    }
    if (version == 7) {
        bool occupied = false;
        for (int row = SceneEvents::firstCustomRow; row < SceneEvents::rowCapacity; ++row) {
            const bool present = u32(rom.bytes(), eventWord(row)) != 0;
            if (present != result.eventRows.contains(row))
                throw Error("Expanded event row differs from its scene allocation");
            occupied |= present;
        }
        if (!occupied)
            throw Error("Expanded event directory has no authored missions");
    }
    quint32 cursor = align(metadataEnd);
    for (const auto &member : result.members) {
        if (member.offset != cursor)
            throw Error("Scene allocation members overlap metadata or one another");
        cursor = align(cursor + member.size);
    }
    if (cursor != result.allocation)
        throw Error("Scene allocation has an unowned trailing region");
    for (const auto word : allowed) {
        const quint64 target = quint64(u32(rom.bytes(), word)) + base;
        if (target >= result.offset && target < quint64(result.offset) + result.allocation &&
            !claimed.contains(word))
            throw Error("Unowned resource link enters the scene allocation");
    }
    return result;
}
QByteArray
SceneProgramResources::rebuild(const Rom &rom, const QMap<quint32, QByteArray> &compiled,
                               const std::optional<QVector<AuthoredSceneBuild>> &authored,
                               const SceneEventBuild *events) {
    const auto prior = placement(rom);
    const auto legacy = prior && prior->legacyArenaRetired ? std::optional<LegacySceneArena>{}
                                                           : LegacySceneArena::read(rom);
    const bool legacyRetired = (prior && prior->legacyArenaRetired) || legacy.has_value();
    auto oldAuthored = prior ? prior->authored : QVector<AuthoredSceneBuild>{};
    if (legacy)
        oldAuthored += legacy->authored;
    if (compiled.isEmpty() && (!authored || (!prior && !legacy && authored->isEmpty())) && !events)
        return rom.bytes();
    std::optional<SceneEventBuild> migratedEvents;
    if (legacy && !events) {
        migratedEvents = SceneEvents::load(rom).plan();
        events = &*migratedEvents;
    }
    struct Member {
        QByteArray bytes;
        QVector<quint32> references;
    };
    QMap<quint32, Member> members;
    if (prior)
        for (auto it = prior->members.cbegin(); it != prior->members.cend(); ++it)
            members[it.key()] = {rom.bytes().mid(prior->offset + it->offset, it->size),
                                 it->references};
    const auto sources = Directors::load(rom);
    for (auto it = compiled.cbegin(); it != compiled.cend(); ++it) {
        if (!sources.entries().contains(it.key()) && !sources.continuations().contains(it.key()))
            throw Error("Rebuilt script is not a current Director resource");
        const auto parsed = DirectorProgram::decode(it.value());
        const auto &original = sources.entries().contains(it.key())
                                   ? sources.entries()[it.key()]
                                   : sources.continuations()[it.key()];
        if (parsed.resourceClass() != original.program.resourceClass() ||
            parsed.commands.last().opcode != 0x80000001)
            throw Error("Rebuilt script changed its terminal resource class");
        const auto links = references(rom, it.key());
        if (links.isEmpty())
            throw Error("Scene resource has no directory owner");
        const auto at = base + it.key(), size = u32(rom.bytes(), at),
                   decoded = u32(rom.bytes(), at + 4);
        qsizetype consumed = 0;
        decodeLzss(rom.bytes().mid(at + 8, size - 4), decoded, &consumed);
        const auto packed = encodeLzss(it.value());
        // Retain padding or unrecognized bytes after the old compressed stream.
        const auto trailing = rom.bytes().mid(at + 8 + consumed, size - 4 - consumed);
        QByteArray envelope(8, 0);
        put32(envelope, 0, quint32(4 + packed.size() + trailing.size()));
        put32(envelope, 4, quint32(it.value().size()));
        envelope += packed;
        envelope += trailing;
        members[links.first()] = {envelope, links};
    }
    const auto authoredScenes = authored ? *authored : oldAuthored;
    QJsonArray manifest;
    QMap<quint32, quint32> restored;
    QSet<int> removedSelectors;
    QSet<quint32> removedKeys;
    QSet<QString> identities;
    QSet<int> selectors;
    if (authored) {
        for (const auto &old : oldAuthored) {
            bool retained = false;
            for (const auto &next : authoredScenes)
                retained |= next.id == old.id && next.selector == old.selector;
            if (!retained) {
                const auto owner = 0x1f3ca88U + quint32(old.selector) * 4;
                const auto key = sources.selectors()[old.selector];
                if (compiled.contains(key))
                    throw Error("A removed scene still has a compiled replacement");
                removedSelectors.insert(old.selector);
                removedKeys.insert(key);
                members.remove(owner);
                restored[owner] = 0;
            }
        }
    }
    for (const auto &scene : authoredScenes) {
        const auto selector = scene.selector;
        if (!QRegularExpression("^[0-9a-f]{32}$").match(scene.id).hasMatch() ||
            identities.contains(scene.id) || scene.name.trimmed().isEmpty() ||
            scene.name.size() > 120 || selector < 0 || selector >= sources.selectors().size() ||
            selectors.contains(selector))
            throw Error("Invalid authored scene identity or selector");
        identities.insert(scene.id);
        selectors.insert(selector);
        const auto owner = 0x1f3ca88U + quint32(selector) * 4;
        const auto currentKey = sources.selectors()[selector];
        bool owned = false;
        for (const auto &old : oldAuthored)
            owned |= old.id == scene.id && old.selector == selector;
        if ((currentKey || Directors::reservedSelectors().contains(selector)) && !owned)
            throw Error("The new scene selector is already occupied or reserved");
        const auto programBytes =
            !authored && compiled.contains(currentKey) ? compiled[currentKey] : scene.program;
        const auto program = DirectorProgram::decode(programBytes);
        if (program.resourceClass() < 0 ||
            (owned &&
             program.resourceClass() != sources.entries()[currentKey].program.resourceClass()))
            throw Error("Authored scene requires its original terminal resource class");
        QByteArray trailing;
        if (owned) {
            const auto old =
                members.contains(owner)
                    ? members[owner].bytes
                    : rom.bytes().mid(base + currentKey, 4 + u32(rom.bytes(), base + currentKey));
            qsizetype consumed = 0;
            decodeLzss(old.mid(8), u32(old, 4), &consumed);
            trailing = old.mid(8 + consumed);
        }
        const auto packed = encodeLzss(programBytes);
        QByteArray envelope(8, 0);
        put32(envelope, 0, quint32(4 + packed.size() + trailing.size()));
        put32(envelope, 4, quint32(programBytes.size()));
        envelope += packed;
        envelope += trailing;
        members[owner] = {envelope, {owner}};
        manifest << QJsonObject{{"id", scene.id}, {"name", scene.name}, {"selector", selector}};
    }
    if (!removedSelectors.isEmpty()) {
        auto check = [&](const DirectorProgram &program) {
            for (const auto &command : program.commands)
                if (command.opcode == 0x80000003 &&
                    removedSelectors.contains(int(command.operands[0])))
                    throw Error(QString("A rebuilt scene still launches removed selector %1")
                                    .arg(command.operands[0]));
        };
        for (const auto *table : {&sources.entries(), &sources.continuations()})
            for (const auto &source : *table) {
                if (removedKeys.contains(source.key))
                    continue;
                bool owned = false;
                for (const auto &scene : authoredScenes)
                    if (sources.selectors()[scene.selector] == source.key) {
                        owned = true;
                        check(DirectorProgram::decode(scene.program));
                    }
                if (!owned)
                    check(compiled.contains(source.key)
                              ? DirectorProgram::decode(compiled[source.key])
                              : source.program);
            }
        for (const auto &scene : authoredScenes)
            check(DirectorProgram::decode(scene.program));
        const auto launchBindings = events ? events->bindings : SceneEvents::load(rom).bindings();
        for (auto it = launchBindings.cbegin(); it != launchBindings.cend(); ++it)
            if (removedSelectors.contains(it.value()))
                throw Error("An event still launches a removed scene");
    }
    const auto eventBindings = events  ? events->bindings
                               : prior ? prior->eventBindings
                                       : QMap<QString, int>{};
    const auto eventFlags = events ? events->flags : prior ? prior->eventFlags : QJsonObject{};
    QSet<int> eventRows = SceneEvents::flagRows(eventFlags);
    QJsonObject bindingMetadata;
    for (auto it = eventBindings.cbegin(); it != eventBindings.cend(); ++it) {
        const auto &site = SceneEvents::site(it.key());
        if (it.value() < 0 || it.value() >= sources.selectors().size() ||
            it.value() == site.selector || removedSelectors.contains(it.value()) ||
            (!sources.selectors()[it.value()] && !selectors.contains(it.value())))
            throw Error("Event binding targets an absent scene selector");
        eventRows.insert(site.row);
        bindingMetadata[it.key()] = it.value();
    }
    const bool hadExpanded = eventRowCount(rom) == SceneEvents::rowCapacity;
    bool expanded = false;
    for (int row : eventRows)
        expanded |= row >= SceneEvents::firstCustomRow;
    if (events) {
        QSet<int> supplied;
        for (auto it = events->rows.cbegin(); it != events->rows.cend(); ++it)
            supplied.insert(it.key());
        if (supplied != eventRows)
            throw Error("Event binding plan has missing or unowned rows");
        // Restore any recognized old binding, including a browser-owned row. Its allocation remains
        // untouched.
        const auto oldEvents = SceneEvents::load(rom);
        QSet<int> oldRows = SceneEvents::flagRows(oldEvents.flagEdits());
        for (auto it = oldEvents.bindings().cbegin(); it != oldEvents.bindings().cend(); ++it)
            oldRows.insert(SceneEvents::site(it.key()).row);
        for (int row : oldRows) {
            if (!eventRows.contains(row)) {
                const auto owner = eventWord(row);
                members.remove(owner);
                if (row < SceneEvents::firstCustomRow || expanded)
                    restored[owner] = SceneEvents::retailKey(row);
            }
        }
        for (auto it = events->rows.cbegin(); it != events->rows.cend(); ++it) {
            const auto original = SceneEvents::retailRow(rom, it.key());
            if (it.value() != SceneEvents::bindRow(SceneEvents::flagRow(original, it.key(), eventFlags), it.key(), eventBindings))
                throw Error("Compiled event program differs from its binding plan");
            const auto owner = eventWord(it.key());
            const auto key = it.key() < SceneEvents::firstCustomRow || hadExpanded
                                 ? u32(rom.bytes(), owner)
                                 : 0;
            QByteArray trailing;
            if (key) {
                const auto at = base + key, size = u32(rom.bytes(), at);
                qsizetype consumed = 0;
                decodeLzss(rom.bytes().mid(at + 8, size - 4), u32(rom.bytes(), at + 4), &consumed);
                trailing = rom.bytes().mid(at + 8 + consumed, size - 4 - consumed);
            }
            const auto packed = encodeLzss(it.value());
            QByteArray envelope(8, 0);
            put32(envelope, 0, quint32(4 + packed.size() + trailing.size()));
            put32(envelope, 4, quint32(it.value().size()));
            envelope += packed;
            envelope += trailing;
            members[owner] = {envelope, {owner}};
        }
    }
    if (members.isEmpty() && !legacyRetired) {
        auto image = rom.bytes();
        for (auto it = restored.cbegin(); it != restored.cend(); ++it)
            put32(image, it.key(), it.value());
        if (hadExpanded)
            changeEventDirectory(image, false);
        if (prior) {
            if (quint64(prior->offset) + prior->allocation == quint64(image.size()))
                image.resize(prior->offset);
            else
                image.replace(prior->offset, prior->allocation,
                              QByteArray(prior->allocation, char(-1)));
        }
        image.replace(descriptorOffset, descriptorSize, QByteArray(descriptorSize, char(-1)));
        return image;
    }
    auto allocationMetadata = QJsonObject{{"authored", manifest},
                                                    {"eventBindings", bindingMetadata},
                                                    {"legacyArenaRetired", legacyRetired}};
    if (!eventFlags.isEmpty()) allocationMetadata["eventFlags"] = eventFlags;
    const auto metadata = QJsonDocument(allocationMetadata).toJson(QJsonDocument::Compact);
    quint32 refsSize = 0;
    for (const auto &member : members)
        refsSize += quint32(member.references.size() * 4);
    quint32 refsCursor = quint32(members.size() * 20);
    const auto metadataOffset = refsCursor + refsSize;
    QByteArray payload(align(metadataOffset + quint32(metadata.size())), char(-1));
    payload.replace(metadataOffset, metadata.size(), metadata);
    int row = 0;
    for (auto it = members.cbegin(); it != members.cend(); ++it, ++row) {
        put32(payload, row * 20, it.key());
        put32(payload, row * 20 + 4, quint32(payload.size()));
        put32(payload, row * 20 + 8, quint32(it->bytes.size()));
        put32(payload, row * 20 + 12, refsCursor);
        put32(payload, row * 20 + 16, quint32(it->references.size()));
        for (const auto word : it->references) {
            put32(payload, refsCursor, word);
            refsCursor += 4;
        }
        payload += it->bytes;
        payload += QByteArray(align(quint32(payload.size())) - payload.size(), char(-1));
    }
    const quint32 allocation = quint32(payload.size());
    const bool tail =
        prior && quint64(prior->offset) + prior->allocation == quint64(rom.bytes().size());
    const quint32 offset = prior && (allocation <= prior->allocation || tail)
                               ? prior->offset
                               : align(quint32(rom.bytes().size()));
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000)
        throw Error("Rebuilt scenes exceed the cartridge");
    auto image = rom.bytes();
    if (hadExpanded != expanded)
        changeEventDirectory(image, expanded);
    if (prior)
        image.replace(prior->offset, prior->allocation, QByteArray(prior->allocation, char(-1)));
    if (tail && offset == prior->offset)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + allocation)
        image += QByteArray(offset + allocation - image.size(), char(-1));
    image.replace(offset, allocation, payload);
    for (auto it = restored.cbegin(); it != restored.cend(); ++it)
        put32(image, it.key(), it.value());
    row = 0;
    for (const auto &member : members) {
        for (const auto word : member.references)
            put32(image, word, offset + u32(payload, row * 20 + 4) - base);
        ++row;
    }
    if (expanded)
        image.replace(expandedEventDirectory + 4, retailEventBytes,
                      image.mid(SceneEvents::directory + 4, retailEventBytes));
    QByteArray header(descriptorSize, 0);
    header.replace(0, 8, "LC05SCPT");
    put32(header, 8, expanded ? 7 : SceneEvents::hasCampaignEdits(eventFlags) ? 6 : eventFlags.isEmpty() ? 4 : 5);
    put32(header, 12, offset);
    put32(header, 16, allocation);
    put32(header, 20, quint32(members.size()));
    put32(header, 24, crc32(payload));
    put32(header, 28, metadataOffset);
    put32(header, 32, quint32(metadata.size()));
    if (expanded)
        put32(header, 36, crc32(image.mid(expandedEventDirectory, eventDirectoryBytes)));
    put32(header, 92, crc32(header.first(92)));
    image.replace(descriptorOffset, descriptorSize, header);
    const auto reopened = Rom::fromBytes(image);
    placement(reopened);
    const auto check = Directors::load(reopened);
    for (auto it = compiled.cbegin(); it != compiled.cend(); ++it) {
        const auto links = references(rom, it.key());
        const auto key = u32(image, links.first());
        const auto &program = check.entries().contains(key) ? check.entries()[key].program
                                                            : check.continuations()[key].program;
        if (program.source != it.value())
            throw Error("Rebuilt scene failed decoded readback");
    }
    for (const auto &scene : authoredScenes) {
        const auto oldKey = sources.selectors()[scene.selector];
        const auto expected =
            !authored && compiled.contains(oldKey) ? compiled[oldKey] : scene.program;
        const auto key = check.selectors()[scene.selector];
        if (check.entries()[key].program.source != expected)
            throw Error("Authored scene failed decoded readback");
    }
    return image;
}
} // namespace lordly
