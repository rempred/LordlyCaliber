#include "engine/actor_art.h"
#include "engine/actor_pose.h"
#include "engine/class_data.h"
#include "game/director/actor_state.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <bit>
using namespace lordly;
class BodyArtTests : public QObject {
    Q_OBJECT
    QJsonObject fixture(const QString &name, const QString &path) {
        QTemporaryDir work;
        QProcess reference;
        reference.start("node",
                        {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath(name), path,
                         work.filePath("reference.json")});
        if (!reference.waitForFinished(60000) || reference.exitCode() != 0)
            throw Error(QString::fromUtf8(reference.readAllStandardError()));
        QFile file(work.filePath("reference.json"));
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing body-art fixture");
        return QJsonDocument::fromJson(file.readAll()).object();
    }
  private slots:
    void alternatePosesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto data = fixture("body-pose-mips.cjs", path);
        const auto rows = data["rows"].toArray();
        QCOMPARE(rows.size(), 17633);
        const auto rom = Rom::open(path);
        QMap<int, ActorArt> banks;
        int calls = 0;
        for (const auto &value : rows) {
            const auto row = value.toObject();
            const int source = row["source"].toInt(), flags = row["flags"].toInt(),
                      id = source * 4 + flags;
            if (!banks.contains(id))
                banks.insert(id, ActorArt::loadBody(rom, source, source, flags >> 1, flags & 1));
            ActorPosePlayer player(rom, banks[id], row["state"].toInt());
            player.setLinkedObjectKnownAbsent(true);
            player.actor().position = {1.25f, -2.5f, 3.75f};
            for (const auto &call : row["calls"].toArray()) {
                const auto expected = call.toArray();
                const int result = player.advance();
                QVERIFY2(result == LORDLY_POSE_OK, qPrintable(player.error()));
                const auto &a = player.actor();
                QCOMPARE(a.cursor, expected[0].toInt());
                QCOMPARE(a.delay, expected[1].toInt());
                QCOMPARE(a.frame, expected[2].toInt());
                QCOMPARE(a.state, expected[3].toInt());
                QCOMPARE(a.previous_state, expected[4].toInt());
                QCOMPARE(std::bit_cast<quint32>(a.position.x), quint32(expected[5].toInteger()));
                QCOMPARE(std::bit_cast<quint32>(a.position.y), quint32(expected[6].toInteger()));
                QCOMPARE(std::bit_cast<quint32>(a.position.z), quint32(expected[7].toInteger()));
                QByteArray material(reinterpret_cast<const char *>(a.material), 16);
                material.append(reinterpret_cast<const char *>(a.material_delta), 16);
                QCOMPARE(material.toHex(), expected[8].toString().toLatin1());
                QCOMPARE(player.result(), expected[9].toInt());
                QJsonArray audio;
                for (const auto &r : player.audioRequests())
                    audio << QJsonArray{r.context, r.value};
                QCOMPARE(audio, expected[10].toArray());
                ++calls;
            }
        }
        QCOMPARE(calls, 141064);
    }
    void handlesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto data = fixture("body-handle-mips.cjs", path);
        const auto rows = data["rows"].toArray();
        QCOMPARE(rows.size(), 108900);
        const auto bytes = QByteArray::fromHex(data["table"].toString().toLatin1());
        QVector<unsigned short> table;
        for (int i = 0; i < bytes.size(); i += 2)
            table << u16(bytes, i);
        const auto classes = ClassData::load(Rom::open(path));
        for (const auto &v : rows) {
            const auto r = v.toArray();
            unsigned short handle = 0;
            QCOMPARE(lordly_actor_body_handle(table.constData(), int(table.size()), r[0].toInt(),
                                              r[1].toInt(),
                                              u8(classes.entries()[r[0].toInt()].record, 0x45),
                                              r[2].toInt(), r[3].toInt(), &handle),
                     0);
            QCOMPARE(handle & 0xfff, r[4].toInt());
        }
        unsigned short retained = 0x1234;
        QCOMPARE(lordly_actor_body_handle(table.constData(), int(table.size()), 0, 9999, 0, 0, 0,
                                          &retained),
                 1);
        QCOMPARE(retained, 0x1234);
    }
    void bodyProgramsAndFrames() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto data = fixture("body-art-fixture.cjs", path);
        const auto rom = Rom::open(path);
        const auto routes = data["routes"].toArray(), banks = data["banks"].toArray();
        QCOMPARE(routes.size(), 660);
        QCOMPARE(banks.size(), 381);
        for (const auto &v : routes) {
            const auto route = v.toObject();
            const int source = route["sourceClass"].toInt(), flags = route["flags"].toInt();
            if (route["invalid"].toBool()) {
                QVERIFY_THROWS_EXCEPTION(
                    Error, ActorArt::loadBody(rom, source, source, flags >> 1, flags & 1));
                continue;
            }
            const auto art = ActorArt::loadBody(rom, source, source, flags >> 1, flags & 1);
            QCOMPARE(art.bodyHandle(), route["handle"].toInt());
            QCOMPARE(art.descriptorKey(), quint32(route["descriptorKey"].toInteger()));
        }
        int programs = 0, frames = 0, layers = 0;
        for (const auto &v : banks) {
            const auto row = v.toObject();
            const int source = row["sourceClass"].toInt(), flags = row["flags"].toInt();
            const auto art = ActorArt::loadBody(rom, source, source, flags >> 1, flags & 1);
            const auto poses = row["poses"].toArray();
            QCOMPARE(art.poses().size(), poses.size());
            for (int state = 0; state < poses.size(); ++state) {
                const auto expected = poses[state].toObject();
                const auto &actual = art.poses()[state];
                QCOMPARE(sha256(actual.source), expected["hash"].toString().toLatin1());
                const auto records = expected["records"].toArray();
                QCOMPARE(actual.records.size(), records.size());
                for (int i = 0; i < records.size(); ++i) {
                    const auto r = records[i].toArray();
                    QCOMPARE(actual.records[i].opcode, r[0].toInt());
                    QCOMPARE(actual.records[i].operands.size(), r.size() - 1);
                    for (int j = 1; j < r.size(); ++j)
                        QCOMPARE(u8(actual.records[i].operands, j - 1), r[j].toInt());
                }
                ++programs;
            }
            for (const auto &f : row["pictures"].toArray()) {
                const auto expected = f.toObject();
                const auto frame = art.frame(rom, expected["token"].toInt(), art.bodyVariant());
                const auto reference = expected["layers"].toArray();
                QCOMPARE(frame.size(), reference.size());
                for (int i = 0; i < frame.size(); ++i) {
                    const auto e = reference[i].toObject();
                    const auto &a = frame[i];
                    QCOMPARE(a.metadata.origin, QPoint(e["x"].toInt(), e["y"].toInt()));
                    QCOMPARE(a.metadata.width, e["width"].toInt());
                    QCOMPARE(a.metadata.height, e["height"].toInt());
                    QCOMPARE(a.metadata.flags, e["flags"].toInt());
                    QCOMPARE(a.metadata.scaleX / 1024., e["sx"].toDouble());
                    QCOMPARE(a.metadata.scaleY / 1024., e["sy"].toDouble());
                    QCOMPARE(sha256(a.pixels.image.rgba), e["hash"].toString().toLatin1());
                    ++layers;
                }
                ++frames;
            }
        }
        QCOMPARE(programs, data["programs"].toInt());
        QCOMPARE(frames, data["frames"].toInt());
        QCOMPARE(layers, data["layers"].toInt());
    }
};
QTEST_GUILESS_MAIN(BodyArtTests)
#include "body_art_tests.moc"
