/* Primary resident event-VM service12 only. Native menu callers also use
 * IDs528..530 in the original bit accessor; its global entry stays unchanged.
 * These wrappers are linked into the permanently mapped save runtime.
 */
extern unsigned char lc_custom_story_flags[];
int lc_native_story_flag_get(int id);
void lc_native_story_flag_set(int id, int unused, int value);

int lc_event_story_flag_get(int raw_id)
{
    int id = (short)raw_id;
    unsigned int index;
    int invert = id < 0;
    if (invert) id = -id;
    if (id < 256) return lc_native_story_flag_get((short)raw_id);
    index = (unsigned int)id - 256U;
    if (index >= 512U) return 0;
    return ((lc_custom_story_flags[index >> 3] >> (index & 7)) & 1) ^ invert;
}

void lc_event_story_flag_set(int id, int unused, int value)
{
    unsigned int index, mask;
    if ((unsigned int)id < 256U) {
        lc_native_story_flag_set(id, unused, value);
        return;
    }
    index = (unsigned int)id - 256U;
    if (index >= 512U) return;
    mask = 1U << (index & 7);
    lc_custom_story_flags[index >> 3] = (unsigned char)
        ((lc_custom_story_flags[index >> 3] & ~mask) | (value == 1 ? mask : 0));
}
