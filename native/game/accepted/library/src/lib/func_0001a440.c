typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 errStatus;
    void *dramAddr;
    void *C2Addr;
    u32 sectorSize;
    u32 C1ErrNum;
    u32 C1ErrSector[4];
} __OSBlockInfo;
typedef struct {
    u32 cmdType;
    u16 transferMode;
    u16 blockNum;
    s32 sectorNum;
    u32 devAddr;
    u32 bmCtlShadow;
    u32 seqCtlShadow;
    __OSBlockInfo block[2];
} __OSTranxInfo;
typedef struct OSPiHandle_s {
    struct OSPiHandle_s *next;
    u8 type;
    u8 latency;
    u8 pageSize;
    u8 relDuration;
    u8 pulse;
    u8 domain;
    u32 baseAddress;
    u32 speed;
    __OSTranxInfo transferInfo;
} OSPiHandle;
extern OSPiHandle *D_800E79AC;
extern OSPiHandle D_800BE110;
s32 func_0001c0e0(OSPiHandle *eph);
void bzero(void *p, s32 n);
void func_0001a440(void) {
    if (D_800BE110.baseAddress == 0xA8000000) {
        return;
    }
    D_800BE110.type = 3;
    D_800BE110.baseAddress = 0xA8000000;
    D_800BE110.latency = 5;
    D_800BE110.pulse = 0xC;
    D_800BE110.pageSize = 0xD;
    D_800BE110.relDuration = 2;
    D_800BE110.domain = 1;
    D_800BE110.speed = 0;
    bzero(&D_800BE110.transferInfo, sizeof(D_800BE110.transferInfo));
    func_0001c0e0(&D_800BE110);
    D_800E79AC = &D_800BE110;
}
