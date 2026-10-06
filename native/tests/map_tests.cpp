#include "engine/archive.h"
#include "engine/scenario_map.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <bit>
using namespace lordly;
class MapTests : public QObject {
    Q_OBJECT
  private slots:
    void calibratedTerrain() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            const auto first = ScenarioMap::load(rom, 1);
            QVERIFY(first.imageBounds);
            QVERIFY(first.imageGridBounds());
            const auto second = ScenarioMap::load(rom, 2);
            QVERIFY(second.imageBounds);
            // Independent manual reference points: the world origin is near the
            // center of the matched Tenne/Volmus reference images, not the grid origin.
            QVERIFY(first.imageBounds->contains(QPointF(0, 0)));
            QVERIFY(second.imageBounds->contains(QPointF(0, 0)));
            int calibrated = 0, images = 0;
            for (int key = 1; key <= 64; ++key) {
                const auto map = ScenarioMap::load(rom, key);
                images += map.image.has_value();
                if (map.imageBounds) {
                    QVERIFY(map.image);
                    const auto grid = map.imageGridBounds();
                    QVERIFY(grid);
                    QVERIFY(grid->width() > 0 && grid->height() > 0);
                    ++calibrated;
                }
            }
            qInfo() << "Calibrated" << calibrated << "of" << images << "ROM image selections";
            QCOMPARE(calibrated, 49);
            QCOMPARE(images, 57);
            QVERIFY(!ScenarioMap::load(rom, 10).imageBounds);
            // A different but well-formed current-ROM image must still load, while
            // its old alignment must not be reused just because dimensions match.
            auto changed = rom.bytes();
            const auto key = u32(changed, 0x1e512a6 + 5 * 4);
            const auto member = LhaMember::read(changed, key + 0x594284);
            auto decoded = member.decode(rom);
            decoded[8] ^= 1;
            const auto encoded = member.rebuild(rom, decoded);
            const auto at = changed.size();
            changed.append(QByteArray(4, 0));
            changed.append(encoded);
            changed.append(char(0));
            while (changed.size() % 4)
                changed.append(char(0));
            put32(changed, at, quint32(encoded.size() + 1));
            put32(changed, 0x1e512a6 + 5 * 4, quint32(at - 0x594280));
            const auto replaced = ScenarioMap::load(Rom::fromBytes(changed), 1);
            QVERIFY(replaced.image);
            QVERIFY(!replaced.imageBounds);
            QVERIFY(!replaced.imageGridBounds());
            QCOMPARE(replaced.bounds, first.bounds);
            QCOMPARE(replaced.points, first.points);
            QVERIFY(replaced.image->rgba != first.image->rgba);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void coordinateEncoding() {
        ScenarioMap map;
        map.bounds = QRectF(-10, -20, 32, 64);
        map.points = {{2, 3}, {4, 5}};
        QVERIFY(!map.imageGridBounds());
        map.imageBounds = QRectF(-14, -12, 48, 40);
        QCOMPARE(map.imageGridBounds().value(), QRectF(-32, 32, 384, 160));
        QCOMPARE(map.world(128, 128), QPointF(6, 12));
        QCOMPARE(map.placement(1, 0).value(), QPointF(2, 3));
        QVERIFY(!map.placement(0, 0));
        QVERIFY(!map.placement(3, 0));
        for (int x = 0; x < 256; ++x)
            for (int z = 0; z < 256; ++z) {
                QCOMPARE(map.bytes(map.world(x, z), false), QPoint(x, z));
                QCOMPARE(map.bytes(map.world(x, z), true), QPoint(x, std::max(1, z)));
            }
        QCOMPARE(map.bytes({-1e20, 1e20}, false), QPoint(0, 255));
        QVERIFY_THROWS_EXCEPTION(Error, map.world(256, 0));
        QVERIFY_THROWS_EXCEPTION(Error, map.bytes({qQNaN(), 0}, false));
    }
    void capturedBoundsAndPlacements() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto references = qEnvironmentVariable("LORDLY_REFERENCE_DATA");
        if (path.isEmpty() || references.isEmpty())
            QSKIP("Private ROM and independent capture repository required");
        QTemporaryDir work;
        QProcess generator;
        const auto fixture = work.filePath("maps.json");
        generator.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("map-fixture.cjs"),
             references, fixture});
        QVERIFY(generator.waitForFinished(30000));
        const auto errors = generator.readAllStandardError();
        QVERIFY2(generator.exitCode() == 0, errors.constData());
        QFile file(fixture);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["rows"].toArray();
        QCOMPARE(rows.size(), 64);
        const auto rom = Rom::open(path);
        int captures = 0, sites = 0, images = 0, coordinates = 0;
        QStringList differences;
        QMap<int, int> coordinateDifferences;
        for (const auto &value : rows) {
            const auto row = value.toObject();
            const int key = row["key"].toInt();
            ScenarioMap map;
            try {
                map = ScenarioMap::load(rom, key);
            } catch (const std::exception &e) {
                QFAIL(qPrintable(QString("Key %1: %2").arg(key).arg(e.what())));
            }
            if (row.contains("bounds")) {
                const auto raw = QByteArray::fromHex(row["bounds"].toString().toLatin1());
                const auto f = [&](int at) { return double(std::bit_cast<float>(u32(raw, at))); };
                QCOMPARE(map.bounds, QRectF(QPointF(f(0), f(4)), QPointF(f(8), f(12))));
                ++captures;
            }
            for (const auto &v : row["sites"].toArray()) {
                const auto s = v.toObject();
                const auto p = map.placement(s["selector"].toInt(), 0);
                if (!p || std::abs(p->x() - s["x"].toDouble()) >= 0.00001 ||
                    std::abs(p->y() - s["z"].toDouble()) >= 0.00001) {
                    differences << QString("%1:%2").arg(key).arg(s["selector"].toInt());
                    QVERIFY(!map.placementNotice.isEmpty());
                }
                ++sites;
            }
            for (const auto &v : row["deployments"].toArray()) {
                const auto d = v.toObject(), placement = d["placement"].toObject();
                if (placement["mode"].toString() != "coordinate")
                    continue;
                const auto bytes = placement["coordinateBytes"].toArray();
                QCOMPARE(bytes.size(), 2);
                const auto p = map.world(bytes[0].toInt(), bytes[1].toInt());
                const auto expected = d["world"].toObject();
                if (std::abs(p.x() - expected["x"].toDouble()) >= .00001 ||
                    std::abs(p.y() - expected["z"].toDouble()) >= .00001)
                    ++coordinateDifferences[key];
                ++coordinates;
            }
            if (map.image) {
                QVERIFY(map.image->width > 0);
                QCOMPARE(map.image->rgba.size(), map.image->width * map.image->height * 4);
                ++images;
            }
        }
        qInfo() << "Compared" << captures << "post-load bounds and" << sites
                << "site positions; decoded" << images << "selected images";
        QStringList expectedDifferences;
        for (int key : {10, 22, 51, 54, 64}) {
            if (key == 54) {
                expectedDifferences << "54:2";
                continue;
            }
            const int count = key == 51 ? 6 : key == 64 ? 7 : 8;
            for (int selector = 1; selector <= count; ++selector)
                expectedDifferences << QString("%1:%2").arg(key).arg(selector);
        }
        QCOMPARE(differences, expectedDifferences);
        // Group-adjusted positions for 20/21 and zero placeholders for alternate
        // overlay 54 are not initial byte-coordinate positions in this reference.
        const QMap<int, int> expectedCoordinateDifferences{{20, 4}, {21, 4}, {54, 6}};
        QCOMPARE(coordinateDifferences, expectedCoordinateDifferences);
        QCOMPARE(coordinates, 436);
        QCOMPARE(captures, 64);
        QVERIFY(sites >= 470);
        auto bad = rom.bytes();
        put32(bad, 0x262610c, 0xffffffff);
        QVERIFY_THROWS_EXCEPTION(Error, ScenarioMap::load(Rom::fromBytes(bad), 1));
        bad = rom.bytes();
        put8(bad, 0x2626110, 0x84);
        QVERIFY_THROWS_EXCEPTION(Error, ScenarioMap::load(Rom::fromBytes(bad), 1));
        bad = rom.bytes();
        put32(bad, 0x26261f3, 0x7fc00000);
        QVERIFY_THROWS_EXCEPTION(Error, ScenarioMap::load(Rom::fromBytes(bad), 1));
        bad = rom.bytes();
        put32(bad, 0x1e512a6 + 5 * 4, 0x10000000);
        QVERIFY_THROWS_EXCEPTION(Error, ScenarioMap::load(Rom::fromBytes(bad), 1));
    }
};
QTEST_GUILESS_MAIN(MapTests)
#include "map_tests.moc"
