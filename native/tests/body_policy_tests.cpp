#include "engine/actor_pose.h"
#include "engine/body_visual_policy.h"
#include "engine/pose_runtime.h"
#include "engine/project.h"
#include "engine/scene_actors.h"
#include "rebuild/compiler.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <bit>
using namespace lordly;
namespace {
QByteArray bytes(const QJsonValue &value) {
    return QByteArray::fromHex(value.toString().toLatin1());
}
QJsonValue normalize(const QJsonValue &value, const QMap<int, int> &classes,
                     const QString &field = {}) {
    if (value.isObject()) {
        auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            it.value() = normalize(it.value(), classes, it.key());
        return object;
    }
    if (value.isArray()) {
        auto array = value.toArray();
        for (int i = 0; i < array.size(); ++i)
            array[i] = normalize(array[i], classes, field);
        return array;
    }
    if (value.isString()) {
        auto record = bytes(value);
        int offset = -1;
        if ((field == "rows" || field == "row") && record.size() == 248)
            offset = 0x48;
        if ((field == "record" || field == "actor" || field == "initial") && record.size() == 336)
            offset = 0xe8;
        if (offset >= 0) {
            for (int at : {offset, offset + 4}) {
                const auto id = u32(record, at);
                if (classes.contains(id))
                    put32(record, at, classes[id]);
            }
            return QString::fromLatin1(record.toHex());
        }
    }
    return value;
}
QJsonObject machine(const QString &script, const QString &romPath, const QString &directory,
                    const QString &name, QJsonObject input = {}) {
    const auto rom = Rom::open(romPath);
    input["key"] = double(AnimationRoutes::resourceKey(rom));
    input["directory"] = double(DirectDescriptors::resourceKey(rom));
    if (const auto placement = ClassPlacement::read(rom)) {
        input["classAddress"] = double(placement->layout.indexBase);
        input["classOffset"] = double(placement->dataRomOffset);
        input["classSize"] = double(placement->layout.end - placement->layout.indexBase);
    }
    QJsonArray owners;
    for (const auto &entry : ClassData::load(rom).entries())
        owners << u8(entry.record, 69);
    input["owners"] = owners;
    const auto config = directory + "/" + name + "-input.json",
               output = directory + "/" + name + ".json";
    atomicWrite(config, QJsonDocument(input).toJson());
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start("node", {QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath(script), romPath,
                           output, config});
    if (!process.waitForFinished(60000) || process.exitCode())
        throw Error(QString::fromLocal8Bit(process.readAll()));
    qInfo().noquote() << process.readAll();
    QFile file(output);
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing constructor fixture");
    return QJsonDocument::fromJson(file.readAll()).object();
}
QJsonArray actors(const SceneActors &scene) {
    QJsonArray result;
    for (int slot = 0; slot < 28; ++slot)
        if (scene.actor(slot))
            result << QJsonObject{
                {"slot", slot},
                {"record", QString::fromLatin1(scene.actor(slot)->record().toHex())}};
    return result;
}
QJsonArray motions(const SceneActors &scene) {
    QJsonArray result;
    for (int slot = 0; slot < 28; ++slot)
        if (scene.motion(slot).present) {
            const auto &m = scene.motion(slot);
            QByteArray record(16, 0);
            put32(record, 0, std::bit_cast<quint32>(m.vx));
            put32(record, 4, std::bit_cast<quint32>(m.vy));
            put32(record, 8, std::bit_cast<quint32>(m.vz));
            put16(record, 12, m.remaining);
            put8(record, 14, m.pause);
            put8(record, 15, m.retained);
            result << QJsonObject{{"slot", slot}, {"record", QString::fromLatin1(record.toHex())}};
        }
    return result;
}
void supply(SceneActors &scene, const QJsonObject &c) {
    QVector<QByteArray> rows;
    for (const auto &row : c["rows"].toArray())
        rows << bytes(row);
    scene.supplyBodyRows(rows);
    for (const auto &value : c["objects"].toArray()) {
        const auto object = value.toObject();
        scene.supplyBodyObject(quint32(object["address"].toInteger()), bytes(object["record"]));
    }
    for (const auto &value : c["initial"].toArray()) {
        const auto actor = value.toObject();
        scene.supplyActor(actor["slot"].toInt(), bytes(actor["record"]));
    }
}
void replay(Project &project, const QJsonObject &rosters, const QJsonObject &presentation) {
    const auto data = project.bodyAnimationData();
    for (const auto &value : rosters["results"].toArray()) {
        const auto c = value.toObject();
        SceneActors scene(project.rom(), 2, 60, [](int, float, float) { return 123.75f; }, data);
        supply(scene, c);
        PresentationRoster roster;
        roster.sideGroups = {0, 30};
        for (int bank = 0; bank < 2; ++bank)
            for (int member = 0; member < 100; ++member)
                roster.members[bank].insert(member, {quint16(member == 9 ? 0 : 1), 0});
        scene.supplyRosterLaunch(
            {qint8(c["context"].toInt()), qint16(c["scenario"].toInt()), roster});
        int unused = -100;
        QVERIFY(scene.service(c["variant"].toInt() ? 0xab : 0x45,
                              {quint32(c["first"].toInt()), quint32(c["second"].toInt())}, unused));
        QCOMPARE(actors(scene), c["actors"].toArray());
        QJsonArray draws;
        for (int slot : scene.immediateRosterDraws())
            draws << slot;
        QCOMPARE(draws, c["draws"].toArray());
    }
    int index = 0;
    for (const auto &value : presentation["entrances"].toArray()) {
        const auto c = value.toObject();
        SceneActors scene(project.rom(), 2, 60, [](int, float, float) { return 7.75f; }, data);
        scene.supplySceneFlags(8);
        supply(scene, c);
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
        scene.supplyPresentationLaunch({c["preset"].toInt(), qint8(c["variant"].toInt()), roster});
        scene.startEntrance();
        const auto states = c["states"].toArray();
        for (int tick = 0; tick < states.size(); ++tick) {
            if (tick) {
                scene.advanceMovement();
                QVERIFY(scene.advancePoses());
                scene.advancePresentation();
            }
            const auto expected = states[tick].toObject();
            const auto context = QString("entrance %1 tick %2").arg(index).arg(tick);
            QVERIFY2(actors(scene) == expected["actors"].toArray(),
                     qPrintable(context + " actors"));
            QVERIFY2(motions(scene) == expected["motions"].toArray(),
                     qPrintable(context + " motion"));
            QCOMPARE(scene.presentationRecord(), bytes(expected["presentation"]));
            QCOMPARE(scene.presentationOpacity(),
                     std::optional{quint8(expected["opacity"].toInt())});
        }
        ++index;
    }
}
} // namespace
class BodyPolicyTests : public QObject {
    Q_OBJECT
  private slots:
    void constructionInheritance() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            QJsonObject mapping;
            QMap<int, int> inverse;
            for (int source : {135, 136, 161}) {
                const int id = project.appendClass(source, QString("Body %1").arg(source));
                const auto route = project.animationRoutes()->entry(id);
                QVERIFY(route == project.animationRoutes()->entry(source));
                QVERIFY(route.fixedPose);
                QCOMPARE(route.construction, source == 135 ? 1 : 2);
                mapping[QString::number(source)] = id;
                inverse[id] = source;
            }
            QVERIFY(project.animationRoutes()->hasBodyPolicies());
            QTemporaryDir work;
            QVERIFY(work.isValid());
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.animationRoutes()->json(), project.animationRoutes()->json());
            const auto built = rebuild(project, tools, work.path());
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-body-policies-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, output);
            auto reopened = Project::fromRom(Rom::open(output));
            QCOMPARE(reopened.bodyAnimationData()->routes().visualPolicies(),
                     project.animationRoutes()->visualPolicies());
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            for (const auto &block : BodyVisualPolicyImage::constructionBlocks()) {
                QCOMPARE(built.image.mid(block.offset - 4, 4),
                         rom.bytes().mid(block.offset - 4, 4));
                QCOMPARE(built.image.mid(block.offset + block.size, 4),
                         rom.bytes().mid(block.offset + block.size, 4));
                auto corrupt = built.image;
                corrupt[block.offset] ^= 1;
                auto bad = Project::fromRom(Rom::fromBytes(corrupt));
                QVERIFY_THROWS_EXCEPTION(Error, bad.loadAnimationRoutes());
            }
            const auto roster =
                machine("actor-roster-mips.cjs", path, work.path(), "original-roster");
            const auto presentation =
                machine("actor-presentation-mips.cjs", path, work.path(), "original-presentation");
            QCOMPARE(machine("actor-roster-mips.cjs", output, work.path(), "rebuilt-roster"),
                     roster);
            QCOMPARE(
                machine("actor-presentation-mips.cjs", output, work.path(), "rebuilt-presentation"),
                presentation);
            const auto mappedRoster = machine("actor-roster-mips.cjs", output, work.path(),
                                              "mapped-roster", {{"classes", mapping}});
            const auto mappedPresentation =
                machine("actor-presentation-mips.cjs", output, work.path(), "mapped-presentation",
                        {{"classes", mapping}});
            QCOMPARE(normalize(mappedRoster, inverse), QJsonValue(roster));
            QCOMPARE(normalize(mappedPresentation, inverse), QJsonValue(presentation));
            replay(reopened, mappedRoster, mappedPresentation);
            QVERIFY(!QTest::currentTestFailed());
            qInfo().noquote() << "Body policy candidate:" << output;
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void privatePosePolicyChanges() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            const int id = project.appendClass(30, "Private pose policy");
            auto copies = *project.animationSequences();
            copies.create(id, 0, *project.animationRoutes());
            project.setAnimationSequences(copies.json());
            const int pose = project.assignPrivateAttack(id, 0, 4, false, 0);
            QCOMPARE(pose, 100);
            QVERIFY(project.privatePoseMappings().isEmpty());
            auto routes = *project.animationRoutes();
            auto entry = routes.entry(id);
            entry.fixedPose = true;
            routes.setEntry(id, entry);
            project.setAnimationRoutes(routes.json());
            const auto check = [&](int state, int expected) {
                const auto data = project.bodyAnimationData();
                ActorPosePlayer player(
                    project.rom(), ActorArt::loadBody(project.rom(), id, id, 0, 0, data), state);
                player.setLinkedObjectKnownAbsent(true);
                QCOMPARE(player.advance(), int(LORDLY_POSE_OK));
                QCOMPARE(player.actor().state, expected);
            };
            check(pose, pose);
            check(40, 0);
            QVERIFY(project.bodyAnimationData()->routes().registeredPose(id, id, 0, pose));
            QVERIFY(!project.bodyAnimationData()->routes().registeredPose(id, id, 0, 40));
            // A deliberately assigned existing primary must also survive the policy.
            QCOMPARE(project.assignPrivateAttack(id, 0, 5, false, 40, false), 40);
            check(40, 40);
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.privatePoseMappings(), project.privatePoseMappings());
            const auto built = rebuild(project, tools, work.path());
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-private-body-policy-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, output);
            auto reopened = Project::fromRom(Rom::open(output));
            QCOMPARE(reopened.bodyAnimationData()->routes().privatePoses(),
                     project.privatePoseMappings());
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            QProcess process;
            process.setProcessChannelMode(QProcess::MergedChannels);
            process.start("node", {QFINDTESTDATA("private-pose-mips.cjs"), path, output});
            QVERIFY(process.waitForFinished(60000));
            const auto log = process.readAll();
            QVERIFY2(process.exitCode() == 0, log.constData());
            qInfo().noquote() << log;
            qInfo().noquote() << "Private body policy candidate:" << output;
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void upgradeEquipmentOnlyExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            auto original = Project::fromRom(Rom::open(path));
            original.appendClass(57, "Equipment policy");
            QTemporaryDir work;
            auto legacy = rebuild(original, tools, work.path()).image;
            QCOMPARE(u32(legacy, AnimationRoutes::descriptorOffset + 8), 3U);
            QCOMPARE(u32(legacy, BodyVisualPolicyImage::descriptorOffset + 8), 1U);
            // Version-three exports before body policies used zero padding here.
            // Recreate that valid prior owner, retaining its actual 60-byte selector.
            legacy.replace(0x2076a8 + 60, 72, QByteArray(72, 0));
            legacy.replace(
                BodyVisualPolicyImage::descriptorOffset + 48, 32,
                QByteArray::fromHex(sha256(BodyVisualPolicyImage::code(Rom::fromBytes(legacy)))));
            auto project = Project::fromRom(Rom::fromBytes(legacy));
            project.loadAnimationRoutes();
            QVERIFY(!project.animationRoutes()->hasBodyPolicies());
            QCOMPARE(project.animationRoutes()->entry(165).equipmentArt, qint16(2));
            QVERIFY(project.animationRoutes()->entry(135).fixedPose);
            QCOMPARE(project.animationRoutes()->entry(135).construction, 1);
            const int clone = project.appendClass(135, "Body policy");
            const auto built = rebuild(project, tools, work.path());
            QCOMPARE(u32(built.image, AnimationRoutes::descriptorOffset + 8), 4U);
            QCOMPARE(u32(built.image, BodyVisualPolicyImage::descriptorOffset + 8), 2U);
            QCOMPARE(u32(built.image, PoseRuntimeImage::descriptorOffset + 24), 1U);
            QVERIFY(built.image.mid(0x2076a8 + 68, 44) != QByteArray(44, 0));
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-body-policy-upgrade-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, output);
            auto reopened = Project::fromRom(Rom::open(output));
            QCOMPARE(reopened.bodyAnimationData()->routes().entry(clone).construction, 1);
            QCOMPARE(reopened.bodyAnimationData()->routes().entry(165).equipmentArt, qint16(2));
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            qInfo().noquote() << "Body policy upgrade candidate:" << output;
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(BodyPolicyTests)
#include "body_policy_tests.moc"
