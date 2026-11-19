#include "engine/scene_audio.h"
#include <algorithm>
namespace lordly {
SceneAudio::SceneAudio(LordlyAudioInputs inputs, std::array<quint32, 4> gains) : inputs_(inputs) {
    std::copy(gains.cbegin(), gains.cend(), state_.gains);
}
void SceneAudio::emitRequest(void *user, int kind, int context, uint32_t value, uint32_t extra) {
    auto &self = *static_cast<SceneAudio *>(user);
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
    std::optional<uint32_t> indexedGain;
    if (opcode == 0xb4 && !state_.envelope_present && words[1] >= 4 &&
        (words[0] < 2 || words[0] == 7)) {
        const quint32 address = 0x800eb2e0u + words[1] * 8u;
        if (address < 0x80000000u || address > 0x807ffffcu)
            throw Error("The scene's sound gain address is outside game RAM");
        if (gainReader_) indexedGain = gainReader_(address);
        if (!indexedGain) next.missingGain_ = words[1];
    }
    if (!lordly_audio_command(&next.state_, &next.inputs_, opcode, words.data(),
                              indexedGain ? &*indexedGain : nullptr, emitRequest,
                              &next))
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
