#include "engine/project.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include "engine/sprite_sources.h"
using namespace lordly;
namespace {
QByteArray pixels(const QImage &image) {
    QByteArray bytes;
    for (int y = 0; y < image.height(); ++y)
        bytes.append(reinterpret_cast<const char *>(image.constScanLine(y)), image.width() * 4);
    return bytes;
}
QVector<SpriteAsset> examples() {
    QVector<SpriteAsset> result;
    quint32 seed = 0x8178;
    auto random = [&]() {
        seed = 1664525 * seed + 1013904223;
        return seed >> 24;
    };
    for (int i = 0; i < 24; ++i) {
        auto asset = SpriteAsset::blank(QString("Test %1").arg(i), {7 + i % 4, 5 + i % 3});
        asset.anchor = {i % 3 - 1, i % 5 - 2};
        asset.kind = "sequence";
        for (int f = 0; f < 3; ++f) {
            auto frame = asset.frames.first();
            frame.id = SpriteLibrary::newId();
            frame.name = QString("Frame %1").arg(f);
            frame.ticks = f == 0 ? 0 : f * 7;
            frame.layers.clear();
            for (int l = 0; l < 4; ++l) {
                SpriteLayer layer{SpriteLibrary::newId(),
                                  QString("Layer %1").arg(l),
                                  l != 2,
                                  {l - 2, f - 1},
                                  QImage(QSize(3 + l, 4 + l), QImage::Format_RGBA8888)};
                for (int y = 0; y < layer.image.height(); ++y)
                    for (int x = 0; x < layer.image.width(); ++x) {
                        auto p = layer.image.scanLine(y) + 4 * x;
                        for (int c = 0; c < 4; ++c)
                            p[c] = uchar(random());
                        if (x == 0)
                            p[3] = 0;
                    }
                frame.layers.append(layer);
            }
            if (f == 0)
                asset.frames.clear();
            asset.frames.append(frame);
        }
        asset.provenance = {{"source", "synthetic"}, {"entry", i}};
        result.append(asset);
    }
    return result;
}
} // namespace
class SpriteLibraryTests : public QObject {
    Q_OBJECT
  private slots:
    void gameFramePlacement() {
        ActorFrameLayer first;
        first.metadata = {7, {-1, -1}, 2, 1, 0, 2048, 1024};
        first.pixels.image = {2, 1, QByteArray::fromHex("ff0000ff0000ffff")};
        ActorFrameLayer second;
        second.metadata = {8, {1, 0}, 1, 1, 0, 1024, 1024};
        second.pixels.image = {1, 1, QByteArray::fromHex("00ff0080")};
        auto last = second;
        last.metadata.origin = {0, 2};
        auto asset = spriteAssetFromFrames("Placed layers", {{0, {first, second}}, {23, {last}}});
        QCOMPARE(asset.size, QSize(4, 4));
        QCOMPARE(asset.anchor, QPoint(2, 1));
        QCOMPARE(asset.frames[0].ticks, 0);
        QCOMPARE(asset.frames[1].ticks, 23);
        QCOMPARE(asset.frames[0].layers[0].position, QPoint(0, 0));
        QCOMPARE(asset.frames[0].layers[1].position, QPoint(3, 1));
        QCOMPARE(asset.frames[1].layers[0].position, QPoint(2, 3));
        QCOMPARE(pixels(asset.frames[0].layers[0].image), QByteArray::fromHex("ff0000ffff0000ff0000ffff0000ffff"));
        QCOMPARE(asset.composite(0).pixelColor(3, 1), QColor(0, 255, 0, 128));
        QCOMPARE(asset.composite(1).pixelColor(0, 0), QColor(0, 0, 0, 0));
        QCOMPARE(SpriteAsset::fromAssetFile(asset.assetFile()), asset);
        first.metadata.scaleX = 512;
        first.metadata.origin = {-3, -1};
        const auto fractional = spriteAssetFromFrames("Fractional", {{8, {first}}});
        QCOMPARE(fractional.size, QSize(2, 1));
        QCOMPARE(fractional.anchor, QPoint(2, 1));
        QCOMPARE(fractional.composite(0).pixelColor(0, 0), QColor(255, 0, 0));
        QCOMPARE(fractional.composite(0).pixelColor(1, 0), QColor(0, 0, 0, 0));
        first.metadata.scaleX = 0;
        const auto invisible = spriteAssetFromFrames("Invisible", {{8, {first}}, {8, {}}});
        QCOMPARE(invisible.composite(0).pixelColor(0, 0), QColor(0, 0, 0, 0));
        QCOMPARE(invisible.frames.size(), 2);
        first.metadata.scaleX = 65535;
        first.pixels.image = {128, 1, QByteArray(512, '\0')};
        QVERIFY_THROWS_EXCEPTION(Error, spriteAssetFromFrames("Oversized", {{8, {first}}}));
        QVERIFY_THROWS_EXCEPTION(Error, spriteAssetFromFrames("Empty", {}));
    }
    void gamePoseFrames() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto body = BodyAnimationData::load(rom);
            int checked = 0;
            for (int id : {1, 2, 10}) {
                const int field = u8(ClassData::load(rom).entries()[id].record, 69);
                const auto art = ActorArt::loadBody(rom, id, field ? field : id, 0, 0, body);
                const auto asset = spriteAssetFromPose(rom, art, 40, "ROM pose");
                int frame = 0;
                for (const auto &record : art.poses()[40].records) {
                    if (record.opcode != 1) continue;
                    const auto layers = art.bodyFrame(rom, u8(record.operands, 0), art.defaultAppearance(rom));
                    const auto &copy = asset.frames[frame++];
                    QCOMPARE(copy.ticks, int(u8(record.operands, 1)));
                    QCOMPARE(copy.layers.size(), layers.size());
                    for (int l = 0; l < layers.size(); ++l) {
                        const auto &original = layers[l];
                        if (original.metadata.scaleX == 1024 && original.metadata.scaleY == 1024) {
                            QCOMPARE(copy.layers[l].position - asset.anchor, original.metadata.origin);
                            QCOMPARE(pixels(copy.layers[l].image), original.pixels.image.rgba);
                            ++checked;
                        }
                    }
                }
                QCOMPARE(asset.frames.size(), frame);
                QCOMPARE(SpriteAsset::fromAssetFile(asset.assetFile(true)), asset);
            }
            QVERIFY(checked > 5);
            const auto actor = ActorArt::load(rom, 1);
            const auto asset = spriteAssetFromPose(rom, actor, 0, "Cutscene actor");
            QVERIFY(!asset.frames.isEmpty());
            QVERIFY_THROWS_EXCEPTION(Error, spriteAssetFromPose(rom, actor, -1, "Invalid"));
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void browserCompatibility() {
        try {
            QTemporaryDir work;
            QJsonArray files;
            const auto assets = examples();
            for (const auto &asset : assets) {
                files.append(asset.assetFile(true));
                QCOMPARE(SpriteAsset::fromAssetFile(asset.assetFile()), asset);
                QCOMPARE(SpriteAsset::fromAssetFile(asset.assetFile(true)), asset);
            }
            atomicWrite(work.filePath("input.json"), QJsonDocument(files).toJson());
            QProcess node;
            node.start("node", {QString(LORDLY_SOURCE_DIR) + "/tests/sprite-library-reference.cjs",
                                work.filePath("input.json"), work.filePath("output.json")});
            QVERIFY(node.waitForFinished(30000));
            QCOMPARE(node.exitCode(), 0);
            QFile file(work.filePath("output.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto expected = QJsonDocument::fromJson(file.readAll()).array();
            QCOMPARE(expected.size(), assets.size());
            for (int i = 0; i < assets.size(); ++i) {
                const auto row = expected[i].toObject();
                for (int f = 0; f < assets[i].frames.size(); ++f)
                    QCOMPARE(pixels(assets[i].composite(f)).toBase64(),
                             row["composite"].toArray()[f].toString().toLatin1());
                QCOMPARE(assets[i].rotated(true).json(true),
                         SpriteAsset::fromAssetFile(row["rotated"].toObject()).json(true));
                QCOMPARE(assets[i].resized({13, 9}, true).json(true),
                         SpriteAsset::fromAssetFile(row["scaled"].toObject()).json(true));
                QCOMPARE(assets[i].rotated(true).rotated(false), assets[i]);
            }
            // Version 1 supplies layer geometry through the asset dimensions.
            auto old = SpriteAsset::blank("Old", {4, 3}).assetFile(true);
            old["version"] = 1;
            auto asset = old["asset"].toObject();
            auto frames = asset["frames"].toArray();
            auto frame = frames[0].toObject();
            auto layers = frame["layers"].toArray();
            auto layer = layers[0].toObject();
            for (const auto &key : {"width", "height", "x", "y"})
                layer.remove(key);
            layers[0] = layer;
            frame["layers"] = layers;
            frames[0] = frame;
            asset["frames"] = frames;
            old["asset"] = asset;
            QCOMPARE(SpriteAsset::fromAssetFile(old).frames.first().layers.first().image.size(),
                     QSize(4, 3));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void pixelToolsAndLimits() {
        auto asset = SpriteAsset::blank("Pixels", {9, 7});
        auto image = asset.frames[0].layers[0].image;
        const auto diagonal = spriteLine(image, {-3, -3}, {8, 8}, QColor(1, 2, 3, 128), 1);
        QCOMPARE(image.pixelColor(1, 1), QColor(Qt::transparent));
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 9; ++x)
                QCOMPARE(diagonal.pixelColor(x, y),
                         x == y ? QColor(1, 2, 3, 128) : QColor(Qt::transparent));
        const auto selected = spriteLine(image, {-4, 3}, {12, 3}, Qt::red, 3, {2, 2, 3, 3});
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 9; ++x)
                QCOMPARE(selected.pixelColor(x, y), QRect(2, 2, 3, 3).contains(x, y)
                                                        ? QColor(Qt::red)
                                                        : QColor(Qt::transparent));
        auto bordered = spriteLine(image, {4, 0}, {4, 6}, Qt::black);
        auto filled = spriteFill(bordered, {0, 0}, Qt::red);
        QCOMPARE(filled.pixelColor(3, 6), QColor(Qt::red));
        QCOMPARE(filled.pixelColor(5, 3), QColor(Qt::transparent));
        QCOMPARE(filled.pixelColor(4, 3), QColor(Qt::black));
        auto restricted = spriteFill(image, {3, 3}, Qt::blue, {2, 2, 3, 3});
        QCOMPARE(restricted, spriteLine(image, {3, 3}, {3, 3}, Qt::blue, 3));
        QCOMPARE(spriteLine(image, {-8, -8}, {-2, -2}, Qt::red, 1), image);
        QCOMPARE(spriteFill(image, {8, 6}, Qt::red, {1, 1, 2, 2}), image);
        QCOMPARE(spriteShift(diagonal, {3, 2}, true).pixelColor(3, 2), diagonal.pixelColor(0, 0));
        QCOMPARE(spriteShift(diagonal, {1, 1}).pixelColor(0, 0), QColor(Qt::transparent));
        auto translucent = image;
        translucent.setPixelColor(2, 2, QColor(201, 103, 57, 0));
        translucent.setPixelColor(3, 2, QColor(201, 103, 57, 99));
        const auto moved = spriteMoveSelection(translucent, {2, 2, 2, 1}, {1, 0});
        QCOMPARE(moved.pixelColor(2, 2), QColor(Qt::transparent));
        QCOMPARE(moved.pixelColor(3, 2), QColor(201, 103, 57, 0));
        QCOMPARE(moved.pixelColor(4, 2), QColor(201, 103, 57, 99));
        QCOMPARE(spriteMoveSelection(translucent, {8, 8, 1, 1}, {1, 0}), translucent);
        asset.frames[0].layers[0].image = diagonal;
        asset.frames[0].layers[0].position = {-2, 1};
        const auto cropped = asset.resized({6, 5}, false, {1, -1}, true);
        QCOMPARE(cropped.frames[0].layers[0].position, QPoint());
        QCOMPARE(cropped.frames[0].layers[0].image.pixelColor(0, 1), diagonal.pixelColor(1, 1));
        auto file = asset.assetFile(true), bad = file;
        bad["version"] = 99;
        QVERIFY_THROWS_EXCEPTION(Error, SpriteAsset::fromAssetFile(bad));
        auto payload = file["asset"].toObject();
        payload["width"] = 4097;
        bad["version"] = 2;
        bad["asset"] = payload;
        QVERIFY_THROWS_EXCEPTION(Error, SpriteAsset::fromAssetFile(bad));
        SpriteLibrary library;
        library.replace(asset);
        const auto before = library.json();
        auto invalid = asset;
        invalid.frames[0].layers.append(invalid.frames[0].layers[0]);
        QVERIFY_THROWS_EXCEPTION(Error, library.replace(invalid));
        QCOMPARE(library.json(), before);
        QVERIFY_THROWS_EXCEPTION(Error, spriteScale(QImage(), {5, 5}));
        auto native = asset.assetFile();
        payload = native["asset"].toObject();
        auto frames = payload["frames"].toArray();
        auto frame = frames[0].toObject();
        auto layers = frame["layers"].toArray();
        auto layer = layers[0].toObject();
        layer["width"] = 5;
        layers[0] = layer;
        frame["layers"] = layers;
        frames[0] = frame;
        payload["frames"] = frames;
        native["asset"] = payload;
        QVERIFY_THROWS_EXCEPTION(Error, SpriteAsset::fromAssetFile(native));
    }
    void projectPersistenceAndArchiveBounds() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            SpriteLibrary library;
            for (const auto &asset : examples())
                library.replace(asset);
            const QJsonObject legacy{
                {"format", "ob64-patch"},
                {"version", 36},
                {"patches", QJsonObject{{"spriteLibrary", library.json(true)}}}};
            project.importLegacy(legacy);
            QCOMPARE(project.sprites().json(), library.json());
            QVERIFY(!project.manifest()["legacyImport"].toObject()["patches"].toObject().contains("spriteLibrary"));
            QVERIFY(!project.pendingLegacySections().contains("spriteLibrary"));
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            project.saveArchive(work.filePath("project.lcproject"));
            for (const auto &location :
                 {work.filePath("project"), work.filePath("project.lcproject")}) {
                auto reopened = Project::fromRom(rom);
                reopened.applyManifest(Project::readProject(location));
                QCOMPARE(reopened.sprites().json(), library.json());
            }
            project.setSprites({});
            auto manifest = project.manifest();
            manifest["legacyImport"]=legacy; // An older native save can retain the browser copy.
            auto reopened = Project::fromRom(rom);
            reopened.applyManifest(manifest);
            QVERIFY(reopened.sprites().assets().isEmpty());
            auto malformed = manifest;
            malformed["spriteLibrary"] = 3;
            QVERIFY_THROWS_EXCEPTION(Error, reopened.applyManifest(malformed));
            QVERIFY(reopened.sprites().assets().isEmpty());
            // This valid 65 MiB document exceeded the former limit but contains almost no art.
            QByteArray large(65 * 1024 * 1024, ' ');
            large.append("{\"value\":17}");
            const auto packed = QByteArray("LC05PACK\n") + qCompress(large, 1);
            atomicWrite(work.filePath("large"), packed);
            QCOMPARE(Project::readProject(work.filePath("large"))["value"].toInt(), 17);
            auto tooShort = packed;
            put32(tooShort, 9, 16);
            atomicWrite(work.filePath("bad"), tooShort);
            QVERIFY_THROWS_EXCEPTION(Error, Project::readProject(work.filePath("bad")));
            auto tooLong = packed;
            put32(tooLong, 9, quint32(large.size() + 1));
            atomicWrite(work.filePath("bad"), tooLong);
            QVERIFY_THROWS_EXCEPTION(Error, Project::readProject(work.filePath("bad")));
            atomicWrite(work.filePath("bad"), packed + "extra");
            QVERIFY_THROWS_EXCEPTION(Error, Project::readProject(work.filePath("bad")));
            atomicWrite(work.filePath("bad"), packed.chopped(3));
            QVERIFY_THROWS_EXCEPTION(Error, Project::readProject(work.filePath("bad")));
            auto overBudget = packed;
            put32(overBudget, 9, 513 * 1024 * 1024);
            atomicWrite(work.filePath("bad"), overBudget);
            QVERIFY_THROWS_EXCEPTION(Error, Project::readProject(work.filePath("bad")));
            QCOMPARE(project.rebuildRecords(), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(SpriteLibraryTests)
#include "sprite_library_tests.moc"
