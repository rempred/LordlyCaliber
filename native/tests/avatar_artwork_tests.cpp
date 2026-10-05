#include "engine/avatar_library.h"
#include "engine/dialogue_draw.h"
#include "engine/project.h"
#include "rebuild/compiler.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class AvatarArtworkTests : public QObject {
    Q_OBJECT
  private slots:
    void imageMappingAndLivePreview() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadAvatarArtwork();
            const auto data = project.classData();
            const auto original = project.avatarArtwork()->words(data, 1, 0);
            QCOMPARE(AvatarArtwork::mapImage(AvatarArtwork::render(original)), original);
            QImage ramp(40, 48, QImage::Format_RGBA8888);
            for (int y = 0; y < 48; ++y)
                for (int x = 0; x < 40; ++x)
                    ramp.setPixelColor(x, y, {x * 255 / 39, y * 255 / 47, (x * 7 + y * 3) % 256});
            QVERIFY_THROWS_EXCEPTION(Error, AvatarArtwork::mapImage(ramp));
            const auto reduced = AvatarArtwork::mapImage(ramp, true);
            QVERIFY(!AvatarArtwork::pack(reduced).isEmpty());
            QCOMPARE(AvatarArtwork::mapImage(ramp, true), reduced);
            QSet<quint16> colors;
            for (int i = 0; i < 1920; ++i) {
                const auto word = u16(reduced, i * 2);
                QVERIFY(word & 1);
                colors.insert(word);
            }
            QVERIFY(colors.size() <= 80);
            QVERIFY(colors.size() > 60);
            ramp.fill(Qt::transparent);
            const auto flat = AvatarArtwork::mapImage(ramp, false, Qt::red);
            for (int i = 0; i < 1920; ++i)
                QCOMPARE(u16(flat, i * 2), quint16(0xf801));
            ramp.fill(QColor(255, 255, 255, 128));
            const auto mixed = AvatarArtwork::mapImage(ramp);
            QCOMPARE(u16(mixed, 0), quint16(0x8421));
            auto edit = *project.avatarArtwork();
            edit.setWords(data, 1, 0, reduced);
            project.setAvatarArtwork(edit.json());
            DialogueDrawing drawing(rom);
            drawing.setPortraitProvider([&](int id, bool variant) {
                return project.avatarArtwork()->words(project.classData(), id, variant ? 1 : 0);
            });
            auto decoded = [](const DialogueTexture &texture) {
                QByteArray words(3840, 0);
                for (int i = 0; i < 1920; ++i)
                    put16(words, i * 2, texture.palette[u8(texture.indices, i)]);
                return words;
            };
            QCOMPARE(decoded(drawing.portrait(1, false)), reduced);
            edit.restore(1, 0);
            project.setAvatarArtwork(edit.json());
            QCOMPARE(decoded(drawing.portrait(1, false)), original);
            QCOMPARE(project.appendClass(1, "Preview portrait"), 165);
            QCOMPARE(decoded(drawing.portrait(165, false)), original);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void portraitEditingAndExport() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadAvatarArtwork();
            const auto original = *project.avatarArtwork();
            auto edits = original;
            auto classes = project.classData();
            const auto desired = original.words(classes, 2, 0);
            QVERIFY(!desired.isEmpty());
            QVERIFY(desired != original.words(classes, 1, 0));
            edits.setWords(classes, 1, 0, desired);
            QCOMPARE(project.appendClass(1, "Portrait knight"), 165);
            classes = project.classData(); // Art was opened before the new class existed.
            edits.setWords(classes, 165, 1, desired);
            project.setAvatarArtwork(edits.json());
            QTemporaryDir work;
            auto saved = Project::fromRom(rom);
            saved.applyManifest(project.manifest());
            QCOMPARE(saved.avatarArtwork()->json(), edits.json());
            project.saveFolder(work.filePath("folder"));
            project.saveArchive(work.filePath("project.lcpack"));
            for (const auto &file : {work.filePath("folder"), work.filePath("project.lcpack")}) {
                auto reopened = Project::fromRom(rom);
                reopened.applyManifest(Project::readProject(file));
                QCOMPARE(reopened.avatarArtwork()->json(), edits.json());
            }
            const auto built = rebuild(project, tools, work.path());
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-portraits-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto candidate = candidates.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            qInfo().noquote() << "Portrait candidate:" << candidate;
            const auto reopenedRom = Rom::open(candidate);
            const auto actual = AvatarLibrary::load(reopenedRom);
            const auto actualClasses = ClassData::load(reopenedRom);
            QCOMPARE(actual.members().size(), 144);
            QVERIFY(actual.placement());
            for (int id = 0; id < classes.entries().size(); ++id) {
                QCOMPARE(actualClasses.entries()[id].appearance.mid(4),
                         classes.entries()[id].appearance.mid(4));
                for (int selector = 0; selector < 2; ++selector) {
                    const auto token = u16(actualClasses.entries()[id].appearance, selector * 2);
                    QCOMPARE(actual.words(token), edits.words(classes, id, selector));
                    if (!edits.modified(id, selector))
                        QCOMPARE(token, u16(classes.entries()[id].appearance, selector * 2));
                }
            }
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("avatar-library-reference.cjs"), candidate,
                                     work.filePath("reference.json")});
            QVERIFY(reference.waitForFinished(30000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("reference.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto decoded = QJsonDocument::fromJson(file.readAll()).object();
            QCOMPARE(decoded["variants"].toArray().size(), 210);
            for (const auto &value : decoded["variants"].toArray()) {
                const auto row = value.toObject();
                QCOMPARE(actual.words(quint16(row["token"].toInt())).toBase64(),
                         row["words"].toString().toLatin1());
            }
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            QCOMPARE(rebuild(project, tools, work.path()).image, built.image);
            auto again = Project::fromRom(reopenedRom);
            QCOMPARE(rebuild(again, tools, work.path()).image, built.image);
            // Both the class data and portrait allocation grow after reopening.
            again.loadAvatarArtwork();
            auto later = *again.avatarArtwork();
            QCOMPARE(again.appendClass(165, "Portrait mage"), 166);
            const auto nextClasses = again.classData();
            later.setWords(nextClasses, 166, 0, original.words(classes, 3, 0));
            again.setAvatarArtwork(later.json());
            const auto second = rebuild(again, tools, work.path());
            const auto secondRom = Rom::fromBytes(second.image);
            const auto secondLibrary = AvatarLibrary::load(secondRom);
            const auto secondClasses = ClassData::load(secondRom);
            QCOMPARE(secondLibrary.members().size(), 145);
            for (int id = 0; id < nextClasses.entries().size(); ++id)
                for (int selector = 0; selector < 2; ++selector)
                    QCOMPARE(secondLibrary.words(
                                 u16(secondClasses.entries()[id].appearance, selector * 2)),
                             later.words(nextClasses, id, selector));
            // Old allocation must be cleared only after every retained owned member moves.
            QVERIFY(secondLibrary.placement()->offset != actual.placement()->offset);
            QCOMPARE(second.image.mid(actual.placement()->offset, actual.placement()->allocation),
                     QByteArray(actual.placement()->allocation, char(-1)));
            auto classFirst = Project::fromRom(rom);
            classFirst.appendClass(1, "Earlier class");
            auto classFirstRom = Rom::fromBytes(rebuild(classFirst, tools, work.path()).image);
            classFirst = Project::fromRom(classFirstRom);
            classFirst.loadAvatarArtwork();
            auto late = *classFirst.avatarArtwork();
            late.setWords(classFirst.classData(), 165, 0, desired);
            classFirst.setAvatarArtwork(late.json());
            const auto classThenArt = Rom::fromBytes(rebuild(classFirst, tools, work.path()).image);
            QCOMPARE(AvatarLibrary::load(classThenArt)
                         .words(u16(ClassData::load(classThenArt).entries()[165].appearance, 0)),
                     desired);
            auto corrupt = built.image;
            corrupt[actual.placement()->offset + actual.placement()->allocation - 1] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, AvatarLibrary::load(Rom::fromBytes(corrupt)));
            corrupt = rom.bytes();
            corrupt[AvatarLibrary::descriptorOffset] = 0;
            auto held = AvatarArtwork::load(Rom::fromBytes(corrupt));
            held.setWords(classes, 1, 0, desired);
            auto copy = classes;
            QVERIFY_THROWS_EXCEPTION(
                Error, held.rebuild(Rom::fromBytes(corrupt), copy, held.decodedBundles()));
            corrupt = rom.bytes();
            corrupt[0x45b00 + 3] ^= 2;
            copy = classes;
            QVERIFY_THROWS_EXCEPTION(
                Error, edits.rebuild(Rom::fromBytes(corrupt), copy, edits.decodedBundles()));
            auto invalid = desired;
            for (int i = 0; i < 81; ++i)
                put16(invalid, i * 2, quint16(i * 2 + 1));
            QVERIFY_THROWS_EXCEPTION(Error, edits.setWords(classes, 1, 0, invalid));
            invalid = desired;
            put16(invalid, 0, 0);
            QVERIFY_THROWS_EXCEPTION(Error, edits.setWords(classes, 1, 0, invalid));
            QJsonObject art{
                {"schemaVersion", 5},
                {"avatars",
                 QJsonObject{{"1:0-1", QJsonObject{{"classId", 1},
                                                   {"selectorIndices", QJsonArray{0, 1}},
                                                   {"pixelsRgba5551BeBase64",
                                                    QString::fromLatin1(desired.toBase64())}}}}}};
            auto legacy = Project::fromRom(rom);
            legacy.importLegacy({{"format", "ob64-patch"},
                                 {"version", 36},
                                 {"patches", QJsonObject{{"art", art}}}});
            QVERIFY(legacy.pendingLegacySections().isEmpty());
            for (int selector = 0; selector < 2; ++selector)
                QCOMPARE(legacy.avatarArtwork()->words(legacy.classData(), 1, selector), desired);
            legacy.setAvatarArtwork(original.json());
            auto empty = Project::fromRom(rom);
            empty.applyManifest(legacy.manifest());
            QVERIFY(!empty.avatarArtwork()->modified());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void currentRomLibraryAndDialogue() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            const auto library = AvatarLibrary::load(rom);
            QCOMPARE(library.members().size(), 142);
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("avatar-library-reference.cjs"), path,
                                     work.filePath("avatars.json")});
            QVERIFY(reference.waitForFinished(30000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("avatars.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto expected = QJsonDocument::fromJson(file.readAll()).object();
            QCOMPARE(expected["bundles"].toInt(), 140);
            QCOMPARE(expected["variants"].toArray().size(), 208);
            for (const auto &value : expected["variants"].toArray()) {
                const auto row = value.toObject();
                const auto token = quint16(row["token"].toInt());
                QCOMPARE(library.words(token).toBase64(), row["words"].toString().toLatin1());
                const auto image = library.image(token);
                QByteArray rgba;
                for (int y = 0; y < 48; ++y)
                    rgba.append(reinterpret_cast<const char *>(image.constScanLine(y)), 160);
                QCOMPARE(rgba.toBase64(), row["rgba"].toString().toLatin1());
            }
            const auto classes = ClassData::load(rom);
            DialogueDrawing drawing(rom);
            for (int id = 0; id < classes.entries().size(); ++id)
                for (bool variant : {false, true}) {
                    const auto token = u16(classes.entries()[id].appearance, variant ? 2 : 0);
                    const auto texture = drawing.portrait(id, variant);
                    if (!token) {
                        QCOMPARE(texture.indices, QByteArray(1920, 0));
                        continue;
                    }
                    const auto &bundle = library.bundle(token >> 3);
                    QCOMPARE(texture.indices, bundle.indices);
                    const auto palette = library.palette(token);
                    for (int i = 0; i < 80; ++i)
                        QCOMPARE(texture.palette[i], u16(palette, i * 2));
                }
            auto relocated = rom.bytes();
            const auto at = quint32(relocated.size());
            QByteArray directory(4 + library.members().size() * 4, 0);
            put32(directory, 0, quint32(directory.size() - 4));
            for (int i = 0; i < library.members().size(); ++i)
                put32(directory, 4 + i * 4, library.members()[i]);
            const auto token = u16(classes.entries()[1].appearance, 0),
                       replacement = u16(classes.entries()[2].appearance, 0);
            QVERIFY(token && replacement);
            put32(directory, 4 + (token >> 3) * 4, library.members()[replacement >> 3]);
            relocated += directory;
            const auto key = at - AvatarLibrary::resourceBase;
            for (const auto &[hi, lo] : AvatarLibrary::sites()) {
                put32(relocated, hi, (u32(relocated, hi) & 0xffff0000) | (key >> 16));
                put32(relocated, lo, (u32(relocated, lo) & 0xffff0000) | (key & 65535));
            }
            DialogueDrawing changed(Rom::fromBytes(relocated));
            QCOMPARE(changed.portrait(1, false).indices, library.bundle(replacement >> 3).indices);
            auto invalid = relocated;
            invalid[0x45b00 + 3] ^= 2;
            QVERIFY_THROWS_EXCEPTION(Error, AvatarLibrary::load(Rom::fromBytes(invalid)));
            invalid = relocated;
            put32(invalid, at, 8193 * 4);
            QVERIFY_THROWS_EXCEPTION(Error, AvatarLibrary::load(Rom::fromBytes(invalid)));
            QVERIFY_THROWS_EXCEPTION(Error, library.bundle(0));
            QVERIFY_THROWS_EXCEPTION(Error, library.bundle(142));
            auto project = Project::fromRom(rom);
            QCOMPARE(project.appendClass(1, "Portrait test"), 165);
            const auto built = rebuild(project, tools, work.path());
            DialogueDrawing added(Rom::fromBytes(built.image));
            QCOMPARE(added.portrait(165, false).indices, drawing.portrait(1, false).indices);
            QCOMPARE(added.portrait(165, false).palette, drawing.portrait(1, false).palette);
            QVERIFY_THROWS_EXCEPTION(Error, added.portrait(166, false));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(AvatarArtworkTests)
#include "avatar_artwork_tests.moc"
