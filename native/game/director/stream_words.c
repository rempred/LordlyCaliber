/* The original loader substitutes every matching word, not only operands.
 * Two bounded passes add failure atomicity when host launch context is missing. */
#include "stream_words.h"
int lordly_director_translate(LordlyDirectorWord *words, int count,
                             const unsigned short *values, const unsigned char *known,
                             int *missing_index)
{
    int i;
    if (!words || !values || !known || !missing_index || count < 0 || count > 0x400000)
        return 2;
    for (i = 0; i < count; ++i)
        if ((words[i] & 0xffffff00U) == 0x08880000U && !known[words[i] & 255U]) {
            *missing_index = words[i] & 255U;
            return 1;
        }
    for (i = 0; i < count; ++i)
        if ((words[i] & 0xffffff00U) == 0x08880000U)
            words[i] = values[words[i] & 255U];
    return 0;
}
