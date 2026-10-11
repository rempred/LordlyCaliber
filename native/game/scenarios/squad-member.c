/* Adapted from accepted func_0019554C.c. Original pool selection, initialization,
 * equipment order, leader flags, name override and player-record tail remain.
 * An LS template appends one 35-byte record per follower in the same resource.
 * The unchanged constructor publishes prior members before each helper call.
 */
typedef unsigned char u8;
typedef unsigned short u16;
extern u8 lc_player_records[], lc_enemy_records[], lc_classes[], lc_sources[];
extern u8 lc_scenario_level;
extern const char lc_special_name[];
extern void lc_initialize(u8 *, u8);
extern void lc_equipment(u16, u8 *);
extern void lc_finish_character(int, u8);
extern int lc_special_scenario(void);
extern void lc_clear(void *, int);
extern char *lc_copy_name(char *, const char *);

void lc_scenario_member(int index, const u8 *source, int selector, u8 flag, u8 source_id)
{
    u8 *record;
    const u8 *spec;
    int offset, member;
    u8 cls, copy;
    record = (flag || source_id >= 30) ? lc_enemy_records + index * 52 : lc_player_records + index * 56;
    offset = selector == 0 ? 0 : selector == 1 ? 7 : 16;
    spec = source + offset;
    if (selector && source[25] == 'L' && source[26] == 'S') {
        member = 1;
        while (member < 5 && lc_sources[source_id * 25 + member + 2]) ++member;
        if (member < 5) {
            offset = (source[25 + member * 2] << 8) | source[26 + member * 2];
            spec = source + offset * 35;
        }
    }
    cls = spec[0];
    record[0x11] = cls;
    copy = lc_classes[cls * 72 + 69];
    record[0x12] = copy ? copy : cls;
    lc_initialize(record, lc_scenario_level + spec[1]);
    *(u16 *)(record + 0x2a) = 0;
    *(u16 *)(record + 0x2c) = 0;
    *(u16 *)(record + 0x2e) = 0;
    *(u16 *)(record + 0x30) = 0;
    lc_equipment((spec[2] << 8) | spec[3], record);
    lc_equipment((spec[4] << 8) | spec[5], record);
    record[0x32] = 0;
    record[0x33] = selector == 0 ? 0x82 : 0x80;
    lc_finish_character(flag != 0 || source_id >= 30, index);
    if (lc_special_scenario() && source_id == 30 && selector == 0) {
        lc_clear(record, 17);
        lc_copy_name((char *)record, lc_special_name);
    }
    if (!flag && source_id < 30) {
        record[0x34] = index;
        record[0x35] = record[0x36] = record[0x37] = 0;
    }
}
