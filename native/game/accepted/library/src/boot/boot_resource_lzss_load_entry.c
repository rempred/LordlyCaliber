/*
 * UnpackProc: serve LZ unpack requests from the boot request queue (PURE_C replacement for the
 * earlier register-bound HYBRID source).  The format byte is the "%d" of the error report, so
 * no register binding is required.
 */
typedef unsigned char u8;
typedef unsigned int u32;

typedef struct UnpackRequest {
    u32 field_00;
    void *reply_queue;      /* +0x04: queue the finished request is sent back on */
    u8 format;              /* +0x08: 1 = LZ stream decoded by func_0000A510 */
    u8 pad_09[3];
    void *src;              /* +0x0C */
    void *dst;              /* +0x10 */
} UnpackRequest;

extern void func_00023AE0(void *queue, void *msg, int flags);     /* receive message */
extern void func_00023C10(void *queue, void *msg, int flags);     /* send message */
extern void func_00023940(const char *fmt, ...);                  /* diagnostic printf */
extern unsigned int func_0000A510(void *dst, void *src);

extern u8 g_boot_resource_lzss_table[];                           /* request queue, 0x800AF320 */
/* Format string at 0x800AE038, relative to the existing 0x800B0000 link anchor. */
extern const char g_boot_resource_lzss_error_anchor[];

void func_0000B030(void)
{
    UnpackRequest *request = 0;

    for (;;) {
        func_00023AE0(g_boot_resource_lzss_table, &request, 1);
        if (request->format == 1) {
            func_0000A510(request->dst, request->src);
            func_00023C10(request->reply_queue, request, 1);
        } else {
            func_00023940(g_boot_resource_lzss_error_anchor - 0x1FC8, request->format);
        }
    }
}
