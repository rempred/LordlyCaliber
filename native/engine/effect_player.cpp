#include "engine/effect_player.h"
#include "engine/attack_data.h"
#include "engine/actor_art.h"
#include "engine/class_data.h"
#include "engine/effect_resources.h"
#include "engine/accepted_effect_code.h"
#include "rebuild/effect_preview_runtime.h"
#include <algorithm>
extern "C" int lc_visual_image_commands_format(unsigned int *, unsigned int, int, int, int, int, int, int);
extern "C" int lc_visual_origins(int, const unsigned char *, const unsigned char *, const float *,
                                 unsigned int, unsigned int, const float *, const float *, int *);
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
        scene.targetClass > 255 || scene.sourceHeight < 0 || scene.sourceHeight > 1024 ||
        scene.targetHeight < 0 || scene.targetHeight > 1024 || scene.targetCount < 1 ||
        scene.targetCount > 5 || scene.strikeCount < 0 || scene.strikeCount > AttackData::maximumStrikes)
        throw Error("Invalid effect preview actors");
    scene_ = scene;
    if (!scene_.bodyAnimations)
        scene_.bodyAnimations = BodyAnimationData::load(Rom::fromBytes(rom_));
    const auto &classes = scene_.bodyAnimations->classes();
    if (scene_.sourceCurrentClass < 0)
        scene_.sourceCurrentClass = classes.initialCurrentClass(scene_.sourceClass);
    if (scene_.targetCurrentClass < 0)
        scene_.targetCurrentClass = classes.initialCurrentClass(scene_.targetClass);
    if (scene_.sourceCurrentClass >= classes.entries().size() || scene_.targetCurrentClass >= classes.entries().size())
        throw Error("Effect preview current class is outside the class table");
    tick_ = 0;
    heap_ = 0x80500000;
    visual_ = {};
    delay_ = frame_ = hold_ = 0;
    frameDraw_ = 0;
    allocations_.clear();
    resources_.clear();
    machine_ = std::make_unique<EffectMachine>();
    auto &m = *machine_;
    impacts_.clear();
    audio_.clear();
    pendingAudioCue_ = -1;
    // Observe native cue translation and dispatch, without replacing either
    // routine or fabricating queue acceptance, voice lengths or audio output.
    // Observe past the entry trampoline: muted requests never reach this point.
    m.observe(0x801f9e04, [this] { pendingAudioCue_ = int(machine_->registers[6]); });
    m.observe(0x801c5940, [this] {
        if (audio_.size() >= 4096) throw Error("Effect sound trace exceeds its cue budget");
        audio_.append({tick_, pendingAudioCue_, int(machine_->registers[5]),
                       int(machine_->registers[4]), pendingAudioCue_ >= 0});
        pendingAudioCue_ = -1;
    });
    // The visual sandbox records imported playback requests, just as it records
    // retail dispatch. Synthesis is auditioned separately by native libmus.
    for (quint32 address : {0x80081db4u,0x80081f7cu,0x8008202cu}) m.service(address,[]{});
    m.service(0x80083b24,[this] {
        auto &m=*machine_;auto bank=m.registers[4]-128;
        const auto length=m.word(bank+0x44);
        if(length>128000 || length%2 || bank<0x80430000 || quint64(bank)+256+length>0x80500000)
            throw Error("Imported sound lies outside its preview allocation");
        AudioCue c{tick_,-1,-2};c.pcm.resize(length);
        for(quint32 i=0;i<length;i+=2){c.pcm[i]=char(m.byte(bank+256+i+1));c.pcm[i+1]=char(m.byte(bank+256+i));}
        c.label="Imported sound";
        for(const auto &s:visual_.audio.sounds)if(s.pcm==c.pcm){c.label=s.name;break;}
        audio_.append(c);pendingAudioCue_=-1;
    });
    // The preview has no battle result consumer. Observe the native contact
    // notification, preserving the script's target and exact playback tick.
    m.service(0x801c893c, [this] {
        impacts_.append({tick_, int(machine_->registers[4]), int(machine_->registers[5])});
        machine_->registers[2] = 0;
    });
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
    installAcceptedEffectCode(m, rom_);
    const auto placement = ClassPlacement::read(Rom::fromBytes(rom_));
    if (placement)
        m.mount(
            placement->layout.indexBase,
            rom_.mid(placement->dataRomOffset, placement->layout.end - placement->layout.indexBase),
            true);
    m.registers[29] = 0x807ff000;
    auto chain = [&](quint32 base, int count, int stride, int offset = 0) {
        for (int i = 0; i < count; ++i)
            m.half(base + i * stride + offset, quint16(i == count - 1 ? 65535 : i + 1));
    };
    // Match the retail initializers, so a preview cannot hide pool exhaustion.
    m.word(0x80220d80, 24);
    m.word(0x80220d84, 0x80700000);
    m.word(0x80220d88, 0);
    m.word(0x80220d8c, 0);
    m.word(0x80220d90, 24);
    m.word(0x80220d94, 0x80700000 + 24 * 156);
    m.half(0x80220e8e, 0);
    chain(0x80700000, 24, 156);
    m.word(0x80220d68, 32);
    m.word(0x80220d6c, 0x80760000);
    m.half(0x80220d64, 0);
    m.word(0x80220d70, 0x807a0000);
    chain(0x80760000, 32, 148);
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
    m.word(0x80220ca0, 128);
    m.word(0x80220ca4, 0);
    m.word(0x80220ca8, 0x80770000);
    m.word(0x80220cac, 32);
    m.word(0x80220cb0, 0);
    chain(0x80770000, 128, 20, 16);
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
    // The isolated scene's camera matches the raster's default oblique view.
    // Exported effects use the actual game camera, established by func_0025F800.
    for (int i = 0; i < 16; ++i)
        m.number(0x80220e20 + i * 4, 0);
    m.number(0x80220e20, .75f);
    m.number(0x80220e34, .75f);
    m.number(0x80220e44, -.15f);
    m.number(0x80220e48, 1);
    m.number(0x80220e54, -43.2f);
    m.number(0x80220e5c, 1);
    m.service(0x80070f30, [this] {
        auto &c = *machine_;
        const quint64 size = (quint64(c.registers[4]) + 15) & ~15ULL;
        if (size > 0x200000 || heap_ + size > 0x80600000)
            throw Error("Effect allocation exceeds its preview budget");
        c.registers[2] = heap_;
        allocations_[heap_] = c.registers[4];
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
    // func_0020D590 receives an actor pointer, not its formation index. Its
    // result comes from the idle frame's artwork bounds, including orientation.
    m.service(0x801ca100, [this, heights = QMap<quint64, int>{}]() mutable {
        auto &c = *machine_;
        const auto actor = c.registers[4], flags = c.word(actor + 0x40);
        const int identity = int(c.word(actor + 0x48)), current = int(c.word(actor + 0x4c));
        const auto caster = c.word(0x801ce8bc) + 0x1c4 + (scene_.enemyCaster ? 0 : 9) * 0xf8;
        const int explicitHeight = actor == caster ? scene_.sourceHeight : scene_.targetHeight;
        if (identity == 0x101 || explicitHeight) {
            c.registers[2] = identity == 0x101 ? 38 : explicitHeight;
            return;
        }
        const int flagA = (flags >> 10) & 1, flagB = (flags >> 8) & 1;
        const quint64 key = (quint64(quint32(identity)) << 32) | (quint64(quint16(current)) << 2) |
                            (flagA << 1) | flagB;
        if (!heights.contains(key))
            heights.insert(key, ActorArt::loadBody(Rom::fromBytes(rom_), identity, current,
                                                  flagA, flagB, scene_.bodyAnimations).combatHeight());
        c.registers[2] = heights.value(key);
    });
    // Capture the unrelocated resource before the native consumer fixes its
    // internal pointers. The original loader and its cache remain in use.
    constexpr quint32 resourceLoader = 0x8020c380, resourceTrampoline = 0x80001000;
    QByteArray trampoline(16, 0);
    put32(trampoline, 0, m.word(resourceLoader));
    put32(trampoline, 4, m.word(resourceLoader + 4));
    put32(trampoline, 8, 0x08000000 | ((resourceLoader + 8) >> 2 & 0x3ffffff));
    m.mount(resourceTrampoline, trampoline, true);
    m.service(resourceLoader, [this] {
        auto &c = *machine_;
        const auto type = int(c.registers[4]), id = int(c.registers[5]);
        const auto returnAddress = c.registers[31];
        c.word(0x80001000, c.word(0x8020c380));
        c.word(0x80001004, c.word(0x8020c384));
        c.call(0x80001000);
        c.registers[31] = returnAddress;
        const auto pointer = c.registers[2], length = allocations_.value(pointer);
        if (length) {
            const auto bytes = c.memory.mid(c.physical(pointer, length), length);
            auto found = std::find_if(resources_.begin(), resources_.end(), [=](const Resource &r) {
                // Inline native preloaders can pass the same raw ID for two
                // independent edited owners. Retain both captured artworks.
                return r.type == type && r.id == id && r.bytes == bytes;
            });
            if (found == resources_.end()) resources_.append({type, id, bytes, pointer, {}});
            else found->address = pointer;
        }
    });
    constexpr quint32 inputs = 0x80720000, battle = 0x80740000;
    for (int i = 0; i < 60; ++i)
        m.byte(inputs + i, 255);
    const int sourceBase = scene.enemyCaster ? 0 : 9;
    const int targetBase = scene.allies ? sourceBase : 9 - sourceBase;
    m.byte(inputs, quint8(sourceBase));
    // Area effects use neighbouring actors for their visual choreography.
    // This neutral full formation never changes the attack's damage targets.
    const int targets = scene.strikeCount ? scene.strikeCount : scene.targetCount;
    for (int i = 0; i < targets; ++i) {
        m.byte(inputs + 20 + i, quint8(targetBase + i % scene.targetCount));
        m.byte(inputs + 40 + i, scene.strikeCount ? 0 : quint8(targetBase + i));
    }
    if (scene.strikeCount) m.byte(inputs + 40, 128);
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
        const bool caster = i == sourceBase;
        const int classId = occupied ? (caster ? scene.sourceClass : scene.targetClass) : 0;
        m.word(a + 0x48, classId);
        m.word(a + 0x4c, occupied ? (caster ? scene_.sourceCurrentClass : scene_.targetCurrentClass) : 0);
        m.word(a + 0x40, i < 9 ? 0x500 : 0);
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
        append(effect);
    }
    m.word(0x800e9ba0, 0x80600000);
}
void EffectPlayer::append(int effect) {
    if (!machine_ || visual_.frames || effect < 0 || effect > 254 ||
        (effect > 132 && effect < 136))
        throw Error("Choose a native attack effect before continuing playback");
    auto &m = *machine_;
    if (effect >= 136) {
        const auto attacks = AttackPlacement::read(Rom::fromBytes(rom_));
        bool assigned = false;
        if (attacks)
            for (int i = 1; i < attacks->count; ++i)
                assigned |= m.byte(attacks->settings + i * 32 + 21) == effect;
        if (!assigned)
            throw Error("The exported effect slot is not assigned to an attack");
    }
    if (!running()) {
        tick_ = 0;
        // The full battle loop retires these camera/transition bookkeeping
        // entries after the root task finishes. This preview omits that camera
        // loop, but must retire completed entries before another submission.
        for (int i = 0; i < 4; ++i) m.half(0x80220db8 + i * 20, 0);
    }
    m.registers[4] = quint32(effect);
    m.registers[5] = 0x80720000;
    m.registers[6] = 0x80720014;
    m.registers[7] = 0x80720028;
    m.call(0x8020e850);
}
void EffectPlayer::startVisual(const AttackVisual &visual, int inherited, const Scene &scene) {
    visual.validate();
    validateEffectArtwork(Rom::fromBytes(rom_), visual);
    if (visual.nativeEdited() || scene.strikeCount || visual.audio.enabled) {
        start(-1, scene);
        installEffectPreviewRuntime(*machine_, visual, inherited, [this, visual](quint32 submit) {
            machine_->service(submit,[this,visual] {
                auto &m=*machine_;const auto config=0x80430000u;
                const auto audio=config+m.word(config+16);
                for(int i=0;i<visual.audio.sounds.size();++i) {
                    const auto pointer=audio+m.word(audio+32+i*16+8);
                    if(pointer!=m.registers[4])continue;
                    AudioCue cue{tick_,-1,-2};cue.source=visual.audio.sounds[i].json();
                    cue.variant=m.registers[5]?1:0;cue.label=visual.audio.sounds[i].name;
                    audio_.append(cue);pendingAudioCue_=-1;return;
                }
                throw Error("Private sound source is outside its owning attack");
            });
        });
        machine_->registers[4] = 136;
        machine_->registers[5] = 0x80720000;
        machine_->registers[6] = 0x80720014;
        machine_->registers[7] = 0x80720028;
        machine_->call(0x8020e850);
        return;
    }
    start(visual.frames ? -1 : visual.nativeEffect < 0 ? inherited : visual.nativeEffect, scene);
    visual_ = visual;
    delay_ = visual.delay;
    if (visual.frames) {
        auto &m = *machine_;
        m.service(0x80000600, [this] {
            auto &c = *machine_;
            std::array<unsigned int, 2048> commands{};
            const auto &asset = *visual_.frames;
            auto at = c.word(0x800e9ba0);
            for (int origin = 0; origin < originCount_; ++origin) {
                const int count = lc_visual_image_commands_format(
                    commands.data(), 0x80500008, asset.size.width(), asset.size.height(),
                    asset.anchor.x() + 160 - origins_[origin * 2],
                    asset.anchor.y() + 120 - origins_[origin * 2 + 1], 2048,
                    visual_.fullColorFrames());
                if (count < 0)
                    throw Error("Imported effect drawing exceeds its command budget");
                for (int i = 0; i < count; ++i) {
                    c.word(at, commands[i]);
                    at += 4;
                }
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
    m.mount(0x80500000, AttackVisual::gameImage(visual_.frames->composite(frame_), visual_.fullColorFrames()));
    hold_ = visual_.frames->frames[frame_].ticks;
}
bool EffectPlayer::running() const {
    return machine_ && (machine_->word(0x80427004) || delay_ > 0 || (visual_.frames ? frame_ < visual_.frames->frames.size()
                                                      : machine_->word(0x80220d88) != 0));
}
bool EffectPlayer::advance() {
    if (!running())
        return false;
    if (tick_ >= 1200 * std::max(1, scene_.strikeCount))
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
    if (visual_.frames) {
        std::array<float, 60> positions{};
        std::array<float, 16> view{}, projection{};
        std::array<unsigned char, 20> targets{};
        const unsigned char caster = m.byte(0x80720000);
        unsigned int occupied = 0, opponents = 0;
        for (int i = 0; i < 20; ++i) {
            targets[i] = m.byte(0x80720014 + i);
            if (visual_.framePlacement()) {
                const auto actor = 0x80740000 + 0x1c4 + i * 0xf8;
                if (!m.word(actor + 0x48))
                    continue;
                occupied |= 1U << i;
                if (m.word(actor + 0x40) & 0x100)
                    opponents |= 1U << i;
                m.registers[4] = i;
                m.registers[5] = 0x80720100;
                m.registers[6] = 0x10;
                m.call(0x801f7344);
                for (int j = 0; j < 3; ++j)
                    positions[i * 3 + j] = m.number(0x80720100 + j * 4);
            }
        }
        for (int i = 0; i < 16; ++i) {
            view[i] = m.number(0x80220e20 + i * 4);
            projection[i] = m.number(0x80220e90 + i * 4);
        }
        originCount_ =
            lc_visual_origins(visual_.framePlacement(), &caster, targets.data(), positions.data(),
                              occupied, opponents, view.data(), projection.data(), origins_.data());
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
EffectRaster::Result EffectPlayer::draw(QSize size, double zoom) {
    if (!machine_)
        throw Error("No effect is selected");
    auto result = EffectRaster::draw(machine_->memory, 0x80600000, machine_->word(0x800e9ba0), size, zoom);
    for (const auto &texture : result.textures) {
        if (texture.format || texture.bits != 32 || texture.rowBytes <= 0 || texture.rowBytes % 4) continue;
        for (auto &r : resources_) {
            const auto address = r.address & 0x1fffffff;
            if ((r.type == 4 || r.type == 7 || r.type == 8) && texture.address >= address &&
                texture.address < address + r.bytes.size() && r.bytes.size() % texture.rowBytes == 0 && r.size.isEmpty())
                r.size = {texture.rowBytes / 4, int(r.bytes.size() / texture.rowBytes)};
        }
    }
    return result;
}
} // namespace lordly
