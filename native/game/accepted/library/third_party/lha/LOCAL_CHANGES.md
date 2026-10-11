# LHa game adaptation ledger

The unmodified comparison baseline is **LHa for UNIX 1.14c, March 7, 1996**, retained
byte-for-byte in [upstream/](upstream/). The archive SHA-256 is
`D88AAE866980AF28B1789683E738852561CA489C98AC584B0E6580D9E73C1FB4`;
[README.md](README.md) and [upstream-manifest.json](upstream-manifest.json) record provenance.
The original 1995 1.14 distribution was not located. Source correspondence and matching
output do not prove that 1.14c was the game's historical source ancestor.

All original distribution files, copyright notices, author credits, source banners and
distribution terms remain intact. Adapted files retain their upstream attribution; this
ledger supplements those notices. Keep `upstream/` pristine. In particular, retain the
original EUC-JP documents and the notices in `upstream/src/lharc.c` and the makefiles,
not merely the selected decoder excerpts.

## Scope and current evidence

The canonical adaptation source paths under `ob64/` contain **21 accepted PURE_C
producers covering 22 preserved physical owners and 14,272 bytes**. Every producer
passed its canonical linked-byte and relocation checks. The combined structural,
sole-ownership, source-to-object and exact-ROM audit passed on September 24, 2026
at 05:03:02 UTC. The [trial report](../../docs/audit/2026-09-23-boot-decompression-trial.md)
records the final gates and separates these library results from unfinished custom code.

Each selected producer has mechanical `PURE_C` classification and an exact complete-interval
isolated linked comparison using the authenticated pinned tools. The common-header migration
was checked by `build/boot-decompression-trial-r1/probe/common-header-r1/results.json`
(19 producers) and `common-header-r2/results.json` (the remaining `decode` and `crcio`
producers). These ignored diagnostic artifacts were followed by the ordinary canonical
linked diffs and the completed-wave audit; isolated comparisons alone did not establish
acceptance.

The already-active reset at ROM `0xD994..0xD9B8` is **36 bytes of existing context**, excluded
from the 14,272 new candidate bytes. The game's ancillary custom bitstream/LZSS routines
are not attributed to LHa; their independently derived sources and acceptance remain
separate. The B030 conversion from hybrid to pure C is reported separately and is
excluded from these LHa byte totals.

Every interval below is a normalized Rev 0 ROM interval with an exclusive end. Multiple
functions in one source, or one source spanning two owners, preserve complete physical
coverage. This packaging is a present build requirement, not a claim about historical
translation-unit boundaries. Public and local entry spelling follows the project's owner
and linkage model; an older semantic build name need not equal the upstream function name.

## Shared declarations and helpers

`ob64/lha.h` adapts `lha.h`, `lha_macro.h` and `crcio.c`. `huf.h`, `dhuf.h` and `shuf.h`
add the respective decoder state and constants. Encoding, archive UI, command-line handling
and unrelated upstream functions are not emitted by these decoding producers.

- Input uses `LhaStream`: a buffer pointer at `+0x08` and a signed target-int cursor at
  `+0x10`; `getc(fp)` reads `fp->buf[fp->pos++]`. Padding fields only preserve observed
  offsets. Exhausted compressed input continues to supply zero through `fillbuf`.
- Runtime state remains independent extern scalar and pointer slots. CRC and Huffman
  arrays in upstream become pointers where the ROM loads a pointer before accessing
  elements. No synthetic global-state aggregate replaces those accesses.
- Target widths remain explicit: `bitbuf`, CRC, dictionary bits, dictionary size and
  selected counters use unsigned halfwords; `subbitbuf` and `bitcount` use bytes. Dynamic
  tree indices are signed-short elements, frequencies unsigned-short elements, and their
  scalar indices/counts retain the target int/long widths. `dicsiz` is a game-specific
  unsigned short rather than upstream's unsigned long.
- `fillbuf(unsigned char n)` is a normal `static inline` function retaining the upstream
  operation order and byte parameter. The memory-backed `getc` is its game input change.
  `calccrc(unsigned char *p, unsigned int n)` is likewise a normal inline function; it
  retains the CRC update loop and return, omitting upstream `reading_size` accounting and
  progress-indicator behavior absent from these retail paths.
- Preliminary local-block/macro substitutes were semantically similar but changed the
  pinned compiler's argument lifetimes and register scheduling. Restoring ordinary inline
  helper functions resolved the static-Huffman, dynamic-decoder, legacy-decoder and copy
  mismatches. The final shared helper implementation preserves that evidence; it uses no
  register binding, assembly escape, rewritten instruction or fabricated padding.
