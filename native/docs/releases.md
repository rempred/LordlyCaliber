# GitHub Windows builds

`.github/workflows/windows-release.yml` builds every push to `main` and also
supports **Actions → Windows release ZIP → Run workflow**. Select `main`.
Other branches are deliberately excluded from this release workflow.

Each successful run uploads `release-files-RUN-ATTEMPT`, containing:

- The complete portable `LordlyCaliber-main-COMMIT-windows-x64.zip`.
- Its SHA-256 checksum file.
- Signed provenance for the ZIP, GUI executable and CLI executable.
- Source/build metadata and [verification instructions](../VERIFY-BUILD.md).

Push-triggered builds do not create GitHub releases. To make a public download,
run the workflow manually with **Publish this Main build as a GitHub prerelease**
enabled. Only that explicit run grants the separate publishing job permission
to create a release. It creates a unique `native-main-RUN-ATTEMPT` tag pointing
to the exact built commit and never overwrites an existing release. These are
development prereleases; this workflow does not declare a stable 0.5 release.

The build job uses Windows Server 2022's MSVC 2022 toolset, pinned Qt/CMake
archives, and the authenticated N64 dependency in
[`rebuild/TOOLCHAIN-DISTRIBUTION.md`](../rebuild/TOOLCHAIN-DISTRIBUTION.md).
There are no private runner dependencies, ROM uploads or required repository
secrets. GitHub supplies short-lived tokens for attestations and optional release
publishing. Standard public-repository GitHub-hosted runners are used.

## Local checks

Use PowerShell 7 with the supported Visual Studio C++ tools installed:

```powershell
native/tools/bootstrap.ps1
native/tools/bootstrap-n64.ps1
$env:QT_QPA_PLATFORM = 'offscreen'
native/tools/build.ps1 -Test
native/tools/package.ps1 -Destination native/dist/local-check
native/tools/check-release.ps1 -Zip native/dist/local-check.zip -ExpectedCommit (git rev-parse HEAD)
```

Use a fresh destination each time. Add `-RequireClean` to the final command when
checking a committed build. CI always requires an unchanged source checkout
and verifies that HEAD matches its triggering Main commit. Local development
packages record source changes, including non-ignored untracked files.

ROM-dependent tests are skipped in public CI. Maintainers can separately pass
`-RomPath` to the existing build checks and use `check-package.ps1` with a private
ROM for a compile/export/reopen check. Never upload its ROM outputs or reports
containing private payloads to the public release workflow.
