#include "engine/archive.h"
#include "engine/battlefield.h"
#include "engine/color_combiner.h"
#include "engine/field_borders.h"
#include "engine/field_brush.h"
#include "engine/map_edits.h"
#include "engine/project.h"
#include "engine/scenario_map.h"
#include "engine/scenario_world.h"
#include "engine/scene_images.h"
#include "game/scenarios/site-count.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineF>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <bit>
#include <cmath>
using namespace lordly;
namespace {
double surfaceHeight(const QVector<FieldTriangle> &triangles, QPointF p) {
    double result = -1e9;
    for (const auto &t : triangles) {
        const auto &q = t.points;
        const double x = q[1].x() - q[0].x(), z = q[1].z() - q[0].z(), xx = q[2].x() - q[0].x(),
                     zz = q[2].z() - q[0].z(), px = p.x() - q[0].x(), pz = p.y() - q[0].z(),
                     det = x * zz - xx * z;
        if (std::abs(det) < 1e-8)
            continue;
        const double a = (px * zz - xx * pz) / det, b = (x * pz - px * z) / det;
        if (a >= 0 && b >= 0 && a + b <= 1)
            result =
                std::max(result, q[0].y() + a * (q[1].y() - q[0].y()) + b * (q[2].y() - q[0].y()));
    }
    return result;
}
double movementHeight(const QByteArray &data, int x, int z) {
    int at = 28 + u16(data, 28 + z * 2) * 2;
    for (int cell = 0; cell < u16(data, 0);) {
        const int value = u16(data, at);
        at += 2;
        const int count = value >> 8 == 255 ? value & 255 : 1;
        const int h = value >> 8 == 255 ? u16(data, at) : value;
        if (value >> 8 == 255)
            at += 2;
        if (x < cell + count) {
            const double lo = std::bit_cast<float>(u32(data, 12)),
                         hi = std::bit_cast<float>(u32(data, 16));
            return lo + (hi - lo) * h / 65279.;
        }
        cell += count;
    }
    return -1e9;
}
} // namespace
class MapTests : public QObject {
    Q_OBJECT
  private slots:
    void mountainSaddleShading() {
        QByteArray mask(65536, char(-1));
        for (int z = 40; z <= 80; ++z)
            for (int x = 30; x <= 100; ++x)
                mask[z * 256 + x] = 7;
        const QVector<QVector3D> summits{{40, 2, 60}, {65, .2f, 60}, {90, 2, 60}};
        for (int type : {7, 14}) {
            auto region = mask;
            region.replace(char(7), char(type));
            const FieldBrushShape shape(region, type, {0, 0, 256, 256}, {}, summits);
            QVERIFY(std::abs(shape.profile({65, 60}) - .2) < .01);
            QVERIFY(shape.profile({55, 60}) > .5);
            QVERIFY(shape.profile({75, 60}) > .5);
            QVERIFY(shape.profile({65, 64}) < shape.profile({65, 60}));
            QVERIFY(shape.mountainUv({65, 60}).y() < .15);
            QVERIFY(shape.mountainUv({40, 60}).y() > .95);
            QVERIFY(shape.mountainUv({90, 60}).y() > .95);
            const FieldBrushShape flat(region, type, {0, 0, 256, 256}, {},
                                       {{40, 0, 60}, {90, 0, 60}});
            QCOMPARE(flat.mountainUv({65, 60}).y(), 0.);
        }
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   supplied = qEnvironmentVariable("LORDLY_SADDLE_PROJECT");
        if (path.isEmpty() || supplied.isEmpty())
            QSKIP("Private key-1 project required for image/export check");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        project.applyManifest(Project::readProject(supplied));
        const auto &edits = *project.mapEdits();
        const auto &source = edits.source(1);
        const auto b = source.map.bounds;
        const auto model = edits.model(1);
        QVERIFY(model.size() < 512 * 1024);
        const auto field = Battlefield::load(rom, 1, {}, model);
        auto image = edits.rebuild(rom);
        Rom::updateChecksum(image);
        const auto exported = Rom::fromBytes(image);
        const auto reopened = MapEdits::load(exported);
        QCOMPARE(reopened.source(1).geometry[0].data, model);
        QCOMPARE(reopened.source(1).geometry[1].data, editFieldHeight(source, edits.changes(1)));
        const auto exportField = Battlefield::load(exported, 1);
        int checked = 0;
        for (const auto &peak : edits.mountainPeaks(1))
            if (peak.addedId && peak.height < .3) {
                const QPointF world(b.left() + peak.position.x() * b.width() / 256,
                                    b.top() + peak.position.y() * b.height() / 256);
                const auto h = surfaceHeight(field.surface(), world);
                QVERIFY(std::abs(h - peak.height) < .06);
                const QRectF crop(world - QPointF(b.width() * 25 / 256, b.height() * 25 / 256),
                                  QSizeF(b.width() * 50 / 256, b.height() * 50 / 256));
                const auto preview = field.render(crop, 768);
                QCOMPARE(exportField.render(crop, 768), preview);
                double grey = 0;
                for (int y = 381; y < 388; ++y)
                    for (int x = 381; x < 388; ++x)
                        grey += qGray(preview.pixel(x, y));
                grey /= 49;
                qInfo() << "Low saddle" << peak.height << "mesh" << h << "grey" << grey;
                // The reported bright saddles measured 159 and 168 in this crop;
                // both controls must now use the dark lower rock rows.
                QVERIFY(grey < 100);
                ++checked;
            }
        QCOMPARE(checked, 2);
        QVERIFY(reopened.source(1).map.image->rgba != source.map.image->rgba);
        QCOMPARE(reopened.source(2).geometry[0].data, edits.source(2).geometry[0].data);
    }
    void automaticMountainRidges() {
        QByteArray mask(65536, char(-1));
        for (int z = 30; z <= 90; ++z)
            for (int x = 25; x <= 105; ++x)
                mask[z * 256 + x] = 7;
        const QRectF bounds(0, 0, 256, 256);
        const QVector<QVector3D> summits{{40, 2, 60}, {90, 2, 60}};
        const FieldBrushShape joined(mask, 7, bounds, {}, summits);
        QVERIFY(joined.profile({65, 60}) > 1.5);
        QVERIFY(joined.profile({65, 60}) > joined.profile({65, 35}) + .5);
        for (auto p : joined.ridgeVertices()) {
            QVERIFY(joined.distance(p) > 0);
            QVERIFY(joined.profile(p) > 1.5);
        }
        MountainPeak moved{{40, 42}, 7, 2, 2, {40, 60}};
        const FieldBrushShape relocated(mask, 7, bounds, {}, summits, {moved});
        // Interior routing may keep the middle of a broad mountain near its
        // centre. Moving an endpoint must still lower its old site and raise
        // the new site, rather than merely changing texture coordinates.
        QVERIFY(relocated.profile({40, 60}) < joined.profile({40, 60}) - .5);
        QVERIFY(relocated.profile({40, 42}) > joined.profile({40, 42}) + .5);
        MountainPeak lowered{{40, 60}, 7, 0, 2, {40, 60}};
        const FieldBrushShape low(mask, 7, bounds, {}, summits, {lowered});
        QVERIFY(low.profile({65, 60}) < joined.profile({65, 60}) - .5);
        const FieldBrushShape branched(mask, 7, bounds, {},
                                       {{40, 2, 60}, {90, 2, 60}, {65, 3, 42}});
        for (auto p : branched.ridgeVertices()) {
            QVERIFY(branched.distance(p) > 0);
            QVERIFY(std::isfinite(branched.profile(p)));
            QVERIFY(branched.profile(p) >= 0 && branched.profile(p) <= 3.01);
        }
        // A deep inlet forces the connecting crest around the bottom of a U.
        for (int z = 30; z <= 75; ++z)
            for (int x = 50; x <= 80; ++x)
                mask[z * 256 + x] = char(-1);
        const FieldBrushShape bent(mask, 7, bounds, {}, summits);
        const auto ridge = bent.ridgeVertices();
        QVERIFY(!ridge.isEmpty());
        QVERIFY(std::any_of(ridge.begin(), ridge.end(), [](QPointF p) { return p.y() > 76; }));
        for (auto p : ridge)
            QVERIFY(bent.distance(p) > 0);
        // A road crossing the remaining neck disconnects the two mountains.
        for (int z = 76; z <= 90; ++z)
            for (int x = 63; x <= 67; ++x)
                mask[z * 256 + x] = 1;
        const FieldBrushShape cut(mask, 7, bounds, {}, summits);
        QVERIFY(cut.ridgeVertices().isEmpty());
        QCOMPARE(cut.profile({65, 83}), 0.);

        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required for export check");
        const auto rom = Rom::open(path);
        auto edit = MapEdits::load(rom);
        const auto peaks = edit.mountainPeaks(5);
        const auto found = std::find_if(peaks.begin(), peaks.end(), [](const auto &p) {
            return QLineF(p.position, {69.9732, 82.1529}).length() < .01;
        });
        QVERIFY(found != peaks.end());
        const auto peak = *found;
        const auto &s = edit.source(5);
        const auto b = s.map.bounds;
        auto world = [&](QPointF p) {
            return QPointF(b.left() + b.width() * p.x() / 256, b.top() + b.height() * p.y() / 256);
        };
        edit.setPeakHeight(5, peak, peak.height + .01);
        const auto model = edit.model(5);
        QVERIFY(model.size() < 512 * 1024);
        const auto field = Battlefield::load(rom, 5, {}, model);
        const QPointF saddle = world({73.4485, 106.824});
        const double nativeHeight = surfaceHeight(s.field.surface(), saddle),
                     rebuiltHeight = surfaceHeight(field.surface(), saddle);
        qInfo() << "Native ridge saddle" << nativeHeight << "rebuilt" << rebuiltHeight
                << "model bytes" << model.size();
        QVERIFY(nativeHeight > .9);
        QVERIFY(rebuiltHeight > .75);
        auto reopened = MapEdits::load(Rom::fromBytes(edit.rebuild(rom)));
        QCOMPARE(reopened.source(5).geometry[0].data, model);
        QCOMPARE(reopened.source(5).terrain[0].data, s.terrain[0].data);
        QCOMPARE(reopened.source(4).geometry[0].data, edit.source(4).geometry[0].data);
        const auto &grid = reopened.source(5).geometry[1].data;
        auto f = [&](int at) { return double(std::bit_cast<float>(u32(grid, at))); };
        const int x = int(std::round((saddle.x() - f(4)) / (f(8) - f(4)) * (u16(grid, 0) - 1))),
                  z = int(std::round((saddle.y() - f(20)) / (f(24) - f(20)) * (u16(grid, 2) - 1)));
        QVERIFY(movementHeight(grid, x, z) > .7);
        auto copy = MapEdits::load(rom);
        copy.apply(edit.json());
        QCOMPARE(copy.model(5), model);
        edit.removePeak(5, peak);
        QVERIFY(surfaceHeight(Battlefield::load(rom, 5, {}, edit.model(5)).surface(), saddle) <
                rebuiltHeight - .2);
    }
    void mountainPeakLayout() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto native = MapEdits::load(rom);
        const auto nativePeaks = native.mountainPeaks(5);
        const auto found = std::find_if(nativePeaks.begin(), nativePeaks.end(), [](auto p) {
            return QRectF(116, 116, 92, 116).contains(p.position) && p.height > 1.5;
        });
        QVERIFY(found != nativePeaks.end());
        const auto peak = *found;
        const auto nativeBounds = native.source(5).map.bounds;
        auto nativeWorld = [&](QPointF p) {
            return QPointF(nativeBounds.left() + p.x() * nativeBounds.width() / 256,
                           nativeBounds.top() + p.y() * nativeBounds.height() / 256);
        };
        native.setPeakHeight(5, peak, 3);
        const auto beforeMove = Battlefield::load(rom, 5, {}, native.model(5)).triangles();
        const auto target = peak.position + QPointF(5, 0);
        native.movePeak(5, peak, target);
        const auto afterMove = Battlefield::load(rom, 5, {}, native.model(5)).triangles();
        QVERIFY(surfaceHeight(beforeMove, nativeWorld(peak.position)) >
                surfaceHeight(afterMove, nativeWorld(peak.position)) + .5);
        QVERIFY(surfaceHeight(afterMove, nativeWorld(target)) > 2.7);
        native.removePeak(5, peak);
        const auto afterRemove = Battlefield::load(rom, 5, {}, native.model(5)).triangles();
        QVERIFY(surfaceHeight(afterMove, nativeWorld(target)) >
                surfaceHeight(afterRemove, nativeWorld(target)) + .5);
        auto nativeCopy = MapEdits::load(rom);
        nativeCopy.apply(native.json());
        QCOMPARE(nativeCopy.model(5), native.model(5));
        for (const auto &p : nativeCopy.mountainPeaks(5))
            QVERIFY(!p.sameIdentity(peak));
        for (int type : {7, 14}) {
            auto edit = MapEdits::load(rom);
            const auto &source = edit.source(5);
            QPoint center(-1, -1);
            for (int y = 20; y < 236 && center.x() < 0; y += 4)
                for (int x = 20; x < 236 && center.x() < 0; x += 4) {
                    bool plain = true;
                    for (int dy = -9; dy <= 9 && plain; ++dy)
                        for (int dx = -9; dx <= 9; ++dx)
                            plain &= u8(source.visualTerrain, (y + dy) * 256 + x + dx) == 3;
                    if (plain)
                        center = {x, y};
                }
            QVERIFY(center.x() >= 0);
            edit.paint(5, {center}, type, 7);
            const auto painted = edit.json();
            const auto paintedModel = edit.model(5);
            const auto originals = edit.mountainPeaks(5);
            QVector<MountainPeak> local;
            for (const auto &p : originals)
                if (QLineF(p.position, center).length() < 8)
                    local << p;
            QVERIFY(!local.isEmpty());
            edit.movePeak(5, local[0], local[0].position + QPointF(2, 0));
            auto roundtrip = MapEdits::load(rom);
            roundtrip.apply(edit.json());
            QCOMPARE(roundtrip.model(5), edit.model(5));
            edit.restorePeaks(5);
            QCOMPARE(edit.json(), painted);
            QCOMPARE(edit.model(5), paintedModel);
            for (const auto &p : local)
                edit.removePeak(5, p);
            const auto flat = edit.model(5);
            const auto remaining = edit.mountainPeaks(5);
            for (const auto &p : remaining)
                QVERIFY(QLineF(p.position, center).length() >= 8);
            const auto added = edit.addPeak(5, center + QPointF(-2, 0), 3);
            QVERIFY(added.addedId > 0);
            const auto before = edit.model(5);
            const auto beforeHeight = editFieldHeight(source, edit.changes(5));
            const QPointF destination = center + QPointF(2, 0);
            edit.movePeak(5, added, destination);
            const auto after = edit.model(5);
            const auto b = source.map.bounds;
            auto world = [&](QPointF p) {
                return QPointF(b.left() + p.x() * b.width() / 256,
                               b.top() + p.y() * b.height() / 256);
            };
            const auto oldTriangles = Battlefield::load(rom, 5, {}, before).triangles();
            const auto newField = Battlefield::load(rom, 5, {}, after);
            const auto newTriangles = newField.triangles();
            const double oldTop = surfaceHeight(oldTriangles, world(added.position)),
                         oldNow = surfaceHeight(newTriangles, world(added.position)),
                         newBefore = surfaceHeight(oldTriangles, world(destination)),
                         newTop = surfaceHeight(newTriangles, world(destination));
            qInfo() << "Moved peak" << type << "old location" << oldTop << oldNow << "new location"
                    << newBefore << newTop;
            QVERIFY(oldTop > oldNow + .5);
            QVERIFY(newTop > newBefore + .5);
            auto heightAtGrid = [&](const QByteArray &data, QPointF p) {
                const auto wp = world(p);
                auto f = [&](int at) { return double(std::bit_cast<float>(u32(data, at))); };
                const int x = int(std::round((wp.x() - f(4)) / (f(8) - f(4)) * (u16(data, 0) - 1))),
                          z = int(
                              std::round((wp.y() - f(20)) / (f(24) - f(20)) * (u16(data, 2) - 1)));
                return movementHeight(data, x, z);
            };
            const auto afterHeight = editFieldHeight(source, edit.changes(5));
            QVERIFY(heightAtGrid(beforeHeight, added.position) >
                    heightAtGrid(afterHeight, added.position) + .15);
            QVERIFY(heightAtGrid(afterHeight, destination) >
                    heightAtGrid(beforeHeight, destination) + .15);
            const auto second = edit.addPeak(5, center + QPointF(0, 3), 1.2);
            const auto saved = edit.json();
            QVERIFY_THROWS_EXCEPTION(Error, edit.movePeak(5, added, {-2, 0}));
            QVERIFY_THROWS_EXCEPTION(Error, edit.addPeak(5, destination));
            QCOMPARE(edit.json(), saved);
            roundtrip.apply(saved);
            QCOMPARE(roundtrip.model(5), edit.model(5));
            auto bad = saved;
            auto maps = bad["maps"].toObject();
            auto row = maps[QString::number(source.modelId)].toObject();
            auto entries = row["peaks"].toArray();
            entries << entries.last();
            row["peaks"] = entries;
            maps[QString::number(source.modelId)] = row;
            bad["maps"] = maps;
            QVERIFY_THROWS_EXCEPTION(Error, edit.apply(bad));
            QCOMPARE(edit.json(), saved);
            const auto exported = MapEdits::load(Rom::fromBytes(edit.rebuild(rom)));
            QCOMPARE(exported.model(5), edit.model(5));
            QCOMPARE(exported.source(5).geometry[1].data, editFieldHeight(source, edit.changes(5)));
            QCOMPARE(exported.fineTerrain(5), edit.fineTerrain(5));
            QVERIFY(exported.source(5).map.image->rgba != source.map.image->rgba);
            QCOMPARE(edit.model(1), edit.source(1).geometry[0].data);
            edit.removePeak(5, second);
            edit.removePeak(5, added); // Stable ID remains valid after moving.
            QCOMPARE(edit.model(5), flat);
            QVERIFY(surfaceHeight(Battlefield::load(rom, 5, {}, flat).triangles(),
                                  world(destination)) < .2);
            roundtrip.apply(edit.json());
            QCOMPARE(roundtrip.model(5), flat); // Removed defaults must not reappear.
            edit.restorePeaks(5);
            QCOMPARE(edit.json(), painted);
            const auto erased = edit.addPeak(5, center + QPointF(2, 0));
            edit.paint(5, {erased.position}, 3, 1);
            for (const auto &p : edit.mountainPeaks(5))
                QVERIFY(!p.sameIdentity(erased));
            roundtrip.apply(edit.json());
            QCOMPARE(roundtrip.model(5), edit.model(5));
        }
    }
    void mountainPeakHeights() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto edit = MapEdits::load(rom);
        const auto original = edit.json();
        const auto &source = edit.source(5);
        const auto peaks = edit.mountainPeaks(5);
        QVERIFY(peaks.size() > 1);
        const auto selected = std::find_if(peaks.begin(), peaks.end(), [](auto p) {
            // Inside the movement-height grid, on Joe's central native mountain.
            return QRectF(116, 116, 92, 116).contains(p.position) && p.height > 1.5;
        });
        QVERIFY(selected != peaks.end());
        const auto peak = *selected;
        const auto b = source.map.bounds;
        const QPointF point(b.left() + b.width() * peak.position.x() / 256,
                            b.top() + b.height() * peak.position.y() / 256);
        edit.setPeakHeight(5, peak, .5);
        const auto low = edit.model(5);
        const auto lowGrid = editFieldHeight(source, edit.changes(5));
        const double lowY = surfaceHeight(Battlefield::load(rom, 5, {}, low).triangles(), point);
        edit.setPeakHeight(5, peak, 3.5);
        const auto high = edit.model(5);
        const double highY = surfaceHeight(Battlefield::load(rom, 5, {}, high).triangles(), point);
        qInfo() << "Peak" << peak.position << "low/high Y" << lowY << highY;
        QVERIFY(highY > lowY + 2.7);
        QVERIFY(high.size() <= 524288);
        const auto changedPeaks = edit.mountainPeaks(5);
        QCOMPARE(changedPeaks.size(), peaks.size());
        for (int i = 0; i < peaks.size(); ++i) {
            QCOMPARE(changedPeaks[i].position, peaks[i].position);
            QCOMPARE(changedPeaks[i].height,
                     peaks[i].position == peak.position ? 3.5 : peaks[i].height);
        }
        QCOMPARE(edit.fineTerrain(5), source.terrain[0].data);
        QCOMPARE(edit.model(1), edit.source(1).geometry[0].data);
        const auto saved = edit.json();
        auto restored = MapEdits::load(rom);
        restored.apply(saved);
        QCOMPARE(restored.model(5), high);
        QVERIFY_THROWS_EXCEPTION(Error, edit.setPeakHeight(5, peak, -1));
        QVERIFY_THROWS_EXCEPTION(Error, edit.setPeakHeight(5, peak, 8.01));
        QVERIFY_THROWS_EXCEPTION(
            Error, edit.setPeakHeight(5, peak, std::numeric_limits<double>::quiet_NaN()));
        auto invalid = saved;
        auto maps = invalid["maps"].toObject();
        auto row = maps[QString::number(source.modelId)].toObject();
        row["peaks"] = QJsonArray{QJsonObject{{"x", -1}, {"z", 2}, {"terrain", 7}, {"height", 2}}};
        maps[QString::number(source.modelId)] = row;
        invalid["maps"] = maps;
        QVERIFY_THROWS_EXCEPTION(Error, edit.apply(invalid));
        QCOMPARE(edit.json(), saved);
        const auto exported = Rom::fromBytes(edit.rebuild(rom));
        const auto reopened = MapEdits::load(exported);
        QCOMPARE(reopened.source(5).geometry[0].data, high);
        QVERIFY(reopened.source(5).map.image->rgba != source.map.image->rgba);
        const auto &grid = reopened.source(5).geometry[1].data;
        auto f = [&](int at) { return double(std::bit_cast<float>(u32(grid, at))); };
        const int heightX =
                      int(std::round((point.x() - f(4)) / (f(8) - f(4)) * (u16(grid, 0) - 1))),
                  heightZ =
                      int(std::round((point.y() - f(20)) / (f(24) - f(20)) * (u16(grid, 2) - 1)));
        qInfo() << "Movement height low/high" << movementHeight(lowGrid, heightX, heightZ)
                << movementHeight(grid, heightX, heightZ);
        QVERIFY(movementHeight(grid, heightX, heightZ) >
                movementHeight(lowGrid, heightX, heightZ) + .5);
        edit.setPeakHeight(5, peak, peak.defaultHeight);
        QCOMPARE(edit.json(), original);
        QCOMPARE(edit.model(5), source.geometry[0].data);
        edit.paint(5, {{161.5, 132.5}}, 7, 3); // Same native mountain extension as Joe's project.
        const auto painted = edit.json();
        const auto extended = edit.mountainPeaks(5);
        const auto inside = std::find_if(extended.begin(), extended.end(), [](auto p) {
            return QRectF(116, 116, 92, 116).contains(p.position);
        });
        QVERIFY(inside != extended.end());
        const auto paintedModel = edit.model(5);
        edit.setPeakHeight(5, *inside, 3);
        QVERIFY(edit.model(5) != paintedModel);
        edit.setPeakHeight(5, *inside, inside->defaultHeight);
        QCOMPARE(edit.json(), painted);
        // New ordinary and snowy mountains have independently adjustable generated summits.
        QPoint center(-1, -1);
        for (int y = 20; y < 236 && center.x() < 0; y += 4)
            for (int x = 20; x < 236 && center.x() < 0; x += 4) {
                bool plain = true;
                for (int dy = -9; dy <= 9 && plain; ++dy)
                    for (int dx = -9; dx <= 9; ++dx)
                        plain &= u8(source.visualTerrain, (y + dy) * 256 + x + dx) == 3;
                if (plain)
                    center = {x, y};
            }
        QVERIFY(center.x() >= 0);
        for (int type : {7, 14}) {
            edit.restore(5);
            edit.paint(5, {center}, type, 7);
            const auto added = edit.mountainPeaks(5);
            const auto found = std::find_if(added.begin(), added.end(), [&](auto p) {
                return p.terrain == type && QLineF(p.position, center).length() < 5;
            });
            QVERIFY(found != added.end());
            const auto p = *found;
            const auto before = edit.model(5);
            edit.setPeakHeight(5, p, 8);
            QVERIFY(edit.model(5) != before);
            edit.setPeakHeight(5, p, 0);
            QVERIFY(edit.model(5) != before);
            edit.paint(5, {center}, 255, 9);
            QVERIFY(edit.changes(5).peaks.isEmpty());
            QCOMPARE(edit.json(), original);
        }
    }
    void nativeMountainSlopes() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto edit = MapEdits::load(rom);
        const auto &source = edit.source(5);
        const auto b = source.map.bounds;
        QVector3D summit;
        float top = -1e9f;
        for (const auto &t : source.terrainFaces) {
            if (t.terrain != 7)
                continue;
            for (const auto &p : t.points) {
                const QPointF grid((p.x() - b.left()) * 256 / b.width(),
                                   (p.z() - b.top()) * 256 / b.height());
                if (QRectF(116, 116, 92, 116).contains(grid) && p.y() > top) {
                    summit = p;
                    top = p.y();
                }
            }
        }
        QVERIFY(top > .5f);
        // Joe's small addition at the side of this mountain must not relocate
        // its existing high summit to the centre of a new distance-field dome.
        edit.paint(5, {{161.5, 132.5}}, 7, 3);
        const FieldBorders borders(source, edit.changes(5));
        QVERIFY(std::any_of(
            source.terrainFaces.begin(), source.terrainFaces.end(), [&](const FieldTriangle &t) {
                return borders.removes(t) &&
                       std::find(t.points.begin(), t.points.end(), summit) != t.points.end();
            }));
        const auto model = edit.model(5);
        const auto field = Battlefield::load(rom, 5, {}, model);
        const double height = surfaceHeight(field.triangles(), {summit.x(), summit.z()});
        qInfo() << "Native summit" << summit << "rebuilt height" << height;
        QVERIFY2(std::abs(height - summit.y()) < .35,
                 "A small border edit must retain the native summit");
        const auto crop = QRectF(b.left() + b.width() * 116 / 256, b.top() + b.height() * 116 / 256,
                                 b.width() * 92 / 256, b.height() * 116 / 256);
        const auto picture = field.render(crop, 768);
        QVERIFY(
            picture.save(QCoreApplication::applicationDirPath() + "/native-mountain-slopes.png"));
        const auto exported = Rom::fromBytes(edit.rebuild(rom));
        QCOMPARE(Battlefield::load(exported, 5).render(crop, 768), picture);
        const auto reopened = MapEdits::load(exported);
        const auto &rebuilt = reopened.source(5);
        const auto &grid = rebuilt.geometry[1].data;
        auto f = [&](int at) { return double(std::bit_cast<float>(u32(grid, at))); };
        const int x = int(std::round((summit.x() - f(4)) / (f(8) - f(4)) * (u16(grid, 0) - 1))),
                  z = int(std::round((summit.z() - f(20)) / (f(24) - f(20)) * (u16(grid, 2) - 1)));
        QVERIFY(movementHeight(grid, x, z) > summit.y() * .6);
        QVERIFY(rebuilt.map.image->rgba != source.map.image->rgba);
    }
    void raisedTerrainFoot() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        for (int key : {1, 5}) for (int type : {5, 7, 13, 14}) {
            auto edit = MapEdits::load(rom);
            const auto &s = edit.source(key);
            QPoint center(-1, -1);
            for (int y = 16; y < 240 && center.x() < 0; y += 4)
                for (int x = 16; x < 240 && center.x() < 0; x += 4) {
                    bool plain = true;
                    for (int dy = -9; dy <= 9 && plain; ++dy)
                        for (int dx = -9; dx <= 9; ++dx)
                            if (u8(s.visualTerrain, (y + dy) * 256 + x + dx) != 3) {
                                plain = false; break;
                            }
                    if (plain) center = {x, y};
                }
            QVERIFY(center.x() >= 0);
            edit.paint(key, {center}, type, 7);
            const auto model = edit.model(key);
            const int geometry = int((u32(model, 48) + 15) & ~15u);
            constexpr int widths[] = {0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 3, 3, 1,
                                      1, 0, 1, 1, 1, 2, 2, 3, 1, 2, 2, 3, 2};
            quint32 last = 0;
            for (int at = int(u32(model, 0)); u32(model, at);) {
                const int op = int(u32(model, at));
                QVERIFY(op <= 25);
                if (op == 1) last = u32(model, at + 4);
                at += (1 + widths[op]) * 4;
            }
            QVERIFY(last);
            int vertices = 0;
            for (int at = geometry + int(last & 0xffffff); u32(model, at) != 0xdf000000; at += 8) {
                const auto a = u32(model, at), b = u32(model, at + 4);
                // Fog uses primitive coverage; clear maps use vertex coverage.
                // Neither may fade the dark foot into the ground underneath it.
                if (a >> 24 == 0xfa) QCOMPARE(b & 255, 255u);
                if (a >> 24 != 1) continue;
                const int count = (a >> 12) & 255, start = geometry + int(b & 0xffffff);
                for (int i = 0; i < count; ++i) QCOMPARE(u8(model, start + i * 16 + 15), 255);
                vertices += count;
            }
            QVERIFY(vertices > 0);
            const auto picture = Battlefield::load(rom, key, {}, model).render(s.map.bounds, 1024);
            const auto exported = Rom::fromBytes(edit.rebuild(rom));
            QCOMPARE(Battlefield::load(exported, key).render(s.map.bounds, 1024), picture);
        }
    }
    void terrainGroundPreservation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto edits = MapEdits::load(rom);
        const auto &s = edits.source(6);
        const QPoint center(40, 55);
        QCOMPARE(u8(s.terrain[0].data, center.y() * 256 + center.x()), 3);
        QCOMPARE(u8(s.visualTerrain, center.y() * 256 + center.x()), 3);
        const auto before = s.field.render(s.map.bounds, 1024);
        edits.paint(6, {center}, 5, 7);
        const FieldBorders borders(s, edits.changes(6));
        QVERIFY(borders.removed.isEmpty()); // New forest on open plain removes no native ground.
        const auto field = Battlefield::load(rom, 6, {}, edits.model(6));
        const auto after = field.render(s.map.bounds, 1024);
        QVERIFY(before != after);
        const QRect changed((center.x() - 10) * 4, (center.y() - 10) * 4, 80, 80);
        int changedOutside = 0;
        for (int y = 0; y < before.height(); ++y)
            for (int x = 0; x < before.width(); ++x)
                if (!changed.contains(x, y) && before.pixel(x, y) != after.pixel(x, y)) ++changedOutside;
        QCOMPARE(changedOutside, 0);
        const auto out = QCoreApplication::applicationDirPath();
        QVERIFY(before.save(out + "/forest-ground-before.png"));
        QVERIFY(after.save(out + "/forest-ground-after.png"));
        const auto rebuilt = Rom::fromBytes(edits.rebuild(rom));
        QCOMPARE(Battlefield::load(rebuilt, 6).render(s.map.bounds, 1024), after);
        edits.restore(6); QCOMPARE(edits.model(6), s.geometry[0].data);
        // The supplied mountain report is on key 5. Keep a close rendered/export
        // comparison of an authored ridge alongside the unchanged native terrain.
        const auto &mountain = edits.source(5);
        edits.paint(5, {{30, 70}, {38, 45}}, 7, 7);
        const auto painted = Battlefield::load(rom, 5, {}, edits.model(5));
        QVERIFY(painted.render(mountain.map.bounds, 1536).save(out + "/mountain-rim-after.png"));
        const auto ridgeRom = Rom::fromBytes(edits.rebuild(rom));
        QCOMPARE(Battlefield::load(ridgeRom, 5).render(mountain.map.bounds, 1024),
                 painted.render(mountain.map.bounds, 1024));
    }
    void cityPalette() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto edits = MapEdits::load(rom);
            const auto &source = edits.source(1);
            const auto assets = edits.cityAssets();
            QCOMPARE(assets.size(), 19);
            for (const auto &a : assets) {
                QVERIFY(!a.faces.isEmpty());
                QVERIFY(!a.materials.isEmpty());
                QVERIFY(a.bounds.isValid());
                QVERIFY(a.thumbnail.save(QCoreApplication::applicationDirPath() + "/city-" + a.id +
                                         ".png"));
                const auto p = source.map.bounds.center();
                edits.placeCity(1, a.id, p, 90);
                const auto field = Battlefield::load(rom, 1, {}, edits.model(1));
                const auto triangles = field.triangles();
                QCOMPARE(triangles.size(), source.field.triangleCount() + a.faces.size());
                // An independent extent check catches omitted or double-applied rotation/scale.
                double minX = 1e9, maxX = -1e9, minZ = 1e9, maxZ = -1e9;
                for (int i = source.field.triangleCount(); i < triangles.size(); ++i)
                    for (auto v : triangles[i].points) {
                        minX = std::min(minX, double(v.x()));
                        maxX = std::max(maxX, double(v.x()));
                        minZ = std::min(minZ, double(v.z()));
                        maxZ = std::max(maxZ, double(v.z()));
                    }
                QVERIFY(std::abs(maxX - minX - a.bounds.height()) < .04);
                QVERIFY(std::abs(maxZ - minZ - a.bounds.width()) < .04);
                edits.removeCity(1, 0);
                QVERIFY(!edits.modified());
            }
            edits.placeCity(1, "latium-house", source.map.world(100, 110));
            edits.placeCity(1, "romulus-tower", source.map.world(120, 110), 90);
            edits.placeCity(1, "talpaea-gatehouse", source.map.world(140, 110), 180);
            for (int code = 32; code <= 35; ++code) {
                const int y = 120 + (code - 32) * 10;
                edits.paint(1, {{100., double(y)}, {155., double(y)}}, code, 1.5);
                QCOMPARE(u8(edits.fineTerrain(1), y * 256 + 120), quint8(1));
            }
            const auto before = edits.json();
            QVERIFY_THROWS_EXCEPTION(Error, edits.moveCity(1, 0, source.map.bounds.topLeft(), 0));
            QCOMPARE(edits.json(), before);
            auto copy = MapEdits::load(rom);
            copy.apply(before);
            QCOMPARE(copy.json(), before);
            QCOMPARE(copy.model(1), edits.model(1));
            const auto field = Battlefield::load(rom, 1, {}, edits.model(1));
            QVERIFY(field.render(source.map.bounds, 1024)
                        .save(QCoreApplication::applicationDirPath() + "/city-on-tenne.png"));
            const auto exported = Rom::fromBytes(edits.rebuild(rom));
            const auto reopened = MapEdits::load(exported);
            QCOMPARE(reopened.source(1).geometry[0].data, edits.model(1));
            QCOMPARE(reopened.fineTerrain(1), edits.fineTerrain(1));
            QCOMPARE(reopened.source(1).field.render(source.map.bounds, 512),
                     field.render(source.map.bounds, 512));
            QCOMPARE(reopened.source(2).geometry[0].data, edits.source(2).geometry[0].data);
            edits.restore(1);
            QCOMPARE(edits.rebuild(rom), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void bridgePainting() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto edits = MapEdits::load(rom);
            const auto &source = edits.source(1);
            const auto brush =
                source.field.terrainBrush(2, source.terrain[0].data, source.map.bounds);
            qInfo() << "Native bridge template" << brush.bridge.size() << "faces"
                    << brush.texture.size() << "filtered" << brush.bridgeFiltered;
            QCOMPARE(brush.bridge.size(), 116);
            for (const auto &[radius, count] :
                 std::array<std::pair<double, int>, 3>{{{1., 5}, {1.5, 9}, {2., 13}}}) {
                edits.paint(1, {{100, 110}}, 1, radius);
                QCOMPARE(edits.changes(1).paint.count(char(1)), count);
                edits.restore(1);
            }
            for (int type : {2, 10}) {
                edits.paint(1, {{175, 68}, {211, 68}}, type, 1.5);
                edits.paint(1, {{180, 83}, {215, 83}}, type, 3);
                const auto model = edits.model(1);
                const auto field = Battlefield::load(rom, 1, {}, model);
                const auto &b = source.map.bounds;
                const QRectF crop(b.left() + b.width() * 150 / 256.,
                                  b.top() + b.height() * 55 / 256., b.width() * 78 / 256.,
                                  b.height() * 60 / 256.);
                QVERIFY(field.render(crop, 800).save(QCoreApplication::applicationDirPath() +
                                                     QString("/bridge-painted-%1.png").arg(type)));
                const FieldBorders cut(source, edits.changes(1));
                int kept = 0;
                for (const auto &t : source.field.triangles())
                    kept += !cut.removes(t);
                const auto triangles = field.triangles();
                double lo = 1e9, hi = -1e9;
                for (int i = kept; i < triangles.size(); ++i)
                    for (auto p : triangles[i].points) {
                        lo = std::min(lo, double(p.y()));
                        hi = std::max(hi, double(p.y()));
                    }
                QVERIFY(hi - lo > .05);
                auto copy = MapEdits::load(rom);
                copy.apply(edits.json());
                QCOMPARE(copy.model(1), model);
                const auto exported = Rom::fromBytes(edits.rebuild(rom));
                const auto reopened = MapEdits::load(exported);
                QCOMPARE(reopened.source(1).geometry[0].data, model);
                QCOMPARE(reopened.source(1).geometry[1].data,
                         editFieldHeight(source, edits.changes(1)));
                const auto world = source.map.world(193, 68);
                const auto &hd = reopened.source(1).geometry[1].data;
                auto f = [&](int at) { return double(std::bit_cast<float>(u32(hd, at))); };
                const int hx = int(
                              std::round((world.x() - f(4)) / (f(8) - f(4)) * (u16(hd, 0) - 1))),
                          hz = int(
                              std::round((world.y() - f(20)) / (f(24) - f(20)) * (u16(hd, 2) - 1)));
                qInfo() << "Bridge deck height" << surfaceHeight(field.surface(), world)
                        << "movement" << movementHeight(hd, hx, hz);
                QVERIFY2(std::abs(movementHeight(hd, hx, hz) -
                                  surfaceHeight(field.surface(), world)) < .02,
                         "Bridge movement height must match its visible deck");
                QCOMPARE(reopened.fineTerrain(1), edits.fineTerrain(1));
                QCOMPARE(reopened.source(1).field.render(crop, 800), field.render(crop, 800));
                QCOMPARE(reopened.source(2).geometry[0].data, edits.source(2).geometry[0].data);
                edits.restore(1);
            }
            QCOMPARE(edits.rebuild(rom), rom.bytes());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void terrainBorderReplacement() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto edit = MapEdits::load(rom);
            const auto &source = edit.source(1);
            QCOMPARE(u8(source.visualTerrain, 65 * 256 + 35), quint8(5));
            edit.paint(1, {{35, 20}, {31, 43}, {35, 63}, {52, 90}, {75, 110}, {82, 137}}, 1, 4);
            const FieldBorders plan(source, edit.changes(1));
            int faces = 0;
            for (const auto &s : plan.removed)
                faces += s.size();
            qInfo() << "Border regeneration" << faces << "removed native faces,"
                    << plan.removed.size() << "draws";
            QVERIFY(faces > 0);
            const auto roadModel = edit.model(1);
            qInfo() << "Rebuilt border model" << roadModel.size();
            const auto field = Battlefield::load(rom, 1, {}, roadModel);
            QVERIFY(field.render(source.map.bounds, 1024)
                        .save(QCoreApplication::applicationDirPath() + "/terrain-border-road.png"));
            const auto exported = Rom::fromBytes(edit.rebuild(rom));
            const auto reopened = MapEdits::load(exported);
            QVERIFY(reopened.source(1).geometry[1].data != source.geometry[1].data);
            QCOMPARE(reopened.source(1).field.render(source.map.bounds, 512),
                     field.render(source.map.bounds, 512));

            // Exercise every replacement material inside both native raised types.
            // Unremoved native faces must survive, and flat replacements must lower
            // both the drawn surface and the game's independent movement grid.
            for (int old : {5, 7}) {
                QPoint centre(-1, -1);
                for (int y = 25; y < 230 && centre.x() < 0; ++y)
                    for (int x = 25; x < 230 && centre.x() < 0; ++x) {
                        bool full = true;
                        for (int dy = -9; dy <= 9 && full; ++dy)
                            for (int dx = -9; dx <= 9; ++dx) {
                                const int at = (y + dy) * 256 + x + dx;
                                full &= u8(source.visualTerrain, at) == old &&
                                        u8(source.terrain[0].data, at) > 0 &&
                                        u8(source.terrain[0].data, at) < 15;
                            }
                        if (full)
                            centre = {x, y};
                    }
                QVERIFY(centre.x() >= 0);
                const auto world = source.map.world(centre.x(), centre.y());
                const auto &height = source.geometry[1].data;
                auto real = [&](int at) { return std::bit_cast<float>(u32(height, at)); };
                const int hx = int(std::round((world.x() - real(4)) / (real(8) - real(4)) *
                                              (u16(height, 0) - 1))),
                          hz = int(std::round((world.y() - real(20)) / (real(24) - real(20)) *
                                              (u16(height, 2) - 1)));
                for (int type = 1; type <= 14; ++type) {
                    if (type == old)
                        continue;
                    edit.restore(1);
                    edit.paint(1, {centre}, type, 4);
                    const FieldBorders cut(source, edit.changes(1));
                    QVERIFY(!cut.removed.isEmpty());
                    const auto model = edit.model(1);
                    const auto drawn = Battlefield::load(rom, 1, {}, model);
                    const auto before = source.field.triangles(), after = drawn.triangles();
                    int kept = 0;
                    const auto sites = source.field.siteGroups().values();
                    for (const auto &triangle : before) {
                        if (cut.removes(triangle))
                            continue;
                        QVERIFY(kept < after.size());
                        for (int v = 0; v < 3; ++v) {
                            const auto shift = after[kept].points[v] - triangle.points[v];
                            QVERIFY(std::abs(shift.x()) < .0001);
                            QVERIFY(std::abs(shift.z()) < .0001);
                            if (!sites.contains(triangle.group))
                                QVERIFY(std::abs(shift.y()) < .0001);
                        }
                        ++kept;
                    }
                    QVERIFY(kept < before.size());
                    QVERIFY(after.size() > kept);
                    const auto newHeight = editFieldHeight(source, edit.changes(1));
                    if (!FieldBorders::raised(type)) {
                        QVERIFY2(surfaceHeight(drawn.surface(), world) <
                                     surfaceHeight(source.field.surface(), world) - .1,
                                 qPrintable(
                                     QString("Terrain %1 did not cut type %2").arg(type).arg(old)));
                        QVERIFY(movementHeight(newHeight, hx, hz) <
                                movementHeight(height, hx, hz) - .05);
                    }
                    // The cut's surrounding native terrain remains elevated.
                    const auto rim = source.map.world(centre.x() + 6, centre.y());
                    if (!FieldBorders::raised(type))
                        QVERIFY(surfaceHeight(drawn.surface(), rim) >
                                surfaceHeight(drawn.surface(), world) + .05);
                    const auto romAgain = Rom::fromBytes(edit.rebuild(rom));
                    const auto again = MapEdits::load(romAgain);
                    QCOMPARE(again.source(1).geometry[0].data, model);
                    QCOMPARE(again.source(1).geometry[1].data, newHeight);
                    QCOMPARE(u8(again.fineTerrain(1), centre.y() * 256 + centre.x()), quint8(type));
                    QCOMPARE(again.source(2).geometry[0].data, edit.source(2).geometry[0].data);
                }
            }
            edit.restore(1);
            QCOMPARE(edit.rebuild(rom), rom.bytes());
            qInfo() << "Every brush replaces forest/highlands, rebuilds edges, preserves other "
                       "faces and maps, and exports geometry, movement heights and terrain.";
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void curvedRoadContinuity() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto edit = MapEdits::load(rom);
            const auto &source = edit.source(1);
            const QVector<QPointF> stroke{{35, 20}, {31, 43},  {35, 63},
                                          {52, 90}, {75, 110}, {82, 137}};
            edit.paint(1, stroke, 1, 4);
            const auto field = Battlefield::load(rom, 1, {}, edit.model(1));
            const auto bounds = source.map.bounds;
            const QRectF crop(bounds.left() + bounds.width() * .06,
                              bounds.top() + bounds.height() * .04, bounds.width() * .4,
                              bounds.height() * .55);
            const auto folder = QCoreApplication::applicationDirPath();
            const auto picture = field.render(crop, 900);
            QVERIFY(picture.save(folder + "/road-continuity-after.png"));
            const FieldBrushShape shape(edit.changes(1).paint, 1, bounds, source.visualTerrain);
            double largestJump = 0;
            for (int y = 15; y < 145; ++y)
                for (int x = 20; x < 90; ++x) {
                    const QPointF p(x, y);
                    if (shape.distance(p) < .1)
                        continue;
                    for (const QPointF axis : {QPointF(.0001, 0), QPointF(0, .0001)})
                        largestJump = std::max(
                            largestJump,
                            QLineF(shape.roadUv(p - axis), shape.roadUv(p + axis)).length());
                }
            qInfo() << "Largest curved-road UV discontinuity" << largestJump;
            auto brokenCentre = [&](const QImage &rendered) {
                int broken = 0;
                for (int i = 1; i < stroke.size(); ++i)
                    for (int n = 1; n < 100; ++n) {
                        const auto p = QPointF(stroke[i - 1]) +
                                       QPointF(stroke[i] - stroke[i - 1]) * (n / 100.);
                        const auto world = shape.world(p);
                        const int x = int((world.x() - crop.left()) * 900 / crop.width()),
                                  y = int((world.y() - crop.top()) * 900 / crop.height());
                        const auto c = rendered.pixelColor(x, y);
                        broken += c.blue() < c.green() * .35 || c.red() < c.green() * .9;
                    }
                return broken;
            };
            const int broken = brokenCentre(picture);
            qInfo() << "Broken road centre samples:" << broken << "/ 495";
            // Compare against the exposed base after clearing the crossed terrain.
            auto original = source.field.surface();
            const FieldBorders cut(source, edit.changes(1));
            original.removeIf([&](const FieldTriangle &t) { return cut.removes(t); });
            const auto triangles = field.triangles();
            int kept = 0;
            for (const auto &t : source.field.triangles())
                kept += !cut.removes(t);
            double minimumClearance = 1e9;
            for (int i = kept; i < triangles.size(); ++i) {
                const auto &t = triangles[i].points;
                const auto p = (t[0] + t[1] + t[2]) / 3;
                for (const auto &base : original) {
                    const auto &q = base.points;
                    const double x = q[1].x() - q[0].x(), z = q[1].z() - q[0].z(),
                                 xx = q[2].x() - q[0].x(), zz = q[2].z() - q[0].z(),
                                 px = p.x() - q[0].x(), pz = p.z() - q[0].z(),
                                 det = x * zz - xx * z;
                    if (std::abs(det) < 1e-8)
                        continue;
                    const double a = (px * zz - xx * pz) / det, b = (x * pz - px * z) / det;
                    if (a < 0 || b < 0 || a + b > 1)
                        continue;
                    const double height =
                        q[0].y() + a * (q[1].y() - q[0].y()) + b * (q[2].y() - q[0].y());
                    minimumClearance = std::min(minimumClearance, p.y() - height);
                }
            }
            qInfo() << "Minimum road-to-surface clearance" << minimumClearance;
            QVERIFY2(largestJump < .005,
                     "Road UVs must not jump across nearest-centerline boundaries");
            QVERIFY2(broken < 5, "A curved forest crossing must have a continuous road centre");
            QVERIFY2(minimumClearance > -.02,
                     "Road mesh must not sink below the surface beyond vertex quantization");
            const auto exported = Rom::fromBytes(edit.rebuild(rom));
            QCOMPARE(Battlefield::load(exported, 1).render(crop, 900), picture);
            const auto reopened = MapEdits::load(exported);
            const auto &again = reopened.source(1);
            QCOMPARE(
                again.field.terrainBrush(1, again.terrain[0].data, again.map.bounds).texture.size(),
                source.field.terrainBrush(1, source.terrain[0].data, bounds).texture.size());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void paintedTerrainFog() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            for (int key : {1, 5}) {
                auto edit = MapEdits::load(rom);
                QCOMPARE(edit.source(key).field.terrainFog(), key == 5);
                edit.paint(key, {{110, 140}}, 7, 4);
                const auto model = edit.model(key);
                const int geometry = int((u32(model, 48) + 15) & ~15u);
                constexpr int widths[] = {0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 3, 3, 1,
                                          1, 0, 1, 1, 1, 2, 2, 3, 1, 2, 2, 3, 2};
                quint32 last = 0;
                for (int at = int(u32(model, 0)); u32(model, at);) {
                    const int op = int(u32(model, at));
                    QVERIFY(op <= 25);
                    if (op == 1)
                        last = u32(model, at + 4);
                    at += (1 + widths[op]) * 4;
                }
                QVERIFY(last);
                quint32 mode = 0, cycle = 0, high = 0, low = 0;
                QSet<quint32> blenders;
                for (int at = geometry + int(last & 0xffffff); u32(model, at) != 0xdf000000;
                     at += 8) {
                    const auto a = u32(model, at), b = u32(model, at + 4);
                    if (a == 0xd9000000)
                        mode = b;
                    if (a == 0xe3000a01)
                        cycle = b;
                    if (a >> 24 == 0xfc) {
                        high = a;
                        low = b;
                    }
                    if (a == 0xe200001c)
                        blenders.insert(b);
                }
                QCOMPARE(bool(mode & 0x10000), key == 5);
                QCOMPARE(cycle, key == 5 ? 0x100000u : 0u);
                QVERIFY(blenders.contains(key == 5 ? 0xc8112078u : 0x00552078u));
                QVERIFY(blenders.contains(key == 5 ? 0xc81049d8u : 0x005049d8u));
                if (key == 5) {
                    CombineInputs in;
                    in.texel0 = {200, 160, 120, 255};
                    in.primitive = {255, 255, 255, 102};
                    in.shade = {128, 128, 128, 0};
                    const ColorCombiner combiner(high, low, true);
                    const auto color = combiner.evaluate(in);
                    QCOMPARE(color[0], quint8(100));
                    QCOMPARE(color[3], quint8(102));
                    in.shade[3] = 255; // Fog must not change brush coverage.
                    QCOMPARE(combiner.evaluate(in), color);
                }
                const auto rebuilt = Rom::fromBytes(edit.rebuild(rom));
                QCOMPARE(MapEdits::load(rebuilt).source(key).geometry[0].data, model);
            }
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void roadTextureFlow() {
        QByteArray mask(65536, char(-1)), native(65536, char(3));
        // A diagonal whose X+Z is constant exposed the old texture-coordinate bug.
        for (int x = 40; x <= 200; ++x)
            for (int d = -5; d <= 5; ++d)
                mask[(240 - x + d) * 256 + x] = 1;
        FieldBrushShape shape(mask, 1, QRectF(0, 0, 256, 256), native);
        const auto a = shape.roadUv({60.5, 180.5}), b = shape.roadUv({100.5, 140.5});
        QVERIFY(std::abs(a.y() - b.y()) > 50);
        const double left = shape.roadUv({100.5 - 2, 140.5 - 2}).x();
        const double right = shape.roadUv({100.5 + 2, 140.5 + 2}).x();
        QVERIFY(std::min(left, right) < .2);
        QVERIFY(std::max(left, right) > .8);
        QVERIFY(std::abs(shape.roadUv({100.5, 140.5}).x() - .5) < .15);
    }
    void briefingArtworkExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto edit = MapEdits::load(rom);
            edit.moveSite(1, 0, edit.source(1).map.points[0] + QPointF(1, 1));
            edit.paint(1, {{110, 140}, {145, 170}}, 1, 3);
            const auto exported = Rom::fromBytes(edit.rebuild(rom));
            const auto before = ScenarioMap::load(rom, 1), after = ScenarioMap::load(exported, 1);
            QVERIFY(after.image->rgba != before.image->rgba);
            QCOMPARE(after.imageName, before.imageName);
            QCOMPARE(after.image->width, before.image->width);
            QCOMPARE(after.image->height, before.image->height);
            QVERIFY(after.imageBounds);
            QVERIFY(QLineF(after.imageBounds->topLeft(), before.imageBounds->topLeft()).length() <
                    .00001);
            QCOMPARE(ScenarioMap::load(exported, 2).image->rgba,
                     ScenarioMap::load(rom, 2).image->rgba);
            const auto id = SceneImages::presentationImage(exported, 1);
            QCOMPARE(id, SceneImages::presentationImage(rom, 1));
            QCOMPARE(SceneImages::decode(exported, id).composite.rgba, after.image->rgba);
            const auto &r = *after.image;
            QImage(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width, r.height,
                   QImage::Format_RGBA8888)
                .save(QCoreApplication::applicationDirPath() + "/briefing-edited.png");
            auto reimported = MapEdits::load(exported);
            QCOMPARE(reimported.rebuild(exported), exported.bytes());
            reimported.moveSite(1, 0, reimported.source(1).map.points[0] + QPointF(.25, .25));
            const auto second = ScenarioMap::load(Rom::fromBytes(reimported.rebuild(exported)), 1);
            QCOMPARE(second.imageBounds, after.imageBounds);
            qInfo() << "Briefing image, scene lookup, same framing, unedited maps and reimport "
                       "verified.";
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void suppliedMapProject() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   file = qEnvironmentVariable("LORDLY_MAP_PROJECT");
        if (path.isEmpty() || file.isEmpty())
            QSKIP("Private user project required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.applyManifest(Project::readProject(file));
            const auto &edit = *project.mapEdits();
            const auto oldPath = qEnvironmentVariable("LORDLY_MAP_OLD_ROM");
            const auto old = Rom::open(oldPath);
            const auto exported = Rom::fromBytes(edit.rebuild(rom));
            QSet<int> visited;
            const auto changed = edit.json()["maps"].toObject();
            for (int key = 1; key <= 64; ++key) {
                const int model = u8(rom.bytes(), 0x654a0 + key * 9 + 3);
                if (key == 54 || visited.contains(model) || !changed.contains(QString::number(model)))
                    continue;
                visited.insert(model);
                const auto &source = edit.source(key);
                const auto field = Battlefield::load(rom, key, {}, edit.model(key));
                const auto stem =
                    QCoreApplication::applicationDirPath() + QString("/user-map-%1").arg(key);
                field.render(source.map.bounds, 1024).save(stem + "-corrected.png");
                Battlefield::load(old, key)
                    .render(source.map.bounds, 1024)
                    .save(stem + "-before.png");
                if (edit.changes(key).paint.contains(char(7))) {
                    const auto expected =
                        source.field.terrainBrush(7, source.terrain[0].data, source.map.bounds).texture;
                    QByteArray texels;
                    for (int y = 0; y < expected.height(); ++y)
                        for (int x = 0; x < expected.width(); ++x) {
                            const auto c = expected.pixelColor(x, y);
                            const int at = int(texels.size());
                            texels += QByteArray(2, 0);
                            put16(texels, at,
                                  quint16(((c.red() * 31 + 127) / 255 << 11) |
                                          ((c.green() * 31 + 127) / 255 << 6) |
                                          ((c.blue() * 31 + 127) / 255 << 1) | (c.alpha() >= 128)));
                        }
                    QVERIFY(!texels.isEmpty());
                    QVERIFY(edit.model(key).contains(texels));
                }
                QCOMPARE(Battlefield::load(exported, key).render(source.map.bounds, 512),
                         field.render(source.map.bounds, 512));
                QVERIFY(ScenarioMap::load(exported, key).image->rgba != source.map.image->rgba);
                QFile grid(stem + "-fine.bin");
                QVERIFY(grid.open(QIODevice::WriteOnly));
                grid.write(MapEdits::load(exported).fineTerrain(key));
                qInfo() << "User map" << key << "model bytes" << edit.model(key).size();
            }
            QCOMPARE(visited.size(), changed.size());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
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
            source.field.terrainBrush(1, source.terrain[0].data, source.map.bounds)
                .texture.scaled(256, 256)
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
            const FieldBorders borders(source, edit.changes(1));
            int movedVertices = 0, kept = 0;
            for (int t = 0; t < beforeTriangles.size(); ++t) {
                if (borders.removes(beforeTriangles[t]))
                    continue;
                for (int v = 0; v < 3; ++v) {
                    const auto delta =
                        afterTriangles[kept].points[v] - beforeTriangles[t].points[v];
                    if (beforeTriangles[t].group == movedGroup) {
                        QVERIFY(std::abs(delta.x() - 2) < .0001);
                        QVERIFY(std::abs(delta.z() - 1) < .0001);
                        ++movedVertices;
                    } else
                        QVERIFY(delta.length() < .0001);
                }
                ++kept;
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
            QCOMPARE(newSource.geometry[1].data, editFieldHeight(source, edit.changes(1)));
            QCOMPARE(newSource.geometry[0].data, model);
            QVERIFY(newSource.map.image->rgba != source.map.image->rgba);
            QCOMPARE(newSource.map.image->width, source.map.image->width);
            QCOMPARE(newSource.map.image->height, source.map.image->height);
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
            int movedSites = 0, cutMaps = 0;
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
                // Native display lists differ by map; exercise the new face
                // filtering wherever the map contains an editable raised region.
                for (int cell = 16 * 256; cell < 240 * 256; ++cell) {
                    const int terrain = u8(source.terrain[0].data, cell);
                    if (!FieldBorders::raised(u8(source.visualTerrain, cell)) || !terrain ||
                        terrain >= 15)
                        continue;
                    edits.paint(key, {{double(cell % 256), double(cell / 256)}}, 3, 3);
                    const FieldBorders cut(source, edits.changes(key));
                    QVERIFY(!cut.removed.isEmpty());
                    const auto bytes = edits.model(key);
                    QVERIFY(bytes.size() < 512 * 1024);
                    const auto after = Battlefield::load(rom, key, {}, bytes).triangles();
                    int kept = 0;
                    for (const auto &triangle : before)
                        if (!cut.removes(triangle)) {
                            QVERIFY(kept < after.size());
                            for (int v = 0; v < 3; ++v) {
                                const auto shift = after[kept].points[v] - triangle.points[v];
                                QVERIFY(std::abs(shift.x()) < .0001);
                                QVERIFY(std::abs(shift.z()) < .0001);
                            }
                            ++kept;
                        }
                    QVERIFY(after.size() > kept);
                    qInfo() << "Native terrain cut: key" << key << "bytes" << bytes.size();
                    ++cutMaps;
                    edits.restore(key);
                    break;
                }
            }
            QVERIFY(movedSites > 200);
            QVERIFY(cutMaps > 20);
            for (int type = 1; type <= 14; ++type) {
                // Matching native terrain is deliberately a geometry no-op.
                // Exercise each material on a different, paintable native type.
                QPoint target(-1, -1);
                const auto &source = edits.source(1);
                for (int y = 32; y < 224 && target.x() < 0; y += 8)
                    for (int x = 32; x < 224 && target.x() < 0; x += 8) {
                        const int at = y * 256 + x;
                        if (u8(source.visualTerrain, at) != type &&
                            u8(source.terrain[0].data, at) > 0 &&
                            u8(source.terrain[0].data, at) < 15)
                            target = {x, y};
                    }
                QVERIFY(target.x() >= 0);
                edits.paint(1, {target}, type, 3);
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
    void battlefieldJoiningAndBoundary() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto edit = MapEdits::load(rom);
            const auto &s = edit.source(1);
            QPoint interior(-1, -1), edge(-1, -1);
            for (int y = 25; y < 230; ++y)
                for (int x = 25; x < 230; ++x) {
                    if (u8(s.visualTerrain, y * 256 + x) != 5)
                        continue;
                    bool full = true;
                    for (int dy = -4; dy <= 4; ++dy)
                        for (int dx = -4; dx <= 4; ++dx)
                            full &= u8(s.visualTerrain, (y + dy) * 256 + x + dx) == 5;
                    if (full && interior.x() < 0)
                        interior = {x, y};
                    if (u8(s.visualTerrain, y * 256 + x + 3) == 3 && edge.x() < 0)
                        edge = {x, y};
                }
            QVERIFY(interior.x() >= 0);
            QVERIFY(edge.x() >= 0);
            edit.paint(1, {interior}, 5, 3);
            auto noOp = Battlefield::load(rom, 1, {}, edit.model(1));
            QCOMPARE(noOp.triangleCount(), s.field.triangleCount());
            QCOMPARE(noOp.render(s.map.bounds, 512), s.field.render(s.map.bounds, 512));
            QCOMPARE(editFieldHeight(s, edit.changes(1)), s.geometry[1].data);
            edit.restore(1);
            edit.paint(1, {edge + QPoint(4, 0)}, 5, 9);
            FieldBrushShape join(edit.changes(1).paint, 5, s.map.bounds, s.visualTerrain);
            FieldBrushShape separate(edit.changes(1).paint, 5, s.map.bounds);
            QVERIFY(join.rise(edge) < separate.rise(edge));
            const auto field = Battlefield::load(rom, 1, {}, edit.model(1));
            QVERIFY(field.triangleCount() > s.field.triangleCount());
            const auto world = s.map.world(edge.x(), edge.y());
            const auto folder = QCoreApplication::applicationDirPath();
            const QRectF crop(world.x() - 2.5, world.y() - 2.5, 5, 5);
            QVERIFY(s.field.render(crop, 600).save(folder + "/join-before.png"));
            QVERIFY(field.render(crop, 600).save(folder + "/join-after.png"));
            qInfo() << "Forest join at" << edge << "added rise" << join.rise(edge)
                    << "separate brush rise" << separate.rise(edge);
            edit.restore(1);
            // More than the former 5,000 triangles, using repeated real edits.
            for (int y = 40; y <= 200; y += 32)
                for (int x = 40; x <= 200; x += 32)
                    edit.paint(1, {{double(x), double(y)}}, 4, 11);
            const auto compact = edit.model(1);
            const auto many = Battlefield::load(rom, 1, {}, compact);
            const int added = many.triangleCount() - s.field.triangleCount();
            qInfo() << "36 dabs:" << added << "added triangles," << compact.size() << "model bytes";
            QVERIFY(added > 5000);
            QVERIFY(compact.size() < 512 * 1024);
            QVERIFY(!many.render(s.map.bounds, 512).isNull());
            edit.restore(1);
            // Scenery painting must not silently expand movement or the cursor.
            constexpr int cell = 8 * 256 + 128;
            QCOMPARE(u8(s.terrain[0].data, cell), quint8(0));
            const int type = u8(s.visualTerrain, cell) == 4 ? 3 : 4;
            edit.paint(1, {{128, 8}}, type, 5);
            QCOMPARE(u8(edit.changes(1).paint, cell), quint8(type));
            QCOMPARE(u8(edit.fineTerrain(1), cell), quint8(0));
            const auto terrain = fieldMembers(edit.plan().first().terrain);
            QCOMPARE(terrain[1].data, s.terrain[1].data);
            QCOMPARE(edit.boundary(1), s.boundary);
            QVERIFY(Battlefield::load(rom, 1, {}, edit.model(1)).triangleCount() >
                    s.field.triangleCount());
            const QRectF expanded(QPointF(2, 2), QPointF(253, 253));
            edit.setBoundary(1, expanded);
            QCOMPARE(u8(edit.fineTerrain(1), cell), quint8(type));
            QCOMPARE(u8(edit.fineTerrain(1), 128), quint8(0));
            auto copy = MapEdits::load(rom);
            copy.apply(edit.json());
            QCOMPARE(copy.json(), edit.json());
            QCOMPARE(copy.boundary(1), expanded);
            const auto image = edit.rebuild(rom);
            QCOMPARE(image.mid(0x13c878, 4), QByteArray::fromHex("0202fdfd"));
            QCOMPARE(image.mid(0x13c87c, 4), rom.bytes().mid(0x13c87c, 4));
            const auto reopened = MapEdits::load(Rom::fromBytes(image));
            QCOMPARE(reopened.boundary(1), expanded);
            QCOMPARE(reopened.source(1).map.bounds, s.map.bounds);
            QCOMPARE(reopened.points(1), s.map.points);
            QCOMPARE(reopened.fineTerrain(1), edit.fineTerrain(1));
            QCOMPARE(reopened.source(1).geometry[1].data, editFieldHeight(s, edit.changes(1)));
            QCOMPARE(u8(reopened.source(1).terrain[1].data, 2 * 64 + 32), quint8(type));
            QCOMPARE(reopened.source(2).terrain[0].data, edit.source(2).terrain[0].data);
            edit.setBoundary(1, QRectF(QPointF(32, 32), QPointF(220, 220)));
            QCOMPARE(u8(edit.fineTerrain(1), cell), quint8(0));
            const auto shrunk = fieldMembers(edit.plan().first().terrain);
            QCOMPARE(u8(shrunk[1].data, 2 * 64 + 32), quint8(0));
            QVERIFY_THROWS_EXCEPTION(Error, edit.setBoundary(1, QRectF(0, 0, 255, 255)));
            QVERIFY_THROWS_EXCEPTION(Error, edit.setBoundary(39, expanded));
            edit.restore(1);
            QCOMPARE(edit.rebuild(rom), rom.bytes());
            // Shared missions use one physical map and must export matching cursor edges.
            edit.setBoundary(3, expanded);
            QCOMPARE(edit.boundary(11), expanded);
            const auto shared = edit.rebuild(rom);
            QCOMPARE(shared.mid(0x13c874 + 3 * 4, 4), QByteArray::fromHex("0202fdfd"));
            QCOMPARE(shared.mid(0x13c874 + 11 * 4, 4), QByteArray::fromHex("0202fdfd"));
            edit.restore(3);
            const auto &fort = edit.source(47);
            edit.setBoundary(47, QRectF(QPointF(1, 1), QPointF(254, 254)));
            const auto fortFine = edit.fineTerrain(47);
            const auto fortTerrain = fieldMembers(edit.plan().first().terrain);
            int protectedCells = 0;
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x) {
                    if (u8(fort.terrain[1].data, y * 64 + x) < 15)
                        continue;
                    bool excluded = true;
                    for (int dz = 0; dz < 4; ++dz)
                        for (int dx = 0; dx < 4; ++dx)
                            excluded &= !u8(fortFine, (y * 4 + dz) * 256 + x * 4 + dx);
                    if (!excluded) {
                        QCOMPARE(fortTerrain[1].data[y * 64 + x], fort.terrain[1].data[y * 64 + x]);
                        ++protectedCells;
                    }
                }
            QVERIFY(protectedCells > 0);
            qInfo() << "Matching forest, compact geometry, scenery paint and boundary ROM/project "
                       "roundtrips verified.";
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void battlefieldBrushProfiles() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto edit = MapEdits::load(rom);
            const auto &source = edit.source(1);
            const auto mountain =
                source.field.terrainBrush(7, source.terrain[0].data, source.map.bounds);
            const auto forest =
                source.field.terrainBrush(5, source.terrain[0].data, source.map.bounds);
            QCOMPARE(mountain.texture.size(), QSize(32, 64));
            QCOMPARE(forest.texture.size(), QSize(32, 64));
            QCOMPARE(forest.border.size(), QSize(64, 32));
            // Choose the largest clear patch of plains,
            // so original hills cannot masquerade as our generated elevation.
            QPoint center;
            int best = -1;
            for (int y = 24; y < 232; y += 4)
                for (int x = 24; x < 232; x += 4) {
                    int radius = 0;
                    for (int r = 1; r <= 22; ++r) {
                        bool plain = true;
                        for (int z = -r; z <= r && plain; ++z)
                            for (int xx = -r; xx <= r; ++xx)
                                if (xx * xx + z * z <= r * r &&
                                    u8(source.terrain[0].data, (y + z) * 256 + x + xx) != 3) {
                                    plain = false;
                                    break;
                                }
                        if (!plain)
                            break;
                        radius = r;
                    }
                    if (radius > best) {
                        center = {x, y};
                        best = radius;
                    }
                }
            QVERIFY(best >= 10);
            const int radius = std::min(best, 20);
            const auto b = source.map.bounds;
            const QPointF world(b.left() + (center.x() + .5) * b.width() / 256,
                                b.top() + (center.y() + .5) * b.height() / 256);
            const double extent = (radius + 5) * b.width() / 256;
            const QRectF crop(world.x() - extent, world.y() - extent, extent * 2, extent * 2);
            const auto folder = QCoreApplication::applicationDirPath();
            source.field.render(crop, 512).save(folder + "/brush-before.png");
            edit.paint(1, {center}, 7, radius);
            const auto field = Battlefield::load(rom, 1, {}, edit.model(1));
            const auto tris = field.triangles();
            double bottom = 1e6, top = -1e6;
            for (int t = source.field.triangleCount(); t < tris.size(); ++t)
                for (const auto &v : tris[t].points) {
                    bottom = std::min(bottom, double(v.y()));
                    top = std::max(top, double(v.y()));
                }
            qInfo() << "Brush profile" << center << radius << b << bottom << top;
            QVERIFY2(top - bottom > .5, "Mountain painting must produce an actual raised surface");
            const auto rendered = field.render(crop, 512);
            QVERIFY(rendered.save(folder + "/brush-mountain.png"));
            double foot = 0, summit = 0;
            int nf = 0, ns = 0;
            const double pixelRadius = radius / double(radius + 5) * 256;
            for (int y = 0; y < 512; ++y)
                for (int x = 0; x < 512; ++x) {
                    const double d = std::hypot(x - 256., y - 256.) / pixelRadius;
                    const double light = qGray(rendered.pixel(x, y));
                    if (d < .15) {
                        summit += light;
                        ++ns;
                    }
                    if (d > .85 && d < .95) {
                        foot += light;
                        ++nf;
                    }
                }
            QVERIFY2(summit / ns > foot / nf * 1.4, "Mountain profile must rise from dark foot to "
                                                    "light summit without repeating bands");
            const auto exported = Rom::fromBytes(edit.rebuild(rom));
            const auto reopened = MapEdits::load(exported);
            const auto &rebuilt = reopened.source(1);
            QVERIFY(rebuilt.geometry[1].data != source.geometry[1].data);
            QCOMPARE(rebuilt.geometry[0].data, edit.model(1));
            QCOMPARE(rebuilt.field.render(crop, 512), rendered);
            QVERIFY(rebuilt.map.image->rgba != source.map.image->rgba);
            QCOMPARE(rebuilt.map.image->width, source.map.image->width);
            QCOMPARE(rebuilt.map.image->height, source.map.image->height);
            QCOMPARE(reopened.source(2).geometry[1].data, edit.source(2).geometry[1].data);
            // Independent RLE readback: verify a raised central movement sample,
            // not merely a changed compressed member or renderer-only hill.
            auto height = [](const QByteArray &data, int x, int z) {
                auto f = [&](int at) { return double(std::bit_cast<float>(u32(data, at))); };
                int at = 28 + u16(data, 28 + z * 2) * 2, cell = 0;
                while (cell < u16(data, 0)) {
                    const int token = u16(data, at);
                    at += 2;
                    const int n = token >= 0xff00 ? token & 255 : 1;
                    const int value = token >= 0xff00 ? u16(data, at) : token;
                    if (token >= 0xff00)
                        at += 2;
                    if (x < cell + n)
                        return f(12) + (f(16) - f(12)) * value / 65279.;
                    cell += n;
                }
                return -1000.;
            };
            const auto &hd = source.geometry[1].data;
            auto f = [&](int at) { return double(std::bit_cast<float>(u32(hd, at))); };
            const int hx = int(std::round((world.x() - f(4)) / (f(8) - f(4)) * (u16(hd, 0) - 1))),
                      hz =
                          int(std::round((world.y() - f(20)) / (f(24) - f(20)) * (u16(hd, 2) - 1)));
            QVERIFY(height(rebuilt.geometry[1].data, hx, hz) > height(hd, hx, hz) + .5);
            edit.restore(1);
            edit.paint(1, {center}, 5, radius);
            const auto trees = Battlefield::load(rom, 1, {}, edit.model(1));
            QVERIFY(trees.render(crop, 512).save(folder + "/brush-forest.png"));
            edit.restore(1);
            edit.paint(1, {center}, 4, radius);
            QVERIFY(Battlefield::load(rom, 1, {}, edit.model(1))
                        .render(crop, 512)
                        .save(folder + "/brush-barrens.png"));
            qInfo() << "Verified full forest/rock textures, raised native mesh, light summit, "
                       "movement-height readback, export/reopen, updated briefing and other maps; "
                       "centre"
                    << center << "radius" << radius;
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