- `getbits` and `init_getbits` remain separately callable interior functions in their
  complete physical owners. Defining-owner guards preserve local definitions; consumers
  have declarations for the existing callable addresses. Compatibility aliases map
  `g_lha_*` names onto the same shared declarations and storage, not duplicate objects.
- Allocator, free, memory fill, file-like write/read and diagnostic calls resolve to the
  game's existing entries. Existing ROM strings and tables are referenced through symbols;
  the adaptation does not emit replacement string or writable state storage.

## Producer-by-producer changes

| Canonical source | ROM interval | Bytes | Upstream functions and retained local changes |
|---|---|---:|---|
| `ob64/make_table.c` | `C024..C310` | 748 | `maketbl.c::make_table`. Preserve count/weight/start construction and tree algorithm. Independent left/right pointer state replaces arrays. Replace upstream `error()` with the observed four-argument game diagnostic and existing ROM strings. |
| `ob64/decode.c` | `C310..C778` | 1,128 | `slide.c::decode`, `crcio.c::make_crctable`, `getbits`. Preserve all three bodies. `decode` returns void, omits the upstream null-allocation failure branch, uses the game allocator/free, and stores dictionary size as unsigned short. The callback table contains seven methods (lh1..lh5, lzs, lz5), so the LZS offset test uses method 6; no lh6 row. CRC table storage is pointer-backed. `getbits` invokes the shared inline refill. |
| `ob64/crcio.c` | `C778..C990` | 536 | `crcio.c::fwrite_crc`, `init_getbits`, `util.c::convdelim`. Keep all three bodies. CRC calculation omits the absent read-size counter; remove verification/text conversion paths. File-write failure reports through the returning game diagnostic with existing strings. `init_getbits` uses inline refill and omits EUC cache reset. `convdelim` omits multibyte-character handling. |
| `ob64/copyfile.c` | `C990..CB4C` | 444 | `util.c::copyfile` with `crcio.c::calccrc`. Binary-only 2,048-byte chunks; remove text conversion, text cache initialization and text-mode error branches. Preserve optional output/CRC, unsigned-short chunk length, long remaining/returned size, allocation/free and read/write order. Replace fatal errors with the observed returning diagnostic calls; inline CRC without read-size accounting. This owner is utility code, not `huf.c`. |
| `ob64/read_pt_len.c` | `CB4C..CEB8` | 876 | `huf.c::read_pt_len`. Preserve the upstream body, signed-short locals, zero/nonzero count branches, unary lengths, special zero insertion and table build. Change entry declaration/name and shared-state access through header aliases; refill is the shared inline function. |
| `ob64/read_c_len.c` | `CEB8..D248` | 912 | `huf.c::read_c_len`. Preserve upstream body, signed-short locals, tree walk, zero-run handling and both table branches. Header aliases supply pointer state and the shared inline refill. |
| `ob64/decode_c_st1.c` | `D248..D600` | 952 | `huf.c::decode_c_st1`. Preserve upstream body. Fix position alphabet parameters to `np=14`, `pbit=4` instead of upstream mode-dependent variables. Preserve the blocksize-load preamble before the old D250 interior label. |
| `ob64/decode_p_st1.c` | `D600..D994` | 916 | `huf.c::decode_p_st1`. Preserve upstream body with fixed `np=14`, pointer tables and shared refill. Include the bitbuf/table-load preamble before the old D610 label and the final extra-position-bit read. |
| `ob64/start_c_dyn.c` | `D9B8..DBBC` | 516 | `dhuf.c::start_c_dyn`. Preserve upstream threshold, stock/block initialization, leaves and sibling-frequency construction. Use observed pointer globals and scalar widths; no algorithm rewrite. |
| `ob64/decode_start_dyn.c` | `DBBC..DCA8` | 236 | `dhuf.c::decode_start_dyn` with `start_p_dyn` expanded in its caller. Preserve `n_max=286`, `maxmatch=256`, initialization calls, root-P setup and `nextcount=64`; use independent retail state. |
| `ob64/reconst.c` | `DCA8..E1F0` | 1,352 | `dhuf.c::reconst` (844 bytes) and local `swap_inc` (508 bytes). Retain both complete bodies. Initialize `b = g_lha_block[start]` before the loop, matching the retail pre-read/default value absent from the 1.14c declaration. Preserve subsequent assignments and leader-swap algorithm. |
| `ob64/make_new_node.c` | `E1F0..E3F0` | 512 | `dhuf.c::make_new_node` with `update_p` expanded in place. Preserve node growth, parent/block assignments, renormalization and total-count updates. Include setup before the old E204 prologue label. |
| `ob64/decode_dyn.c` | `E3F0..EA98` | 1,704 | `dhuf.c::decode_c_dyn` (776 bytes) and local `decode_p_dyn` (928 bytes), with `update_c`/`update_p` expanded into their callers. Keep both decoders, shared inline refill, count-driven new-node growth and extra-bit reads. This producer spans **two** preserved physical owners, 792 and 912 bytes; the second callable body starts at E6F8, with 16 bytes before the physical E708 cut. |
| `ob64/decode_start_st0.c` | `EA98..EADC` | 68 | `shuf.c::decode_start_st0`. Add the observed reset of the separate st0 block count at `0x800AF3C8` before bit initialization. Preserve `n_max=286`, `maxmatch=256` and runtime `np=1<<(13-6)`. |
| `ob64/read_tree_c.c` | `EADC..EC00` | 292 | `shuf.c::read_tree_c`. Preserve upstream body and forward `for (i=0; i<4096; i++)` constant-table fill. An early manually reversed loop was rejected; the restored upstream loop lets the compiler generate the retail reverse traversal and register allocation. The final forward loop is not a game algorithm change. |
| `ob64/decode_start_fix.c` | `EC00..ECF0` | 240 | `shuf.c::decode_start_fix` with ordinary inline `ready_made(0)`. Preserve `n_max=314`, `maxmatch=60`, runtime `np=64`, dynamic-C initialization and position-table construction. Existing fixed-table data is referenced through the observed state. |
| `ob64/decode_c_st0.c` | `ECF0..F22C` | 1,340 | `shuf.c::decode_c_st0`, inline `read_tree_p` and `ready_made(1)`. Retain the game distinction: the entry gate reads blocksize at `0x800AF3C6`, while header write/decrement use the separate count at `0x800AF3C8`. Compile-time `NP=14` controls the helper/table build, distinct from runtime `np`. In the helper's constant-tree branch, retain the observed **256-entry reverse** `c_table` fill (`i=255` down to 0), rather than upstream's 256-entry forward fill. Shared inline refill preserves the exact decoder scheduling. |
| `ob64/decode_p_st0.c` | `F22C..F5A0` | 884 | `shuf.c::decode_p_st0`. Preserve upstream body, runtime `np` lookup/walk boundary and six extra position bits. Use independent pointer tables and the shared inline refill. |
| `ob64/decode_lzs.c` | `F5A0..F618` | 120 | `larc.c::decode_c_lzs` and local `decode_p_lzs`. Preserve both bodies without algorithm changes; match-position and dictionary location refer to retail state. |
| `ob64/decode_lz5.c` | `F618..F734` | 284 | `larc.c::decode_start_lzs`, local `decode_c_lz5`, local `decode_p_lz5`. Preserve all three bodies. Byte input uses the memory-backed stream adapter; flag and match-position state is external retail storage. The filename does not imply omission of the LZS initializer. |
| `ob64/decode_start_lz5.c` | `F734..F808` | 212 | `larc.c::decode_start_lz5`. Preserve upstream initialization body, fill patterns and loop directions; point `text` and flag state at retail storage and bind `memset` to the game entry. |

