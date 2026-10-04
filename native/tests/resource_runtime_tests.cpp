#include "engine/scene_resource_dispatch.h"
#include "engine/scene_resources.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class ResourceRuntimeTests : public QObject {
    Q_OBJECT
  private slots:
    void dispatcherAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("ui-dispatch-mips.cjs"),
             path, work.filePath("dispatch.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("dispatch.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 192);
        for (const auto &value : cases)
            for (const bool interrupted : {false, true}) {
                const auto c = value.toObject();
                SceneResources pool({1, -1, 0xffff});
                auto initial = QByteArray::fromHex(c["initial"].toString().toLatin1());
                for (int i = 0; i < 6; ++i)
                    pool.supplyRecord(i, initial.mid(i * 168, 168));
                SceneResourcePassState state;
                state.randomSeed = quint32(c["seed"].toDouble());
                state.cleanupStatus = quint16(c["status"].toInt());
                state.renderContext = 0x803e0000;
                state.selectedInput = c["selected"].toBool();
                state.queue.count = quint16(c["count"].toInt());
                for (int i = 0; i < 6; ++i)
                    state.queue.entries[i] = quint16(c["entries"].toArray()[i].toInt());
                SceneResourceDispatcher dispatch(state);
                QHash<quint32, QByteArray> payloads;
                for (int i = 0; i < 6; ++i)
                    payloads[0x80300000 + i * 256] = QByteArray(32, char(0x80 + i));
                QByteArray data(32, char(-18));
                bool created = false, spawned = false, pause = interrupted;
                const int mode = c["mode"].toInt();
                auto fresh = [](int slot) {
                    QByteArray r(168, 0);
                    put16(r, 0, 0x8000);
                    put8(r, 4, 2);
                    put16(r, 14, 19);
                    put32(r, 16, 0x802ff100);
                    put32(r, 20, 0x802ff104);
                    put16(r, 32, 32);
                    put32(r, 36, 0x80300000 + slot * 256);
                    put16(r, 0xa2, 0xffff);
                    return r;
                };
                QJsonArray trace;
                auto handler = [&](SceneResources &next, QByteArray &r, SceneResourcePassState &s,
                                   const SceneResourceDispatcher::Call &call) {
                    if (pause) {
                        // Deliberate failed-service writes must not escape.
                        next.supplyRecord(0, QByteArray(168, char(-1)));
                        r.fill(char(-1));
                        s.randomSeed = 123;
                        s.queue.count = 123;
                        pause = false;
                        return false;
                    }
                    pause = interrupted;
                    const int i = call.slot;
                    using Service = SceneResourceDispatcher::Service;
                    switch (call.service) {
                    case Service::Initialize:
                        trace.append(QJsonObject{{"op", "init"},
                                                 {"slot", i},
                                                 {"head", bool(u8(r, 2) & 4)},
                                                 {"input", s.selectedInput},
                                                 {"speed", int(u8(r, 4))},
                                                 {"context", double(u32(r, 0x98))}});
                        data = QByteArray(32, char(0x30 + i));
                        put8(r, 0x60, u8(r, 0x60) + 1);
                        if (mode == 2 && i == 5 && !created && !next.active(0)) {
                            next.supplyRecord(0, fresh(0));
                            created = true;
                        }
                        if (mode == 2 && i == 2)
                            put8(r, 2, u8(r, 2) | 2);
                        break;
                    case Service::CaptureRender:
                        put32(r, 0x98, 0); // Known empty render list, as in the reference.
                        break;
                    case Service::RestorePayload:
                        trace.append(QJsonObject{{"op", "restore"}, {"slot", i}});
                        data = payloads[u32(r, 0x24)];
                        break;
                    case Service::Update:
                        trace.append(QJsonObject{{"op", "update"},
                                                 {"slot", i},
                                                 {"head", bool(u8(r, 2) & 4)},
                                                 {"input", s.selectedInput},
                                                 {"context", next.context().context},
                                                 {"before", QString(data.toHex())}});
                        put8(r, 0x60, u8(r, 0x60) + 3);
                        put8(data, 0, u8(data, 0) + 1);
                        if (mode == 2 && i == 1)
                            put8(r, 2, u8(r, 2) | 2);
                        if (mode == 2 && i == 3 && !spawned && !next.active(4)) {
                            next.supplyRecord(4, fresh(4));
                            spawned = true;
                        }
                        break;
                    case Service::SavePayload:
                        trace.append(QJsonObject{{"op", "save"},
                                                 {"slot", i},
                                                 {"head", bool(u8(r, 2) & 4)},
                                                 {"data", QString(data.toHex())}});
                        payloads[u32(r, 0x24)] = data;
                        break;
                    case Service::Release:
                        for (const auto handle : next.release(i))
                            trace.append(QJsonObject{{"op", "free"}, {"handle", double(handle)}});
                        break;
                    case Service::AdvanceRender:
                        throw Error("Unexpected render node in dispatcher fixture");
                    }
                    return true;
                };
                for (const auto &snapshot : c["snapshots"].toArray()) {
                    const auto expected = snapshot.toObject();
                    trace = {};
                    int attempts = 0;
                    SceneResourceDispatcher::Result result;
                    do {
                        result = dispatch.advance(pool, handler, interrupted ? 1 : 1000);
                        QVERIFY2(++attempts < 2000, qPrintable(c["id"].toString()));
                    } while (result != SceneResourceDispatcher::Complete);
                    QVERIFY(!dispatch.inProgress());
                    QVERIFY(!dispatch.pending());
                    QCOMPARE(trace, expected["trace"].toArray());
                    QByteArray actual;
                    for (int i = 0; i < 6; ++i)
                        actual += pool.record(i);
                    QVERIFY2(actual.toHex() == expected["pool"].toString().toLatin1(),
                             qPrintable(c["id"].toString()));
                    QCOMPARE(dispatch.state().randomSeed, quint32(expected["seed"].toDouble()));
                    QCOMPARE(int(dispatch.state().queue.count), expected["count"].toInt());
                    QCOMPARE(pool.context().context, expected["current"].toInt());
                    QCOMPARE(dispatch.state().selectedInput, expected["input"].toBool());
                    for (int i = 0; i < 6; ++i) {
                        QCOMPARE(int(dispatch.state().queue.entries[i]),
                                 expected["entries"].toArray()[i].toInt());
                        QCOMPARE(payloads[0x80300000 + i * 256].toHex(),
                                 expected["payloads"].toArray()[i].toString().toLatin1());
                    }
                }
            }
    }
    void releaseAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("ui-release-mips.cjs"),
             path, work.filePath("release.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("release.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 384);
        for (const auto &value : cases) {
            const auto c = value.toObject();
            SceneResources pool({0, 0, 0xffff});
            const auto initial = QByteArray::fromHex(c["initial"].toString().toLatin1());
            for (int i = 0; i < 6; ++i)
                pool.supplyRecord(i, initial.mid(i * 168, 168));
            QJsonArray callbacks;
            const auto handles = pool.release(
                c["root"].toInt(), [&](SceneResources &next, int slot, quint32 address) {
                    callbacks.append(QJsonObject{{"slot", slot}, {"address", double(address)}});
                    if (c["mutate"].toBool() && slot == 1) {
                        auto child = next.record(4);
                        put16(child, 0xa2, 5);
                        next.supplyRecord(4, child);
                    }
                    auto record = next.record(slot);
                    put32(record, 0x38, 0x12340000 + slot);
                    next.supplyRecord(slot, record);
                });
            QCOMPARE(callbacks, c["callbacks"].toArray());
            QJsonArray freed;
            for (const auto handle : handles)
                freed.append(double(handle));
            QCOMPARE(freed, c["freed"].toArray());
            QByteArray actual;
            for (int i = 0; i < 6; ++i)
                actual += pool.record(i);
            QCOMPARE(actual.toHex(), c["after"].toString().toLatin1());
        }
        SceneResources pool({0, 0, 0xffff});
        auto record = QByteArray(168, 0);
        put16(record, 0, 0x8000);
        put8(record, 3, 8);
        put16(record, 0xa2, 1);
        pool.supplyRecord(0, record);
        put16(record, 0xa2, 0);
        pool.supplyRecord(1, record);
        const auto first = pool.record(0), second = pool.record(1);
        QVERIFY_THROWS_EXCEPTION(Error, pool.release(0));
        QCOMPARE(pool.record(0), first);
        QCOMPARE(pool.record(1), second);
        put8(record, 3, 0);
        put32(record, 0x1c, 0x801998a0);
        pool.supplyRecord(1, record);
        QVERIFY_THROWS_EXCEPTION(Error, pool.release(0));
        QCOMPARE(pool.record(0), first);
        QCOMPARE(pool.record(1), record);
    }
    void closingAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("ui-closing-mips.cjs"),
             path, work.filePath("closing.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("closing.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 224);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            SceneResources pool({c["margin"].toInt(), 0, 0xffff});
            const auto initial = QByteArray::fromHex(c["initial"].toString().toLatin1());
            for (int i = 0; i < 6; ++i)
                pool.supplyRecord(i, initial.mid(i * 168, 168));
            int update = 0;
            for (const auto &snapshot : c["snapshots"].toArray()) {
                const auto expected = snapshot.toObject();
                QJsonArray releases;
                for (int i = 0; i < c["count"].toInt(); ++i) {
                    const int slot = c["order"].toArray()[i].toInt();
                    if (pool.advanceClosing(slot)) {
                        releases.append(slot);
                        pool.supplyRecord(slot, QByteArray(168, 0));
                    }
                }
                QByteArray actual;
                for (int i = 0; i < 6; ++i)
                    actual += pool.record(i);
                QVERIFY2(actual.toHex() == expected["pool"].toString().toLatin1(),
                         qPrintable(c["id"].toString() + " update " + QString::number(update++)));
                QCOMPARE(releases, expected["releases"].toArray());
            }
        }
        SceneResources pool({1, 0, 0xffff});
        auto record = QByteArray(168, 0);
        put16(record, 0, 0xf000);
        put16(record, 10, 100);
        put16(record, 12, 100);
        pool.supplyRecord(0, record);
        QVERIFY_THROWS_EXCEPTION(Error, pool.advanceClosing(0));
        QCOMPARE(pool.record(0), record);
        put16(record, 10, 0);
        put16(record, 12, 0);
        pool.supplyRecord(0, record);
        QVERIFY(pool.advanceClosing(0)); // Already closed: no division or early clearing.
        QCOMPARE(pool.record(0), record);
    }
    void openingAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("ui-opening-mips.cjs"),
             path, work.filePath("opening.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("opening.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 512);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            SceneResources pool({0, 0, 0xffff});
            SceneResourceQueue queue{};
            queue.count = quint16(c["count"].toInt());
            const auto initial = QByteArray::fromHex(c["initial"].toString().toLatin1());
            for (int i = 0; i < 6; ++i) {
                pool.supplyRecord(i, initial.mid(i * 168, 168));
                queue.entries[i] = quint16(c["order"].toArray()[i].toInt());
            }
            int update = 0;
            for (const auto &expected : c["snapshots"].toArray()) {
                pool.advanceOpening(queue);
                QByteArray actual;
                for (int i = 0; i < 6; ++i)
                    actual += pool.record(i);
                QVERIFY2(actual.toHex() == expected.toString().toLatin1(),
                         qPrintable(c["id"].toString() + " update " + QString::number(update++)));
            }
        }
        SceneResources pool({0, 0, 0xffff});
        SceneResourceQueue queue{2, {0, 1, 2, 3, 4, 5}};
        auto record = QByteArray(168, 0);
        put16(record, 0, 0xe800);
        put8(record, 4, 8);
        put16(record, 0x28, 100);
        put16(record, 0x2a, 61);
        pool.supplyRecord(0, record);
        put8(record, 4, 0);
        pool.supplyRecord(1, record);
        const auto first = pool.record(0);
        QVERIFY_THROWS_EXCEPTION(Error, pool.advanceOpening(queue));
        QCOMPARE(pool.record(0), first);
        QCOMPARE(pool.record(1), record);
        queue.count = 7;
        QVERIFY_THROWS_EXCEPTION(Error, pool.advanceOpening(queue));
        queue.count = 1;
        queue.entries[0] = 6;
        QVERIFY_THROWS_EXCEPTION(Error, pool.advanceOpening(queue));
    }
    void priorityAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("ui-priority-mips.cjs"),
             path, work.filePath("priority.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("priority.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 384);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            SceneResources pool({0, 0, 0xffff});
            SceneResourceQueue queue{};
            queue.count = 65535;
            const auto initial = QByteArray::fromHex(c["initial"].toString().toLatin1());
            for (int i = 0; i < 6; ++i) {
                pool.supplyRecord(i, initial.mid(i * 168, 168));
                queue.entries[i] = quint16(c["entries"].toArray()[i].toInt());
            }
            pool.rebuildPriority(queue);
            QCOMPARE(queue.count, c["count"].toInt());
            QByteArray actual;
            for (int i = 0; i < 6; ++i) {
                actual += pool.record(i);
                QCOMPARE(queue.entries[i], c["queue"].toArray()[i].toInt());
            }
            QCOMPARE(actual.toHex(), c["after"].toString().toLatin1());
        }
    }
    void registrationAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("ui-resource-mips.cjs"),
             path, work.filePath("resources.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("resources.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 1116);
        for (const auto &v : cases) {
            const auto c = v.toObject();
            SceneResources resources(
                {c["margin"].toInt(), c["context"].toInt(), quint16(c["parent"].toInt())});
            const auto initial = QByteArray::fromHex(c["initial"].toString().toLatin1());
            for (int i = 0; i < 6; ++i)
                resources.supplyRecord(i, initial.mid(i * 168, 168));
            QVector<quint32> args;
            for (const auto &a : c["args"].toArray())
                args << quint32(qint64(a.toDouble()));
            int slot;
            if (c["window"].toBool())
                slot = resources.registerWindow(args[0], qint32(args[1]), qint32(args[2]),
                                                qint32(args[3]), qint32(args[4]));
            else
                slot = resources.registerResource(quint16(args[0]), args[1], qint32(args[2]),
                                                  qint32(args[3]), qint32(args[4]), qint32(args[5]),
                                                  qint32(args[6]));
            QCOMPARE(slot, c["slot"].toInt());
            QByteArray actual;
            for (int i = 0; i < 6; ++i)
                actual += resources.record(i);
            QCOMPARE(actual.toHex(), c["after"].toString().toLatin1());
        }
    }
    void exhaustionAndRecordBounds() {
        SceneResources pool({0, 0, 0xffff});
        for (int i = 0; i < 6; ++i) {
            QCOMPARE(pool.registerWindow(0x80198be8, 0, 0, 0, 0), i);
            QVERIFY(pool.active(i));
        }
        QVector<QByteArray> records;
        for (int i = 0; i < 6; ++i)
            records << pool.record(i);
        QVERIFY_THROWS_EXCEPTION(Error, pool.registerWindow(0x80198be8, 0, 0, 0, 0));
        QVERIFY_THROWS_EXCEPTION(Error, pool.registerResource(0x8000, 0x80225abc, 0, 0, 0, 0, 0));
        for (int i = 0; i < 6; ++i)
            QCOMPARE(pool.record(i), records[i]);
        QVERIFY_THROWS_EXCEPTION(Error, pool.supplyRecord(0, QByteArray(167, 0)));
        QVERIFY_THROWS_EXCEPTION(Error, pool.record(-1));
        QVERIFY_THROWS_EXCEPTION(Error, pool.record(6));
        auto released = records[3];
        put16(released, 0, 0x1234);
        pool.supplyRecord(3, released);
        pool.setContext({1, 0x1ffff, 2});
        QCOMPARE(pool.registerResource(0xc800, 0x80225abc, 0, 0, 0, 0, 0), 3);
        QCOMPARE(u16(pool.record(3), 0xa2), 0xffff);
        QCOMPARE(u16(pool.record(3), 0xa4), 2);
        QCOMPARE(pool.record(3).mid(0x88, 8), released.mid(0x88, 8));
    }
};
QTEST_GUILESS_MAIN(ResourceRuntimeTests)
#include "resource_runtime_tests.moc"
