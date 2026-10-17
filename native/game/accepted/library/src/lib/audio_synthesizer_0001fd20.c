/* Whole producer ROM 1FD20..20360, independently reconstructed from ROM.
 * Original-owned global pointer and protected companions retain their roles. */
typedef struct L4Command { unsigned int word0, word1; } L4Command;
typedef struct L4Link { struct L4Link *next, *previous; } L4Link;
typedef struct L4Filter L4Filter;
struct L4Filter {
    L4Filter *input;
    L4Command *(*pull)(void *, short *, int, int, L4Command *);
    int (*param)(void *, int, void *);
    short field0c, field0e;
    void *state;
};
typedef struct L4Load { L4Filter filter; char fields14[52]; } L4Load;
typedef struct L4Resample { L4Filter filter; char fields14[32]; } L4Resample;
typedef struct L4Env { L4Filter filter; char fields14[60]; } L4Env;
typedef struct L4Voice {
    L4Link link;
    int field08;
    L4Filter *filter;
    L4Load load;
    L4Resample resample;
    L4Env env;
} L4Voice;
typedef struct L4Param { struct L4Param *next; char fields04[24]; } L4Param;
typedef struct L4Client {
    struct L4Client *next;
    void *field04;
    int (*handler)(struct L4Client *);
    void *field0c;
    int sampleTime;
} L4Client;
typedef struct L4Synth {
    L4Client *clients;
    L4Link freeVoices, allocatedVoices, lameVoices;
    int nextSample, currentSample;
    void *dmaConstructor;
    void *heap;
    L4Param *freeParams;
    L4Filter *mainBus, *auxBus, *save;
    int maxVoices, field40, outputRate, maxBlock;
} L4Synth;
typedef struct L4Config {
    int field00, maxVoices, maxParams, field0c;
    void *dmaConstructor;
    void *heap;
    int outputRate;
    unsigned char fxType;
} L4Config;
extern L4Synth *alGlobals;
extern void *alHeapDBAlloc(char *, int, void *, int, int);
extern void alSaveNew(void *);
extern void alAuxBusNew(void *, void *, int);
extern void alMainBusNew(void *, void *, int);
extern void *alSynAllocFX(void *, int, void *, void *);
extern int alMainBusParam(void *, int, void *);
extern void alLink(L4Link *, L4Link *);
extern void alUnlink(L4Link *);
extern void alLoadNew(void *, void *, void *);
extern void alLoadParam(void *, int, void *);
extern void alResampleNew(void *, void *);
extern int alResampleParam(void *, int, void *);
extern void alEnvmixerNew(void *, void *);
extern int alEnvmixerParam(void *, int, void *);
extern int alAuxBusParam(void *, int, void *);
extern int alSaveParam(void *, int, void *);
int func_00020308(L4Synth *, L4Client **);
int func_00020274(L4Synth *, int);
void func_000201e0(L4Synth *);

