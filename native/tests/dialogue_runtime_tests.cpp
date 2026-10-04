#include "engine/dialogue_archive.h"
#include "engine/dialogue_text.h"
#include "engine/scene_dialogue.h"
#include "engine/scene_playback.h"
#include "game/director/text_metrics.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class DialogueRuntimeTests : public QObject {
    Q_OBJECT
    static QByteArray poolBytes(const SceneResources &pool) {
        QByteArray bytes;
        for (int i = 0; i < 6; ++i)
            bytes += pool.record(i);
        return bytes;
    }
  private slots:
    void glyphWidthsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("text-glyph-mips.cjs"),
             path, work.filePath("glyph.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("glyph.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        DialogueFontMetrics metrics{QByteArray::fromHex(root["normal"].toString().toLatin1()),
                                    QByteArray::fromHex(root["alternate"].toString().toLatin1())};
        const auto cases = root["results"].toArray();
        QCOMPARE(cases.size(), 4608);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            metrics.fixedIndex = qint16(c["fixed"].toInt());
            const auto character = quint32(c["character"].toInt()) | 0x12340000;
            QCOMPARE(metrics.glyphWidth(character, c["other"].toInt() != 0), c["width"].toInt());
            QCOMPARE(lordly_text_lead_byte(character), c["lead"].toInt());
        }
        metrics.fixedIndex = -2;
        QVERIFY_THROWS_EXCEPTION(Error, metrics.glyphWidth('A'));
        QCOMPARE(metrics.glyphWidth(0x81), 11); // Lead/control paths precede table access.
        QCOMPARE(metrics.glyphWidth(16), 0);
        metrics.fixedIndex = -1;
        metrics.alternate.clear();
        QVERIFY_THROWS_EXCEPTION(Error, metrics.glyphWidth('A', true));
        metrics.normal.resize(166);
        QVERIFY_THROWS_EXCEPTION(Error, metrics.glyphWidth(0xfd));
        QCOMPARE(metrics.glyphWidth('A'), quint8((81 * 7 + 3) & 255));
    }
    void initializationGeometryAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-layout-mips.cjs"),
                                 path, work.filePath("layout.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("layout.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 1152);
        int index = 0;
        for (const auto &v : cases) {
            const auto c = v.toObject();
            const auto initial = QByteArray::fromHex(c["initial"].toString().toLatin1());
            const auto source = QByteArray::fromHex(c["source"].toString().toLatin1());
            const auto result =
                initializeDialogueWindow(initial, c["slot"].toInt(), source,
                                         quint16(c["width"].toInt()), quint16(c["lines"].toInt()));
            QVERIFY2(result.window.toHex() == c["window"].toString().toLatin1(),
                     qPrintable(QString("Window %1").arg(index)));
            QVERIFY2(result.state.toHex() == c["state"].toString().toLatin1(),
                     qPrintable(QString("Payload %1").arg(index)));
            QCOMPARE(result.warnings, quint32(c["warnings"].toInt()));
            QCOMPARE(result.cursor, u32(result.state, 8));
            ++index;
        }
        QVERIFY_THROWS_EXCEPTION(
            Error, initializeDialogueWindow(QByteArray(168, 0), 0, "No terminator", 0, 0));
        QVERIFY_THROWS_EXCEPTION(
            Error, initializeDialogueWindow(QByteArray(167, 0), 0, QByteArray(1, 0), 0, 0));
    }
    void leadingControlsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-prefix-mips.cjs"),
                                 path, work.filePath("prefix.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("prefix.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(root["retailCount"].toInt(), 4569);
        const auto cases = root["results"].toArray();
        QCOMPARE(cases.size(), 4625);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            auto window = QByteArray::fromHex(c["initialWindow"].toString().toLatin1());
            auto state = QByteArray::fromHex(c["initialState"].toString().toLatin1());
            const auto source = QByteArray::fromHex(c["source"].toString().toLatin1());
            quint32 cursor = 0;
            parseDialoguePrefix(window, state, source, cursor);
            QVERIFY2(window.toHex() == c["window"].toString().toLatin1(),
                     qPrintable(c["id"].toString()));
            QVERIFY2(state.toHex() == c["state"].toString().toLatin1(),
                     qPrintable(c["id"].toString()));
            QCOMPARE(cursor, quint32(c["cursor"].toInt()));
        }
        QByteArray window(168, 0), state(1144, 0);
        quint32 cursor = 0;
        QVERIFY_THROWS_EXCEPTION(Error, parseDialoguePrefix(window, state, "@x@f0", cursor));
        QCOMPARE(window, QByteArray(168, 0));
        QCOMPARE(state, QByteArray(1144, 0));
        QCOMPARE(cursor, 0U);
        parseDialoguePrefix(window, state, QByteArray("@x@nBody\0", 9), cursor);
        QCOMPARE(cursor, 2U);
        QCOMPARE(u8(window, 0x8a), 0x80);
        parseDialoguePrefix(window, state, QByteArray("@x@nBody\0", 9), cursor);
        QCOMPARE(cursor, 2U); // The body grammar owns @n, not the prefix parser.
    }
    void creationResumesWithCurrentContext() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto bytes = Rom::open(path).bytes();
        const QVector<quint32> words{0xbf, 3,   0, 0,           0,           0,
                                     10,   192, 1, 1,           1,           0xffffffffU,
                                     1,    0,   0, 0x80000000U, 0x1a,        0x1007b,
                                     6,    3,   0, 0x80000000U, 0x80000001U, 0xff000001U};
        QByteArray raw(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(raw, i * 4, words[i]);
        const auto packed = encodeLzss(raw);
        constexpr int at = 0x2700000;
        put32(bytes, at, quint32(packed.size() + 4));
        put32(bytes, at + 4, quint32(raw.size()));
        bytes.replace(at + 8, packed.size(), packed);
        put32(bytes, 0x1f3ca84, 4);
        put32(bytes, 0x1f3ca88, at - 0x594280);
        const auto rom = Rom::fromBytes(bytes);
        ScenePlayback playback(rom, Directors::load(rom), 0, {}, SceneCameras::freshModeTwo(rom));
        SceneResources pool({8, 0xffff, 13});
        SceneDialogue windows(0);
        windows.setControl(0, 42);
        const auto before = poolBytes(pool);
        playback.supplyDialogue(pool, windows);
        auto emptyDomains = [](LordlySceneService) { return true; };
        QCOMPARE(playback.advance(emptyDomains), ScenePlayback::Suspended);
        QCOMPARE(playback.pendingService(), std::optional{LORDLY_SCENE_DIRECTOR});
        QVERIFY(playback.reason().contains("roster"));
        QCOMPARE(poolBytes(playback.dialogue()->resources), before);
        DialogueNameInputs inputs;
        inputs.army = QByteArray(5600, 0);
        inputs.guests = QByteArray(5200, 0);
        inputs.groups = QByteArray(25, 0);
        inputs.fixedName = QByteArray("Joe\0", 4);
        inputs.defaultTextParameter = 65530;
        inputs.army.replace(0, 6, "Player");
        put8(inputs.army, 0x11, 81);
        playback.dialogue()->context = SceneDialogueContext(inputs);
        playback.dialogue()->context->setTextParameter(7);
        QCOMPARE(playback.advance(emptyDomains), ScenePlayback::Suspended);
        QVERIFY(playback.reason().contains("projection"));
        QCOMPARE(poolBytes(playback.dialogue()->resources), before);
        QCOMPARE(playback.dialogue()->context->textParameter(), 7);
        QCOMPARE(playback.dialogue()->windows.control(0), 42);
        playback.dialogue()->project = [](int) -> std::optional<QPointF> { return std::nullopt; };
        QCOMPARE(playback.advance(emptyDomains), ScenePlayback::Advanced);
        QCOMPARE(playback.completedUpdates(), 1U);
        QCOMPARE(playback.dialogue()->context->textParameter(), 65530);
        QCOMPARE(playback.dialogue()->context->name(0), QByteArray("Player"));
        QCOMPARE(playback.dialogue()->context->name(1), QByteArray("Player"));
        QCOMPARE(playback.dialogue()->context->name(2), QByteArray("Joe"));
        QCOMPARE(playback.dialogue()->windows.control(0), 0);
        QCOMPARE(u16(playback.dialogue()->resources.record(0), 0x92), 1);
        QVERIFY(!playback.dialogue()->resources.active(1)); // One creation after two suspensions.
        QCOMPARE(playback.advance(emptyDomains), ScenePlayback::Advanced);
        QCOMPARE(playback.dialogue()->context->textParameter(), 123);
        QVERIFY(u8(playback.dialogue()->resources.record(0), 0x8a) & 2);
    }
    void sharedNamesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-context-mips.cjs"),
                                 path, work.filePath("names.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("names.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        const auto cases = root["results"].toArray(), patterns = root["patterns"].toArray();
        QCOMPARE(cases.size(), 480);
        auto inputsFor = [&](int index) {
            const auto p = patterns[index].toObject();
            DialogueNameInputs inputs;
            inputs.army = QByteArray::fromHex(p["army"].toString().toLatin1());
            inputs.guests = QByteArray::fromHex(p["guests"].toString().toLatin1());
            inputs.groups = QByteArray::fromHex(p["groups"].toString().toLatin1());
            inputs.fixedName = p["fixed"].toString().toLatin1();
            inputs.currentGroup = quint8(p["currentGroup"].toInt());
            inputs.defaultTextParameter = quint16(p["defaultTextParameter"].toInt());
            for (const auto &v : p["overrides"].toArray()) {
                const auto o = v.toObject();
                inputs.overrides << DialogueNameOverride{quint8(o["selector"].toInt()),
                                                         o["text"].toString().toLatin1()};
            }
            return inputs;
        };
        auto compare = [&](const SceneDialogueContext &context, const QJsonObject &expected) {
            QCOMPARE(context.ownerWorkspace().toHex(), expected["owner"].toString().toLatin1());
            QCOMPARE(context.groupWorkspace().toHex(), expected["group"].toString().toLatin1());
            QCOMPARE(context.textParameter(), expected["textParameter"].toInt());
            const auto refs = expected["references"].toArray();
            for (int i = 0; i < 4; ++i) {
                const auto ref = context.reference(i);
                quint32 address = 0;
                switch (ref.kind) {
                case SceneDialogueContext::Kind::Absent:
                    break;
                case SceneDialogueContext::Kind::Army:
                    address = 0x80193bc0 + ref.index * 56;
                    break;
                case SceneDialogueContext::Kind::Guest:
                    address = 0x80195560 + ref.index * 52;
                    break;
                case SceneDialogueContext::Kind::OwnerWorkspace:
                    address = 0x8022ac00;
                    break;
                case SceneDialogueContext::Kind::GroupWorkspace:
                    address = 0x8022ac40;
                    break;
                case SceneDialogueContext::Kind::Fixed:
                    address = 0x80196a58;
                    break;
                }
                QCOMPARE(address, quint32(refs[i].toDouble()));
            }
        };
        for (const auto &value : cases) {
            const auto c = value.toObject();
            SceneDialogueContext context(inputsFor(c["pattern"].toInt()));
            QByteArray actor(336, 0), row(248, 0);
            put8(row, 0xf6, 5);
            put32(row, 0x48, 84);
            context.prepare(2, actor, row);
            compare(context, c["first"].toObject());
            if (QTest::currentTestFailed())
                return;
            QCOMPARE(context.name(1), QByteArray("Army5-one-two"));
            QCOMPARE(context.name(2), QByteArray("Fixed"));
            context.setTextParameter(123);
            context.prepare(c["mode"].toInt(),
                            QByteArray::fromHex(c["actor"].toString().toLatin1()),
                            QByteArray::fromHex(c["row"].toString().toLatin1()));
            compare(context, c["second"].toObject());
            if (QTest::currentTestFailed())
                return;
        }
        auto inputs = inputsFor(1);
        SceneDialogueContext valid(inputs);
        QByteArray actor(336, 0), row(248, 0);
        put32(row, 0x48, 84);
        put8(row, 0xf6, 5);
        valid.prepare(2, actor, row);
        const auto owner = valid.ownerWorkspace(), group = valid.groupWorkspace();
        valid.setTextParameter(0x1007b);
        put8(row, 0xf6, 200);
        QVERIFY_THROWS_EXCEPTION(Error, valid.prepare(2, actor, row));
        QCOMPARE(valid.ownerWorkspace(), owner);
        QCOMPARE(valid.groupWorkspace(), group);
        QCOMPARE(valid.textParameter(), 123);
        QCOMPARE(valid.name(1), QByteArray("Army5-one-two"));
        inputs.overrides[1].text = QByteArray(64, 'A') + '\0';
        SceneDialogueContext tooLong(inputs);
        put8(row, 0xf6, 5);
        QVERIFY_THROWS_EXCEPTION(Error, tooLong.prepare(2, actor, row));
        QCOMPARE(tooLong.ownerWorkspace(), QByteArray(64, 0));
        QCOMPARE(tooLong.reference(0).kind, SceneDialogueContext::Kind::Absent);
    }
    void playbackTracksWindowsAcrossSuspension() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto bytes = Rom::open(path).bytes();
            const QVector<quint32> words{6, 3, 0,           0x80000000U, 5,
                                         3, 0, 0x80000000U, 0x80000001U, 0xff000001U};
            QByteArray raw(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(raw, i * 4, words[i]);
            const auto packed = encodeLzss(raw);
            constexpr int at = 0x2700000;
            put32(bytes, at, quint32(packed.size() + 4));
            put32(bytes, at + 4, quint32(raw.size()));
            bytes.replace(at + 8, packed.size(), packed);
            put32(bytes, 0x1f3ca84, 4);
            put32(bytes, 0x1f3ca88, at - 0x594280);
            const auto rom = Rom::fromBytes(bytes);
            const auto directors = Directors::load(rom);
            SceneResources pool({8, 0xffff, 13});
            SceneDialogue windows(0);
            windows.create({3, 20, 2, 0, 0, 10, 192, 1, 1, 1, 84, 1, 0}, pool,
                           [](int) { return QPointF(100, 50); });
            auto create = [&] {
                return ScenePlayback(rom, directors, 0, {}, SceneCameras::freshModeTwo(rom));
            };
            auto emptyDomains = [](LordlySceneService) { return true; };
            int calls = 0;
            auto project = [&](int actor) -> std::optional<QPointF> {
                if (actor != 0)
                    throw Error("Unexpected dialogue actor");
                ++calls;
                return QPointF(140.75, 200.25);
            };
            auto baseline = create();
            baseline.supplyDialogue(pool, windows, project);
            QCOMPARE(baseline.advance(emptyDomains), ScenePlayback::Advanced);
            QCOMPARE(calls, 1);
            const auto followed = poolBytes(baseline.dialogue()->resources);
            QCOMPARE(u16(baseline.dialogue()->resources.record(0), 0x8c), 140);
            QCOMPARE(u16(baseline.dialogue()->resources.record(0), 0x94), 155);
            QVERIFY(u8(baseline.dialogue()->resources.record(0), 0x8a) & 2);
            QCOMPARE(baseline.advance(emptyDomains), ScenePlayback::Advanced);
            QCOMPARE(calls, 1); // Close removes the tracking before this stage.
            QVERIFY(baseline.dialogue()->windows.tracking(0).isEmpty());
            QVERIFY(u16(baseline.dialogue()->resources.record(0), 0) & 0x1000);
            const auto closed = poolBytes(baseline.dialogue()->resources);
            auto resumed = create();
            resumed.supplyDialogue(pool, windows); // Projection is initially unavailable.
            QCOMPARE(resumed.advance(emptyDomains), ScenePlayback::Suspended);
            QCOMPARE(resumed.pendingService(), std::optional{LORDLY_SCENE_AFTER_DIRECTOR});
            QCOMPARE(resumed.completedUpdates(), 0U);
            QCOMPARE(u16(resumed.dialogue()->resources.record(0), 0x8c), 100);
            const int cursor = resumed.director().state().resume_cursor;
            resumed.dialogue()->project = project;
            calls = 0;
            for (int tick = 0; tick < 2; ++tick) {
                int attempts = 0;
                ScenePlayback::Result status;
                do {
                    status = resumed.advance(emptyDomains, {}, 1, 1);
                    QVERIFY2(status != ScenePlayback::Suspended, qPrintable(resumed.reason()));
                    QVERIFY(++attempts < 100);
                } while (status == ScenePlayback::Budget);
                QCOMPARE(status, ScenePlayback::Advanced);
                QCOMPARE(poolBytes(resumed.dialogue()->resources), tick ? closed : followed);
                QCOMPARE(resumed.completedUpdates(), quint64(tick + 1));
                if (!tick)
                    QCOMPARE(resumed.director().state().resume_cursor, cursor);
            }
            QCOMPARE(calls, 1);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void archivesFromCurrentRom() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-archive-reference.cjs"),
                                 path, work.filePath("archives.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("archives.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 348);
        QCOMPARE(DialogueArchive::directory(rom).size(), 503);
        int entries = 0;
        for (const auto &v : cases) {
            const auto c = v.toObject();
            const auto archive = DialogueArchive::load(rom, c["selector"].toInt());
            QCOMPARE(archive.bytes().toHex(), c["bytes"].toString().toLatin1());
            const auto expected = c["entries"].toArray();
            QCOMPARE(archive.entryCount(), expected.size());
            for (int i = 0; i < expected.size(); ++i)
                QCOMPARE(archive.entry(i).toHex(), expected[i].toString().toLatin1());
            entries += archive.entryCount();
        }
        QCOMPARE(entries, 4569);
        QVERIFY_THROWS_EXCEPTION(Error, DialogueArchive::load(rom, -1));
        QVERIFY_THROWS_EXCEPTION(Error, DialogueArchive::load(rom, 503));
        QVERIFY_THROWS_EXCEPTION(Error, DialogueArchive::load(rom, 9)); // Empty selector.
        const auto key = DialogueArchive::directory(rom)[0];
        const qsizetype at = qsizetype(key) + 0x594280;
        auto malformed = rom.bytes();
        put32(malformed, at, 24);
        QVERIFY_THROWS_EXCEPTION(Error, DialogueArchive::load(Rom::fromBytes(malformed), 0));
        malformed = rom.bytes();
        const auto member = LhaMember::read(malformed, at + 4);
        const auto last = at + 4 + member.headerSize + member.storedSize - 4;
        put8(malformed, last, u8(malformed, last) ^ 0x80);
        QVERIFY_THROWS_EXCEPTION(Error, DialogueArchive::load(Rom::fromBytes(malformed), 0));
    }
    void archivePointersPreserveAliases() {
        QByteArray bytes(12, 0);
        put32(bytes, 0, 12);
        put32(bytes, 4, 16);
        put32(bytes, 8, 12);
        bytes += QByteArray("@nA\0B@p\0", 8);
        const auto archive = DialogueArchive::fromDecoded(bytes);
        QCOMPARE(archive.entryCount(), 3);
        QCOMPARE(archive.entry(0), QByteArray("@nA"));
        QCOMPARE(archive.entry(1), QByteArray("B@p"));
        QCOMPARE(archive.entry(2), archive.entry(0));
        QCOMPARE(archive.bytes(), bytes);
        QVERIFY_THROWS_EXCEPTION(Error, archive.entry(3));
        auto bad = bytes;
        put32(bad, 4, 4);
        QVERIFY_THROWS_EXCEPTION(Error, DialogueArchive::fromDecoded(bad));
        bad = bytes;
        bad[19] = 'X';
        QVERIFY_THROWS_EXCEPTION(Error, DialogueArchive::fromDecoded(bad));
        put32(bad, 0, 13);
        QVERIFY_THROWS_EXCEPTION(Error, DialogueArchive::fromDecoded(bad));
    }
    void windowsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-window-mips.cjs"),
                                 path, work.filePath("windows.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("windows.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 64);
        int snapshots = 0;
        for (const auto &value : cases) {
            const auto c = value.toObject();
            SceneResources pool({8, 0xffff, 13});
            const auto initial = QByteArray::fromHex(c["initial"].toString().toLatin1());
            for (int i = 0; i < 6; ++i)
                pool.supplyRecord(i, initial.mid(i * 168, 168));
            SceneDialogue dialogue(2);
            for (int i = 0; i < 8; ++i)
                dialogue.setControl(i, 0xa7);
            QVector<quint32> args;
            for (const auto &a : c["args"].toArray())
                args << quint32(qint64(a.toDouble()));
            const bool present = c["point"].isArray();
            auto point = c["point"].toArray();
            auto project = [&](int actor) -> std::optional<QPointF> {
                if (actor != 27)
                    throw Error("Unexpected Actor in reference sequence");
                if (!present)
                    return std::nullopt;
                return QPointF(point[0].toDouble(), point[1].toDouble());
            };
            for (const auto &snapshot : c["states"].toArray()) {
                const auto state = snapshot.toObject();
                const auto op = state["operation"].toObject();
                const auto kind = op["kind"].toString();
                if (kind == "create")
                    dialogue.create(args, pool, project);
                else if (kind == "place") {
                    point = op["point"].toArray();
                    dialogue.updatePlacement(pool, project);
                } else if (kind == "resume")
                    dialogue.resume(args[0], pool);
                else if (kind == "pause") {
                    const int slot = op["slot"].toInt();
                    auto record = pool.record(slot);
                    put8(record, 0x8a, u8(record, 0x8a) | 1);
                    pool.supplyRecord(slot, record);
                } else if (kind == "close")
                    dialogue.close(qint32(op["id"].toInt()), pool);
                else
                    QFAIL("Unknown reference operation");
                QCOMPARE(poolBytes(pool).toHex(), state["records"].toString().toLatin1());
                const auto bindings = state["bindings"].toArray();
                for (int i = 0; i < 28; ++i)
                    QCOMPARE(dialogue.tracking(i).toHex(), bindings[i].toString().toLatin1());
                QByteArray controls;
                for (int i = 0; i < 8; ++i)
                    controls += char(dialogue.control(i));
                QCOMPARE(controls.toHex(), state["control"].toString().toLatin1());
                QCOMPARE(dialogue.query(args[0], pool), state["query"].toInt());
                ++snapshots;
            }
        }
        QCOMPARE(snapshots, 560);
    }
    void missingContextIsAtomic() {
        SceneResources pool({8, 0xffff, 13});
        SceneDialogue dialogue(0);
        dialogue.setControl(0, 0x1a7);
        QVector<quint32> args{3, 20, 2, 0, 0, 10, 192, 1, 1, 1, 84, 1, 0};
        const auto initial = poolBytes(pool);
        QVERIFY_THROWS_EXCEPTION(Error, dialogue.create(args, pool));
        QCOMPARE(poolBytes(pool), initial);
        QCOMPARE(dialogue.control(0), 0xa7);
        QVERIFY(dialogue.tracking(0).isEmpty());
        args[10] = 0xffffffff;
        auto absent = [](int) -> std::optional<QPointF> { return std::nullopt; };
        QVERIFY_THROWS_EXCEPTION(Error, dialogue.create(args, pool, absent));
        QCOMPARE(poolBytes(pool), initial);
        QCOMPARE(dialogue.create(args, pool, absent, [](int) { return -1; }), 0);
        QCOMPARE(u16(pool.record(0), 0x92), 1); // Native negation of inferred -1.
        QCOMPARE(dialogue.control(0), 0);
        auto active = poolBytes(pool);
        QVERIFY_THROWS_EXCEPTION(Error, dialogue.updatePlacement(pool));
        QCOMPARE(poolBytes(pool), active);
        dialogue.close(3, pool);
        QVERIFY(dialogue.tracking(0).isEmpty());
        auto released = pool.record(0);
        put16(released, 0, 0);
        pool.supplyRecord(0, released);
        QCOMPARE(dialogue.query(3, pool), 1);
        dialogue.setSharedStatus(2);
        QCOMPARE(dialogue.query(3, pool), 3);
        args[12] = 1;
        args[10] = 84;
        args[4] = 511;
        args[5] = 65530;
        QCOMPARE(dialogue.create(args, pool), 0); // Fixed placement needs no projector.
        QCOMPARE(u16(pool.record(0), 0x8c), 255);
        QCOMPARE(u16(pool.record(0), 0x94), 65530);
        for (int i = 1; i < 6; ++i)
            pool.registerWindow(0x80198be8, 0, 0, 0, 0);
        active = poolBytes(pool);
        const auto tracking = dialogue.tracking(0);
        dialogue.setControl(1, 19);
        QVERIFY_THROWS_EXCEPTION(Error, dialogue.create(args, pool));
        QCOMPARE(poolBytes(pool), active);
        QCOMPARE(dialogue.tracking(0), tracking);
        QCOMPARE(dialogue.control(1), 19);
        QVERIFY_THROWS_EXCEPTION(Error, dialogue.setControl(8, 1));
        QVERIFY_THROWS_EXCEPTION(Error, dialogue.supplyTracking(0, QByteArray(5, 0)));
    }
};
QTEST_GUILESS_MAIN(DialogueRuntimeTests)
#include "dialogue_runtime_tests.moc"
