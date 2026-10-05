#pragma once
#include "engine/sprite_object.h"
#include <QMap>
#include <QPoint>
#include <array>
namespace lordly {
struct PoseRecord {
    int opcode = 0;
    QByteArray operands;
};
struct PoseProgram {
    QByteArray source;
    QVector<PoseRecord> records;
};
struct ActorPoseSelection {
    int animation = 0, facing = 0, state = 0;
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
struct BodyAppearance {
    quint16 equipment = 0;
    std::optional<quint8> paletteReplacement;
};
struct BodyLayerSelection {
    ActorLayer metadata;
    int child = 0, paletteOffset = 0;
};
class ActorArt {
  public:
    static ActorArt load(const Rom &rom, int bank);
    static ActorArt loadBody(const Rom &rom, int source, int owner, int flagA, int flagB);
    // Direct descriptor handles used by Stage props; no class or cutscene vocabulary.
    static ActorArt loadDirect(const Rom &rom, quint16 handle);
    static int bankCount(const Rom &rom);
    bool isBody() const {
        return body_;
    }
    int sourceClass() const {
        return sourceClass_;
    }
    int ownerClass() const {
        return ownerClass_;
    }
    int flagA() const {
        return flagA_;
    }
    int flagB() const {
        return flagB_;
    }
    quint16 bodyHandle() const {
        return bodyHandle_;
    }
    int bodyVariant() const {
        return bodyHandle_ >> 12;
    }
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
    PoseRecord poseRecord(int state, int cursor) const;
    QVector<ActorPoseSelection> selections() const;
    QVector<ActorLayer> layers(int token) const;
    QVector<ActorFrameLayer> frame(const Rom &rom, int token, int variant, int sepia = 0,
                                   const QVector<quint8> &perLayer = {}) const;
    QVector<BodyLayerSelection> bodyLayers(const Rom &, int token, BodyAppearance) const;
    QVector<ActorFrameLayer> bodyFrame(const Rom &, int token, BodyAppearance, int sepia = 0) const;

  private:
    static ActorArt descriptor(const Rom &rom, quint32 key);
    int bank_ = 0;
    bool body_ = false;
    int sourceClass_ = 0, ownerClass_ = 0, flagA_ = 0, flagB_ = 0;
    quint16 bodyHandle_ = 0;
    quint32 descriptorKey_ = 0;
    QVector<quint32> artKeys_;
    QByteArray metadata_, palettes_, paletteMap_, imageKinds_;
    std::array<int, 4> bodyReplacements_{65535, 65535, 65535, 65535};
    QVector<PoseProgram> poses_;
    QVector<short> stateTable_;
};
} // namespace lordly
