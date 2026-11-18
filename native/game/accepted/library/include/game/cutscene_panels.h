#ifndef OB64_GAME_CUTSCENE_PANELS_H
#define OB64_GAME_CUTSCENE_PANELS_H

#include "common/types.h"

typedef struct PanelParameters {
    u16 *field00;
    u8 *field04;
    void *field08;
    void *field0C;
    void *field10;
    void *field14;
    u16 field18;
    u16 field1A;
    u16 field1C;
    u8 pad1E[2];
    u16 field20;
    u16 field22;
    u8 field24[0x40];
    u8 field64;
    u8 field65;
    u8 field66;
    u8 field67;
} PanelParameters;

typedef struct PanelContent {
    u8 text[0x500];
    u8 pad500[0x30];
    u8 *rows[48];
    u8 field5F0;
    u8 field5F1;
    u8 field5F2;
    u8 field5F3;
    u8 field5F4;
    u8 field5F5;
    u8 field5F6;
    u8 field5F7;
    u8 pad5F8[4];
    u8 field5FC;
    u8 field5FD;
    u8 field5FE;
    u8 pad5FF;
    s32 key;
} PanelContent;

typedef struct Panel {
    struct Panel *link00;
    struct Panel *link04;
    u8 pad08[0xC];
    u8 flags;
    u8 pad15;
    u16 field16;
    s16 field18;
    u8 pad1A[8];
    u8 field22;
    u8 field23;
    s8 field24;
    u8 field25;
    u8 pad26[6];
    s32 field2C;
    s32 field30;
    s32 field34;
    s32 field38;
    u8 pad3C[4];
    s32 field40;
    u8 pad44[4];
    s32 field48;
    s32 field4C;
    s32 field50;
    s32 field54;
    s32 field58;
    u8 pad5C[0x74];
    PanelContent *content;
} Panel;

typedef struct PanelSlot {
    s32 key;
    void *field04;
    void *field08;
    Panel *panel;
    void *field10;
    s16 status;
    u8 pad16[2];
} PanelSlot;

typedef struct PanelScene {
    u8 pad0000[0x19B8];
    PanelSlot *slots[14];
} PanelScene;

typedef struct PanelSubject {
    u8 pad00[0x2C];
    s16 field2C;
} PanelSubject;

typedef struct PanelPositionState {
    PanelSubject *subjects[0x23C / 4];
    u8 field23C;
    u8 field23D;
    u8 field23E;
    u8 position;
} PanelPositionState;

extern PanelParameters *D_8023DE34;
extern PanelScene *D_8022A974;
extern Panel *D_8018FDC0;
extern PanelPositionState *D_8023DE30;

typedef void (*PanelCallback)(Panel *, s32);

void func_00297CC8(s32 slot, s32 preset);
void func_00283748(s32 slot);
s32 func_002836C8(s32 slot);
PanelContent *func_00296518(Panel *panel, s32 key);
PanelContent *func_00296754(Panel *panel, s32 section);
void func_00297644(Panel *panel, s32 index);
void func_002978F4(Panel *panel, s32 index);

void func_00281BD8(s32 slot);
void func_00281C78(s32 slot);
void func_00281CE4(s32 slot);
void func_00281ED0(s32 slot);

void func_0028adf4(void);
void func_0028af64(void);
void func_0028b0e0(void);

#endif
