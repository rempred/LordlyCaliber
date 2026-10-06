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
    if (!lordly_audio_command(&next.state_, &next.inputs_, opcode, words.data(), emitRequest,
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
