#pragma once
#include "engine/attack_visual.h"
#include "engine/effect_machine.h"
#include "engine/effect_raster.h"
#include <memory>
namespace lordly {
// Runs only battle visual tasks with explicit preview inputs, without console
// devices, game logic, audio, or host-memory access.
class EffectPlayer {
  public:
    struct Resource {
        int type = 0, id = 0;
        QByteArray bytes;
        quint32 address = 0;
        QSize size;
    };
    struct Scene {
        int sourceClass = 2, targetClass = 2, sourceHeight = 38, targetHeight = 38, attack = 55,
            targetCount = 5;
        bool allies = false;
        bool enemyCaster = true; // Native actor slots 0–8 have opponent flag 0x100.
    };
    explicit EffectPlayer(const Rom &);
    void start(int effect, const Scene &);
    // Submit another native effect with the same actors, retaining native pools
    // and cached resources (including while another visual is still running).
    void append(int effect);
    void startVisual(const AttackVisual &, int inheritedEffect, const Scene &);
    bool advance();
    EffectRaster::Result draw(QSize size = {640, 480}, double zoom = 1.5);
    int tick() const {
        return tick_;
    }
    bool running() const;
    const QByteArray &memory() const {
        return machine_->memory;
    }
    static int retailEffect(int attack);
    const QVector<Resource> &resources() const { return resources_; }

  private:
    QByteArray rom_;
    std::unique_ptr<EffectMachine> machine_;
    Scene scene_;
    quint32 heap_ = 0;
    int tick_ = 0;
    AttackVisual visual_;
    int delay_ = 0, frame_ = 0, hold_ = 0;
    quint32 frameDraw_ = 0;
    std::array<int, 40> origins_{};
    int originCount_ = 0;
    QMap<quint32, quint32> allocations_;
    QVector<Resource> resources_;
    void loadFrame();
};
} // namespace lordly
