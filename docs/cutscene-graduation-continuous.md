# Graduation continuation

This candidate plays the saved Graduation checkpoint through modeled terminal release.
It keeps all eight Actors, completes movement and pose changes, advances dialogue, and fades to black.
Independent review is pending.

Load the US revision-zero ROM and select Graduation.
Use **Import playback inputs** to load `../../docs/reviews/cutscene-graduation-continuous-20260917/playback-input.json` relative to this document directory.
Use the ordinary Play and seek controls.
The preview footer says **Automatic dialogue advance: simulated timing.**

The ready input uses `normal-mode-two-continuous` with `native-captured-continuous-v1` memory.
Its `resourceServices.resourceSchedule.pageAdvancePolicy` is `automatic`.
The policy acknowledges completed, nonbranching text on the next eligible resource pass.
It sends an A pulse and then releases it.
The native text service controls writing, clearing, and closing.
There are no fitted reading delays or historical button timestamps.
Story choices and unsupported service paths remain explicit boundaries.

The sequence reaches modeled termination after 698 declared resource passes.
Preview index zero follows the first update from the original saved checkpoint.
These pass counts do not establish historical displayed-frame timing or a natural scene launch.
Terminal release does not claim execution of later scene teardown or the next scene.

The sibling `neutral-playback-input.json` preserves neutral controls.
It stops at the first completed page with `awaiting-dialogue-input` after 332 passes.
That result is a normal acknowledgement wait, not a frozen text service.

The shared runtime supplies movement, pose selection, dialogue lifecycle, resource scheduling, color fading, and native audio queue requests.
Qualified native code supplies the captured callback and remaining helper calculations.
Dialogue anchors use the current native Actor matrix and viewport.
Current resource ownership and color state are exchanged between the services.
Future expected states remain in tests, outside playback inputs.

Ordinary composition includes Actor sprites, scene props, background layers, dialogue artwork, and the final fade.
Strict framebuffer capture retains its scene-prop ownership restriction.
Original displayed-pixel equality remains outside this result.

Run `node editor/tests/cutscene-graduation-continuous.test.js` from the parent repository.
The focused test covers native record comparisons, neutral and automatic policies, public load, seeking, repeat compilation, asynchronous cancellation, and storage limits.
The parent milestone directory owns the generator, native controls, ready inputs, images, and final handoff.
