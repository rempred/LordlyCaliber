#include "engine/attack_data.h"
#include "engine/effect_player.h"
#include "engine/project.h"
#include "rebuild/attack_runtime.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class AttackDataTests : public QObject {
    Q_OBJECT
    Rom source() const {
        return Rom::open(qEnvironmentVariable("LORDLY_TEST_ROM"));
    }
  private slots:
    void visualExportAndPlayback() {
        try {
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty())
                QSKIP("Private compiler required");
            auto project = Project::fromRom(source());
            project.loadAttacks();
            auto data = *project.attacks();
            auto native = data.entry(55);
            native.visual.nativeEffect = 8;
            native.visual.delay = 3;
            data.set(55, native);
            QImage pixels(32, 32, QImage::Format_RGBA8888);
            pixels.fill(QColor(255, 0, 0, 255));
            auto asset = SpriteAsset::fromImage("Test effect", pixels);
            asset.anchor = {16, 16};
            asset.frames[0].ticks = 3;
            auto imported = data.entry(56);
            imported.visual.frames = asset;
            imported.visual.delay = 2;
            data.set(56, imported);
            for (const auto [id, effect] : {std::pair{57, 97}, std::pair{58, 123}}) {
                auto a = data.entry(id);
                a.visual.nativeEffect = effect;
                a.visual.delay = 1;
                data.set(id, a);
            }
            QImage tiled(67, 35, QImage::Format_RGBA8888);
            const QColor colors[]{Qt::red, Qt::green, Qt::blue, Qt::transparent};
            for (int y = 0; y < tiled.height(); ++y)
                for (int x = 0; x < tiled.width(); ++x)
                    tiled.setPixelColor(x, y, colors[(x / 7 + y / 5) % 4]);
            auto multiple = data.entry(59);
            multiple.visual.frames = SpriteAsset::fromImage("Tiled frames", tiled);
            multiple.visual.frames->anchor = {165, 10};
            multiple.visual.frames->frames[0].ticks = 2;
            auto second = multiple.visual.frames->frames[0];
            second.id = "second";
            second.layers[0].image = tiled.flipped(Qt::Horizontal);
            second.ticks = 3;
            multiple.visual.frames->frames.append(second);
            multiple.visual.frames->kind = "sequence";
            data.set(59, multiple);
            project.setAttacks(data.json());
            QTemporaryDir output(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM"))
                                     .absoluteDir()
                                     .filePath("Test ROMs/native-attack-visuals-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            project.saveFolder(output.path());
            auto restored = Project::fromRom(source());
            restored.applyManifest(Project::readProject(output.filePath("project.json")));
            QCOMPARE(restored.attacks()->entries(), data.entries());
            const auto built = rebuild(restored, tools, output.path());
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            exportRom(
                restored, built,
                output.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName()));
            const auto rom = Rom::fromBytes(built.image);
            const auto reopened = Project::fromRom(rom);
            QCOMPARE(reopened.attacks()->entry(55), native);
            QCOMPARE(reopened.attacks()->entry(56).visual.build(1), imported.visual.build(1));
            const auto p = AttackPlacement::read(rom);
            QVERIFY(p);
            for (int id : {55, 56, 57, 58, 59}) {
                const int slot =
                    u8(built.image, p->romOffset + AttackData::settingsOffset + id * 32 + 21);
                EffectPlayer actual(rom);
                actual.start(slot, {});
                int ticks = 0, triangles = 0;
                EffectPlayer expected(source());
                expected.startVisual(data.entry(id).visual,
                                     EffectPlayer::retailEffect(data.entry(id).animation), {});
                while (actual.advance()) {
                    QVERIFY(expected.advance());
                    const auto frame = actual.draw({320, 240}, .75);
                    triangles += frame.triangles;
                    ++ticks;
                    QCOMPARE(frame.image, expected.draw({320, 240}, .75).image);
                }
                QVERIFY(!expected.advance());
                QVERIFY(triangles > 0);
                if (id == 56)
                    QCOMPARE(ticks, 6);
                qInfo() << "Compiled effect for attack" << id << "ticks" << ticks << "triangles"
                        << triangles;
            }
            auto again = reopened;
            const auto secondExport = rebuild(again, tools, output.path());
            const auto secondOpen = Project::fromRom(Rom::fromBytes(secondExport.image));
            QCOMPARE(secondOpen.attacks()->entries(), reopened.attacks()->entries());
            for (int effect : {42, 68}) {
                EffectPlayer actual(rom);
                EffectPlayer::Scene scene;
                scene.targetCount = 1;
                actual.start(effect, scene);
                while (actual.advance())
                    actual.draw({160, 120}, .375);
                QVERIFY(actual.tick() > 20);
                qInfo() << "Sparse-target native effect" << effect << "completed" << actual.tick();
            }
            qInfo().noquote() << "Attack visual candidate:" << output.path();
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void initTestCase() {
        if (qEnvironmentVariableIsEmpty("LORDLY_TEST_ROM"))
            QSKIP("Private original ROM required");
    }
    void retailDataAndTemplateMaps() {
        const auto rom = source();
        const auto attacks = AttackData::loadRetail(rom);
        QCOMPARE(attacks.entries().size(), 159);
        QVERIFY(!attacks.modified());
        for (int id = 1; id < 159; ++id)
            QCOMPARE(attacks.entry(id).data, rom.bytes().mid(0x60980 + 16 * id, 12));
        QCOMPARE(AttackData::displayName(attacks.entry(4)), QString("Slash"));
        QVERIFY(attacks.entry(45).dynamicName);
        QVERIFY(attacks.sharingName(55).contains(56));
        for (int id = 45; id <= 48; ++id) {
            const auto targets = AttackData::templateTargets(id);
            for (int element = 1; element <= 6; ++element)
                QCOMPARE(targets.value(element),
                         int(u8(rom.bytes(), 0x6136c + 8 * (id - 45) + element)));
        }
        QCOMPARE(AttackData::elements().value(9), QString("Wind + Water"));
    }
    void sharedAndIndependentNames() {
        auto data = AttackData::loadRetail(source());
        const auto original = data.json();
        const auto users = data.sharingName(55);
        data.rename(55, "Shared lightning", true);
        for (int id : users)
            QCOMPARE(AttackData::displayName(data.entry(id)), QString("Shared lightning"));
        data.rename(55, "Solo lightning", false);
        QCOMPARE(data.sharingName(55), QVector<int>{55});
        QCOMPARE(AttackData::displayName(data.entry(56)), QString("Shared lightning"));
        data.apply(original);
        QVERIFY(!data.modified());
        const int copy = data.duplicate(55, "Independent spell");
        QCOMPARE(copy, 159);
        auto edited = data.entry(copy);
        put8(edited.data, 10, 0x9a);
        put8(edited.data, 11, 0xe7);
        edited.powerPercent = 175;
        edited.targetPattern = 12;
        edited.targetMask = 0x155;
        edited.status = 10;
        edited.chance = 35;
        data.set(copy, edited);
        QCOMPARE(data.entry(55).data, AttackData::loadRetail(source()).entry(55).data);
        auto restored = AttackData::loadRetail(source());
        restored.apply(data.json());
        QCOMPARE(restored.entries(), data.entries());
        QCOMPARE(u8(restored.entry(copy).data, 10), quint8(0x9a));
        QCOMPARE(restored.sharingName(copy), QVector<int>{copy});
        const int created = restored.create("New attack");
        QCOMPARE(created, 160);
        QCOMPARE(restored.entry(created).data, restored.entry(5).data);
        QCOMPARE(restored.entry(created).behavior, 5);
    }
    void validationAndCapacity() {
        auto data = AttackData::loadRetail(source());
        const auto before = data.json();
        auto malformed = before;
        auto rows = malformed["entries"].toArray();
        auto row = rows[0].toObject();
        row["chance"] = 101;
        rows[0] = row;
        malformed["entries"] = rows;
        QVERIFY_THROWS_EXCEPTION(Error, data.apply(malformed));
        QCOMPARE(data.json(), before);
        auto a = data.entry(1);
        a.targetPattern = 12;
        QVERIFY_THROWS_EXCEPTION(Error, data.set(1, a));
        QCOMPARE(data.json(), before);
        for (int id = 159; id <= 254; ++id)
            QCOMPARE(data.duplicate(4, QString("Attack %1").arg(id)), id);
        QCOMPARE(data.entries().size(), 255);
        const auto full = data.json();
        QVERIFY_THROWS_EXCEPTION(Error, data.create("Overflow"));
        QCOMPARE(data.json(), full);
        auto restored = AttackData::loadRetail(source());
        restored.apply(full);
        QCOMPARE(restored.entries(), data.entries());
    }
    void animationDonorsAndSpecialPoses() {
        auto project = Project::fromRom(source());
        project.loadAttacks();
        const auto original = project.bodyAnimationData();
        auto data = *project.attacks();
        const int ordinary = data.duplicate(5, "Independent Cleave");
        const int special = data.duplicate(17, "Independent Sonic Boom");
        project.setAttacks(data.json());
        auto overrides = *project.animationOverrides();
        QJsonArray rows{QJsonObject{
            {"classId", 2}, {"actionId", 5}, {"normalSelector", 41}, {"blockedSelector", 40}}};
        overrides.setEntries(rows, project.rom(), project.classData(), *project.animationRoutes());
        project.setAnimationOverrides(overrides.json());
        auto body = project.bodyAnimationData();
        QCOMPARE(body->selector(2, 2, 5, 0, 0), 41);
        QCOMPARE(body->selector(2, 2, ordinary, 0, 0), original->selector(2, 2, 5, 0, 0));
        QCOMPARE(body->attackSelector(22, 22, 17, 0, 0), 42);
        QCOMPARE(body->attackSelector(22, 22, special, 0, 0), 42);
        QCOMPARE(body->attackSelector(57, 57, 32, 0, 0), 43);
        QCOMPARE(body->attackSelector(57, 57, 32, 0, 1), 42);
        rows << QJsonObject{{"classId", 2},
                            {"actionId", ordinary},
                            {"normalSelector", 41},
                            {"blockedSelector", 40}};
        overrides.setEntries(rows, project.rom(), project.classData(), *project.animationRoutes());
        project.setAnimationOverrides(overrides.json());
        QCOMPARE(project.bodyAnimationData()->selector(2, 2, ordinary, 0, 0), 41);
    }
    void targetingExportAndReopen() {
        try {
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty()) QSKIP("Private compiler required");
            auto project = Project::fromRom(source());
            project.loadAttacks();
            auto attacks = *project.attacks();
            const int id = attacks.duplicate(124, "Liquification");
            QCOMPARE(id, 159);
            auto attack = attacks.entry(id);
            attack.targetPattern = 7;
            attack.status = 8;
            attack.chance = 25;
            attack.afterEffect = 0;
            auto dragon = project.record(Table::Classes, 65).bytes;
            put8(dragon, 55, id);
            put8(dragon, 56, 1);
            project.replaceBytes(Table::Classes, 65, dragon);
            QTemporaryDir output(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM"))
                                     .absoluteDir().filePath("Test ROMs/native-attack-targets-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            // Each side is retained through both project and compiled-ROM reopening.
            for (int side : {3, 2, 1}) {
                attack.targetSide = side;
                attacks.set(id, attack);
                project.setAttacks(attacks.json());
                const auto archive = output.filePath(QString("side-%1.lcproject").arg(side));
                project.saveArchive(archive);
                auto restored = Project::fromRom(source());
                restored.applyManifest(Project::readProject(archive));
                QCOMPARE(restored.attacks()->entry(id), attack);
                const auto built = rebuild(restored, tools, output.path());
                auto checksum = built.image;
                Rom::updateChecksum(checksum);
                QCOMPARE(checksum, built.image);
                const auto reopened = Project::fromRom(Rom::fromBytes(built.image));
                QCOMPARE(reopened.attacks()->entry(id), attack);
                QCOMPARE(u8(reopened.record(Table::Classes, 65).bytes, 55), quint8(159));
                QCOMPARE(u8(reopened.record(Table::Classes, 65).bytes, 56), quint8(1));
                exportRom(restored, built,
                          output.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName()));
            }
            qInfo().noquote() << "Attack targeting candidate:" << output.path();
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void projectAndRelocatedRecordExport() {
        try {
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty())
                QSKIP("Private compiler required");
            auto project = Project::fromRom(source());
            project.loadAttacks();
            const auto before = project.manifest();
            auto data = *project.attacks();
            data.rename(55, "Independent Lightning", false);
            const int copy = data.duplicate(4, "Copied Slash");
            auto a = data.entry(copy);
            put8(a.data, 1, 2);
            data.set(copy, a);
            for (int id = 160; id <= 254; ++id)
                QCOMPARE(data.duplicate(id % 2 ? 55 : 4, QString("Independent attack %1").arg(id)),
                         id);
            a = data.entry(254);
            a.targetPattern = 12;
            a.targetSide = 2;
            a.targetMask = 0x155;
            a.powerPercent = 175;
            a.status = 10;
            a.condition = 0;
            a.chance = 37;
            a.afterEffect = 2;
            a.afterBasis = 3;
            a.afterPercent = 12;
            put8(a.data, 10, 0x9a);
            put8(a.data, 11, 0xe7);
            data.set(254, a);
            project.setAttacks(data.json());
            const auto originalBody = project.bodyAnimationData();
            auto overrides = *project.animationOverrides();
            overrides.setEntries({QJsonObject{{"classId", 2},
                                              {"actionId", 254},
                                              {"normalSelector", 40},
                                              {"blockedSelector", 41}}},
                                 project.rom(), project.classData(), *project.animationRoutes());
            project.setAnimationOverrides(overrides.json());
            auto copies=*project.animationSequences();
            copies.create(2,0,*project.animationRoutes());
            project.setAnimationSequences(copies.json());
            const int privatePose = project.assignPrivateAttack(2, 0, 254, false, 40);
            QVERIFY(privatePose > 40);
            const auto overrideEntries = project.animationOverrides()->entries();
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 254, 0, 0), privatePose);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 254, 2, 0), 41);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 159, 0, 0),
                     originalBody->selector(2, 2, 4, 0, 0));
            const auto originalClass = project.record(Table::Classes, 2).bytes;
            auto classBytes = originalClass;
            put8(classBytes, 55, 254);
            put8(classBytes, 56, 3);
            put8(classBytes, 57, 159);
            put8(classBytes, 58, 2);
            project.replaceBytes(Table::Classes, 2, classBytes);
            QCOMPARE(project.catalog()["ACTION_NAMES"].toObject()[QString::number(copy)].toString(),
                     QString("Copied Slash"));
            QVERIFY(project.changedRecords() > 0);
            const auto after = project.manifest();
            project.applyManifest(before);
            QCOMPARE(project.attacks()->json(), AttackData::loadRetail(source()).json());
            project.applyManifest(after);
            QCOMPARE(project.attacks()->json(), data.json());
            QTemporaryDir output(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM"))
                                     .dir()
                                     .filePath("Test ROMs/native-attacks-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto folder = output.filePath("project"),
                       archive = output.filePath("attacks.lcproj");
            project.saveFolder(folder);
            project.saveArchive(archive);
            for (const auto &path : {folder, archive}) {
                auto restored = Project::fromRom(source());
                restored.applyManifest(Project::readProject(path));
                QCOMPARE(restored.attacks()->json(), data.json());
            }
            for (int pass = 0; pass < 2; ++pass) {
                const auto built = rebuild(project, tools, output.path());
                const auto placed = AttackPlacement::read(Rom::fromBytes(built.image));
                QVERIFY(placed);
                QCOMPARE(placed->count, 255);
                if (!pass) {
                    auto unfamiliar = removeAttackReferences(Rom::fromBytes(built.image));
                    unfamiliar[0x22d16c] = char(quint8(unfamiliar[0x22d16c]) ^ 1);
                    QVERIFY_THROWS_EXCEPTION(
                        Error, installAttackReferences(Rom::fromBytes(unfamiliar), *placed));
                }
                const auto namePointer =
                    u32(built.image, placed->romOffset + AttackData::recordOffset + copy * 16);
                QVERIFY(namePointer >= placed->address &&
                        namePointer < placed->address + placed->size);
                QCOMPARE(
                    u8(built.image, placed->romOffset + AttackData::recordOffset + copy * 16 + 5),
                    quint8(2));
                auto checksum = built.image;
                Rom::updateChecksum(checksum);
                QCOMPARE(checksum, built.image);
                exportRom(
                    project, built,
                    output.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName()));
                project = Project::fromRom(Rom::fromBytes(built.image));
                QVERIFY(project.attacks());
                QCOMPARE(project.attacks()->json(), data.json());
                QCOMPARE(project.record(Table::Classes, 2).bytes.sliced(4),
                         classBytes.sliced(4)); // Name pointer relocates.
                QVERIFY(!project.attacks()->modified());
                const auto body = project.bodyAnimationData();
                QCOMPARE(project.animationOverrides()->entries(), overrideEntries);
                QCOMPARE(body->selector(2, 2, 254, 0, 0), privatePose);
                QVERIFY(body->descriptor(2, 2, 0, 0));
                QCOMPARE(body->selector(2, 2, 254, 2, 0), 41);
                QCOMPARE(body->selector(2, 2, 159, 0, 0), originalBody->selector(2, 2, 4, 0, 0));
            }
            qInfo().noquote() << "Attack data relocation candidate:" << output.path();
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(AttackDataTests)
#include "attack_data_tests.moc"
