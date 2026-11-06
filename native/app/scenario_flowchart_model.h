#pragma once
#include "engine/event_context.h"
#include "engine/event_flow.h"

namespace lordly {
class Project;

struct ChartAddress {
    int row = -1, offset = -1;
    QString tree;
    QVector<int> path; // Child indices in the authored tree editor.
    int gate=-1;
    bool gateControl=false;
    bool gateReversed=false;
};
struct ChartFlag {
    int id;
    QString use;
};
struct ChartStep {
    QString title;
    ChartAddress address;
    QVector<ChartFlag> flags;
    bool decision = false;
    QVector<int> instructions;
    QString detail;
};
struct ChartEdge {
    int from, to;
    QString label;
};
struct ChartEvent {
    QString section, trigger, title;
    ChartAddress address;
    QVector<ChartStep> steps;
    QVector<ChartEdge> edges;
    int group=-1;
    bool automatic=false;
    QVector<QPair<QString,int>> sections; // Named entry points for the compact outline.
};
// Shared game processing before mission-completion handlers, not VM commands.
ChartEvent missionEndFlow(const Project &);
QVector<ChartEvent> scenarioChart(Project &, const QVector<QPair<QString,EventProgramChoice>> &);
QString storyConditionText(const StoryCondition &);
}
