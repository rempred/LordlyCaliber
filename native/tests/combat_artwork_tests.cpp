#include "engine/combat_artwork.h"
#include "engine/project.h"
#include "rebuild/compiler.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class CombatArtworkTests : public QObject {
    Q_OBJECT
    static QJsonObject reference(const QString &rom, const QString &out, const QString &plan = {}) {
        QProcess process;
        QStringList args{QFINDTESTDATA("combat-artwork-reference.cjs"), rom, out};
        if (!plan.isEmpty())
            args << plan;
        process.start("node", args);
        if (!process.waitForFinished(60000) || process.exitCode())
            throw Error("Combat browser reference failed: " +
                        QString::fromUtf8(process.readAllStandardError()));
        QFile file(out);
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing combat reference");
        return QJsonDocument::fromJson(file.readAll()).object();
    }
    static QByteArray bytes(const QJsonValue &value) {
        return QByteArray::fromBase64(value.toString().toLatin1());
    }
    static SpritePixels changed(CombatArtwork &art, const QString &id, int child, int delta = 1) {
        auto pixels = art.pixels(id, child);
        pixels.color[0] = char(u8(pixels.color, 0) + delta);
        if (!pixels.alpha.isEmpty())
            pixels.alpha[0] =
                char((u8(pixels.alpha, 0) + delta) & (art.original(id).alphaFormat() ? 255 : 15));
        return pixels;
    }
  private slots:
    void browserDeltaPreservation() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            QTemporaryDir work;
            const auto fixture = reference(path, work.filePath("reference.json"));
            auto art = CombatArtwork::load(Rom::open(path));
            QCOMPARE(art.bindings().size(), fixture["counts"].toObject()["bindings"].toInt());
            int checked = 0, nonIndexed = 0;
            for (const auto &value : fixture["samples"].toArray()) {
                const auto row = value.toObject();
                const auto id = row["id"].toString();
                const int child = row["child"].toInt();
                const auto sprite = art.original(id);
                QCOMPARE(sha256(sprite.source()).toLower(), row["decoded"].toString().toLatin1());
                const SpritePixels edit{bytes(row["color"]), bytes(row["alpha"])};
                art.setPixels(id, child, edit);
                const auto current = art.current(id);
                QCOMPARE(current.pixels(child), edit);
                QCOMPARE(current.source().size(), sprite.source().size());
                if (row["rebuilt"].isString())
                    QCOMPARE(sha256(current.source()).toLower(),
                             row["rebuilt"].toString().toLatin1());
                else
                    ++nonIndexed;
                const auto children = row["children"].toArray();
                for (int i = 0; i < children.size(); ++i) {
                    const auto before = sprite.materialize(i), after = current.materialize(i);
                    const auto expected = children[i].toObject();
                    QCOMPARE(sha256(before.color).toLower(),
                             expected["color"].toString().toLatin1());
                    QCOMPARE(sha256(before.alpha).toLower(),
                             expected["alpha"].toString().toLatin1());
                    QCOMPARE(sha256(before.palette).toLower(),
                             expected["palette"].toString().toLatin1());
                    QCOMPARE(after.palette, before.palette);
                    if (i != child) {
                        QCOMPARE(after.color, before.color);
                        QCOMPARE(after.alpha, before.alpha);
                    }
                }
                QCOMPARE(art.mapImage(id, child, art.image(id, child)), edit);
                auto bad = edit;
                bad.color.chop(1);
                QVERIFY_THROWS_EXCEPTION(Error, art.setPixels(id, child, bad));
                if (!sprite.alphaFormat() && !edit.alpha.isEmpty()) {
                    bad = edit;
                    bad.alpha[0] = char(16);
                    QVERIFY_THROWS_EXCEPTION(Error, art.setPixels(id, child, bad));
                }
                art.restore(id, child);
                QVERIFY(!art.modified());
                QCOMPARE(art.current(id).source(), sprite.source());
                ++checked;
            }
            QVERIFY(checked >= 30);
            QVERIFY(nonIndexed >= 4);
            qInfo() << "Browser delta/materialized comparisons:" << checked;
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void exportPersistenceAndSharing() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM/compiler required");
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadCombatArtwork();
            const auto original = *project.combatArtwork();
            auto art = original;
            const auto id = CombatArtwork::identity(0x3bac1c, 0);
            const auto source = original.original(id);
            const auto edit = changed(art, id, 0, 5);
            art.setPixels(id, 0, edit);
            project.setCombatArtwork(art.json());
            QString shared;
            for (const auto &row : original.bindings())
                if (row.id != id && row.resourceKey == original.binding(id).resourceKey) {
                    shared = row.id;
                    break;
                }
            QVERIFY(!shared.isEmpty());
            QTemporaryDir work;
            project.saveFolder(work.filePath("folder"));
            project.saveArchive(work.filePath("project.lcpack"));
            for (const auto &file : {work.filePath("folder"), work.filePath("project.lcpack")}) {
                auto loaded = Project::fromRom(rom);
                loaded.applyManifest(Project::readProject(file));
                QCOMPARE(loaded.combatArtwork()->json(), art.json());
            }
            // Import current and original single-child browser records, then persist a reset.
            QJsonObject entry{
                {"animation", "soldier-thrust"},
                {"artId", 0},
                {"resourceKey", QString("0x%1")
                                    .arg(original.binding(id).resourceKey, 8, 16, QChar('0'))
                                    .toUpper()
                                    .replace("0X", "0x")},
                {"width", source.width()},
                {"height", source.height()},
                {"childOrdinal", 0},
                {"ci8IndicesBase64", QString::fromLatin1(edit.color.toBase64())},
                {"i4IntensityBase64", QString::fromLatin1(edit.alpha.toBase64())}};
            const QJsonObject legacy{
                {"format", "ob64-patch"},
                {"version", 36},
                {"patches",
                 QJsonObject{{"art", QJsonObject{{"schemaVersion", 5},
                                                 {"animations",
                                                  QJsonObject{{"soldier-thrust:0", entry}}}}}}}};
            auto imported = Project::fromRom(rom);
            imported.importLegacy(legacy);
            QCOMPARE(imported.combatArtwork()->json(), art.json());
            QVERIFY(!imported.pendingLegacySections().contains("art.animations"));
            imported.setCombatArtwork(original.json());
            auto reset = Project::fromRom(rom);
            reset.applyManifest(imported.manifest());
            QVERIFY(!reset.combatArtwork()->modified());
            const auto built = rebuild(project, tools, work.path());
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-combat-art-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto candidate = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            qInfo().noquote() << "Combat candidate:" << candidate;
            const auto actualRom = Rom::open(candidate);
            const auto actual = CombatArtwork::load(actualRom);
            QCOMPARE(actual.original(id).source(), art.current(id).source());
            QCOMPARE(actual.original(shared).source(), original.original(shared).source());
            QCOMPARE(actual.binding(shared).resourceKey, original.binding(shared).resourceKey);
            QVERIFY(actual.binding(id).resourceKey != original.binding(id).resourceKey);
            auto restored = built.image.left(rom.bytes().size());
            restored.replace(0x10, 8, rom.bytes().mid(0x10, 8));
            restored.replace(
                CombatArtwork::descriptorOffset, CombatArtwork::descriptorSize,
                rom.bytes().mid(CombatArtwork::descriptorOffset, CombatArtwork::descriptorSize));
            put32(restored, original.binding(id).memberOffset, original.binding(id).resourceKey);
            QCOMPARE(restored, rom.bytes());
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            QJsonArray plan{QJsonObject{
                {"id", id},
                {"descriptor", int(original.binding(id).descriptorKey)},
                {"art", 0},
                {"hash", QString::fromLatin1(sha256(art.current(id).source()).toLower())}}};
            QFile planFile(work.filePath("plan.json"));
            QVERIFY(planFile.open(QIODevice::WriteOnly));
            planFile.write(QJsonDocument(plan).toJson());
            planFile.close();
            QCOMPARE(reference(candidate, work.filePath("reopened.json"),
                               planFile.fileName())["verified"]
                         .toInt(),
                     1);
            QCOMPARE(rebuild(Project::fromRom(actualRom), tools, work.path()).image, built.image);
            // Grow owned data at the tail, then grow again around another export owner's
            // allocation.
            auto again = Project::fromRom(actualRom);
            again.loadCombatArtwork();
            auto next = *again.combatArtwork();
            next.setPixels(shared, 0, changed(next, shared, 0, 9));
            again.setCombatArtwork(next.json());
            const auto second = rebuild(again, tools, work.path());
            const auto secondRom = Rom::fromBytes(second.image);
            const auto reopened = CombatArtwork::load(secondRom);
            QCOMPARE(reopened.original(id).source(), art.current(id).source());
            QCOMPARE(reopened.pixels(shared, 0), next.pixels(shared, 0));
            QCOMPARE(u32(second.image, CombatArtwork::descriptorOffset + 12),
                     u32(built.image, CombatArtwork::descriptorOffset + 12));
            auto thirdBytes = second.image + QByteArray(4096, char(0x35));
            const auto thirdRom = Rom::fromBytes(thirdBytes);
            auto third = CombatArtwork::load(thirdRom);
            QString additional;
            for (const auto &row : third.bindings())
                if (row.id != id && row.id != shared && row.policy == 1) {
                    additional = row.id;
                    break;
                }
            QVERIFY(!additional.isEmpty());
            const auto addSprite = third.original(additional);
            int addedChild = -1;
            for (int i = 0; i < addSprite.children().size(); ++i)
                if (addSprite.children()[i].colorOffset >= 0) {
                    addedChild = i;
                    break;
                }
            QVERIFY(addedChild >= 0);
            third.setPixels(additional, addedChild, changed(third, additional, addedChild, 13));
            const auto moved = third.rebuild(thirdRom, third.decodedResources());
            QVERIFY(u32(moved, CombatArtwork::descriptorOffset + 12) >= quint32(thirdBytes.size()));
            QCOMPARE(moved.mid(second.image.size(), 4096), QByteArray(4096, char(0x35)));
            const auto movedArt = CombatArtwork::load(Rom::fromBytes(moved));
            QCOMPARE(movedArt.pixels(id, 0), edit);
            QCOMPARE(movedArt.pixels(shared, 0), next.pixels(shared, 0));
            auto corrupt = moved;
            put32(corrupt, movedArt.binding(id).memberOffset, original.binding(id).resourceKey);
            QVERIFY_THROWS_EXCEPTION(Error, CombatArtwork::load(Rom::fromBytes(corrupt)));
            corrupt = rom.bytes();
            corrupt[CombatArtwork::descriptorOffset] = '!';
            QVERIFY_THROWS_EXCEPTION(Error,
                                     art.rebuild(Rom::fromBytes(corrupt), art.decodedResources()));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(CombatArtworkTests)
#include "combat_artwork_tests.moc"
