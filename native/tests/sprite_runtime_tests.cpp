#include "engine/sprite_pose.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class SpriteRuntimeTests : public QObject {
    Q_OBJECT
  private slots:
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
