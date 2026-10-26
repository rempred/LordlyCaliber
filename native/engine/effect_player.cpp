#include "engine/effect_player.h"
#include "engine/attack_data.h"
#include "engine/class_data.h"
#include <algorithm>
extern "C" int lc_visual_image_commands(unsigned int *, unsigned int, int, int, int, int, int);
namespace lordly {
EffectPlayer::EffectPlayer(const Rom &rom) : rom_(rom.bytes()) {
    rom.requireSupported();
}
int EffectPlayer::retailEffect(int id) {
    // Byte-proven func_0020144C. Templates must resolve before this reader.
    if (quint8(id - 17) < 28)
        return quint8(id + 80);
    if (quint8(id + 110) < 8)
        return quint8(id + 235);
    if (quint8(id - 55) < 36)
        return quint8(id + 201);
    if (quint8(id - 91) < 54)
        return quint8(id + 204);
    if (quint8(id + 102) < 3)
        return quint8(id + 138);
    return 1;
}
void EffectPlayer::start(int effect, const Scene &scene) {
    if (effect < -1 || effect > 254 || (effect > 132 && effect < 136))
        throw Error("This entry is a battle transition, not an attack effect");
    if (scene.sourceClass < 0 || scene.sourceClass > 255 || scene.targetClass < 0 ||
        scene.targetClass > 255 || scene.sourceHeight < 1 || scene.sourceHeight > 1024 ||
        scene.targetHeight < 1 || scene.targetHeight > 1024 || scene.targetCount < 1 ||
        scene.targetCount > 5)
        throw Error("Invalid effect preview actors");
    scene_ = scene;
    tick_ = 0;
    heap_ = 0x80500000;
    visual_ = {};
    delay_ = frame_ = hold_ = 0;
    frameDraw_ = 0;
    machine_ = std::make_unique<EffectMachine>();
    auto &m = *machine_;
    auto load = [&](quint32 ram, quint32 begin, quint32 end, bool code = true) {
        if (end < begin || end > quint32(rom_.size()))
            throw Error("Effect source range is outside the ROM");
        m.mount(ram, rom_.mid(begin, end - begin), code);
    };
    load(0x80070000, 0x400, 0x2f000);
    // Qualified resident rodata segment, not the early-boot ROM delta.
    load(0x800ae890, 0x3ec90, 0x3ef00, false);
    load(0x800aa750, 0x3ab50, 0x3ab54, false);
    load(0x8016af80, 0x40e80, 0x66e10);
    for (int id : {2, 4, 10, 11, 12, 13, 14}) {
        const auto at = 0x387c0 + id * 40;
        load(u32(rom_, at), u32(rom_, at + 8), u32(rom_, at + 12));
    }
    const auto placement = ClassPlacement::read(Rom::fromBytes(rom_));
    if (placement)
        m.mount(
            placement->layout.indexBase,
            rom_.mid(placement->dataRomOffset, placement->layout.end - placement->layout.indexBase),
            true);
    if (effect >= 136) {
        const auto attacks = AttackPlacement::read(Rom::fromBytes(rom_));
        bool assigned = false;
        if (attacks)
            for (int i = 1; i < attacks->count; ++i)
                assigned |= m.byte(attacks->settings + i * 32 + 21) == effect;
        if (!assigned)
            throw Error("The exported effect slot is not assigned to an attack");
    }
    m.registers[29] = 0x807ff000;
    auto chain = [&](quint32 base, int count, int stride, int offset = 0) {
        for (int i = 0; i < count; ++i)
            m.half(base + i * stride + offset, quint16(i == count - 1 ? 65535 : i + 1));
    };
    m.word(0x80220d80, 256);
    m.word(0x80220d84, 0x80700000);
    m.word(0x80220d88, 0);
    m.word(0x80220d8c, 0);
    m.word(0x80220d90, 256);
    m.word(0x80220d94, 0x80700000 + 256 * 156);
    m.half(0x80220e8e, 0);
    chain(0x80700000, 256, 156);
    m.word(0x80220d68, 256);
    m.word(0x80220d6c, 0x80760000);
    m.half(0x80220d64, 0);
    m.word(0x80220d70, 0x807a0000);
    chain(0x80760000, 256, 148);
    m.half(0x80220da0, 0);
    m.word(0x80220da4, 256);
    m.word(0x80220da8, 0x80790000);
    chain(0x80790000, 256, 120);
    m.word(0x80220dac, 0x807a0400);
    m.half(0x80220d50, 0);
    m.word(0x80220d54, 256);
    m.word(0x80220d58, 0x807b0000);
    chain(0x807b0000, 256, 128);
    m.half(0x80220e10, 0);
    m.word(0x80220e14, 0x807c0000);
    chain(0x807c0000, 64, 16, 12);
    m.word(0x80220ca0, 256);
    m.word(0x80220ca4, 0);
    m.word(0x80220ca8, 0x80770000);
    m.word(0x80220cac, 16);
    m.word(0x80220cb0, 0xffffffff);
    chain(0x80770000, 256, 20, 16);
    m.word(0x80220f20, 0x80780000);
    for (quint32 base : {0x80220ee0U, 0x80220ff0U})
        for (int i = 0; i < 16; ++i)
            m.number(base + i * 4, i % 5 == 0 ? 1.f : 0.f);
    // Combined preview view/projection used by depth-aware background meshes.
    // Leaving its homogeneous row empty causes the native perspective divide
    // to divide by zero even though ordinary sprites still render.
    for (int i = 0; i < 16; ++i)
        m.number(0x80220e90 + i * 4, 0);
    m.number(0x80220e90, 1.f / 160);
    m.number(0x80220ea4, 1.f / 120);
    m.number(0x80220eb8, 1.f / 1000);
    m.number(0x80220ecc, 1);
    m.service(0x80070f30, [this] {
        auto &c = *machine_;
        const quint64 size = (quint64(c.registers[4]) + 15) & ~15ULL;
        if (size > 0x200000 || heap_ + size > 0x80600000)
            throw Error("Effect allocation exceeds its preview budget");
        c.registers[2] = heap_;
        heap_ += quint32(size);
    });
    // Retain native frees until restart; each effect has a bounded lifetime.
    m.service(0x800712c4, [] {});
    m.service(0x80089f80, [this] {
        auto &c = *machine_;
        const auto at = c.registers[4] & 0x0fffffff, size = c.registers[6];
        if (quint64(at) + size > quint64(rom_.size()))
            throw Error("Effect resource exceeds the source ROM");
        c.mount(c.registers[5], rom_.mid(at, size));
    });
    m.service(0x801ca100, [this] {
        auto &c = *machine_;
        c.registers[2] = c.registers[4] < 9 ? scene_.sourceHeight : scene_.targetHeight;
    });
    constexpr quint32 inputs = 0x80720000, battle = 0x80740000;
    for (int i = 0; i < 36; ++i)
        m.byte(inputs + i, 255);
    m.byte(inputs, 0);
    // Area effects use neighbouring actors for their visual choreography.
    // This neutral full formation never changes the attack's damage targets.
    for (int i = 0; i < scene.targetCount; ++i) {
        m.byte(inputs + 12 + i, quint8(scene.allies ? i : i + 9));
        m.byte(inputs + 24 + i, quint8(scene.allies ? i : i + 9));
    }
    m.word(0x801ce8bc, battle);
    m.word(0x801ce8c0, battle + 0x57e0);
    m.word(0x801d0810, battle + 0x7000);
    m.half(0x800e810e, 12);
    m.byte(0x800e9c12, 1);
    constexpr quint32 background = 0x807d0000;
    m.word(0x801d0770, background);
    m.half(background, 0x3634);
    m.byte(background + 2, 0);
    m.byte(background + 3, 2);
    m.half(background + 4, 320);
    m.half(background + 6, 240);
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x) {
            const int s = ((x >> 4) + (y >> 4)) & 1 ? 7 : 5;
            m.half(background + 8 + (y * 320 + x) * 2,
                   quint16((s << 11) | ((s + 2) << 6) | ((s + 3) << 1) | 1));
        }
    for (int i = 0; i < 18; ++i) {
        const auto a = battle + 0x1c4 + i * 0xf8;
        const int cell = i % 9, row = cell / 3, col = cell % 3;
        const bool occupied = cell < 5;
        const int classId = occupied ? (i < 9 ? scene.sourceClass : scene.targetClass) : 0;
        m.word(a + 0x48, classId);
        m.word(a + 0x4c, classId);
        m.word(a + 0x40, i < 9 ? 0x100 : 0);
        m.word(a + 0x54, i < 9 ? col : 2 - col);
        m.word(a + 0x58, i < 9 ? 2 - row : 6 + row);
        m.half(a + 0x20, occupied ? 100 : 0);
        m.half(a + 0x22, occupied ? 100 : 0);
        m.word(a + 0x7c, scene.attack);
        m.word(a + 0x5c, quint32(i < 9 ? -80 - row * 38 : 80 + row * 38));
        m.word(a + 0x60, 0);
        m.word(a + 0x64, quint32((col - 1) * 38));
    }
    if (effect >= 0) {
        m.registers[4] = quint32(effect);
        m.registers[5] = inputs;
        m.registers[6] = inputs + 12;
        m.registers[7] = inputs + 24;
        m.call(0x8020e850);
    }
    m.word(0x800e9ba0, 0x80600000);
}
void EffectPlayer::startVisual(const AttackVisual &visual, int inherited, const Scene &scene) {
    visual.validate();
    start(visual.frames ? -1 : visual.nativeEffect < 0 ? inherited : visual.nativeEffect, scene);
    visual_ = visual;
    delay_ = visual.delay;
    if (visual.frames) {
        auto &m = *machine_;
        m.service(0x80000600, [this] {
            auto &c = *machine_;
            std::array<unsigned int, 2048> commands{};
            const auto &asset = *visual_.frames;
            const int count = lc_visual_image_commands(commands.data(), 0x80500008,
                                                       asset.size.width(), asset.size.height(),
                                                       asset.anchor.x(), asset.anchor.y(), 2048);
            if (count < 0)
                throw Error("Imported effect drawing exceeds its command budget");
            auto at = c.word(0x800e9ba0);
            for (int i = 0; i < count; ++i) {
                c.word(at, commands[i]);
                at += 4;
            }
            c.word(0x800e9ba0, at);
        });
        m.registers[4] = 0x80000600;
        m.registers[5] = 1;
        m.call(0x8020e570);
        frameDraw_ = m.registers[2];
        m.number(frameDraw_ + 8, 1000);
        loadFrame();
    }
}
void EffectPlayer::loadFrame() {
    auto &m = *machine_;
    m.mount(0x80500000, AttackVisual::gameImage(visual_.frames->composite(frame_)));
    hold_ = visual_.frames->frames[frame_].ticks;
}
bool EffectPlayer::running() const {
    return machine_ && (delay_ > 0 || (visual_.frames ? frame_ < visual_.frames->frames.size()
                                                      : machine_->word(0x80220d88) != 0));
}
bool EffectPlayer::advance() {
    if (!running())
        return false;
    if (tick_ >= 1200)
        throw Error("Effect playback exceeds its preview duration");
    auto &m = *machine_;
    if (delay_ > 0) {
        --delay_;
        ++tick_;
        m.word(0x800e9ba0, 0x80600000);
        return true;
    }
    if (visual_.frames) {
        if (!hold_) {
            ++frame_;
            if (frame_ >= visual_.frames->frames.size())
                m.word(frameDraw_ + 12, 0);
            else
                loadFrame();
        }
        if (frame_ < visual_.frames->frames.size())
            --hold_;
    } else {
        m.call(0x8020d9fc);
        m.call(0x8020d3d4);
    }
    m.word(0x800e9ba0, 0x80600000);
    m.word(0x801969b0, 0x80680000);
    m.word(0x80197178, 0x806c0000);
    m.registers[4] = 0;
    m.call(0x8020e224);
    m.call(0x801b0300);
    m.registers[4] = 1;
    m.call(0x8020e3a0);
    if (m.word(0x800e9ba0) < 0x80600000 || m.word(0x800e9ba0) > 0x80680000 ||
        m.word(0x801969b0) > 0x806c0000 || m.word(0x80197178) > 0x80700000)
        throw Error("Effect drawing exceeds its preview allocation");
    ++tick_;
    return true;
}
EffectRaster::Result EffectPlayer::draw(QSize size, double zoom) const {
    if (!machine_)
        throw Error("No effect is selected");
    return EffectRaster::draw(machine_->memory, 0x80600000, machine_->word(0x800e9ba0), size, zoom);
}
} // namespace lordly
