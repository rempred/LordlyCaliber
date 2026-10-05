#pragma once
#include "engine/rom.h"
#include "game/director/scene_audio.h"
#include <QVector>
#include <array>
namespace lordly {
struct SceneAudioRequest {
    int kind, context;
    quint32 value, extra;
    bool operator==(const SceneAudioRequest &) const = default;
};
class SceneAudio {
  public:
    SceneAudio(LordlyAudioInputs inputs, std::array<quint32, 2> gains);
    bool service(quint32, const QVector<quint32> &);
    void advance();
    void supplyInputs(LordlyAudioInputs inputs) {
        inputs_ = inputs;
    }
    const LordlySceneAudio &state() const {
        return state_;
    }
    const QVector<SceneAudioRequest> &requests() const {
        return requests_;
    }
    quint64 requestCount() const {
        return requestCount_;
    }
    void clearRequests() {
        requests_.clear();
    }

  private:
    static void emitRequest(void *, int, int, uint32_t, uint32_t);
    LordlyAudioInputs inputs_{};
    LordlySceneAudio state_{};
    QVector<SceneAudioRequest> requests_;
    quint64 requestCount_ = 0;
};
} // namespace lordly
