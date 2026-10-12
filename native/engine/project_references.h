#pragma once
#include "engine/project.h"
namespace lordly {
struct ProjectReference {
    QString kind;
    int id = 0;
    QString description;
    EditLocation location;
    bool valid = true, changed = false;
};
struct ExportIssue {
    QString message;
    EditLocation location;
    bool blocking = true;
};
QVector<ProjectReference> projectReferences(Project snapshot);
QVector<ProjectChange> findProjectUses(Project snapshot, const QString &kind, int id,
                                     const QString &key = {});
QVector<ExportIssue> exportIssues(Project snapshot);
} // namespace lordly
