#include "engine/sound_preview.h"
#include "engine/effect_machine.h"
#include "engine/sound_parts.h"
#include "rebuild/sound_preview_runtime.h"
#include <QJsonDocument>
#include "third_party/vadpcm/codec/vadpcm.h"
#include <QHash>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
namespace lordly {
namespace {
constexpr quint32 bank = 0x80104710, effects = 0x8010a760;
constexpr quint32 channels = 0x80400000, voices = 0x80410000;
struct Sample {
    QVector<qint16> pcm;
    int loopStart = 0, loopEnd = 0;
    quint32 loops = 0;
};
struct Voice {
    std::shared_ptr<Sample> sample;
    double position = 0, pitch = 1, volume = 0, targetVolume = 0;
    int ramp = 0, pan = 64;
    quint32 loops = 0;
    int part = -1;
};
int lastPartStart(const AttackSound &s,int variant) {
    int result=0;
    for(const auto &v:s.parts) {
        const auto p=v.toObject();
        if(p["muted"].toBool())continue;
        const auto original=soundPartState(p,variant?"original":"enemyOriginal");
        const int tick=p["tick"].toInt()>=0?p["tick"].toInt():original.isEmpty()?0:u32(original,0x144);
        result=qMax(result,tick+lastPartStart(AttackSound::fromJson(p["source"].toObject()),variant));
    }
    if(result>5400)throw Error("Nested sound timing exceeds the 90-second preview limit");
    return result;
}
} // namespace
static SoundPreview render(const Rom &rom, int request,
                                const std::shared_ptr<std::atomic_bool> &cancel,const AttackSound *imported,
                                bool collectParts, const QVector<AudioCue> *timeline = nullptr, int visualTicks = 0) {
    rom.requireSupported();
    auto bytes = rom.bytes();
    if (!timeline && !imported && (request < 0 || request >= int(u32(bytes, 0x4e3144))))
        throw Error("This request is outside the game's sound-effect bank");
    EffectMachine m;
    m.mount(0x80070000, bytes.mid(0x400, 0x2ec00), true);
    // Resident libmus tables/constants; placement confirmed by their callers.
    m.mount(0x800a7c00, bytes.mid(0x38000, 0x7000), false);
    m.mount(bank, bytes.mid(0x423ff0, 0x5ce0), false);
    m.mount(effects, bytes.mid(0x4e3140, 0xde70), false);
    // Game-side request queue, including duplicate/priority/handle handling.
    // This relocated resident family uses ROM + 0x800AAA70, not the boot delta.
    m.mount(0x800e9c20, bytes.mid(0x3f1b0, 0x1cd0), true);
    if (bytes.mid(0x423ff0, 15) != "N64 PtrTablesV2")
        throw Error("Unsupported sound sample bank");
    m.registers[29] = 0x807ff000;
    auto call = [&](quint32 pc, std::initializer_list<quint32> args = {}) {
        int i = 0;
        for (auto arg : args) {
            if (i < 4)
                m.registers[4 + i] = arg;
            else
                m.word(m.registers[29] + 16 + (i - 4) * 4, arg);
            ++i;
        }
        m.call(pc, 2000000);
    };
    m.service(0x800901c0, [] {}); // Data-cache writeback has no host side effect.
    call(0x80081db4, {bank, 0x429cd0u});
    call(0x80081f7c, {effects});
    // Native game loader binds this effects bank to the second sample bank.
    call(0x8008202c, {effects, bank});
    for (const auto &[a, v] :
         std::array<std::pair<quint32, quint32>, 8>{{{0x800b1804, 68},
                                                     {0x800b1808, voices},
                                                     {0x800b180c, channels},
                                                     {0x800b1810, channels + 4 * 0x13c},
                                                     {0x800b1814, 60},
                                                     {0x800b1818, 16666},
                                                     {0x800b1820, 1},
                                                     {0x800b1824, 0x12345678}}})
        m.word(a, v);
    m.half(0x800b181c, 26623);
    m.half(0x800b181e, 32767);
    for (int i = 0; i < 68; ++i)
        call(0x80083738, {channels + quint32(i) * 0x13c});
    QHash<quint32, std::shared_ptr<Sample>> samples;
    auto sample = [&](quint32 wave) {
        if (samples.contains(wave))
            return samples.value(wave);
        const auto start = m.word(wave), length = m.word(wave + 4), book = m.word(wave + 16);
        if (start > quint32(bytes.size()) || length > quint32(bytes.size()) - start ||
            length > 0x400000 || m.byte(wave + 8) > 1)
            throw Error("Unsupported or out-of-range sound sample");
        if(m.byte(wave+8)==1){
            if(length%2)throw Error("Invalid raw16 sound length");
            auto result=std::make_shared<Sample>();result->pcm.resize(length/2);
            for(quint32 i=0;i<length/2;++i)result->pcm[i]=qint16(u16(bytes,start+i*2));
            if(const auto loop=m.word(wave+12)) {
                result->loopStart=m.word(loop);result->loopEnd=m.word(loop+4);result->loops=m.word(loop+8);
                if(result->loops && (result->loopStart<0 || result->loopEnd<=result->loopStart || result->loopEnd>result->pcm.size()))
                    throw Error("Invalid raw sound loop");
            }
            samples.insert(wave,result);return result;
        }
        const int order = int(m.word(book)), predictors = int(m.word(book + 4));
        if (order < 1 || order > 8 || predictors < 1 || predictors > 16 || length % 9)
            throw Error("Invalid sound predictor or frame data");
        QVector<vadpcm_vector> coefficients(order * predictors);
        for (int i = 0; i < coefficients.size(); ++i)
            for (int j = 0; j < 8; ++j)
                coefficients[i].v[j] = qint16(m.half(book + 8 + (i * 8 + j) * 2));
        auto result = std::make_shared<Sample>();
        result->pcm.resize(length / 9 * 16);
        vadpcm_vector state{};
        if (vadpcm_decode(predictors, order, coefficients.constData(), &state, length / 9,
                          result->pcm.data(), bytes.constData() + start) != kVADPCMErrNone)
            throw Error("Invalid sound predictor index");
        if (const auto loop = m.word(wave + 12)) {
            result->loopStart = int(m.word(loop));
            result->loopEnd = int(m.word(loop + 4));
            result->loops = m.word(loop + 8);
            if (result->loops && (result->loopStart < 0 || result->loopEnd <= result->loopStart ||
                                  result->loopEnd > result->pcm.size()))
                throw Error("Invalid sound sample loop");
        }
        samples.insert(wave, result);
        return result;
    };
    std::array<Voice, 64> active;
    auto voice = [&]() -> Voice & {
        const auto address = m.registers[4];
        if (address < voices || address >= voices + 64 * 28 || (address - voices) % 28)
            throw Error("Sound voice is outside the preview pool");
        return active[(address - voices) / 28];
    };
    SoundPreview result;
    int audioTick = 0;
    QHash<quint32,QByteArray> channelStates;
    QHash<quint32,int> ordinals;
    quint32 flushingChannel = 0;
    m.observe(0x80083738,[&]{ordinals.remove(m.registers[4]);});
    m.observe(0x8008236c,[&]{
        const auto c=m.registers[17];
        if(collectParts) {
            auto state=m.memory.mid(m.physical(c,0x13c),0x13c);
            state.resize(soundPartStateSize);put32(state,0x148,m.word(0x800b1824));channelStates[c]=state;
        }
    });
    m.observe(0x80082ae8,[&]{flushingChannel=m.registers[4];});
    m.service(0x800884c0, [&] {
        auto &v = voice();
        v.sample = sample(m.registers[5]);
        v.position = 0;
        v.loops = v.sample->loops;
        ++result.voices;
        if (collectParts) {
            v.part = int(result.parts.size());
            result.parts.append({audioTick, int((m.registers[4] - voices) / 28),
                                 m.word(m.registers[5]), {}});
            auto state=channelStates.value(flushingChannel);
            if(state.size()==soundPartStateSize) {
                const auto owner=m.word(flushingChannel+0x78);
                const bool carrier=owner>=64 && m.word(owner-64)==0x4c43534e && m.word(owner-60)==0x44303031 && m.word(owner-48)==3;
                put32(state,0x13c,m.word(flushingChannel+0x80)-(carrier?owner:effects));
                put32(state,0x140,++ordinals[flushingChannel]);put32(state,0x144,audioTick);
                result.parts.last().state=state;
            }
        }
    });
    m.service(0x80088550, [&] { voice().sample.reset(); });
    m.service(0x80088410, [&] {
        auto &v = voice();
        v.targetVolume = std::clamp(int(m.registers[5]), 0, 32767) / 32767.;
        v.ramp = std::clamp(int(m.registers[6] * .032), 0, 32000);
        if (!v.ramp)
            v.volume = v.targetVolume;
    });
    m.service(0x80088380, [&] {
        const auto bits = m.registers[5];
        float pitch;
        std::memcpy(&pitch, &bits, 4);
        if (!std::isfinite(pitch) || pitch < 0 || pitch > 2)
            throw Error("Invalid sound pitch");
        voice().pitch = pitch;
    });
    m.service(0x800882e0, [&] { voice().pan = int(m.registers[5] & 127); });
    m.service(0x80088240, [] {}); // Dry audition, no N64 reverb return.
    QVector<AudioCue> cues = timeline ? *timeline : QVector<AudioCue>{{0, -1, request}};
    if (imported) cues[0].pcm = imported->pcm;
    std::stable_sort(cues.begin(), cues.end(), [](const auto &a, const auto &b) { return a.tick < b.tick; });
    QHash<QByteArray, quint32> importedBanks;
    QHash<QByteArray, quint32> authoredBanks;
    quint32 submit = 0;
    quint32 nextBank = 0x80300000;
    int lastTick = visualTicks * 2, nextCue = 0;
    for (const auto &cue : cues) {
        if (cue.tick < 0 || cue.tick > 2700) throw Error("Sound cue is outside the preview");
        lastTick = qMax(lastTick, cue.tick * 2);
        if (!cue.source.isEmpty()) {
            const auto source=AttackSound::fromJson(cue.source);
            lastTick=qMax(lastTick,cue.tick*2+lastPartStart(source,cue.variant));
            const auto key=QJsonDocument(cue.source).toJson(QJsonDocument::Compact);
            if(authoredBanks.contains(key))continue;
            if(!submit)submit=installSoundPreviewRuntime(m);
            const auto encoded=buildSoundSource(source,bytes.size());bytes+=encoded;
            if(nextBank+encoded.size()>channels)throw Error("Private sounds exceed the preview allocation");
            m.mount(nextBank,encoded);authoredBanks.insert(key,nextBank);
            nextBank+=(encoded.size()+15)&~15;continue;
        }
        if (cue.pcm.isEmpty() || importedBanks.contains(cue.pcm)) continue;
        AttackSound sound; sound.pcm = cue.pcm;
        const auto custom = sound.bank(bytes.size()); bytes += custom;
        if (nextBank + custom.size() > channels) throw Error("Imported sound preview exceeds its bank allocation");
        m.mount(nextBank, custom, false);
        call(0x80081db4, {nextBank, 0});
        call(0x80081f7c, {nextBank + 128});
        call(0x8008202c, {nextBank + 128, nextBank});
        importedBanks.insert(cue.pcm, nextBank + 128);
        nextBank += (custom.size() + 15) & ~15;
    }
    // Imported bank initialization changes the default bank. Restore retail.
    call(0x80081f7c, {effects});
    // Preview loops are bounded. Original sequencing runs at 60 Hz; the game
    // requests a 32 kHz DAC at boot (ROM 22B4), independent of visual 30 Hz.
    for (int tick = 0; tick < lastTick + 900; ++tick) {
        if (cancel && cancel->load())
            return {};
        audioTick = tick;
        while (nextCue < cues.size() && cues[nextCue].tick * 2 == tick) {
            const auto &cue = cues[nextCue++];
            if (!cue.source.isEmpty())
                call(submit,{authoredBanks[QJsonDocument(cue.source).toJson(QJsonDocument::Compact)],quint32(cue.variant)});
            else if (!cue.pcm.isEmpty())
                call(0x80083b24, {importedBanks[cue.pcm], 0, 128, 128, 0xffffffff});
            else if (cue.request >= 0 && cue.request < int(u32(bytes, 0x4e3144)))
                // func_00208DC8's battle request is zero-based. The queue passes
                // it unchanged to MusStartEffect; channel target IDs are a
                // separate one-based field and must not be substituted here.
                call(0x800ea604, {0, quint32(cue.request)});
            else ++result.unresolved;
        }
        call(0x800eac24);
        call(0x800eaf1c);
        call(0x800822ac);
        const int count = (tick + 1) * 32000 / 60 - tick * 32000 / 60;
        const auto offset = result.pcm.size();
        result.pcm.resize(offset + count * 4);
        for (int i = 0; i < count; ++i) {
            double left = 0, right = 0;
            for (auto &v : active) {
                if (!v.sample)
                    continue;
                auto &s = *v.sample;
                if (v.loops && v.position >= s.loopEnd) {
                    v.position =
                        s.loopStart + std::fmod(v.position - s.loopEnd, s.loopEnd - s.loopStart);
                    if (v.loops != 0xffffffff)
                        --v.loops;
                }
                if (v.position >= s.pcm.size()) {
                    v.sample.reset();
                    continue;
                }
                if (v.ramp) {
                    v.volume += (v.targetVolume - v.volume) / v.ramp;
                    --v.ramp;
                }
                const int at = int(v.position);
                const double fraction = v.position - at;
                const int next = v.loops && at + 1 >= s.loopEnd
                                     ? s.loopStart
                                     : std::min(at + 1, int(s.pcm.size()) - 1);
                const double value =
                    (s.pcm[at] * (1 - fraction) + s.pcm[next] * fraction) * v.volume;
                const double pan = v.pan / 127.;
                const double partLeft = value * std::sqrt(1 - pan), partRight = value * std::sqrt(pan);
                left += partLeft;
                right += partRight;
                if (collectParts && v.part >= 0) {
                    auto &pcm = result.parts[v.part].pcm;
                    const auto partOffset = pcm.size();
                    pcm.resize(partOffset + 4);
                    qToLittleEndian<qint16>(qint16(std::clamp(partLeft * .5, -32768., 32767.)), pcm.data() + partOffset);
                    qToLittleEndian<qint16>(qint16(std::clamp(partRight * .5, -32768., 32767.)), pcm.data() + partOffset + 2);
                }
                v.position += v.pitch;
            }
            // Fixed headroom, not per-sound normalization: relative loudness remains.
            qToLittleEndian<qint16>(qint16(std::clamp(left * .5, -32768., 32767.)),
                                    result.pcm.data() + offset + i * 4);
            qToLittleEndian<qint16>(qint16(std::clamp(right * .5, -32768., 32767.)),
                                    result.pcm.data() + offset + i * 4 + 2);
        }
        result.ticks = tick + 1;
        bool busy = false;
        for (int i = 4; i < 68; ++i)
            busy |= m.word(channels + i * 0x13c + 4) != 0;
        const bool pending=submit && m.word(0x80511000+4112+4);
        if (!busy && !pending && nextCue == cues.size() && tick + 1 >= visualTicks * 2)
            return result;
    }
    result.limited = true;
    return result;
}
SoundPreview renderSoundPreview(const Rom &rom,int request,const std::shared_ptr<std::atomic_bool> &cancel,bool parts){return render(rom,request,cancel,nullptr,parts);}
SoundPreview renderImportedSoundPreview(const Rom &rom,const AttackSound &s,const std::shared_ptr<std::atomic_bool> &cancel,bool parts){
    s.validate();if(s.pcm.isEmpty())throw Error("No imported sound");return render(rom,1,cancel,&s,parts);
}
SoundPreview renderAuthoredSound(const Rom &rom,const AttackSound &s,int variant,
                                const std::shared_ptr<std::atomic_bool> &cancel,bool parts) {
    if(!s.hasParts()) {
        if(!s.pcm.isEmpty())return renderImportedSoundPreview(rom,s,cancel,parts);
        const int request=s.cue>=0 ? u16(rom.bytes(),0x2123a0+s.cue*4+(variant?2:0))
                                   : !variant && s.enemyRequest>=0?s.enemyRequest:s.request;
        return renderSoundPreview(rom,request,cancel,parts);
    }
    AudioCue cue{0,-1,-2};cue.source=s.json();cue.variant=variant;
    const QVector<AudioCue> cues{cue};
    auto result=render(rom,0,cancel,nullptr,parts,&cues,0);
    if(!parts)return result;
    if(!s.parts.isEmpty()) {
        auto base=s;base.parts={};
        result.parts=renderAuthoredSound(rom,base,variant,cancel,true).parts;
        for(const auto &value:s.parts) {
            const auto p=value.toObject();const int i=p["index"].toInt();
            if(i<0 || i>=result.parts.size())throw Error("Edited part is absent from this sound variant");
            auto &part=result.parts[i];
            if(p["tick"].toInt()>=0)part.tick=p["tick"].toInt();
            if(p["muted"].toBool())part.pcm.clear();
            else part.pcm=renderAuthoredSound(rom,AttackSound::fromJson(p["source"].toObject()),variant,cancel,true).pcm;
        }
    } else if(!s.state.isEmpty() && !result.parts.isEmpty()) {
        result.parts.resize(1); result.parts[0].tick=0;
        result.pcm=result.parts[0].pcm;
    }
    return result;
}
SoundPreview renderSoundTimeline(const Rom &rom, const QVector<AudioCue> &cues, int visualTicks,
                                const std::shared_ptr<std::atomic_bool> &cancel,bool parts) {
    if (visualTicks < 0 || visualTicks > 2700) throw Error("Effect preview is too long");
    return render(rom, 0, cancel, nullptr, parts, &cues, visualTicks);
}
} // namespace lordly
