/* Observed slot/size pairs terminated by a null slot; no capacity implied. */
typedef struct ResourcePoolEntry {
    void **slot;
    unsigned int size;
} ResourcePoolEntry;

extern void *func_00001330(unsigned int size);
extern void func_000016C4(void *ptr);
extern void *resource_pool;

void func_0000B33C(unsigned int acquire)
{
    if (acquire != 0) {
        unsigned int offset;
        ResourcePoolEntry *entry;

        if (resource_pool == 0) {
            return;
        }
        offset = 0;
        entry = (ResourcePoolEntry *)&resource_pool;
        do {
            *entry->slot = func_00001330(
                *(unsigned int *)((unsigned char *)&resource_pool + 4 + offset));
            offset += 8;
            entry++;
        } while (entry->slot != 0);
    } else {
        ResourcePoolEntry *entry;

        if (resource_pool == 0) {
            return;
        }
        entry = (ResourcePoolEntry *)&resource_pool;
        do {
            func_000016C4(*entry->slot);
            entry++;
        } while (entry->slot != 0);
    }
}
