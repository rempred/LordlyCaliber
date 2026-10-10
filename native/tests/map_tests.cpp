#include "engine/archive.h"
#include "engine/battlefield.h"
#include "engine/map_edits.h"
#include "engine/project.h"
#include "engine/scenario_map.h"
#include "engine/scenario_world.h"
#include "game/scenarios/site-count.h"
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
    void battlefieldEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto edit = MapEdits::load(rom);
            QCOMPARE(edit.rebuild(rom), rom.bytes());
            const auto &source = edit.source(1);
            QVERIFY(edit.canMoveSite(1, 0));
            const auto originalPoints = source.map.points;
            source.field.terrainTexture(1, source.terrain[0].data, source.map.bounds)
                .scaled(256, 256)
                .save(QCoreApplication::applicationDirPath() + "/road-tile.png");
            edit.paint(1, {{105, 120}, {125, 130}}, 1, 2);
            QCOMPARE(u8(edit.fineTerrain(1), 120 * 256 + 105), quint8(1));
            const auto destination = originalPoints[0] + QPointF(2, 1);
            edit.moveSite(1, 0, destination);
            QCOMPARE(edit.points(1)[0], destination);
            const auto model = edit.model(1);
            const auto rebuiltField = Battlefield::load(rom, 1, {}, model);
            const auto beforeTriangles = source.field.triangles(),
                       afterTriangles = rebuiltField.triangles();
            const int movedGroup = source.field.siteGroups()[1];
            int movedVertices = 0;
            for (int t = 0; t < beforeTriangles.size(); ++t)
                for (int v = 0; v < 3; ++v) {
                    const auto delta = afterTriangles[t].points[v] - beforeTriangles[t].points[v];
                    if (beforeTriangles[t].group == movedGroup) {
                        QVERIFY(std::abs(delta.x() - 2) < .0001);
                        QVERIFY(std::abs(delta.z() - 1) < .0001);
                        ++movedVertices;
                    } else
                        QVERIFY(delta.length() < .0001);
                }
            QVERIFY(movedVertices > 100);
            const auto rendered = rebuiltField.render(source.map.bounds, 768);
            QVERIFY(rendered != source.field.render(source.map.bounds, 768));
            QVERIFY(
                rendered.save(QCoreApplication::applicationDirPath() + "/painted-battlefield.png"));
            auto project = Project::fromRom(rom);
            project.setMapEdits(edit.json());
            QTemporaryDir folder;
            project.saveFolder(folder.filePath("project"));
            auto reopened = Project::fromRom(rom);
            reopened.applyManifest(Project::readProject(folder.filePath("project")));
            QVERIFY(reopened.mapEdits());
            QCOMPARE(reopened.mapEdits()->json(), edit.json());
            const auto exported = Rom::fromBytes(edit.rebuild(rom));
            auto loaded = MapEdits::load(exported);
            const auto &newSource = loaded.source(1);
            QCOMPARE(newSource.terrain[0].data, edit.fineTerrain(1));
            QCOMPARE(u8(newSource.terrain[1].data, (120 / 4) * 64 + 105 / 4), quint8(1));
            QVERIFY(QLineF(newSource.map.points[0], destination).length() < .00001);
            QCOMPARE(newSource.geometry[1].data, source.geometry[1].data);
            QCOMPARE(newSource.geometry[0].data, model);
            QCOMPARE(newSource.map.image->rgba, source.map.image->rgba);
            QCOMPARE(loaded.source(2).geometry[0].data, edit.source(2).geometry[0].data);
            QCOMPARE(loaded.source(2).terrain[0].data, edit.source(2).terrain[0].data);
            QCOMPARE(loaded.rebuild(exported), exported.bytes());
            edit.restore(1);
            QVERIFY(!edit.modified());
            QCOMPARE(edit.rebuild(rom), rom.bytes());
            auto invalid = project.mapEdits()->json();
            auto maps = invalid["maps"].toObject();
            auto row = maps["9"].toObject();
            row["paint"] = "broken";
            maps["9"] = row;
            invalid["maps"] = maps;
            QVERIFY_THROWS_EXCEPTION(Error, edit.apply(invalid));
            QVERIFY(!edit.modified());
            qInfo() << "Road geometry, fine/coarse terrain, town geometry and placement, project "
                       "persistence, untouched resources and no-edit export verified.";
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void battlefieldEditingCoverage() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto edits = MapEdits::load(rom);
            const auto world = ScenarioWorld::load(rom);
            QSet<int> visited;
            int movedSites = 0;
            for (int key = 1; key <= 64; ++key) {
                if (key == 54)
                    continue;
                const int model = u8(rom.bytes(), 0x654a0 + key * 9 + 3);
                if (model < 4 || visited.contains(model))
                    continue;
                visited.insert(model);
                const auto &source = edits.source(key);
                const auto before = source.field.triangles();
                for (int row = 0; row < world.siteCount(key); ++row) {
                    if (!edits.canMoveSite(key, row)) {
                        qInfo() << "Connected/special site" << key << row + 1;
                        continue;
                    }
                    const auto delta = QPointF(.25, .125);
                    edits.moveSite(key, row, source.map.points[row] + delta);
                    const auto after =
                        Battlefield::load(rom, key, {}, edits.model(key)).triangles();
                    QCOMPARE(after.size(), before.size());
                    const int group = source.field.siteGroups()[row + 1];
                    int count = 0;
                    for (int t = 0; t < before.size(); ++t)
                        for (int v = 0; v < 3; ++v) {
                            const auto shift = after[t].points[v] - before[t].points[v];
                            if (before[t].group == group) {
                                QVERIFY2(
                                    std::abs(shift.x() - .25) < .0001 &&
                                        std::abs(shift.z() - .125) < .0001,
                                    qPrintable(QString("Incomplete building move: key %1 site %2")
                                                   .arg(key)
                                                   .arg(row + 1)));
                                ++count;
                            } else
                                QVERIFY2(
                                    shift.length() < .0001,
                                    qPrintable(QString("Unrelated geometry moved: key %1 site %2")
                                                   .arg(key)
                                                   .arg(row + 1)));
                        }
                    QVERIFY(count > 0);
                    ++movedSites;
                    edits.restore(key);
                }
            }
            QVERIFY(movedSites > 200);
            for (int type = 1; type <= 14; ++type) {
                edits.paint(1, {{120, 140}, {130, 140}}, type, 3);
                const auto field = Battlefield::load(rom, 1, {}, edits.model(1));
                QVERIFY(field.triangleCount() > edits.source(1).field.triangleCount());
                edits.restore(1);
            }
            edits.paint(3, {{120, 140}}, 1, 3);
            QCOMPARE(edits.fineTerrain(3), edits.fineTerrain(11));
            QCOMPARE(edits.model(3), edits.model(11));
            const auto moved = edits.source(3).map.points[0] + QPointF(.25, .125);
            edits.moveSite(11, 0, moved);
            QCOMPARE(edits.points(3)[0], moved);
            edits.restore(3);
            QVERIFY(!edits.modified());
            const auto &fort = edits.source(47);
            int gate = -1;
            for (int cell = 0; cell < 65536; ++cell)
                if (u8(fort.terrain[0].data, cell) >= 15) {
                    gate = cell;
                    break;
                }
            QVERIFY(gate >= 0);
            edits.paint(47, {{double(gate % 256), double(gate / 256)}}, 3, 32);
            const auto plans = edits.plan();
            QCOMPARE(plans.size(), 1);
            const auto terrain = fieldMembers(plans[0].terrain);
            for (int n = 0; n < terrain.size(); ++n)
                for (int i = 0; i < terrain[n].data.size(); ++i)
                    if (u8(fort.terrain[n].data, i) >= 15)
                        QCOMPARE(terrain[n].data[i], fort.terrain[n].data[i]);
            QCOMPARE(edits.points(47), fort.map.points);
            edits.restore(47);
            QVERIFY(!edits.modified());
            qInfo() << "Verified complete independent movement for" << movedSites << "sites,"
                    << visited.size() << "maps, 14 brush materials, and shared map edits.";
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void battlefieldRendering() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        QStringList errors;
        for (int key = 1; key <= 64; ++key) {
            if (key == 54)
                continue; // Its alternate scenario loader is unsupported.
            try {
                const auto field = Battlefield::load(rom, key);
                QVERIFY(field.triangleCount() > 0);
                const auto map = ScenarioMap::load(rom, key);
                const auto image = field.render(map.bounds, 512);
                QVERIFY(!image.isNull());
                QSet<QRgb> colors;
                for (int y = 0; y < image.height(); y += 8)
                    for (int x = 0; x < image.width(); x += 8)
                        colors.insert(image.pixel(x, y));
                QVERIFY2(colors.size() > 50, qPrintable(QString("Empty or flat map %1").arg(key)));
                const auto output = qEnvironmentVariable("LORDLY_MAP_CAPTURE");
                if (!output.isEmpty()) {
                    QDir().mkpath(output);
                    image.save(output + QString("/field-%1.png").arg(key));
                }
                qInfo() << key << field.triangleCount() << field.buildingCount() << field.notices();
            } catch (const Error &e) {
                errors << QString("Key %1: %2").arg(key).arg(e.what());
            }
        }
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join('\n')));
        const auto bounds = ScenarioMap::load(rom, 1).bounds;
        QCOMPARE(Battlefield::load(rom, 1).buildingCount(), 8);
        const auto ally = Battlefield::load(rom, 1, QVector<quint16>(16, 2)).render(bounds, 768);
        const auto enemy = Battlefield::load(rom, 1, QVector<quint16>(16, 4)).render(bounds, 768);
        QVERIFY(ally != enemy);
        auto changed = rom.bytes();
        // Swap the field-model selection for Tenne with Volmus. This changes the
        // actual map resource, without touching the independent briefing image.
        put32(changed, 0x65710 + 9 * 12, u32(changed, 0x65710 + 7 * 12));
        const auto modified = Rom::fromBytes(changed);
        QVERIFY(Battlefield::load(modified, 1).resourceKey() !=
                Battlefield::load(rom, 1).resourceKey());
        QCOMPARE(ScenarioMap::load(modified, 1).image->rgba, ScenarioMap::load(rom, 1).image->rgba);
        put32(changed, 0x65710 + 9 * 12, 0xffffffff);
        QVERIFY_THROWS_EXCEPTION(Error, Battlefield::load(Rom::fromBytes(changed), 1));
    }
    void initialSiteCounts() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QJsonArray rows;
            for (int key = 0; key < 256; ++key)
                for (quint32 count : {0u, 1u, 6u, 13u, 255u, 0xffffffffu})
                    rows << QJsonObject{
                        {"key", key},
                        {"points", double(count)},
                        {"expected", double(lordly_initial_site_count(quint8(key), count))}};
            const auto counts = ScenarioMap::initialSiteCounts(rom);
            const auto world = ScenarioWorld::load(rom);
            int mismatchedHeaders = 0;
            for (int key = 1; key <= 64; ++key) {
                const int points = ScenarioMap::load(rom, key).points.size();
                rows << QJsonObject{
                    {"key", key}, {"points", points}, {"expected", counts.value(key)}};
                const auto sites = world.scenario(key).sites;
                QCOMPARE(world.siteCount(key), sites.isEmpty() ? 0 : counts.value(key));
                if (!sites.isEmpty() && counts.value(key) != u8(sites, 0)) {
                    QCOMPARE(key, 64);
                    QCOMPARE(counts.value(key), 6);
                    QCOMPARE(u8(sites, 0), quint8(1));
                    QCOMPARE(sites.size(), 25);
                    ++mismatchedHeaders;
                }
            }
            QCOMPARE(mismatchedHeaders, 1);
            QTemporaryDir work;
            const auto fixture = work.filePath("counts.json");
            QFile file(fixture);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QJsonDocument(QJsonObject{{"rom", path}, {"rows", rows}}).toJson());
            file.close();
            QProcess reference;
            reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("site-count-mips.cjs"),
                                     fixture});
            QVERIFY(reference.waitForFinished(30000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            qInfo().noquote() << reference.readAllStandardOutput();
            auto bad = rom.bytes();
            bad[0x127864] ^= 1;
            QVERIFY_THROWS_EXCEPTION(Error, ScenarioWorld::load(Rom::fromBytes(bad)));
            bad = rom.bytes();
            put8(bad, 0x654a0 + 64 * 9 + 7, 255);
            QVERIFY_THROWS_EXCEPTION(Error, ScenarioMap::initialSiteCounts(Rom::fromBytes(bad)));
            const auto resources = WorldResources::load(rom);
            const auto shortSites = resources.rebuild(rom, resources.towns(rom), {},
                                                      {{64, world.scenario(64).sites.first(5)}});
            QVERIFY_THROWS_EXCEPTION(Error, ScenarioWorld::load(Rom::fromBytes(shortSites)));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
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
