#pragma once
#include <QStringList>
#include <QVector>

namespace lordly {
struct ProjectChange {
    QString area, subject;
    QStringList details;
};
struct ChangeReport {
    QString source;
    QVector<ProjectChange> entries;
    QString text() const;
};
} // namespace lordly
