typedef struct MesgQueue MesgQueue;
extern MesgQueue D_800E9BC8;
extern void *D_800C35A0[];
extern int D_800AA780;
extern void osCreateMesgQueue(MesgQueue *queue, void **messages, int count);
extern int osSendMesg(MesgQueue *queue, void *message, int flags);
extern int osRecvMesg(MesgQueue *queue, void **message, int flags);

/* The callable initializer is also embedded in the access acquisition body. */
__inline__ void func_00024b70(void)
{
    D_800AA780 = 1;
    osCreateMesgQueue(&D_800E9BC8, D_800C35A0, 1);
    osSendMesg(&D_800E9BC8, 0, 0);
}

void func_00024bc4(void)
{
    void *message;
    if (!D_800AA780)
        func_00024b70();
    osRecvMesg(&D_800E9BC8, &message, 1);
}

void func_00024c30(void)
{
    osSendMesg(&D_800E9BC8, 0, 0);
}
