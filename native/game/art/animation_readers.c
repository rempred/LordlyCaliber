#include "animation_readers.h"
#include "pose_operands.h"

/* Portable adaptation of accepted func_00201798. Resolved slices replace the
 * absolute class/directory pointers; zero source_size represents offset FFFF.
 * Keep full-byte class matching, owner fallback, FF termination and default28.
 * OBSO overrides are applied by the caller after this base selection. */
int lordly_action_selector(int source_matches_owner,
    const uint8_t *source, size_t source_size, const uint8_t *owner, size_t owner_size,
    unsigned action, unsigned mode, uint8_t *selector) {
    const uint8_t *entry = owner;
    size_t size = owner_size, at = 0;
    if (!selector || action > 255 || mode > 2) return 1;
    if (source_matches_owner && source_size) {
        entry = source;
        size = source_size;
    }
    if (!entry) return 1;
    while (at < size) {
        unsigned key = entry[at];
        if (key == 0xff) { *selector = 0x28; return 0; }
        if (size - at < 4) return 1;
        if (key == action) { *selector = entry[at + mode + 1]; return 0; }
        at += 4;
    }
    return 1;
}

/* Accepted func_00204F34/002050AC share this counted instruction traversal.
 * Decode once into the editor cache instead of rescanning from record zero for
 * every playback tick. Host bounds and width22=3 are editor extensions. */
int lordly_pose_decode(const uint8_t *program, size_t size, const uint8_t *widths,
    size_t width_count, LordlyPoseCommand commands[255], unsigned *count) {
    size_t at = 1;
    unsigned index, total;
    if (!program || !size || !widths || !commands || !count) return 1;
    total = *program;
    for (index = 0; index < total; ++index) {
        unsigned token, width;
        LordlyPoseCommand *out = commands + index;
        if (at >= size) return 1;
        token = program[at];
        if (token >= width_count) return 1;
        width = widths[token];
        if (!width || width > 4 || width > size - at) return 1;
        out->operands[0] = out->operands[1] = out->operands[2] = 0;
        out->opcode = (uint8_t)lc_pose_operands(program + at, width,
            out->operands, out->operands + 1, out->operands + 2);
        out->width = (uint8_t)width;
        at += width;
    }
    if (at != size) return 1;
    *count = total;
    return 0;
}
