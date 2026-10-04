#include "engine/scene_sprites.h"
#include "engine/sprite_pose.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <bit>
using namespace lordly;
class SpriteRuntimeTests : public QObject {
    Q_OBJECT
  private slots:
    void drawPlanAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("sprite-draw-mips.cjs"),
             path, work.filePath("draw.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("draw.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        SceneSprites sprites(Rom::open(path));
        for (const auto &r : data["records"].toArray()) {
            const auto row = r.toObject();
            sprites.supplyRecord(row["slot"].toInt(),
                                 QByteArray::fromHex(row["record"].toString().toLatin1()));
        }
        const auto cases = data["cases"].toArray();
        QCOMPARE(cases.size(), 44);
        for (const auto &r : cases) {
            const auto row = r.toObject();
            const auto expected = row["draws"].toArray();
            const auto draws = sprites.draws(row["pass"].toInt(), quint8(row["special"].toInt()));
            QCOMPARE(draws.size(), expected.size());
            for (int i = 0; i < draws.size(); ++i) {
                const auto &draw = draws[i];
                const auto e = expected[i].toObject();
                QCOMPARE(draw.slot, e["slot"].toInt());
                QCOMPARE(draw.pass, e["pass"].toInt());
                float rotation = 0, scale = 1;
                for (const auto &t : e["transforms"].toArray()) {
                    const auto transform = t.toObject();
                    const auto bits = transform["bits"].toArray();
                    if (transform["kind"] == "translation") {
                        QCOMPARE(std::bit_cast<quint32>(draw.x), quint32(bits[0].toInteger()));
                        QCOMPARE(std::bit_cast<quint32>(draw.y), quint32(bits[1].toInteger()));
                        QCOMPARE(bits[2].toInteger(), 0);
                    } else if (transform["kind"] == "rotation")
                        rotation = std::bit_cast<float>(quint32(bits[0].toInteger()));
                    else {
                        QCOMPARE(bits[0], bits[1]);
                        QCOMPARE(bits[0], bits[2]);
                        scale = std::bit_cast<float>(quint32(bits[0].toInteger()));
                    }
                }
                QCOMPARE(draw.rotation, rotation);
                QCOMPARE(draw.scale, scale);
            }
        }
        auto record = sprites.sprite(0)->record();
        put8(record, 0, 153);
        sprites.supplyRecord(0, record);
        const auto colors = data["colors"].toArray();
        QCOMPARE(colors.size(), 120);
        for (const auto &r : colors) {
            const auto row = r.toObject();
            const auto rgb = row["global"].toArray();
            QByteArray transition;
            if (!row["current"].isNull()) {
                transition = QByteArray(80, 0);
                put8(transition, 4, quint8(row["current"].toInt()));
                put8(transition, 5, 17);
                put8(transition, 6, 39);
            }
            const auto color = sprites.primitiveColor(
                0, 0, quint8(row["alpha"].toInt()),
                {qint16(rgb[0].toInt()), qint16(rgb[1].toInt()), qint16(rgb[2].toInt())},
                transition);
            const quint32 packed = (quint32(color[0]) << 24) | (quint32(color[1]) << 16) |
                                   (quint32(color[2]) << 8) | color[3];
            QCOMPARE(packed, quint32(row["rgba"].toInteger()));
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
                                     .filePath("scene-sprites-mips.cjs"),
                                 path, work.filePath("sprites.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("sprites.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QVERIFY(rows.size() >= 85);
        const auto rom = Rom::open(path);
        for (const auto &v : rows) {
            const auto row = v.toObject();
            SceneSprites sprites(rom), resumed(rom);
            for (const auto &r : row["initial"].toArray()) {
                const auto item = r.toObject();
                const auto record = QByteArray::fromHex(item["record"].toString().toLatin1());
                sprites.supplyRecord(item["slot"].toInt(), record);
                resumed.supplyRecord(item["slot"].toInt(), record);
            }
            const auto operations = row["operations"].toArray(), states = row["states"].toArray();
            for (int step = 0; step < operations.size(); ++step) {
                const auto op = operations[step].toObject(), expected = states[step].toObject();
                if (op.contains("advance")) {
                    QVERIFY(sprites.advancePoses());
                    int attempts = 0;
                    while (!resumed.advancePoses(1))
                        QVERIFY(++attempts < 100);
                } else {
                    QVector<quint32> args;
                    for (const auto &a : op["args"].toArray())
                        args << quint32(a.toInteger());
                    const bool query = op["opcode"].toInt() == 0x65;
                    if (query)
                        args = {0, 0, args[0]};
                    int value = -100, other = -100;
                    QVERIFY(sprites.service(quint32(op["opcode"].toInt()), args, value));
                    QVERIFY(resumed.service(quint32(op["opcode"].toInt()), args, other));
                    QCOMPARE(value, query ? expected["value"].toInt() : -100);
                    QCOMPARE(other, value);
                }
                QJsonArray records;
                for (int slot = 0; slot < 30; ++slot) {
                    const auto sprite = sprites.sprite(slot), other = resumed.sprite(slot);
                    QCOMPARE(sprite != nullptr, other != nullptr);
                    if (!sprite)
                        continue;
                    QCOMPARE(sprite->record(), other->record());
                    records << QJsonObject{
                        {"slot", slot}, {"record", QString::fromLatin1(sprite->record().toHex())}};
                }
                QCOMPARE(records, expected["records"].toArray());
            }
        }
    }
    void lifecycleBoundsAndMissingContext() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        SceneSprites sprites(Rom::open(path));
        int value = 0;
        QVERIFY(sprites.service(0x46, {0, 57, 0, 0, 12, 13, 1, 100, 0}, value));
        const auto initial = sprites.sprite(0)->record();
        QVERIFY_THROWS_EXCEPTION(
            Error,
            sprites.service(0x46, {0, 57, 0, 0, 0xffffffffU, 0xffffffffU, 1, 100, 0}, value));
        QCOMPARE(sprites.sprite(0)->record(), initial);
        QVERIFY_THROWS_EXCEPTION(Error, sprites.service(0x65, {0, 0, 256}, value));
        QVERIFY_THROWS_EXCEPTION(Error, sprites.service(0x64, {0, 0xffffffffU, 65535, 0}, value));
        QCOMPARE(sprites.sprite(0)->record(), initial);
        QVERIFY_THROWS_EXCEPTION(
            Error,
            sprites.service(0x63, {0, 63, 0, 0, 12, 13, 0, 0xffffffffU, 1, 0xffffffffU}, value));
        QCOMPARE(sprites.sprite(0)->record(), initial);
        sprites.setRotationSource([](qint32 group, qint32 entry) {
            if (group != -1 || entry != 1)
                throw Error("Unexpected path");
            return qint16(270);
        });
        QVERIFY(
            sprites.service(0x63, {0, 63, 0, 0, 12, 13, 0, 0xffffffffU, 1, 0xffffffffU}, value));
        QCOMPARE(u16(sprites.sprite(0)->record(), 0x40), 270);
        QCOMPARE(sprites.sprite(0)->sprite().bank, 63);
    }
    void posesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("sprite-pose-mips.cjs"),
             path, work.filePath("poses.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("poses.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto fixture = QJsonDocument::fromJson(file.readAll()).object();
        const auto rows = fixture["rows"].toArray(), custom = fixture["custom"].toArray();
        QCOMPARE(rows.size(), 2275);
        QCOMPARE(custom.size(), 110);
        const auto rom = Rom::open(path);
        QMap<int, ActorArt> banks;
        int calls = 0, pauses = 0;
        auto compare = [&](const QJsonObject &row, const Rom &input, const ActorArt &art) {
            const auto initial = QByteArray::fromHex(row["initial"].toString().toLatin1());
            // Imported unknown record fields must survive every operation.
            SpritePosePlayer player(input, art, initial), resumed(input, art, initial);
            for (const auto &v : row["calls"].toArray()) {
                const auto expected = v.toObject();
                QVERIFY2(player.advance() == LORDLY_POSE_OK, qPrintable(player.error()));
                int status, attempts = 0;
                do {
                    status = resumed.advance(1);
                    if (status == LORDLY_POSE_BUDGET)
                        ++pauses;
                    QVERIFY(++attempts < 100);
                } while (status == LORDLY_POSE_BUDGET);
                QVERIFY2(status == LORDLY_POSE_OK, qPrintable(resumed.error()));
                const auto record = expected["record"].toString().toLatin1();
                QCOMPARE(player.record().toHex(), record);
                QCOMPARE(resumed.record().toHex(), record);
                QCOMPARE(player.result(), expected["result"].toInt());
                QCOMPARE(resumed.result(), player.result());
                QJsonArray audio, resumedAudio;
                for (const auto &r : player.audioRequests())
                    audio << QJsonArray{r.context, r.value};
                for (const auto &r : resumed.audioRequests())
                    resumedAudio << QJsonArray{r.context, r.value};
                QCOMPARE(audio, expected["audio"].toArray());
                QCOMPARE(resumedAudio, audio);
                ++calls;
            }
        };
        for (const auto &v : rows) {
            const auto row = v.toObject();
            const int bank = row["bank"].toInt();
            if (!banks.contains(bank))
                banks.insert(bank, ActorArt::load(rom, bank));
            compare(row, rom, banks[bank]);
        }
        for (const auto &v : custom) {
            const auto row = v.toObject();
            const auto program = QByteArray::fromHex(row["program"].toString().toLatin1());
            const auto packed = encodeLzss(program);
            auto bytes = rom.bytes();
            const quint32 offset = quint32(bytes.size());
            QByteArray header(8, '\0');
            put32(header, 0, quint32(packed.size() + 4));
            put32(header, 4, quint32(program.size()));
            bytes += header;
            bytes += packed;
            while (bytes.size() % 4)
                bytes.append('\0');
            put32(bytes, banks[57].descriptorKey() + 0x594284 + 4, offset - 0x594280);
            const auto input = Rom::fromBytes(bytes);
            compare(row, input, ActorArt::load(input, 57));
        }
        QCOMPARE(calls, 55040);
        QVERIFY(pauses > 0);
    }
    void suspensionAndBounds() {
        struct Input {
            QVector<LordlyPoseRecord> commands;
            bool available = false;
            int sounds = 0;
        };
        Input input{{{2, {128, 127}},
                     {17, {0, 7}},
                     {13, {255, 200}},
                     {16, {255, 128}},
                     {12, {1, 2, 3}},
                     {21, {0x34, 0x12, 3}}}};
        const auto lookup = [](void *opaque, const LordlySpritePose *sprite,
                               LordlyPoseRecord *out) -> int {
            const auto &commands = static_cast<Input *>(opaque)->commands;
            *out = sprite->cursor < 0 || sprite->cursor >= commands.size()
                       ? LordlyPoseRecord{}
                       : commands[sprite->cursor];
            return 1;
        };
        const auto sound = [](void *opaque, const LordlySpritePose *,
                              const LordlyPoseRecord *) -> int {
            auto &i = *static_cast<Input *>(opaque);
            if (!i.available)
                return 0;
            ++i.sounds;
            return 1;
        };
        LordlySpritePose sprite{};
        sprite.cursor = -1;
        LordlyPoseInvocation invocation{};
        QCOMPARE(lordly_sprite_advance_pose(&sprite, &invocation, 1, lookup, sound, &input),
                 LORDLY_POSE_BUDGET);
        QCOMPARE(sprite.x, 128);
        QCOMPARE(sprite.y, -127);
        QCOMPARE(lordly_sprite_advance_pose(&sprite, &invocation, 10, lookup, sound, &input),
                 LORDLY_POSE_SHARED);
        QCOMPARE(sprite.cursor, 0);
        QCOMPARE(input.sounds, 0);
        input.available = true;
        QCOMPARE(lordly_sprite_advance_pose(&sprite, &invocation, 10, lookup, sound, &input),
                 LORDLY_POSE_OK);
        QCOMPARE(input.sounds, 1);
        QCOMPARE(sprite.frame, 0x1234);
        QCOMPARE(sprite.delay, 1);
        QCOMPARE(sprite.x, 128);
        QCOMPARE(sprite.y, -127);
        for (auto value : sprite.color)
            QCOMPARE(value, 200);
        QCOMPARE(lordly_sprite_advance_pose(&sprite, &invocation, 0, lookup, sound, &input),
                 LORDLY_POSE_OK);
        QCOMPARE(sprite.delay, -1);
        for (auto value : sprite.color)
            QCOMPARE(value, 200);
        input.commands = {{4, {0}}};
        sprite.cursor = -1;
        sprite.delay = 0;
        QCOMPARE(lordly_sprite_advance_pose(&sprite, &invocation, 100, lookup, sound, &input),
                 LORDLY_POSE_BUDGET);
        input.commands = {{13, {16, 77}}};
        QCOMPARE(lordly_sprite_advance_pose(&sprite, &invocation, 1, lookup, sound, &input),
                 LORDLY_POSE_INVALID);
        QCOMPARE(sprite.cursor, -1);
        QCOMPARE(sprite.material[0], 128);
    }
};
QTEST_GUILESS_MAIN(SpriteRuntimeTests)
#include "sprite_runtime_tests.moc"
