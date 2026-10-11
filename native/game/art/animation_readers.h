#ifndef LORDLY_ANIMATION_READERS_H
#define LORDLY_ANIMATION_READERS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Nonzero return means invalid/truncated host input. Source and owner groups
 * have already been resolved from the game's directory into bounded slices. */
int lordly_action_selector(int source_matches_owner,
    const uint8_t *source, size_t source_size, const uint8_t *owner, size_t owner_size,
    unsigned action, unsigned mode, uint8_t *selector);
typedef struct { uint8_t opcode, width, operands[3]; } LordlyPoseCommand;
/* Linear bulk adaptation of the counted instruction walk. Output is private
 * scratch until the caller accepts success; an empty slice aliases a later pose. */
int lordly_pose_decode(const uint8_t *program, size_t size, const uint8_t *widths,
    size_t width_count, LordlyPoseCommand commands[255], unsigned *count);
#ifdef __cplusplus
}
#endif
#endif
