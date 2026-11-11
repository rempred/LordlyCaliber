#pragma once
#include "engine/rom.h"
#include <QHash>
#include <QVector>
#include <array>
#include <functional>

namespace lordly {
// Isolated o32 instruction runner for the retail effect programs. There is no
// console, OS, device emulation or access to host memory. Every call and memory
// access is bounded; host services are explicitly registered by EffectPlayer.
class EffectMachine {
  public:
    EffectMachine();
    QByteArray memory;
    std::array<quint32, 32> registers{}, floating{};
    void mount(quint32 address, const QByteArray &bytes, bool executable = false);
    void service(quint32 address, std::function<void()> callback);
    void observe(quint32 address, std::function<void()> callback);
    quint8 byte(quint32) const;
    quint16 half(quint32) const;
    quint32 word(quint32) const;
    void byte(quint32, quint8);
    void half(quint32, quint16);
    void word(quint32, quint32);
    float single(int) const;
    void single(int, float);
    double real(int) const;
    void real(int, double);
    float number(quint32) const;
    void number(quint32, float);
    quint32 physical(quint32, quint32 bytes = 1) const;
    void call(quint32 entry, quint32 budget = 2000000);
    quint32 lastInstruction() const {
        return pc_;
    }

  private:
    quint32 hi_ = 0, lo_ = 0, pc_ = 0;
    bool condition_ = false;
    QVector<std::pair<quint32, quint32>> executable_;
    QHash<quint32, std::function<void()>> services_;
    QHash<quint32, std::function<void()>> observers_;
    std::pair<quint32, quint32> step(quint32 pc, quint32 next);
};
} // namespace lordly
