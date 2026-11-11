#include "engine/audio_cues.h"
#include "engine/actor_pose.h"
#include <QSet>
namespace lordly {
QString effectSoundName(const AudioCue &cue, const QString &usage, int ordinal) {
    // Names identify byte-proven usage, not unauditioned waveform descriptions.
    if (cue.cue == 0x9f)
        return "Gaze of Terror · opening";
    if (cue.cue == 0xa0)
        return "Gaze of Terror · impact";
    if (cue.cue == 0xaf)
        return "Petrify · effect cue";
    return QString("%1 · sound %2").arg(usage).arg(ordinal);
}
PoseAudioTrace tracePoseAudio(const Rom &rom, const ActorArt &art, int state, int variant) {
    PoseAudioTrace result;
    if (!art.isBody())
        throw Error("Choose a class animation for its sound cues");
    ActorPosePlayer player(rom, art, state, variant);
    // This records unresolved alternate cues without claiming a native linked
    // child exists or inventing its equipment/spatial resource selection.
    player.setLinkedAudioContext({0x80600000, art.sourceClass(), art.ownerClass()});
    QSet<QString> visited;
    int currentTick = 0;
    QVector<AudioCue> step;
    player.setAudioObserver([&](int opcode, int key, int request) {
        step.append(
            {currentTick, key, request, opcode <= 18 ? 0 : 1, opcode == 18 || opcode == 20});
    });
    for (int tick = 0; tick < 1800; ++tick) {
        currentTick = tick;
        step.clear();
        if (player.advance() != LORDLY_POSE_OK)
            throw Error(player.error());
        const auto &pose = player.actor();
        const auto key =
            QString("%1/%2/%3/%4").arg(pose.state).arg(pose.cursor).arg(pose.delay).arg(pose.frame);
        if (visited.contains(key)) {
            result.boundary = "First animation cycle; cues repeat when the animation loops.";
            return result;
        }
        visited.insert(key);
        result.cues += step;
        result.ticks = tick + 1;
        if (!player.result())
            return result;
    }
    result.boundary = "Stopped at 1,800 preview ticks; later cues are not listed.";
    return result;
}
} // namespace lordly
