#pragma once
#include "engine/rom.h"
#include <QMap>
namespace lordly {
// Snapshot of image-processing decisions, independent of the decoded-pixel LRU.
// Ordinary Actor image entries are shared by source, variant and art index.
class SceneImageCache {
  public:
    quint8 touch(qint32 source, quint8 variant, int art, bool sepia);
    void invalidate(qint32 source, quint8 variant);
    void convertAll();
    void finishDraw();
    void clear() {
        owners_.clear();
    }

  private:
    struct Entry {
        quint8 conversions = 0, age = 0;
    };
    static quint64 key(qint32 source, quint8 variant) {
        return quint64(quint32(source)) << 8 | variant;
    }
    QMap<quint64, QMap<int, Entry>> owners_;
};
} // namespace lordly
