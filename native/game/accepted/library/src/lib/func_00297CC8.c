#include "game/cutscene_panels.h"

extern u8 D_8023E5FF[];

extern void *resource_alloc(u32);
extern void func_80093380(void *, u32);
extern void *func_0002CD70(void *, s32, u32);
extern s32 func_00045df4(s16);
extern void func_00047ee0(s32, s32);
extern u8 *func_0028ab74(s32);
extern void func_0028F918(s32);

typedef struct PanelRect {
    s16 left, top, right, bottom;
} PanelRect;

extern Panel *func_00051eb0(u16, u8, PanelCallback, s16, s16, s16, s16, s16, s16, s16, s16);
extern Panel *func_00051f74(s32, s32, PanelCallback, s32, s32, s32, s32);
extern void func_00296F98(Panel *, s32);
extern void func_0029700C(Panel *, s32);
extern void func_0029709C(Panel *, s32);
extern void func_0029712C(Panel *, s32);
extern void func_002971B4(Panel *, s32);
extern void func_0029739C(Panel *, s32);
extern void func_002974E8(Panel *, s32);
extern void func_0029779C(Panel *, s32);
extern void func_00052484(Panel *, s32);

extern void *func_00294940(s32, u16 *, PanelSlot *);
extern void func_00294B8C(PanelContent *, u8, PanelSlot *);
extern void *func_00294C54(s32, u8, u16 *, PanelSlot *);
extern void *func_00294FAC(void);
extern s32 func_00294FEC(s32);
extern void func_002950A0(PanelContent *, u8, PanelSlot *);
extern void func_002952D4(PanelContent *, s32);
extern s32 func_002953E0(s32);
extern s32 func_002954B0(u16);
extern void func_002956F8(PanelContent *, u8, PanelSlot *);
extern void func_00295A54(PanelContent *, s32);
extern void func_00295B9C(PanelContent *, u16);
extern PanelContent *func_0029636C(Panel *, s32);
extern PanelContent *func_00296518(Panel *, s32);
extern PanelContent *func_002969B0(Panel *, s32, void *, u16);
extern PanelContent *func_00296AB4(Panel *, s32, PanelSlot *);
extern PanelContent *func_00296C50(Panel *, s32, s32);
extern void func_00296DB0(Panel *, s32);

