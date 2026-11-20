#pragma once
#include "engine/rom.h"
#include "game/director/scene_audio.h"
#include <QHash>
#include <QVector>
#include <array>
#include <functional>
#include <optional>
namespace lordly {
struct SceneAudioRequest {
    int kind, context;
    quint32 value, extra;
    bool operator==(const SceneAudioRequest &) const = default;
};
struct SceneAudioDelivery {
    enum Destination { IndexedGain, DefaultGain, Registered, Queue, Suppressed, Pending };
    Destination destination;
    quint32 channel, argument, stored;
    int registerSlot = -1;
};
class SceneAudio {
  public:
    SceneAudio(LordlyAudioInputs inputs, std::array<quint32, 4> gains);
    bool service(quint32, const QVector<quint32> &);
    void advance();
    void loadChannelTable(const QByteArray &rom);
    void useEntryState(const QByteArray &ram);
    bool hasEntryState() const {
        return hasEntryState_;
    }
    const LordlyAudioInputs &inputs() const {
        return inputs_;
    }
    const std::optional<SceneAudioDelivery> &lastDelivery() const {
        return lastDelivery_;
    }
    using GainReader = std::function<std::optional<quint32>(quint32 address)>;
    void setGainReader(GainReader reader) {
        gainReader_ = std::move(reader);
    }
    std::optional<quint32> missingGain() const {
        return missingGain_;
    }
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
    void deliver(int kind, int index, quint32 argument, quint32 extra);
    std::array<quint32, 7> channels_{0x800eb0b0, 0x800eb100, 0x800eb150, 0x800eb1a0,
                                     0x800eb1f0, 0x800eb240, 0x800eb290};
    QHash<quint32, QByteArray> channelRecords_;
    quint16 gameMode_ = 0;
    bool hasEntryState_ = false;
    std::optional<SceneAudioDelivery> lastDelivery_;
    LordlyAudioInputs inputs_{};
    LordlySceneAudio state_{};
    GainReader gainReader_;
    std::optional<quint32> missingGain_;
    QVector<SceneAudioRequest> requests_;
    quint64 requestCount_ = 0;
};
} // namespace lordly
