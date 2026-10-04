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
