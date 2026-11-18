#include "game/cutscene_panels.h"

extern s32 resource_free(u8 *);
extern void bzero(void *, s32);
extern void bcopy(const void *, void *, s32);
extern void func_80080998(void *, void *, s32);
extern u8 *func_002839E8(u32, s32);
extern u8 *func_00296CE0(s16);
extern void func_8009C550(u8 *, const u8 *);
extern void func_8009C620(u8 *, const u8 *);

PanelContent *func_00296754(Panel *panel, s32 section)
{
    u8 scratch[0x500];
    u8 *buffer;
    PanelContent *content;
    u8 *archive;
    u8 *scan;
    s32 offset;
    s32 seen;
    s32 start;
    s32 rows;
    s32 subjectMarker;
    s32 bufferSize;
    u16 wanted;
    s32 index;
    u8 *rowCursor;
    const u8 *name;

    bzero(scratch, sizeof(scratch));
    buffer = scratch;
    wanted = section & 0xff;
    content = panel->content;
    archive = func_002839E8(0x018855FE, 0);
    seen = 0;
    start = 0;
    offset = 0;
    do {
        u8 *cursor = archive + offset;
        u8 cr = cursor[0];
        u8 lf;

        if (cr == 13 && (lf = cursor[1], lf == 10) && cursor[2] == cr && cursor[3] == lf) {
            seen++;
            if (wanted == seen) {
                u8 *destination = buffer;
                s32 length = offset - start;

                bzero(destination, length + 2);
                func_80080998(destination, archive + start, length);
                break;
            }
            start = offset + 4;
        }
    } while (++offset <= 0x0FFFFFFE);
    resource_free(archive);

    bcopy(scratch, content->text, 0x500);
    /* The last two-byte lookahead can reach offset 0x500 in the content object. */
    scan = (u8 *)content;
    content->rows[0] = scan;
    rows = 1;
    index = 0;
    subjectMarker = 0x878D;
    bufferSize = 0x500;
    rowCursor = (u8 *)content + 4;

    do {
        s32 marker = scan[1] | (scan[0] << 8);

        if (marker == subjectMarker) {
            goto subject;
        }
        if (marker > subjectMarker) {
            goto high_marker;
        }
        if (marker == 0x8197) {
            goto newline;
        }
        scan++;
        goto advance;

high_marker:
        if (marker > 0x878F) {
            goto ordinary;
        }
        goto selected;

subject:
        {
            PanelPositionState *state = D_8023DE30;
            s16 id = state->subjects[state->field23D]->field2C;

            name = func_00296CE0(id);
        }
        func_80080998(scratch, scan + 2, bufferSize - index);
        func_8009C550(scan, name);
        goto append;

selected:
        func_80080998(scratch, scan + 2, bufferSize - index);
        func_8009C550(scan, D_8023DE34->field24);
append:
        func_8009C620(scan, scratch);
        goto next;

newline:
        scan[0] = 0;
        scan += 2;
        *(u8 **)(rowCursor + 0x530) = scan;
        rowCursor += 4;
        rows++;
        index += 2;
        if (rows >= 48) {
            index = 0x500;
        }
        goto next;
ordinary:
        scan++;
advance:
        index++;
next:
        ;
    } while (index < 0x500);

    panel->field25 = rows;
    content->field5F4 = rows;
    content->field5F5 = rows;
    return content;
}
