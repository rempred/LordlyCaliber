"""Build a two-function mod with the portable CLI. Run with --help for the three required paths."""
import argparse
import json
import re
import subprocess
from pathlib import Path


def author(cli, rom, destination):
    destination.mkdir(parents=True, exist_ok=False)
    def run(ops, *options, input_rom=rom):
        proc = subprocess.run([str(cli), "--rom", str(input_rom), "--commands", "-", *map(str, options)],
                              input=json.dumps(ops), encoding="utf8", capture_output=True, timeout=240)
        if proc.returncode:
            raise RuntimeError(proc.stderr)
        return json.loads(proc.stdout)["results"]

    # Begin with a gameplay term from the guide. Every target, address and digest
    # below comes from this installed release and the ROM the user supplied.
    hits, = run([{"op":"lookup","query":"actor identity"}])
    note = next(h for h in hits if "interchangeable identities" in h["text"])
    predicate = re.search(r"`(func_[0-9a-fA-F]+)`",note["text"]).group(1)
    relation, = run([{"op":"symbol","name":predicate}])
    original, = run([{"op":"read","path":relation["owner"]["source"]}])
    # The matching source tests exact equality first, then five adjacent ID groups.
    groups = [(int(base,16),int(count)) for base,count in re.findall(r"\(b - (0x[0-9A-Fa-f]+)\) < (\d+)",original)]
    if groups != [(0x51,3),(0x54,2),(0x56,2),(0x5f,2),(0x63,2)]:
        raise ValueError("This release has a different actor identity predicate")
    groups[1:3] = [(0x54,4)]
    body = ["andi $a0,$a0,255","andi $a1,$a1,255","beq $a0,$a1,matched","nop"]
    for start, count in groups:
        body.extend([f"addiu $t0,$a0,-{start}",f"sltiu $t0,$t0,{count}",f"addiu $t1,$a1,-{start}",
                     f"sltiu $t1,$t1,{count}","and $t0,$t0,$t1","bne $t0,$zero,matched","nop"])
    body.extend(["jr $ra","addu $v0,$zero,$zero","matched:","jr $ra","addiu $v0,$zero,1"])
    predicate_patch = dict(id="actor_identity_groups",owner=predicate,language="asm",source="\n".join(body)+"\n")

    # Locate a caller by its matching source. A symbol binding can also be a
    # data reference; inspect the source before relying on a calling convention.
    needle="for (i = 0; i < 100; i++)"
    candidates=[]
    for ref in relation["referencedBy"]:
        source, = run([{"op":"read","path":ref["source"]}])
        if needle in source and predicate+"(" in source and len(ref["targets"])==1:
            candidates.append((ref["targets"][0],source))
    if len(candidates)!=1:
        raise ValueError("The expected roster loop was not uniquely identified")
    caller, source = candidates[0]
    caller_patch = dict(id="roster_scan_limit",owner=caller,language="c",
                        source=source.replace(needle,"for (i = 0; i < 50; i++)"))
    project=destination/"Actor identity.lcproject"
    output=destination/rom.name
    run([{"op":"patch.put","patch":p} for p in (predicate_patch,caller_patch)],
        "--save-project",project,"--output",output,"--build-dir",destination/"build")

    caller_info, = run([{"op":"symbol","name":caller}])
    def listing(owner):
        part=owner["parts"][0]
        op=dict(op="assembly",offset=part["start"],address=part["address"],size=part["size"])
        return run([op],input_rom=output)[0]
    instructions=listing(caller_info["owner"])
    target=relation["owner"]["parts"][0]["address"]
    assert any(row["instruction"]==f"jal 0x{target:08x}" for row in instructions)
    assert any("slti" in row["instruction"] and row["instruction"].endswith(", 50") for row in instructions)
    installed = listing(relation["owner"])
    assert installed[0]["instruction"].startswith("andi $a0")
    assert any(row["instruction"].startswith("bne ") for row in installed)
    print(f"{project}\n{output}\n{caller} calls {predicate} at RAM 0x{target:08x}.")
    return project, output, installed, groups


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("cli","rom","destination"):
        parser.add_argument("--"+name,required=True,type=Path)
    opts=parser.parse_args()
    author(opts.cli.resolve(),opts.rom.resolve(),opts.destination.resolve())
