#include "game/effect_commands_e09.h"

s32 func_0025C590(s32 value)
{
    u16 rangeValue = value;
    if ((u16)value < 8) return 0x100;
    if ((u16)(rangeValue - 8) < 8) return 0x101;
    if ((u16)(rangeValue - 0x10) < 8) return 0x102;
    if ((u16)(rangeValue - 0x18) < 8) return 0x103;
    if ((u16)(rangeValue - 0x20) < 7) return 0x104;
    if ((u16)(rangeValue - 0x27) < 8) return 0x105;
    if ((u16)(rangeValue - 0x2F) < 4) return 0x106;
    if ((u16)(rangeValue - 0x33) < 7) return 0x107;
    if ((u16)(rangeValue - 0x3A) < 7) return 0x108;
    if ((u16)(rangeValue - 0x41) < 7) return 0x109;
    if ((u16)(rangeValue - 0x48) < 7) return 0x10A;
    if ((u16)(rangeValue - 0x4F) < 7) return 0x10B;
    return (u16)(rangeValue - 0x56) < 7 ? 0x10C : -1;
}
