#include "engine/actor_pose.h"
#include "engine/actor_renderer.h"
#include "engine/animation_descriptor.h"
#include "engine/combat_artwork.h"
#include "engine/scene_actors.h"
#include "engine/sprite_object.h"
#include <QJsonArray>
#include <QtTest>
using namespace lordly;
class AnimationDescriptorTests : public QObject {
    Q_OBJECT
  private slots:
    void appendedSpriteResources() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const RasterImage input{
                3, 2, QByteArray::fromHex("ff0000ff00ff00800000ff0001020344abcdefc010203001")};
            const auto sprite = SpriteObject::fromImage(input);
            QCOMPARE(sprite.width(), 3);
            QCOMPARE(sprite.height(), 2);
            QCOMPARE(sprite.pixels(0).color, QByteArray::fromHex("f80107c1003f0001ae7b110d"));
            QCOMPARE(sprite.pixels(0).alpha, QByteArray::fromHex("ff800044c001"));
            QCOMPARE(sprite.frame(0).image.rgba,
                     QByteArray::fromHex("ff0000ff00ff00800000ff0000000044adceefc010213101"));
            QCOMPARE(sprite.source().size(), 48);
            QCOMPARE(sprite.frame(99).image.rgba, sprite.frame(0).image.rgba);
            QVERIFY_THROWS_EXCEPTION(Error, SpriteObject::fromImage({3, 3, input.rgba}));
            const auto rom = Rom::open(path);
            const auto original = AnimationDescriptor::load(rom, 0x3d7e3c);
            auto descriptor = original;
            auto palette = original.controls()[3].left(512);
            palette[0] ^= 1;
            const int art = descriptor.appendSprite(sprite.source(), 0, palette);
            QCOMPARE(art, 107);
            QCOMPARE(descriptor.sprite(art), sprite.source());
            QCOMPARE(descriptor.controls()[3], original.controls()[3] + palette);
            auto config = descriptor.controls()[2];
            QCOMPARE(u32(config, 0), quint32(108));
            QCOMPARE(u32(config, 28), u32(original.controls()[2], 28) + 1);
            QCOMPARE(u8(config, u32(config, 28) + art), original.controls()[3].size() / 512);
            config.remove(u32(config, 28) + art, 1);
            config.remove(32 + art, 1);
            put32(config, 0, 107);
            put32(config, 28, u32(original.controls()[2], 28));
            QCOMPARE(config, original.controls()[2]);
            QByteArray row(18, 0);
            put16(row, 0, 1);
            put16(row, 2, quint16(art));
            put16(row, 4, quint16(-5));
            put16(row, 6, quint16(-7));
            put16(row, 8, 3);
            put16(row, 10, 2);
            put16(row, 14, 1024);
            put16(row, 16, 1024);
            const int frame = descriptor.appendFrame(row);
            auto poses = descriptor.poses();
            poses.setFrame(40, 0, frame, 2);
            descriptor.setPoses(poses);
            const auto artPreview = ActorArt::loadBody(rom, 2, 2, 0, 0).withDescriptor(descriptor);
            const auto layers = artPreview.bodyFrame(rom, frame, artPreview.defaultAppearance(rom));
            QCOMPARE(layers.size(), 1);
            QCOMPARE(layers.first().metadata.origin, QPoint(-5, -7));
            QCOMPARE(layers.first().pixels.image.rgba, sprite.frame(0).image.rgba);
            QCOMPARE(AnimationDescriptor::fromJson(rom, descriptor.json()).identity(),
                     descriptor.identity());
            const auto before = descriptor.identity();
            QVERIFY_THROWS_EXCEPTION(Error, descriptor.appendSprite(sprite.source(), 3, palette));
            QVERIFY_THROWS_EXCEPTION(Error,
                                     descriptor.appendSprite(sprite.source(), 0, palette.left(32)));
            QCOMPARE(descriptor.identity(), before);
            auto invalid = descriptor.json();
            auto extras = invalid["extraSprites"].toArray();
            auto extra = extras[0].toObject();
            extra["policy"] = 0.5;
            extras[0] = extra;
            invalid["extraSprites"] = extras;
            QVERIFY_THROWS_EXCEPTION(Error, AnimationDescriptor::fromJson(rom, invalid));
            QCOMPARE(descriptor.appendSprite(sprite.source(), 1, palette), 108);
            QCOMPARE(descriptor.controls()[3].size(), original.controls()[3].size() + 512);
            QCOMPARE(AnimationDescriptor::fromJson(rom, descriptor.json()).identity(),
                     descriptor.identity());
            const auto altered =
                sprite.rebuild({{0, {sprite.pixels(0).color, QByteArray(6, char(9))}}});
            descriptor.setSprite(108, altered);
            QCOMPARE(AnimationDescriptor::fromJson(rom, descriptor.json()).sprite(108), altered);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void frameMetadataIsolation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto original = AnimationDescriptor::load(rom, 0x3d7e3c);
            auto edited = original;
            auto row = edited.frame(17);
            const int x = qint16(u16(row, 4));
            put16(row, 4, quint16(x + 11));
            const int token = edited.appendFrame(row);
            QCOMPARE(token, 32);
            QCOMPARE(edited.frame(token), row);
            for (int frame = 0; frame < original.frameCount(); ++frame)
                QCOMPARE(edited.frame(frame), original.frame(frame));
            auto poses = edited.poses();
            poses.setFrame(40, 0, token, 6);
            edited.setPoses(poses);
            const auto body = ActorArt::loadBody(rom, 2, 2, 0, 0).withDescriptor(edited);
            QCOMPARE(body.layers(token).first().origin.x(), x + 11);
            const auto saved = edited.json();
            QCOMPARE(AnimationDescriptor::fromJson(rom, saved).identity(), edited.identity());
            auto legacy = original.json();
            legacy.remove("metadata");
            QCOMPARE(AnimationDescriptor::fromJson(rom, legacy).identity(), original.identity());
            auto invalid = saved;
            auto bytes = edited.controls()[0];
            bytes[edited.frameCount() * 4 + 2] ^= 1;
            invalid["metadata"] = QString::fromLatin1(bytes.toBase64());
            QVERIFY_THROWS_EXCEPTION(Error, AnimationDescriptor::fromJson(rom, invalid));
            const auto retained = edited.identity();
            auto bad = row;
            put16(bad, 2, 65535);
            QVERIFY_THROWS_EXCEPTION(Error, edited.appendFrame(bad));
            QCOMPARE(edited.identity(), retained);
            while (edited.frameCount() <= 256)
                edited.appendFrame(row);
            poses = edited.poses();
            poses.setFrame(40, 0, 256, 2);
            edited.setPoses(poses);
            QCOMPARE(poses.effective(40).records.first().opcode, 21);
            ActorPosePlayer player(rom, ActorArt::loadBody(rom, 2, 2, 0, 0).withDescriptor(edited),
                                   40);
            player.setLinkedObjectKnownAbsent(true);
            QCOMPARE(player.advance(), int(LORDLY_POSE_OK));
            QCOMPARE(player.actor().frame, 256);
            QCOMPARE(player.art()
                         .bodyFrame(rom, 256, player.art().defaultAppearance(rom))
                         .first()
                         .metadata.origin.x(),
                     x + 11);
            QCOMPARE(AnimationDescriptor::fromJson(rom, edited.json()).identity(),
                     edited.identity());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void privateTimingAndPixels() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto body = ActorArt::loadBody(rom, 2, 2, 0, 0);
            QCOMPARE(body.descriptorKey(), quint32(0x3d7e3c));
            auto descriptor = AnimationDescriptor::load(rom, body.descriptorKey());
            const auto original = descriptor;
            QCOMPARE(descriptor.frameCount(), 32);
            QCOMPARE(descriptor.members().size(), 111);
            QCOMPARE(descriptor.controls()[0].size(), 2784);
            auto poses = descriptor.poses();
            QCOMPARE(poses.effective(40).records.first().operands, QByteArray::fromHex("1106"));
            poses.setFrame(40, 0, 17, 2);
            descriptor.setPoses(poses);
            ActorPosePlayer before(rom, body, 40), after(rom, body.withDescriptor(descriptor), 40);
            before.setLinkedObjectKnownAbsent(true);
            after.setLinkedObjectKnownAbsent(true);
            for (int i = 0; i < 2; ++i) {
                QCOMPARE(before.advance(), int(LORDLY_POSE_OK));
                QCOMPARE(after.advance(), int(LORDLY_POSE_OK));
            }
            QCOMPARE(before.actor().frame, 17);
            QCOMPARE(after.actor().frame, 18);
            QCOMPARE(original.poses().effective(40).records.first().operands,
                     QByteArray::fromHex("1106"));
            for (int i : {0, 2, 3})
                QCOMPARE(descriptor.controls()[i], original.controls()[i]);
            const auto valid = descriptor.identity();
            poses.setFrame(40, 0, 32, 2);
            QVERIFY_THROWS_EXCEPTION(Error, descriptor.setPoses(poses));
            QVERIFY_THROWS_EXCEPTION(Error, descriptor.setSprite(0, QByteArray::fromHex("555400")));
            QCOMPARE(descriptor.identity(), valid);

