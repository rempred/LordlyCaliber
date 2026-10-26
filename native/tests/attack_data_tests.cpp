#include "engine/attack_data.h"
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
