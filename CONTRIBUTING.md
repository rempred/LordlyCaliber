# Contributing to Lordly Caliber

Pull requests for editor features, bug fixes, documentation, tests, and tooling are welcome here. If you have a bug report or an idea and would like someone else to implement it, [open an editor issue](https://github.com/rempred/LordlyCaliber/issues/new).

## Send an editor pull request

1. Fork `rempred/LordlyCaliber` and create a branch from its current `main`. The Windows editor lives in `native/`. The [README](README.md#build-from-source-windows-x64) explains how to build it and run the tests.
2. Make the change and exercise the part of the editor it affects. For an export change, build a ROM from your own supported copy, reopen the export, and try the affected path in game. Keep ROMs out of the PR.
3. Push the branch to your fork. On GitHub, choose **compare across forks**, set the base to **rempred/LordlyCaliber → main**, and select your fork and branch as the head. [Compare branches](https://github.com/rempred/LordlyCaliber/compare)
4. In the description, explain what a user can do after the change and how a reviewer can try it. Include the checks you ran, their results, and anything you couldn’t verify. A before-and-after screenshot helps with a visual change. Link a related issue if there is one.

## Verify a byte-identical ROM export with no edits

For PRs that affect ROM building or export, check that the editor built from your branch can reproduce a clean **US Rev 0** ROM byte for byte.

1. Open your original ROM in a fresh editor session. Start with the ROM alone, leaving all game data unchanged.
2. Choose **File → Build and export ROM…**, then **Export ROM…**. Save to a separate folder, keeping the original filename and extension. The editor preserves the input byte order, so compare the export with the exact file you opened.
3. In PowerShell, substitute your two paths and compare the complete files.

```powershell
$sourceRom = 'C:\ROMs\Ogre Battle 64.z64'
$exportedRom = 'C:\ROMs\no-edits\Ogre Battle 64.z64'
fc.exe /b $sourceRom $exportedRom
if ($LASTEXITCODE -ne 0) { throw 'The no-edit ROM comparison failed.' }
Get-FileHash -Algorithm SHA256 -LiteralPath $sourceRom, $exportedRom
```

A passing comparison reports **no differences** and prints the same SHA-256 for both files. Include the ROM revision, byte order, and matching hash in the PR. If the comparison fails, include the first differing file offset or the comparison error.

The automated suite also checks this in `acceptedCBaselineAndUneditedExport`. From the repository root, run `.\native\tools\build.ps1 -Test -RomPath $sourceRom` to include the ROM-dependent check.

## Contribute matching game C

Lordly Caliber imports matching game C after it has been accepted in [OB64-Decomp](https://github.com/rempred/OB64-Decomp). If you independently recovered a game function from the ROM, submit the match there using its [matching and verification workflow](https://github.com/rempred/OB64-Decomp/blob/main/docs/WORKFLOW.md).

If the match comes from another decompilation project, open an OB64-Decomp issue with the ROM revision, function offsets, and how you verified the match. Identify which addresses are ROM offsets and which are runtime addresses. That gives the team enough information to check the finding and independently recover the function under its clean-room rules. [Open a Decomp issue](https://github.com/rempred/OB64-Decomp/issues/new) · [Clean-room policy](https://github.com/rempred/OB64-Decomp/blob/main/AGENTS.md#clean-room-boundary)

If an editor feature uses a newly accepted match, send the feature PR here and link the Decomp commit it uses. For example, a recovered cutscene command belongs in OB64-Decomp. The editor control that lets a mod author use that command belongs in Lordly Caliber.
