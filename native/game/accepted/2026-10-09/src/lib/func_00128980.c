typedef unsigned char u8;
typedef int s32;
typedef struct Func001072B8Object Func001072B8Object;

/* Retain the existing opaque object interface. This caller observes only
 * the signed word at +0x88; its wider meaning remains uncertain. */
extern void func_001072B8(Func001072B8Object *object);

void func_00128980(Func001072B8Object *object)
{
    s32 *field_88 = (s32 *)((u8 *)object + 0x88);

    if (*field_88 < 0) {
        func_001072B8(object);
        *field_88 = 0x5A;
    }
}
