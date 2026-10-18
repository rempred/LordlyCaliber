"""Independent container/decoder checks for playbackExportsMatchPreview outputs.

Run after sequence_editor_tests playbackExportsMatchPreview:
  python native/tests/verify-combat-exports.py native/build/Release
Uses Python's ZIP/CRC reader and installed FFmpeg/FFprobe, not the export writer.
"""
import json
from pathlib import Path
import subprocess
import sys
import zipfile

root = Path(sys.argv[1]).resolve()


def run(*args):
    return subprocess.check_output(args, stderr=subprocess.PIPE)


def rgba(path, *options):
    return run("ffmpeg", "-v", "error", *options, "-i", str(path),
               "-fps_mode", "passthrough", "-f", "rawvideo", "-pix_fmt", "rgba", "-")


expected = [rgba(root / f"combat-playback-{i}.png") for i in range(4)]
order = [0, 0, 1, 2, 3]
with zipfile.ZipFile(root / "combat-playback.zip") as archive:
    assert archive.testzip() is None, "ZIP CRC failure"
    manifest = json.loads(archive.read("animation.json"))
    assert manifest["framesPerSecond"] == 15 and manifest["totalTicks"] == 5
    assert manifest["source"]["stoppedAt"] == "cycle"
    assert len(archive.namelist()) == 7
    assert len(expected[0]) == manifest["width"] * manifest["height"] * 4
    for index, entry in enumerate(manifest["frames"]):
        assert entry["startTick"] == index and entry["durationTicks"] == 1
        decoded = subprocess.check_output(
            ["ffmpeg", "-v", "error", "-f", "image2pipe", "-i", "pipe:0",
             "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgba", "-"],
            input=archive.read(entry["file"]), stderr=subprocess.PIPE)
        assert decoded == expected[order[index]], f"ZIP pixels differ at step {index}"

assert rgba(root / "combat-playback.apng", "-ignore_loop", "1") == b"".join(expected)
apng = json.loads(run("ffprobe", "-v", "error", "-ignore_loop", "1", "-show_frames",
                     "-of", "json", str(root / "combat-playback.apng")))["frames"]
assert len(apng) == 4
for index, frame in enumerate(apng):
    assert abs(float(frame["duration_time"]) - (2 if index == 0 else 1) / 15) < 0.00001

webm = rgba(root / "combat-playback.webm", "-c:v", "libvpx-vp9")
size = len(expected[0])
assert len(webm) == size * 5
for index, source in enumerate(order):
    image = webm[index * size:(index + 1) * size]
    assert image[3::4] == expected[source][3::4], f"WebM alpha differs at step {index}"
    # VP9 YUV420 chroma conversion is not an exact RGB sprite format. Check
    # visible color correspondence globally, while requiring alpha bit-exact.
    errors = [abs(image[p + c] - expected[source][p + c])
              for p in range(0, size, 4) if image[p + 3] > 64 for c in range(3)]
    assert errors and sum(errors) / len(errors) < 18, f"WebM colors/order differ at step {index}"
frames = json.loads(run("ffprobe", "-v", "error", "-show_frames", "-of", "json",
                       str(root / "combat-playback.webm")))["frames"]
assert len(frames) == 5
for index, frame in enumerate(frames):
    assert abs(float(frame["best_effort_timestamp_time"]) - index / 15) < 0.0011
print("PASS: ZIP CRCs and 5 PNG steps, 4 APNG holds, 5 WebM steps; "
      "preview pixels, fades, frame order and 15 Hz timing verified.")
