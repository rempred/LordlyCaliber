/* Authored maps without enemy squads win by liberating their declared enemy
 * stronghold. Ordinary missions retain the native boss/army predicate.
 * Site flags and local indexing are from accepted func_00127830 and
 * func_00108AA0_group; no persistent story flags are repurposed. */
#include "mission-victory-config.h"
typedef unsigned char u8;
typedef unsigned short u16;
extern int lc_native_mission_victory(void);
extern int lc_native_mission_briefing_site(void);
static const u8 mission_rule[] = LC_OBJECTIVES;
static const u8 briefing_objective[] = LC_BRIEFING_OBJECTIVES;
int lc_authored_mission_victory(void) {
    const unsigned key = *(volatile u8 *)0x8018f481;
    const unsigned objective = key < sizeof(mission_rule) ? mission_rule[key] : 0;
    int result;
    if (objective & 128) {
        *(volatile u8 *)0x8018f481 = objective & 127;
        result = lc_native_mission_victory();
        *(volatile u8 *)0x8018f481 = key;
        return result;
    }
    if (!objective) return lc_native_mission_victory();
    if (objective > *(volatile unsigned *)0x801f1070) return 0;
    return (*(volatile u16 *)(0x801951cc + (objective - 1) * 36) & 6) == 2;
}

int lc_authored_mission_briefing_site(void) {
    const unsigned key = *(volatile u8 *)0x8018f481;
    const unsigned count = *(volatile unsigned *)0x801f1070;
    const volatile unsigned *squad = *(volatile unsigned *volatile *)0x801f0d28;
    unsigned objective;
    if (key >= sizeof(mission_rule) || !mission_rule[key]) return lc_native_mission_briefing_site();

    // Retail finds a site under opposing squad 30. An authored mission can
    // have an enemy stronghold without that squad, or move it independently.
    if (squad && (*squad & 0x10)) {
        const int site = lc_native_mission_briefing_site();
        if ((unsigned)site < count) return site;
    }
    if (!count) return -1;
    objective = briefing_objective[key];
    return objective && objective <= count ? objective - 1 : 0;
}

unsigned lc_authored_mission_exit(void) {
    /* A directly completed mission has no Director scene to forward the VM
     * request. The native field exit otherwise pops the mode stack. Its root
     * can be the battlefield because opening the mission replaced the map. */
    if (*(volatile int *)0x801ab84c == -2 &&
        (*(volatile u16 *)0x801e7d62 & 0xff00) == 0x0300 &&
        !*(volatile u8 *)0x801936a8 && !*(volatile u16 *)0x8018f1a0 &&
        *(volatile u16 *)0x8018f1a2 == 0x8005) {
        struct ModeFrame {const struct ModeFrame *parent; u16 mode;};
        const struct ModeFrame *frame = *(const struct ModeFrame *volatile *)0x800c4bbc;
        if (frame && frame->parent && frame->parent->mode == 5) return 0xfffe;
        return 0x8005;
    }
    return *(volatile u16 *)0x800e810e ? 0 : 0xfffe;
}
