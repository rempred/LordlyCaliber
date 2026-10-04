#include "engine/dialogue_archive.h"
#include "engine/scene_dialogue.h"
#include "engine/scene_playback.h"
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
