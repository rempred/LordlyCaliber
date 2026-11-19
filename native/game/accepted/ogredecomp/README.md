# ogredecomp matched C

1,038 functions from [vibe_ogre_decomp](https://github.com/cosburn92/vibe_ogre_decomp)
(ogredecomp) that are not in the Decomp-imported accepted library. Each one is a
standalone file under `src/<segment>/<function>.c`. ogredecomp names its segments
by ROM offset.

## How it is verified

`ogredecomp-owners.json` uses the same schema as `rom-owners.json`.
`tools/build-accepted-rom.cjs` appends its sources and runs the same checks on
every one of them:
- It uses the pinned toolchain (`rebuild/toolchain.json`) and the same cpp, cc1
  and as flags.
- It links each file at the recorded addresses.
- It checks the SHA-256 of every `.text` and `.rodata` part.
- It rejects any unqualified `.data`, `.bss` or literal section.

Run:
`node tools/build-accepted-rom.cjs . <n64-toolchain> <out>/accepted-rom.bin`
→ `2520 complete sources / 2761 exact ROM blocks / 1659 accepted C functions + 1038 ogredecomp C functions`

Every hash was taken from the retail US ROM (SHA-1
`9cd0cfb50b883edb068e0c30d213193b9cf89895`). All 2,761 blocks in the output equal
the retail bytes at their ROM offsets. ogredecomp's own build rebuilds the whole
ROM byte for byte from its sources.

## Differences from the Decomp import

- **No Decomp proof files.** Bindings come from the symbol table of ogredecomp's
  matching ROM build. Rodata bases come from the retail ROM's HI16/LO16 pairs.
  `library-imports.json` and `rom-owners.json` are unchanged. These entries are
  in a separate manifest so that a Decomp refresh does not overwrite them.
- **Generated, not hand-written.** Each file is generated from ogredecomp's
  preprocessed segment file:
  - The type and extern declarations are kept.
  - Other function bodies become prototypes.
  - File-scope definitions become `extern` declarations.
  - Other `inline` helpers become `extern inline`.
  - Plain `char` becomes `signed char`, because ogredecomp compiles without
    `-funsigned-char`.
  - `fabs`/`fabsf` become `__builtin_fabs`/`__builtin_fabsf`.
- **Placeholder names.** Many identifiers are still ogredecomp's `func_`/`D_`
  placeholders with the original VRAM address.
- **One function left out.** `func_8022D988_ovl_29A4C0` depends on `fabsf`
  being compiled inline, and it doesn't match under `-fno-builtin`.

Generator: `tools/lcconv/convert.py` in ogredecomp.
