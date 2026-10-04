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
