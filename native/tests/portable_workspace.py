"""Black-box portable workspace audit. Only the package, input ROM and a fresh output directory are needed."""
import argparse
import base64
import hashlib
import json
import os
import queue
import subprocess
import threading
from pathlib import Path


class Portable:
    def __init__(self, cli, rom, work):
        self.cli, self.rom, self.work = cli.resolve(), rom.resolve(), work.resolve()
        self.env = {k: v for k, v in os.environ.items()
                    if not (k.startswith(("QT_", "LORDLY_")) or k in {"QTDIR", "PATH"})}
        self.env["PATH"] = os.path.join(os.environ["SystemRoot"], "System32")
        self.env["LORDLY_COMPILER_CACHE_DIR"] = str(self.work/"compile-cache")
        self.work.mkdir(parents=True, exist_ok=True)

    def commands(self, ops, *extra, rom=None):
        args = [str(self.cli), "--rom", str(rom or self.rom), "--commands", "-", *map(str, extra)]
        run = subprocess.run(args, input=json.dumps(ops), encoding="utf8", capture_output=True,
                             env=self.env, cwd=self.work, timeout=300)
        if run.returncode:
            raise RuntimeError(f"CLI exited with code {run.returncode}: {run.stderr[-2400:]}")
        return json.loads(run.stdout)

    def workspace(self, *extra, rom=None):
        return Session(self, rom or self.rom, extra)


class Session:
    def __init__(self, client, rom, extra):
        self.log = (client.work / "desktop.log").open("a", encoding="utf8")
        self.p = subprocess.Popen([str(client.cli), "--rom", str(rom), *map(str, extra),
                                  "--settings-dir",str(client.work/"preferences"),"--workspace", "-"],
                                  cwd=client.work, env=client.env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=self.log, text=True, encoding="utf8")
        self.lines = queue.Queue()
        def reader():
            for line in self.p.stdout:
                self.lines.put(line)
            self.lines.put("")
        threading.Thread(target=reader, daemon=True).start()
        assert self.response()["ready"]

    def response(self):
        line = self.lines.get(timeout=90)
        if not line:
            raise RuntimeError(f"Workspace ended, exit {self.p.poll()}")
        return json.loads(line)

    def call(self, op, **kwargs):
        cmd = dict(op=op, **kwargs)
        self.p.stdin.write(json.dumps(cmd)+"\n")
        self.p.stdin.flush()
        r = self.response()
        if not r["ok"]:
            raise RuntimeError(f"{cmd}: {r}")
        if op == "save" and r.get("result") != kwargs["path"]:
            dialog = self.call("controls")
            messages = [c.get("text") for c in dialog["controls"] if c["type"] == "QLabel"]
            raise RuntimeError(f"Save did not finish: {dialog['window']}: {messages}")
        return r.get("result")

    def page(self, *path):
        return self.call("openPage", path=path)

    def button(self, text):
        cs = self.call("controls", query=text)["controls"]
        b = next(c for c in cs if c["type"] in ("QPushButton", "QToolButton") and
                 c.get("enabled") and c.get("text", "").replace("&", "") == text)
        return self.call("activate", id=b["id"])

    def file(self, path):
        self.call("file", path=str(path))

    def field(self, name):
        return self.call("inspect", name=name)

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.p.stdin.close()
        try:
            self.p.wait(timeout=15)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()
        self.log.close()


