#include "engine/effect_player.h"
#include "engine/effect_raster.h"
#include "engine/accepted_effect_code.h"
#include <QFile>
#include <QtTest>
using namespace lordly;
class EffectRasterTests : public QObject {
    Q_OBJECT
  private slots:
    void nativeMotion_data() {
        QTest::addColumn<int>("easing");
        QTest::addColumn<QPoint>("halfway");
        QTest::newRow("linear") << 0 << QPoint(20,-6);
        QTest::newRow("ease-in") << 1 << QPoint(10,-3);
        QTest::newRow("ease-out") << 2 << QPoint(30,-9);
        QTest::newRow("ease-in-out") << 3 << QPoint(20,-6);
    }
    void nativeMotion() {
        QFETCH(int, easing);
        QFETCH(QPoint, halfway);
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom=Rom::open(path);
            AttackVisual baseline;baseline.nativeEffect=8;baseline.sizePercent=101;baseline.delay=3;
            auto motion=baseline;motion.motionStart=45;motion.motionTicks=11;motion.motionX=40;motion.motionY=-12;
            motion.motionEasing=easing;
            QCOMPARE(AttackVisual::fromJson(motion.json()),motion);
            QCOMPARE(AttackVisual::read(motion.build(8)),motion);
            const auto oldBytes=baseline.build(8);
            QCOMPARE(oldBytes.mid(20,12),QByteArray(12,0));
            QCOMPARE(AttackVisual::read(oldBytes),baseline);
            auto invalid=motion;invalid.motionEasing=4;QVERIFY_THROWS_EXCEPTION(Error,invalid.validate());
            auto bounds=[](const QImage &image) {
                QRect result;
                for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x)
                    if(image.pixelColor(x,y).alpha()>10) result|=QRect(x,y,1,1);
                return result;
            };
            EffectPlayer original(rom),moving(rom);
            original.startVisual(baseline,8,{});moving.startVisual(motion,8,{});
            int checked=0;
            while(original.advance()) {
                QVERIFY(moving.advance());
                const int tick=moving.tick();
                QCOMPARE(u32(moving.memory(),0x430030),quint32(std::max(0,tick-3)));
                if(tick!=49 && tick!=54 && tick!=59) continue;
                const auto a=bounds(original.draw({320,240},.75).image);
                const auto b=bounds(moving.draw({320,240},.75).image);
                QVERIFY(!a.isEmpty());QVERIFY(!b.isEmpty());
                const QPoint shift=tick==49?QPoint(0,0):tick==54?halfway:QPoint(40,-12);
                QVERIFY(std::abs((b.left()-a.left())-shift.x())<=1);
                QVERIFY(std::abs((b.top()-a.top())-shift.y())<=1);
                // Dynamic viewports live in each display list. The saved original
                // viewport stays unchanged while frames are submitted to the RSP.
                QCOMPARE(u16(moving.memory(),0x430048),quint16(640));
                ++checked;
            }
            QCOMPARE(checked,3);QVERIFY(!moving.advance());
            moving.startVisual(motion,8,{});QCOMPARE(u32(moving.memory(),0x430030),0U);
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void acceptedCAndModifiedRom() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private US Rev0 ROM required");
        try {
            const auto rom = Rom::open(path).bytes();
            QFile input(":/generated/accepted-effects.bin");
            QVERIFY(input.open(QIODevice::ReadOnly));
            const auto code = input.readAll();
            QCOMPARE(code.first(8), QByteArray("LCCODE01"));
            const auto count = u32(code, 8);
            QVERIFY(count >= 237);
            EffectMachine machine;
            int at = 44;
            for (quint32 i = 0; i < count; ++i) {
                const auto start = u32(code, at), address = u32(code, at + 4), size = u32(code, at + 8);
                at += 16;
                // Independent retail ROM input, not another build of the C.
                QCOMPARE(code.mid(at, size), rom.mid(start, size));
                machine.mount(address, rom.mid(start, size), u32(code, at - 4) != 0);
                at += int(size);
            }
            QCOMPARE(at, code.size());
            const auto before = machine.memory;
            QCOMPARE(installAcceptedEffectCode(machine, rom), int(count));
            QCOMPARE(machine.memory, before); // Includes all retained tails and unrelated data.
            const auto start = u32(code, 44), address = u32(code, 48);
            auto patchedRom = rom;
            patchedRom[start] ^= 1;
            machine.byte(address, machine.byte(address) ^ 1);
            const auto patchedMemory = machine.memory;
            QCOMPARE(installAcceptedEffectCode(machine, patchedRom), int(count) - 1);
            QCOMPARE(machine.memory, patchedMemory);
            // A RAM-only installed hook also survives even with a retail input.
            QCOMPARE(installAcceptedEffectCode(machine, rom), int(count) - 1);
            QCOMPARE(machine.memory, patchedMemory);
            EffectMachine unloaded;
            QCOMPARE(installAcceptedEffectCode(unloaded, rom), 0);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void retailAppearanceTransform() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            QRect bounds[2];
            for (int variant = 0; variant < 2; ++variant) {
                AttackVisual visual;
                visual.nativeEffect = 8;
                visual.hue = 120;
                if (variant) { visual.sizePercent = 150; visual.offsetX = 12; visual.offsetY = -6; }
                EffectPlayer player(Rom::open(path));
                player.startVisual(visual, 8, {});
                while (player.advance()) {
                    if (player.tick() != 56) continue;
                    const auto image = player.draw({320, 240}, .75).image;
                    for (int y = 0; y < image.height(); ++y)
                        for (int x = 0; x < image.width(); ++x)
                            if (image.pixelColor(x,y).alpha() > 10)
                                bounds[variant] |= QRect(x,y,1,1);
                }
            }
            QVERIFY(!bounds[0].isEmpty()); QVERIFY(!bounds[1].isEmpty());
            const QRectF expected((bounds[0].left()-160)*1.5+172,
                (bounds[0].top()-120)*1.5+114, bounds[0].width()*1.5, bounds[0].height()*1.5);
            const auto clipped = expected.intersected(QRectF(0,0,320,240));
            QVERIFY(std::abs(bounds[1].left()-clipped.left()) <= 3);
            QVERIFY(std::abs(bounds[1].top()-clipped.top()) <= 3);
            QVERIFY(std::abs(bounds[1].width()-clipped.width()) <= 3);
            QVERIFY(std::abs(bounds[1].height()-clipped.height()) <= 3);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void previewSides() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            QImage nativeViews[2], importedViews[2];
            QImage pixels(20, 16, QImage::Format_RGBA8888);
            pixels.fill(Qt::transparent);
            pixels.setPixelColor(1, 2, Qt::red);
            pixels.setPixelColor(17, 12, Qt::blue);
            AttackVisual imported;
            imported.frames = SpriteAsset::fromImage("Asymmetric effect", pixels);
            for (int side = 0; side < 2; ++side) {
                EffectPlayer::Scene scene;
                scene.enemyCaster = bool(side);
                scene.sourceClass = 2; scene.targetClass = 5;
                EffectPlayer player(Rom::open(path));
                player.start(8, scene);
                const int sourceId = side ? 0 : 9, targetId = side ? 9 : 0;
                QCOMPARE(u8(player.memory(), 0x720000), quint8(sourceId));
                QCOMPARE(u8(player.memory(), 0x72000c), quint8(targetId));
                const auto sourceActor = 0x740000 + 0x1c4 + sourceId * 0xf8;
                const auto targetActor = 0x740000 + 0x1c4 + targetId * 0xf8;
                QCOMPARE(u32(player.memory(), sourceActor + 0x40) & 0x100, side ? 0x100U : 0U);
                QCOMPARE(u32(player.memory(), sourceActor + 0x48), 2U);
                QCOMPARE(u32(player.memory(), targetActor + 0x48), 5U);
                while (player.advance()) if (player.tick() == 56)
                    nativeViews[side] = player.draw({320, 240}, .75).image;
                player.startVisual(imported, 8, scene);
                QVERIFY(player.advance());
                importedViews[side] = player.draw({320, 240}, .75).image;
            }
            QVERIFY(!nativeViews[0].isNull()); QVERIFY(!nativeViews[1].isNull());
            QVERIFY(nativeViews[0] != nativeViews[1]);
            QCOMPARE(importedViews[0], importedViews[1]);
        } catch(const std::exception &e) { QFAIL(e.what()); }
    }
    void importedFrames() {
        try {
            QImage red(32, 32, QImage::Format_RGBA8888);
            red.fill(QColor(255, 0, 0, 255));
            auto asset = SpriteAsset::fromImage("Red square", red);
            asset.anchor = {16, 16};
            asset.frames[0].ticks = 3;
            AttackVisual visual;
            visual.frames = asset;
            visual.delay = 2;
            const auto bytes = visual.build(0);
            const auto reopened = AttackVisual::read(bytes);
            QCOMPARE(reopened.build(0), bytes);
            QCOMPARE(AttackVisual::fromJson(visual.json()), visual);
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required for native image drawing");
            EffectPlayer player(Rom::open(path));
            player.startVisual(visual, 0, {});
            for (int i = 0; i < 2; ++i) {
                QVERIFY(player.advance());
                QCOMPARE(player.draw().triangles, 0);
            }
            for (int i = 0; i < 3; ++i) {
                QVERIFY(player.advance());
                const auto frame = player.draw();
                QVERIFY(frame.triangles > 0);
                int reds = 0;
                for (int y = 0; y < frame.image.height(); ++y)
                    for (int x = 0; x < frame.image.width(); ++x)
                        reds += frame.image.pixelColor(x, y) == QColor(255, 0, 0, 255);
                QVERIFY(reds > 400);
                if (i == 0)
                    QVERIFY(frame.image.save("native/build/imported-effect.png"));
            }
            QVERIFY(player.advance());
            QCOMPARE(player.draw().triangles, 0);
            QVERIFY(!player.advance());
            auto broken = bytes;
            put32(broken, 32, quint32(bytes.size() + 1));
            QVERIFY_THROWS_EXCEPTION(Error, AttackVisual::read(broken));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void retailPlayback() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private US Rev0 ROM required");
        const auto rom = Rom::open(path);
        QStringList failures;
        int rendered = 0;
        const auto selected = qEnvironmentVariable("LORDLY_EFFECT_ID").split(',');
        for (int id = 0; id <= 132; ++id) {
            if (qEnvironmentVariableIsSet("LORDLY_EFFECT_ID") &&
                !selected.contains(QString::number(id)))
                continue;
            try {
                EffectPlayer player(rom);
                EffectPlayer::Scene scene;
                scene.allies = qEnvironmentVariableIsSet("LORDLY_EFFECT_ALLIES");
                scene.enemyCaster = !qEnvironmentVariableIsSet("LORDLY_EFFECT_PLAYER");
                if (qEnvironmentVariableIsSet("LORDLY_EFFECT_TARGETS"))
                    scene.targetCount = qEnvironmentVariableIntValue("LORDLY_EFFECT_TARGETS");
                if (qEnvironmentVariableIsSet("LORDLY_EFFECT_EDITED")) {
                    AttackVisual appearance;
                    appearance.nativeEffect = id;
                    appearance.hue = 120; appearance.sizePercent = 125;
                    appearance.offsetX = 12; appearance.offsetY = -6;
                    if(qEnvironmentVariableIsSet("LORDLY_EFFECT_MOTION")) {
                        appearance.motionStart=10;appearance.motionTicks=40;
                        appearance.motionX=-24;appearance.motionY=12;appearance.motionEasing=3;
                    }
                    player.startVisual(appearance, id, scene);
                } else player.start(id, scene);
                int triangles = 0;
                bool visible = false;
                while (player.advance()) {
                    const auto frame = player.draw({160, 120}, .375);
                    triangles += frame.triangles;
                    if (!visible)
                        for (int y = 0; y < frame.image.height() && !visible; ++y) {
                            const auto *row = frame.image.constScanLine(y);
                            for (int x = 0; x < frame.image.width(); ++x)
                                if (row[x * 4 + 3]) {
                                    visible = true;
                                    break;
                                }
                        }
                    if (player.tick() == 56 && qEnvironmentVariableIsSet("LORDLY_EFFECT_CAPTURE"))
                        QVERIFY(player.draw().image.save(
                            qEnvironmentVariable("LORDLY_EFFECT_CAPTURE")));
                }
                int editable = 0;
                for (const auto &r : player.resources()) editable += !r.size.isEmpty();
                if (qEnvironmentVariableIsSet("LORDLY_EFFECT_RESOURCES"))
                    for (const auto &r : player.resources())
                        qInfo() << "Resource" << r.type << r.id << r.bytes.size() << r.size;
                qInfo() << "Effect" << id << "ticks" << player.tick() << "triangles" << triangles
                        << "artwork" << editable;
                rendered += visible;
                // A zero-length ribbon can be empty when source equals target;
                // that regression checks bounded completion instead.
                if (!visible && !scene.allies)
                    failures << QString("Effect %1 rendered no visible pixels").arg(id);
            } catch (const std::exception &e) {
                failures << QString("Effect %1: %2").arg(id).arg(e.what());
                qWarning() << failures.last();
            }
        }
        qInfo() << "Effects with visible drawing" << rendered;
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join('\n')));
    }
    void importedTilesAndClipping() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            // Odd width, several TMEM tiles, transparency, and a clipped left
            // edge exercise source stride and texture coordinates independently.
            QImage pixels(67, 35, QImage::Format_RGBA8888);
            const QColor colors[]{Qt::red, Qt::green, Qt::blue, Qt::transparent};
            for (int y = 0; y < pixels.height(); ++y)
                for (int x = 0; x < pixels.width(); ++x)
                    pixels.setPixelColor(x, y, colors[(x / 7 + y / 5) % 4]);
            AttackVisual visual;
            visual.frames = SpriteAsset::fromImage("Tiled effect", pixels);
            visual.frames->anchor = {165, 10};
            visual.frames->frames[0].ticks = 1;
            EffectPlayer player(Rom::open(path));
            player.startVisual(visual, 0, {});
            QVERIFY(player.advance());
            const auto image = player.draw({320, 240}, 1).image;
            for (int y = 0; y < 240; ++y)
                for (int x = 0; x < 320; ++x) {
                    const int sx = x + 5, sy = y - 110;
                    const auto expected = sx < 67 && sy >= 0 && sy < 35 ? pixels.pixelColor(sx, sy)
                                                                        : QColor(Qt::transparent);
                    QCOMPARE(image.pixelColor(x, y), expected);
                }
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void texturedTriangleAndBounds() {
        try {
            QByteArray memory(0x4000, 0);
            quint32 at = 0x1000;
            auto command = [&](quint32 high, quint32 low) {
                put32(memory, at, high);
                put32(memory, at + 4, low);
                at += 8;
            };
            // One-cycle direct TEXEL0, a two-by-two RGBA16 red texture, and three
            // known vertices. The expected pixel position follows world coordinates.
            command(0xfcffffff, 0xfffffe79);
            command(0xfd100001, 0x2100);
            command(0xf5100200, 0x07000000);
            command(0xf3000000, 0x07003000);
            command(0xf5100200, 0x00080200);
            command(0xf2000000, 0x00004004);
            command(0x01003006, 0x2000);
            command(0x05000204, 0);
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 2; ++x)
                    put16(memory, 0x2100 + y * 8 + x * 2, 0xf801);
            const std::array<std::array<qint16, 2>, 3> positions{
                {{-10, 10}, {-10, -10}, {10, -10}}};
            for (int i = 0; i < 3; ++i) {
                put16(memory, 0x2000 + i * 16, quint16(positions[i][0]));
                put16(memory, 0x2002 + i * 16, quint16(positions[i][1]));
                put32(memory, 0x200c + i * 16, 0xffffffff);
            }
            auto rendered = EffectRaster::draw(memory, 0x1000, at, {100, 100}, 1);
            QCOMPARE(rendered.triangles, 1);
            QCOMPARE(rendered.image.pixelColor(42, 74), QColor(255, 0, 0, 255));
            QCOMPARE(rendered.image.pixelColor(10, 10).alpha(), 0);
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 2; ++x)
                    put16(memory, 0x2100 + y * 8 + x * 2, 0xf800);
            rendered = EffectRaster::draw(memory, 0x1000, at, {100, 100}, 1);
            QCOMPARE(rendered.image.pixelColor(42, 74).alpha(), 0);
            put32(memory, 0x1034, 0x900000); // Vertex memory must be bounded.
            QVERIFY_THROWS_EXCEPTION(Error, EffectRaster::draw(memory, 0x1000, at));
            QVERIFY_THROWS_EXCEPTION(Error, EffectRaster::draw(memory, 0x1000, at + 1));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void nativeDrawingCapture() {
        try {
            const auto file = qEnvironmentVariable("LORDLY_EFFECT_RAM");
            if (file.isEmpty())
                QSKIP("Private effect drawing capture required");
            QFile input(file);
            QVERIFY(input.open(QIODevice::ReadOnly));
            const auto memory = input.readAll();
            const auto result = EffectRaster::draw(memory, 0x80600000, u32(memory, 0xe9ba0));
            QVERIFY(result.commands > 0);
            QVERIFY(result.triangles > 0);
            int visible = 0;
            for (int y = 0; y < result.image.height(); ++y)
                for (int x = 0; x < result.image.width(); ++x)
                    visible += result.image.pixelColor(x, y).alpha() > 0;
            QVERIFY(visible > 20);
            QVERIFY(result.image.save(file + ".png"));
            qInfo() << "Native triangles" << result.triangles << "visible pixels" << visible;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(EffectRasterTests)
#include "effect_raster_tests.moc"