            auto artwork = CombatArtwork::load(rom);
            const auto appearance = body.defaultAppearance(rom);
            const auto layers = body.bodyLayers(rom, 17, appearance);
            QVERIFY(!layers.isEmpty());
            const auto selected = layers.first();
            const auto id = CombatArtwork::identity(body.descriptorKey(), selected.metadata.art);
            const auto sprite = artwork.original(id);
            const int child = selected.child < sprite.children().size() ? selected.child : 0;
            auto pixels = artwork.pixels(id, child);
            QVERIFY(!pixels.alpha.isEmpty());
            pixels.alpha.fill(0);
            artwork.setPixels(id, child, pixels);
            const auto captured = artwork.previewEdits().value(body.descriptorKey());
            auto snapshot = AnimationDescriptor::load(rom, body.descriptorKey(), captured);
            const auto privateArt = body.withDescriptor(snapshot);
            const auto sourceFrame = body.bodyFrame(rom, 17, appearance);
            const auto privateFrame = privateArt.bodyFrame(rom, 17, appearance);
            QVERIFY(privateFrame.first().pixels.image.rgba !=
                    sourceFrame.first().pixels.image.rgba);
            // The later global edit restores opaque pixels. Only the shared body sees it.
            pixels.alpha.fill(sprite.alphaFormat() ? 255 : 15);
            artwork.setPixels(id, child, pixels);
            const auto later = artwork.previewEdits().value(body.descriptorKey());
            QCOMPARE(privateArt.bodyFrame(rom, 17, appearance, 0, later).first().pixels.image.rgba,
                     privateFrame.first().pixels.image.rgba);
            QVERIFY(body.bodyFrame(rom, 17, appearance, 0, later).first().pixels.image.rgba !=
                    privateFrame.first().pixels.image.rgba);
            QCOMPARE(body.bodyFrame(rom, 17, appearance).first().pixels.image.rgba,
                     sourceFrame.first().pixels.image.rgba);
            const auto direct =
                ActorArt::loadDirect(rom, body.bodyHandle()).withDescriptor(snapshot);
            QCOMPARE(direct.frame(rom, 17, child, 0, {}, later).first().pixels.image.rgba,
                     privateFrame.first().pixels.image.rgba);

