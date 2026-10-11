#include "engine/category_icons.h"
#include "engine/expanded_promotion_resource.h"
#include "engine/icon_artwork.h"
#include "engine/item_icons.h"
#include "engine/project.h"
#include "engine/tool_artwork.h"
#include "rebuild/class_menu_layout.h"
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
    void categoryIconsAndFamilyTabs() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            auto p = Project::fromRom(rom);
            p.loadCategoryIcons();
            p.loadPromotion();
            const auto original = *p.categoryIcons();
            QCOMPARE(original.iconIds().size(), 8);
            QVERIFY(!original.modified());
            auto art = original;
            auto pixels = art.indices(1);
            pixels[0] ^= 1;
            art.setIndices(1, pixels);
            QCOMPARE(art.indices(2), original.indices(2));
            QVERIFY_THROWS_EXCEPTION(Error, art.setIndices(1, QByteArray(195, 0)));
            QCOMPARE(art.indices(1), pixels);
            auto settings = *p.promotion();
            QList<int> families;
            for (int i = 0; i < 12; ++i) {
                const auto family = settings.createFamily(QString("Icon family %1").arg(i));
                families << family;
                settings.assign(i + 1, family);
                const auto icon = art.create(QString("Family crest %1").arg(i), i % 8 + 1);
                QCOMPARE(icon, i + 9);
                art.assignFamily(family, icon);
            }
            settings.setFamilyLinks(families[0],
                                    {families[1]}); // Linked families retain separate tabs.
            p.setPromotion(settings.json());
            p.setCategoryIcons(art.json());
            QVERIFY(p.changeReport().text().contains("Class category icons"));
            QTemporaryDir work;
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-category-icons-XXXXXX");
            QVERIFY(work.isValid());
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            for (const auto &location :
                 {work.filePath("project"), work.filePath("project.lcproject")}) {
                if (location.endsWith(".lcproject"))
                    p.saveArchive(location);
                else
                    p.saveFolder(location);
                auto reopened = Project::fromRom(rom);
                reopened.applyManifest(Project::readProject(location));
                QCOMPARE(reopened.categoryIcons()->json(), art.json());
                QCOMPARE(reopened.promotion()->json(), p.promotion()->json());
            }
            auto libraryOnly = Project::fromRom(rom);
            auto unused = original;
            unused.create("Unused crest", 1);
            libraryOnly.setCategoryIcons(unused.json());
            const auto libraryBuild = rebuild(libraryOnly, tools, work.path());
            QCOMPARE(libraryBuild.image.mid(0x9bc80, 0xd00), rom.bytes().mid(0x9bc80, 0xd00));
            auto libraryReopen = Project::fromRom(Rom::fromBytes(libraryBuild.image));
            libraryReopen.loadCategoryIcons();
            QVERIFY(!libraryReopen.categoryIcons()->hasTabRuntime());
            QCOMPARE(rebuild(libraryReopen, tools, work.path()).image, libraryBuild.image);
            const auto built = rebuild(p, tools, work.path());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(p, built, candidate);
            const auto loaded = Rom::open(candidate);
            const auto after = CategoryIcons::load(loaded);
            for (int icon : art.iconIds())
                QCOMPARE(after.indices(icon), art.indices(icon));
            for (int i = 0; i < 12; ++i)
                QCOMPARE(after.familyIcon(families[i]), i + 9);
            QVERIFY(!after.modified());
            QVERIFY(after.hasTabRuntime());
            auto beforePack = original.decodedPack(), afterPack = after.decodedPack().first(23496);
            for (int y = 0; y < 14; ++y)
                beforePack.replace(CategoryIcons::pixelOffset + y * 112, 14,
                                   afterPack.mid(CategoryIcons::pixelOffset + y * 112, 14));
            QCOMPARE(afterPack, beforePack); // Every unrelated menu texture remains intact.
            QCOMPARE(chaosFrameTitle(loaded), chaosFrameTitle(rom));
            QCOMPARE(after.palette(), original.palette());
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            const auto placement = ClassPlacement::read(loaded);
            QVERIFY(placement.has_value());
            const auto expanded =
                ExpandedPromotionResource::decode(loaded.bytes().mid(0x27d0000, 65536));
            const auto layout = ClassMenuLayout::build(placement->layout, expanded.data, true);
            QJsonObject context{{"classes", double(placement->layout.records)},
                                {"aliases", double(placement->layout.aliases)},
                                {"discovery", double(placement->layout.discovery)},
                                {"classCount", layout.count},
                                {"tabs", double(layout.categoryTabs)},
                                {"entries", double(layout.categoryClasses)},
                                {"current", double(layout.currentCounts)},
                                {"eligible", double(layout.eligibleCounts)},
                                {"ownerBytes", double(layout.ownerBytes)}};
            const auto contextPath = work.filePath("tabs.json");
            QFile contextFile(contextPath);
            QVERIFY(contextFile.open(QIODevice::WriteOnly));
            contextFile.write(QJsonDocument(context).toJson());
            contextFile.close();
            QProcess cpu;
            cpu.start("node", {QFINDTESTDATA("category-tabs-mips.cjs"), candidate, contextPath});
            QVERIFY(cpu.waitForFinished(60000));
            QVERIFY2(cpu.exitCode() == 0,
                     (cpu.readAllStandardOutput() + cpu.readAllStandardError()).constData());
            auto reopened = Project::fromRom(loaded);
            reopened.loadCategoryIcons();
            reopened.loadPromotion();
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            // A second build changes both class table placement and tab assignment.
            const int added = reopened.appendClass(1, "Family tab recruit");
            auto next = *reopened.promotion();
            next.assign(added, families[11]);
            reopened.setPromotion(next.json());
            auto nextArt = *reopened.categoryIcons();
            nextArt.assignFamily(families[0], 2);
            reopened.setCategoryIcons(nextArt.json());
            const auto twice = rebuild(reopened, tools, work.path());
            const auto twiceRom = Rom::fromBytes(twice.image);
            auto twiceProject = Project::fromRom(twiceRom);
            twiceProject.loadPromotion();
            twiceProject.loadCategoryIcons();
            QCOMPARE(twiceProject.categoryIcons()->familyIcon(families[0]), 2);
            QCOMPARE(twiceProject.promotion()->families["assignments"].toArray()[added].toInt(),
                     families[11]);
            const auto raw = twiceProject.categoryIcons()->decodedPack();
            QCOMPARE(u8(raw, 23496 + 8 + added), 20);
            auto cleared = *twiceProject.categoryIcons();
            for (int family : families)
                cleared.assignFamily(family, 0);
            twiceProject.setCategoryIcons(cleared.json());
            const auto clearedBuild = rebuild(twiceProject, tools, work.path());
            auto clearedProject = Project::fromRom(Rom::fromBytes(clearedBuild.image));
            clearedProject.loadCategoryIcons();
            QVERIFY(clearedProject.categoryIcons()->hasTabRuntime());
            QVERIFY(!clearedProject.categoryIcons()->hasFamilyTabs());
            QCOMPARE(clearedProject.categoryIcons()->decodedPack().mid(23496 + 8, 256),
                     QByteArray(256, 0));
            QCOMPARE(rebuild(clearedProject, tools, work.path()).image, clearedBuild.image);
            auto corrupt = built.image;
            put32(corrupt, CategoryIcons::descriptorOffset + 20, 0);
            QVERIFY_THROWS_EXCEPTION(Error, CategoryIcons::load(Rom::fromBytes(corrupt)));
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            qInfo().noquote() << "Family icon export and MIPS checks:" << candidate;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
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
