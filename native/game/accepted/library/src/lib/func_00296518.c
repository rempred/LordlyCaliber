#include "game/cutscene_panels.h"

extern void *resource_alloc(u32);
extern s32 resource_free(u8 *);
extern void bzero(void *, s32);
extern void bcopy(const void *, void *, s32);
extern void func_80080998(void *, void *, s32);
extern u8 *func_002839E8(u32, s32);

PanelContent *func_00296518(Panel *panel, s32 key)
{
    u8 scratch[0x500];
    PanelContent *content;
    u8 *archive;
    u8 *scan;
    u8 *buffer;
    s32 offset;
    s32 start;
    u16 section;
    s32 seen;
    u32 kind = (u32)key >> 16;
    s32 rows;
    s32 index;
    u8 *rowCursor;
    s32 stopMarker;
    s32 newlineMarker;
    s32 initialFlag;

    panel->content = resource_alloc(sizeof(PanelContent));
    bzero(panel->content, sizeof(PanelContent));
    bzero(scratch, sizeof(scratch));
    buffer = scratch;
    section = key & 0xFF;
    content = panel->content;
    /* Write the key first to reproduce KMC's scheduling before the archive call. */
    content->key = key;
    content->field5F3 = 0x3F;
    content->field5F0 = 0;
    content->field5FE = 2;
    archive = func_002839E8(0x018855FE, 0);
    start = 0;
    seen = 0;
    offset = 0;
    do {
        u8 *cursor = archive + offset;
        u8 cr = cursor[0];
        u8 lf;

        if (cr == 13 && (lf = cursor[1], lf == 10) && cursor[2] == cr && cursor[3] == lf) {
            seen++;
            if (section == seen) {
                goto found;
            }
            start = offset + 4;
        }
    } while (++offset <= 0x0FFFFFFE);

release:
    resource_free(archive);

    if ((kind << 16) == 0x20000) {
        goto alternate;
    }
    goto fill;

found:
    {
        u8 *destination = buffer;
        s32 length = offset - start;

        bzero(destination, length + 2);
        func_80080998(destination, archive + start, length);
    }
    goto release;

alternate:
    content->field5F0 = 6;
fill:
    bcopy(scratch, content->text, 0x500);

    /* The last two-byte lookahead can reach offset 0x500 in the content object. */
    scan = (u8 *)content;
    rows = 1;
    index = 0;
    stopMarker = 0x818F;
    newlineMarker = 0x8197;
    initialFlag = 1;
    rowCursor = (u8 *)content + 4;
    content->rows[0] = scan;
    panel->field25 = 1;
    content->field5F4 = 1;
    do {
        s32 marker = scan[1] | (scan[0] << 8);

        if (marker == stopMarker) {
            goto control;
        }
        if (marker != newlineMarker) {
            goto normal;
        }
        {
            scan[0] = 0;
            scan += 2;
            *(u8 **)(rowCursor + 0x530) = scan;
            rowCursor += 4;
            panel->field25++;
            rows++;
            content->field5F4++;
            index += 2;
            if (rows >= 48) {
                index = 0x500;
            }
            goto scan_next;
        }

control:
        scan[0] = 0;
        scan += 2;
        index += 2;
        content->field5F7 = initialFlag;
        panel->field22 = 0;
normal:
        scan++;
        index++;
scan_next:
        ;
    } while (index < 0x500);
    content->field5F5 = rows;
    content->field5F6 = 17;
    return content;
}