def audit(client):
    work = client.work
    project = work / "tab-edits.lcproject"
    exported = work / client.rom.name
    if exported.exists() or project.exists():
        raise ValueError("Use a fresh audit directory")
    ops = lambda *ops: client.commands(ops)["results"]
    missions = ops({"op": "missions"})[0]
    choice = missions[0]
    seed = ops({"op": "mission.seed", "choice": choice["choice"]})[0]
    saved = work / "mission.sra"
    saved.write_bytes(base64.b64decode(seed["base64"]))
    assert hashlib.sha256(saved.read_bytes()).hexdigest() == seed["sha256"]
    print("Testing mission seed:", choice["location"], choice["mission"], flush=True)
    with client.workspace() as ui:
        call = ui.call
        ui.page("Save Editor")
        call("activate",name="openSave")
        ui.file(saved)
        ui.page("Campaign","Scenarios","Orders")
        call("set",name="scenarioRouteAdvanced",value=True)
        ui.page("Campaign","Storyline","Cutscenes","Stage")
        call("activate",name="sceneOpenSetup")
        call("set",name="cutsceneRawToggle",value=True)
        call("reject")
        paths = call("pages")
        census = {}
        unavailable = {}
        for row in paths:
            path = row["path"]
            key = " / ".join(path)
            try:
                if path[-1] in ("Arrival","Mission","Area Investigation"):
                    ui.page(*path[:-1],"Triggers and rules")
                    if call("controls",query="scenarioEventOtherPrograms")["controls"]:
                        call("set",name="scenarioEventOtherPrograms",value=False)
                if "Custom squads" in path[:-1]:
                    ui.page(*path[:-1])
                    call("activate",name="customNew")
                ui.page(*path)
            except RuntimeError as e:
                unavailable[key] = str(e)
                continue
            active = [r["path"] for r in call("pages") if r["active"]]
            if path not in active:
                unavailable[key] = "Conditional parent currently hidden"
                continue
            controls = call("controls")["controls"]
            census[key] = [dict(type=c["type"], name=c["name"], label=c.get("label", ""),
                                text=c.get("text", "")[:90], enabled=c.get("enabled"))
                           for c in controls if "activate" in c["operations"] or "set" in c["operations"]
                           or "items" in c["operations"]]
        (work / "page-inventory.json").write_text(json.dumps(dict(opened=census, conditional=unavailable),indent=1),encoding="utf8")
        print("Page inventory:", len(census), "opened,", len(unavailable), "conditional", flush=True)
        assert not unavailable, unavailable
        assert set(call("items",name="workspaceNavigation")["items"]) == {path.split(" / ")[0] for path in census}

        def row_edit(page, name, column, value):
            ui.page(page)
            table = call("items", name=name, limit=10)
            col = table["headers"].index(column)
            row = next(r for r in range(10) if call("cell", name=name, index=[r], column=col)["editable"])
            old = call("cell", name=name, index=[row], column=col)["value"]
            assert int(old) != value
            call("edit", name=name, index=[row], column=col, value=value)
            return (page, name, row, col, value)

        class_check = row_edit("Classes", "classesTable", "STR base", 302)
        item_check = row_edit("Items", "itemsTable", "STR", 8)
        ui.page("Attacks")
        call("select", name="attackList", index=[0])
        call("set", name="attackPower", value=113)
        print("Attacks power:", ui.field("attackPower")["value"], flush=True)

        ui.page("Consumables")
        rows = call("items",name="consumablesTable")["items"]
        leaf = next(i for i, row in enumerate(rows) if "Heal Leaf" in row)
        call("select",name="consumablesTable",index=[leaf])
        call("activate",name="assignConsumableUse")
        call("select",name="assignmentEffect",text="Restore HP")
        call("set",name="assignmentAmount",value=47)
        ui.button("Save")
        settings = next(r["path"] for r in paths if r["path"][0]=="Campaign" and r["path"][-1]=="Settings")
        ui.page(*settings)
        day_length = call("set",name="rule_dayLength",value=117,readBack=True)["value"]
        assert day_length == 125
        tools = next(r["path"] for r in paths if r["path"][-1]=="Battle and squads")
        ui.page(*tools)
        call("set",name="rule_deploymentLimit",value=9)
        tool_edits = []
        for tab, name in [("Status effects","rule_poisonStrength"),("Training","rule_trainingBonus"),
                          ("Mission rewards","rule_gothLowDivisor"),("Movement Types","movementBaseSpeed")]:
            ui.page("Tools",tab)
            state=ui.field(name)
            val=state["value"]+1 if state["value"]<state["maximum"] else state["value"]-1
            call("set",name=name,value=val)
            tool_edits.append((tab,name,val))
        ui.page("Tools","Display and options")
        option="chaosCounter"
        toggled=not ui.field(option)["checked"]
        call("set",name=option,value=toggled)
        ui.page("Campaign","World Map")
        node="campaignNodeX"
        state=ui.field(node)
        node_x=state["value"]+1 if state["value"]<state["maximum"] else state["value"]-1
        call("set",name=node,value=node_x)
        call("activate",name="campaignApplyNode")
        relative=("Campaign","Scenarios","Neutral encounters","Global rates")
        ui.page(*relative)
        divisor=ui.field("encounterDivisor")["value"]+1
        call("set",name="encounterDivisor",value=divisor)
        call("activate",name="encounterApplyGlobal")
        dialogue=("Campaign","Storyline","Cutscenes","Dialogue","Source")
        ui.page(*dialogue)
        call("activate",name="dialogueAdd")
        entry=call("items",name="dialogueEntries")["count"]-1
        call("set",name="dialogueSource",value="Portable audit.")
        assert "Portable audit." in ui.field("dialogueSource")["text"]
        art = next(r["path"] for r in paths if r["path"][-1]=="Item icons")
        ui.page(*art)
        call("select",name="artIcons",index=[0])
        call("select",name="artPixelTool",text="Eraser")
        canvas = ui.field("artIconCanvas")
        y=canvas["height"]//2
        for event,x in [("press",2),("move",canvas["width"]-2),("release",canvas["width"]-2)]:
            call("pointer",id=canvas["id"],event=event,x=x,y=y)
        call("activate",name="artCopyToLibrary")
        ui.page("Art Library","Artwork")
        call("set",name="spriteName",value="Audit icon")
        call("key",name="spriteName",key="Return")
        call("activate",name="spriteDuplicateFrame")
        print("Art Library:",ui.field("spriteName")["text"],flush=True)
        call("save",path=str(project))
        # A battery save is an independent document, including when opened against an edited project.
        ui.page("Save Editor")
        save_paths = call("pages")
        save_page=next(r["path"] for r in save_paths if r["path"][-1]=="Game state")
        ui.page(*save_page)
        game=call("items",name="saveGameState")
        print("Save game:",game["headers"],str(game["items"])[:300],flush=True)
        goth=next(i for i,t in enumerate(game["headers"]) if "goth" in t.lower())
        call("edit",name="saveGameState",index=[0],column=goth,value=12345)
        call("activate",name="exportSave")
        battery=work/"edited.sra"
        ui.file(battery)
        assert battery.exists()
    history, changelog = client.commands([{"op":"agent.edits"},{"op":"changes"}],
                                         "--project",project,"--output",exported)["results"]
    sections = {r["scope"].get("key") for r in history
                if r["scope"]["kind"] == "section" and r["status"] == "Matches project"}
    assert {"attacks", "consumableEffects", "tools", "spriteLibrary"} <= sections, sections
    assert "CLI edits" in changelog and "Consumable effects" in changelog
    with client.workspace("--project",project) as ui:
        ui.page("Art Library","Artwork")
        assets = ui.call("items",name="spriteAssets")["items"]
        ui.call("select",name="spriteAssets",index=[next(i for i,row in enumerate(assets) if "Audit icon" in str(row))])
        assert ui.field("spriteName")["text"]=="Audit icon"
    with client.workspace(rom=exported) as ui:
        for page,name,row,col,val in [class_check,item_check]:
            ui.page(page)
            assert int(ui.call("cell",name=name,index=[row],column=col)["value"])==val
        ui.page("Attacks")
        ui.call("select",name="attackList",index=[0])
        assert ui.field("attackPower")["value"]==113
        ui.page(*settings)
        assert ui.field("rule_dayLength")["value"]==day_length
        ui.page(*tools)
        assert ui.field("rule_deploymentLimit")["value"]==9
        for tab,name,val in tool_edits:
            ui.page("Tools",tab)
            assert ui.field(name)["value"]==val,(tab,name,val,ui.field(name))
        ui.page("Tools","Display and options")
        assert ui.field(option)["checked"]==toggled
        ui.page("Campaign","World Map")
        assert ui.field(node)["value"]==node_x
        ui.page(*relative)
        assert ui.field("encounterDivisor")["value"]==divisor
        ui.page(*dialogue)
        ui.call("select",name="dialogueEntries",index=[entry])
        assert "Portable audit." in ui.field("dialogueSource")["text"]
        ui.page("Save Editor")
        ui.call("activate",name="openSave");ui.file(battery);ui.page(*save_page)
        assert int(ui.call("cell",name="saveGameState",index=[0],column=goth)["value"])==12345
    icon=dict(op="icon",table="items",id=1)
    leaf_read=dict(op="domain",name="consumableEffects")
    actual=client.commands([icon,leaf_read],rom=exported)["results"]
    before=client.commands([icon])["results"][0]
    assert actual[0]["sha256"] != before["sha256"]
    assert actual[1]["trees"]["1"] == [dict(type="heal",minimum=47,maximum=47)]
    # Desktop loaders may retain original data as a baseline and only serialize subsequent deltas.
    with client.workspace(rom=exported) as ui:
        ui.page("Consumables")
        ui.call("select",name="consumablesTable",index=[leaf])
        ui.call("activate",name="assignConsumableUse")
        assert "47 HP" in ui.field("assignmentEffect")["text"]
        ui.call("reject")
    print("Passed all ten workspace output families.", flush=True)


