#ifndef LORDLY_DIRECTOR_STREAM_WORDS_H
#define LORDLY_DIRECTOR_STREAM_WORDS_H
#include "control_flow.h"
#ifdef __cplusplus
extern "C" {
#endif
/* PC adaptation of the raw-word translation loop in func_00283FA8.c.
 * known[] distinguishes a supplied zero from unavailable gameplay state.
 * Returns 0 on success, 1 for a missing index, 2 for invalid arguments.
 * A missing input leaves all words unchanged. */
int lordly_director_translate(LordlyDirectorWord *words, int count,
                             const unsigned short *values, const unsigned char *known,
                             int *missing_index);
#ifdef __cplusplus
}
#endif
#endif
