typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u16 wave;
    u16 envelope;
    u8 pan;
    u8 note;
} MusDrum;
typedef struct {
    u8 pad00[4];
    s32 count;
    s32 unk08;
    u32 *channels;
    u32 *volumes;
    u32 *pitchbends;
    u8 *envelopes;
    MusDrum *drums;
    u16 *wavetable;
    u32 master;
    s32 relocated;
} MusSong;
s32 func_000140cc(MusSong *song);
void func_000122c8(s32 handle);
s32 func_000118f0(void *addr) {
    s32 handle = func_000140cc(addr);
    func_000122c8(handle);
    return handle;
}
void func_000122c8(s32 handle);
