# Native Lordly Caliber

Joe authorized a new C++/Qt product in this migration worktree. Existing browser
code is a feature and compatibility reference, not a required implementation.
Keep its original files intact while native features become complete.

Read `docs/architecture.md`, `docs/features.md`, and the active current-state
note in the parent repository (`docs/Plans/task-logs/classes-rebuild-20261002.md`).
Windows, integrated graphics, project folders with sharing archives, and portable
ZIP delivery are agreed. No Chromium, external Decomp dependency for users, full
game playback, or full-console emulation in the present scope.

Verify dependency releases and supported combinations in official online sources
before pinning. Explain intentional older versions. The historical N64 matching
toolchain is a separate compatibility requirement from the PC application tools.

Before adding a test, report exactly what it checks, why it is needed, and how
its inputs and expected results establish the claim. Report the invocation,
outcome, and limitations after running it. No tests merely for implementation
structure, formatting, or documentation.

Preserve source ROMs and unknown data. Perform real-ROM export checks in a
run-specific parent `Test ROMs/` directory under the source ROM filename. Game
sources compile during user export. Keep compiled ROMs and game-code objects
untracked. A working UI alone does not complete the corresponding game feature.

Keep core execution independent of Widgets. Use explicit storage widths and
address spaces. Never treat the early-boot address delta as an overlay mapping.
Use Qt shared libraries and include relevant third-party notices in packages.

Use the repository's Git identity and ordinary local commits. No push, merge to
main, publication, or release without Joe's direct authorization.
