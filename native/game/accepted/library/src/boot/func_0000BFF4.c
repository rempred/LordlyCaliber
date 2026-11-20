typedef signed int s32;
void func_0000BFF4(s32 a, s32 b);
extern signed char g_lha_bad_table_fmt[];
extern signed char g_lha_diag_prefix[];
void func_00023940(const signed char *fmt, ...);
void func_0000BFF4(s32 a, s32 b) {
    func_00023940(g_lha_bad_table_fmt, g_lha_diag_prefix, a, b);
}
