#pragma once
#include <QString>
#include <QVector>
#include <QByteArray>
#include <QJsonObject>
namespace lordly {
class Rom;
class ActorArt;
struct AudioCue {
    int tick = 0, cue = -1, request = -1, context = 0;
    bool spatial = false;
    QByteArray pcm;
    QString label;
    QJsonObject source;
    int variant = 1; // Native spatial variant; 1 is the standard player preview.
};
struct PoseAudioTrace {
    QVector<AudioCue> cues;
    int ticks = 0;
    QString boundary;
};
// Request times use the selected pose's own update clock, not battle time.
PoseAudioTrace tracePoseAudio(const Rom &, const ActorArt &, int state, int variant);
QString effectSoundName(const AudioCue &, const QString &usage, int ordinal);
} // namespace lordly
