// Included inside event_flow.cpp's anonymous namespace, after transfer().
// A second, finite must-analysis tracks the identity of changing indices. The
// ordinary expression lattice deliberately loses exact values at loop joins.
// These labels describe a proved use, never a value or an editable operand.
QMap<int,Registers> loopRegisterInputs(const QByteArray &bytes,const QSet<int> &offsets,const QJsonArray &handlers){
    struct Value {int origin=-1,literal=-1;};
    using State=std::array<Value,8>;
    QMap<int,QVector<int>> edges;
    QMap<int,QSet<int>> incoming;
    QMap<int,int> targets;
    QSet<int> entries;
    for(const auto &value:handlers){const int at=value.toObject()["entry"].toInt();entries.insert(at);incoming[at].insert(-1);}
    for(int at:offsets){
        if(at<0||at+1>=bytes.size())return {};
        const int op=u8(bytes,at),arg=u8(bytes,at+1);
        QVector<int> next;
        if(op==1||op==0x2d||conditional(op)){next<<at+2+qint8(arg)*2;if(op!=1)next<<at+2;}
        else if(op==2||op==3){
            // A static prelude is a proof only if no other edge enters the
            // call/jump word. Validate that after collecting every current edge.
            if(!offsets.contains(at-2)||u8(bytes,at-2)!=0x68)return {};
            const int target=quint16((at+2)/2+(arg<<8)+u8(bytes,at-1))*2;
            targets[at]=target;
            if(op==2)next<<target;
            else next<<at+2;
            // Include call-entry predecessors for the unique-prelude check,
            // although exact helper calls are summarized in the analysis.
            if(op==3&&offsets.contains(target)){incoming[target].insert(at);entries.insert(target);}
        }else if(op!=4&&op!=0x12&&op!=0x2f&&op!=0x11)next<<at+2;
        for(int to:next)if(offsets.contains(to)){edges[at]<<to;incoming[to].insert(at);}
    }
    for(auto it=targets.cbegin();it!=targets.cend();++it)
        if(incoming.value(it.key())!=QSet<int>{it.key()-2})return {};

    // Strongly connected components distinguish a recurring scan from a
    // one-off assignment/index use. Current branches, not catalog edges, own it.
    QMap<int,int> index,low,component;
    QSet<int> onStack;
    QVector<int> stack;
    int serial=0,componentId=0;
    struct Frame {int at,next=0;};
    QVector<Frame> frames;
    const auto enter=[&](int at){index[at]=low[at]=serial++;stack<<at;onStack.insert(at);frames<<Frame{at};};
    for(int root:offsets){
        if(index.contains(root))continue;
        enter(root);
        while(!frames.isEmpty()){
            const int at=frames.last().at;
            const auto &next=edges[at];
            if(frames.last().next<next.size()){
                const int to=next[frames.last().next++];
                if(!index.contains(to))enter(to);
                else if(onStack.contains(to))low[at]=qMin(low[at],index[to]);
                continue;
            }
            frames.removeLast();
            if(low[at]==index[at]){int node;do{node=stack.takeLast();onStack.remove(node);component[node]=componentId;}while(node!=at);++componentId;}
            if(!frames.isEmpty()){const int parent=frames.last().at;low[parent]=qMin(low[parent],low[at]);}
        }
    }

    QMap<int,State> inputs;
    QVector<int> pending;
    QSet<int> queued;
    for(int at:entries)if(offsets.contains(at)){inputs[at]=State{};pending<<at;queued.insert(at);}
    int iterations=0;
    const int maximum=qMax(1000,offsets.size()*48);
    while(!pending.isEmpty()&&iterations++<maximum){
        const int at=pending.takeLast();queued.remove(at);
        const int op=u8(bytes,at),arg=u8(bytes,at+1),d=op&7;
        auto after=inputs[at];
        if(op>=0x68&&op<=0x6f)after[d]={at,arg};
        else if(op>=0x60&&op<=0x67){
            if(arg<8){after[d]=after[arg];if(after[d].literal>=0)after[d].literal&=255;}
            else if(arg!=0xf7)after[d]={};
        }else if(op>=0x78&&op<=0x7f){if(arg<0xe5||arg>=0xfd)after[d]={};}
        else if(op>=0x88&&op<=0x8f){
            if(after[d].literal>=0)after[d].literal=(after[d].literal&255)|(arg<<8);
            // Writing an upper byte changes the induction variable itself.
            after[d].origin=-1;
        }else if(op>=0x90&&op<=0x9f){
            const bool immediate=op>=0x98;
            const int operation=arg>>(immediate?5:3);
            const auto right=immediate?Value{-1,arg&31}:after[arg&7];
            const auto left=after[d];
            after[d]={};
            if(operation==2&&right.literal==1)after[d].origin=left.origin;
            if(left.literal>=0&&right.literal>=0){
                const quint32 a=left.literal,b=right.literal;quint32 value=0;
                bool valid=true;
                switch(operation){case 0:value=a&b;break;case 1:value=a|b;break;case 2:value=a+b;break;case 3:value=a-b;break;case 4:value=a*b;break;
                case 5:if(b)value=a/b;else valid=false;break;case 6:if(b)value=a%b;else valid=false;break;default:valid=false;break;}
                if(valid)after[d].literal=value&65535;
            }
        }else if(op==6)after[arg&7]={};
        else if(op==0x28){after[6]={};after[7]={};}
        else if(op==3){
            const auto helper=EventHelperNames::match(bytes,offsets,targets.value(at,-1));
            for(int r=0;r<8;++r)if(!(helper.preservedRegisters&(1<<r)))after[r]={};
        }else if(op==2||op==4||op==0x10||op==0x11||op==0x13)after=State{};
        for(int to:edges.value(at)){
            bool changed=false;
            if(!inputs.contains(to)){inputs[to]=after;changed=true;}
            else for(int r=0;r<8;++r){auto &old=inputs[to][r];
                if(old.origin>=0&&old.origin!=after[r].origin){old.origin=-1;changed=true;}
                if(old.literal>=0&&old.literal!=after[r].literal){old.literal=-1;changed=true;}
            }
            if(changed&&!queued.contains(to)){pending<<to;queued.insert(to);}
        }
    }
    if(!pending.isEmpty())return {};

    // An origin needs both an increment and an index service in the same
    // component. Merely seeing the same register number is insufficient.
    QMap<int,QSet<int>> increments;
    QMap<int,QMap<int,int>> uses;
    for(auto it=inputs.cbegin();it!=inputs.cend();++it){
        const int at=it.key(),op=u8(bytes,at),arg=u8(bytes,at+1),d=op&7;
        const auto &state=it.value();
        if(op>=0x90&&op<=0x9f){
            const bool immediate=op>=0x98;
            const int amount=immediate?(arg&31):state[arg&7].literal;
            if((arg>>(immediate?5:3))==2&&amount==1&&state[d].origin>=0)
                increments[state[d].origin].insert(component[at]);
        }
        if(((op>=0x60&&op<=0x67)||(op>=0x70&&op<=0x77))&&arg>=8&&arg<0xe4){
            const int bank=arg>>3,origin=state[arg&7].origin;
            const bool getter=op<=0x67;
            const int domain=bank==1?1:getter&&bank>=2&&bank<=6?2:getter&&bank>=7&&bank<=11?4:
                bank==27?32:!getter&&bank==22?64:bank==23?128:0;
            if(origin>=0&&domain)uses[origin][component[at]]|=domain;
        }else if(((op>=0x78&&op<=0x7f)||(op>=0x80&&op<=0x87))&&arg<0xe5){
            const int bank=arg>>3,origin=state[arg&7].origin;
            // Alternate banks 1/5 find by item ID, rather than by inventory
            // slot. Count/item banks 2..4 and 6..8 use actual slot indices.
            const int domain=bank>=2&&bank<=4?8:bank>=6&&bank<=8?16:op>=0x80&&bank==10?64:0;
            if(origin>=0&&domain)uses[origin][component[at]]|=domain;
        }
    }
    QMap<int,int> domains;
    QSet<int> externalOrigins;
    for(auto it=uses.cbegin();it!=uses.cend();++it)
        for(auto use=it.value().cbegin();use!=it.value().cend();++use)
            if(increments.value(it.key()).contains(use.key())){
                domains[it.key()]|=use.value();
                if(component.value(it.key(),-1)!=use.key())externalOrigins.insert(it.key());
            }
    QMap<int,QMap<int,int>> cutoffs;
    for(auto it=inputs.cbegin();it!=inputs.cend();++it){
        const int op=u8(bytes,it.key());
        if(!conditional(op)||(op&15)>=12)continue;
        const int left=(op>>2)&3,right=op&3;
        for(const auto pair:{qMakePair(left,right),qMakePair(right,left)}){
            const auto indexValue=it.value()[pair.first],limit=it.value()[pair.second];
            if(!domains.contains(indexValue.origin)||limit.origin<0||limit.literal<0)continue;
            auto &limits=cutoffs[indexValue.origin];
            if(limits.contains(limit.origin)&&limits[limit.origin]!=limit.literal)limits[limit.origin]=-1;
            else if(!limits.contains(limit.origin))limits[limit.origin]=limit.literal;
        }
    }
    QMap<int,Registers> result;
    for(auto it=inputs.cbegin();it!=inputs.cend();++it)for(int r=0;r<8;++r){
        const int domain=domains.value(it.value()[r].origin);
        if(!domain)continue;
        Expression e;
        switch(domain){
        case 1:e.kind="member-index";e.text="Current member position";break;
        case 2:e.kind="army-index";e.text="Current army character slot";break;
        case 4:e.kind="opposing-index";e.text="Current opposing character slot";break;
        case 8:e.kind="consumable-index";e.text="Current consumable inventory slot";break;
        case 16:e.kind="equipment-index";e.text="Current equipment inventory slot";break;
        case 32:e.kind="temporary-flag-index";e.text="Current temporary flag";break;
        case 64:e.kind="world-location-index";e.text="Current world location";break;
        case 128:e.kind="world-road-index";e.text="Current world road";break;
        default:e.kind="loop-index";e.text="Current loop index";break;
        }
        // For index expressions only, property records the actual literal
        // definition; bank=1 records initialization outside its recurring SCC.
        // These are proof metadata, never an editable register value/service.
        e.property=it.value()[r].origin;e.bank=externalOrigins.contains(e.property)?1:0;
        const auto limits=cutoffs.value(e.property);
        for(auto limit=limits.cbegin();limit!=limits.cend();++limit){
            Expression bound;bound.kind="scan-cutoff";bound.property=limit.key();bound.value=limit.value();e.children<<bound;
        }
        result[it.key()][r]=e;
    }
    return result;
}
