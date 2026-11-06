#include "app/scenario_flowchart_model.h"
#include "engine/project.h"
#include <array>

namespace lordly {
ChartEvent missionEndFlow(const Project &project) {
    ChartEvent graph;
    graph.title="Standard scenario closing logic";
    graph.trigger="Mission ends · game-run processing";
    graph.automatic=true;
    // Reviewed US Rev 0 native owners. Project::rom has already normalized
    // authenticated capacity patches; class-data references normalize below.
    // Never present retail conditions as fact for an unknown modified owner.
    struct Owner {int start,end;const char *hash;};
    static const std::array<Owner,5> owners{{
        {0x1b2dc4,0x1b3ed8,"948dcc9359dbc4a437525d368c3535bf86afb3aaa49e43c82f7e571f209bafe4"},
        {0x1b5fe0,0x1b60ac,"83d1395dcbcf4e7e9e62265ef9c9b97cf425dea222698228a3d8b0346a7eec9f"},
        {0x1b6538,0x1b7414,"b5faa1d446ef9e4212ddc98e1507cc09736b662429b75114e79968a236df808a"},
        {0x1b7414,0x1b75dc,"fdf0934f9b83312238bd07b67112c84be2d157a719b385e2c7b51bb637e90abe"},
        {0x1b75dc,0x1b762c,"46393d92c8419a72e8ff4f185d2f0b0650b73d40883c9a10af2e8ef0e259ef18"}
    }};
    try {
        for(const auto &owner:owners) {
            auto bytes=project.rom().bytes().mid(owner.start,owner.end-owner.start);
            if(sha256(bytes)!=owner.hash) {
                if(const auto placed=ClassPlacement::read(project.rom()))
                    bytes=placed->canonicalReferences(bytes,owner.start);
                if(sha256(bytes)!=owner.hash)throw Error("Unrecognized mission-end owner");
            }
        }
    }catch(const std::exception &) {
        graph.steps.append(ChartStep{"This ROM has modified mission-end code; its internal conditions are not decoded.",{}});
        return graph;
    }
    const auto className=[&](int id){return id>0&&id<project.classData().entries().size()
        ?decodeText(project.classData().entries()[id].name):QString("Class %1").arg(id);};
    QStringList gear;
    const auto &angel=project.classData().entries()[36].record;
    for(int slot=0;slot<4;++slot) {
        const int id=u16(angel,46+slot*2);
        if(id)gear<<decodeText(project.itemNameBytes(id));
    }
    QStringList eligible;
    for(int id=1;id<=164;++id)if(u8(project.rom().bytes(),0x8acb32+id-1)&0x10)
        eligible<<className(id)+QString(" [%1]").arg(id);
    const auto add=[&](QString title,bool decision=false,QString detail={}) {
        const int id=graph.steps.size();ChartStep step{title,{},{},decision,{}};
        step.detail=std::move(detail);graph.steps<<step;return id;
    };
    const auto edge=[&](int from,int to,QString label={}){graph.edges.append({from,to,label});};
    const int rewards=add("Settle mission results and rewards");
    const int special=add("Battlefield is internal slot 54",true,"This internal battlefield bypasses transformation selection. Undead HP recovery still runs.");
    const int protection=add("Count eligible fallen characters and record Mastaba’s Barrier protection",false,
        "The first squad containing each character supplies its carried items. Mastaba’s Barrier is not consumed. The native protection mask aliases some roster slots, so it can also protect a character outside that squad.");
    const int recovery=add("For each roster slot 1–94: occupied by a Zombie, Skeleton or Ghost?",true);
    const int heal=add("Restore that character to full HP");
    const int recoveryNext=add("Continue undead recovery through roster slot 94");
    const int bypass=add("Battlefield is internal slot 54",true);
    const int scan=add("For each roster slot 1–99, in order");
    const int fallen=add("Character exists and has 0 HP",true);
    const int allowed=add("Both identity class and current class allow death transformations",true,
        "Both class eligibility bytes must include the transformation flag (0x10). Eligible classes in this ROM: "+eligible.join(", "));
    const int leader=add("Character is a squad leader",true);
    const int female=add("Character has a female body type",true,"The native test is body code != 0; retail uses 0 for male and 1 for female.");
    const int equipment=add("Unequipped spares exist for every "+className(36)+" default item",true,
        "Current default equipment: "+(gear.isEmpty()?QString("None"):gear.join(", "))+". Each nonzero item needs an inventory entry with owned quantity greater than equipped quantity. Gear worn by this character does not count as a spare.");
    const int intelligence=add("Permanent Intelligence ≥ 60",true);
    const int mentality=add("Permanent Mentality ≥ 61",true);
    const int dexterity=add("Permanent Dexterity ≥ 57",true);
    const int alignment=add("Alignment is 50–100",true);
    const int angelRoll=add("Angel Knight roll passes: random remainder 0–399 < Luck",true,
        "Uses a fresh native random output modulo 400. Approximately Luck ÷ 4 percent; Luck 40 is about 10%. No level requirement.");
    const int angelResult=add("Change current class to "+className(36));
    const int zombieRoll=add("Zombie roll passes: random remainder 0–9 < 3",true,
        "Approximately 30%. Uses a fresh random output, including after a failed Angel Knight roll. This roll occurs before checking Barrier protection.");
    const int barrier=add("Character is protected by Mastaba’s Barrier",true,
        "Uses the protection mask recorded earlier. This blocks Zombie creation only; it does not block Angel Knights.");
    const int zombieResult=add("Change current class to Zombie for this body type",false,
        "Target class is 45 + body code: "+className(45)+" [45] or "+className(46)+" [46] for retail body types.");
    const int identity=add("Both old classes allow replacing the identity class",true,"Both old class eligibility bytes must also include 0x20. Named identities normally keep their original identity.");
    const int replaceIdentity=add("Set identity class to the new class");
    const int restore=add("Restore full HP and clear the four equipment overrides",false,
        "Keeps name, level, experience, maximum HP, attributes, Luck, Alignment and roster membership.");
    const int inventory=add("Update old equipment and equip the new defaults",false,
        "Old equipped counts decrease; items whose tier byte is zero also lose an owned copy. Angel Knights reserve the spare defaults. Zombies receive Old Clothing when the inventory can hold it; transformation still proceeds if the clothing cannot be added.");
    const int next=add("Move to the next roster slot");
    const int more=add("More roster slots remain, through slot 99",true);
    const int finish=add("Finish result processing, then run Mission completion events");
    graph.sections={{"Rewards and preparation",rewards},{"Undead HP recovery",recovery},{"Angel Knight and Zombie checks",bypass}};
    edge(rewards,special);edge(special,recovery,"Yes");edge(special,protection,"No");edge(protection,recovery);
    edge(recovery,heal,"Yes");edge(recovery,recoveryNext,"No");edge(heal,recoveryNext);
    edge(recoveryNext,recovery,"Next slot");edge(recoveryNext,bypass,"After slot 94");
    edge(bypass,finish,"Yes");edge(bypass,scan,"No");edge(scan,fallen);
    edge(fallen,allowed,"Yes");edge(fallen,next,"No");edge(allowed,leader,"Yes");edge(allowed,next,"No");
    edge(leader,next,"Yes");edge(leader,female,"No");edge(female,equipment,"Yes");edge(female,zombieRoll,"No");
    edge(equipment,intelligence,"Yes");edge(equipment,zombieRoll,"No");
    edge(intelligence,mentality,"Yes");edge(intelligence,zombieRoll,"No");
    edge(mentality,dexterity,"Yes");edge(mentality,zombieRoll,"No");
    edge(dexterity,alignment,"Yes");edge(dexterity,zombieRoll,"No");
    edge(alignment,angelRoll,"Yes");edge(alignment,zombieRoll,"No");
    edge(angelRoll,angelResult,"Yes");edge(angelRoll,zombieRoll,"No");edge(angelResult,identity);
    edge(zombieRoll,barrier,"Yes");edge(zombieRoll,next,"No");edge(barrier,next,"Yes");edge(barrier,zombieResult,"No");edge(zombieResult,identity);
    edge(identity,replaceIdentity,"Yes");edge(identity,restore,"No");edge(replaceIdentity,restore);edge(restore,inventory);edge(inventory,next);
    edge(next,more);edge(more,fallen,"Yes");edge(more,finish,"No");
    return graph;
}
}
