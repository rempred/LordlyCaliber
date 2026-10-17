# Accepted pure-C library

This directory retains the complete `PURE_C` source closure from Decomp's passing
verification. [`../library-imports.json`](../library-imports.json) is authoritative
for its current revision, verification time, counts and file membership. Exact
hybrid C/assembly sources are outside this pure-C snapshot.

[`../library-imports.json`](../library-imports.json) records the upstream Git
revision, verification identity, owner/function membership, source paths and
source/dependency identities. Version 6 intake also records proof descriptors,
physical owner identities, complete-producer membership and compiler provenance.
Git versions subsequent library updates. Original
relative include paths are retained. The scoped `.gitattributes` prevents line
ending conversion from changing the accepted source bytes.
Refreshes remove unchanged previously listed files retired by a new producer.
Unlisted historical/support files are preserved and are not additional current
accepted functions.

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
