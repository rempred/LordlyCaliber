"""Verify CLI edit history through commands, the desktop changelog and both project formats."""
import argparse
import base64
import json
import subprocess
import time
from pathlib import Path

from portable_workspace import Portable


HISTORY = {"op": "agent.edits"}
CHANGELOG = {"op": "changes"}
FIELD_EDIT = dict(op="set", table="classes", ids=[5, 6], values={"strBase": 301})
PATCH = dict(id="activity_demo", owner="func_002831E0", language="asm",
             source="jr $ra\naddiu $v0,$zero,42\n")
ADD_PATCH = {"op": "patch.put", "patch": PATCH}


def command_history(client, archive, folder):
    results = client.commands([FIELD_EDIT, ADD_PATCH, HISTORY, CHANGELOG],
                              "--save-project", archive)["results"]
    initial, report = results[-2:]
    assert len(initial) == 3 and all(row["status"] == "Matches project" for row in initial), initial
    assert "CLI edits" in report and "301" in report and "activity_demo" in report

    saved = client.commands([HISTORY], "--project", archive, "--save-project", folder)["results"][0]
    assert saved == initial
    manifest_path = folder / "project.json"
    manifest = json.loads(manifest_path.read_text("utf8"))
    assert len(manifest["agentActivity"]) == 1
    assert manifest["codePatches"][0]["source"] == PATCH["source"]

    noop = client.commands([FIELD_EDIT, ADD_PATCH, HISTORY], "--project", folder,
                           "--save-project", folder)["results"][-1]
    assert noop == initial
    without_history = {k: v for k, v in manifest.items() if k != "agentActivity"}
    kept = client.commands([{"op": "apply", "manifest": without_history}, HISTORY], "--project",
                           folder, "--save-project", client.work / "Applied.lcproject")["results"][-1]
    assert kept == initial
    saved_bytes = manifest_path.read_bytes()
    try:
        client.commands([dict(FIELD_EDIT, values={"strBase": 302}), dict(FIELD_EDIT, ids=[9000])],
                        "--project", folder, "--save-project", folder)
    except RuntimeError:
        pass
    else:
        raise AssertionError("Invalid edit unexpectedly passed")
    assert manifest_path.read_bytes() == saved_bytes
    return manifest


def check_changelog(ui, work):
    ui.call("activate", name="agentsOpenChangelog")
    for _ in range(100):
        if ui.field("copyChangeReport")["enabled"]:
            break
        time.sleep(0.15)
    else:
        raise AssertionError("Desktop changelog did not finish")
    ui.call("activate", name="copyChangeReport")
    copied = ui.call("clipboard")
    saved = work / "Project changelog.txt"
    ui.call("activate", name="saveChangeReport")
    ui.file(saved)
    for text in (copied, saved.read_text("utf8")):
        assert "CLI edits" in text and "activity_demo" in text and "301" in text
    ui.call("reject")


def workspace_history(client, folder, updated):
    with client.workspace("--project", folder) as ui:
        ui.page("Tools", "Agents")
        ui.call("focus", name="agentsCliPath")
        ui.call("key", name="agentsCliPath", key="Ctrl+A")
        ui.call("key", name="agentsCliPath", key="Ctrl+C")
        copied_cli = Path(ui.call("clipboard"))
        assert copied_cli == client.cli and copied_cli.is_file()
        guide = subprocess.run([str(copied_cli), "--guide"], check=True, capture_output=True,
                               encoding="utf8", cwd=client.work, env=client.env, timeout=30).stdout
        ui.call("activate", name="agentsReadGuide")
        displayed = ui.field("agentsGuideText")["text"]
        for example in ("Knight and Berserker", "Wentinus", "assembly predicate", "byte for byte"):
            assert example in displayed and example in guide, example
        ui.call("reject")
        assert ui.call("items", name="agentsEdits")["count"] == 3
        check_changelog(ui, client.work)

        ui.call("activate", name="agentsOpenPatches")
        assert ui.field("projectPatchSource")["text"].strip() == PATCH["source"].strip()
        ui.call("select", name="projectCodePatches", index=[0])
        ui.call("key", name="projectCodePatches", key="Space")
        ui.button("OK")
        ui.page("Tools", "Agents")
        items = ui.call("items", name="agentsEdits")["items"]
        assert any("Pending save" in row and "CLI workspace" in row for row in items), items
        for label, count in (("Undo", 3), ("Redo", 4)):
            action = next(a for a in ui.call("controls", query=label)["controls"]
                          if a["type"] == "QAction" and a.get("enabled")
                          and a.get("text", "").replace("&", "").startswith(label))
            ui.call("activate", id=action["id"])
            items = ui.call("items", name="agentsEdits")["items"]
            assert len(items) == count
            assert any("Pending save" in row for row in items) == (label == "Redo"), items
        screenshot = ui.call("image")
        (client.work / "tools-agents.png").write_bytes(base64.b64decode(screenshot["base64"]))
        ui.call("save", path=str(updated))
        assert all("Pending save" not in row for row in ui.call("items", name="agentsEdits")["items"])
        ui.call("save", path=str(updated))

    history, changelog = client.commands([HISTORY, CHANGELOG], "--project", updated)["results"]
    assert len(history) == 4 and history[0]["source"] == "CLI workspace"
    assert "Disabled" in history[0]["description"] and "Disabled" in changelog
    assert sum(r["status"] == "Earlier edit" for r in history) == 1


def later_changes(client, folder, manifest):
    # Simulate a later desktop edit to a single field, preserving its saved history.
    edit = dict(FIELD_EDIT, ids=[5], values={"strBase": 302})
    manual = client.commands([edit, {"op": "manifest"}], "--project", folder,
                             "--save-project", client.work / "temporary.lcproject")["results"][-1]
    manual["agentActivity"] = manifest["agentActivity"]
    changed = client.work / "manual.json"
    changed.write_text(json.dumps(manual), encoding="utf8")
    later = client.commands([HISTORY], "--project", changed)["results"][0]
    assert len(later) == 3 and sum(r["status"] == "Changed since" for r in later) == 1
    manual["records"]["classes"].pop("5")
    changed.write_text(json.dumps(manual), encoding="utf8")
    reverted = client.commands([HISTORY], "--project", changed)["results"][0]
    assert sum(r["status"] == "Reverted" for r in reverted) == 1

    # An older project still has editable source without any recorded author.
    del manual["agentActivity"]
    changed.write_text(json.dumps(manual), encoding="utf8")
    resaved = client.work / "Resaved legacy.lcproject"
    with client.workspace("--project", changed) as ui:
        ui.page("Tools", "Agents")
        assert any("Origin unrecorded" in r for r in ui.call("items", name="agentsEdits")["items"])
        ui.call("save", path=str(resaved))
    history, data = client.commands([HISTORY, {"op": "manifest"}], "--project", resaved)["results"]
    assert history == [] and data["codePatches"][0]["source"] == PATCH["source"]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("cli", "rom", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    client = Portable(args.cli, args.rom, args.work)
    folder = client.work / "Project folder"
    manifest = command_history(client, client.work / "First agent edits.lcproject", folder)
    workspace_history(client, folder, client.work / "Workspace edits.lcproject")
    later_changes(client, folder, manifest)
    print("Verified copying the CLI path, desktop and terminal examples, saved changelog, patch source, Undo/Redo, repeated saves, "
          "legacy history and both project formats.")
