typedef struct PiManagerView {
    int field_00;
    void *field_04;
    void *commandQueue;
} PiManagerView;
extern PiManagerView D_800AA400;

void *func_00029cc0(void)
{
    if (!D_800AA400.field_00)
        return 0;
    return D_800AA400.commandQueue;
}
