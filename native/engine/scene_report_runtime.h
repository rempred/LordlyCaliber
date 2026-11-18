#pragma once
#include "engine/effect_machine.h"
#include <QMap>
namespace lordly {
// Execute Hugo's Report panel services against a copy of a verified invocation.
// Only the report's overlays and their resident support code are executable.
class SceneReportRuntime {
  public:
    SceneReportRuntime(const Rom &, const QByteArray &captured, const QByteArray &script);
    void create(int slot, int preset);
    void close(int slot, bool immediate);
    void release(int slot);
    void help(int slot, int section);
    void menuLabel();
    void advance(quint16 pressed, quint16 direction);
    struct Panel {
        quint32 address;
        QByteArray record, content;
    };
    struct Slot {
        int index, key, status;
        quint32 panel;
    };
    QVector<Panel> panels() const;
    QVector<Slot> slotsInUse() const;
    bool backgroundVisible() const {
        return machine_.byte(machine_.word(0x8023de34) + 0x66) != 2;
    }
    const QVector<int> &sounds() const {
        return sounds_;
    }

  private:
    int call(quint32 address, quint32 a0 = 0, quint32 a1 = 0);
    quint32 allocate(int size);
    QByteArray read(quint32 address, int size) const;
    QByteArray string(quint32 address, int max) const;
    quint32 loadResource(quint32 key, bool compressed);
    void attachServices();
    Rom rom_;
    EffectMachine machine_;
    quint32 next_ = 0, arenaEnd_ = 0;
    QMap<quint32, int> blocks_, free_;
    QVector<int> sounds_;
};
} // namespace lordly
