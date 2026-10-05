#include "engine/actor_art.h"
#include "engine/actor_pose.h"
#include "engine/class_data.h"
#include "engine/scene_actors.h"
#include "engine/scene_actor_renderer.h"
#include "game/images/sepia.h"
#include "game/director/actor_body.h"
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
    QJsonObject fixture(const QString &name, const QString &path, const QStringList &extra = {}) {
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node", QStringList{QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath(name),
                                path, work.filePath("reference.json")} +
                        extra);
        if (!reference.waitForFinished(60000) || reference.exitCode() != 0)
            throw Error(QString::fromUtf8(reference.readAllStandardError()));
        QFile file(work.filePath("reference.json"));
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing body-art fixture");
        return QJsonDocument::fromJson(file.readAll()).object();
    }
  private slots:
    void bodyImagesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto data = fixture("body-images-mips.cjs", path);
        const auto rows = data["rows"].toArray();
        QCOMPARE(rows.size(), 23094);
        const auto rom = Rom::open(path);
        QMap<int, ActorArt> arts;
        int checked = 0;
        for (const auto &value : rows) {
            const auto row = value.toObject();
            const int source = row["source"].toInt(), flags = row["flags"].toInt();
            const int key = source * 4 + flags;
            if (!arts.contains(key))
                arts.insert(key, ActorArt::loadBody(rom, source, source, flags >> 1, flags & 1));
            const auto &art = arts[key];
            const BodyAppearance appearance{quint16(row["item"].toInt()), quint8(3)};
            const auto selections = art.bodyLayers(rom, row["token"].toInt(), appearance);
            const auto frames = art.bodyFrame(rom, row["token"].toInt(), appearance, row["sepia"].toInt());
            const auto expected = row["layers"].toArray();
            QCOMPARE(selections.size(), expected.size());
            QCOMPARE(frames.size(), expected.size());
            for (int i = 0; i < expected.size(); ++i) {
                const auto e = expected[i].toObject();
                const auto record = QByteArray::fromHex(e["record"].toString().toLatin1());
                const auto &s = selections[i];
                QCOMPARE(s.metadata.art, int(u32(record, 0)));
                QCOMPARE(s.metadata.origin, QPoint(qint32(u32(record, 4)), qint32(u32(record, 8))));
                QCOMPARE(s.metadata.width, int(u32(record, 12)));
                QCOMPARE(s.metadata.height, int(u32(record, 16)));
                QCOMPARE(s.metadata.flags, u32(record, 20));
                QCOMPARE(float(s.metadata.scaleX) / 1024.f, std::bit_cast<float>(u32(record, 24)));
                QCOMPARE(float(s.metadata.scaleY) / 1024.f, std::bit_cast<float>(u32(record, 28)));
                QCOMPARE(s.paletteOffset, e["paletteOffset"].toInt());
                QCOMPARE(art.artKeys()[s.metadata.art], quint32(e["key"].toInteger()));
                QCOMPARE(frames[i].pixels.child, e["child"].toInt());
                QCOMPARE(e["flags"].toInt(), 2 | row["sepia"].toInt() | (s.metadata.flags & 0xc0 ? 4 : 0));
                QVERIFY(e["sixteen"].toBool());
                ++checked;
            }
        }
        const auto warm = QByteArray::fromHex(data["warm16"].toString().toLatin1());
        QCOMPARE(warm.size(), 131072);
        for (int value = 0; value < 65536; ++value)
            QCOMPARE(lordly_warm_rgba16(quint16(value)), u16(warm, value * 2));
        qInfo() << checked << "original body image layers and all RGBA16 warm-tone inputs agree";
    }
    void equipmentRenderingAndRollback() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        SceneActors actors(rom, 2, 255, [](int, float, float) { return 0.f; });
        actors.supplyBodyRows(QVector<QByteArray>(28, QByteArray(248, 0)));
        int value = 0;
        QVERIFY(actors.service(0x14, {0,30,0,0,0,0,0,0,0}, value));
        QVERIFY(actors.service(0x2a, {0,2,0,0,0,2}, value));
        const auto camera = SceneCameras::freshModeTwo(rom).actor();
        actors.prepareModeTwoMatrices(camera, {{0,0,0},{{200,-200,50},{-200,200,75}}});
        const auto queue = SceneDrawQueue::build(actors.actorRecords(), {}, camera,
                                                {0x80300000,0x80310000});
        SceneActorRenderer renderer(rom);
        const auto draw = [&] { return renderer.world(actors, queue, {}, {64,64}, {0,0,0,0,0,0}, {camera,{255,255,255},{}}); };
        QVector<QByteArray> rows(28,QByteArray(248,0));
        const int identity = u8(actors.actor(0)->record(),0x147);
        put16(rows[identity],0x36,1); actors.supplyBodyRows(rows);
        QCOMPARE(actors.bodyEquipment(0),quint16(1));
        const auto first = draw();
        bool changed = false;
        for (int item : {2,6,11,20,50,100}) {
            put16(rows[identity],0x36,quint16(item)); actors.supplyBodyRows(rows);
            const auto other = draw();
            if (first.image != other.image) { changed = true; break; }
        }
        QVERIFY(changed);
        put16(rows[identity],0x36,1); actors.supplyBodyRows(rows);
        QCOMPARE(draw().image,first.image);
        const auto records = actors.actorRecords();
        put16(rows[identity],0x36,278); actors.supplyBodyRows(rows);
        QVERIFY_THROWS_EXCEPTION(Error,draw());
        QCOMPARE(actors.actorRecords(),records);
    }
    void bodyInitializationAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto cases = fixture("actor-body-mips.cjs", path)["results"].toArray();
        QCOMPARE(cases.size(), 675);
        const auto rom = Rom::open(path);
        for (const auto &value : cases) {
            const auto row = value.toObject();
            SceneActors scene(rom, 2, 0x3c);
            QVector<QByteArray> caller;
            for (const auto &v : row["rows"].toArray())
                caller << QByteArray::fromHex(v.toString().toLatin1());
            scene.supplyBodyRows(caller);
            for (const auto &v : row["initial"].toArray()) {
                const auto a = v.toObject();
                scene.supplyActor(a["slot"].toInt(),
                                  QByteArray::fromHex(a["record"].toString().toLatin1()));
            }
            const auto a = row["args"].toArray();
            // Compare the C record transformation at the original pre-pose
            // boundary too. Appearance identity assignment belongs to the
            // host scene; the complete post-pose comparison below includes it.
            for (const auto &v : row["before"].toArray()) {
                const auto expected = v.toObject();
                const int slot = expected["slot"].toInt();
                const auto initial = scene.actor(slot)->record();
                LordlyActorRecord raw;
                std::memcpy(raw.bytes, initial.constData(), sizeof(raw.bytes));
                const int id = u8(initial, 0x147);
                const int kind =
                    id == 255 ? 0
                              : lordly_body_classification(
                                    reinterpret_cast<const uint8_t *>(caller[id].constData()),
                                    u8(initial, 0x149));
                const LordlyActorBodySelection selection{
                    slot,         a[1].toInt(), a[2].toInt(),         a[3].toInt(),
                    a[4].toInt(), a[5].toInt(), quint8(a[6].toInt()), quint8(a[7].toInt()),
                    0x3c};
                int advance = 0;
                QCOMPARE(lordly_actor_body_select(&raw, &selection, kind, &advance),
                         LORDLY_ACTOR_OK);
                QCOMPARE(advance, 1);
                auto bytes = QByteArray::fromHex(expected["record"].toString().toLatin1());
                bytes[0x147] = initial[0x147];
                bytes[0x148] = initial[0x148];
                QCOMPARE(QByteArray(reinterpret_cast<const char *>(raw.bytes), sizeof(raw.bytes)),
                         bytes);
            }
            try {
                scene.selectBody({a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt(),
                                  a[4].toInt(), a[5].toInt(), quint8(a[6].toInt()),
                                  quint8(a[7].toInt()), 0x3c});
            } catch (const Error &e) {
                QFAIL(qPrintable(row["name"].toString() + ": " + QString::fromUtf8(e.what())));
            }
            for (const auto &v : row["after"].toArray()) {
                const auto expected = v.toObject();
                const auto actual = scene.actor(expected["slot"].toInt())->record().toHex();
                QVERIFY2(actual == expected["record"].toString().toLatin1(),
                         qPrintable(row["name"].toString()));
            }
            QJsonArray actualRows, registrations;
            for (int i = 0; i < 28; ++i)
                actualRows << QString::fromLatin1(scene.bodyRow(i).toHex());
            for (const auto &r : scene.bodyRegistrations())
                registrations << QJsonArray{r.source, r.owner, r.flagA, r.flagB, r.equipment};
            QCOMPARE(actualRows, row["afterRows"].toArray());
            QCOMPARE(registrations, row["registrations"].toArray());
        }
    }
    void bodyCommandContextAndAtomicity() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        SceneActors scene(rom, 2, 7);
        int value = 123;
        QVERIFY(scene.service(0x14, {6, 3, 0, 0, 120000, 0, 25000, 2, 0}, value));
        const auto before = scene.actor(6)->record();
        scene.supplyMotion(6, {1, 0, 2, 12, 0, 0, 1});
        QVERIFY_THROWS_EXCEPTION(Error, scene.service(0x2a, {6, 32, 10, 1, 0, 32}, value));
        QCOMPARE(scene.actor(6)->record(), before);
        QCOMPARE(scene.motion(6).remaining, 12);
        scene.supplyBodyRows(QVector<QByteArray>(28, QByteArray(248, 0)));
        QVERIFY(scene.service(0x2a, {6, 32, 10, 1, 0, 32}, value));
        const auto changed = scene.actor(6)->record();
        QCOMPARE(u8(changed, 0x13d), 1);
        QCOMPARE(u32(changed, 0xe8), 32U);
        QCOMPARE(u32(changed, 0xec), 32U);
        QCOMPARE(u8(changed, 0x147), 0);
        QCOMPARE(u8(changed, 0x148), 1);
        QCOMPARE(u8(changed, 0x13c), 7);
        QCOMPARE(u8(changed, 0x141), 2);
        QCOMPARE(u32(changed, 0x11c), u32(before, 0x11c));
        QCOMPARE(scene.motion(6).remaining, 12);
        QCOMPARE(scene.actor(6)->pose().art().sourceClass(), 32);
        const auto row = scene.bodyRow(0);
        QVERIFY_THROWS_EXCEPTION(Error, scene.service(0x2a, {6, 0, 10, 1, 0, 0}, value));
        QCOMPARE(scene.actor(6)->record(), changed);
        QCOMPARE(scene.bodyRow(0), row);
        scene.advancePoses();
        scene.advancePlanar();
        QCOMPARE(scene.actor(6)->pose().actor().position.x, 121.f);
        QCOMPARE(scene.motion(6).remaining, 11);
        SceneActors otherMode(rom, 0, 0);
        QVERIFY(otherMode.service(0x2a, {999, 999, 999, 999, 999, 999}, value));
        QCOMPARE(value, 123);
    }
    void alternatePosesAgainstMips() {
        compareAlternate(false);
    }
    void linkedPoseRequestsAgainstMips() {
        compareAlternate(true);
    }

  private:
    void compareAlternate(bool linked) {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto data =
            fixture("body-pose-mips.cjs", path, linked ? QStringList{"linked"} : QStringList{});
        const auto rows = data["rows"].toArray();
        QCOMPARE(rows.size(), 17633);
        const auto rom = Rom::open(path);
        QMap<int, ActorArt> banks;
        int calls = 0, linkedRequests = 0;
        for (const auto &value : rows) {
            const auto row = value.toObject();
            const int source = row["source"].toInt(), flags = row["flags"].toInt(),
                      id = source * 4 + flags;
            if (!banks.contains(id))
                banks.insert(id, ActorArt::loadBody(rom, source, source, flags >> 1, flags & 1));
            ActorPosePlayer player(rom, banks[id], row["state"].toInt());
            if (linked)
                player.setLinkedAudioContext({0x80350000, source, source});
            else
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
                QJsonArray requests;
                for (const auto &r : player.linkedAudioRequests()) {
                    requests << QJsonArray{qint64(r.linked.objectAddress), r.linked.source,
                                           r.linked.owner, r.key, r.opcode};
                    ++linkedRequests;
                }
                QCOMPARE(requests, expected[11].toArray());
                ++calls;
            }
        }
        QCOMPARE(calls, 141064);
        if (linked)
            QVERIFY(linkedRequests > 0);
        else
            QCOMPARE(linkedRequests, 0);
    }
  private slots:
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
