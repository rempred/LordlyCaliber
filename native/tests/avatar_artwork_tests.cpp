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
