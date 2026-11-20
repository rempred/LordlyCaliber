"""Verify the shipped patch example, source navigation and installed behavior through the portable CLI."""
import argparse
import importlib.util
import json
import subprocess
from pathlib import Path
from portable_workspace import Portable


def main(client):
    root=client.cli.parent
    run=lambda ops,*args,**kw:client.commands(ops,*args,**kw)["results"]
    guide=subprocess.run([str(client.cli),"--guide"],capture_output=True,encoding="utf8",
                         env=client.env,cwd=client.work,check=True).stdout
    assert "agent-patch-example.py" in guide
    sources,=run([{"op":"files","query":"reference/editor/app/","content":"attackPower"}])
    file=next(f for f in sources if f.endswith("/attack_editor.cpp"))
    matches,=run([{"op":"find","path":file,"query":"powerPercent"}])
    start=matches[0]["line"]
    excerpt,=run([{"op":"read","path":file,"line":start,"count":3}])
    assert "powerPercent" in excerpt["text"] and excerpt["line"]==start
    engine,builders=run([{"op":"files","query":"reference/editor/engine/","content":"powerPercent"},
                         {"op":"files","query":"reference/editor/rebuild/","content":"p.attacks()->build("}])
    assert engine and builders
    example,=run([{"op":"read","path":"reference/agent-patch-example.py"}])
    script=client.work/"example.py"
    script.write_text(example,encoding="utf8")
    spec=importlib.util.spec_from_file_location("example",script)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    import os
    old=os.environ.copy()
    try:
        os.environ.clear()
        os.environ.update(client.env)
        project,out,listing,groups=module.author(client.cli,client.rom,client.work/"actor")
    finally:
        os.environ.clear()
        os.environ.update(old)

    # Execute the installed leaf's integer and branch instructions with the
    # VR4300 delay slot. Compare every possible pair of byte-sized actor IDs.
    words=[row["word"] for row in listing]
    def predicate(a,b):
        regs=[0]*32
        regs[4],regs[5],regs[31]=a,b,0xffffffff
        pc=0
        pending=None
        for _ in range(len(words)):
            w=words[pc]; following=pc+1
            op=w>>26;rs=(w>>21)&31;rt=(w>>16)&31;rd=(w>>11)&31
            imm=w&65535; signed=imm-65536 if imm>=32768 else imm
            jump=None
            if not w: pass
            elif op in (4,5):
                take=(regs[rs]==regs[rt])==(op==4)
                if take: jump=following+signed
            elif op==12: regs[rt]=regs[rs]&imm
            elif op==9: regs[rt]=(regs[rs]+signed)&0xffffffff
            elif op==11: regs[rt]=int(regs[rs]<(signed&0xffffffff))
            elif op==0 and (w&63)==36: regs[rd]=regs[rs]&regs[rt]
            elif op==0 and (w&63)==33: regs[rd]=(regs[rs]+regs[rt])&0xffffffff
            elif op==0 and (w&63)==8 and rs==31: jump=-1
            else: raise AssertionError(hex(w))
            regs[0]=0
            if pending==-1: return regs[2]
            pc=following if pending is None else pending
            pending=jump
        raise AssertionError("Actor lookup did not return")
    for a in range(256):
        for b in range(256):
            expected=a==b or any(start<=a<start+count and start<=b<start+count for start,count in groups)
            assert predicate(a,b)==expected,(a,b)
    reopened=client.work/"reopened"/client.rom.name
    reopened.parent.mkdir()
    report=client.commands([], "--project",project,"--output",reopened,"--build-plan")
    assert reopened.read_bytes()==out.read_bytes()
    steps={s["name"]:s for s in report["steps"]}
    for name in ("user-roster_scan_limit","user-actor_identity_groups"):
        assert steps[name]["declaredRanges"] and steps[name]["writtenRanges"]
    baseline=client.work/"disabled"/client.rom.name
    baseline.parent.mkdir()
    run([{"op":"patch.enable","id":key,"enabled":False} for key in ("actor_identity_groups","roster_scan_limit")],
        "--project",project,"--output",baseline)
    assert baseline.read_bytes()==client.rom.read_bytes()
    print("Verified source discovery, all 65,536 actor pairs, installed C call, ownership, reproducibility and disabled baseline.")


if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("cli","rom","work"):
        p.add_argument("--"+name,type=Path,required=True)
    args=p.parse_args()
    main(Portable(args.cli,args.rom,args.work))
