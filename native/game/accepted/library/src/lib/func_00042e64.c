typedef unsigned char u8;
typedef struct { u8 bytes[56]; } Record56;
typedef struct BitRecordSet BitRecordSet;

extern Record56 g_func_0019554C_records_56[];
extern BitRecordSet D_80187444;
extern void boot_bitstream_descriptor_encode(u8 *buffer, BitRecordSet *set);
extern void *func_00001330(int size);
extern void func_000016C4(void *pointer);
extern void func_00023460(void *source, void *destination, int size);
extern void func_00050918(u8 *bytes, void *record);
extern void func_8016BA10(int value);

void func_00042e64(u8 *buffer)
{
    u8 **saved;
    int i;

    func_8016BA10(0);
    saved = func_00001330(100 * sizeof(*saved));
    for (i = 0; i < 100; i++) {
        saved[i] = func_00001330(16);
        func_00023460(&g_func_0019554C_records_56[i], saved[i], 16);
        func_00050918(saved[i], &g_func_0019554C_records_56[i]);
    }
    boot_bitstream_descriptor_encode(buffer, &D_80187444);
    for (i = 0; i < 100; i++) {
        func_00023460(saved[i], &g_func_0019554C_records_56[i], 16);
        func_000016C4(saved[i]);
    }
    func_000016C4(saved);
    func_8016BA10(1);
}