def artwork_audit(client):
    """Compare the GUI's PNG output before and after an edit and after ROM reopen."""
    cases = [
        ("Class category icons","categoryIconExport","categoryIconTool","categoryIconCanvas"),
        ("Portraits","portraitExport","portraitTool","portraitCanvas"),
        ("Army sprites","armyExport","armyTool","armyCanvas"),
        ("Advanced animations","combatExport","combatTool","combatCanvas"),
        ("Attack names","captionExport",None,"attackCaptionCanvas"),
    ]
    directory = client.work
    project = directory/"artwork.lcproject"
    rom = directory/client.rom.name
    if project.exists() or rom.exists():
        raise ValueError("Use a fresh artwork audit directory")
    def picture(ui, button, path):
        ui.call("activate",name=button)
        ui.file(path)
        data=path.read_bytes()
        assert data.startswith(b"\x89PNG"), path
        return hashlib.sha256(data).hexdigest()
    snapshots = {}
    with client.workspace() as ui:
        paths=ui.call("pages")
        for title,button,tool,canvas in cases:
            path=next(r["path"] for r in paths if r["path"][0]=="Art and Animation" and r["path"][-1]==title)
            ui.page(*path)
            initial=picture(ui,button,directory/(button+"-before.png"))
            if tool:
                choices=ui.call("items",name=tool)["items"]
                erase=next((t for t in choices if t.lower().startswith(("eras","clear"))),"Pencil")
                ui.call("select",name=tool,text=erase)
            else:
                ui.call("activate",name="captionGenerate")
                ui.call("select",name="captionColor",text="Transparent")
            c=ui.field(canvas)
            for i in (2,1,3):
                y=c["height"]*i//4
                for event,x in (("press",1),("move",c["width"]-2),("release",c["width"]-2)):
                    ui.call("pointer",id=c["id"],event=event,x=x,y=y)
            changed=picture(ui,button,directory/(button+"-edited.png"))
            assert initial!=changed,(title,"image did not change")
            snapshots[button]=(path,changed)
            print(title, "edited", flush=True)
        ui.call("save",path=str(project))
    history = client.commands([{"op":"agent.edits"}],"--project",project,"--output",rom)["results"][0]
    sections = {r["scope"].get("key") for r in history if r["status"] == "Matches project"}
    assert {"categoryIcons","avatarArtwork","armyArtwork","combatArtwork"} <= sections, sections
    with client.workspace(rom=rom) as ui:
        for button,(path,expected) in snapshots.items():
            ui.page(*path)
            actual=picture(ui,button,directory/(button+"-reopened.png"))
            assert actual==expected,(button,actual,expected)
            print(path[-1], "exported PNG matches ROM reopen", flush=True)


