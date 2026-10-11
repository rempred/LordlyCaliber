# Accepted pure-C library

This directory is a byte-preserved snapshot of every `PURE_C` owner in Decomp's
passing verification of 2026-10-11 15:51 UTC. The snapshot contains 788 owners,
829 compiler-recorded functions, 763 producer sources, and 787 source/header
files including their recorded dependencies. Exact hybrid C/assembly sources
are outside this pure-C snapshot.

[`../library-imports.json`](../library-imports.json) records the upstream Git
revision, verification identity, owner/function membership, source paths and
source/dependency identities. Git versions subsequent library updates. Original
relative include paths are retained. The scoped `.gitattributes` prevents line
ending conversion from changing the accepted source bytes.
The manifest defines the current snapshot. Older producer files can remain as
historical source when an upstream owner moves into a combined producer; those
unlisted files are not additional current accepted functions.

Importing source makes it available for future development. It does not compile
every function into the editor or replace the functions in the user's ROM.
Current consumers and intentional portable adaptations remain in
[`../../decomp-interest.json`](../../decomp-interest.json),
[`../animation-imports.json`](../animation-imports.json), and
[`../gameplay-imports.json`](../gameplay-imports.json). Those adaptation baselines
are retained independently when refreshing this library.

Some functions require game globals, tables, services or a particular overlay.
These remain explicit external inputs, not fabricated PC implementations. The
library contains source only; no ROM, game-art archive, object or compiled N64
payload is included. The editor's existing export path compiles required game
changes locally with its bundled tooling.

The accepted LHa adapters retain their complete original 1.14c distribution,
attribution and adaptation notes under `third_party/lha/`. The distribution is
supporting source, not an additional claim of matching game functions.