            SceneActors actors(rom, 2, 255, [](int, float, float) { return 0.f; });
            actors.supplyBodyRows(QVector<QByteArray>(28, QByteArray(248, 0)));
            int value = 0;
            QVERIFY(actors.service(0x14, {0, 30, 0, 0, 0, 0, 0, 0, 0}, value));
            QVERIFY(actors.service(0x2a, {0, 2, 0, 0, 0, 2}, value));
            const auto camera = SceneCameras::freshModeTwo(rom).actor();
            actors.prepareModeTwoMatrices(camera, {{0, 0, 0}, {{200, -200, 50}, {-200, 200, 75}}});
            auto record = actors.actor(0)->record();
            put32(record, 0xf8, 17);
            ActorRenderer renderer(rom);
            const auto draw = [&](const ActorArt &art) {
                QImage image(320, 240, QImage::Format_ARGB32);
                image.fill(Qt::transparent);
                ActorRenderer::paint(image, renderer.projectRecord(art, record, false, camera,
                                                                   {255, 255, 255}, false, 0, 255,
                                                                   nullptr, nullptr, appearance));
                return image;
            };
            const auto hidden = draw(privateArt);
            snapshot.setSprite(selected.metadata.art, original.sprite(selected.metadata.art));
            const auto visible = draw(body.withDescriptor(snapshot));
            QVERIFY(hidden != visible);
            QCOMPARE(draw(privateArt), hidden);
            QCOMPARE(draw(body), visible);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_MAIN(AnimationDescriptorTests)
#include "animation_descriptor_tests.moc"
