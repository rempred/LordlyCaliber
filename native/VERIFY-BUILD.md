# Verify a Lordly Caliber build

Download `LordlyCaliber-main-COMMIT-windows-x64.zip` from Actions or a release.
Extract it once, open the `LordlyCaliber-main-COMMIT-windows-x64` folder, and run
`LordlyCaliber.exe`. The download contains the application files directly.

The matching `.zip.sha256` and combined `attestations.jsonl` are separate release
assets, or are available in the same Actions run's `verification-files` artifact.
The ZIP checksum is also displayed in the run summary. Verification files are
optional for running the editor; online signature verification needs only the
application ZIP and the [GitHub CLI](https://cli.github.com/).

## Check the download

In PowerShell, replace the filename below with the downloaded ZIP's name:

```powershell
$zip = 'LordlyCaliber-main-COMMIT-windows-x64.zip'
$expected = ((Get-Content -LiteralPath "$zip.sha256" -Raw).Trim() -split '\s+')[0]
if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $expected) {
    throw 'Download checksum mismatch'
}
```

A checksum detects a changed download. Verify the signed origin of the ZIP you
downloaded with the GitHub CLI:

```powershell
gh attestation verify $zip --repo rempred/LordlyCaliber --source-ref refs/heads/main --signer-workflow rempred/LordlyCaliber/.github/workflows/windows-release.yml --deny-self-hosted-runners
```

The command must succeed and identify the intended Main commit. Add
`--source-digest FULL_COMMIT_SHA` to require a specific commit. To use the
downloaded attestation bundle, append `--bundle attestations.jsonl`; the CLI can
still require network access for its trust material. This is not an offline
verification procedure.

After extracting the portable ZIP, the same command also works with the path to
`LordlyCaliber.exe`, `LordlyCaliber-cli.exe` or `FILES.sha256` as its file argument.
Those files are attested before upload; their bundle is included beside the
application folder. The combined bundle from `verification-files` or the release
also covers the completed download ZIP. The ZIP's own attestation is generated
after upload and therefore accompanies the ZIP separately.

`build-info.json` records the source commit, workflow run and dependency policy.
`FILES.sha256` lists checksums for every other file inside the package. Those
records are protected by the ZIP attestation; reading metadata alone does not
authenticate a download.

## What the build establishes

GitHub-hosted Windows runners check out the triggering Main commit and compile
the editor and CLI from this repository. The workflow is public and its actions
are pinned to commit IDs. Qt is downloaded from its official archive and checked
against the repository's SHA-256 pins. The N64 export tools are a separately
authenticated, prebuilt third-party dependency; their individual identities,
source revisions, build recipes and patches ship with the package. The entire
GitHub-generated download ZIP, including dependencies, is attested using the
upload action's archive digest. Releases use that exact ZIP without repacking it.

The workflow tests ZIP integrity, package completeness and startup with developer
paths removed. It runs the public test suite, with private-ROM cases skipped.
It does not use or publish a game ROM or claim a new emulator playthrough.

Attestations establish build origin and integrity, not freedom from bugs or
independent byte-for-byte reproducibility. GitHub's
[attestation documentation](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations)
describes the verification model.
