#include "engine/director.h"
#include "engine/director_session.h"
#include "engine/project.h"
#include "engine/promotion.h"
#include "engine/scene_program_resources.h"
#include "game/director/control_flow.h"
#include "game/director/dispatch_core.h"
#include "game/director/stream_words.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
static int definition(void *, LordlyDirectorWord word, LordlyDirectorDefinition *out) {
    const auto found = Directors::grammar().constFind(word);
    if (found == Directors::grammar().end())
        return 0;
    *out = {found->words, found->queryWords, found->requiredMode};
    return 1;
}
struct QueryFixture {
    int value = 0, calls = 0;
    bool fadePresent = false;
    int fadeState = 2;
};
static int queryService(void *context, LordlyDirectorWord opcode, const LordlyDirectorWord *words,
                        int count, int *value) {
    auto &fixture = *static_cast<QueryFixture *>(context);
    if (opcode == 0x7d || opcode == 0x7e) {
        if (fixture.fadePresent)
            fixture.fadeState = 1;
        return 1;
    }
    if (opcode != 0x57 || count != 4 || words[3] != 77)
        return 0;
    ++fixture.calls;
    *value = fixture.value;
    return 1;
}
class DirectorTests : public QObject {
    Q_OBJECT
  private slots:
    void legacySceneAllocationUpgrade() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto directory = Directors::load(rom);
            const auto source = directory.entries().first();
            auto current =
                SceneProgramResources::rebuild(rom, {{source.key, source.program.source}});
            const auto placement = *SceneProgramResources::placement(Rom::fromBytes(current));
            auto header = current.mid(SceneProgramResources::descriptorOffset, 96);
            auto payload = current.mid(placement.offset, placement.allocation);
            const auto referenceEnd = u32(header, 28), aligned = (referenceEnd + 15U) & ~15U;
            const auto shift = placement.members.first().offset - aligned;
            payload.remove(aligned, shift);
            for (quint32 i = referenceEnd; i < aligned; ++i)
                payload[i] = char(-1);
            int row = 0;
            for (const auto &member : placement.members) {
                put32(payload, row * 20 + 4, member.offset - shift);
                for (const auto word : member.references)
                    put32(current, word, placement.offset + member.offset - shift - 0x594280);
                ++row;
            }
            put32(header, 8, 1);
            put32(header, 16, quint32(payload.size()));
            put32(header, 24, crc32(payload));
            put32(header, 28, 0);
            put32(header, 32, 0);
            put32(header, 92, crc32(header.first(92)));
            current.resize(placement.offset);
            current += payload;
            current.replace(SceneProgramResources::descriptorOffset, 96, header);
            auto old = Rom::fromBytes(current);
            QVERIFY(SceneProgramResources::placement(old)->authored.isEmpty());
            const auto read = ScenePrograms::load(old);
            const auto key = read.original().selectors()[source.selectors.first()];
            QCOMPARE(read.program(key).source, source.program.source);
            const auto upgraded =
                SceneProgramResources::rebuild(old, {{key, source.program.source}});
            QCOMPARE(u32(upgraded, SceneProgramResources::descriptorOffset + 8), quint32(3));
            const auto reopened = ScenePrograms::load(Rom::fromBytes(upgraded));
            QCOMPARE(
                reopened.program(reopened.original().selectors()[source.selectors.first()]).source,
                source.program.source);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void authoredSceneIdentityAndExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and N64 toolchain required");
        try {
            const auto rom = Rom::open(path);
            auto scenes = ScenePrograms::load(rom);
            const auto available = scenes.availableSelectors();
            QVERIFY(!available.isEmpty());
            quint32 templateKey = 0;
            for (const auto &resource : scenes.original().entries()) {
                const auto &bytes = resource.program.source;
                if (resource.program.resourceClass() == 5 && bytes.size() >= 92 &&
                    u32(bytes, 0) == 0x80000006 && u32(bytes, 8) == 0x36 &&
                    u32(bytes, 40) == 0x35 &&
                    bytes.last(20) ==
                        QByteArray::fromHex("0000007d800000028000000080000001ff000005")) {
                    templateKey = resource.key;
                    break;
                }
            }
            QVERIFY(templateKey);
            const auto original = scenes.program(templateKey).source;
            const auto blank = scenes.createScene("New journey", templateKey, true);
            QVERIFY(blank.nativeKey == 0 && !blank.authoredId.isEmpty());
            QCOMPARE(scenes.program(blank).source, original.first(72) + original.last(20));
            QCOMPARE(scenes.resource(blank).selectors.first(), available.first());
            const auto copy = scenes.createScene("Copied journey", templateKey, false);
            QCOMPARE(scenes.program(copy).source, original);
            QVERIFY(copy != blank);
            scenes.setOperand(blank, 2, 0, 1234);
            scenes.renameScene(blank, "Journey \u2014 revised");
            scenes.validateReferences(rom);
            QCOMPARE(scenes.changedPrograms(), 2);
            auto project = Project::fromRom(rom);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto reopenedProject = Project::fromRom(rom);
            reopenedProject.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(reopenedProject.scenePrograms()->json(), scenes.json());
            auto invalid = scenes.json();
            auto rows = invalid["authored"].toArray();
            auto bad = rows[1].toObject();
            bad["selector"] = rows[0].toObject()["selector"];
            rows[1] = bad;
            invalid["authored"] = rows;
            const auto before = scenes.json();
            QVERIFY_THROWS_EXCEPTION(Error, scenes.apply(invalid));
            QCOMPARE(scenes.json(), before);
            scenes.removeScene(copy);
            QCOMPARE(scenes.changedPrograms(), 1);
            scenes.apply(before);
            const auto built = rebuild(project, tools, work.filePath("compile"));
            auto loadedRom = Rom::fromBytes(built.image);
            const auto placement = SceneProgramResources::placement(loadedRom);
            QCOMPARE(placement->authored.size(), 2);
            auto loaded = ScenePrograms::load(loadedRom);
            QVERIFY(!loaded.modified());
            QCOMPARE(loaded.authoredScenes()[blank.authoredId].name,
                     QString("Journey \u2014 revised"));
            QCOMPARE(loaded.program(blank).source, scenes.program(blank).source);
            QCOMPARE(loaded.program(copy).source, original);
            const auto nativeKey =
                loaded.original().selectors()[scenes.resource(blank).selectors.first()];
            QCOMPARE(loaded.canonical(nativeKey), blank);
            loaded.renameScene(blank, "Renamed after reopening");
            loaded.setOperand(nativeKey, 2, 0,
                              5678); // Native aliases resolve to the same owned scene.
            QCOMPARE(loaded.program(blank).commands[2].operands[0], quint32(5678));
            auto again = Project::fromRom(loadedRom);
            again.setScenePrograms(loaded.json());
            const auto second = rebuild(again, tools, work.filePath("recompile"));
            const auto reopened = ScenePrograms::load(Rom::fromBytes(second.image));
            QCOMPARE(reopened.authoredScenes()[blank.authoredId].name,
                     QString("Renamed after reopening"));
            QCOMPARE(reopened.program(blank).commands[2].operands[0], quint32(5678));
            QCOMPARE(reopened.program(copy).source, original);
            loaded.removeScene(blank);
            QVERIFY(!loaded.authoredScenes().contains(blank.authoredId));
            auto collision = scenes.authoredPlan();
            collision[0].selector = 0;
            QVERIFY_THROWS_EXCEPTION(Error, SceneProgramResources::rebuild(rom, {}, collision));
            // Clearing an originally occupied row does not make it available for a new owner.
            auto cleared = rom.bytes();
            put32(cleared, 0x1f3ca88, 0);
            QVERIFY(!ScenePrograms::load(Rom::fromBytes(cleared)).availableSelectors().contains(0));
            QDir parent(QFileInfo(path).absolutePath());
            QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/authored-scene-native-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            const auto exported = ScenePrograms::load(Rom::open(candidate));
            QCOMPARE(exported.program(blank).source, scenes.program(blank).source);
            qInfo().noquote() << "Authored scene export candidate:" << candidate;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void changedSceneReferences() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        quint32 key = 0;
        for (const auto &resource : scenes.original().entries())
            if (resource.program.resourceClass() == 5) {
                key = resource.key;
                break;
            }
        QVERIFY(key);
        auto replace = [&](std::initializer_list<quint32> values) {
            QByteArray bytes;
            for (auto value : values) {
                const auto at = bytes.size();
                bytes.resize(at + 4);
                put32(bytes, at, value);
            }
            scenes.replace(key, bytes);
        };
        replace({0x80000003, 0, 0x80000001, 0xff000005});
        scenes.validateReferences(rom);
        replace({0x80000003, 319, 0x80000001, 0xff000005});
        QVERIFY_THROWS_EXCEPTION(Error, scenes.validateReferences(rom));
        replace({0x80000003, 0x088800ff, 0x80000001, 0xff000005});
        scenes.validateReferences(rom); // Runtime launch substitution remains explicit.
        replace({0x99, 0, 0x80000001, 0xff000005});
        scenes.validateReferences(rom);
        replace({0x99, 254, 0x80000001, 0xff000005});
        scenes.validateReferences(rom);
        replace({0x99, 255, 0x80000001, 0xff000005});
        scenes.validateReferences(rom);
        replace({0x99, 5, 0x80000001, 0xff000005});
        QVERIFY_THROWS_EXCEPTION(Error, scenes.validateReferences(rom));
        replace({0x59, 54321, 0x80000005, 54321, 0x80000001, 0xff000005});
        scenes.validateReferences(rom);
        replace({0x59, 54321, 0x80000001, 0xff000005});
        QVERIFY_THROWS_EXCEPTION(Error, scenes.validateReferences(rom));
        replace({0x80000006, 22, 0x80000001, 0xff000005});
        scenes.validateReferences(rom);
        replace({0x80000006, 9999, 0x80000001, 0xff000005});
        QVERIFY_THROWS_EXCEPTION(Error, scenes.validateReferences(rom));
        auto dialogues = Dialogues::load(rom);
        const auto added = dialogues.appendEntry(Dialogues::Table::Primary, 0, "New dialogue@s@c");
        replace({0xbf, 0, 0, quint32(added), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80000001, 0xff000005});
        QVERIFY_THROWS_EXCEPTION(Error, scenes.validateReferences(rom));
        scenes.validateReferences(rom, &dialogues);
        scenes.setOperand(key, 0, 2, 0x8000);
        scenes.validateReferences(rom, &dialogues);
        scenes.setOperand(key, 0, 2, 0x4000);
        QVERIFY_THROWS_EXCEPTION(Error, scenes.validateReferences(rom, &dialogues));
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (!tools.isEmpty()) {
            auto project = Project::fromRom(rom);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            QVERIFY_THROWS_EXCEPTION(Error, rebuild(project, tools, work.path()));
        }
    }
    void sceneEditingAndDynamicExport() {
        QString phase = "model editing";
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and N64 toolchain required");
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadScenePrograms();
            auto edited = *project.scenePrograms();
            for (const auto &scene : edited.original().entries())
                QCOMPARE(edited.program(scene.key).encode(), scene.program.source);
            quint32 key = 0;
            int camera = -1;
            for (const auto &scene : edited.original().entries()) {
                if (scene.program.resourceClass() != 5)
                    continue;
                for (int i = 0; i < scene.program.commands.size(); ++i)
                    if (scene.program.commands[i].opcode == 0x35 ||
                        scene.program.commands[i].opcode == 0x36) {
                        key = scene.key;
                        camera = i;
                        break;
                    }
                if (camera >= 0)
                    break;
            }
            auto original = edited.program(key);
            QVERIFY(camera >= 0);
            auto expected = original.source;
            const auto value = original.commands[camera].operands[0] + 1000U;
            put32(expected, original.commands[camera].word * 4 + 4, value);
            edited.setOperand(key, camera, 0, value);
            QCOMPARE(edited.program(key).source, expected);
            edited.insertCommand(key, camera, 0x35);
            QCOMPARE(edited.program(key).commands.size(), original.commands.size() + 1);
            edited.removeCommand(key, camera);
            QCOMPARE(edited.program(key).source, expected);
            const auto snapshot = edited.json();
            QVERIFY_THROWS_EXCEPTION(Error,
                                     edited.removeCommand(key, int(original.commands.size()) - 1));
            QVERIFY_THROWS_EXCEPTION(
                Error, edited.setOperand(key, int(original.commands.size()) - 1, 0, 0xff000002));
            QVERIFY_THROWS_EXCEPTION(Error, edited.insertCommand(key, camera, 0x12345678));
            QCOMPARE(edited.json(), snapshot);
            edited.restore(key);
            QVERIFY(!edited.modified());
            edited.apply(snapshot);
            project.setScenePrograms(edited.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto reopenedProject = Project::fromRom(rom);
            reopenedProject.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(reopenedProject.scenePrograms()->json(), snapshot);
            phase = "public rebuild";
            const auto built = rebuild(project, tools, work.filePath("compile"));
            auto builtRom = Rom::fromBytes(built.image);
            const auto placement = SceneProgramResources::placement(builtRom);
            QVERIFY(placement.has_value());
            QCOMPARE(placement->offset, quint32(0x2800000));
            QCOMPARE(placement->members.size(), 1);
            const auto member = placement->members.first();
            const auto newKey = u32(built.image, member.references.first());
            const auto reopened = Directors::load(builtRom);
            QCOMPARE(reopened.entries()[newKey].program.source, expected);
            QCOMPARE(reopened.entries()[newKey].name, edited.resource(key).name);
            for (auto selector : edited.resource(key).selectors)
                QCOMPARE(reopened.selectors()[selector], newKey);
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            phase = "idempotent rebuild";
            QCOMPARE(SceneProgramResources::rebuild(builtRom, {{newKey, expected}}), built.image);
            const auto oldAt = key + 0x594280;
            qsizetype oldConsumed = 0, newConsumed = 0;
            decodeLzss(rom.bytes().mid(oldAt + 8, u32(rom.bytes(), oldAt) - 4),
                       u32(rom.bytes(), oldAt + 4), &oldConsumed);
            const auto newAt = newKey + 0x594280;
            decodeLzss(built.image.mid(newAt + 8, u32(built.image, newAt) - 4),
                       u32(built.image, newAt + 4), &newConsumed);
            QCOMPARE(
                built.image.mid(newAt + 8 + newConsumed, u32(built.image, newAt) - 4 - newConsumed),
                rom.bytes().mid(oldAt + 8 + oldConsumed,
                                u32(rom.bytes(), oldAt) - 4 - oldConsumed));
            auto grownProgram = DirectorProgram::decode(expected);
            const auto cameraTemplate = original.commands[camera];
            quint32 random = 173;
            for (int i = 0; i < 2000; ++i) {
                auto command = cameraTemplate;
                for (auto &operand : command.operands) {
                    random = random * 1664525U + 1013904223U;
                    operand = random;
                }
                grownProgram.commands.insert(grownProgram.commands.size() - 1, command);
            }
            const auto grown = grownProgram.encode();
            phase = "tail growth";
            const auto tail = SceneProgramResources::rebuild(builtRom, {{newKey, grown}});
            const auto tailPlacement = SceneProgramResources::placement(Rom::fromBytes(tail));
            QCOMPARE(tailPlacement->offset, placement->offset);
            QVERIFY(tailPlacement->allocation > placement->allocation);
            const QByteArray foreign = QByteArray("Foreign resource follows the scene allocation.")
                                           .leftJustified(64, char(0x7e));
            const auto separated = built.image + foreign;
            phase = "foreign neighbor growth";
            const auto relocated =
                SceneProgramResources::rebuild(Rom::fromBytes(separated), {{newKey, grown}});
            const auto moved = SceneProgramResources::placement(Rom::fromBytes(relocated));
            QVERIFY(moved->offset >= separated.size());
            QCOMPARE(relocated.mid(built.image.size(), foreign.size()), foreign);
            QCOMPARE(relocated.mid(placement->offset, placement->allocation),
                     QByteArray(placement->allocation, char(-1)));
            auto damaged = built.image;
            damaged[placement->offset + placement->allocation - 1] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error,
                                     SceneProgramResources::placement(Rom::fromBytes(damaged)));
            // An extra, unowned selector pointing inside owned storage must prevent reuse.
            damaged = built.image;
            put32(damaged, 0x1f3ca88 + 319 * 4, newKey);
            QVERIFY_THROWS_EXCEPTION(Error,
                                     SceneProgramResources::placement(Rom::fromBytes(damaged)));
            // All physical aliases remain shared after a rebuild.
            auto aliased = rom.bytes();
            put32(aliased, 0x1f3ca88 + 319 * 4, key);
            phase = "alias rebuild";
            const auto aliasImage =
                SceneProgramResources::rebuild(Rom::fromBytes(aliased), {{key, expected}});
            const auto aliasDirectory = Directors::load(Rom::fromBytes(aliasImage));
            QCOMPARE(aliasDirectory.selectors()[319],
                     aliasDirectory.selectors()[edited.resource(key).selectors.first()]);
            phase = "multiple resource preservation";
            const auto continuation = edited.original().continuations().first();
            const auto multiple = SceneProgramResources::rebuild(
                rom, {{key, expected}, {continuation.key, continuation.program.source}});
            const auto multipleRom = Rom::fromBytes(multiple);
            QCOMPARE(SceneProgramResources::placement(multipleRom)->members.size(), 2);
            const auto multipleDirectory = Directors::load(multipleRom);
            const auto currentPrimary =
                multipleDirectory.selectors()[edited.resource(key).selectors.first()];
            const auto currentContinuation =
                multipleDirectory.continuationSelectors()[continuation.selectors.first()];
            const auto multipleGrown =
                SceneProgramResources::rebuild(multipleRom, {{currentPrimary, grown}});
            const auto afterGrowth = Directors::load(Rom::fromBytes(multipleGrown));
            const auto retainedKey =
                afterGrowth.continuationSelectors()[continuation.selectors.first()];
            QCOMPARE(afterGrowth.continuations()[retainedKey].program.source,
                     continuation.program.source);
            QVERIFY(currentContinuation != 0);
            QDir parent(QFileInfo(path).absolutePath());
            QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/scene-native-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            QCOMPARE(Rom::open(candidate).bytes(), built.image);
            qInfo().noquote() << "Scene export candidate:" << candidate;
        } catch (const std::exception &e) {
            QFAIL(qPrintable(phase + ": " + e.what()));
        }
    }
    void launchTranslationsAreAtomic() {
        QVector<quint32> words;
        for (int i = 0; i < 256; ++i)
            words << (0x08880000u | i);
        words << 0x08880100u << 0x80000001u;
        const auto original = words;
        DirectorLaunchTranslations values;
        for (int i = 0; i < 255; ++i)
            values.set(i, quint16(i * 257));
        int missing = -1;
        QCOMPARE(lordly_director_translate(words.data(), int(words.size()), values.values.data(),
                                           values.known.data(), &missing),
                 1);
        QCOMPARE(missing, 255);
        QCOMPARE(words, original);
        values.set(255, 0);
        QCOMPARE(lordly_director_translate(words.data(), int(words.size()), values.values.data(),
                                           values.known.data(), &missing),
                 0);
        for (int i = 0; i < 255; ++i)
            QCOMPARE(words[i], quint32(i * 257));
        QCOMPARE(words[255], 0u);
        QCOMPARE(words[256], original[256]);
        QCOMPARE(words[257], original[257]);
    }
    void continuationSessionOwnership() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto bytes = Rom::open(path).bytes();
        int allocation = 0x2700000;
        auto program = [&](std::initializer_list<quint32> words) {
            QByteArray data(words.size() * 4, 0);
            int at = 0;
            for (auto word : words) {
                put32(data, at, word);
                at += 4;
            }
            const auto stored = encodeLzss(data);
            const auto key = allocation - 0x594280;
            put32(bytes, allocation, quint32(stored.size() + 4));
            put32(bytes, allocation + 4, quint32(data.size()));
            bytes.replace(allocation + 8, stored.size(), stored);
            allocation += (int(stored.size()) + 9) & ~1;
            return quint32(key);
        };
        const quint32 root0 = program(
                          {0x99, 0, 0, 0x80000000, 1, 0, 0x80000000, 0x80000001, 0xff000001}),
                      root1 = program({0x80000003, 2, 0x80000001, 0xff000001}),
                      root2 =
                          program({0x80000004, 0x08880005, 0, 0x80000000, 0x80000001, 0xff000001}),
                      child0 = program({0x99, 1, 0, 0x80000000, 0x80000001}),
                      child1 = program({0x9a, 0x80000000, 0x80000001});
        put32(bytes, 0x1f3ca84, 12);
        put32(bytes, 0x1f3ca88, root0);
        put32(bytes, 0x1f3ca8c, root1);
        put32(bytes, 0x1f3ca90, root2);
        put32(bytes, 0x594280 + 0x189099e, 8);
        put32(bytes, 0x594284 + 0x189099e, child0);
        put32(bytes, 0x594288 + 0x189099e, child1);
        const auto resources = Directors::load(Rom::fromBytes(bytes));
        DirectorSession nested(resources, 0, 2);
        QCOMPARE(nested.dispatch(), LORDLY_DIRECTOR_YIELD);
        QCOMPARE(nested.resourceKey(), root0);
        QVERIFY(!nested.isContinuation());
        QCOMPARE(nested.state().resume_cursor, 4); // Original parent, not child zero.
        QCOMPARE(nested.state().counter, 0u);
        QCOMPARE(nested.dispatch(), LORDLY_DIRECTOR_YIELD);
        QCOMPARE(nested.state().counter, 1u);
        QCOMPARE(nested.state().resume_cursor, 7);
        QCOMPARE(nested.dispatch(), LORDLY_DIRECTOR_END);
        QCOMPARE(nested.dispatch(), LORDLY_DIRECTOR_END);
        DirectorSession missing(resources, 1, 2);
        QCOMPARE(missing.dispatch(), LORDLY_DIRECTOR_TRANSFER);
        QVERIFY(missing.reason().contains("translation value 5"));
        QCOMPARE(missing.resourceKey(), root1);
        DirectorLaunchTranslations translations;
        translations.set(5, 0);
        DirectorSession replacement(resources, 1, 2, translations);
        QCOMPARE(replacement.dispatch(), LORDLY_DIRECTOR_YIELD);
        QCOMPARE(replacement.resourceKey(), root2);
        QCOMPARE(replacement.state().resume_cursor, 0);
        QCOMPARE(replacement.dispatch(), LORDLY_DIRECTOR_YIELD);
        QCOMPARE(replacement.state().resume_cursor, 4);
        QCOMPARE(replacement.dispatch(), LORDLY_DIRECTOR_END);
        DirectorSession suspended(resources, 0, 2);
        for (int i = 0; i < 16; ++i) {
            const auto result = suspended.dispatch({}, 1);
            if (result == LORDLY_DIRECTOR_YIELD)
                break;
            QCOMPARE(result, LORDLY_DIRECTOR_BUDGET);
        }
        QCOMPARE(suspended.resourceKey(), root0);
        QCOMPARE(suspended.state().resume_cursor, 4);
    }
    void recoveredDispatcherAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess generator;
        generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("director-dispatch-mips.cjs"),
                                 path, work.filePath("reference.json")});
        QVERIFY(generator.waitForFinished(30000));
        const auto errors = generator.readAllStandardError();
        QVERIFY2(generator.exitCode() == 0, errors.constData());
        QFile file(work.filePath("reference.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["rows"].toArray();
        QCOMPARE(rows.size(), 51);
        for (const auto &v : rows) {
            const auto row = v.toObject(), initial = row["initial"].toObject(),
                       expected = row["expected"].toObject();
            QVector<LordlyDirectorWord> words;
            for (const auto &w : row["words"].toArray())
                words << quint32(w.toInteger());
            LordlyDirectorDispatch state{};
            state.resume_cursor = initial["resume"].toInt();
            state.mode = initial["mode"].toInt();
            state.counter = quint32(initial["counter"].toInteger());
            state.controller = quint16(initial["controller"].toInt());
            state.barrier_state = quint8(initial["barrierState"].toInt());
            QueryFixture fixture{initial["producer"].toInt(), 0, initial["fadePresent"].toBool(),
                                 2};
            const auto status = lordly_director_dispatch(&state, words.data(), int(words.size()),
                                                         1000, definition, queryService, &fixture);
            const auto name = row["name"].toString();
            QVERIFY2(status == (expected["released"].toBool()      ? LORDLY_DIRECTOR_RELEASE
                                : expected["result"].toInt() == -1 ? LORDLY_DIRECTOR_END
                                                                   : LORDLY_DIRECTOR_YIELD),
                     qPrintable(name));
            QVERIFY2(state.resume_cursor == expected["resume"].toInt(), qPrintable(name));
            QVERIFY2(state.counter == quint32(expected["counter"].toInteger()), qPrintable(name));
            QVERIFY2(state.barrier_state == expected["barrierState"].toInt(), qPrintable(name));
            QVERIFY2(fixture.calls == expected["producerCalls"].toInt(), qPrintable(name));
            QCOMPARE(fixture.fadeState, expected["fadeState"].toInt());
        }
    }
    void boundedDispatchAndMissingServices() {
        LordlyDirectorDispatch state{};
        state.mode = 2;
        const LordlyDirectorWord camera[] = {0x36, 0, 0, 0, 0, 0, 0, 0, 0x80000001};
        QCOMPARE(lordly_director_dispatch(&state, camera, 9, 100, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_SERVICE);
        QCOMPARE(state.cursor, 0);
        QCOMPARE(state.resume_cursor, 0);
        QCOMPARE(state.counter, 0u);
        state = {};
        state.mode = 2;
        const LordlyDirectorWord counters[] = {1, 2, 1, 2, 0, 0x80000000};
        QCOMPARE(lordly_director_dispatch(&state, counters, 6, 2, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_BUDGET);
        QCOMPARE(state.cursor, 2);
        QCOMPARE(state.counter, 0u);
        QCOMPARE(lordly_director_dispatch(&state, counters, 6, 100, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_YIELD);
        QCOMPARE(state.resume_cursor, 6);
        state = {};
        state.mode = 2;
        const LordlyDirectorWord restart[] = {0xe, 0, 5, 0x80000000};
        QCOMPARE(lordly_director_dispatch(&state, restart, 4, 10, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_BUDGET);
        QCOMPARE(state.cursor, 0);
        const LordlyDirectorWord truncated[] = {0xe, 0};
        state = {};
        state.mode = 2;
        QCOMPARE(lordly_director_dispatch(&state, truncated, 2, 10, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_INVALID);
        const LordlyDirectorWord transfer[] = {0x99, 7, 0, 0x80000000};
        state = {};
        state.mode = 2;
        QCOMPARE(lordly_director_dispatch(&state, transfer, 4, 10, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_TRANSFER);
        QCOMPARE(state.stream_selector, 7);
        QCOMPARE(state.cursor, 3);
    }
    void recoveredControlFlow() {
        // All six comparisons, including equality and signed negative values.
        for (int op = 0; op < 6; ++op)
            for (int value : {-8, -7, -6}) {
                LordlyDirectorWord words[] = {0, quint32(op), quint32(-7), 0x80000002, 0x80000000};
                const bool expected[] = {value == -7, value != -7, value >= -7,
                                         value <= -7, value > -7,  value < -7};
                int nesting = 0, result = -99;
                QCOMPARE(lordly_director_compare(words, 5, 0, value, 0, 0, &nesting, &result), 0);
                QCOMPARE(nesting, expected[op] ? 1 : 0);
                QCOMPARE(result, expected[op] ? 2 : 3);
            }
        LordlyDirectorWord nested[] = {0, 0, 5, 0x80000002, 0, 0x80000002, 0x80000000};
        int nesting = 1, result = -99;
        QCOMPARE(lordly_director_compare(nested, 7, 0, 0, 0, 0, &nesting, &result), 0);
        QCOMPARE(nesting, 1);
        QCOMPARE(result, 3); // Second bridge scanning backward.
        nesting = 2;
        QCOMPARE(lordly_director_compare(nested, 7, 0, 0, 0, 0, &nesting, &result), 0);
        QCOMPARE(nesting, 2);
        QCOMPARE(result, -1); // Barrier found, insufficient bridges.
        QCOMPARE(lordly_director_compare(nested, 7, 0, 0, 0, 1, &nesting, &result), 0);
        QCOMPARE(result, 3); // Resync scans forward, independent of nesting.
        LordlyDirectorWord operandMarker[] = {0, 0, 5, 0x36, 0, 0x80000002, 0x80000000};
        nesting = 0;
        QCOMPARE(lordly_director_compare(operandMarker, 7, 0, 0, 0, 0, &nesting, &result), 0);
        QCOMPARE(result, 5); // Marker inside camera operands must still be consumed.
        result = 42;
        QCOMPARE(lordly_director_compare(nested, 6, 0, 0, 0, 0, &nesting, &result), 1);
        QCOMPARE(result, 42);
        QCOMPARE(nesting, 0);
        QCOMPARE(lordly_director_compare(nested, 7, -1, 0, 0, 0, &nesting, &result), 2);
        LordlyDirectorWord labels[] = {0x36, 0x80000005, 99, 0x80000005, 99};
        QCOMPARE(lordly_director_find_label(labels, 5, 99, &result), 0);
        QCOMPARE(result, 1);
        QCOMPARE(lordly_director_find_label(labels, 5, 98, &result), 1);
        QCOMPARE(lordly_director_find_label(labels, 2, 99, &result), 1);
    }
    void losslessCommandWords() {
        // Environment(-1), camera with signed coordinates, terminal/class 8.
        const auto bytes = QByteArray::fromHex("80000006ffffffff00000036fffffc18000007d000000bb8"
                                               "00000fa0000013880000177000001b5880000001ff000008");
        const auto program = DirectorProgram::decode(bytes);
        QCOMPARE(program.commands.size(), 3);
        QCOMPARE(program.commands[1].word, 2);
        QCOMPARE(program.commands[1].operands[0], quint32(0xfffffc18));
        QCOMPARE(program.resourceClass(), 8);
        QCOMPARE(program.encode(), bytes);
        auto edited = program;
        edited.commands[0].operands[0] = 7;
        auto expected = bytes;
        put32(expected, 4, 7);
        QCOMPARE(edited.encode(), expected);
        QVERIFY_THROWS_EXCEPTION(Error, DirectorProgram::decode(bytes.first(13)));
        QVERIFY_THROWS_EXCEPTION(Error, DirectorProgram::decode(QByteArray::fromHex("00000036")));
        QVERIFY_THROWS_EXCEPTION(Error, DirectorProgram::decode(QByteArray::fromHex("deadbeef")));
        edited.commands[1].operands.removeLast();
        QVERIFY_THROWS_EXCEPTION(Error, edited.encode());
        const auto continuation = DirectorProgram::decode(QByteArray::fromHex("80000001"));
        QCOMPARE(continuation.resourceClass(), -1);
        QCOMPARE(continuation.encode(), QByteArray::fromHex("80000001"));
    }
    void retailDirectoryAndHashes() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private source ROM required");
        const auto rom = Rom::open(path);
        const auto directory = Directors::load(rom);
        QCOMPARE(directory.selectors().size(), 1693);
        QCOMPARE(directory.entries().size(), 1498);
        QFile catalog(":/game/director/grammar.json");
        QVERIFY(catalog.open(QIODevice::ReadOnly));
        const auto metadata = QJsonDocument::fromJson(catalog.readAll()).object();
        const auto names = metadata["names"].toObject();
        QCOMPARE(directory.continuationSelectors().size(), 5);
        for (const auto &value : metadata["continuations"].toArray()) {
            const auto row = value.toObject();
            const auto key = quint32(row["key"].toInteger());
            QCOMPARE(directory.continuationSelectors()[row["selector"].toInt()], key);
            const auto &entry = directory.continuations()[key];
            QCOMPARE(sha256(entry.program.source),
                     row["decodedSha256"].toString().toLatin1().toLower());
            QCOMPARE(entry.program.encode(), entry.program.source);
        }
        int commands = 0;
        QMap<int, int> classes;
        for (const auto &resource : directory.entries()) {
            const auto name = names[QString::number(resource.key)].toObject();
            QVERIFY(!name.isEmpty());
            QCOMPARE(sha256(resource.program.source),
                     name["decodedSha256"].toString().toLatin1().toLower());
            QCOMPARE(resource.program.encode(), resource.program.source);
            for (int selector : resource.selectors)
                QCOMPARE(directory.selectors()[selector], resource.key);
            commands += int(resource.program.commands.size());
            ++classes[resource.program.resourceClass()];
        }
        QCOMPARE(commands, 193771);
        const QMap<int, int> expectedClasses{{1, 369}, {2, 47}, {4, 117}, {5, 92},
                                             {6, 3},   {7, 1},  {8, 869}};
        QCOMPARE(classes, expectedClasses);
        const auto focus = directory.entries().constFind(0x01a14970);
        QVERIFY(focus != directory.entries().end());
        QCOMPARE(focus->envelope + 4, quint32(0x01fa8bf4));
        QCOMPARE(focus->program.commands.size(), 282);
        QCOMPARE(focus->program.resourceClass(), 1);
        auto bad = rom.bytes();
        put32(bad, 0x1f3ca84, 3);
        QVERIFY_THROWS_EXCEPTION(Error, Directors::load(Rom::fromBytes(bad)));
        bad = rom.bytes();
        put32(bad, 0x1f3ca88, 0x10000000);
        QVERIFY_THROWS_EXCEPTION(Error, Directors::load(Rom::fromBytes(bad)));
        bad = rom.bytes();
        put32(bad, 0x1f3e500, 0xffffffff);
        QVERIFY_THROWS_EXCEPTION(Error, Directors::load(Rom::fromBytes(bad)));
        qInfo() << "Preserved" << directory.entries().size() << "streams and" << commands
                << "commands";
    }
};
QTEST_GUILESS_MAIN(DirectorTests)
#include "director_tests.moc"
