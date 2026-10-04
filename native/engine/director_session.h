#pragma once
#include "engine/director.h"
#include "game/director/dispatch_core.h"
#include <array>
#include <functional>
#include <optional>
namespace lordly {
struct DirectorLaunchTranslations {
    std::array<unsigned short, 256> values{};
    std::array<unsigned char, 256> known{};
    void set(int index, quint16 value);
};
// Owns the original single saved-stream slot, including replacement by nested
// continuations. Domain services and update-tick ordering belong to the caller.
class DirectorSession {
  public:
    using Service = std::function<bool(quint32, const QVector<quint32> &, int &)>;
    DirectorSession(Directors resources, int selector, int mode,
                    DirectorLaunchTranslations translations = {});
    int dispatch(const Service &service = {}, int instructionBudget = 10000);
    const LordlyDirectorDispatch &state() const {
        return state_;
    }
    int selector() const {
        return current_.selector;
    }
    bool isContinuation() const {
        return current_.continuation;
    }
    quint32 resourceKey() const {
        return current_.key;
    }
    const QString &reason() const {
        return reason_;
    }
    void setController(quint16 mask) {
        state_.controller = mask;
    }
    void setRequestedSelector(int selector) {
        requestedSelector_ = selector;
    }
    void setCounter(quint32 value) {
        state_.counter = value;
    }
    void setBarrierState(quint8 value) {
        state_.barrier_state = value;
    }

  private:
    struct Stream {
        QVector<quint32> words;
        quint32 key = 0;
        int selector = 0, resume = 0;
        bool continuation = false;
    };
    Stream load(int selector, bool continuation) const;
    static int lookup(void *, LordlyDirectorWord, LordlyDirectorDefinition *);
    static int invoke(void *, LordlyDirectorWord, const LordlyDirectorWord *, int, int *);
    Directors resources_;
    DirectorLaunchTranslations translations_;
    Stream current_;
    std::optional<Stream> previous_;
    std::optional<int> requestedSelector_;
    LordlyDirectorDispatch state_{};
    const Service *service_ = nullptr;
    QString reason_;
    bool ended_ = false;
};
} // namespace lordly
