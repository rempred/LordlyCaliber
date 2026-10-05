#pragma once
#include "engine/artwork_edits.h"
#include "engine/pose_bank.h"
#include <QJsonObject>
#include <array>
namespace lordly {
// A private, immutable-source descriptor snapshot. Original resource bytes remain
// referenced until edited; current project pixels can be captured when copying.
class AnimationDescriptor {
  public:
    static AnimationDescriptor load(const Rom &, quint32 key,
                                    const QMap<int, QByteArray> &artwork = {});
    quint32 key() const {
        return key_;
    }
    const QVector<quint32> &members() const {
        return members_;
    }
    const std::array<QByteArray, 4> &controls() const {
        return controls_;
    }
    const QMap<int, QByteArray> &artwork() const {
        return artwork_;
    }
    PoseBank poses() const {
        return PoseBank::parse(controls_[1]);
    }
    void setPoses(const PoseBank &);
    QByteArray frame(int token) const;
    int appendFrame(const QByteArray &);
    int frameCount() const;
    QByteArray sprite(int art) const;
    void setSprite(int art, const QByteArray &);
    int appendSprite(const QByteArray &, int policy, const QByteArray &palette);
    QByteArray identity() const;
    QJsonObject json() const;
    static AnimationDescriptor fromJson(const Rom &, const QJsonObject &);

  private:
    void setMetadata(const QByteArray &);
    Rom rom_;
    quint32 key_ = 0;
    int originalArtCount_ = 0;
    QVector<quint32> members_;
    std::array<QByteArray, 4> controls_;
    QMap<int, QByteArray> artwork_;
};
} // namespace lordly
