# N64 toolchain source and build recipes

The adjacent pinned archives contain the complete GCC, Binutils and MSYS2
runtime sources. `config/gnu-binutils-2.6-build.json` and
`tools/build_gnu_binutils_2_6.js` are copied unchanged from OB64 Decomp's
accepted build recipe; the four patches in `tools/toolchain/` are its exact
inputs. They describe the compiler host packages, flags, output identities and
Windows/N64 linker fixes. To reproduce Binutils, use that script's `--help`
instructions with a clean checkout at its pinned commit and the documented
MSYS2 host. It does not require the game or a Decomp checkout.

GCC source commit: `43d1cdb67ed135879869b5266f01efaaada5e35a`.
The accepted compiler was built as a Win32 console program with MSVC, with
`CFLAGS=/nologo /Od /Brepro /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS /Di386 /DWIN32 /D_WIN32 /D_X86_=1 /DALMOST_STDC`
and `LDFLAGS=/nologo /SUBSYSTEM:CONSOLE /Brepro`.
The repository's Windows configuration and Makefiles build target `cc1.exe`.
Run `configure.bat winnt i386` in an x86 MSVC developer shell with `sed` on
PATH. Copy the included `gcc-host/sys/file.h` shim into the source's `sys/`
directory; the backend includes it but does not use declarations from it.
Run `nmake cc1.exe` with the CFLAGS/LDFLAGS above. The binary pin records the
accepted host build; a different host compiler can change its executable hash.
The C preprocessor is built from the same source archive in a separate clean
directory, using the same configuration, shim and flags, plus `/DHAVE_STRERROR`.
That define selects the modern C runtime's `strerror` instead of the obsolete
`sys_errlist` declaration. Run `nmake cccp.exe` with the above CFLAGS/LDFLAGS and
`LIBS="obstack.obj alloca.obj kernel32.lib"`; modern MSVC supplies its static CRT
through object directives, replacing the makefile's obsolete `libc.lib` name.
Copy `cccp.exe` as `cpp.exe` beside `cc1.exe`. The pinned preprocessor was built
with MSVC 14.44.35228 for x86 and depends only on `KERNEL32.dll`.
No compiler source patches were needed. These two executables are distributed,
not libgcc or a platform SDK. Export disables host include directories and host
platform macros and supplies the target's MIPS/big-endian defines explicitly.

Qt source versions and binary/source archive checksums are in the adjacent
`dependency-sources.json` and top-level `dependencies.json`. Qt was supplied
by the official Qt 6.12.0 MSVC2022 archive and is dynamically linked, unchanged.
MSYS2's source archive includes its package build recipe and runtime patches.
