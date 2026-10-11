typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

union EndMarker {
    struct {
        unsigned int padding : 16;
        unsigned int value : 16;
    } bits;
    u32 whole;
};

extern s16 func_00046738(s32 arg0);
extern s32 func_00046854(s32 arg0);
extern s32 func_00048024(void);
extern void func_00048294(void);
extern void resource_free(s32 resource);

extern volatile u8 D_80197B03;
extern volatile s32 D_80197B0C;
extern volatile s32 D_80197B10;
extern volatile s32 D_80197B18;
extern volatile s32 D_80197B1C;

s32 func_00047a94(s32 arg0)
{
    volatile s32 *active_flags;
    s32 var_s2_25;
    union EndMarker end_marker;
    s8 *temp_s5_37;
    s32 one;
    s32 two;
    s8 *state_index;
    s32 temp_s0_48;
    s32 temp_v0_61;
    s32 temp_v1_112;
    u32 requested_flags;
    s32 var_v0_176;
    u8 temp_v0_93;
    s32 var_v0_131;
    volatile s32 *flags_temp;
    volatile u8 *phase_flag;
    s32 tail_one;
    s32 remaining_flags;
    void *helper_label_address;

    helper_label_address = &&helper_result_one;

loop_1:
    if (D_80197B03 == 0) {
        var_s2_25 = *(s32 *)0x80197B1C;
    } else {
        var_s2_25 = *(s32 *)0x80197B18;
    }

    flags_temp = &D_80197B0C;
    if (*flags_temp & 0x3F) {
        s8 *temp_s4_40;

        state_index = (s8 *)flags_temp - 0xC;
        temp_s5_37 = (s8 *)flags_temp - 0xB;
        one = 1;
        two = 2;
        temp_s4_40 = (s8 *)flags_temp - 8;
        active_flags = flags_temp;

loop_6:
        temp_s0_48 = var_s2_25 + *(u16 *)(var_s2_25 + (*(u8 *)state_index * 2));
        if (*(u16 *)(temp_s0_48 + (*(u8 *)(state_index + 1) * 2)) != 0xFFFF) {
            end_marker.bits.padding = 0;
            end_marker.bits.value = 0xFFFF;

loop_8:
            temp_v0_61 = func_00046854(
                var_s2_25 + *(u16 *)(temp_s0_48 + (*(u8 *)temp_s5_37 * 2)));
            if (temp_v0_61 == one) {
                goto helper_result_one;
            }
            if (temp_v0_61 == two) {
                goto loop_1;
            }
            goto scanner_continue;

helper_result_one:
            if (*(u8 *)0x801936A8 == 0) {
                goto helper_cleanup;
            }
            goto helper_deferred_return;

helper_cleanup:
            resource_free(D_80197B18);
            resource_free(D_80197B1C);
            D_80197B18 = 0;
            D_80197B1C = 0;
            *(s16 *)(temp_s5_37 + 5) = 0;
            func_00048294();
            *(s32 *)(temp_s5_37 + 0xB) = one;
            return func_00048024();

scanner_continue:
            *(s16 *)temp_s4_40 = 0;
            temp_v0_93 = *(u8 *)(temp_s4_40 - 3) + 1;
            do {
                *(u8 *)(temp_s4_40 - 3) = temp_v0_93;
            } while (0);
            if (*(u16 *)(temp_s0_48 + ((temp_v0_93 & 0xFF) * 2)) == end_marker.whole) {
                goto block_14;
            }
            goto loop_8;
        } else {
block_14:
            *active_flags &=
                ~*(s16 *)(0x8018F2F8 + (*(u8 *)((s8 *)active_flags - 0xC) * 2));
            temp_v1_112 = D_80197B0C;
            if (temp_v1_112 & 1) {
                *(s8 *)0x80197B00 = 0;
            } else if (temp_v1_112 & 2) {
                *((s8 *)active_flags - 0xC) = one;
            } else if (temp_v1_112 & 4) {
                *((s8 *)active_flags - 0xC) = two;
            } else if (temp_v1_112 & 8) {
                var_v0_131 = 3;
                *((s8 *)active_flags - 0xC) = var_v0_131;
            } else if (temp_v1_112 & 0x10) {
                var_v0_131 = 4;
                *((s8 *)active_flags - 0xC) = var_v0_131;
            } else if (temp_v1_112 & 0x20) {
                var_v0_131 = 5;
                *((s8 *)active_flags - 0xC) = var_v0_131;
            }

no_active_state:
            remaining_flags = D_80197B0C;
            *(s8 *)0x80197B01 = 0;
            *(s16 *)0x80197B04 = 0;
            if (!(remaining_flags & 0x3F)) {
                goto block_25;
            }
            goto loop_6;
        }
    } else {
block_25:
        phase_flag = &D_80197B03;
        tail_one = 1;
        if (*phase_flag != 0) {
            goto block_36;
        }

        arg0 = D_80197B10;
        *phase_flag = tail_one;
        D_80197B0C = arg0;

        requested_flags = arg0++;
        arg0--;
        requested_flags++;
        requested_flags--;

        if (!(requested_flags & 1)) {
            goto request_bit_2;
        }
        *(s8 *)0x80197B00 = 0;
        goto request_state_done;

request_bit_2:
        if (!(requested_flags & 2)) {
            goto request_bit_4;
        }
        *(s8 *)0x80197B00 = tail_one;
        goto request_state_done;

request_bit_4:
        var_v0_176 = requested_flags & 4;
        if (var_v0_176) {
            var_v0_176 = 2;
            goto store_request_state;
        }
        var_v0_176 = requested_flags & 8;
        if (var_v0_176) {
            var_v0_176 = 3;
            goto store_request_state;
        }
        var_v0_176 = requested_flags & 0x10;
        if (var_v0_176) {
            var_v0_176 = 4;
            goto store_request_state;
        }
        var_v0_176 = arg0 & 0x20;
        if (!var_v0_176) {
            goto request_state_done;
        }
        var_v0_176 = 5;

store_request_state:
        *(s8 *)0x80197B00 = var_v0_176;

request_state_done:
        *(s8 *)0x80197B01 = 0;
        *(s16 *)0x80197B04 = 0;
        goto loop_1;

block_36:
        if (!(D_80197B10 & 8)) {
            goto block_39;
        }
        *(u16 *)0x8018F1A0 = 0x8000;
        *(s16 *)0x8018F1A2 = func_00046738(0xFF);
        return D_80197B10 & 8;

helper_deferred_return:
        return *(s32 *)(temp_s5_37 + 0x13);

block_39:
        if (*(u16 *)0x8018F1A0 == 0) {
            *(s16 *)0x8018F1A2 = func_00046738(0xFF);
        }
        return 0;
    }
}
