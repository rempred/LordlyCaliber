# Native Lordly Caliber

Joe authorized a new C++/Qt product in this migration worktree. The archived
browser application is in `../Editor-Old/`; shared JavaScript, fixtures and tools
still used by native builds, tests or generators remain at the repository root.
Keep those active dependencies available. The archived browser code is a feature
and compatibility reference, not a required implementation.

Read `docs/architecture.md`, `docs/features.md`, and the active current-state
note in the parent repository (`docs/Plans/task-logs/classes-rebuild-20261002.md`).
Windows, integrated graphics, project folders with sharing archives, and portable
ZIP delivery are agreed. No Chromium, external Decomp dependency for users, full
game playback, or full-console emulation in the present scope.

Unless Joe explicitly says otherwise, build authorized editor changes and update
`native/live/` so he can test them. Link the live executable when reporting the
update. This does not authorize pushes or releases (Joe, 2026-10-27).

Joe gives standing permission to close the live editor for authorized updates
without asking again, including discarding unsaved session changes. He does not
keep real work in the editor during these updates. Do not create extra project
saves, backups or recovery snapshots just to preserve the session before an
update. Reopen the updated editor and link its executable afterward. This
permission applies to editor updates; honor any later instruction to retain
specific work (Joe, 2026-10-28).

Verify dependency releases and supported combinations in official online sources
before pinning. Explain intentional older versions. The historical N64 matching
toolchain is a separate compatibility requirement from the PC application tools.

Before adding a test, report exactly what it checks, why it is needed, and how
its inputs and expected results establish the claim. Report the invocation,
outcome, and limitations after running it. No tests merely for implementation
structure, formatting, or documentation.

New or expanded features require a small behavior matrix defined before coding:
representative inputs, relevant options/game paths, expected results, and what
was actually observed. Put it in the existing task note or test recipe. Exercise
the editor entry point and exported game's consumers where behavior depends on
them. For attacks, include several renamed/new attacks, animation modes, damage
or status results, labels, and completed playback. Use copied battery fixtures
and cold boot for game tests. A helper pass, screenshot of a preview, skipped
ROM test, or successful build does not substitute for an untested game path.
Reuse passing cases when their relevant inputs have not changed. The parent
guide is `docs/feature-testing.md`; keep ROM/save fixtures local.

Preserve source ROMs and unknown data. Perform real-ROM export checks in a
run-specific parent `Test ROMs/` directory under the source ROM filename. Game
sources compile during user export. Keep compiled ROMs and game-code objects
untracked. A working UI alone does not complete the corresponding game feature.

Keep core execution independent of Widgets. Use explicit storage widths and
address spaces. Never treat the early-boot address delta as an overlay mapping.
Use Qt shared libraries and include relevant third-party notices in packages.

Use the repository's Git identity and ordinary local commits. No push, merge to
main, publication, or release without Joe's direct authorization.
