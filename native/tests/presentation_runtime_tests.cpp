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
    void rosterMaterializationAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node", {QFINDTESTDATA("actor-roster-mips.cjs"), path, work.filePath("roster.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile f(work.filePath("roster.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto results = QJsonDocument::fromJson(f.readAll()).object()["results"].toArray();
        QCOMPARE(results.size(), 50);
        const auto rom = Rom::open(path);
        int index = 0;
        try {
            for (const auto &v : results) {
                const auto c = v.toObject();
                SceneActors scene(rom, 2, 60, [](int, float, float) { return 123.75f; });
                QVector<QByteArray> rows;
                for (const auto &r : c["rows"].toArray())
                    rows << bytes(r);
                scene.supplyBodyRows(rows);
                for (const auto &o : c["objects"].toArray()) {
                    const auto r = o.toObject();
                    scene.supplyBodyObject(quint32(r["address"].toInteger()), bytes(r["record"]));
                }
                PresentationRoster roster;
                roster.sideGroups = {0, 30};
                for (int bank = 0; bank < 2; ++bank)
                    for (int member = 0; member < 100; ++member)
                        roster.members[bank].insert(member, {quint16(member == 9 ? 0 : 1), 0});
                scene.supplyRosterLaunch(
                    {qint8(c["context"].toInt()), qint16(c["scenario"].toInt()), roster});
                int unused = -100;
                QVERIFY(scene.service(c["variant"].toInt() ? 0xab : 0x45,
                                      {quint32(c["first"].toInt()), quint32(c["second"].toInt())},
                                      unused));
                QCOMPARE(unused, -100);
                int count = 0;
                for (int i = 0; i < 28; ++i)
                    count += scene.actor(i).has_value();
                QCOMPARE(count, c["actors"].toArray().size());
                for (const auto &a : c["actors"].toArray()) {
                    const auto r = a.toObject();
                    const int slot = r["slot"].toInt();
                    QVERIFY(scene.actor(slot));
                    const auto actual = scene.actor(slot)->record(), expected = bytes(r["record"]);
                    if (actual != expected) {
                        int at = 0;
                        while (at < actual.size() && actual[at] == expected[at])
                            ++at;
                        QFAIL(qPrintable(
                            QString("case%1 slot%2 byte%3 actual%4 expected%5")
                                .arg(index)
                                .arg(slot)
                                .arg(at, 0, 16)
                                .arg(actual.mid(at, 16).toHex(), expected.mid(at, 16).toHex())));
                    }
                }
                QVector<int> draws;
                for (const auto &d : c["draws"].toArray())
                    draws << d.toInt();
                QCOMPARE(scene.immediateRosterDraws(), draws);
                ++index;
            }
        } catch (const std::exception &e) {
            QFAIL(qPrintable(QString("case%1: %2").arg(index).arg(e.what())));
        }
    }
    void rosterMutableLeaderScan() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        QFile f(QFINDTESTDATA("../../tests/fixtures/cutscene-roster-construction.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(f.readAll()).object()["finalizers"].toArray();
        QCOMPARE(cases.size(), 3);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            SceneActors scene(rom, 2, 0);
            QVector<QByteArray> rows(20, QByteArray(248, 0));
            const auto inputRows = c["rows"].toObject();
            for (auto it = inputRows.begin(); it != inputRows.end(); ++it) {
                const auto row = it.value().toObject();
                put32(rows[it.key().toInt()], 0x40, quint32(row["flags"].toInteger()));
                put32(rows[it.key().toInt()], 0x4c, quint32(row["context"].toInteger()));
            }
            scene.supplyBodyRows(rows);
            scene.supplyRosterLaunch({});
            QMap<QString, QByteArray> identities;
            const auto before = c["before"].toArray(), after = c["after"].toArray();
            for (int slot = 0; slot < 28; ++slot) {
                if (before[slot].isNull())
                    continue;
                const auto name = before[slot].toString();
                const auto a = c["actors"].toObject()[name].toObject();
                QByteArray record(336, 0);
                put32(record, 0xe4, slot);
                put32(record, 0xe8, 30);
                put8(record, 0x147, a["row"].toInt());
                put16(record, 0x138, a["state"].toInt());
                // Identity marker outside fields touched by the normalizer.
                put32(record, 0xdc, quint32(slot + 1));
                scene.supplyActor(slot, record);
                identities[name] = record;
            }
            scene.materializeRoster(0, 0, 0);
            for (int slot = 0; slot < 28; ++slot) {
                QCOMPARE(scene.actor(slot).has_value(), !after[slot].isNull());
                if (after[slot].isNull())
                    continue;
                auto expected = identities[after[slot].toString()];
                put32(expected, 0xe4, slot);
                QCOMPARE(scene.actor(slot)->record(), expected);
            }
        }
    }
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

            const auto entrances = data["entrances"].toArray();
            QCOMPARE(entrances.size(), 20);
            int entranceIndex = 0;
            for (const auto &v : entrances) {
                const auto c = v.toObject();
                context = QString("entrance %1 construction").arg(entranceIndex);
                SceneActors scene(rom, 2, 0x3c, [](int, float, float) { return 7.75f; });
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
                PresentationRoster roster;
                roster.overrideFlag = quint8(c["overrideFlag"].toInt());
                for (int bank = 0; bank < 2; ++bank) {
                    roster.sideGroups[bank] = quint8(c["sideGroups"].toArray()[bank].toInt());
                    for (const auto &m : c["members"].toArray()[bank].toArray()) {
                        const auto member = m.toObject();
                        roster.members[bank].insert(
                            member["id"].toInt(),
                            {quint16(member["value"].toInt()), quint8(member["flags"].toInt())});
                    }
                }
                scene.supplyPresentationLaunch(
                    {c["preset"].toInt(), qint8(c["variant"].toInt()), roster});
                int value = -100;
                QVERIFY(scene.service(0x3f, {0xa7a7a7a7U}, value));
                QCOMPARE(value, -100);
                const auto states = c["states"].toArray();
                for (int tick = 0; tick < states.size(); ++tick) {
                    context = QString("entrance %1 tick %2").arg(entranceIndex).arg(tick);
                    if (tick) {
                        scene.advanceMovement();
                        QVERIFY(scene.advancePoses());
                        scene.advancePresentation();
                    }
                    const auto expected = states[tick].toObject();
                    QVERIFY2(scene.presentationRecord() == bytes(expected["presentation"]),
                             qPrintable(context));
                    QCOMPARE(scene.presentationOpacity(),
                             std::optional{quint8(expected["opacity"].toInt())});
                    QJsonArray actualActors, motions;
                    for (int slot = 0; slot < 28; ++slot) {
                        if (scene.actor(slot))
                            actualActors << QJsonObject{
                                {"slot", slot},
                                {"record",
                                 QString::fromLatin1(scene.actor(slot)->record().toHex())}};
                        if (scene.motion(slot).present)
                            motions << QJsonObject{
                                {"slot", slot},
                                {"record",
                                 QString::fromLatin1(motionBytes(scene.motion(slot)).toHex())}};
                    }
                    if (actualActors != expected["actors"].toArray()) {
                        const auto want = expected["actors"].toArray();
                        QCOMPARE(actualActors.size(), want.size());
                        for (int i = 0; i < want.size(); ++i) {
                            const auto a = actualActors[i].toObject(), b = want[i].toObject();
                            QCOMPARE(a["slot"], b["slot"]);
                            const auto actual = bytes(a["record"]), desired = bytes(b["record"]);
                            if (actual != desired) {
                                int offset = 0;
                                while (offset < actual.size() && actual[offset] == desired[offset])
                                    ++offset;
                                QFAIL(qPrintable(
                                    context +
                                    QString(" slot %1 byte %2: %3 vs %4")
                                        .arg(a["slot"].toInt())
                                        .arg(offset, 0, 16)
                                        .arg(
                                            QString::fromLatin1(actual.mid(offset, 12).toHex()),
                                            QString::fromLatin1(desired.mid(offset, 12).toHex()))));
                            }
                        }
                    }
                    QVERIFY2(motions == expected["motions"].toArray(),
                             qPrintable(context + " motion"));
                }
                ++entranceIndex;
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
        SceneActors entrance(rom, 2, 0, [](int, float, float) { return 7.75f; });
        QVERIFY_THROWS_EXCEPTION(Error, entrance.startEntrance());
        entrance.supplyPresentationLaunch({64, 0, {}});
        QVERIFY_THROWS_EXCEPTION(Error, entrance.startEntrance());
        QVERIFY(!entrance.presentationOpacity());
        rows.resize(20);
        for (auto &row : rows)
            if (row.isEmpty())
                row = QByteArray(248, 0);
        entrance.supplyBodyRows(rows);
        entrance.supplyPresentationLaunch({0, -3, {}});
        QVERIFY_THROWS_EXCEPTION(Error, entrance.startEntrance());
        QVERIFY(entrance.presentationRecord().isEmpty());
        entrance.supplyBodyObject(0x80330000U, object);
        entrance.supplyBodyObject(0x80330040U, object);
        // The override bypasses member reads; an otherwise empty roster is valid.
        PresentationRoster roster;
        roster.overrideFlag = 1;
        entrance.supplyPresentationLaunch({0, -3, roster});
        entrance.startEntrance();
        QVERIFY(entrance.actor(0));
        QVERIFY(entrance.actor(1));
        QCOMPARE(entrance.presentationOpacity(), std::optional{quint8(128)});
        SceneActors crowded(rom, 2, 0, [](int, float, float) { return 0.f; });
        for (int i = 0; i < 27; ++i) {
            auto occupied = before;
            put32(occupied, 0xe4, quint32(i));
            put8(occupied, 0x147, 255);
            crowded.supplyActor(i, occupied);
        }
        put32(rows[0], 4, 0x80330040U);
        put32(rows[0], 8, 0x80330080U);
        crowded.supplyBodyRows(rows);
        for (quint32 address : {0x80330000U, 0x80330040U, 0x80330080U})
            crowded.supplyBodyObject(address, object);
        crowded.supplyPresentationLaunch({0, 0, {}});
        const auto occupiedBefore = crowded.actor(0)->record();
        QVERIFY_THROWS_EXCEPTION(Error, crowded.startEntrance());
        QVERIFY(!crowded.actor(27));
        QVERIFY(crowded.presentationRecord().isEmpty());
        QVERIFY(!crowded.presentationOpacity());
        QCOMPARE(crowded.actor(0)->record(), occupiedBefore);
        QVERIFY_THROWS_EXCEPTION(Error, crowded.materializeRoster(-1,-1,0));
        crowded.supplyRosterLaunch({});
        QVERIFY_THROWS_EXCEPTION(Error, crowded.materializeRoster(-1,-1,0));
        QVERIFY(!crowded.actor(27));
        QCOMPARE(crowded.actor(0)->record(),occupiedBefore);
        QVERIFY(crowded.immediateRosterDraws().isEmpty());
        SceneActors empty(rom, 2, 0);
        empty.advancePresentation();
        QVERIFY(empty.presentationRecord().isEmpty());
    }
};
QTEST_GUILESS_MAIN(PresentationRuntimeTests)
#include "presentation_runtime_tests.moc"
