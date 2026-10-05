#ifndef LORDLY_SCENE_AUDIO_H
#define LORDLY_SCENE_AUDIO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Reconstructed scene-side audio routing. Resident voices and synthesis remain
 * external services. Request values keep their full words until the consumer. */
enum {
    LORDLY_AUDIO_PENDING,
    LORDLY_AUDIO_REGISTER,
    LORDLY_AUDIO_DIRECT,
    LORDLY_AUDIO_RELEASE,
    LORDLY_AUDIO_RESET,
    LORDLY_AUDIO_GAIN,
    LORDLY_AUDIO_BUS_GAIN_PENDING,
    LORDLY_AUDIO_CONTROL,
    LORDLY_AUDIO_TIME,
    LORDLY_AUDIO_PAN,
    LORDLY_AUDIO_PITCH,
    LORDLY_AUDIO_BUS_GAIN
};
typedef struct {
    int16_t route;
    uint32_t voices[7];
    int32_t controller_suppressed;
    int scene_present;
} LordlyAudioInputs;
typedef struct {
    uint8_t pending[3];
    int envelope_present;
    int16_t step, level, remaining, group;
    uint32_t gains[2];
} LordlySceneAudio;
typedef void (*LordlyAudioSink)(void *, int kind, int context, uint32_t value, uint32_t extra);
int lordly_audio_command(LordlySceneAudio *, const LordlyAudioInputs *, uint32_t opcode,
                         const uint32_t args[4], LordlyAudioSink, void *);
void lordly_audio_advance(LordlySceneAudio *, const LordlyAudioInputs *, LordlyAudioSink, void *);
#ifdef __cplusplus
}
#endif
#endif
