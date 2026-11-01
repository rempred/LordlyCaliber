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
// Stored squad overrides that the game's equipment initializer ignores.
// These are advisory and include unchanged source assignments.
QVector<ProjectChange> equipmentWarnings(Project snapshot);
struct EquipmentWarningReview {
    QVector<ProjectChange> introduced, existing;
};
// Compare against the opened ROM unless the review supplies another reference ROM.
EquipmentWarningReview reviewEquipmentWarnings(Project snapshot, const Rom *reference = nullptr);
} // namespace lordly
