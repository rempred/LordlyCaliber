typedef unsigned char u8;

extern void *D_801CE8BC;
extern u8 *D_801CE8C0;

void *func_80070F30(int size);
void func_800712C4(void *pointer);
void func_00023460(void *destination, void *source, int size);
void func_801EFAAC(void);
void func_0021C3B0(void);

void func_002158E4(void)
{
    register void *owner;
    u8 saved_value;
    u8 *snapshot;

    snapshot = func_80070F30(0x6094);
    owner = D_801CE8BC;
    /* KMC scheduling workaround: keep these identical operations in both
     * arms. A full null test propagates zero into the copy argument and
     * leaves a branch; this bit test disappears and preserves retail order.
     * The bit has no inferred meaning, and both paths perform the same work.
     */
    if ((unsigned int)owner & 1) {
        *(int *)(D_801CE8C0 + 0x814) = 0;
        func_00023460(owner, snapshot, 0x6094);
        saved_value = D_801CE8C0[0x82E];
    } else {
        *(int *)(D_801CE8C0 + 0x814) = 0;
        func_00023460(owner, snapshot, 0x6094);
        saved_value = D_801CE8C0[0x82E];
    }

    func_801EFAAC();
    D_801CE8C0[0x82E] = saved_value;
    func_00023460(snapshot + 0x1C4, (u8 *)D_801CE8BC + 0x1C4, 0x1360);
    func_00023460(snapshot + 0x1524, (u8 *)D_801CE8BC + 0x1524, 0x3C00);
    func_00023460(snapshot + 0x5124, (u8 *)D_801CE8BC + 0x5124, 0x19C);
    func_00023460(snapshot + 0x52C0, (u8 *)D_801CE8BC + 0x52C0, 0x400);
    *(int *)((u8 *)D_801CE8BC + 0x56C0) = *(int *)(snapshot + 0x56C0);
    func_800712C4(snapshot);
    func_0021C3B0();
}
