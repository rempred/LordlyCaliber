#include "engine/attack_data.h"
#include "engine/custom_runtime.h"
#include "engine/capacity_placement.h"
#include "engine/squad_members.h"
#include "engine/encounters.h"
#include "engine/effect_player.h"
#include "engine/effect_composition.h"
#include "engine/image.h"
#include "engine/project.h"
#include "rebuild/attack_runtime.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
using namespace lordly;
class AttackDataTests : public QObject {
    Q_OBJECT
    Rom source() const {
        return Rom::open(qEnvironmentVariable("LORDLY_TEST_ROM"));
    }
  private slots:
    void piecePlaybackSpeed() {
        EffectComposition model; model.size={8,8}; model.duration=12;
        EffectPiece p; p.id="sequence";p.name="Colours";p.duration=12;
        QImage image(2,2,QImage::Format_RGBA8888); image.fill(Qt::red);
        p.artwork=SpriteAsset::fromImage("Colours",image);p.artwork.frames[0].ticks=2;
        for (const auto colour : {Qt::green, Qt::blue}) {
            auto frame=p.artwork.frames[0];frame.id=SpriteLibrary::newId();frame.layers[0].image.fill(colour);
            p.artwork.frames.append(frame);
        }
        p.artwork.kind="sequence";
        model.pieces={p}; model.pieces[0].speedPercent=50;
        for (int tick=0;tick<12;++tick)
            QCOMPARE(model.render(tick).pixelColor(3,3),QColor(tick<4?Qt::red:tick<8?Qt::green:Qt::blue));
        model.pieces[0].speedPercent=200;
        for (int tick=0;tick<12;++tick)
            QCOMPARE(model.render(tick).pixelColor(3,3),QColor(tick%3==0?Qt::red:tick%3==1?Qt::green:Qt::blue));
        QCOMPARE(model.pieces[0].artwork,p.artwork);
        QCOMPARE(EffectComposition::fromAsset(model.bake()),model);
        auto legacy=model.json();auto pieces=legacy["pieces"].toArray();auto first=pieces[0].toObject();
        first.remove("speedPercent");pieces[0]=first;legacy["pieces"]=pieces;
        QCOMPARE(EffectComposition::fromJson(legacy).pieces[0].speedPercent,100);
        model.pieces[0].speedPercent=0;QVERIFY_THROWS_EXCEPTION(Error,model.validate());
    }
    void compositionTimingAndPixels() {
        EffectComposition model; model.name="Moving sparks";model.size={32,32};model.duration=7;
        QImage red(2,2,QImage::Format_RGBA8888);red.fill(QColor(255,0,0,128));
        EffectPiece first;first.id="red";first.name="Red spark";first.artwork=SpriteAsset::fromImage("Red",red);
        first.start=1;first.duration=3;first.from={-6,0};first.to={6,0};
        QImage green(2,2,QImage::Format_RGBA8888);green.fill(Qt::green);
        EffectPiece second;second.id="green";second.name="Green spark";second.artwork=SpriteAsset::fromImage("Green",green);
        second.start=3;second.duration=3;second.from=second.to={0,6};second.toOpacity=0;
        model.pieces={first,second};model.validate();
        QCOMPARE(model.render(0).pixelColor(9,15).alpha(),0);
        QCOMPARE(model.render(1).pixelColor(9,15),QColor(255,0,0,128));
        QCOMPARE(model.render(2).pixelColor(15,15),QColor(255,0,0,128));
        QCOMPARE(model.render(3).pixelColor(21,15),QColor(255,0,0,128));
        QCOMPARE(model.render(3).pixelColor(15,21),QColor(0,255,0,255));
        QCOMPARE(model.render(4).pixelColor(21,15).alpha(),0);
        QVERIFY(std::abs(model.render(4).pixelColor(15,21).alpha()-128)<=1);
        QCOMPARE(model.render(5).pixelColor(15,21).alpha(),0);
        auto changed=model;changed.pieces[0].to={0,-6};
        QCOMPARE(changed.render(3).pixelColor(15,21),model.render(3).pixelColor(15,21));
        QCOMPARE(EffectComposition::fromJson(model.json()),model);
        const auto asset=model.bake();QCOMPARE(EffectComposition::fromAsset(asset),model);
        int tick=0;for(const auto &f:asset.frames){for(int n=0;n<f.ticks;++n)QCOMPARE(f.layers[0].image,model.render(tick++));}QCOMPARE(tick,7);
        AttackVisual visual;visual.frames=asset;QVERIFY(visual.fullColorFrames());
        const auto decoded=decodeN64Image(AttackVisual::gameImage(red,true));
        QCOMPARE(decoded.rgba,QByteArray::fromHex("ff000080ff000080ff000080ff000080"));
        const auto bytes = visual.build(8);
        QCOMPARE(u16(bytes, 30), quint16(0x103));
        QCOMPARE(model.placement, 1);
        auto legacy = model.json();
        legacy.remove("placement");
        QCOMPARE(EffectComposition::fromJson(legacy).placement, 0);
        const auto read=AttackVisual::read(bytes);QCOMPARE(read,visual);
        QCOMPARE(EffectComposition::fromAsset(*read.frames),model);
        auto invalid=model;invalid.pieces[0].start=6;QVERIFY_THROWS_EXCEPTION(Error,invalid.validate());
        invalid=model;invalid.pieces[0].duration=2147483647;QVERIFY_THROWS_EXCEPTION(Error,invalid.validate());
        invalid=model;invalid.size={256,240};invalid.duration=300;QVERIFY_THROWS_EXCEPTION(Error,invalid.validate());
        invalid=model;while(invalid.pieces.size()<25){auto p=first;p.id=SpriteLibrary::newId();invalid.pieces<<p;}QVERIFY_THROWS_EXCEPTION(Error,invalid.validate());
        auto animated=first;animated.start=0;animated.duration=5;animated.from=animated.to={0,0};
        animated.artwork.frames[0].ticks=1;auto frame=animated.artwork.frames[0];frame.id="green";frame.layers[0].image=green;animated.artwork.frames<<frame;
        model.pieces={animated};model.duration=5;
        QCOMPARE(model.render(2).pixelColor(15,15),QColor(255,0,0,128));
        model.pieces[0].loop=false;QCOMPARE(model.render(2).pixelColor(15,15),QColor(0,255,0,255));
        auto raw=bytes;put16(raw,30,2);QVERIFY_THROWS_EXCEPTION(Error,AttackVisual::read(raw));
    }
    void visualExportAndPlayback() {
        try {
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty())
                QSKIP("Private compiler required");
            auto project = Project::fromRom(source());
            project.loadAttacks();
            auto data = *project.attacks();
            auto native = data.entry(55);
            native.visual.nativeEffect = 8;
            native.visual.delay = 3;
            native.visual.hue = 120;
            native.visual.sizePercent = 125;
            native.visual.offsetX = 12;
            native.visual.offsetY = -6;
            native.visual.motionStart=15; native.visual.motionTicks=20;
            native.visual.motionX=40; native.visual.motionY=-20; native.visual.motionEasing=3;
            EffectPlayer originalEffect(source());
            originalEffect.start(8, {});
            while (originalEffect.advance()) {}
            for (const auto &resource : originalEffect.resources())
                if (resource.type == 4 && resource.id == 1) {
                    EffectArtwork art{32, 32, resource.bytes, sha256(resource.bytes)};
                    for (int i = 0; i < art.rgba.size(); i += 4) {
                        put8(art.rgba, i, 32); put8(art.rgba, i + 1, 160);
                        put8(art.rgba, i + 2, 240);
                    }
                    native.visual.artwork[0x40001] = art;
                }
            QVERIFY(native.visual.artwork.contains(0x40001));
            QCOMPARE(AttackVisual::read(native.visual.build(8)), native.visual);
            QCOMPARE(AttackVisual::fromJson(native.visual.json()), native.visual);
            data.set(55, native);
            const int copied = data.duplicate(55, "Copied edited Fireball");
            QCOMPARE(copied, 159);
            auto copy = data.entry(copied);
            copy.visual.color = 0xff8000;
            copy.visual.hue = 0;
            data.set(copied, copy);
            QCOMPARE(data.entry(55), native);
            QImage pixels(32, 32, QImage::Format_RGBA8888);
            pixels.fill(QColor(255, 0, 0, 255));
            auto asset = SpriteAsset::fromImage("Test effect", pixels);
            asset.anchor = {16, 16};
            asset.frames[0].ticks = 3;
            auto imported = data.entry(56);
            imported.visual.frames = asset;
            imported.visual.delay = 2;
            data.set(56, imported);
            for (const auto [id, effect] : {std::pair{57, 97}, std::pair{58, 123}}) {
                auto a = data.entry(id);
                a.visual.nativeEffect = effect;
                a.visual.delay = 1;
                data.set(id, a);
            }
            QImage tiled(67, 35, QImage::Format_RGBA8888);
            const QColor colors[]{Qt::red, Qt::green, Qt::blue, Qt::transparent};
            for (int y = 0; y < tiled.height(); ++y)
                for (int x = 0; x < tiled.width(); ++x)
                    tiled.setPixelColor(x, y, colors[(x / 7 + y / 5) % 4]);
            auto multiple = data.entry(59);
            multiple.visual.frames = SpriteAsset::fromImage("Tiled frames", tiled);
            multiple.visual.frames->anchor = {165, 10};
            multiple.visual.frames->frames[0].ticks = 2;
            auto second = multiple.visual.frames->frames[0];
            second.id = "second";
            second.layers[0].image = tiled.flipped(Qt::Horizontal);
            second.ticks = 3;
            multiple.visual.frames->frames.append(second);
            multiple.visual.frames->kind = "sequence";
            data.set(59, multiple);
            auto geometry = data.entry(60);
            geometry.visual.nativeEffect = 94;
            geometry.visual.color = 0xff0000;
            data.set(60, geometry);
            auto coloured = data.entry(61);
            coloured.visual.nativeEffect = 8;
            for (const auto &r : originalEffect.resources())
                if (r.type == 4 && (r.id == 1 || r.id == 2)) {
                    const int width = r.id == 1 ? 32 : 16;
                    EffectArtwork art{width, width, r.bytes, sha256(r.bytes)};
                    if (r.id == 1) art.hue = 120;
                    else art.color = 0xff8000;
                    coloured.visual.artwork[quint32(4 << 16 | r.id)] = art;
                }
            QCOMPARE(coloured.visual.artwork.size(), 2);
            data.set(61, coloured);
            const int textureCopy = data.duplicate(61, "Independent artwork colours");
            QCOMPARE(textureCopy, 160);
            auto secondColours = data.entry(textureCopy);
            secondColours.visual.artwork[0x40002].color = 0x00ff00;
            data.set(textureCopy, secondColours);
            QCOMPARE(data.entry(61), coloured);
            EffectComposition composition;composition.name="Mixed independent pieces";composition.size={67,35};composition.duration=7;
            EffectPiece moving;moving.id="moving";moving.name="Moving red";moving.start=0;moving.duration=5;
            QImage soft(10,6,QImage::Format_RGBA8888);soft.fill(QColor(231,43,17,93));
            moving.artwork=SpriteAsset::fromImage("Soft red",soft);moving.from={-20,0};moving.to={20,0};
            auto movingFrame=moving.artwork.frames[0];movingFrame.id="green";
            movingFrame.layers[0].image.fill(QColor(11,249,63,93));
            moving.artwork.frames.append(movingFrame);moving.artwork.kind="sequence";moving.speedPercent=50;
            auto later=moving;later.id="later";later.name="Later blue";later.start=3;later.duration=4;later.from=later.to={0,10};later.toOpacity=0;
            soft.fill(QColor(11,63,249,197));later.artwork=SpriteAsset::fromImage("Soft blue",soft);
            composition.pieces = {moving, later};
            composition.placement = 0;
            auto composed=data.entry(62);composed.visual={};composed.visual.frames=composition.bake();data.set(62,composed);
            const int composedCopy=data.duplicate(62,"Copied composition");QCOMPARE(composedCopy,161);
            auto different=data.entry(161);auto changedComposition=EffectComposition::fromAsset(*different.visual.frames);changedComposition.pieces[0].to={0,-10};different.visual.frames=changedComposition.bake();data.set(161,different);
            for (int placement = 1; placement <= 3; ++placement) {
                const int id = data.duplicate(62, QString("Placement %1").arg(placement));
                QCOMPARE(id, 161 + placement);
                auto placed = data.entry(id);
                auto model = composition;
                model.placement = placement;
                model.size = {4, 4};
                model.duration = 2;
                model.pieces = {moving};
                model.pieces[0].duration = 2;
                model.pieces[0].from = model.pieces[0].to = {};
                placed.visual.frames = model.bake();
                data.set(id, placed);
            }
            project.setAttacks(data.json());
            QTemporaryDir output(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM"))
                                     .absoluteDir()
                                     .filePath("Test ROMs/native-attack-visuals-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            project.saveFolder(output.path());
            auto restored = Project::fromRom(source());
            restored.applyManifest(Project::readProject(output.filePath("project.json")));
            QCOMPARE(restored.attacks()->entries(), data.entries());
            const auto built = rebuild(restored, tools, output.path());
            auto checksum = built.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, built.image);
            exportRom(
                restored, built,
                output.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName()));
            const auto rom = Rom::fromBytes(built.image);
            const auto reopened = Project::fromRom(rom);
            QCOMPARE(reopened.attacks()->entry(55), native);
            QCOMPARE(reopened.attacks()->entry(56).visual.build(1), imported.visual.build(1));
            const auto p = AttackPlacement::read(rom);
            QVERIFY(p);
            QCOMPARE(reopened.attacks()->entry(62).visual,composed.visual);
            QCOMPARE(EffectComposition::fromAsset(*reopened.attacks()->entry(62).visual.frames),composition);
            for (int id : {55, 56, 57, 58, 59, 60, 61, 62, 159, 160, 161}) {
                const int slot =
                    u8(built.image, p->romOffset + AttackData::settingsOffset + id * 32 + 21);
                EffectPlayer actual(rom);
                actual.start(slot, {});
                int ticks = 0, triangles = 0;
                EffectPlayer expected(source());
                expected.startVisual(data.entry(id).visual,
                                     EffectPlayer::retailEffect(data.entry(id).animation), {});
                while (actual.advance()) {
                    QVERIFY(expected.advance());
                    const auto frame = actual.draw({320, 240}, .75);
                    triangles += frame.triangles;
                    ++ticks;
                    QCOMPARE(frame.image, expected.draw({320, 240}, .75).image);
                    if(id==62 && ticks==1) {
                        // The first piece's original 93/255 alpha and unquantized RGB survive MIPS drawing.
                        QCOMPARE(frame.image.pixelColor(140,120),QColor(231,43,17,93));
                    }
                }
                QVERIFY(!expected.advance());
                QVERIFY(triangles > 0);
                if (id == 61 || id == 160) {
                    int checked = 0;
                    for (const auto &r : actual.resources()) {
                        if (r.type != 4 && r.type != 7 && r.type != 8) continue;
                        const int resourceId = r.id & 65535;
                        if (r.type == 4 && resourceId == 1) {
                            // Original A0FF060B rotated120 degrees -> 06A0FF0B.
                            QCOMPARE(u32(r.bytes, 0), 0x06a0ff0bU);
                        } else if (r.type == 4 && resourceId == 2) {
                            const auto original = coloured.visual.artwork[0x40002].rgba;
                            for (int j = 0; j < r.bytes.size(); j += 4) {
                                const int hi = std::max({u8(original,j), u8(original,j+1), u8(original,j+2)});
                                QCOMPARE(u8(r.bytes,j), quint8(id == 61 ? hi : 0));
                                QCOMPARE(u8(r.bytes,j+1), quint8(id == 61 ? hi * 128 / 255 : hi));
                                QCOMPARE(u8(r.bytes,j+2), quint8(0));
                                QCOMPARE(u8(r.bytes,j+3), u8(original,j+3));
                            }
                        } else {
                            auto sourceResource = std::find_if(originalEffect.resources().begin(), originalEffect.resources().end(),
                                [&](const EffectPlayer::Resource &source) { return source.type == r.type && source.id == resourceId; });
                            QVERIFY(sourceResource != originalEffect.resources().end());
                            QCOMPARE(r.bytes, sourceResource->bytes);
                        }
                        ++checked;
                    }
                    QVERIFY(checked > 2);
                }
                if (id == 60) {
                    bool found = false;
                    for (const auto &r : actual.resources())
                        if (r.type == 6 && (r.id & 65535) == 647) {
                            found = true;
                            // Original first vertex is RGBA FAE755FF. Red tint
                            // keeps brightness FA and alpha FF, not its yellow.
                            QCOMPARE(u32(r.bytes, 40 + 12), 0xfa0000ffU);
                        }
                    QVERIFY(found);
                }
                if (id == 55 || id == 159) {
                    bool found = false;
                    for (const auto &r : actual.resources())
                        if (r.type == 4 && (r.id & 65535) == 1) {
                            found = true;
                            const auto &original = native.visual.artwork[0x40001].rgba;
                            // Independently specified RGB after a 120-degree hue
                            // rotation, or brightness-preserving orange tint.
                            for (int j = 0; j < r.bytes.size(); j += 4) {
                                QCOMPARE(u8(r.bytes, j), quint8(240));
                                QCOMPARE(u8(r.bytes, j + 1), quint8(id == 55 ? 32 : 120));
                                QCOMPARE(u8(r.bytes, j + 2), quint8(id == 55 ? 160 : 0));
                                QCOMPARE(u8(r.bytes, j + 3), u8(original, j + 3));
                            }
                        }
                    QVERIFY(found);
                }
                if (id == 56)
                    QCOMPARE(ticks, 6);
                qInfo() << "Compiled effect for attack" << id << "ticks" << ticks << "triangles"
                        << triangles;
            }
            // Run edited copies and the untouched donor in one machine. This
            // exercises real shared caches, task ownership and slot recycling.
            EffectPlayer shared(rom);
            const auto slotFor = [&](int id) {
                return u8(built.image, p->romOffset + AttackData::settingsOffset + id * 32 + 21);
            };
            for (int id : {162, 163, 164})
                for (bool enemyCaster : {false, true})
                    for (bool allies : {false, true})
                        for (bool special : {false, true}) {
                            QCOMPARE(reopened.attacks()->entry(id).visual, data.entry(id).visual);
                            EffectPlayer::Scene scene;
                            scene.enemyCaster = enemyCaster;
                            scene.allies = allies;
                            scene.targetCount = 3;
                            EffectPlayer actual(rom), expected(source());
                            actual.start(slotFor(id), scene);
                            expected.startVisual(data.entry(id).visual, 0, scene);
                            // A defeated target remains an effect anchor (revival/status cleanup).
                            const int targetBase =
                                allies ? (enemyCaster ? 0 : 9) : (enemyCaster ? 9 : 0);
                            put16(const_cast<QByteArray &>(actual.memory()),
                                  0x740000 + 0x1c4 + targetBase * 0xf8 + 0x20, 0);
                            put16(const_cast<QByteArray &>(expected.memory()),
                                  0x740000 + 0x1c4 + targetBase * 0xf8 + 0x20, 0);
                            if (special)
                                for (auto player : {&actual, &expected}) {
                                    auto &memory = const_cast<QByteArray &>(player->memory());
                                    const int actor = 0x740000 + 0x1c4 + 18 * 0xf8;
                                    memory.replace(
                                        actor, 0xf8,
                                        memory.mid(0x740000 + 0x1c4 + targetBase * 0xf8, 0xf8));
                                    put32(memory, actor + 0x5c, 15);
                                    put32(memory, actor + 0x64, 22);
                                    put8(memory, 0x720014, 18);
                                }
                            int ticks = 0;
                            while (actual.advance()) {
                                QVERIFY(expected.advance());
                                const auto frame = actual.draw({320, 240}, .75).image;
                                QCOMPARE(frame, expected.draw({320, 240}, .75).image);
                                if (++ticks == 1 && id != 164) {
                                    const bool right = id == 163 ? !enemyCaster : targetBase == 9;
                                    const int x = right ? 220 : 100;
                                    if (special && id == 162)
                                        QCOMPARE(frame.pixelColor(171, 152),
                                                 QColor(231, 43, 17, 93));
                                    else
                                        QCOMPARE(frame.pixelColor(x, 143), QColor(231, 43, 17, 93));
                                    if (id == 162) {
                                        QCOMPARE(frame.pixelColor(x, 148), QColor(231, 43, 17, 93));
                                        QCOMPARE(frame.pixelColor(x, 154), QColor(231, 43, 17, 93));
                                    }
                                }
                                if (ticks == 1 && id == 164 && !special)
                                    QCOMPARE(frame.pixelColor(targetBase == 9 ? 231 : 88, 147),
                                             QColor(231, 43, 17, 93));
                            }
                            QVERIFY(!expected.advance());
                            QVERIFY(ticks >= 2);
                        }
            shared.start(slotFor(55), {});
            shared.append(slotFor(159));
            shared.append(8);
            while (shared.advance()) shared.draw({160, 120}, .375);
            bool red = false, orange = false, untouched = false;
            for (const auto &r : shared.resources())
                if (r.type == 4 && (r.id & 65535) == 1) {
                    red |= u8(r.bytes, 0) == 240 && u8(r.bytes, 1) == 32;
                    orange |= u8(r.bytes, 0) == 240 && u8(r.bytes, 1) == 120;
                    untouched |= sha256(r.bytes) == native.visual.artwork[0x40001].originalHash;
                }
            QVERIFY(red); QVERIFY(orange); QVERIFY(untouched);
            for (int next : {8, int(slotFor(159)), 8}) {
                shared.append(next);
                int triangles = 0, checked = 0;
                while (shared.advance()) {
                    triangles += shared.draw({160, 120}, .375).triangles;
                    const auto &memory = shared.memory();
                    const auto cache = u32(memory, 0x220ca8) & 0x1fffffff;
                    for (int i = 0; i < 128; ++i) {
                        const auto row = cache + i * 20;
                        if (!u16(memory, row) || u16(memory, row + 2) != 4 ||
                            (u32(memory, row + 4) & 65535) != 1) continue;
                        const auto owner = u32(memory, row + 4) >> 16;
                        const auto image = u32(memory, row + 12) & 0x1fffffff;
                        if (owner == 159) {
                            QCOMPARE(u8(memory, image), quint8(240));
                            QCOMPARE(u8(memory, image + 1), quint8(120));
                            QCOMPARE(u8(memory, image + 2), quint8(0));
                        } else if (!owner) {
                            QCOMPARE(sha256(memory.mid(image, 4096)), native.visual.artwork[0x40001].originalHash);
                        }
                        ++checked;
                    }
                }
                // Native RNG advances across uses, so repeated frames need not
                // equal a freshly initialized battle. The live texture data must.
                QVERIFY(triangles > 0); QVERIFY(checked > 0);
            }
            EffectPlayer clean(source()), unchanged(rom);
            clean.start(8, {}); unchanged.start(8, {});
            while (clean.advance()) {
                QVERIFY(unchanged.advance());
                QCOMPARE(clean.draw({160, 120}, .375).image, unchanged.draw({160, 120}, .375).image);
            }
            QVERIFY(!unchanged.advance());
            auto again = reopened;
            const auto secondExport = rebuild(again, tools, output.path());
            const auto secondOpen = Project::fromRom(Rom::fromBytes(secondExport.image));
            QCOMPARE(secondOpen.attacks()->entries(), reopened.attacks()->entries());
            for (int effect : {42, 68}) {
                EffectPlayer actual(rom);
                EffectPlayer::Scene scene;
                scene.targetCount = 1;
                actual.start(effect, scene);
                while (actual.advance())
                    actual.draw({160, 120}, .375);
                QVERIFY(actual.tick() > 20);
                qInfo() << "Sparse-target native effect" << effect << "completed" << actual.tick();
            }
            qInfo().noquote() << "Attack visual candidate:" << output.path();
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void initTestCase() {
        if (qEnvironmentVariableIsEmpty("LORDLY_TEST_ROM"))
            QSKIP("Private original ROM required");
    }
    void retailDataAndTemplateMaps() {
        const auto rom = source();
        const auto attacks = AttackData::loadRetail(rom);
        QCOMPARE(attacks.entries().size(), 159);
        QVERIFY(!attacks.modified());
        for (int id = 1; id < 159; ++id)
            QCOMPARE(attacks.entry(id).data, rom.bytes().mid(0x60980 + 16 * id, 12));
        QCOMPARE(AttackData::displayName(attacks.entry(4)), QString("Slash"));
        QVERIFY(attacks.entry(45).dynamicName);
        QVERIFY(attacks.sharingName(55).contains(56));
        for (int id = 45; id <= 48; ++id) {
            const auto targets = AttackData::templateTargets(id);
            for (int element = 1; element <= 6; ++element)
                QCOMPARE(targets.value(element),
                         int(u8(rom.bytes(), 0x6136c + 8 * (id - 45) + element)));
        }
        QCOMPARE(AttackData::elements().value(9), QString("Wind + Water"));
    }
    void sharedAndIndependentNames() {
        auto data = AttackData::loadRetail(source());
        const auto original = data.json();
        const auto users = data.sharingName(55);
        data.rename(55, "Shared lightning", true);
        for (int id : users)
            QCOMPARE(AttackData::displayName(data.entry(id)), QString("Shared lightning"));
        data.rename(55, "Solo lightning", false);
        QCOMPARE(data.sharingName(55), QVector<int>{55});
        QCOMPARE(AttackData::displayName(data.entry(56)), QString("Shared lightning"));
        data.apply(original);
        QVERIFY(!data.modified());
        const int copy = data.duplicate(55, "Independent spell");
        QCOMPARE(copy, 159);
        auto edited = data.entry(copy);
        put8(edited.data, 10, 0x9a);
        put8(edited.data, 11, 0xe7);
        edited.powerPercent = 175;
        edited.targetPattern = 12;
        edited.targetMask = 0x155;
        edited.status = 10;
        edited.chance = 35;
        data.set(copy, edited);
        QCOMPARE(data.entry(55).data, AttackData::loadRetail(source()).entry(55).data);
        auto restored = AttackData::loadRetail(source());
        restored.apply(data.json());
        QCOMPARE(restored.entries(), data.entries());
        QCOMPARE(u8(restored.entry(copy).data, 10), quint8(0x9a));
        QCOMPARE(restored.sharingName(copy), QVector<int>{copy});
        const int created = restored.create("New attack");
        QCOMPARE(created, 160);
        QCOMPARE(restored.entry(created).data, restored.entry(5).data);
        QCOMPARE(restored.entry(created).behavior, 5);
    }
    void validationAndCapacity() {
        auto data = AttackData::loadRetail(source());
        const auto before = data.json();
        auto malformed = before;
        auto rows = malformed["entries"].toArray();
        auto row = rows[0].toObject();
        row["chance"] = 101;
        rows[0] = row;
        malformed["entries"] = rows;
        QVERIFY_THROWS_EXCEPTION(Error, data.apply(malformed));
        QCOMPARE(data.json(), before);
        auto a = data.entry(1);
        a.targetPattern = 12;
        QVERIFY_THROWS_EXCEPTION(Error, data.set(1, a));
        QCOMPARE(data.json(), before);
        for (int id = 159; id <= 254; ++id)
            QCOMPARE(data.duplicate(4, QString("Attack %1").arg(id)), id);
        QCOMPARE(data.entries().size(), 255);
        const auto full = data.json();
        QVERIFY_THROWS_EXCEPTION(Error, data.create("Overflow"));
        QCOMPARE(data.json(), full);
        auto restored = AttackData::loadRetail(source());
        restored.apply(full);
        QCOMPARE(restored.entries(), data.entries());
    }
    void animationDonorsAndSpecialPoses() {
        auto project = Project::fromRom(source());
        project.loadAttacks();
        const auto original = project.bodyAnimationData();
        auto data = *project.attacks();
        const int ordinary = data.duplicate(5, "Independent Cleave");
        const int special = data.duplicate(17, "Independent Sonic Boom");
        project.setAttacks(data.json());
        auto overrides = *project.animationOverrides();
        QJsonArray rows{QJsonObject{
            {"classId", 2}, {"actionId", 5}, {"normalSelector", 41}, {"blockedSelector", 40}}};
        overrides.setEntries(rows, project.rom(), project.classData(), *project.animationRoutes());
        project.setAnimationOverrides(overrides.json());
        auto body = project.bodyAnimationData();
        QCOMPARE(body->selector(2, 2, 5, 0, 0), 41);
        QCOMPARE(body->selector(2, 2, ordinary, 0, 0), original->selector(2, 2, 5, 0, 0));
        QCOMPARE(body->attackSelector(22, 22, 17, 0, 0), 42);
        QCOMPARE(body->attackSelector(22, 22, special, 0, 0), 42);
        QCOMPARE(body->attackSelector(57, 57, 32, 0, 0), 43);
        QCOMPARE(body->attackSelector(57, 57, 32, 0, 1), 42);
        rows << QJsonObject{{"classId", 2},
                            {"actionId", ordinary},
                            {"normalSelector", 41},
                            {"blockedSelector", 40}};
        overrides.setEntries(rows, project.rom(), project.classData(), *project.animationRoutes());
        project.setAnimationOverrides(overrides.json());
        QCOMPARE(project.bodyAnimationData()->selector(2, 2, ordinary, 0, 0), 41);
    }
    void spellArraysAndMissingOptions() {
        try {
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty()) QSKIP("Private compiler required");
            auto project = Project::fromRom(source());project.loadAttacks();
            auto data = *project.attacks();
            const auto retail = data.json();
            const int spell = data.duplicate(55, "New flame");
            const int array = data.duplicate(45, "Independent magic");
            QCOMPARE(spell, 159);QCOMPARE(array, 160);
            auto a = data.entry(spell);put8(a.data, 1, 2);
            a.targetPattern = 10;a.targetSide = 1;a.status = 7;a.condition = 2;a.chance = 75;
            a.visual.nativeEffect = 8;a.visual.hue = 60;
            data.set(spell, a);
            for (const auto pair : {qMakePair(45,55),qMakePair(45,99),qMakePair(array,63),qMakePair(49,57),qMakePair(51,55),qMakePair(145,147)}) {
                auto command = data.entry(pair.first);command.spellLinks[pair.second] = spell;
                data.set(pair.first, command);
            }
            QCOMPARE(data.resolvedTargets(45).value(1),spell);
            QCOMPARE(data.resolvedTargets(array).value(1),55);
            QCOMPARE(data.resolvedTargets(array).value(2),spell);
            QCOMPARE(data.resolvedTargets(46),AttackData::templateTargets(46));
            const auto before = data.json();
            auto invalid = data.entry(45);invalid.spellLinks[55] = array;
            QVERIFY_THROWS_EXCEPTION(Error,data.set(45,invalid));QCOMPARE(data.json(),before);
            invalid.spellLinks[55] = 254;
            QVERIFY_THROWS_EXCEPTION(Error,data.set(45,invalid));QCOMPARE(data.json(),before);
            invalid = data.entry(spell);invalid.condition = 1;
            QVERIFY_THROWS_EXCEPTION(Error,data.set(spell,invalid));QCOMPARE(data.json(),before);
            project.setAttacks(data.json());
            auto knight = project.record(Table::Classes,5).bytes;put8(knight,55,array);put8(knight,56,2);
            project.replaceBytes(Table::Classes,5,knight);
            QTemporaryDir output(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).absoluteDir().filePath("Test ROMs/native-attack-options-XXXXXX"));
            QVERIFY(output.isValid());output.setAutoRemove(false);
            const auto folder = output.filePath("project"), archive = output.filePath("options.lcproj");
            project.saveFolder(folder);project.saveArchive(archive);
            for (const auto &path : {folder,archive}) {
                auto restored = Project::fromRom(source());restored.applyManifest(Project::readProject(path));
                QCOMPARE(restored.attacks()->json(),data.json());
                QCOMPARE(u8(restored.record(Table::Classes,5).bytes,55),quint8(array));
            }
            const auto built = rebuild(project,tools,output.path());
            const auto candidate = output.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName());
            exportRom(project,built,candidate);
            auto reopened = Project::fromRom(Rom::open(candidate));reopened.loadAttacks();
            QCOMPARE(reopened.attacks()->json(),data.json());
            QCOMPARE(rebuild(reopened,tools,output.path()).image,built.image);
            auto checksum = built.image;Rom::updateChecksum(checksum);QCOMPARE(checksum,built.image);
            QProcess runtime;runtime.start("node",{QFINDTESTDATA("attack-options-mips.cjs"),candidate});
            QVERIFY(runtime.waitForFinished(60000));QVERIFY2(runtime.exitCode()==0,runtime.readAllStandardError().constData());
            qInfo().noquote()<<runtime.readAllStandardOutput();
            data.apply(retail);QCOMPARE(data.entries().size(),159);QVERIFY(data.entry(45).spellLinks.isEmpty());
            qInfo().noquote()<<"Attack options candidate:"<<output.path();
        } catch(const std::exception &e) {QFAIL(e.what());}
    }
    void battleNamesExportAndReopen() {
        try {
            const auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if(tools.isEmpty())QSKIP("Private compiler required");
            auto project=Project::fromRom(source());project.loadAttacks();
            auto data=*project.attacks();
            const auto report=qEnvironmentVariable("LORDLY_ATTACK_PROJECT");
            if(!report.isEmpty()) {
                // The report can use another base ROM. Exercise only its exact
                // attack definitions on the known test ROM; never rewrite it.
                QFile file(report);QVERIFY(file.open(QIODevice::ReadOnly));
                data.apply(QJsonDocument::fromJson(file.readAll()).object()["attacks"].toObject());
            } else {
                for(int donor : {87,88,89,90}) data.duplicate(donor,donor<89?"Exorcism":"Mass Exorcism");
            }
            project.setAttacks(data.json());
            QTemporaryDir output(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).absoluteDir().filePath("Test ROMs/native-attack-names-XXXXXX"));
            QVERIFY(output.isValid());output.setAutoRemove(false);
            const auto built=rebuild(project,tools,output.path());
            const auto candidate=output.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName());
            exportRom(project,built,candidate);
            auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadAttacks();
            QCOMPARE(reopened.attacks()->json(),data.json());
            QProcess runtime;runtime.start("node",{QFINDTESTDATA("attack-names-mips.cjs"),candidate});
            QVERIFY(runtime.waitForFinished(60000));QVERIFY2(runtime.exitCode()==0,runtime.readAllStandardError().constData());
            qInfo().noquote()<<runtime.readAllStandardOutput();
            qInfo().noquote()<<"Battle name candidate:"<<output.path();
        } catch(const std::exception &e) {QFAIL(e.what());}
    }
    void playbackPipeline() {
        try {
            const auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if(tools.isEmpty())QSKIP("Private compiler required");
            auto project=Project::fromRom(source());project.loadAttacks();project.loadTools();
            auto attacks=*project.attacks();QCOMPARE(attacks.duplicate(55,"Playback fixture"),159);
            QCOMPARE(attacks.duplicate(55,"Following action"),160);
            const int copied=attacks.duplicate(55,"Copied effect playback");
            QCOMPARE(copied,161);
            auto edited=attacks.entry(copied);
            edited.visual.nativeEffect=101;
            edited.visual.hue=120;
            attacks.set(copied,edited);
            const int imported=attacks.duplicate(55,"Imported effect playback");
            QCOMPARE(imported,162);
            auto animated=attacks.entry(imported);
            QImage pixels(8,8,QImage::Format_RGBA8888);
            pixels.fill(Qt::red);
            animated.visual.frames=SpriteAsset::fromImage("Impact",pixels);
            animated.visual.frames->frames[0].ticks=4;
            animated.visual.delay=0;
            attacks.set(imported,animated);
            project.setAttacks(attacks.json());
            auto options=project.tools()->json();options["highAttackStreamSplit"]=true;project.setTools(options);
            QTemporaryDir output(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).absoluteDir().filePath("Test ROMs/native-playback-XXXXXX"));
            QVERIFY(output.isValid());output.setAutoRemove(false);
            const auto built=rebuild(project,tools,output.path());
            const auto candidate=output.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName());
            exportRom(project,built,candidate);
            qInfo().noquote()<<"Playback pipeline candidate:"<<candidate;
            auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadAttacks();
            QCOMPARE(reopened.attacks()->json(),attacks.json());
            QProcess runner;runner.start("node",{QFINDTESTDATA("attack-playback-mips.cjs"),candidate});
            QVERIFY(runner.waitForFinished(120000));
            qInfo().noquote()<<runner.readAllStandardOutput();
            QVERIFY2(runner.exitCode()==0,runner.readAllStandardError().constData());
            runner.start("node",{QFINDTESTDATA("battle-native-playback-mips.cjs"),candidate});
            QVERIFY(runner.waitForFinished(180000));
            qInfo().noquote()<<runner.readAllStandardOutput();
            QVERIFY2(runner.exitCode()==0,runner.readAllStandardError().constData());
        } catch(const std::exception &e) {QFAIL(e.what());}
    }
    void repeatedStrikesExportAndReopen_data() {
        QTest::addColumn<bool>("highAttack");
        QTest::addColumn<bool>("expandedArmy");
        QTest::newRow("ordinary-buffer") << false << false;
        QTest::newRow("high-attack-buffer") << true << false;
        QTest::newRow("expanded-army") << false << true;
        QTest::newRow("expanded-army-high-attack") << true << true;
    }
    void repeatedStrikesExportAndReopen() {
        QFETCH(bool,highAttack);
        QFETCH(bool,expandedArmy);
        try {
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty()) QSKIP("Private compiler required");
            auto project = Project::fromRom(source()); project.loadAttacks();
            project.loadTools();auto settings=project.tools()->json();
            settings["highAttackStreamSplit"]=highAttack;project.setTools(settings);
            if (expandedArmy) {
                project.loadScenarios(); auto scenarios = *project.scenarios();
                const auto classes = project.classData();
                QVector<CustomMember> members;
                for (int cell=0;cell<5;++cell) members.append({5,0,8-cell,{0,0}});
                for (int row=0;row<scenarios.scenario(1).squads.size();++row)
                    scenarios.setSquad(1,row,editSquadMembers(scenarios.scenario(1).squads[row],members),classes);
                while (scenarios.scenario(1).squads.size()<30) scenarios.addSquad(1,0,classes);
                project.setScenarios(scenarios.json()); QVERIFY(project.expandedArmies());
            }
            auto data = *project.attacks();
            const auto original = data.json();
            const int spell = data.duplicate(55, "Repeated spell");
            const int melee = data.duplicate(4, "Repeated melee");
            for (const auto [id, count] : {qMakePair(spell,18),qMakePair(melee,10),qMakePair(76,3)}) {
                auto attack = data.entry(id); attack.targetPattern = 11; attack.strikeCount = count;
                attack.targetSide = 1; attack.playback = id == melee ? 1 : 0; data.set(id, attack);
            }
            for (int count : {0,19,256}) {
                auto invalid = data.entry(spell); invalid.strikeCount = count;
                QVERIFY_THROWS_EXCEPTION(Error, data.set(spell, invalid));
            }
            // Projects predating this field retain the native six-strike default.
            auto legacy = original; auto rows = legacy["entries"].toArray();
            for (int i=0;i<rows.size();++i) { auto row=rows[i].toObject();row.remove("strikeCount");rows[i]=row; }
            legacy["entries"] = rows;
            auto old = AttackData::loadRetail(source()); old.apply(legacy); QCOMPARE(old.json(), original);
            project.setAttacks(data.json());
            QTemporaryDir output(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).dir()
                                     .filePath("Test ROMs/native-repeated-strikes-XXXXXX"));
            // The battle playback owner is also used by custom encounter rounds.
            project.loadEncounters();
            auto encounters = *project.encounters();
            encounters.setCustomSquad(CustomSquad{},project.records(Table::Classes).size());
            project.setEncounters(encounters.json());
            QVERIFY(output.isValid()); output.setAutoRemove(false);
            const auto folder=output.filePath("project"), archive=output.filePath("strikes.lcproj");
            project.saveFolder(folder); project.saveArchive(archive);
            for (const auto &path : {folder,archive}) {
                auto restored=Project::fromRom(source());restored.applyManifest(Project::readProject(path));
                QCOMPARE(restored.attacks()->json(), data.json());
            }
            const auto built=rebuild(project,tools,output.path());
            auto checksum=built.image;Rom::updateChecksum(checksum);QCOMPARE(checksum,built.image);
            const auto candidate=output.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName());
            exportRom(project,built,candidate);
            auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadAttacks();
            QCOMPARE(reopened.attacks()->json(),data.json());
            QCOMPARE(reopened.expandedArmies(),expandedArmy);
            reopened.loadTools();QVERIFY(reopened.tools()->highAttack().available());
            QCOMPARE(reopened.tools()->highAttack().enabled(),highAttack);
            reopened.loadEncounters();
            QCOMPARE(reopened.encounters()->customSquads().json(),encounters.customSquads().json());
            verifyCustomCallers(reopened.rom(),CustomPlacement::read(reopened.rom()));
            QCOMPARE(rebuild(reopened,tools,output.path()).image,built.image);
            QProcess runner;
            runner.start("node",{QFINDTESTDATA("attack-strike-playback-mips.cjs"),candidate});
            QVERIFY(runner.waitForFinished(60000));
            QVERIFY2(runner.exitCode()==0,runner.readAllStandardError().constData());
            qInfo().noquote()<<runner.readAllStandardOutput();
            if (expandedArmy) {
                runner.start("node",{QFINDTESTDATA("attack-capacity-mips.cjs"),candidate});
                QVERIFY(runner.waitForFinished(60000));
                QVERIFY2(runner.exitCode()==0,runner.readAllStandardError().constData());
                qInfo().noquote()<<runner.readAllStandardOutput();
                const auto capacity = CapacityPlacement::read(Rom::open(candidate)); QVERIFY(capacity);
                auto foreign = removeAttackReferences(Rom::open(candidate));
                // A damaged recorded capacity word must never be normalized away.
                foreign[0x22f624] ^= 1;
                const auto placement = AttackPlacement::read(reopened.rom()); QVERIFY(placement);
                QVERIFY_THROWS_EXCEPTION(Error,installAttackReferences(Rom::fromBytes(foreign),*placement));
            }
            if(highAttack) {
                runner.start("node",{QFINDTESTDATA("attack-high-buffer-mips.cjs"),candidate});
                QVERIFY(runner.waitForFinished(60000));
                QVERIFY2(runner.exitCode()==0,runner.readAllStandardError().constData());
                qInfo().noquote()<<runner.readAllStandardOutput();
                const auto placement=AttackPlacement::read(reopened.rom());QVERIFY(placement);
                auto foreign=removeAttackReferences(reopened.rom());
                foreign[0x21ec4c]^=1;
                QVERIFY_THROWS_EXCEPTION(Error,installAttackReferences(Rom::fromBytes(foreign),*placement));
                foreign=removeAttackReferences(reopened.rom());foreign[0x21ed00]^=1;
                QVERIFY_THROWS_EXCEPTION(Error,installAttackReferences(Rom::fromBytes(foreign),*placement));
                settings["highAttackStreamSplit"]=false;reopened.setTools(settings);
                auto disabled=Project::fromRom(Rom::fromBytes(rebuild(reopened,tools,output.path()).image));
                disabled.loadTools();QVERIFY(disabled.tools()->highAttack().available());
                QVERIFY(!disabled.tools()->highAttack().enabled());
            }
            qInfo().noquote()<<"Repeated strikes candidate:"<<candidate;
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void targetingExportAndReopen() {
        try {
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty()) QSKIP("Private compiler required");
            auto project = Project::fromRom(source());
            project.loadAttacks();
            auto attacks = *project.attacks();
            const int id = attacks.duplicate(124, "Liquification");
            QCOMPARE(id, 159);
            auto attack = attacks.entry(id);
            attack.targetPattern = 7;
            attack.status = 8;
            attack.chance = 25;
            attack.afterEffect = 0;
            auto dragon = project.record(Table::Classes, 65).bytes;
            put8(dragon, 55, id);
            put8(dragon, 56, 1);
            project.replaceBytes(Table::Classes, 65, dragon);
            QTemporaryDir output(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM"))
                                     .absoluteDir().filePath("Test ROMs/native-attack-targets-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            // Each side is retained through both project and compiled-ROM reopening.
            for (int side : {3, 2, 1}) {
                attack.targetSide = side;
                attacks.set(id, attack);
                project.setAttacks(attacks.json());
                const auto archive = output.filePath(QString("side-%1.lcproject").arg(side));
                project.saveArchive(archive);
                auto restored = Project::fromRom(source());
                restored.applyManifest(Project::readProject(archive));
                QCOMPARE(restored.attacks()->entry(id), attack);
                const auto built = rebuild(restored, tools, output.path());
                auto checksum = built.image;
                Rom::updateChecksum(checksum);
                QCOMPARE(checksum, built.image);
                const auto reopened = Project::fromRom(Rom::fromBytes(built.image));
                QCOMPARE(reopened.attacks()->entry(id), attack);
                QCOMPARE(u8(reopened.record(Table::Classes, 65).bytes, 55), quint8(159));
                QCOMPARE(u8(reopened.record(Table::Classes, 65).bytes, 56), quint8(1));
                exportRom(restored, built,
                          output.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName()));
            }
            qInfo().noquote() << "Attack targeting candidate:" << output.path();
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void projectAndRelocatedRecordExport() {
        try {
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty())
                QSKIP("Private compiler required");
            auto project = Project::fromRom(source());
            project.loadAttacks();
            const auto before = project.manifest();
            auto data = *project.attacks();
            data.rename(55, "Independent Lightning", false);
            const int copy = data.duplicate(4, "Copied Slash");
            auto a = data.entry(copy);
            put8(a.data, 1, 2);
            data.set(copy, a);
            for (int id = 160; id <= 254; ++id)
                QCOMPARE(data.duplicate(id % 2 ? 55 : 4, QString("Independent attack %1").arg(id)),
                         id);
            a = data.entry(254);
            a.targetPattern = 12;
            a.targetSide = 2;
            a.targetMask = 0x155;
            a.playback = 1;
            a.powerPercent = 175;
            a.status = 10;
            a.condition = 0;
            a.chance = 37;
            a.afterEffect = 2;
            a.afterBasis = 3;
            a.afterPercent = 12;
            put8(a.data, 10, 0x9a);
            put8(a.data, 11, 0xe7);
            data.set(254, a);
            project.setAttacks(data.json());
            const auto originalBody = project.bodyAnimationData();
            auto overrides = *project.animationOverrides();
            overrides.setEntries({QJsonObject{{"classId", 2},
                                              {"actionId", 254},
                                              {"normalSelector", 40},
                                              {"blockedSelector", 41}}},
                                 project.rom(), project.classData(), *project.animationRoutes());
            project.setAnimationOverrides(overrides.json());
            auto copies=*project.animationSequences();
            copies.create(2,0,*project.animationRoutes());
            project.setAnimationSequences(copies.json());
            const int privatePose = project.assignPrivateAttack(2, 0, 254, false, 40);
            QVERIFY(privatePose > 40);
            const auto overrideEntries = project.animationOverrides()->entries();
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 254, 0, 0), privatePose);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 254, 2, 0), 41);
            QCOMPARE(project.bodyAnimationData()->selector(2, 2, 159, 0, 0),
                     originalBody->selector(2, 2, 4, 0, 0));
            const auto originalClass = project.record(Table::Classes, 2).bytes;
            auto classBytes = originalClass;
            put8(classBytes, 55, 254);
            put8(classBytes, 56, 3);
            put8(classBytes, 57, 159);
            put8(classBytes, 58, 2);
            project.replaceBytes(Table::Classes, 2, classBytes);
            QCOMPARE(project.catalog()["ACTION_NAMES"].toObject()[QString::number(copy)].toString(),
                     QString("Copied Slash"));
            QVERIFY(project.changedRecords() > 0);
            const auto after = project.manifest();
            project.applyManifest(before);
            QCOMPARE(project.attacks()->json(), AttackData::loadRetail(source()).json());
            project.applyManifest(after);
            QCOMPARE(project.attacks()->json(), data.json());
            QTemporaryDir output(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM"))
                                     .dir()
                                     .filePath("Test ROMs/native-attacks-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto folder = output.filePath("project"),
                       archive = output.filePath("attacks.lcproj");
            project.saveFolder(folder);
            project.saveArchive(archive);
            for (const auto &path : {folder, archive}) {
                auto restored = Project::fromRom(source());
                restored.applyManifest(Project::readProject(path));
                QCOMPARE(restored.attacks()->json(), data.json());
            }
            for (int pass = 0; pass < 2; ++pass) {
                const auto built = rebuild(project, tools, output.path());
                const auto placed = AttackPlacement::read(Rom::fromBytes(built.image));
                QVERIFY(placed);
                QCOMPARE(placed->count, 255);
                if (!pass) {
                    auto unfamiliar = removeAttackReferences(Rom::fromBytes(built.image));
                    unfamiliar[0x22d16c] = char(quint8(unfamiliar[0x22d16c]) ^ 1);
                    QVERIFY_THROWS_EXCEPTION(
                        Error, installAttackReferences(Rom::fromBytes(unfamiliar), *placed));
                    unfamiliar = removeAttackReferences(Rom::fromBytes(built.image));
                    unfamiliar[0x22b214] = char(quint8(unfamiliar[0x22b214]) ^ 1);
                    QVERIFY_THROWS_EXCEPTION(
                        Error, installAttackReferences(Rom::fromBytes(unfamiliar), *placed));
                    // Separate-strike mode still preserves an existing physical-body patch.
                    put8(unfamiliar, placed->romOffset + AttackData::settingsOffset + 254 * 32 + 13, 0);
                    const auto retained = installAttackReferences(Rom::fromBytes(unfamiliar), *placed);
                    QCOMPARE(retained[0x22b214], unfamiliar[0x22b214]);
                }
                const auto namePointer =
                    u32(built.image, placed->romOffset + AttackData::recordOffset + copy * 16);
                QVERIFY(namePointer >= placed->address &&
                        namePointer < placed->address + placed->size);
                QCOMPARE(
                    u8(built.image, placed->romOffset + AttackData::recordOffset + copy * 16 + 5),
                    quint8(2));
                auto checksum = built.image;
                Rom::updateChecksum(checksum);
                QCOMPARE(checksum, built.image);
                exportRom(
                    project, built,
                    output.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName()));
                project = Project::fromRom(Rom::fromBytes(built.image));
                QVERIFY(project.attacks());
                QCOMPARE(project.attacks()->json(), data.json());
                QCOMPARE(project.record(Table::Classes, 2).bytes.sliced(4),
                         classBytes.sliced(4)); // Name pointer relocates.
                QVERIFY(!project.attacks()->modified());
                const auto body = project.bodyAnimationData();
                QCOMPARE(project.animationOverrides()->entries(), overrideEntries);
                QCOMPARE(body->selector(2, 2, 254, 0, 0), privatePose);
                QVERIFY(body->descriptor(2, 2, 0, 0));
                QCOMPARE(body->selector(2, 2, 254, 2, 0), 41);
                QCOMPARE(body->selector(2, 2, 159, 0, 0), originalBody->selector(2, 2, 4, 0, 0));
            }
            qInfo().noquote() << "Attack data relocation candidate:" << output.path();
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(AttackDataTests)
#include "attack_data_tests.moc"
