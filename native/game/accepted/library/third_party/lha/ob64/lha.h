/*
 * Boot LHA extractor: shared declarations for the game adaptation.
 *
 * Adapted from LHa for UNIX 1.14c (lha.h / lha_macro.h), vendored at third_party/lha
 * (lha-114c.tar.gz, SHA256 D88AAE86...1FB4); see its README for the original notices.
 * Upstream: Masaru Oki, Nobutaka Watazaki, Tsugio Okamoto et al.  Local changes for the
 * game's copy are listed in ../LOCAL_CHANGES.md and source comments.  Version ancestry is not claimed:
 * 1.14c is the reference baseline, not a proven ancestor.
 *
 * Linkage: upstream names map onto the accepted target symbols and retail data addresses
 * (early-boot runtime = ROM + 0x8006FC00).  Upstream file-static data are extern here
 * because their storage belongs to the existing retail data owners.
 */
#ifndef OB64_LHA_H
#define OB64_LHA_H

#define CHAR_BIT 8
#define UCHAR_MAX 255
#define THRESHOLD 3
#define CRCPOLY 0xA001
#define MAGIC0 18
#define MAGIC5 19
#define DELIM '/'
#define DELIM2 0xff
#define UPDATE_CRC(c) crc = crctable[(crc ^ (c)) & 0xFF] ^ (crc >> CHAR_BIT)

/* Local change: input is an in-memory stream, and getc() reads its buffer. */
typedef struct LhaStream {
    unsigned char pad_00[8];
    unsigned char *buf;                  /* +0x08 */
    unsigned char pad_0C[4];
    int pos;                             /* +0x10 */
} LhaStream;
#define getc(fp) ((fp)->buf[(fp)->pos++])

struct interfacing {
    LhaStream *infile;
    void *outfile;
    unsigned long original;
    unsigned long packed;
    int dicbit;
    int method;
};

struct decode_option {
    unsigned short (*decode_c)(void);
    unsigned short (*decode_p)(void);
    void (*decode_start)(void);
};

/* Upstream names -> accepted target symbols. */
#define decode           func_0000C310
#define make_crctable    func_0000C604
#define getbits          func_0000C65C
#define fwrite_crc       func_0000C778
#define init_getbits     func_0000C838
#define convdelim        func_0000C938
#define malloc           func_00001330       /* 0x80070F30 */
#define free             func_000016C4       /* 0x800712C4 */
#define fwrite           func_0000F9D8       /* 0x8007F5D8 */
#define lha_printf       func_00023940       /* 0x80093540 */

/* Retail data (runtime addresses). */
extern LhaStream *infile;                    /* 0x800AF36C */
extern void *outfile;                        /* 0x800AF370 */
extern unsigned long origsize;               /* 0x800AF394 */
extern unsigned long compsize;               /* 0x800AF398 */
extern unsigned short dicbit;                /* 0x800AF39C */
extern unsigned long count;                  /* 0x800AF3A0 */
extern unsigned short loc;                   /* 0x800AF3A4 */
extern unsigned char *text;                  /* 0x800AF3A8 */
extern int prev_char;                        /* 0x800AF3AC */
extern unsigned short dicsiz;                /* 0x800AF3B0; local change: upstream 1.14c is unsigned long */
extern struct decode_option decode_set;      /* 0x800AF3B4 */
extern unsigned short crc;                   /* 0x800AF3C0 */
extern unsigned short bitbuf;                /* 0x800AF3C2 */
extern unsigned char subbitbuf;              /* 0x800AF3C4 */
extern unsigned char bitcount;               /* 0x800AF3C5 */
extern int flag;                             /* 0x800AF3EC */
extern int flagcnt;                          /* 0x800AF3F0 */
extern int matchpos;                         /* 0x800AF3F4 */
extern unsigned short *crctable;             /* 0x800AF3F8; local change: pointer, not a static array */
extern struct decode_option decode_define[]; /* 0x800A8778; local change: lh1..lh5, lzs, lz5 (no lh6) */

/*
 * Local change: fillbuf() (crcio.c) is expanded at every use.  The ROM has no
 * out-of-line fillbuf, and every reader contains its body.
 */
/* Keep the upstream byte parameter as an ordinary inline function. A macro
 * changes KMC's parameter lifetime and register allocation in several readers. */
static inline void fillbuf(unsigned char n)
{
    while (n > bitcount) {
        n -= bitcount;
        bitbuf = (bitbuf << bitcount) + (subbitbuf >> (CHAR_BIT - bitcount));
        if (compsize != 0) {
            compsize--;
            subbitbuf = (unsigned char)getc(infile);
        } else
            subbitbuf = 0;
        bitcount = CHAR_BIT;
    }
    bitcount -= n;
    bitbuf = (bitbuf << n) + (subbitbuf >> (CHAR_BIT - n));
    subbitbuf <<= n;
}

/* Game output/copy paths inline this upstream loop and omit reading_size. */
static inline unsigned short calccrc(unsigned char *p, unsigned int n)
{
    while (n-- > 0)
        UPDATE_CRC(*p++);
    return crc;
}

/* getbits/init_getbits are file-static in their owners and bound by fixed address elsewhere. */
#ifndef LHA_DEFINES_C310
unsigned short getbits(unsigned char n);
#endif
#ifndef LHA_DEFINES_C778
void init_getbits(void);
#endif
void fwrite_crc(unsigned char *p, int n, void *fp);

extern void *malloc(unsigned int size);
extern void free(void *ptr);
extern void *memset(void *s, int c, unsigned int n);
extern unsigned int fwrite(const void *p, unsigned int size, unsigned int n, void *fp);
extern void lha_printf(const char *fmt, ...);



/* Compatibility names share the same declarations and storage across codecs. */
#define g_lha_infile infile
#define g_lha_outfile outfile
#define g_lha_origsize origsize
#define g_lha_compsize compsize
#define g_lha_dicbit dicbit
#define g_lha_count count
#define g_lha_loc loc
#define g_lha_text text
#define g_lha_prev_char prev_char
#define g_lha_dicsiz dicsiz
#define g_lha_decode_set decode_set
#define g_lha_crc crc
#define g_lha_bitbuf bitbuf
#define g_lha_subbitbuf subbitbuf
#define g_lha_bitcount bitcount
#define g_lha_flag flag
#define g_lha_flagcnt flagcnt
#define g_lha_matchpos matchpos
#define g_lha_crctable crctable
#define g_lha_decode_define decode_define
#define LHA_GETC(fp) getc(fp)
#define LHA_FILLBUF fillbuf

#endif
