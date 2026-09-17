# Saved Graduation continuation

The saved Graduation input advances all eight Actors through 69 eligible scheduler updates.
The input includes both alternate-pose Actors, camera state, owner records, and native auxiliary memory.
Independent review accepts this bounded saved-state continuation.
See the parent review at `docs/reviews/cutscene-graduation-resume-review-20260917/review.md`.

Load the US revision-zero ROM and select the Graduation scene.
Use **Import playback inputs** to load `../../docs/reviews/cutscene-graduation-resume-integration-20260917/playback-input.json` from this document directory.
Use the ordinary preview controls to inspect or seek the movement window.
Preview index zero follows one update from the saved checkpoint.
Indices 34 and 68 follow updates 35 and 69.
These indices do not assert historical displayed-frame timing.

The `normal-mode-two-held-movement-window` profile requires normal scheduling and scene mode two.
It retains the saved movement-count query until the requested update limit.
The limit must precede movement completion.
It does not execute the subsequent command or establish a natural scene launch.

The existing shared movement integrator and pose decoder compute Actor states.
Qualified native code computes the remaining scheduler, projection-transform, and cache effects from supplied initial memory.
The product does not consume expected future records from the test fixture.
The alternate registry resolves source/context 32/32, flags 0/1, State ten through handle 36.

The profile rejects alternate scheduling, nonempty secondary sprite slots, unavailable native memory, and code differing from the qualified ROM.
It also stops at unsupported proximity movement, terrain movement, pose controls, or a parser transition.
Supported pose controls are empty, frame, loop, and extended frame.
Mode-two Actor geometry uses saved camera inputs without fabricating mode-zero presentation channels.

Ordinary composition includes all eight Actor sprites, twelve scene props, and two background layers.
Strict framebuffer capture retains its existing scene-prop ownership restriction.
Composed appearance is not proof of original displayed pixels or historical timing.

Run `node editor/tests/cutscene-graduation-resume.test.js` from the parent repository.
The test compares 552 complete Actor records, 13,284 auxiliary-memory bytes, and 120 previously accepted motion/pose samples.
It also checks public import/load, seeking, repeat compilation, asynchronous compilation, cancellation, rejected inputs, and existing storage ceilings.

The parent milestone directory owns `probe.py` and `generate.py`.
Their inputs are the master ROM, existing derived Graduation RAM, frozen native-probe definitions, and original native assembly references.
Run the probe before the generator.
The generator writes the product code table, test fixture, ready playback input, and source qualification report.