/* Keep separate flag-clear tails so KMC can merge them after scheduling. */
void func_00297CC8(s32 slot, s32 preset)
{
    Panel *panel = 0;
    register PanelSlot *record;
    PanelContent *content;
    PanelRect first, second;
    struct {
        u16 selection;
        /* Preserve the observed frame extent around the selection halfword. */
        u8 reserve[16];
    } frame;

    func_00283748(slot);
    record = resource_alloc(sizeof(*record));
    D_8022A974->slots[slot] = record;
    func_80093380(record, sizeof(*record));

    switch (preset) {
    case 4:
    case 7: {
        u16 initial;
        panel = func_00051eb0(0x400, 0, func_002978F4, 0x20, 0x5E, 0x20, 0x5E, 0x20, 0x5E, 0x78, 0xA0);
        content = func_00296518(panel, (preset + 0x400) | 0x20000);
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        initial = D_8023DE34->field18;
        if (initial != 0) {
            panel->field22 = initial;
        }
        if (func_00045df4(1) != 1) {
            PanelContent *current = panel->content;
            current->field5F4--;
            current->field5F1++;
            panel->field25--;
        }
        panel->field48 = panel->field40 + content->field5F6 * content->field5F4 + 8;
        break;
    }
    case 5:
        panel = func_00051f74(0, 0, func_00297644, 0x18, 0x13, 0x124, 0x50);
        if (D_8018FDC0->link04 != panel) {
            Panel *link04 = panel->link04;
            Panel *link00 = panel->link00;
            Panel *list;
            if (link04 != 0) {
                link04->link00 = link00;
            }
            list = D_8018FDC0;
            link00->link04 = link04;
            panel->link00 = 0;
            panel->link04 = list->link04;
            list->link04->link00 = panel;
            list->link04 = panel;
        }
        func_00296518(panel, 0xD)->key = 5;
        panel->flags |= 3;
        break;
    case 6:
        D_8023DE34->field65 = 0x80;
        panel = func_00051eb0(0x400, 0, func_002971B4, 0x122, 0x7A, 0x122, 0x7A, 0x7F, 0x7A, 0x122, 0xDC);
        content = func_00296AB4(panel, preset + 0x400, record);
        content->field5F5 = func_00294FEC(0x79);
        if (D_8023DE34->field1A != 0) {
            content->field5F1 = D_8023DE34->field20;
            content->field5FC = D_8023DE34->field1A;
            func_002950A0(content, content->field5F1, record);
        }
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        break;
    case 8:
    case 10: {
        D_8023DE30->field23D = D_8023DE30->field23E;
        func_0028F918(1);
        switch (D_8023DE30->position) {
        case 0:
            first.left = 0x9E;
            first.top = 0x9B;
            first.right = 0x126;
            first.bottom = 0xCD;
            break;
        case 1:
            first.left = 0x1C;
            first.top = 0x9B;
            first.right = 0x1C;
            first.bottom = 0xCD;
            break;
        case 2:
            first.left = 0x9E;
            first.top = 0x1D;
            first.right = 0x126;
            first.bottom = 0x1D;
            break;
        case 3:
            first.left = 0x1C;
            first.top = 0x1D;
            first.right = 0x1C;
            first.bottom = 0x1D;
            break;
        }
        {
            /* Indirect coordinate reads let KMC reuse the halfwords in the merged call tail. */
            s16 *left = &first.left;
            s16 *top = &first.top;
            if (preset == 8) {
                s16 right = first.right;
                s16 bottom = first.bottom;
                panel = func_00051eb0(0x100, 0, func_0029739C, right, bottom, right, bottom,
                                      *left, *top, (s16)(*left + 0x88), (s16)(*top + 0x32));
            } else {
                panel = func_00051eb0(0x100, 0, func_002974E8, 0x1C, 0x58, 0x122, 0xDC,
                                      *left, *top, (s16)(*left + 0x88), (s16)(*top + 0x32));
            }
        }
        func_00296DB0(panel, preset + 0x100);
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        break;
    }
    case 9: {
        s32 left, top;
        switch (D_8023DE30->position) {
        case 0:
            second.left = 0x9E;
            second.top = 0x9B;
            second.right = 0x126;
            second.bottom = 0xCD;
            break;
        case 1:
            second.left = 0x1C;
            second.top = 0x9B;
            second.right = 0x1C;
            second.bottom = 0xCD;
            break;
        case 2:
            second.left = 0x9E;
            second.top = 0x1D;
            second.right = 0x126;
            second.bottom = 0x1D;
            break;
        case 3:
            second.left = 0x1C;
            second.top = 0x1D;
            second.right = 0x1C;
            second.bottom = 0x1D;
            break;
        }
        left = second.left;
        top = second.top;
        panel = func_00051eb0(0x100, 0, func_0029779C, left, top,
                              (s16)(left + 0x88), (s16)(top + 0x32),
                              0x1C, 0x58, 0x122, 0xDC);
        content = func_0029636C(panel, preset + 0x100);
        panel->field24 = -2;
        panel->field23 = 0;
        panel->field22 = 0;
        content->field5F6 = 0x11;
        break;
    }
    case 11: {
        s32 mode;
        D_8023DE34->field65 = 0x80;
        panel = func_00051eb0(0x400, 0, func_002971B4, 0x11E, 0x5E, 0x11E, 0x5E, 0x7F, 0x5E, 0x11E, 0xDC);
        func_0002CD70(D_8023E5FF, 0xFF, 8);
        if (D_8023DE34->field18 != func_00045df4(1) + 1) {
            func_00047ee0(0x43, 5);
            mode = 3;
        } else {
            func_00047ee0(0x43, 4);
            mode = 6;
        }
        /* The one-iteration scope retains the retail call delay slot. */
        do {
            record->field08 = func_00294940(mode, &frame.selection, record);
        } while (0);
        content = func_00296AB4(panel, preset + 0x400, record);
        if (D_8023DE34->field1A != 0) {
            content->field5F1 = D_8023DE34->field20;
            content->field5FC = D_8023DE34->field1A;
            func_00294B8C(content, content->field5F1, record);
        }
        content->field5F5 = frame.selection;
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        break;
    }
    case 12: {
        s32 mode;
        u8 index;
        panel = func_00051eb0(0x100, 0, func_00052484, 0x1A, 0x58, 0x1A, 0x58, 0x1A, 0x58, 0x120, 0xDC);
        if (D_8023DE34->field18 != func_00045df4(1) + 1) {
            mode = 4;
            index = D_8023DE34->field67;
        } else {
            mode = 7;
            index = D_8023DE34->field67;
        }
        record->field08 = func_00294C54(mode, index, &frame.selection, record);
        /* Keep the reload after writing the returned pointer. */
        func_002969B0(panel, preset + 0x100, *(void *volatile *)&record->field08, frame.selection);
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        break;
    }
    case 13: {
        void *data;
        panel = func_00051eb0(0, 0, func_00296F98, 0x120, 0x60, 0x120, 0x76, 0xC2, 0x60, 0x120, 0x76);
        if (D_8018FDC0->link04 != panel) {
            Panel *link04 = panel->link04;
            Panel *link00 = panel->link00;
            Panel *list;
            if (link04 != 0) {
                link04->link00 = link00;
            }
            list = D_8018FDC0;
            link00->link04 = link04;
            panel->link00 = 0;
            panel->link04 = list->link04;
            list->link04->link00 = panel;
            list->link04 = panel;
        }
        data = func_00294FAC();
        record->field08 = data;
        func_002969B0(panel, preset, data, 1)->field5F5 = 1;
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        break;
    }
    case 14: {
        PanelContent *helpContent;
        panel = func_00051eb0(0, 0, func_0029700C, 0x122, 0x58, 0x122, 0x58, 0x7F, 0x58, 0x122, 0x6E);
        helpContent = func_00296C50(panel, preset, 1);
        func_002952D4(helpContent, 0);
        helpContent->field5F5 = 1;
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        break;
    }
    case 15:
    case 24: {
        PanelContent *detailContent;
        if (preset == 15) {
            panel = func_00051eb0(0, 0, func_0029709C, 0x1F, 0x14, 0x1F, 0x14, 0x1F, 0x14, 0x121, 0x32);
        } else {
            panel = func_00051eb0(0, 0, func_0029709C, 0x1F, 0x14, 0x121, 0x40, 0x1F, 0x14, 0x121, 0x32);
        }
        detailContent = func_00296C50(panel, preset, 1);
        func_00295A54(detailContent, 0);
        detailContent->field5F5 = 1;
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        break;
    }
    case 16:
    case 23:
        panel = func_00051eb0(0x400, 0, func_002971B4, 0x121, 0x47, 0x121, 0x47, 0x1F, 0x47, 0x121, 0xDC);
        record->field08 = func_00294940(1, &frame.selection, record);
        content = func_00296AB4(panel, 0x410, record);
        content->field5F5 = func_002953E0(0xFFFFFF);
        if (preset == 16) {
            u16 id = D_8023DE34->field1A;
            if (id == 0) {
                panel->field23 = 0;
                panel->field22 = 0;
                panel->field24 = -2;
                break;
            }
            {
                s32 position = func_002954B0(id);
                u8 visible = content->field5F4;
                s32 selected;
                /* Keep the return register separate from the comparison copy. */
                do {
                    selected = position;
                } while (0);
                ((volatile PanelContent *)content)->field5FC = position;
                if ((u8)selected >= visible) {
                    content->field5F1 = selected - visible + 1;
                }
            }
            func_002956F8(content, content->field5F1, record);
        } else {
            content->field5F1 = D_8023DE34->field22;
            content->field5FC = D_8023DE34->field1C;
            func_002956F8(content, content->field5F1, record);
        }
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        break;
    case 17:
        panel = func_00051eb0(0, 0, func_0029712C, 0x1F, 0x14, 0x121, 0x32, 0x1F, 0x14, 0x121, 0x40);
        content = func_00296C50(panel, preset, 2);
        func_00295B9C(content, D_8023DE34->field1C);
        content->field5F5 = 1;
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        break;
    case 18: {
        void *data;
        u16 row;
        s32 key;
        panel = func_00051eb0(0x100, 0, func_00052484, 0x1F, 0x58, 0x1F, 0x58, 0x1F, 0x58, 0x121, 0xDC);
        data = func_00294C54(5, *func_0028ab74((u8)func_002953E0(D_8023DE34->field1C)),
                                       &frame.selection, record);
        row = frame.selection;
        key = preset + 0x100;
        record->field08 = data;
        func_002969B0(panel, key, data, row);
        panel->field23 = 0;
        panel->field22 = 0;
        panel->field24 = -2;
        break;
    }
    case 19:
    case 20:
    case 21:
    case 22:
        D_8023DE34->field65 = 0x80;
        frame.selection = D_8023DE34->field18;
        if (func_00045df4(1) != 1) {
            frame.selection++;
        }
        panel = func_00051eb0(0x200, 0, func_00052484, 0x1F, 0xB4, 0x1F, 0xB4, 0x1F, 0xB4, 0x121, 0xDC);
        func_00296518(panel, frame.selection + 9);
        content = panel->content;
        content->key = (frame.selection + 0x13) | 0x30200;
        panel->field24 = -2;
        panel->field23 = 0;
        panel->field22 = 0;
        content->field5F4 = 2;
        break;
    }
    panel->flags |= 8;
    /* The nested one-iteration scopes preserve the retail saved-register assignment. */
    do {
        do {
            record->panel = panel;
            content = panel->content;
            if (content != 0) {
                record->key = content->key;
            }
        } while (0);
    } while (0);
}
