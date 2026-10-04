#pragma once
#include "engine/scene_resources.h"
#include <optional>
namespace lordly {
struct SceneResourcePassState {
    SceneResourceQueue queue{};
    quint32 randomSeed = 0, renderContext = 0;
    quint16 cleanupStatus = 0xffff;
    // Initializers inherit the previous selection; updates select their own domain.
    bool selectedInput = false;
};
// ROM 69D8..6EE8. Payload ownership and callback/render implementations belong to
// the runtime owner. All service addresses are identities, never host pointers.
class SceneResourceDispatcher {
  public:
    enum class Service {
        Initialize,
        CaptureRender,
        RestorePayload,
        Update,
        AdvanceRender,
        SavePayload,
        Release
    };
    struct Call {
        Service service;
        int slot;
        quint32 address;
    };
    enum Result { Complete, Suspended, Budget };
    using Handler = std::function<bool(SceneResources &, QByteArray &scratch,
                                       SceneResourcePassState &, const Call &)>;
    explicit SceneResourceDispatcher(SceneResourcePassState state) : state_(state) {}
    // False/throw discards the service's pool, scratch and pass-state writes.
    // The handler must stage its other state too. Completed services are never
    // replayed on resume. Publish a frame only after Complete.
    Result advance(SceneResources &, const Handler &, int stageBudget = 1000);
    SceneResourcePassState &state() {
        return state_;
    }
    const SceneResourcePassState &state() const {
        return state_;
    }
    bool inProgress() const {
        return running_;
    }
    std::optional<Call> pending() const {
        return pending_;
    }
    const QByteArray &scratch() const {
        return scratch_;
    }

  private:
    enum class Phase {
        InitScan,
        Initialize,
        CaptureRender,
        InitSave,
        InitCommit,
        InitRelease,
        InitNext,
        Priority,
        UpdateScan,
        Restore,
        Update,
        Render,
        UpdateSave,
        UpdateCommit,
        UpdateRelease,
        UpdateNext,
        FinalPriority,
        CleanupScan,
        CleanupRelease,
        Finish
    };
    bool invoke(SceneResources &, const Handler &, Service, quint32 address = 0);
    void current(SceneResources &, int slot);
    void clearHead();
    SceneResourcePassState state_;
    QByteArray scratch_;
    Phase phase_ = Phase::InitScan;
    int slot_ = 0, lastInitialized_ = 0;
    bool running_ = false;
    std::optional<Call> pending_;
};
} // namespace lordly
