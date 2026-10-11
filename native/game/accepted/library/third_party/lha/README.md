# LHa for UNIX source baseline

This directory preserves **LHa for UNIX 1.14c, March 7, 1996**, from the
[Ryukoku University archive](https://ftp.st.ryukoku.ac.jp/pub/utils/archiver/lha-114c.tar.gz).
All 34 upstream files (219,024 bytes) are retained byte-for-byte under `upstream/`.
[upstream-manifest.json](upstream-manifest.json) records the archive and every file's identity.

Archive SHA-256: `D88AAE866980AF28B1789683E738852561CA489C98AC584B0E6580D9E73C1FB4`.
The archive MD5 also agrees with the historical FreeBSD `lha-114c.tgz` port record.
No upstream build or installation script was executed during intake.

The original January 1995 version 1.14 distribution was not found. This maintained
1.14-series release is the explicit comparison baseline; it must not be relabeled
as that original release or as a proven ancestor of Ogre Battle 64. A separately
retrieved Debian 1.14e source is retained in ignored research storage for comparisons.
Several decoder files are unchanged between these releases, so matching them alone
cannot uniquely identify the game's starting version.

## Copyright and distribution terms

Keep the entire original distribution and its notices. It has no standalone
`LICENSE` or `COPYING` file. The original distribution terms are in
[man/lha.n](upstream/man/lha.n), [man/lha.man](upstream/man/lha.man), and
[README.euc](upstream/README.euc), with further notices and credits in
[src/lharc.c](upstream/src/lharc.c) and the makefiles. The Japanese documents retain
their original EUC-JP encoding. The source headers credit their original authors
and contributors, including Nobutaka Watazaki; those headers are preserved.

## Game adaptation

Joe authorized this upstream-source intake and adaptation on September 23, 2026.
That authorization applies to this library; the game's separate bitstream and
custom LZSS code continue to use the ROM and project evidence.

Keep `upstream/` pristine. Game adaptations retain upstream attribution, document
each difference in [LOCAL_CHANGES.md](LOCAL_CHANGES.md), and must pass the ordinary
per-function linked byte diff followed by the complete-wave verifier. Vendoring,
algorithm correspondence, and uncompiled drafts do not count as matching C.