def interaction_audit(client):
    """Use native input, clipboard, menu operations and a multi-file GUI import."""
    work = client.work
    project = work/"interactions.lcproject"
    rom = work/client.rom.name
    if project.exists() or rom.exists():
        raise ValueError("Use a fresh interaction audit directory")
    frames=client.commands([dict(op="use.animation",id=6)])["results"][0]
    paths=[]
    for i,frame in enumerate(frames):
        path=work/f"Effect frame {i+1}.png"
        path.write_bytes(base64.b64decode(frame["base64"]))
        paths.append(str(path))
    tree_op=dict(op="domain",name="consumableEffects")
    effects=client.commands([tree_op])["results"][0]
    effects["version"]=max(2,effects["version"])
    step={"type":"heal","minimum":37,"maximum":37}
    effects.setdefault("trees",{})["1"]=[{"type":"if","condition":{"type":"class","value":5},
                                          "then":[step],"else":[]}]
    initial_project=work/"branch.lcproject"
    client.commands([dict(op="domain.set",name="consumableEffects",value=effects)],"--save-project",initial_project)
    with client.workspace("--project",initial_project) as ui:
        call=ui.call
        roots=call("windows")
        call("useWindow",id=next(w["id"] for w in roots if w["main"]))
        menu=next(c for c in call("controls")["controls"] if c["type"]=="QMenuBar")
        actions=call("menuItems",id=menu["id"])
        file=next(a for a in actions if a["text"].replace("&","")=="File")
        call("activate",id=file["id"])
        patches=next(a for a in call("menuItems") if "code patches" in a["text"])
        call("activate",id=patches["id"])
        assert any(w["ownsInput"] and "code patches" in w["title"].lower() for w in call("windows"))
        call("reject")
        call("clipboard",text="Portable")
        assert call("clipboard")=="Portable"
        ui.page("Consumables");call("select",name="consumablesTable",index=[1])
        call("activate",name="editConsumableTree")
        tree=ui.field("itemEffectTree")
        # Root WHEN, its IF, and the THEN and ELSE branch rows.
        for parent in ([0],[0,0],[0,0,0]):
            call("expand",id=tree["id"],index=parent)
        dest=call("cell",id=tree["id"],index=[0,0,1])
        call("transfer",id=dest["viewport"],x=dest["x"],y=dest["y"],
             **{"from":tree["id"],"index":[0,0,0,0],"action":"move"})
        ui.button("OK")
        call("activate",name="editConsumableVisual")
        ui.button("Import sequence…")
        ui.button("Files…")
        call("file",paths=paths)
        assert "Import artwork" not in call("controls")["window"]
        # A four-frame sequence maps directly onto the four game frames.
        if any(c.get("text","").replace("&","")=="OK" for c in call("controls")["controls"]):
            ui.button("OK")
        if call("controls",query="itemVisualName")["controls"]:
            ui.button("OK")
        terrain=("Campaign","Scenarios","Neutral encounters","Map encounters")
        ui.page(*terrain)
        call("activate",name="encounterAdvancedTerrain")
        call("useWindow",id=next(w["id"] for w in call("windows") if "terrain rules" in w["title"].lower()))
        table=ui.field("terrainAssignments")
        palette=ui.field("encounterPoolTiles")
        row=0
        dest=call("cell",id=table["id"],index=[row],column=2)
        before=dest["text"]
        # Assign an available pool to this native terrain and verify the actual row changed.
        for i in range(call("items",id=palette["id"])["count"]):
            call("transfer",id=dest["viewport"],x=dest["x"],y=dest["y"],
                 **{"from":palette["id"],"index":[i],"action":"copy"})
            after=call("cell",name="terrainAssignments",index=[row],column=2)["text"]
            if before!=after: break
        assert before!=after
        call("reject")
        ui.page("Art and Animation","Combat animations")
        view=ui.field("sequenceFrames")
        if call("items",id=view["id"])["count"]<2:
            call("activate",name="sequenceDuplicate")
        def cell(row):
            return call("cell",name="sequenceFrames",index=[row],decoration=True)
        initial=cell(0)
        count=call("items",name="sequenceFrames")["count"]
        destination=next((i for i in range(1,count) if cell(i).get("png")!=initial.get("png")),1)
        target=cell(destination)
        call("select",name="sequenceFrames",index=[0])
        call("select",name="sequenceFrames",index=[destination],extend=True)
        assert cell(0)["selected"] and cell(destination)["selected"]
        call("select",name="sequenceFrames",index=[0])
        target=cell(destination)
        call("transfer",id=target["viewport"],x=target["x"]+4,y=target["y"],
             **{"from":ui.field("sequenceFrames")["id"],"index":[0],"action":"move"})
        assert cell(destination).get("png")==initial.get("png")
        call("save",path=str(project))
    result=client.commands([dict(op="manifest"),dict(op="use.animation",id=1)],"--project",project,"--output",rom)["results"]
    assert any("animation" in k.lower() for k in result[0])
    assert "consumableEffects" in result[0]
    branch=client.commands([tree_op],rom=rom)["results"][0]["trees"]["1"][0]
    assert not branch["then"] and branch["else"]==[step]
    newframes=client.commands([dict(op="use.animation",id=1)],rom=rom)["results"][0]
    assert [f["sha256"] for f in newframes]==[f["sha256"] for f in result[1]]
    retail=client.commands([dict(op="use.animation",id=1)])["results"][0]
    assert [f["sha256"] for f in newframes]!=[f["sha256"] for f in retail]
    with client.workspace(rom=rom) as ui:
        ui.page(*terrain)
        ui.call("activate",name="encounterAdvancedTerrain")
        ui.call("useWindow",id=next(w["id"] for w in ui.call("windows") if "terrain rules" in w["title"].lower()))
        assert ui.call("cell",name="terrainAssignments",index=[row],column=2)["text"]==after
        ui.call("reject")
        ui.page("Art and Animation","Combat animations")
        assert ui.call("cell",name="sequenceFrames",index=[destination],decoration=True).get("png")==initial.get("png")
    print("Verified menus, dialogs, clipboard, multi-file import, effect-tree moves, terrain pool assignment and frame reordering through ROM reopen.")


if __name__ == "__main__":
    p=argparse.ArgumentParser(description=__doc__)
    for arg in ("cli","rom","work"):
        p.add_argument("--"+arg,type=Path,required=True)
    modes=p.add_mutually_exclusive_group()
    modes.add_argument("--artwork", action="store_true")
    modes.add_argument("--interactions", action="store_true")
    args=p.parse_args()
    run = artwork_audit if args.artwork else interaction_audit if args.interactions else audit
    run(Portable(args.cli,args.rom,args.work))
