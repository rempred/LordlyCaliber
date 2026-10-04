#include "engine/rom.h"
#include "engine/scene_actors.h"
#include "game/director/scene_schedule.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class SceneRuntimeTests : public QObject {
    Q_OBJECT
  private slots:
    void heightAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("actor-height-mips.cjs"),
             path, work.filePath("height.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("height.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 247);
        const auto rom = Rom::open(path);
        for (const auto &v : cases) {
            const auto row = v.toObject();
            const int slot = row["slot"].toInt();
            SceneActors scene(rom, 2, 0);
            scene.supplyActor(slot, QByteArray::fromHex(row["initialActor"].toString().toLatin1()));
            scene.supplyHeightJob(
                slot, ActorHeightJob(QByteArray::fromHex(row["initial"].toString().toLatin1())));
            if (row["args"].isArray()) {
                QVector<quint32> args{quint32(slot)};
                for (const auto &a : row["args"].toArray())
                    args << quint32(a.toInt());
                int ignored = 0;
                QVERIFY(scene.service(0x7b, args, ignored));
            }
            const auto states = row["states"].toArray();
            for (int step = 0; step < states.size(); ++step) {
                if (step)
                    scene.advanceHeights();
                const auto expected = states[step].toObject();
                QCOMPARE(scene.actor(slot)->record().toHex(),
                         expected["actor"].toString().toLatin1());
                QCOMPARE(scene.heightJob(slot).record().toHex(),
                         expected["job"].toString().toLatin1());
            }
        }
        // Failure in a later slot must not commit an earlier actor's update.
        SceneActors scene(rom, 2, 0);
        const auto initial =
            QByteArray::fromHex(cases[0].toObject()["initialActor"].toString().toLatin1());
        scene.supplyActor(0, initial);
        int ignored = 0;
        QVERIFY(scene.service(0x7b, {0, 1, 5, 4}, ignored));
        QVERIFY(scene.service(0x7b, {27, 1, 5, 4}, ignored));
        const auto job = scene.heightJob(0).record();
        QVERIFY_THROWS_EXCEPTION(Error, scene.advanceHeights());
        QCOMPARE(scene.actor(0)->record(), initial);
        QCOMPARE(scene.heightJob(0).record(), job);
        QVERIFY_THROWS_EXCEPTION(Error, scene.service(0x7b, {0, 0xffffffffU, 100, 0}, ignored));
        QCOMPARE(scene.heightJob(0).record(), job);
        QVERIFY(scene.service(0x7b, {0, 0, 5, 4}, ignored));
        const auto invalid = scene.heightJob(0).record();
        QVERIFY_THROWS_EXCEPTION(Error, scene.advanceHeights());
        QCOMPARE(scene.heightJob(0).record(), invalid);
    }
    void schedulerAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("scene-schedule-mips.cjs"),
                                 path, work.filePath("schedule.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("schedule.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 265);
        for (const auto &v : cases) {
            const auto row = v.toObject();
            // Repeat with a one-service host budget and a pause before every
            // service. Both must preserve native order and all state snapshots.
            for (const bool interrupted : {false, true}) {
                struct Context {
                    LordlySceneSchedule state{};
                    QJsonObject change;
                    QJsonArray calls;
                    bool interrupted = false, retry = false;
                } context;
                context.state.mode = quint8(row["mode"].toInt());
                context.state.alternate = quint8(row["alternate"].toInt());
                context.state.delayed_counter = quint8(row["counter"].toInt());
                context.change = row["change"].toObject();
                context.interrupted = interrupted;
                auto invoke = [](void *opaque, LordlySceneService service) -> int {
                    auto &c = *static_cast<Context *>(opaque);
                    if (c.interrupted && !c.retry) {
                        c.retry = true;
                        return 0;
                    }
                    c.retry = false;
                    const auto address = lordly_scene_service_address(service);
                    c.calls.append(QJsonObject{{"address", qint64(address)},
                                               {"mode", c.state.mode},
                                               {"alternate", c.state.alternate},
                                               {"counter", c.state.delayed_counter}});
                    if (c.change["address"].toInteger() == address) {
                        c.state.mode = quint8(c.change["mode"].toInt());
                        c.state.alternate = quint8(c.change["alternate"].toInt());
                        c.state.delayed_counter = quint8(c.change["counter"].toInt());
                    }
                    return 1;
                };
                int result, attempts = 0;
                do {
                    result = lordly_scene_schedule(&context.state, invoke, &context,
                                                   interrupted ? 1 : 100);
                    QVERIFY(++attempts < 100);
                    QVERIFY(result != LORDLY_SCENE_INVALID);
                } while (result != LORDLY_SCENE_COMPLETE);
                QCOMPARE(context.calls, row["sequence"].toArray());
                const auto after = row["after"].toObject();
                QCOMPARE(context.state.mode, after["mode"].toInt());
                QCOMPARE(context.state.alternate, after["alternate"].toInt());
                QCOMPARE(context.state.delayed_counter, after["counter"].toInt());
                QCOMPARE(context.state.active, 0);
            }
        }
    }
};
QTEST_GUILESS_MAIN(SceneRuntimeTests)
#include "scene_runtime_tests.moc"
