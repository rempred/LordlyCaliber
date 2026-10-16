typedef struct Func0011B344Slots {
    void *field_00;
    void *field_04;
    void *field_08;
    void *field_0C;
    void *field_10;
} Func0011B344Slots;

typedef struct Func0011B344Owner {
    unsigned char padding_00_A4[0xA4];
    Func0011B344Slots *field_A4;
} Func0011B344Owner;

extern void func_000016C4(void *pointer);

void func_0011B344(Func0011B344Owner *owner)
{
    if (owner->field_A4 != 0) {
        if (owner->field_A4->field_00 != 0) {
            func_000016C4(owner->field_A4->field_00);
            owner->field_A4->field_00 = 0;
        }
        if (owner->field_A4->field_04 != 0) {
            func_000016C4(owner->field_A4->field_04);
            owner->field_A4->field_04 = 0;
        }
        if (owner->field_A4->field_08 != 0) {
            func_000016C4(owner->field_A4->field_08);
            owner->field_A4->field_08 = 0;
        }
        if (owner->field_A4->field_0C != 0) {
            func_000016C4(owner->field_A4->field_0C);
            owner->field_A4->field_0C = 0;
        }
        if (owner->field_A4->field_10 != 0) {
            func_000016C4(owner->field_A4->field_10);
            owner->field_A4->field_10 = 0;
        }
    }
    if (owner->field_A4 != 0) {
        func_000016C4(owner->field_A4);
        owner->field_A4 = 0;
    }
}

