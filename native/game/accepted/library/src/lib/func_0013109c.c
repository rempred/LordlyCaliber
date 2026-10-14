typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef int s32;

extern u8 D_8018F481;
extern u8 D_801951B0[];
extern u16 D_801951CC[][18];
extern u8 D_801E7E13[], D_801E7E14[];
extern void *D_801F0CB0[];
extern s32 func_0005c110(s32);
extern s32 func_00045df4(s16);
extern void func_801B2B78(void *);
/* The address is shared by separately loaded images. Keep this call neutral. */
extern void func_80214F80(u16, s32, s8, u8);

/* Keeping the output addresses in the inline helper retains the retail byte
 * stack slots and the signed load of out0 after the unit loop in KMC 2.7.2. */
static __inline__ void site_outputs(u16 site, s8 *out0, u8 *out1)
{
    u16 flags = *(u16 *)(D_801951B0 + site * 36 + 28);

    if (flags & 2) {
        *out0 = D_801E7E13[D_8018F481 * 12];
        if (func_0005c110(0x11)) {
            if (func_00045df4(0x6B)) {
                *out0 = 1;
            } else {
                *out0 = 4;
            }
        }
        *out1 = 1;
    } else if (flags & 4) {
        *out0 = D_801E7E14[D_8018F481 * 12];
        *out1 = 2;
    } else {
        *out0 = -1;
        *out1 = 0;
    }
}

void func_0013109C(s32 site)
{
    s8 out0;
    u8 out1;
    s32 scenario;
    s32 scenario2;
    s32 site_id;
    s32 selection;
    s32 index;
    void **units;
    u16 flags;

    scenario = D_8018F481;
    site_id = (u16)site;
    /* Separate equal-result arms retain the individual retail comparisons;
     * joining the first or last pair with || folds them into range tests. */
    if ((scenario == 0x2D) && (site_id == 7)) {
        selection = 4;
    } else if ((scenario == 0x2E) && (site_id == 7)) {
        selection = 4;
    } else if (((scenario == 0x32) && (site_id == 6)) ||
               ((scenario == 0x14) && (site_id == 11)) ||
               ((scenario == 0x15) && (site_id == 11))) {
        selection = 5;
    } else if ((scenario == 0x25) && (site_id == 6)) {
        selection = 6;
    } else if ((scenario == 0x26) && (site_id == 6)) {
        selection = 6;
    } else {
        flags = *(u16 *)(D_801951B0 + site_id * 36 + 28);
        if (flags & 0x800) {
            selection = 10;
        } else if (flags & 0x400) {
            selection = 9;
        } else if (flags & 0x20) {
            selection = 0;
        } else {
            /* A register halfword retains the fresh truncation/index sequence.
             * The flags view is the existing 36-byte record alias. */
            register u16 second_site = site;
            u16 flags2 = D_801951CC[second_site][0];

            if (flags2 & 0x40) {
                selection = 1;
            } else {
                selection = (flags2 & 0x80) ? 3 : 2;
            }
        }
    }

    scenario2 = D_8018F481;
    if ((u8)scenario2 == 0x27) {
        if ((u16)site == 0) {
            selection = 6;
        } else if ((u16)site == 1) {
            selection = 7;
        } else {
            selection = 8;
        }
    } else if ((u32)(scenario2 - 0x2F) < 2 ||
               (u32)(scenario2 - 0x3C) < 2) {
        if ((u16)site == 0) {
            selection = 4;
        } else if ((u16)site == 5) {
            selection = 8;
        } else {
            selection = 7;
        }
    } else if ((u8)scenario2 == 0x33) {
        if ((u16)site == 0) {
            selection = 5;
        } else if ((u16)site == 3) {
            selection = 8;
        } else {
            selection = 7;
        }
    }

    site_outputs((u16)site, &out0, &out1);

    index = 0;
    units = D_801F0CB0;
    do {
        void *unit = *units++;

        index++;
        func_801B2B78(unit);
    } while (index < 50);
    func_80214F80(site, selection, out0, out1);
}
