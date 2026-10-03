# Game source owners

`accepted/func_000bbd50.c` is copied unchanged from OB64 Decomp's accepted
`src/lib/func_000bbd50.c` at commit `96b75a67b71bf6f5e938dbef34e355c48f039865`.
It is a pure-C class-family lookup for the existing retail promotion roster.
It is compiled locally during applicable exports, not built into an N64 byte
bundle before shipping. The export compares its complete 48-byte result with
the corresponding source ROM owner before writing it.

Placement: z64 ROM `0x000BBD50`, Army overlay RAM `0x801CF2F0`.
The placement comes from the accepted Army overlay mapping documented in the
parent `docs/promotion-system.md`, not the early-boot RAM mapping.

This owner still has the retail 81-entry family bound. It does not establish
support for expanded class IDs. Modified owners must have explicit allocation,
dependency, and behavior coverage before replacing it.
