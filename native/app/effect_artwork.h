#pragma once
#include "engine/effect_player.h"
#include <atomic>
#include <memory>
namespace lordly {
struct EffectClip {
    QVector<QImage> images;
    QVector<int> holds;
    int ticks = 0;
    QString error;
    QMap<quint32, EffectArtwork> artwork;
};
QMap<int, QString> effectNames(const Rom &);
QImage effectChecker(const QImage &);
QImage effectArtworkImage(const EffectArtwork &, const AttackVisual &);
EffectClip renderEffectClip(const Rom &, const AttackVisual &, int inherited,
                            const EffectPlayer::Scene &, const std::shared_ptr<std::atomic_bool> &);
} // namespace lordly
