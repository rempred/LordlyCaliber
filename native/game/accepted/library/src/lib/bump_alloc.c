/* Complete ROM 1FC90 owner; the first two interface arguments are unused. */
typedef struct L4Heap {
    unsigned char *base;
    unsigned char *current;
    int length;
    int field0c;
} L4Heap;

void *bump_alloc(void *unused0, int unused1, L4Heap *heap, int count, int size)
{
    int bytes = (count * size + 15) & ~15;
    unsigned char *next = heap->current + bytes;
    void *result = 0;

    if (heap->base + heap->length >= next) {
        result = heap->current;
        heap->current = next;
    }
    return result;
}
