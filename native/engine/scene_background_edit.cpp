#include "engine/scene_background_edit.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
namespace lordly {
namespace {
void qualify(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Background authoring currently requires US revision zero");
    QFile file(":/game/director/background-owners.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing background launch source identities");
    for (const auto &value : QJsonDocument::fromJson(file.readAll()).object()["owners"].toArray()) {
        const auto row = value.toObject();
        const int at = row["rom"].toInt(), size = row["size"].toInt();
        if (at < 0 || size <= 0 || at + size > rom.bytes().size() ||
            sha256(rom.bytes().mid(at, size)) != row["sha256"].toString().toLatin1())
            throw Error("Background launch code differs from its supported source: " +
                        row["name"].toString());
    }
}
} // namespace
BackgroundEdit BackgroundEdit::inspect(const Rom &rom, const DirectorProgram &program) {
    BackgroundEdit result;
    try {
        const int type = program.resourceClass();
        if (type != 1 && type != 8 && type != 4 && type != 5)
            throw Error("This scene inherits its scenery or uses a different presentation route.");
        for (int i = 0; i < program.commands.size(); ++i) {
            const auto &c = program.commands[i];
            if (c.opcode != 0x80000006)
                continue;
            if (result.command >= 0)
                throw Error(
                    "This scene has multiple background requests; edit its commands individually.");
            result.command = i;
            result.selector = qint16(c.operands.value(0));
        }
        if (result.command < 0)
            throw Error(
                "This scene does not select a background; its caller supplies the scenery.");
        if (result.selector < 0)
            throw Error("The game chooses this background from gameplay state.");
        const auto &c = program.commands[result.command];
        if (c.operands.size() != 1 || c.operands[0] != quint32(result.selector))
            throw Error("This background operand uses an unsupported encoding.");
        if (type == 4 || type == 5) {
            if (result.command != 0 &&
                !(type == 4 && result.command == 1 && program.commands[0].opcode == 0x80000007))
                throw Error("A room background must be selected by its initial setup command.");
        } else if (result.command != 0)
            throw Error("An environment must be selected by the scene's initial setup command.");
        qualify(rom);
        if (SceneImages::launch(program).environment != result.selector)
            throw Error("The scene launch scan selects a different background.");
        result.route = type == 4 || type == 5 ? SceneGroup : Environment;
    } catch (const Error &e) {
        result.route = Unavailable;
        result.reason = QString::fromUtf8(e.what());
    }
    return result;
}
QVector<BackgroundChoice> BackgroundEdit::choices(const Rom &rom) const {
    if (!editable())
        return {};
    qualify(rom);
    const quint32 root = route == SceneGroup ? 0x16b3d18 : 4;
    const qsizetype at = 0x594280 + root;
    const auto size = u32(rom.bytes(), at);
    if (size % 4 || size > 4096 || at + 4 + size > rom.bytes().size())
        throw Error("Background selector directory has invalid bounds");
    QVector<BackgroundChoice> result;
    for (int candidate = 0; candidate < int(size / 4); ++candidate) {
        try {
            auto images = route == SceneGroup ? SceneImages::group(rom, candidate)
                                              : SceneImages::environment(rom, candidate);
            if (images.isEmpty())
                continue;
            if (route == Environment)
                images << SceneImages::foreground(rom, candidate);
            images.removeDuplicates();
            QStringList names;
            for (const auto &id : images)
                names << SceneImages::catalog()[id].name;
            result << BackgroundChoice{candidate,
                                       QString("%1 %2 · %3")
                                           .arg(route == SceneGroup ? "Scene group" : "Environment")
                                           .arg(candidate)
                                           .arg(names.join(" + ")),
                                       images};
        } catch (const Error &) {
            // Unknown image layouts and empty rows remain intact, without being
            // offered as a background that this editor can visually identify.
        }
    }
    return result;
}
DirectorProgram BackgroundEdit::select(const Rom &rom, const DirectorProgram &program,
                                       int value) const {
    const auto current = inspect(rom, program);
    if (!current.editable() || current.route != route || current.command != command)
        throw Error(current.reason.isEmpty() ? "The scene's background route changed"
                                             : current.reason);
    const auto options = current.choices(rom);
    if (std::none_of(options.begin(), options.end(),
                     [&](const auto &option) { return option.selector == value; }))
        throw Error("Choose a populated background from the current ROM");
    auto result = program;
    result.commands[command].operands[0] = quint32(value);
    result = DirectorProgram::decode(result.encode());
    if (SceneImages::launch(result).environment != value)
        throw Error("Another raw launch marker overrides this background selection");
    return result;
}
} // namespace lordly
