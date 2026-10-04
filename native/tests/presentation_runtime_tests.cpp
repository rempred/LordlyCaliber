#include "engine/scene_actors.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <bit>
using namespace lordly;
namespace {
QByteArray bytes(const QJsonValue &v) {
    return QByteArray::fromHex(v.toString().toLatin1());
}
QByteArray motionBytes(const LordlyActorMotion &m) {
    QByteArray b(16, 0);
    put32(b, 0, std::bit_cast<quint32>(m.vx));
    put32(b, 4, std::bit_cast<quint32>(m.vy));
    put32(b, 8, std::bit_cast<quint32>(m.vz));
    put16(b, 12, m.remaining);
    put8(b, 14, m.pause);
    put8(b, 15, m.retained);
    return b;
}
} // namespace
class PresentationRuntimeTests : public QObject {
    Q_OBJECT
  private slots:
    void lifecycleAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("actor-presentation-mips.cjs"),
                                 path, work.filePath("presentation.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("presentation.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        const auto rom = Rom::open(path);
        const auto results = data["results"].toArray();
        QCOMPARE(results.size(), 44);
        int caseIndex = 0;
        QString context;
        try {
            for (const auto &v : results) {
                const auto c = v.toObject();
                SceneActors scene(rom, 2, 0x3c, [](int, float, float) { return 0.f; });
                scene.supplySceneFlags(8);
                QVector<QByteArray> rows;
                for (const auto &r : c["rows"].toArray())
                    rows << bytes(r);
                scene.supplyBodyRows(rows);
                for (const auto &a : c["initial"].toArray()) {
                    const auto r = a.toObject();
                    scene.supplyActor(r["slot"].toInt(), bytes(r["record"]));
                }
                for (const auto &a : c["objects"].toArray()) {
                    const auto r = a.toObject();
                    scene.supplyBodyObject(quint32(r["address"].toDouble()), bytes(r["record"]));
                }
                int value = -100;
                QVERIFY(scene.service(0xa4, {quint32(c["group"].toInt())}, value));
                QCOMPARE(value, -100);
                const auto states = c["states"].toArray();
                for (int tick = 0; tick < states.size(); ++tick) {
                    context = QString("case %1 tick %2").arg(caseIndex).arg(tick);
                    if (tick) {
                        scene.advanceMovement();
                        QVERIFY(scene.advancePoses());
                        scene.advancePresentation();
                    }
                    const auto expected = states[tick].toObject();
                    QVERIFY2(
                        scene.presentationRecord() == bytes(expected["presentation"]),
                        qPrintable(QString("vector case %1 tick %2").arg(caseIndex).arg(tick)));
                    QVERIFY(scene.service(0x40, {0, 0}, value));
                    QCOMPARE(value, expected["active"].toInt());
                    QVERIFY(scene.service(0xa5, {0, 0}, value));
                    QCOMPARE(value, expected["active"].toInt());
                    for (const auto &a : expected["actors"].toArray()) {
                        const auto r = a.toObject();
                        const int slot = r["slot"].toInt();
                        const auto actual = scene.actor(slot)->record(), want = bytes(r["record"]);
                        if (actual != want) {
                            int offset = 0;
                            while (offset < actual.size() && actual[offset] == want[offset])
                                ++offset;
                            QFAIL(qPrintable(
                                QString("actor case %1 tick %2 slot %3 first differing byte %4: %5 "
                                        "vs %6")
                                    .arg(caseIndex)
                                    .arg(tick)
                                    .arg(slot)
                                    .arg(offset, 0, 16)
                                    .arg(QString::fromLatin1(actual.mid(offset, 12).toHex()),
                                         QString::fromLatin1(want.mid(offset, 12).toHex()))));
                        }
                    }
                    QJsonArray motions;
                    for (int slot = 0; slot < 28; ++slot)
                        if (scene.motion(slot).present)
                            motions << QJsonObject{
                                {"slot", slot},
                                {"record",
                                 QString::fromLatin1(motionBytes(scene.motion(slot)).toHex())}};
                    QCOMPARE(motions, expected["motions"].toArray());
                }
                ++caseIndex;
            }
            context = "boundaries";
            const auto boundaries = data["boundaries"].toArray();
            QCOMPARE(boundaries.size(), 168);
            for (const auto &v : boundaries) {
                const auto c = v.toObject();
                SceneActors scene(rom, 2, 0x3c);
                scene.supplyBodyRows({bytes(c["row"])});
                scene.supplyBodyObject(quint32(c["address"].toDouble()), bytes(c["object"]));
                scene.supplyActor(27, bytes(c["initial"]));
                scene.supplyPresentationRecord(bytes(c["vector"]));
                for (const auto &s : c["states"].toArray()) {
                    scene.advancePresentation();
                    const auto expected = s.toObject();
                    QCOMPARE(scene.presentationRecord(), bytes(expected["presentation"]));
                    QCOMPARE(scene.actor(27)->record(), bytes(expected["actor"]));
                }
            }
            context = "speeds";
            SceneActors speedScene(rom, 2, 0);
            QCOMPARE(data["speeds"].toArray().size(), 964);
            for (const auto &v : data["speeds"].toArray()) {
                const auto c = v.toObject();
                QCOMPARE(speedScene.bodySpeed(c["source"].toInt(), c["owner"].toInt(),
                                              c["direction"].toInt()),
                         float(c["value"].toDouble()));
            }
        } catch (const std::exception &e) {
            QFAIL(qPrintable(context + ": " + QString::fromUtf8(e.what())));
        }
    }
    void missingContextIsAtomic() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        SceneActors scene(rom, 2, 0);
        int ignored = -100;
        QVector<QByteArray> rows(2, QByteArray(248, 0));
        for (int i = 0; i < 2; ++i) {
            QVERIFY(scene.service(0x14, {quint32(i), 30, 0, 0, 0, 0, 0, 0, 0}, ignored));
            auto actor = scene.actor(i)->record();
            put8(actor, 0x147, quint8(i));
            put8(actor, 0x149, 0);
            scene.supplyActor(i, actor);
            put32(rows[i], 0, 0x80330000U + i * 64);
            put32(rows[i], 0x40, 0x100);
            put32(rows[i], 0x48, 30);
            put32(rows[i], 0x4c, 30);
        }
        scene.supplyBodyRows(rows);
        QByteArray object(48, 0);
        put16(object, 0x28, 40);
        scene.supplyBodyObject(0x80330000U, object);
        const auto before = scene.actor(0)->record();
        QVERIFY_THROWS_EXCEPTION(Error, scene.startReturn(0));
        QCOMPARE(scene.actor(0)->record(), before);
        QVERIFY(scene.presentationRecord().isEmpty());
        scene.supplyBodyObject(0x80330040U, object);
        scene.startReturn(0);
        QCOMPARE(scene.presentationRecord().size(), 29);
        scene.vacate(1);
        const auto vector = scene.presentationRecord(), actor = scene.actor(0)->record();
        QVERIFY_THROWS_EXCEPTION(Error, scene.advancePresentation());
        QCOMPARE(scene.presentationRecord(), vector);
        QCOMPARE(scene.actor(0)->record(), actor);
        QVERIFY_THROWS_EXCEPTION(Error, scene.supplyPresentationRecord(QByteArray(28, 4)));
        QVERIFY_THROWS_EXCEPTION(Error, scene.bodySpeed(30, 172, 0));
        QVERIFY_THROWS_EXCEPTION(Error, scene.bodySpeed(30, 30, 2));
        SceneActors empty(rom, 2, 0);
        empty.advancePresentation();
        QVERIFY(empty.presentationRecord().isEmpty());
    }
};
QTEST_GUILESS_MAIN(PresentationRuntimeTests)
#include "presentation_runtime_tests.moc"
