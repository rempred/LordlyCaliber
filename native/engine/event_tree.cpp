#include "engine/event_tree.h"
#include <QHash>
#include <algorithm>
#include <functional>
namespace lordly {
StoryTree storyTree(const StoryEvent &event, const QByteArray &program, int startOffset, int budget) {
    StoryTree result;
    const int size = event.actions.size(), exit = size;
    if (!size) return result;
    const auto offset = [&](int i) { return i >= 0 && i < size && !event.actions[i].instructions.isEmpty()
                                            ? event.actions[i].instructions.first() : -1; };
    QHash<int,int> owners;
    for(int i=0;i<size;++i)for(int at:event.actions[i].instructions)owners[at]=i;
    const auto index = [&](int at) { return owners.value(at,exit); };
    const auto opcode = [&](int i) {
        const auto &words = event.actions[i].instructions;
        return words.isEmpty() || words.last() >= program.size() ? -1 : int(u8(program, words.last()));
    };
    const auto jump = [&](int i) { return event.actions[i].kind == "route" &&
                                        (opcode(i) == 1 || opcode(i) == 2); };
    QVector<QVector<int>> edges(size + 1);
    for (int i = 0; i < size; ++i) {
        const auto &a = event.actions[i];
        if (a.target >= 0 && a.otherwise >= 0) edges[i] = {index(a.target), index(a.otherwise)};
        else if (jump(i)) edges[i] = {index(a.target)};
        else if (a.kind == "end" || a.kind == "program" || opcode(i) == 4 || opcode(i) == 0x11 || opcode(i) == 0x12)
            edges[i] = {exit};
        else edges[i] = {i + 1};
    }
    // Only merge at a point every outgoing path must reach. A merely reachable
    // common node is insufficient (one side may return or loop before it).
    int start = startOffset >= 0 ? index(startOffset) : 0;
    if (start == exit) start = 0;
    QSet<int> active;
    QVector<int> queue{start};
    for(int q=0;q<queue.size()&&active.size()<qMax(64,budget*4);++q){
        const int i=queue[q];if(i==exit||active.contains(i))continue;
        active.insert(i);for(int next:edges[i])if(next!=exit&&!active.contains(next))queue<<next;
    }
    QSet<int> terminating{exit};
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i : active) if (!terminating.contains(i))
            for (int next : edges[i]) if (terminating.contains(next)) { terminating.insert(i); changed = true; break; }
    }
    QVector<QSet<int>> post(size + 1);
    for (int i = 0; i <= size; ++i) post[i] = i == exit || !terminating.contains(i) ? QSet<int>{i} : terminating;
    changed = true;
    while (changed) {
        changed = false;
        for (int i : active) {
            if (!terminating.contains(i)) continue;
            auto common = post[edges[i].first()];
            for (int next : edges[i]) common.intersect(post[next]);
            common.insert(i);
            if (post[i] != common) { post[i] = common; changed = true; }
        }
    }
    auto join = [&](int a, int b) {
        auto common = post[a];common.intersect(post[b]);common.remove(exit);
        int nearest = exit;
        for (int candidate : common)
            if (nearest == exit || post[candidate].size() > post[nearest].size() ||
                (post[candidate].size() == post[nearest].size() && candidate < nearest)) nearest = candidate;
        return nearest;
    };
    struct Destination {int index, offset;};
    auto follow = [&](int at) {
        int i=index(at);
        QSet<int> seen;
        while (i != exit && jump(i) && !seen.contains(i)) { seen.insert(i);at=event.actions[i].target;i=index(at); }
        return Destination{i,at};
    };
    const auto same=[](Destination a,Destination b){return a.index==b.index&&a.offset==b.offset;};
    const auto test = [&](int i) { return i >= 0 && i < size && !event.actions[i].advanced &&
        event.actions[i].target >= 0 && event.actions[i].otherwise >= 0 && event.actions[i].kind == "condition"; };
    const auto condition = [&](int i) {
        auto value = event.actions[i].condition;
        if (value.operation.isEmpty()) { value.operation = "test";value.title = event.actions[i].title;value.fields = event.actions[i].fields; }
        return value;
    };
    auto combine = [](QString op, StoryCondition a, StoryCondition b) {
        if (a.operation == op) { a.children << b;return a; }
        StoryCondition combined;combined.operation = op;combined.children = {a,b};return combined;
    };
    QSet<int> shown;
    auto reference = [&](int i, QString title, bool continuation = false, int targetOffset = -1) {
        StoryTreeNode node;node.kind = StoryTreeNode::Reference;node.action = i < size ? i : -1;
        node.offset = targetOffset >= 0 ? targetOffset : offset(i);node.referenceLabel = title;node.continuation = continuation;return node;
    };
    std::function<QVector<StoryTreeNode>(int,int,QSet<int>,int)> walk;
    walk = [&](int current, int stop, QSet<int> path, int depth) {
        QVector<StoryTreeNode> nodes;
        while (current != exit && current != stop) {
            if (path.contains(current)) { nodes << reference(current,"Repeat from");break; }
            if (shown.contains(current)) { nodes << reference(current,"Continue at");break; }
            if (budget <= 0 || depth >= 12) { nodes << reference(current,"Continue with",true);break; }
            path.insert(current);shown.insert(current);
            if (jump(current)) {
                if (edges[current].first() == exit)
                    nodes << reference(exit,"Continue in shared event logic",false,event.actions[current].target);
                current = edges[current].first();continue;
            }
            --budget;
            StoryTreeNode node;node.action = current;node.offset = offset(current);
            if (test(current)) {
                node.kind = StoryTreeNode::Decision;node.condition = condition(current);
                auto yes = follow(event.actions[current].target), no = follow(event.actions[current].otherwise);
                // Consecutive pure tests with the same failure/success exit are
                // native short-circuit AND/OR. Their evaluation order is retained.
                while (budget > 0) {
                    if (test(yes.index) && yes.index != current && !path.contains(yes.index) && !shown.contains(yes.index) && same(follow(event.actions[yes.index].otherwise),no)) {
                        node.condition = combine("all",node.condition,condition(yes.index));
                        shown.insert(yes.index);path.insert(yes.index);--budget;yes = follow(event.actions[yes.index].target);
                    } else if (test(no.index) && no.index != current && !path.contains(no.index) && !shown.contains(no.index) && same(follow(event.actions[no.index].target),yes)) {
                        node.condition = combine("any",node.condition,condition(no.index));
                        shown.insert(no.index);path.insert(no.index);--budget;no = follow(event.actions[no.index].otherwise);
                    } else break;
                }
                const int merge = join(yes.index,no.index);
                node.thenBranch = walk(yes.index,merge,path,depth+1);
                node.elseBranch = walk(no.index,merge,path,depth+1);
                // A destination outside this handler is still a meaningful
                // route, not an empty branch or an implicit event completion.
                if (yes.index == exit && yes.offset >= 0)
                    node.thenBranch << reference(exit,"Continue at",false,yes.offset);
                if (no.index == exit && no.offset >= 0)
                    node.elseBranch << reference(exit,"Continue at",false,no.offset);
                nodes << node;current = merge;
            } else {
                node.kind = StoryTreeNode::Action;nodes << node;
                // Unresolved control flow remains explicit. Never display both
                // possible branches as an unconditional sequence of actions.
                if (edges[current].size() > 1) break;
                current = edges[current].first();
            }
        }
        return nodes;
    };
    result.nodes = walk(start,exit,{},0);
    for (int i = 0; i < size; ++i) if (!shown.contains(i) && !jump(i)) result.otherEntries << offset(i);
    return result;
}
}
