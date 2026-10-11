#include "engine/icon_artwork.h"
#include "engine/item_icons.h"
#include "engine/project.h"
#include "rebuild/compiler.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
namespace {
QByteArray rgba(const QImage &image) {
    QByteArray bytes;
    for (int y = 0; y < image.height(); ++y)
        bytes.append(reinterpret_cast<const char *>(image.constScanLine(y)), image.width() * 4);
    return bytes;
}
} // namespace
class IconArtworkTests : public QObject {
    Q_OBJECT
  private slots:
    void readEditExportReopen() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadIconArtwork();
            const auto original = *project.iconArtwork();
            ItemIcons reader(rom);
            for (int p = 0; p < 2; ++p)
                for (int id = 1; id <= IconArtwork::specs()[p].count; ++id) {
                    const auto item = quint16((p ? 0x200 : 0) | id);
                    QCOMPARE(rgba(original.image(item)), reader.image(item).rgba);
                    QCOMPARE(original.mapImage(item, original.image(item), false),
                             original.words(item));
                }
            QCOMPARE(original.rebuild(rom, original.decodedPacks()), rom.bytes());
            auto edited = original;
            for (const auto item : {quint16(1), quint16(0x201)}) {
                auto bytes = edited.words(item);
                const auto palette = edited.palette(IconArtwork::packIndex(item));
                for (int p = 0; p < 256; ++p)
                    put16(bytes, p * 2, palette[(p * 37) % palette.size()]);
                edited.setWords(item, bytes);
            }
            project.setIconArtwork(edited.json());
            QVERIFY(project.iconArtwork()->modified());
            QTemporaryDir work;
            QTemporaryDir output(QFileInfo(path).absolutePath() + "/Test ROMs/native-icons-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            project.saveFolder(work.filePath("project"));
            project.saveArchive(work.filePath("project.lcproject"));
            for (const auto &location :
                 {work.filePath("project"), work.filePath("project.lcproject")}) {
                auto reopened = Project::fromRom(rom);
                reopened.applyManifest(Project::readProject(location));
                QCOMPARE(reopened.iconArtwork()->json(), edited.json());
            }
            const auto built = rebuild(project, tools, work.path());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            const auto loaded = Rom::open(candidate);
            QCOMPARE(loaded.bytes(), built.image);
            const auto reopened = IconArtwork::load(loaded);
            const ItemIcons decoder(loaded);
            for (int p = 0; p < 2; ++p)
                for (int id = 1; id <= IconArtwork::specs()[p].count; ++id) {
                    const auto item = quint16((p ? 0x200 : 0) | id);
                    QCOMPARE(reopened.words(item), edited.words(item));
                    QCOMPARE(rgba(edited.image(item)), decoder.image(item).rgba);
                    if (id != 1)
                        QCOMPARE(reopened.words(item), original.words(item));
                }
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            auto preserved = rom.bytes();
            preserved.replace(
                IconArtwork::descriptorOffset, IconArtwork::descriptorSize,
                built.image.mid(IconArtwork::descriptorOffset, IconArtwork::descriptorSize));
            for (const auto &spec : IconArtwork::specs())
                for (const auto &[hi, lo] : spec.sites) {
                    put32(preserved, hi, u32(built.image, hi));
                    put32(preserved, lo, u32(built.image, lo));
                }
            Rom::updateChecksum(preserved);
            QCOMPARE(built.image.first(rom.bytes().size()), preserved);
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("item-icons-reference.cjs"), candidate,
                                     work.filePath("reference.json")});
            QVERIFY(reference.waitForFinished(30000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("reference.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto images = QJsonDocument::fromJson(file.readAll()).object()["icons"].toArray();
            QCOMPARE(images.size(), 321);
            for (const auto &value : images) {
                const auto row = value.toObject();
                QCOMPARE(rgba(edited.image(quint16(row["item"].toInt()))).toHex(),
                         row["rgba"].toString().toLatin1());
            }
            QCOMPARE(rebuild(project, tools, work.path()).image, built.image);
            auto again = Project::fromRom(loaded);
            again.loadIconArtwork();
            QCOMPARE(rebuild(again, tools, work.path()).image, built.image);
            auto next = *again.iconArtwork();
            next.setWords(2, original.words(3));
            again.setIconArtwork(next.json());
            const auto twice = rebuild(again, tools, work.path());
            QCOMPARE(u32(twice.image, IconArtwork::descriptorOffset + 12),
                     u32(built.image, IconArtwork::descriptorOffset + 12));
            QCOMPARE(IconArtwork::load(Rom::fromBytes(twice.image)).words(2), original.words(3));
            // Palette ownership is per complete pack; a foreign color cannot displace
            // a color still used by other icons.
            auto overflow = original;
            auto words = overflow.words(1);
            for (int i = 0; i < 256; ++i)
                put16(words, i * 2, quint16(i * 2 + 1));
            QVERIFY_THROWS_EXCEPTION(Error, overflow.setWords(1, words));
            QVERIFY(!overflow.modified());
            QImage source(16, 16, QImage::Format_RGBA8888);
            source.fill(QColor(242, 37, 149));
            source.setPixelColor(1, 1, QColor(21, 91, 41, 0));
            const auto mapped = original.mapImage(1, source, true);
            overflow.setWords(1, mapped);
            QVERIFY(overflow.modified());
            QCOMPARE(overflow.image(1).pixelColor(1, 1).alpha(), 0);
            QVERIFY_THROWS_EXCEPTION(Error,
                                     original.mapImage(1, QImage(17, 16, QImage::Format_RGBA8888)));
            for (const auto &spec : IconArtwork::specs())
                for (const auto &[hi, lo] : spec.sites) {
                    auto bad = rom.bytes();
                    bad[hi] ^= 1;
                    QVERIFY_THROWS_EXCEPTION(Error, IconArtwork::load(Rom::fromBytes(bad)));
                    bad = rom.bytes();
                    bad[lo + 3] ^= 2;
                    QVERIFY_THROWS_EXCEPTION(Error, IconArtwork::load(Rom::fromBytes(bad)));
                }
            auto occupied = rom.bytes();
            occupied[IconArtwork::descriptorOffset] = 0;
            auto held = IconArtwork::load(Rom::fromBytes(occupied));
            held.setWords(1, edited.words(1));
            QVERIFY_THROWS_EXCEPTION(Error,
                                     held.rebuild(Rom::fromBytes(occupied), held.decodedPacks()));
            auto corrupt = built.image;
            corrupt[u32(corrupt, IconArtwork::descriptorOffset + 12) + 12] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, IconArtwork::load(Rom::fromBytes(corrupt)));
            // A new class allocates its rebuilt tables in the same expanding ROM.
            QCOMPARE(project.appendClass(1, "Icon test"), 165);
            const auto combined = rebuild(project, tools, work.path());
            const auto combinedRom = Rom::fromBytes(combined.image);
            QCOMPARE(Project::fromRom(combinedRom).records(Table::Classes).size(), 166);
            QCOMPARE(IconArtwork::load(combinedRom).words(1), edited.words(1));
            auto combinedProject = Project::fromRom(combinedRom);
            const auto repeatedCombined = rebuild(combinedProject, tools, work.path());
            QCOMPARE(repeatedCombined.image, combined.image);
            // Both editing orders and a later growth must retain the other domain.
            auto iconsFirst = Project::fromRom(loaded);
            QCOMPARE(iconsFirst.appendClass(1, "Later class"), 165);
            const auto iconsThenClasses = rebuild(iconsFirst, tools, work.path());
            QCOMPARE(IconArtwork::load(Rom::fromBytes(iconsThenClasses.image)).words(1),
                     edited.words(1));
            combinedProject.loadIconArtwork();
            auto laterArt = *combinedProject.iconArtwork();
            laterArt.setWords(3, original.words(4));
            combinedProject.setIconArtwork(laterArt.json());
            QCOMPARE(combinedProject.appendClass(1, "Another class"), 166);
            const auto extended = rebuild(combinedProject, tools, work.path());
            const auto extendedRom = Rom::fromBytes(extended.image);
            QCOMPARE(Project::fromRom(extendedRom).records(Table::Classes).size(), 167);
            QCOMPARE(IconArtwork::load(extendedRom).words(3), original.words(4));
            auto foreignOwner = combined.image;
            foreignOwner[0xaeb8c + 8] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, Project::fromRom(Rom::fromBytes(foreignOwner)));
            auto foreignIcon = combined.image;
            foreignIcon[0xaee98 + 3] ^= 2;
            QVERIFY_THROWS_EXCEPTION(Error, Project::fromRom(Rom::fromBytes(foreignIcon)));
            const auto combinedDir = output.filePath("with-class");
            QDir().mkpath(combinedDir);
            exportRom(project, combined, QDir(combinedDir).filePath(QFileInfo(path).fileName()));
            qInfo().noquote() << "Candidates:" << output.path();
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void browserImportAndReset() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadIconArtwork();
            const auto words = project.iconArtwork()->words(2);
            QJsonObject art{
                {"schemaVersion", 5},
                {"avatars", QJsonObject{}},
                {"icons", QJsonObject{{"equipment:1",
                                       QJsonObject{{"pack", "equipment"},
                                                   {"itemId", 1},
                                                   {"pixelsRgba5551BeBase64",
                                                    QString::fromLatin1(words.toBase64())}}}}}};
            QJsonObject legacy{
                {"format", "ob64-patch"}, {"version", 36}, {"patches", QJsonObject{{"art", art}}}};
            project.importLegacy(legacy);
            QCOMPARE(project.iconArtwork()->words(1), words);
            QVERIFY(project.pendingLegacySections().isEmpty());
            auto empty = project.iconArtwork()->json();
            empty["icons"] = QJsonObject{};
            project.setIconArtwork(empty);
            auto reopened = Project::fromRom(rom);
            reopened.applyManifest(project.manifest());
            QVERIFY(!reopened.iconArtwork()->modified());
            art["futureArtwork"] = QJsonObject{{"future", 1}};
            legacy["patches"] = QJsonObject{{"art", art}};
            project.importLegacy(legacy);
            QCOMPARE(project.pendingLegacySections(), QStringList{"art.futureArtwork"});
            const auto before = project.iconArtwork()->json();
            art["icons"] = QJsonObject{
                {"equipment:1",
                 QJsonObject{{"pack", "equipment"},
                             {"itemId", 2},
                             {"pixelsRgba5551BeBase64", QString::fromLatin1(words.toBase64())}}}};
            legacy["patches"] = QJsonObject{{"art", art}};
            QVERIFY_THROWS_EXCEPTION(Error, project.importLegacy(legacy));
            QCOMPARE(project.iconArtwork()->json(), before);
            QVERIFY_THROWS_EXCEPTION(Error, IconArtwork().words(1));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(IconArtworkTests)
#include "icon_artwork_tests.moc"
