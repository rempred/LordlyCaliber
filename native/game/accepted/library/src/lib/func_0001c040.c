typedef struct DmaIoMessage {
    unsigned short type;
    unsigned char priority;
    unsigned char status;
    void *return_queue;
    void *dram_address;
    unsigned int device_address;
    unsigned int size;
    void *device_handle;
} DmaIoMessage;
typedef struct PiManagerView {
    int active;
} PiManagerView;
extern PiManagerView D_800AA400;
extern void *func_00029cc0(void);
extern int osJamMesg(void *queue, void *message, int flags);
extern int osSendMesg(void *queue, void *message, int flags);

int func_0001c040(void *handle, DmaIoMessage *message, int direction)
{
    int result;
    if (!D_800AA400.active)
        return -1;
    message->device_handle = handle;
    if (direction == 0)
        message->type = 15;
    else
        message->type = 16;
    if (message->priority == 1)
        result = osJamMesg(func_00029cc0(), message, 0);
    else
        result = osSendMesg(func_00029cc0(), message, 0);
    return result;
}
