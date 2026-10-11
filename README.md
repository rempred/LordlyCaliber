# Lordly Caliber

Lordly Caliber is a native Windows editor for *Ogre Battle 64: Person of
Lordly Caliber*, built with C++ and Qt. The current native application is a
local 0.5 development build. See the [native editor guide](native/README.md)
for features, supported ROMs, limitations and project/export instructions.

## Run and build

For an existing local installation, run `native/live/LordlyCaliber.exe`.
For development, install the supported Visual Studio C++ tools, then run
these commands from the repository root:

```powershell
native/tools/bootstrap.ps1
native/tools/build.ps1
```

Run the native checks with `native/tools/build.ps1 -Test -RomPath <source.v64>`.
See [native dependencies](native/dependencies.json) and the
[native guide](native/README.md) for toolchain setup and private-ROM checks.
Supply your own supported ROM; no ROM is included in this repository.

## Repository layout

| Location | Purpose |
|---|---|
| `native/` | Current application, engine, game rebuild sources, tests, assets and packaging |
| Root JavaScript files | Shared format readers, reference data and compatibility code still used by native builds, tests or generators |
| `tests/` | Shared fixtures and test helpers still used by the native checks |
| `tools/` | Shared director grammar generator still used by the native tools |
| [`Editor-Old/`](Editor-Old/README.md) | Archived browser application, its assets, old documentation, tests, generators and standalone utilities |

The archived browser entry point is `Editor-Old/index.html`. It still uses shared
files at the repository root, so retain the full checkout when opening it or
serving it locally. Its documentation describes the old browser product.
Battery-save management is now integrated into the native editor's Testing tab;
the former standalone utility is preserved in `Editor-Old/supplemental-tools/`.

Released under the [MIT License](LICENSE). See the
[community credits](Editor-Old/CREDITS.md) and native package notices.
