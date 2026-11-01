# Accepted matching C library

This directory retains the complete `PURE_C` and `HYBRID_C` source closure from Decomp's passing
verification. [`../library-imports.json`](../library-imports.json) is authoritative
for its current revision, verification time, counts and file membership. Exact
hybrid C/assembly sources retain their accepted inline assembly unchanged.

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

`../rom-owners.json` qualifies every accepted producer for the source-built ROM
baseline. The application build compiles these sources, checks every output
region against its accepted hash and embeds the resulting untracked artifact.
ROM exports reuse that C-built baseline wherever the original bytes are intact;
modified code, data and unconverted gaps remain unchanged. The separate
`../effect-owners.json` selects the C actively executed by effect previews.
Intentional portable adaptations remain in
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
