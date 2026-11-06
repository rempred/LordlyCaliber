#pragma once
#include "engine/event_flow.h"
#include "engine/event_helper_names.h"
#include <algorithm>

namespace lordly::EventHelperNames {
// Presentation-only extraction after the ordinary flow has been projected.
// A helper receives its own page only when that page cannot sever fallthrough
// from the preceding event or conceal an externally reachable internal path.
// Return value is the number of routines summarized. No instruction is dropped.
inline int summarize(QVector<StoryEvent> &events, const QByteArray &bytes,
                     const QSet<int> &offsets, QMap<int,QSet<int>> incoming) {
    const auto byte=[&](int at){return int(quint8(bytes[at]));};
    const auto conditional=[](int op){return op>=0xa0&&QSet<int>{1,2,3,6,7,11,12,13,14,15}.contains(op&15);};
    const auto fallsThrough=[](int op){return op!=1&&op!=2&&op!=4&&op!=0x11&&op!=0x12&&op!=0x2f;};
    // Include current direct edges and physical fallthrough, not only the
    // audited retail edges. Unresolved local transfers could enter any word.
    QVector<int> localTransfers;
    for(int at:offsets){
        if(at<0||at+1>=bytes.size())return 0;
        const int op=byte(at),arg=byte(at+1);
        if(fallsThrough(op)&&offsets.contains(at+2))incoming[at+2].insert(at);
        if(op==1||op==0x2d||conditional(op))incoming[at+2+qint8(arg)*2].insert(at);
        if(op==2||op==3){
            if(!offsets.contains(at-2)||byte(at-2)!=0x68)return 0;
            const int target=quint16((at+2)/2+(arg<<8)+byte(at-1))*2;
            incoming[target].insert(at);localTransfers<<at;
        }
    }
    for(int at:localTransfers)
        for(int from:incoming.value(at))if(from!=at-2)return 0;
    QVector<int> entries=offsets.values();std::sort(entries.begin(),entries.end());
    int count=0;
    for(int entry:entries){
        const auto helper=match(bytes,offsets,entry);
        if(!helper.known()||!hasOnlyInternalEntrances(helper,incoming))continue;
        // Even a currently unreachable physical predecessor must not acquire
        // a broken presentation edge after an ordinary branch edit.
        if(offsets.contains(entry-2)&&fallsThrough(byte(entry-2)))continue;
        const int end=entry+helper.byteLength;
        const auto contains=[&](int at){return at>=entry&&at<end;};
        QSet<int> owned;bool safe=true;
        for(const auto &event:events){
            if(contains(event.entry)){safe=false;break;}
            for(int at:event.triggerInstructions)if(contains(at))safe=false;
            for(const auto &requirement:event.requirements)
                for(int at:requirement.instructions)if(contains(at))safe=false;
            for(const auto &action:event.actions){
                bool inside=false,outside=false;
                for(int at:action.instructions){
                    if(contains(at)){inside=true;if(owned.contains(at))safe=false;owned.insert(at);}
                    else outside=true;
                }
                if(inside&&outside)safe=false;
            }
        }
        if(!safe||owned.size()!=helper.byteLength/2)continue;
        for(int at=entry;at<end;at+=2)if(!owned.contains(at)){safe=false;break;}
        if(!safe)continue;
        for(auto &event:events){
            auto &actions=event.actions;
            actions.erase(std::remove_if(actions.begin(),actions.end(),[&](const StoryAction &action){
                return !action.instructions.isEmpty()&&contains(action.instructions.first());
            }),actions.end());
        }
        StoryEvent routine;routine.entry=entry;routine.group=-1;
        routine.title=name(helper.kind);routine.trigger="Shared event logic";
        StoryAction action;action.kind="return";
        action.title=name(helper.kind)+"; return the result to the calling event";
        for(int at=entry;at<end;at+=2)action.instructions<<at;
        routine.actions<<action;events<<routine;++count;
    }
    return count;
}
}
