#include "app/scenario_flowchart_model.h"
#include "app/event_tree_dialog.h"
#include "engine/event_flag_links.h"
#include "engine/project.h"
#include "engine/retail_event_gate.h"
#include <algorithm>

namespace lordly {
QString storyConditionText(const StoryCondition &c) {
    if (c.children.isEmpty()) {
        if(c.title=="Visiting squad"){
            QStringList states;
            for(const auto &f:c.fields)if(f.value>=0&&f.choices.contains(f.value))states<<f.choices[f.value].toLower();
            return states.isEmpty()?"Any visiting squad":"Visiting squad is "+states.join(" and ");
        }
        if(c.title=="Check squad member"){
            QString member="Squad member";QStringList states;
            for(const auto &f:c.fields){
                if(f.label=="Member")member=f.choices.value(f.value,member);
                if(f.label=="HP"&&f.value>=0)states<<(f.value?"has HP above zero":"has zero HP");
                if(f.label=="Petrification"&&f.value>=0)states<<(f.value?"is petrified":"is not petrified");
            }
            return states.isEmpty()?member:member+" "+states.join(" and ");
        }
        return c.title;
    }
    QStringList parts;
    for (const auto &child : c.children) parts << storyConditionText(child);
    if (c.operation == "not") return "NOT (" + parts.join(" AND ") + ")";
    return "(" + parts.join(c.operation == "any" ? " OR " : " AND ") + ")";
}
namespace {
void flagFields(ChartStep &step, const QVector<StoryField> &fields, const QString &use) {
    for (const auto &f : fields) if (f.kind == "flag" || f.label == "Flag") {
        const int id = qAbs(qint16(f.value));
        if (std::none_of(step.flags.cbegin(),step.flags.cend(),[&](const auto &v){return v.id==id && v.use==use;}))
            step.flags.append({id,use});
    }
}
void conditionFlags(ChartStep &step, const StoryCondition &c) {
    flagFields(step,c.fields,"Checks");
    for (const auto &child : c.children) conditionFlags(step,child);
}
QString actionTitle(const StoryAction &a) {
    if (a.condition.operation.isEmpty()) return a.title;
    return storyConditionText(a.condition);
}
ChartEvent nativeEvent(const StoryEvent &e, int row, const QByteArray &bytes,
                       const QHash<int,QString> &targets) {
    ChartEvent graph;
    graph.title=e.title; graph.trigger=e.trigger; graph.address={row,e.entry};graph.group=e.group;
    auto actions=e.requirements + e.actions;
    // A target guard is shown in WHEN by the compact editor. It can be in the
    // middle of a retail handler (Cayes checks the visitor before the town).
    // Restore it at its actual position before resolving physical successors.
    if(!e.triggerInstructions.isEmpty()){
        StoryAction gate;gate.kind="trigger";gate.title=e.trigger;gate.instructions=e.triggerInstructions;
        for(const auto &f:e.triggerFields)gate.title=f.label+" is "+f.choices.value(f.value,QString::number(f.value));
        actions<<gate;
    }
    std::sort(actions.begin(),actions.end(),[](const auto &a,const auto &b){return a.instructions.first()<b.instructions.first();});
    QHash<int,int> owners;
    for (int i=0;i<actions.size();++i) {
        const auto &a=actions[i];
        ChartStep step{actionTitle(a),{row,a.instructions.first()}};
        step.decision=a.otherwise>=0 || a.kind=="requirement" || a.kind=="condition" || (a.kind=="trigger"&&!e.triggerFields.isEmpty());
        const int last=a.instructions.last();
        if(a.target>=0&&a.otherwise>=0&&RetailEventGate::supports(bytes,row,last)){
            const int taken=last+2+qint8(u8(bytes,last+1))*2,fallthrough=last+2;
            if((a.target==taken&&a.otherwise==fallthrough)||(a.target==fallthrough&&a.otherwise==taken)){
                step.address.gate=last;step.address.gateReversed=a.target!=taken;
            }
        }else if((a.kind=="requirement"||a.kind=="trigger")&&u8(bytes,last)==0x2f){
            int comparisons=0,pc=-1;
            for(int at:a.instructions)if(RetailEventGate::supports(bytes,row,at)){++comparisons;pc=at;}
            if(comparisons==1&&pc==last-2&&qint8(u8(bytes,pc+1))==1)step.address.gate=pc;
        }
        QString use="Checks";
        for (int pc : a.instructions) {
            owners[pc]=i;
            if ((u8(bytes,pc)&0xf8)==0x70 && (u8(bytes,pc+1)>>3)==12) {
                use="Sets or clears";
                for(const auto &f:a.fields) if(f.label=="State") use=f.value?"Sets ON":"Clears OFF";
            }
        }
        flagFields(step,a.fields,use); conditionFlags(step,a.condition);step.instructions=a.instructions;
        graph.steps<<step;
    }
    const auto destination=[&](int at){
        if (!owners.contains(at)) {
            owners[at]=graph.steps.size();
            graph.steps.append({targets.value(at,QString("Event instruction %1").arg(at)),{row,at}});
        }
        return owners[at];
    };
    int skipped=-1;
    for(int i=0;i<actions.size();++i) {
        const auto &a=actions[i];
        const int pc=a.instructions.last(), op=u8(bytes,pc);
        if(a.target>=0) {
            graph.edges.append({i,destination(a.target),a.otherwise>=0?"Yes":op==3?"Call":QString{}});
            if(a.otherwise>=0) graph.edges.append({i,destination(a.otherwise),"No"});
            if(op!=3 || a.otherwise>=0) continue;
        }
        const bool guard=a.kind=="requirement" || (a.kind=="trigger"&&!e.triggerFields.isEmpty()) || (a.kind=="condition" && a.otherwise<0 && op==0x2f);
        if(guard) {
            if(skipped<0) {skipped=graph.steps.size(); graph.steps.append(ChartStep{"Skip this event",{}});}
            graph.edges.append({i,skipped,"No"});
        } else if(a.kind!="trigger"&&(a.kind=="end" || a.kind=="program" || op==4 || op==0x11 || op==0x12 || op==0x2f || op==1 || op==2)) continue;
        if(owners.contains(pc+2) || targets.contains(pc+2))
            graph.edges.append({i,destination(pc+2),guard?"Yes":op==3?"On return":QString{}});
    }
    return graph;
}
// Compile the authored visual branches from their saved structure. Paths also
// address the corresponding editable node in the existing authoring dialog.
struct AuthoredGraph {
    ChartEvent &graph;
    const SceneEvents &flags;
    const Project &project;
    struct Tail {int step;QString label;};
    using Tails=QVector<Tail>;
    int append(const Tails &incoming, ChartStep step) {
        const int id=graph.steps.size(); graph.steps<<step;
        for(const auto &tail:incoming) graph.edges.append({tail.step,id,tail.label});
        return id;
    }
    QPair<Tails,Tails> condition(QJsonObject c, QVector<int> path, Tails incoming) {
        const auto type=c["type"].toString();
        if(type=="not") {
            auto branches=condition(c["condition"].toObject(),path+QVector<int>{0},incoming);
            return {branches.second,branches.first};
        }
        if(type=="all"||type=="any") {
            Tails bypass;
            const auto children=c["conditions"].toArray();
            for(int i=0;i<children.size();++i) {
                auto branches=condition(children[i].toObject(),path+QVector<int>{i},incoming);
                if(type=="all"){incoming=branches.first;bypass+=branches.second;}
                else {incoming=branches.second;bypass+=branches.first;}
            }
            return type=="all"?qMakePair(incoming,bypass):qMakePair(bypass,incoming);
        }
        auto address=graph.address;address.path=path;
        ChartStep step{eventConditionSummary(c,address.row,&flags),address,{},true};
        if(type=="flag")step.flags.append({c["flag"].toInt(),"Checks"});
        const int id=append(incoming,step);
        return {Tails{{id,"Yes"}},Tails{{id,"No"}}};
    }
    Tails body(QJsonArray nodes,QVector<int> parent,Tails incoming) {
        for(int i=0;i<nodes.size();++i) {
            const auto node=nodes[i].toObject();auto address=graph.address;
            address.path=parent+QVector<int>{i};
            const auto type=node["type"].toString();
            if(type=="if"){
                auto branches=condition(node["condition"].toObject(),address.path+QVector<int>{0},incoming);
                auto yes=body(node["then"].toArray(),address.path+QVector<int>{1},branches.first);
                auto no=body(node["else"].toArray(),address.path+QVector<int>{2},branches.second);
                incoming=yes+no;
            }else {
                ChartStep step{eventTreeSummary(node,address.row,&flags,&project),address};
                if(type=="flag")step.flags.append({node["flag"].toInt(),node["value"].toInt(1)?"Sets ON":"Clears OFF"});
                const int id=append(incoming,step);
                incoming={{id,{}}};
                if(type=="program"||type=="stop"||type=="continue"||type=="return"||
                   type=="startBattlefield"||type=="completeMission"||type=="returnToMap"||type=="battleResults")incoming.clear();
            }
        }
        return incoming;
    }
};
void applyGates(ChartEvent &graph,const QJsonObject &gates,const SceneEvents &flags){
    for(int i=0;i<graph.steps.size();++i){
        const auto address=graph.steps[i].address;
        if(address.gate<0||!gates.contains(QString::number(address.gate)))continue;
        const auto gate=gates[QString::number(address.gate)].toObject();
        int yes=-1,no=-1;
        for(const auto &edge:graph.edges)if(edge.from==i){if(edge.label=="Yes")yes=edge.to;if(edge.label=="No")no=edge.to;}
        if(yes<0||no<0)continue;
        if(address.gateReversed)std::swap(yes,no);
        if(gate["mode"]=="keep"){
            graph.steps[i].decision=false;
            const bool semanticYes=(gate["outcome"]=="yes")!=address.gateReversed;
            graph.steps[i].title="Continue with the "+QString(semanticYes?"Yes":"No")+" branch";
            graph.steps[i].address.gateControl=true;
            graph.steps[i].flags.clear();
            graph.edges.removeIf([&](const auto &edge){return edge.from==i;});
            graph.edges.append({i,gate["outcome"]=="yes"?yes:no,{}});
            continue;
        }
        if(gate["mode"]!="if")continue;
        auto added=address;added.gateControl=true;
        ChartStep step{eventConditionSummary(gate["condition"].toObject(),address.row,&flags),added,{},true};
        std::function<void(QJsonObject)> readFlags=[&](QJsonObject condition){
            const auto type=condition["type"].toString();
            if(type=="flag")step.flags.append({condition["flag"].toInt(),"Checks"});
            else if(type=="not")readFlags(condition["condition"].toObject());
            else for(auto child:condition["conditions"].toArray())readFlags(child.toObject());
        };
        readFlags(gate["condition"].toObject());
        for(auto &edge:graph.edges){
            if(edge.from>=i)++edge.from;
            if(edge.to>i)++edge.to;
        }
        if(yes>i)++yes;if(no>i)++no;
        graph.steps.insert(i,step);
        auto destination=[&](QString outcome){return outcome=="original"?i+1:outcome=="yes"?yes:no;};
        graph.edges.append({i,destination(gate["yes"].toString()),"Yes"});
        graph.edges.append({i,destination(gate["no"].toString()),"No"});
        if(gate["yes"]!="original"&&gate["no"]!="original"){
            graph.edges.removeIf([&](const auto &edge){return edge.from==i+1;});
            graph.steps.removeAt(i+1);
            for(auto &edge:graph.edges){if(edge.from>i+1)--edge.from;if(edge.to>i+1)--edge.to;}
        }else ++i;
    }
}
}
QVector<ChartEvent> scenarioChart(Project &project,const QVector<QPair<QString,EventProgramChoice>> &choices) {
    QVector<ChartEvent> result;
    project.loadCampaignMap();
    const auto &events=*project.sceneEvents();
    const bool original=!project.scenePrograms()->modified()&&(!project.dialogues()||!project.dialogues()->modified())&&
        sha256(project.runtimeRom().bytes())=="571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a";
    QSet<QPair<int,int>> seen;
    QSet<int> shownMissionEnd;
    std::optional<ChartEvent> missionEnd;
    for(const auto &[phase,choice]:choices) {
        if(seen.contains({choice.row,choice.entry}))continue;
        seen.insert({choice.row,choice.entry});
        const auto flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),choice.row,original,false,&*project.campaignMap());
        const auto bytes=SceneEvents::flagRow(SceneEvents::retailRow(project.rom(),choice.row),choice.row,events.flagEdits(),false);
        QHash<int,QVector<ChartFlag>> uses;
        for(const auto &ref:EventFlagLinks::referencesInProgram(project.rom(),events,*project.scenePrograms(),choice.row)){
            ChartFlag flag{ref.flag,ref.read?"Checks":ref.state<0?"Sets or clears":ref.state?"Sets ON":"Clears OFF"};
            auto &list=uses[ref.offset];
            if(std::none_of(list.cbegin(),list.cend(),[&](const auto &x){return x.id==flag.id&&x.use==flag.use;}))list<<flag;
        }
        const auto trees=events.flagEdits()[QString::number(choice.row)].toObject()["trees"].toArray();
        QHash<int,QString> targets; QHash<int,int> owners;
        for(const auto &e:flow) {
            for(const auto &a:e.requirements+e.actions)for(int pc:a.instructions){targets[pc]=actionTitle(a);owners[pc]=e.entry;}
            for(int pc:e.triggerInstructions){targets[pc]=e.title;owners[pc]=e.entry;}
            targets[e.entry]=e.title;owners[e.entry]=e.entry;
        }
        QSet<int> include{choice.entry};
        // Location dispatch programs share helpers and can prepend edited
        // arrival rules. Follow only this location's reachable handler links.
        if(choice.entry>=0){
            QVector<int> pending{choice.entry};
            while(!pending.isEmpty()){
                const int entry=pending.takeLast();
                for(const auto &e:flow)if(e.entry==entry)for(const auto &a:e.actions)
                    for(int dest:{a.target,a.otherwise})if(owners.contains(dest)&&!include.contains(owners[dest])){
                        include.insert(owners[dest]);pending<<owners[dest];
                    }
            }
        }
        for(const auto &e:flow) {
            if(choice.entry>=0&&!include.contains(e.entry))continue;
            if(e.group==5&&!shownMissionEnd.contains(choice.row)) {
                if(!missionEnd)missionEnd=missionEndFlow(project);
                auto common=*missionEnd;common.section=phase;
                // Stable UI identity only, never an editable event address.
                common.address.offset=choice.row;
                result<<common;shownMissionEnd.insert(choice.row);
            }
            ChartEvent graph;
            if(e.authoredId.isEmpty()){
                graph=nativeEvent(e,choice.row,bytes,targets);
                for(auto &s:graph.steps){
                    QVector<ChartFlag> pcUses;
                    const auto gates=events.flagEdits()[QString::number(choice.row)].toObject()["gates"].toObject();
                    for(int pc:s.instructions)if(!gates.contains(QString::number(pc)))pcUses+=uses.value(pc);
                    if(pcUses.isEmpty())continue;
                    for(const auto &flag:pcUses){
                        s.flags.removeIf([&](const auto &x){return x.id==flag.id&&x.use=="Sets or clears"&&flag.use!="Checks"&&flag.use!="Sets or clears";});
                        if(std::none_of(s.flags.cbegin(),s.flags.cend(),[&](const auto &x){return x.id==flag.id&&x.use==flag.use;}))s.flags<<flag;
                    }
                }
                applyGates(graph,events.flagEdits()[QString::number(choice.row)].toObject()["gates"].toObject(),events);
            }
            else {
                graph.title=e.title;graph.trigger=e.trigger;graph.address={choice.row,e.entry,e.authoredId};graph.group=e.group;
                for(const auto &v:trees)if(v.toObject()["id"]==e.authoredId){
                    AuthoredGraph builder{graph,events,project};
                    const auto tails=builder.body(v.toObject()["nodes"].toArray(),{},{});
                    if(!tails.isEmpty())builder.append(tails,{"Continue with other events",{}});
                    break;
                }
            }
            QStringList selectors;
            if(!e.authoredId.isEmpty())for(const auto &v:trees)if(v.toObject()["id"]==e.authoredId){
                const int target=v.toObject()["target"].toInt(-1);
                if(e.group>=1&&e.group<=4&&target>=0)selectors<<eventTargets(&project,choice.row,e.group).value(target,QString::number(target));
            }
            if(!selectors.isEmpty()){
                for(auto &edge:graph.edges){++edge.from;++edge.to;}
                graph.steps.prepend({selectors.isEmpty()?e.trigger:e.trigger+" · "+selectors.join(", "),graph.address,{},false,e.triggerInstructions});
                if(graph.steps.size()>1)graph.edges.prepend({0,1,{}});
            }
            graph.section=phase;
            if(graph.group==5)graph.trigger="Mission completion · after standard closing logic";
            if(choice.entry<0 && choice.name!=phase)graph.section+=" · "+choice.name;
            result<<std::move(graph);
        }
    }
    return result;
}
}
