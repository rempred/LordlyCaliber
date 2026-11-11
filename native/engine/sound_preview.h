#pragma once
#include "engine/rom.h"
#include "engine/attack_audio.h"
#include "engine/audio_cues.h"
#include <atomic>
#include <memory>
namespace lordly {
struct SoundPart {
    int tick = 0, voice = 0; // Native audio clock: 60 ticks/second, unlike effect ticks.
    quint32 sampleRomOffset = 0;
    QByteArray pcm; // Stereo 32 kHz, starts at this part's first sample (no leading silence).
    QByteArray state; // Qualified pre-update native channel state + note identity.
};
struct SoundPreview {
    QByteArray pcm; // Interleaved little-endian signed 16-bit stereo, 32 kHz.
    int voices = 0, ticks = 0;
    bool limited = false;
    int unresolved = 0;
    QVector<SoundPart> parts;
};
// Original libmus scheduling, sample selection and modulation; host dry mixer.
SoundPreview renderSoundPreview(const Rom &, int request,
                                const std::shared_ptr<std::atomic_bool> &cancel = {}, bool parts = false);
SoundPreview renderImportedSoundPreview(const Rom &,const AttackSound &,
                                const std::shared_ptr<std::atomic_bool> &cancel = {}, bool parts = false);
SoundPreview renderAuthoredSound(const Rom &, const AttackSound &, int variant = 1,
                                const std::shared_ptr<std::atomic_bool> &cancel = {}, bool parts = true);
SoundPreview renderSoundTimeline(const Rom &, const QVector<AudioCue> &, int visualTicks,
                                const std::shared_ptr<std::atomic_bool> &cancel = {}, bool parts = false);
} // namespace lordly
