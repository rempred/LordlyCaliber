#pragma once
#include "engine/event_flow.h"
namespace lordly {
// Read-only control-flow projection. Edits still address the original fields;
// composing conditions here never rewrites or relocates the game instructions.
struct StoryTreeNode {
    enum Kind { Action, Decision, Reference } kind = Action;
    int action = -1, offset = -1;
    StoryCondition condition;
    QVector<StoryTreeNode> thenBranch, elseBranch;
    QString referenceLabel;
    bool continuation = false;
};
struct StoryTree {
    QVector<StoryTreeNode> nodes;
    QVector<int> otherEntries;
};
StoryTree storyTree(const StoryEvent &, const QByteArray &program, int startOffset = -1, int budget = 40);
}
