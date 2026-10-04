#pragma once
#include "engine/sprite_object.h"
#include <QMap>
#include <QPoint>
namespace lordly {
struct PoseRecord {
    int opcode = 0;
    QByteArray operands;
};
struct PoseProgram {
    QByteArray source;
    QVector<PoseRecord> records;
};
struct ActorLayer {
    int art = 0;
    QPoint origin;
    int width = 0, height = 0;
    quint16 flags = 0, scaleX = 0, scaleY = 0;
};
struct ActorFrameLayer {
    ActorLayer metadata;
    SpriteFrame pixels;
};
class ActorArt {
  public:
    static ActorArt load(const Rom &rom, int bank);
    int bank() const {
        return bank_;
    }
    quint32 descriptorKey() const {
        return descriptorKey_;
    }
    const QVector<quint32> &artKeys() const {
        return artKeys_;
    }
    const QVector<PoseProgram> &poses() const {
        return poses_;
    }
    int resolve(int animation, int facing, int option = 0) const;
    QVector<ActorLayer> layers(int token) const;
    QVector<ActorFrameLayer> frame(const Rom &rom, int token, int variant) const;

  private:
    int bank_ = 0;
    quint32 descriptorKey_ = 0;
    QVector<quint32> artKeys_;
    QByteArray metadata_, palettes_, paletteMap_;
    QVector<PoseProgram> poses_;
    QVector<short> stateTable_;
};
} // namespace lordly
