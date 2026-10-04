#include "engine/dialogue_archive.h"
#include "engine/dialogue_draw.h"
#include "engine/dialogue_player.h"
#include "engine/dialogue_text.h"
#include "engine/scene_dialogue.h"
#include "engine/scene_dialogue_runtime.h"
#include "engine/scene_playback.h"
#include "game/director/text_metrics.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
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
    void ordinaryDrawingCommands() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            DialogueDrawing drawing(Rom::open(path));
            QFile fixtures(
                QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath())
                    .filePath("../../tests/fixtures/cutscene-active-dialogue-drawing.json"));
            QVERIFY(fixtures.open(QIODevice::ReadOnly));
            const auto cases =
                QJsonDocument::fromJson(fixtures.readAll()).object()["cases"].toArray();
            QCOMPARE(cases.size(), 6);
            int rectangles = 0;
            for (const auto &value : cases) {
                const auto c = value.toObject();
                auto record = QByteArray::fromHex(c["recordHex"].toString().toLatin1());
                auto payload = QByteArray::fromHex(c["payloadHex"].toString().toLatin1());
                const auto commands = drawing.compose(record, payload);
                const auto expected = c["commands"].toArray();
                QCOMPARE(commands.size(), expected.size());
                for (int i = 0; i < commands.size(); ++i) {
                    const auto &actual = commands[i];
                    const auto reference = expected[i].toObject();
                    for (int j = 0; j < 4; ++j) {
                        QCOMPARE(actual.bounds[j], reference["bounds"].toArray()[j].toInt());
                        QCOMPARE(actual.scissor[j], reference["scissor"].toArray()[j].toInt());
                    }
                    for (int j = 0; j < 2; ++j) {
                        QCOMPARE(actual.uv[j], reference["uv"].toArray()[j].toDouble());
                        QCOMPARE(actual.step[j], reference["step"].toArray()[j].toDouble());
                    }
                    QCOMPARE(actual.texture.indices.toHex(),
                             reference["indicesHex"].toString().toLatin1());
                    const auto palette = reference["palette"].toArray();
                    for (int j = 0; j < actual.texture.palette.size(); ++j)
                        QCOMPARE(int(actual.texture.palette[j]), palette[j].toInt());
                    ++rectangles;
                }
                RasterImage image{320, 240, QByteArray(320 * 240 * 4, 0)};
                DialogueDrawing::paint(image, commands);
                QVERIFY(image.rgba != QByteArray(320 * 240 * 4, 0));
                if (c["sample"].toInt() == 89) {
                    const QImage rendered(reinterpret_cast<const uchar *>(image.rgba.constData()), 320, 240, QImage::Format_RGBA8888);
                    QVERIFY(rendered.save("native-dialogue-render.png"));
                }
                put8(payload, 0x47, 1);
                QVERIFY_THROWS_EXCEPTION(Error, drawing.compose(record, payload));
            }
            QCOMPARE(rectangles, 93);
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void composedResourceLifecycle() {
        try {
            SceneResources pool({1, -1, 0xffff});
            SceneDialogue dialogue(0);
            SceneDialogueRuntime runtime;
            SceneResourceDispatcher dispatch(SceneResourcePassState{});
            DialogueTextEnvironment environment;
            environment.metrics.normal = QByteArray(256, 6);
            environment.metrics.alternate = QByteArray(256, 6);
            environment.longNameSuffix = QByteArray("\0", 1);
            const QByteArray shake = QByteArray::fromHex("00050301fdfa80");
            auto source = [](const QByteArray &) {
                return QByteArray("@v020@y003First@aSecond@sTail\0", 30);
            };
            for (int i = 0; i < 2; ++i)
                QCOMPARE(dialogue.create({quint32(i), 0, 0, quint32(i), 20, quint32(20 + i * 70), 0,
                                          0, 0, 0, 0, 0, 1},
                                         pool),
                         i);
            auto step = [&](quint16 pressed, bool interrupted = false) {
                auto &pass = dispatch.state();
                if (pass.queue.count) {
                    pool.advanceOpening(pass.queue);
                    for (int q = 0; q < pass.queue.count; ++q) {
                        const int slot = pass.queue.entries[q];
                        if (pool.advanceClosing(slot)) {
                            QByteArray scratch(168, 0);
                            if (!runtime.service(
                                    pool, scratch, pass,
                                    {SceneResourceDispatcher::Service::Release, slot, 0}, dialogue,
                                    environment, {512, pressed, 0, 0}, shake, source))
                                throw Error("Missing closing service");
                        }
                    }
                    pool.rebuildPriority(pass.queue);
                }
                runtime.clearSounds();
                bool pause = interrupted;
                auto handler = [&](SceneResources &next, QByteArray &scratch,
                                   SceneResourcePassState &s,
                                   const SceneResourceDispatcher::Call &call) {
                    if (pause) {
                        pause = false;
                        return false;
                    }
                    pause = interrupted;
                    return runtime.service(next, scratch, s, call, dialogue, environment,
                                           {512, pressed, 0, 0}, shake, source);
                };
                int attempts = 0;
                while (dispatch.advance(pool, handler, interrupted ? 1 : 1000) !=
                       SceneResourceDispatcher::Complete)
                    if (++attempts > 2000)
                        throw Error("Dialogue composition did not finish");
            };
            int frames = 0;
            for (; frames < 100; ++frames) {
                step(0, true);
                if (runtime.window(0, pool) && runtime.window(1, pool) &&
                    u8(runtime.window(0, pool)->frame.state, 0x3c) == 3 &&
                    u8(runtime.window(1, pool)->frame.state, 0x3c) == 3)
                    break;
            }
            QVERIFY(frames < 100);
            QCOMPARE(dispatch.state().queue.entries[0], quint16(0));
            QVERIFY(runtime.window(0, pool)->frame.state.mid(0x178, 5) == "First");
            QVERIFY(runtime.window(1, pool)->frame.state.mid(0x178, 5) == "First");
            const auto lower = runtime.window(1, pool)->frame.state;
            step(0x8000, true);
            QCOMPARE(u8(runtime.window(0, pool)->frame.state, 0x3c), quint8(2));
            QCOMPARE(runtime.window(1, pool)->frame.state, lower);
            for (frames = 0; frames < 100 && dialogue.query(0, pool) != 2; ++frames)
                step(0);
            QVERIFY(frames < 100);
            QCOMPARE(dialogue.query(1, pool), 0);
            dialogue.resume(0, pool);
            for (frames = 0; frames < 100 && pool.active(0); ++frames)
                step(0, true);
            QVERIFY(frames < 100);
            QVERIFY(!runtime.window(0, pool));
            QCOMPARE(dialogue.query(0, pool), 1);
            QVERIFY(pool.active(1));
            QCOMPARE(dispatch.state().queue.entries[0], quint16(1));
            step(0x8000);
            QCOMPARE(u8(runtime.window(1, pool)->frame.state, 0x3c), quint8(2));
            // Reusing the released slot initializes independent text and payload state.
            QCOMPARE(dialogue.create({2, 0, 0, 2, 30, 40, 0, 0, 0, 0, 0, 0, 1}, pool), 0);
            step(0);
            QVERIFY(runtime.window(0, pool));
            QCOMPARE(u16(runtime.window(0, pool)->frame.state, 0x34), quint16(0));
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void insertionStorageAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-source-bindings-mips.cjs"),
                                 path, work.filePath("bindings.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("bindings.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        DialogueTextFrame frame{QByteArray::fromHex(root["window"].toString().toLatin1()),
                                QByteArray::fromHex(root["state"].toString().toLatin1())};
        const auto source = QByteArray::fromHex(root["source"].toString().toLatin1());
        DialogueTextEnvironment environment;
        environment.storage[1] = QByteArray("ABC\0", 4);
        environment.storage[2] = QByteArray("XYZ\0", 4);
        environment.references[48] = 1;
        const auto cases = root["results"].toArray();
        QCOMPARE(cases.size(), 6);
        for (int update = 0; update < cases.size(); ++update) {
            if (update == 1)
                environment.references[48] = 2;
            if (update == 2)
                environment.storage[1] = QByteArray("PQR\0", 4);
            const auto expected = cases[update].toObject();
            const auto result =
                revealDialogueText(frame, source, environment, quint16(expected["speed"].toInt()));
            QCOMPARE(frame.state.toHex(), expected["state"].toString().toLatin1());
            QCOMPARE(frame.window.toHex(), expected["window"].toString().toLatin1());
            QCOMPARE(result, expected["result"].toInt());
            QCOMPARE(frame.sounds.size(), expected["sounds"].toInt());
            QCOMPARE(frame.insertion.has_value(), !expected["insertion"].isNull());
            if (frame.insertion) {
                const auto insertion = expected["insertion"].toObject();
                QVERIFY(frame.insertion->sourceIdentity.has_value());
                QCOMPARE(*frame.insertion->sourceIdentity, quint64(insertion["identity"].toInt()));
                QCOMPARE(frame.insertion->cursor, quint32(insertion["cursor"].toInt()));
            }
            if (update == 0) {
                const auto before = frame.state;
                auto missing = environment;
                missing.storage.remove(1);
                QVERIFY_THROWS_EXCEPTION(Error, revealDialogueText(frame, source, missing, 256));
                QCOMPARE(frame.state, before);
            }
        }
    }
    void windowUpdatesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-update-mips.cjs"),
                                 path, work.filePath("updates.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        qInfo().noquote() << reference.readAllStandardOutput();
        QFile file(work.filePath("updates.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        DialogueTextEnvironment environment;
        const auto entries = root["substitutions"].toArray();
        for (int i = 0; i < entries.size(); ++i)
            environment.substitutions[i] =
                entries[i].isNull() ? QByteArray{}
                                    : QByteArray::fromHex(entries[i].toString().toLatin1());
        environment.longNameSuffix = QByteArray::fromHex(root["suffix"].toString().toLatin1());
        const auto shake = QByteArray::fromHex(root["shake"].toString().toLatin1());
        const auto cases = root["results"].toArray();
        QCOMPARE(cases.size(), 408);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            DialogueTextFrame frame{QByteArray::fromHex(c["window"].toString().toLatin1()),
                                    QByteArray::fromHex(c["state"].toString().toLatin1())};
            const auto controls = QByteArray::fromHex(c["controls"].toString().toLatin1());
            for (int i = 0; i < 8; ++i)
                frame.controls[i] = quint8(controls[i]);
            const auto source = QByteArray::fromHex(c["source"].toString().toLatin1());
            int update = 0;
            for (const auto &s : c["snapshots"].toArray()) {
                const auto expected = s.toObject(), input = expected["input"].toObject();
                try {
                    advanceDialogueWindow(
                        frame, source, environment,
                        {quint16(input["speed"].toInt()), quint16(input["pressed"].toInt()),
                         quint16(input["repeat"].toInt()), quint16(input["history"].toInt())},
                        shake);
                } catch (const std::exception &e) {
                    QFAIL(qPrintable(c["id"].toString() + ": " + e.what()));
                }
                const auto label = c["id"].toString() + " update " + QString::number(update++);
                QVERIFY2(frame.window.toHex() == expected["window"].toString().toLatin1(),
                         qPrintable(label + " window"));
                QVERIFY2(frame.state.toHex() == expected["state"].toString().toLatin1(),
                         qPrintable(label + " payload"));
                const auto sounds = expected["sounds"].toArray();
                QCOMPARE(frame.sounds.size(), sounds.size());
                for (int i = 0; i < sounds.size(); ++i) {
                    const auto sound = sounds[i].toObject();
                    QCOMPARE(frame.sounds[i].sound, sound["sound"].toInt());
                    QCOMPARE(frame.sounds[i].volume, sound["volume"].toInt());
                }
                const auto expectedControls =
                    QByteArray::fromHex(expected["controls"].toString().toLatin1());
                for (int i = 0; i < 8; ++i)
                    QCOMPARE(frame.controls[i], quint8(expectedControls[i]));
                QCOMPARE(frame.insertion.has_value(), !expected["insertion"].isNull());
                if (frame.insertion) {
                    const auto insertion = expected["insertion"].toObject();
                    QCOMPARE(frame.insertion->key, insertion["key"].toInt());
                    QCOMPARE(frame.insertion->cursor, quint32(insertion["cursor"].toInt()));
                }
                const auto children = expected["children"].toArray();
                QCOMPARE(frame.children.size(), children.size());
                for (int i = 0; i < children.size(); ++i) {
                    const auto child = children[i].toObject();
                    QCOMPARE(frame.children[i].x, child["x"].toInt());
                    QCOMPARE(frame.children[i].y, child["y"].toInt());
                    QCOMPARE(frame.children[i].entry, child["entry"].toInt());
                }
            }
        }
        DialogueTextFrame frame{QByteArray(168, 0), QByteArray(1144, 0)};
        put8(frame.state, 0x3c, 2);
        const auto before = frame;
        QVERIFY_THROWS_EXCEPTION(Error, advanceDialogueWindow(frame, QByteArray("ABC\0", 4), {},
                                                              {0, 0x8000, 0, 0}, shake, 10));
        QCOMPARE(frame.window, before.window);
        QCOMPARE(frame.state, before.state);
        put8(frame.state, 0x3c, 5);
        put8(frame.state, 0x41, 1);
        put8(frame.state, 0x3f, 255);
        const auto beforeShake = frame.state;
        QVERIFY_THROWS_EXCEPTION(
            Error, advanceDialogueWindow(frame, QByteArray("ABC\0", 4), {}, {256, 0, 0, 0}, shake));
        QCOMPARE(frame.state, beforeShake);
    }
    void textRevealAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-reveal-mips.cjs"),
                                 path, work.filePath("reveal.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        qInfo().noquote() << reference.readAllStandardOutput();
        QFile file(work.filePath("reveal.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        DialogueTextEnvironment environment;
        const auto entries = root["substitutions"].toArray();
        for (int i = 0; i < entries.size(); ++i)
            environment.substitutions[i] =
                entries[i].isNull() ? QByteArray{}
                                    : QByteArray::fromHex(entries[i].toString().toLatin1());
        environment.longNameSuffix = QByteArray::fromHex(root["suffix"].toString().toLatin1());
        QCOMPARE(root["retailCount"].toInt(), 4569);
        const auto cases = root["results"].toArray();
        QVERIFY(cases.size() > 4569);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            DialogueTextFrame frame{QByteArray::fromHex(c["window"].toString().toLatin1()),
                                    QByteArray::fromHex(c["state"].toString().toLatin1())};
            const auto controls = QByteArray::fromHex(c["controls"].toString().toLatin1());
            for (int i = 0; i < 8; ++i)
                frame.controls[i] = quint8(controls[i]);
            const auto source = QByteArray::fromHex(c["source"].toString().toLatin1());
            for (const auto &s : c["snapshots"].toArray()) {
                const auto expected = s.toObject();
                quint8 result = 0;
                try {
                    result =
                        revealDialogueText(frame, source, environment, quint16(c["speed"].toInt()));
                } catch (const std::exception &e) {
                    QFAIL(qPrintable(c["id"].toString() + ": " + e.what()));
                }
                QVERIFY2(frame.window.toHex() == expected["window"].toString().toLatin1(),
                         qPrintable(c["id"].toString() + " window"));
                QVERIFY2(frame.state.toHex() == expected["state"].toString().toLatin1(),
                         qPrintable(c["id"].toString() + " payload speed " +
                                    QString::number(c["speed"].toInt())));
                QCOMPARE(result, expected["result"].toInt());
                QCOMPARE(frame.sounds.size(), expected["sounds"].toInt());
                for (const auto &sound : frame.sounds) {
                    QCOMPARE(sound.sound, 0x1a);
                    QCOMPARE(sound.volume, 10);
                }
                const auto expectedControls =
                    QByteArray::fromHex(expected["controls"].toString().toLatin1());
                for (int i = 0; i < 8; ++i)
                    QCOMPARE(frame.controls[i], quint8(expectedControls[i]));
                QCOMPARE(frame.insertion.has_value(), !expected["insertion"].isNull());
                if (frame.insertion) {
                    const auto insertion = expected["insertion"].toObject();
                    QCOMPARE(frame.insertion->key, insertion["key"].toInt());
                    QCOMPARE(frame.insertion->cursor, quint32(insertion["cursor"].toInt()));
                }
                const auto children = expected["children"].toArray();
                QCOMPARE(frame.children.size(), children.size());
                for (int i = 0; i < children.size(); ++i) {
                    const auto child = children[i].toObject();
                    QCOMPARE(frame.children[i].x, child["x"].toInt());
                    QCOMPARE(frame.children[i].y, child["y"].toInt());
                    QCOMPARE(frame.children[i].entry, child["entry"].toInt());
                }
            }
        }
        DialogueTextFrame frame{QByteArray(168, 0), QByteArray(1144, 0)};
        const auto before = frame;
        QVERIFY_THROWS_EXCEPTION(Error, revealDialogueText(frame, QByteArray("@%0\0", 4), {}, 512));
        QCOMPARE(frame.window, before.window);
        QCOMPARE(frame.state, before.state);
        QVERIFY_THROWS_EXCEPTION(
            Error, revealDialogueText(frame, QByteArray("{unterminated\0", 14), {}, 512));
        QCOMPARE(frame.state, before.state);
        QVERIFY_THROWS_EXCEPTION(Error,
                                 revealDialogueText(frame, QByteArray("@m@m@m\0", 7), {}, 512, 3));
        QCOMPARE(frame.window, before.window);
        QCOMPARE(frame.state, before.state);
        QVERIFY(frame.children.isEmpty());
        QVERIFY(frame.sounds.isEmpty());
    }
    void choiceScanningAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-choices-mips.cjs"),
                                 path, work.filePath("choices.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("choices.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(root["retailCount"].toInt(), 4569);
        const auto cases = root["results"].toArray();
        QCOMPARE(cases.size(), 4616);
        const quint32 markers[] = {35, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 102};
        for (const auto &v : cases) {
            const auto c = v.toObject();
            const auto source = QByteArray::fromHex(c["source"].toString().toLatin1());
            QCOMPARE(countDialogueChoices(source, 0), c["count"].toInt());
            const auto cursors = c["cursors"].toArray();
            for (int i = 0; i < 12; ++i) {
                quint32 cursor = 0;
                seekDialogueChoice(source, cursor, markers[i] | 0x100);
                QVERIFY2(cursor == quint32(cursors[i].toInt()),
                         qPrintable(c["id"].toString() + " marker " + QString::number(markers[i])));
            }
        }
        quint32 cursor = 1;
        QVERIFY_THROWS_EXCEPTION(Error, seekDialogueChoice(QByteArray("X@w\0", 4), cursor, '#'));
        QCOMPARE(cursor, 1U);
        QVERIFY_THROWS_EXCEPTION(Error, countDialogueChoices(QByteArray("ABC\0", 4), 0, 2));
    }
    void windowMotionAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-motion-mips.cjs"),
                                 path, work.filePath("motion.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("motion.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 270);
        int sample = 0;
        for (const auto &v : cases) {
            const auto c = v.toObject();
            auto window = QByteArray::fromHex(c["window"].toString().toLatin1());
            auto state = QByteArray::fromHex(c["state"].toString().toLatin1());
            for (const auto &s : c["snapshots"].toArray()) {
                advanceDialoguePosition(state);
                trackDialoguePointer(window, state);
                positionDialogueBounds(window, state);
                const auto expected = s.toObject();
                QVERIFY2(window.toHex() == expected["window"].toString().toLatin1(),
                         qPrintable(QString("Window %1").arg(sample)));
                QVERIFY2(state.toHex() == expected["state"].toString().toLatin1(),
                         qPrintable(QString("Payload %1").arg(sample)));
            }
            ++sample;
        }
        QByteArray window(167, 0), state(1144, 'x');
        const auto before = state;
        QVERIFY_THROWS_EXCEPTION(Error, positionDialogueBounds(window, state));
        QCOMPARE(state, before);
        QVERIFY_THROWS_EXCEPTION(Error, trackDialoguePointer(window, state));
        QCOMPARE(state, before);
        QVERIFY_THROWS_EXCEPTION(Error, advanceDialoguePosition(window));
    }
    void completeInitializationAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-initialize-mips.cjs"),
                                 path, work.filePath("windows.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("windows.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        DialogueTextEnvironment environment;
        const auto entries = root["substitutions"].toArray();
        for (int i = 0; i < entries.size(); ++i)
            environment.substitutions[i] = QByteArray::fromHex(entries[i].toString().toLatin1());
        environment.longNameSuffix = QByteArray::fromHex(root["suffix"].toString().toLatin1());
        environment.metrics = {QByteArray::fromHex(root["normal"].toString().toLatin1()),
                               QByteArray::fromHex(root["alternate"].toString().toLatin1())};
        QCOMPARE(root["retailCount"].toInt(), 4569);
        const auto cases = root["results"].toArray();
        QCOMPARE(cases.size(), 4644);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            const auto initial = QByteArray::fromHex(c["initial"].toString().toLatin1());
            const auto source = QByteArray::fromHex(c["source"].toString().toLatin1());
            InitializedDialogue result;
            try {
                result = initializeDialogueWindow(initial, c["slot"].toInt(), source, environment);
            } catch (const std::exception &e) {
                QFAIL(qPrintable(c["id"].toString() + ": " + e.what()));
            }
            QVERIFY2(result.window.toHex() == c["window"].toString().toLatin1(),
                     qPrintable(c["id"].toString() + " window"));
            QVERIFY2(result.state.toHex() == c["state"].toString().toLatin1(),
                     qPrintable(c["id"].toString() + " payload"));
            QCOMPARE(result.warnings, quint32(c["warnings"].toInt()));
            QCOMPARE(result.cursor, u32(result.state, 8));
        }
        // Fixed dimensions bypass unavailable measurements, just as native code does.
        const auto fixed = initializeDialogueWindow(
            QByteArray(168, 0), 0, QByteArray("@*Body\0", 7), DialogueTextEnvironment{});
        QCOMPARE(u16(fixed.state, 0x1a), 36);
        QCOMPARE(u16(fixed.state, 0x1e), 3);
        QVERIFY_THROWS_EXCEPTION(Error, initializeDialogueWindow(QByteArray(168, 0), 0,
                                                                 QByteArray("Body\0", 5),
                                                                 DialogueTextEnvironment{}));
    }
    void dialogueWidthsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-width-mips.cjs"),
                                 path, work.filePath("widths.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("widths.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        QHash<int, QByteArray> substitutions;
        const auto entries = root["substitutions"].toArray();
        for (int i = 0; i < entries.size(); ++i)
            substitutions[i] = QByteArray::fromHex(entries[i].toString().toLatin1());
        const auto suffix = QByteArray::fromHex(root["suffix"].toString().toLatin1());
        DialogueFontMetrics metrics{QByteArray::fromHex(root["normal"].toString().toLatin1()),
                                    QByteArray::fromHex(root["alternate"].toString().toLatin1())};
        QCOMPARE(root["retailCount"].toInt(), 4569);
        const auto cases = root["results"].toArray();
        QCOMPARE(cases.size(), 6169);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            metrics.fixedIndex = qint16(c["fixed"].toInt());
            const auto source = c["source"].isNull()
                                    ? QByteArray{}
                                    : QByteArray::fromHex(c["source"].toString().toLatin1());
            quint16 width = 0;
            try {
                width = measureDialogueWidth(source, quint16(c["columns"].toInt()), substitutions,
                                             suffix, metrics, c["minimum"].toInt());
            } catch (const std::exception &e) {
                QFAIL(qPrintable(c["id"].toString() + ": " + e.what()));
            }
            QVERIFY2(width == c["width"].toInt(), qPrintable(QString("%1, actual %2 expected %3")
                                                                 .arg(c["id"].toString())
                                                                 .arg(width)
                                                                 .arg(c["width"].toInt())));
        }
        QVERIFY_THROWS_EXCEPTION(Error,
                                 measureDialogueWidth(QByteArray("@%0\0", 4), 36, {}, {}, metrics));
        QVERIFY_THROWS_EXCEPTION(Error, measureDialogueWidth(QByteArray("@!7X\0", 5), 36,
                                                             substitutions, suffix, metrics));
        substitutions[48] = QByteArray{};
        QVERIFY_THROWS_EXCEPTION(
            Error, measureDialogueWidth(QByteArray("@%0\0", 4), 36, substitutions, {}, metrics));
        QVERIFY_THROWS_EXCEPTION(Error,
                                 measureDialogueWidth(QByteArray("@w\0", 3), 36, {}, {}, metrics));
        QVERIFY_THROWS_EXCEPTION(
            Error, measureDialogueWidth(QByteArray("ABC\0", 4), 36, {}, {}, metrics, 0, 2));
    }
    void generalTextMeasurementsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("text-measure-mips.cjs"),
             path, work.filePath("widths.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("widths.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        DialogueFontMetrics metrics{QByteArray::fromHex(root["normal"].toString().toLatin1()),
                                    QByteArray::fromHex(root["alternate"].toString().toLatin1())};
        const auto cases = root["results"].toArray();
        QCOMPARE(cases.size(), 1851);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            metrics.fixedIndex = qint16(c["fixed"].toInt());
            const auto source = QByteArray::fromHex(c["source"].toString().toLatin1());
            const auto width = measureNativeText(source, metrics);
            QVERIFY2(width == c["width"].toInt(),
                     qPrintable(QString("Source %1, actual %2 expected %3")
                                    .arg(c["source"].toString())
                                    .arg(width)
                                    .arg(c["width"].toInt())));
        }
        QVERIFY_THROWS_EXCEPTION(Error, measureNativeText(QByteArray("{Qno end\0", 9), metrics));
        QVERIFY_THROWS_EXCEPTION(Error, measureNativeText(QByteArray("{T0}\0", 5), metrics));
        QVERIFY_THROWS_EXCEPTION(Error, measureNativeText(QByteArray("ABC\0", 4), metrics, 2));
        QVERIFY_THROWS_EXCEPTION(Error, measureNativeText(QByteArray("A\0", 2), {}));
    }
    void lineCountsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-lines-mips.cjs"),
                                 path, work.filePath("lines.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("lines.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        QHash<int, QByteArray> substitutions;
        const auto entries = root["substitutions"].toArray();
        for (int i = 0; i < entries.size(); ++i)
            substitutions[i] = entries[i].isNull()
                                   ? QByteArray{}
                                   : QByteArray::fromHex(entries[i].toString().toLatin1());
        const auto suffix = QByteArray::fromHex(root["suffix"].toString().toLatin1());
        QCOMPARE(root["retailCount"].toInt(), 4569);
        const auto cases = root["results"].toArray();
        QCOMPARE(cases.size(), 32305);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            const auto result =
                countDialogueLines(QByteArray::fromHex(c["source"].toString().toLatin1()),
                                   quint16(c["columns"].toInt()), substitutions, suffix);
            QVERIFY2(result == c["lines"].toInt(),
                     qPrintable(c["id"].toString() + " columns " +
                                QString::number(c["columns"].toInt()) + " actual " +
                                QString::number(result) + " expected " +
                                QString::number(c["lines"].toInt())));
        }
        QVERIFY_THROWS_EXCEPTION(Error, countDialogueLines(QByteArray{}, 36, {}, {}));
        QVERIFY_THROWS_EXCEPTION(Error, countDialogueLines(QByteArray("@%0\0", 4), 36, {}, {}));
        QVERIFY_THROWS_EXCEPTION(Error, countDialogueLines(QByteArray("@w\0", 3), 36, {}, {}));
        QVERIFY_THROWS_EXCEPTION(Error, countDialogueLines(QByteArray("ABC\0", 4), 36, {}, {}, 2));
    }
    void segmentScanningAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("dialogue-segment-mips.cjs"),
                                 path, work.filePath("segments.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("segments.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        QHash<int, QByteArray> substitutions;
        const auto entries = root["substitutions"].toArray();
        for (int i = 0; i < entries.size(); ++i)
            substitutions[i] = entries[i].isNull()
                                   ? QByteArray{}
                                   : QByteArray::fromHex(entries[i].toString().toLatin1());
        QCOMPARE(root["retailCount"].toInt(), 4569);
        const auto cases = root["results"].toArray();
        QCOMPARE(cases.size(), 23070);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            const auto source = c["source"].isNull()
                                    ? QByteArray{}
                                    : QByteArray::fromHex(c["source"].toString().toLatin1());
            const auto result =
                scanDialogueSegment(source, quint16(c["columns"].toInt()), substitutions);
            QVERIFY2(result.count == c["count"].toInt(),
                     qPrintable(c["id"].toString() + " count, columns " +
                                QString::number(c["columns"].toInt())));
            QCOMPARE(result.cursor, quint32(c["cursor"].toInt()));
            QCOMPARE(result.stop, c["stop"].toInt());
            QCOMPARE(result.published, c["published"].toBool());
        }
        QVERIFY_THROWS_EXCEPTION(Error, scanDialogueSegment(QByteArray("@%0\0", 4), 36, {}));
        substitutions[48] = QByteArray("@%0\0", 4);
        QVERIFY_THROWS_EXCEPTION(Error,
                                 scanDialogueSegment(QByteArray("@%0\0", 4), 36, substitutions));
        QVERIFY_THROWS_EXCEPTION(Error, scanDialogueSegment(QByteArray("{T05\0", 5), 36, {}));
        QVERIFY_THROWS_EXCEPTION(Error, scanDialogueSegment(QByteArray("@w\0", 3), 36, {}));
        QVERIFY_THROWS_EXCEPTION(Error, scanDialogueSegment(QByteArray("ABC\0", 4), 36, {}, 2));
    }
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
            DialogueTextEnvironment environment;
            environment.storage[0x12345678] = "unrelated";
            environment.references[90] = 0x12345678;
            context.supplyTextSources(environment);
            QCOMPARE(environment.substitution(90), QByteArray("unrelated"));
            QCOMPARE(environment.storage[0x8022ac00].toHex(),
                     expected["owner"].toString().toLatin1());
            QCOMPARE(environment.storage[0x8022ac40].toHex(),
                     expected["group"].toString().toLatin1());
            const auto refs = expected["references"].toArray();
            for (int i = 0; i < 4; ++i) {
                QCOMPARE(environment.references[48 + i], quint64(refs[i].toDouble()));
                if (!environment.references[48 + i])
                    QVERIFY(environment.substitution(48 + i).isNull());
                else {
                    const auto text = environment.substitution(48 + i);
                    QCOMPARE(text.left(text.indexOf('\0')), context.name(i));
                }
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
        QCOMPARE(cases.size(), 349);
        QCOMPARE(DialogueArchive::directory(rom).size(), 503);
        int entries = 0, alternateEntries = 0;
        QCOMPARE(DialogueArchive::directory(rom, DialogueArchive::Table::Alternate).size(), 1);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            const bool alternate = c["root"].toInt() == 0x21b8ba4;
            const auto table =
                alternate ? DialogueArchive::Table::Alternate : DialogueArchive::Table::Primary;
            const auto archive = DialogueArchive::load(rom, c["selector"].toInt(), table);
            QCOMPARE(archive.bytes().toHex(), c["bytes"].toString().toLatin1());
            const auto expected = c["entries"].toArray();
            QCOMPARE(archive.entryCount(), expected.size());
            for (int i = 0; i < expected.size(); ++i)
                QCOMPARE(archive.entry(i).toHex(), expected[i].toString().toLatin1());
            QByteArray window(168, 0);
            put16(window, 0x88, (alternate ? 0x8000 : 0) | (archive.entryCount() - 1));
            put16(window, 0x96, c["selector"].toInt());
            QCOMPARE(DialogueArchive::windowSource(rom, window),
                     archive.entry(archive.entryCount() - 1) + char(0));
            if (alternate)
                alternateEntries += archive.entryCount();
            else
                entries += archive.entryCount();
        }
        QCOMPARE(entries, 4569);
        QVERIFY(alternateEntries > 0);
        QByteArray unsupported(168, 0);
        put16(unsupported, 0x88, 0x4000);
        QVERIFY_THROWS_EXCEPTION(Error, DialogueArchive::windowSource(rom, unsupported));
        put16(unsupported, 0x88, 0xc000);
        QVERIFY_THROWS_EXCEPTION(Error, DialogueArchive::windowSource(rom, unsupported));
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
