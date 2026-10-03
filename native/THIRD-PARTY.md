# Third-party components

This portable build uses dynamically linked Qt 6.12.0 Core, Gui, Widgets,
Concurrent and SVG libraries. The open-source Qt libraries are provided under
LGPL-3.0; applicable license texts are in `licenses/qtbase-6` and
`licenses/qtsvg-6`. Their complete corresponding source archives, including
third-party copyright notices, are in `dependency-sources/`. Qt remains
replaceable as separate DLLs. No additional restriction is imposed on modifying
these libraries or reverse engineering for debugging such modifications.
Qt information: https://www.qt.io/ ; https://doc.qt.io/qt-6/licensing.html .

The export compiler is the decompals KMC/GCC 2.7.2 source revision
`43d1cdb67ed135879869b5266f01efaaada5e35a` (GPL-2.0). The assembler, linker and
object extractor use GNU Binutils 2.6 revision
`54514ded39ceb32165a125ddba04ca5b551773a2` (GPL-2.0), including the documented
Windows and N64 linker patches. These tools run as separate processes.
Their license texts are in `licenses/mips-*`, source archives are in
`dependency-sources/`, and build recipes and patches are in
`dependency-sources/toolchain-source/`.

The GNU tools include MSYS2 runtime 3.6.10-2. Its full source package, build
recipe, patches and license notices are included in
`dependency-sources/msys2-runtime-3.6.10-2.src.tar.zst`.
Project information: https://www.msys2.org/ .

Microsoft Visual C++ runtime DLLs are copied from the installed Visual Studio
redistributable directory for application-local deployment, under Microsoft's
redistribution terms. Microsoft retains its copyright in these files.
https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files

The application source is available in the Lordly Caliber repository under its
existing MIT license: https://github.com/rempred/LordlyCaliber . This development
branch has not been published or released. Recovered game source provenance is
recorded in `game/README.md`; no original ROM or game-art archive is included.