void func_0001fd20(L4Synth *s, L4Config *config)
{
    void *heap = config->heap;
    L4Filter *save;
    L4Voice *voices;
    L4Param *params;
    int i;

    s->clients = 0;
    s->maxVoices = config->maxVoices;
    s->currentSample = 0;
    s->nextSample = 0;
    s->outputRate = config->outputRate;
    s->maxBlock = 160;
    s->dmaConstructor = config->dmaConstructor;
    save = alHeapDBAlloc(0, 0, heap, 1, 28);
    alSaveNew(save);
    s->save = save;
    s->auxBus = alHeapDBAlloc(0, 0, heap, 1, 76);
    s->field40 = 1;
    alAuxBusNew(s->auxBus, alHeapDBAlloc(0, 0, heap, config->maxVoices, 4), config->maxVoices);
    s->mainBus = alHeapDBAlloc(0, 0, heap, 1, 32);
    alMainBusNew(s->mainBus, alHeapDBAlloc(0, 0, heap, config->maxVoices, 4), config->maxVoices);
    if (config->fxType)
        alSynAllocFX(s, 0, config, heap);
    else
        alMainBusParam(s->mainBus, 2, s->auxBus);
    s->freeVoices.next = 0;
    s->freeVoices.previous = 0;
    s->lameVoices.next = 0;
    s->lameVoices.previous = 0;
    s->allocatedVoices.next = 0;
    s->allocatedVoices.previous = 0;
    voices = alHeapDBAlloc(0, 0, heap, config->maxVoices, sizeof(L4Voice));
    for (i = 0; i < config->maxVoices; i++) {
        L4Voice *voice = &voices[i];
        L4Load *load = &voice->load;
        L4Resample *resample = &voice->resample;
        L4Env *env = &voice->env;
        alLink(&voice->link, &s->freeVoices);
        voice->field08 = 0;
        alLoadNew(load, s->dmaConstructor, heap);
        alLoadParam(load, 1, 0);
        alResampleNew(resample, heap);
        alResampleParam(resample, 1, load);
        alEnvmixerNew(env, heap);
        alEnvmixerParam(env, 1, resample);
        alAuxBusParam(s->auxBus, 2, env);
        voice->filter = &env->filter;
    }
    alSaveParam(save, 1, s->mainBus);
    params = alHeapDBAlloc(0, 0, heap, config->maxParams, sizeof(L4Param));
    s->freeParams = 0;
    for (i = 0; i < config->maxParams; i++) {
        params[i].next = s->freeParams;
        s->freeParams = &params[i];
    }
    s->heap = heap;
}

L4Command *func_0001ffec(L4Command *inputCommands, int *length, short *outputBuffer, int samples)
{
    L4Synth *s = alGlobals;
    L4Command *commands = inputCommands;
    short *output = outputBuffer;
    L4Client *client;
    short dmemOutput = 0;
    if (!s->clients) {
        *length = 0;
        return commands;
    }
    s->nextSample = func_00020308(s, &client);
    while (s->nextSample - s->currentSample < samples) {
        int usecs;
        s->nextSample &= ~15;
        usecs = client->handler(client);
        client->sampleTime += func_00020274(s, usecs);
        s->nextSample = func_00020308(s, &client);
    }
    s->nextSample &= ~15;
    while (samples > 0) {
        int count = s->maxBlock;
        L4Filter *save;
        if (samples < count)
            count = samples;
        {
            /* Keep the current packet until pull consumes commands + 1. */
            commands->word0 = 0x07000000;
            commands->word1 = 0;
        }
        save = s->save;
        save->param(save, 6, output);
        samples -= count;
        commands = save->pull(save, &dmemOutput, count, s->currentSample, commands + 1);
        output += count * 2;
        s->currentSample += count;
    }
    *length = commands - inputCommands;
    func_000201e0(s);
    return commands;
}

L4Param *func_0002019c(void)
{
    L4Synth *s = alGlobals;
    L4Param *param = 0;
    if (s->freeParams) {
        param = s->freeParams;
        s->freeParams = param->next;
        param->next = 0;
    }
    return param;
}

void func_000201c8(L4Param *param)
{
    L4Synth *s = alGlobals;
    param->next = s->freeParams;
    s->freeParams = param;
}

void func_000201e0(L4Synth *s)
{
    L4Link *voice;
    while ((voice = s->lameVoices.next) != 0) {
        alUnlink(voice);
        alLink(voice, &s->freeVoices);
    }
}

void func_00020234(L4Synth *s, L4Voice *voice)
{
    alUnlink(&voice->link);
    alLink(&voice->link, &s->lameVoices);
}

int func_00020274(L4Synth *s, int usecs)
{
    return (int)(float)(((double)((float)usecs * (float)s->outputRate) / 1000000.0) + 0.5);
}

int func_000202bc(L4Synth *s, int usecs)
{
    return (int)(float)(((double)((float)usecs * (float)s->outputRate) / 1000000.0) + 0.5) & ~15;
}

int func_00020308(L4Synth *s, L4Client **client)
{
    L4Client *p;
    int minimum = 0x7fffffff;
    *client = 0;
    for (p = s->clients; p; p = p->next) {
        if (p->sampleTime - s->currentSample < minimum) {
            *client = p;
            minimum = p->sampleTime - s->currentSample;
        }
    }
    return (*client)->sampleTime;
}
