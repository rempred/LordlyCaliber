#include "engine/actor_pose.h"
#include "engine/actor_renderer.h"
#include "engine/animation_descriptor.h"
#include "engine/combat_artwork.h"
#include "engine/scene_actors.h"
#include <QtTest>
using namespace lordly;
class AnimationDescriptorTests : public QObject {
    Q_OBJECT
  private slots:
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
