#ifndef LC_BATTLE_STATE_H
#define LC_BATTLE_STATE_H
/* High Attack moves the stream trailer below the enlarged packet buffer.
 * Only explicit state fields move; packet bytes and actor records never do.
 * The two bases and the 0x7000 displacement are the maintained v23 layout. */
static inline void *lc_battle_tail(void *base)
{
    unsigned int address = (unsigned int)base;
    return (void *)(address == 0x80450000 || address == 0x8044a820
                    ? address - 0x7000 : address);
}
static inline void *lc_battle_field(void *base, unsigned int offset)
{
    unsigned int address = (unsigned int)base;
    if ((address == 0x80450000 && offset >= 0x810 && offset < 0x910) ||
        (address == 0x8044a820 && offset >= 0x5ff0 && offset < 0x60f0))
        address -= 0x7000;
    return (void *)(address + offset);
}
#endif
