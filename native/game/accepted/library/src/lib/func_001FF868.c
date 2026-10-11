void func_001FF868(signed short arg0, signed short arg1, signed short arg2, signed short arg3, unsigned char arg4) {
    *(signed short *)0x801D0804 = arg0;
    *(signed short *)0x801D0806 = arg1;
    *(signed short *)0x801D0808 = arg2;
    *(signed short *)0x801D080A = arg3;
    *(signed char *)0x801D080D = 0;
    *(unsigned short *)0x801D07FC = *(unsigned short *)0x801D07F4;
    *(unsigned short *)0x801D07FE = *(unsigned short *)0x801D07F6;
    *(unsigned short *)0x801D0800 = *(unsigned short *)0x801D07F8;
    *(unsigned short *)0x801D0802 = *(unsigned short *)0x801D07FA;
    *(unsigned char *)0x801D080C = arg4;
}
