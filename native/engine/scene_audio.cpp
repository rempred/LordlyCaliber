#include "engine/scene_audio.h"
#include "engine/effect_machine.h"
#include "rebuild/accepted_rom.h"
#include <algorithm>
namespace lordly {
SceneAudio::SceneAudio(LordlyAudioInputs inputs, std::array<quint32, 4> gains) : inputs_(inputs) {
    std::copy(gains.cbegin(), gains.cend(), state_.gains);
}
void SceneAudio::loadChannelTable(const QByteArray &rom) {
    for (size_t i = 0; i < channels_.size(); ++i)
        channels_[i] = u32(rom, 0x286878 + 4 * i);
}
void SceneAudio::useEntryState(const QByteArray &ram) {
    if (ram.size() != 0x400000 && ram.size() != 0x800000)
        throw Error("Scene audio needs a complete entry RAM image");
    auto address = [&](quint32 pointer, quint32 size) -> qsizetype {
        if (pointer < 0x80000400 || quint64(pointer) + size > 0x80000000ull + ram.size() ||
            (pointer & 3))
            throw Error("Invalid resident audio pointer in scene entry RAM");
        return pointer - 0x80000000u;
    };
    SceneAudio entry({}, {});
    const auto count = u32(ram, 0xb1804);
    const auto pool = u32(ram, 0xb180c);
    if (count > 256)
        throw Error("Invalid channel count in scene entry RAM");
    const auto base = count ? address(pool, count * 0x13c) : 0;
    for (size_t i = 0; i < channels_.size(); ++i) {
        const auto channel = u32(ram, 0x22a8a8 + 4 * i);
        entry.channels_[i] = channel;
        if (!channel)
            continue;
        const auto at = address(channel, 0x50);
        // Resident voice query at 80081B08 counts pool entries with the same nonzero handle.
        const auto handle = u32(ram, at + 0x24);
        entry.channelRecords_[channel] = ram.mid(at, 0x50);
        for (quint32 n = 0; handle && n < count; ++n)
            entry.inputs_.voices[i] += u32(ram, base + n * 0x13c + 0x44) == handle;
    }
    entry.inputs_.controller_suppressed = (u8(ram, 0x193686 + 0xb5 / 8) >> (0xb5 % 8)) & 1;
    entry.inputs_.route = qint16(u16(ram, 0x22a994));
    entry.gameMode_ = u16(ram, 0xe810e);
    for (int i = 0; i < 4; ++i)
        entry.state_.gains[i] = u32(ram, 0xeb2e0 + 8 * i);
    if (const auto scene = u32(ram, 0x22a974)) {
        const auto at = address(scene, 0x1cb5);
        entry.inputs_.scene_present = 1;
        for (int i = 0; i < 3; ++i)
            entry.state_.pending[i] = u8(ram, at + 0x1cb2 + i);
    }
    if (const auto envelope = u32(ram, 0x22a8c4)) {
        const auto at = address(envelope, 8);
        entry.state_.envelope_present = 1;
        entry.state_.step = qint16(u16(ram, at));
        entry.state_.level = qint16(u16(ram, at + 2));
        entry.state_.remaining = qint16(u16(ram, at + 4));
        entry.state_.group = qint16(u16(ram, at + 6));
    }
    entry.hasEntryState_ = true;
    *this = std::move(entry);
}

// Run the compiled owners up to their resident writes or queue submission.
// Only the result is kept in a playback checkpoint. The instruction runner is scratch.
void SceneAudio::deliver(int kind, int index, quint32 argument, quint32 extra) {
    using D = SceneAudioDelivery;
    static const auto gain = acceptedRomBytes(0x2831e0, 68);
    static const auto router = acceptedRomBytes(0x208dc8, 232);
    static const auto resident = acceptedRomBytes(0x3fb94, 196);
    EffectMachine m;
    auto &r = m.registers;
    r[29] = 0x807ff000;
    r[5] = argument;
    m.mount(0x800ea604, resident, true);
    std::optional<D> result;
    const quint32 channel = index == 7 ? 0x800eb0b0 : channels_.at(index);
    if (channelRecords_.contains(channel))
        m.mount(channel, channelRecords_.value(channel));
    if (kind == LORDLY_AUDIO_GAIN) {
        m.mount(0x80227210, gain, true);
        for (size_t i = 0; i < channels_.size(); ++i)
            m.word(0x8022a8a8 + 4 * quint32(i), channels_[i]);
        static const auto defaultGain = acceptedRomBytes(0x3fdc8, 24);
        static const auto indexedGain = acceptedRomBytes(0x3fe14, 24);
        m.mount(0x800ea838, defaultGain, true);
        m.mount(0x800ea884, indexedGain, true);
        m.observe(0x800ea838, [&] { result = D{D::DefaultGain, r[4], r[5], 0}; });
        m.observe(0x800ea884, [&] { result = D{D::IndexedGain, r[4], r[5], 0}; });
        r[4] = index;
        m.call(0x80227210, 64);
        if (result)
            result->stored = m.word(result->channel + 0x3c);
    } else {
        r[4] = channel;
        std::optional<quint32> residentArgument;
        m.observe(0x800ea604, [&] { residentArgument = r[5]; });
        m.service(0x800ea924, [&] {
            if (r[5] == 1 && residentArgument)
                result = D{D::Queue, r[4], *residentArgument, r[6]};
        });
        if (kind == LORDLY_AUDIO_REGISTER) {
            m.mount(0x801c5938, router, true);
            m.half(0x800e810e, gameMode_);
            for (int i = 0; i < 7; ++i)
                m.word(0x801d06b0 + 4 * i, ~argument);
            m.call(0x801c5938, 180);
            for (int i = 0; i < 7; ++i) {
                if (m.word(0x801d06b0 + 4 * i) == argument)
                    result = D{D::Registered, channel, argument, argument, i};
            }
            if (!channel && gameMode_ == 12 && !residentArgument)
                result = D{D::Suppressed, 0, argument, 0};
        } else if (kind == LORDLY_AUDIO_PENDING) {
            r[6] = extra;
            m.call(0x800ea6b0, 32);
            result = D{D::Pending, channel, argument, m.word(channel + 4)};
        } else {
            m.call(0x800ea604, 96);
            if (!result)
                result = D{D::Pending, channel, argument, m.word(channel + 4)};
        }
    }
    if (!result)
        throw Error("The scene audio service returned without a destination");
    if (result->destination == D::Pending || result->destination == D::DefaultGain ||
        result->destination == D::IndexedGain) {
        channelRecords_[result->channel] = m.memory.mid(m.physical(result->channel, 0x50), 0x50);
    }
    lastDelivery_ = result;
}
void SceneAudio::emitRequest(void *user, int kind, int context, uint32_t value, uint32_t extra) {
    auto &self = *static_cast<SceneAudio *>(user);
    if (kind == LORDLY_AUDIO_REGISTER || kind == LORDLY_AUDIO_PENDING ||
        kind == LORDLY_AUDIO_DIRECT)
        self.deliver(kind, context, value, extra);
    self.requests_.append({kind, context, value, extra});
    ++self.requestCount_;
}
bool SceneAudio::service(quint32 opcode, const QVector<quint32> &args) {
    const int count = opcode == 0x6e                     ? 3
                      : opcode == 0x6f || opcode == 0x70 ? 2
                      : opcode == 0x71 || opcode == 0xb4 ? 4
                                                         : 0;
    if (!count)
        return false;
    if (args.size() != count)
        throw Error("Scene audio command is truncated");
    std::array<uint32_t, 4> words{};
    std::copy(args.cbegin(), args.cend(), words.begin());
    auto next = *this;
    if (opcode == 0x6f) {
        if (words[0] > 7)
            throw Error("Invalid story audio gain context");
        next.deliver(LORDLY_AUDIO_GAIN, int(words[0]), words[1], 0);
        const auto &delivery = *next.lastDelivery_;
        const bool global = delivery.destination == SceneAudioDelivery::DefaultGain;
        emitRequest(&next, global ? LORDLY_AUDIO_BUS_GAIN_PENDING : LORDLY_AUDIO_GAIN,
                    global ? 0 : int(words[0]), delivery.argument, 0);
        *this = std::move(next);
        return true;
    }
    std::optional<uint32_t> indexedGain;
    if (opcode == 0xb4 && !state_.envelope_present && words[1] >= 4 &&
        (words[0] < 2 || words[0] == 7)) {
        const quint32 address = 0x800eb2e0u + words[1] * 8u;
        if (address < 0x80000000u || address > 0x807ffffcu)
            throw Error("The scene's sound gain address is outside game RAM");
        if (gainReader_)
            indexedGain = gainReader_(address);
        if (!indexedGain)
            next.missingGain_ = words[1];
    }
    if (!lordly_audio_command(&next.state_, &next.inputs_, opcode, words.data(),
                              indexedGain ? &*indexedGain : nullptr, emitRequest, &next))
        throw Error(
            "Scene audio command requires a valid context, gain bus and current scene state");
    *this = std::move(next);
    return true;
}
void SceneAudio::advance() {
    auto next = *this;
    lordly_audio_advance(&next.state_, &next.inputs_, emitRequest, &next);
    *this = std::move(next);
}
} // namespace lordly
