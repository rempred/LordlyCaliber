/* Adapted accepted func_0020D590 for wide idle-frame tokens. The native body
 * selector, counted pose directory, and frame bounds still own the lookup. */
typedef unsigned char u8;
typedef unsigned int u32;

typedef struct {
    u8 prefix[0x40];
    u32 flags;
    int unused, source, current, resource;
} HeightActor;

typedef struct {
    int low04, low08, high04, high08;
} FrameBounds;

extern int lc_body_handle(int, int, int, int);
extern const u8 *lc_pose_records(int, int);
extern const u8 lc_pose_widths[];
extern FrameBounds lc_frame_bounds(int, int);

int lc_body_height(const HeightActor *actor) {
    FrameBounds bounds;
    const u8 *record;
    int count, opcode, token;
    if (actor->source == 0x101) return 38;
    record = lc_pose_records(lc_body_handle(actor->source, actor->current,
                              (actor->flags >> 10) & 1, (actor->flags >> 8) & 1), 0);
    if (!record) return 0;
    count = *record++;
    while (count--) {
        opcode = *record;
        if (opcode == 1 || opcode == 21) {
            token = record[1];
            if (opcode == 21) token += record[2] * 256;
            bounds = lc_frame_bounds(actor->resource, token);
            return (int)(0u - (u32)bounds.low08);
        }
        record += lc_pose_widths[opcode];
    }
    return 0;
}
