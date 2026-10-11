# Windows N64 export dependency

`n64-toolchain-windows.zip` contains the six prebuilt third-party host tools
listed in `toolchain.json`. It contains no ROM, game objects or editor binary.
The editor and CLI are compiled from this repository by the release workflow;
these export tools and the official Qt packages are pinned binary dependencies.

The archive checksum is in `toolchain-distribution.json`. Both the bootstrap
script and the editor authenticate each extracted tool against `toolchain.json`.
An archive checksum does not replace those individual binary checks.

The GCC/Binutils source commits, MSYS2 source package, build recipes and patches
are documented in [toolchain-source/README.md](toolchain-source/README.md).
Every full release includes the corresponding source archives and notices under
`dependency-sources/` and `licenses/`.

Maintainers regenerate the archive from an existing authenticated tool directory:

```powershell
native/tools/pack-toolchain.ps1 -N64Tools native/.local/n64-toolchain
```

The packer checks every input against the existing pins, sorts entries and uses
a fixed ZIP timestamp. Commit the archive and its distribution manifest together.
Changing tool binaries requires a separately reviewed update to their source,
build recipes, compatibility checks and the embedded authentication pins.
