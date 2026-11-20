typedef unsigned char u8;
void boot_display_list_counter_step(void);
extern u8 D_800AEF99;
void boot_display_list_counter_packet_emit(u8 alpha);
void boot_display_list_counter_step(void) {
    if (D_800AEF99 != 0) {
        if (D_800AEF99 > 12) {
            D_800AEF99 = 12;
        }
        boot_display_list_counter_packet_emit(D_800AEF99 * 255 / 12);
    }
}
void boot_display_list_counter_packet_emit(u8 alpha);