## Important distinctions retained in the source

`huf.h` fixes the newer static decoder's position alphabet at 14/4. `shuf.h` has both
compile-time `NP=14` and a separate runtime `np` slot; they are not interchangeable.
The st0 gate/count split at AF3C6/AF3C8 likewise remains explicit despite the cleaner
single-state expression in upstream. Exact diagnostics support preserving these accesses;
they do not authorize an interpretation that repairs or merges them.

Dynamic Huffman constants retain `N_CHAR=314`, `TREESIZE_C=628`, `ROOT_C=0`, `ROOT_P=628`.
The observed most-P low-halfword access is part of the existing big-endian word slot,
not evidence for another independent global. Reconst's initial `block[start]` read is a
documented source difference supported by the actual instructions and compiler comparison.

The inline representation changes affect compiler behavior without changing the upstream
refill or CRC algorithms. Conversely, the st0 state split, helper loop direction, initial
reconst read, memory-stream adapter and absent text/UI/error paths are explicit game
differences. These categories are recorded separately so future cleanup does not erase
ROM-supported behavior while trying to make the source resemble the reference library.

Acceptance includes every complete owner, original-assembly exclusion, placement,
actual relocation contracts and the complete normalized retail ROM comparison under
the canonical workflow. The final ROM SHA-256 is
`571E83396BC81E70DA4C0A20313D82DBD7DFE685F2C37418C8E27F927E2CC67A`.
