typedef unsigned short u16;
typedef unsigned int u32;
typedef struct { u16 v[3]; } U16x3;
extern u16 D_801FDA60;
extern U16x3 D_801FD958;
void func_001466E4(u32 i) {
    U16x3 t = D_801FD958;
    if (i >= 3) {
        i = 0;
    }
    D_801FDA60 = t.v[i];
}
