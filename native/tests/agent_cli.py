"""Exercise the packaged agent CLI against a user's US Rev 0 ROM, in a fresh work directory."""
import argparse
import base64
import hashlib
import json
import subprocess
from pathlib import Path


def main():
    args = argparse.ArgumentParser(description=__doc__)
    for option in ("cli", "rom", "work"):
        args.add_argument("--" + option, type=Path, required=True)
    args.add_argument("--tools", type=Path)
    args.add_argument("--library", type=Path)
    options = args.parse_args()
    options.work.mkdir(parents=True, exist_ok=False)
    rom = options.rom.resolve()
    original = rom.read_bytes()
    command = [str(options.cli.resolve()), "--rom", str(rom)]
    if options.tools:
        command += ["--tools", str(options.tools.resolve())]
    if options.library:
        command += ["--library", str(options.library.resolve())]

    def run(ops, *extra, failure=None, source=None):
        argv = command.copy()
        if source:
            argv[2] = str(source)
        args = argv + ["--commands", "-", *map(str, extra)]
        proc = subprocess.run(args, input=json.dumps(ops), capture_output=True, encoding="utf-8", timeout=240)
        if failure:
            assert proc.returncode and failure.lower() in proc.stderr.lower(), proc.stderr
            return
        assert proc.returncode == 0, proc.stderr
        return json.loads(proc.stdout)

    def output(label):
        directory = options.work / label
        directory.mkdir()
        return directory / rom.name

    untouched = output("unedited")
    run([], "--output", untouched)
    assert untouched.read_bytes() == original
    check = [{"op": "records", "table": "classes", "ids": [5, 6], "fields": ["strBase"]}]
    update = dict(op="set", table="classes", ids=[5, 6], values={"strBase": 300})
    project = options.work / "classes.lcproject"
    edited = output("classes")
    run([update], "--save-project", project, "--output", edited)
    records = run(check, source=edited)["results"][0]
    assert [r["values"]["strBase"] for r in records] == [300, 300]
    before = project.read_bytes()
    run([update, dict(update, ids=[9999])], "--save-project", project, failure="record ID")
    assert project.read_bytes() == before

    info = run([{"op": "regions"}, {"op": "owners", "query": "func_002831E0"},
                {"op": "assets"}, {"op": "toolchain"}, {"op": "domains"}])["results"]
    regions, owners, assets, tools, domains = info
    cursor = 0
    for region in regions:
        assert region["offset"] == cursor
        cursor += region["size"]
    assert cursor == len(original) and sum("assembly" in r for r in regions) == 6184
    assert "cc1.exe" in tools["tools"] and "sceneEvents" in domains
    owner = next(r for r in owners if "func_002831E0" in r["targets"])
    script_region = next(r for r in regions if r.get("offset") == owner["parts"][0]["start"])
    fetched = run([{"op": "read", "path": owner["source"]},
                   {"op": "region", "name": script_region["name"]},
                   {"op": "asset", "offset": assets[0]["offset"]},
                   {"op": "icon", "table": "consumables", "id": 1}])["results"]
    assert "func_002831E0" in fetched[0] and ".word" in fetched[1]["source"]
    decoded = base64.b64decode(fetched[2]["base64"])
    assert hashlib.sha256(decoded).hexdigest() == fetched[2]["sha256"]
    assert base64.b64decode(fetched[3]["base64"]).startswith(b"\x89PNG")
    asm = "jr $ra\naddiu $v0,$zero,42\n"
    c = "int func_002831E0(void) { return 42; }\n"
    for language, source in (("c", c), ("asm", asm)):
        patch = {"id": "return42", "owner": "func_002831E0", "language": language, "source": source}
        rom_out = output(language)
        archive = options.work / (language + ".lcproject")
        run([{"op": "patch.put", "patch": patch}], "--save-project", archive, "--output", rom_out)
        region = run([{"op": "region", "name": script_region["name"]}], source=rom_out)["results"][0]
        words = [int(line.split()[1], 16) for line in region["source"].splitlines()]
        # Both independent functions must return the literal from the delay slot.
        assert words[:2] == [0x03E00008, 0x2402002A], (language, words[:4])
        disabled = output(language + "-disabled")
        run([{"op": "patch.enable", "id": "return42", "enabled": False}],
            "--project", archive, "--output", disabled)
        assert disabled.read_bytes() == original
    part = owner["parts"][0]
    explicit = dict(id="bounded", language="asm", source=asm, offset=part["start"],
                    address=part["address"], size=part["size"], expectedSha256=part["sha256"])
    for patch, error in ((dict(explicit, expectedSha256="0" * 64), "conflicts"),
                         (dict(explicit, source="nop\n" * 100), "does not fit")):
        rejected = output(error)
        run([{"op": "patch.put", "patch": patch}], "--output", rejected, failure=error)
        assert not rejected.exists()
    reject = output("overlap")
    run([{"op": "patch.put", "patch": explicit},
         {"op": "patch.put", "patch": dict(explicit, id="overlap")}],
        "--output", reject, failure="overlaps")
    assert not reject.exists()
    missing = {"id": "missing", "module": "unused_module", "language": "c",
               "expectedSourceSha256": "0" * 64, "source": "int missing(void) { return 1; }"}
    unused = output("unused-module")
    run([{"op": "patch.put", "patch": missing}], "--output", unused, failure="unused by this project")
    assert not unused.exists()

    with subprocess.Popen(command + ["--project", str(archive), "--workspace", "-"],
                          stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          text=True, encoding="utf-8") as session:
        def request(op, **data):
            session.stdin.write(json.dumps(dict(op=op, **data)) + "\n")
            session.stdin.flush()
            response = json.loads(session.stdout.readline())
            assert response["ok"], response
            return response.get("result")

        assert json.loads(session.stdout.readline())["ready"]
        tabs = request("items", name="workspaceNavigation")["items"]
        for index, title in enumerate(tabs):
            request("select", name="workspaceNavigation", index=index)
            assert request("controls", query="workspaceNavigation")["controls"], title
        request("navigate", location={"editor": "Code patches"})
        assert request("inspect", name="projectPatchSource")["text"].strip() == asm.strip()
        request("select", name="projectCodePatches", index=[0])
        request("key", name="projectCodePatches", key="Space")
        ok = next(c for c in request("controls", query="OK")["controls"]
                  if c["type"] == "QPushButton" and c.get("text", "").replace("&", "") == "OK")
        request("activate", id=ok["id"])
        toggled = options.work / "disabled-in-desktop.lcproject"
        request("save", path=str(toggled))
        assert not run([{"op": "patches"}], "--project", toggled)["results"][0][0]["enabled"]
        undo = next(c for c in request("controls", query="Undo")["controls"]
                    if c["type"] == "QAction" and c.get("shortcut") == "Ctrl+Z")
        request("activate", id=undo["id"])
        # Draw across the middle of the first weapon icon using the actual art canvas.
        request("navigate", location={"editor": "icon", "id": 1})
        request("select", name="artPixelTool", text="Eraser")
        canvas = request("inspect", name="artIconCanvas")
        y = canvas["height"] // 2
        for event, x in (("press", 2), ("move", canvas["width"] - 2), ("release", canvas["width"] - 2)):
            request("pointer", id=canvas["id"], event=event, x=x, y=y)
        request("select", name="workspaceNavigation", text="Consumables")
        rows = request("items", name="consumablesTable")["items"]
        leaf = next(i for i, row in enumerate(rows) if "Heal Leaf" in row)
        request("select", name="consumablesTable", index=[leaf])
        button = request("inspect", name="assignConsumableUse")
        for event in ("press", "release"):
            request("pointer", id=button["id"], event=event, x=button["width"] // 2, y=button["height"] // 2)
        assert "Heal Leaf" in request("controls", query="assignmentEffect")["window"]
        request("select", name="assignmentEffect", text="Restore HP")
        request("set", name="assignmentAmount", value=43)
        save = next(c for c in request("controls", query="Save")["controls"]
                    if c["type"] == "QPushButton" and "Save" in c.get("text", ""))
        request("activate", id=save["id"])
        widget_project = options.work / "from-workspace.lcproject"
        request("save", path=str(widget_project))
        assert request("image")["mime"] == "image/png"
        session.stdin.close()
        assert session.wait(timeout=20) == 0
    icon = {"op": "icon", "table": "items", "id": 1}
    model, patches, artwork = run([{"op": "domain", "name": "consumableEffects"}, {"op": "patches"}, icon],
                                   "--project", widget_project)["results"]
    assert model["trees"]["1"][0]["minimum"] == 43
    assert patches[0].get("enabled", True) and artwork["sha256"] != run([icon])["results"][0]["sha256"]
    modules = run([], "--project", widget_project, "--build-plan")["modules"]
    active = next(row for row in modules if row["name"] == "item_tree_execute")
    assert "lc_tree_prepare" in active["entries"]
    replacement = dict(id="tree", language="c", module=active["name"], source=active["source"],
                       expectedSourceSha256=active["originalSourceSha256"])
    for invalid, expected in ((dict(replacement, source="int example(void) { return 1; }"), "removed module entry"),
                              (dict(replacement, expectedSourceSha256="0" * 64), "differs from the patch baseline")):
        rejected = output(expected)
        run([{"op": "patch.put", "patch": invalid}], "--project", widget_project,
            "--output", rejected, failure=expected)
        assert not rejected.exists()
    rejected = output("duplicate-module")
    run([{"op": "patch.put", "patch": replacement},
         {"op": "patch.put", "patch": dict(replacement, id="second_tree")}],
        "--project", widget_project, "--output", rejected, failure="two enabled patches")
    assert not rejected.exists()
    assert rom.read_bytes() == original
    print(f"Passed shared project operations, {len(tabs)} workspaces, C/assembly patches, "
          f"{len(regions)} ROM regions, {len(assets)} archive headers, conflicts and ROM identity.")


if __name__ == "__main__":
    main()
