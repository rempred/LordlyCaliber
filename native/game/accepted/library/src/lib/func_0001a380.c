#include "common/types.h"

/* Local views of the message queue and EPI message storage used by this owner.
 * These layouts retain the observed queue at sp+0x28 and message at sp+0x10. */
typedef struct DmaQueue {
    void *receive_thread;
    void *send_thread;
    s32 valid_count;
    s32 first;
    s32 capacity;
    void **messages;
} DmaQueue;

typedef struct DmaIoMessage {
    u16 type;
    u8 priority;
    u8 status;
    DmaQueue *return_queue;
    void *dram_address;
    u32 device_address;
    u32 size;
    void *device_handle;
} DmaIoMessage;

extern void *D_800E7A20;
extern void func_00023970(DmaQueue *, void **, u32);
extern void os_inval_dcache(void *, u32);
extern s32 func_0001c040(void *, DmaIoMessage *, s32);
extern s32 func_00023AE0(DmaQueue *, void **, s32);

void func_0001a380(u32 rom_address, u8 *destination, u32 remaining)
{
    DmaIoMessage io;
    DmaQueue queue;
    void *message;
    u32 chunk;

    func_00023970(&queue, &message, 1);
    io.priority = 0;
    io.return_queue = &queue;
    os_inval_dcache(destination, remaining);
    while (remaining != 0) {
        chunk = remaining;
        if (chunk > 0x200) chunk = 0x200;
        io.device_address = rom_address;
        io.dram_address = destination;
        io.size = chunk;
        func_0001c040(D_800E7A20, &io, 0);
        /* Keeping the chunk live through the start call reproduces the original
         * allocation; the compiler schedules these updates before that call. */
        rom_address += chunk;
        destination += chunk;
        remaining -= chunk;
        func_00023AE0(&queue, &message, 1);
    }
}
