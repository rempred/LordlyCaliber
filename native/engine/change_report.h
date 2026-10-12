#pragma once
#include <QStringList>
#include <QVector>
#include <QMetaType>

namespace lordly {
struct EditLocation {
    QString editor;
    int id = -1, child = -1;
    QString key;
};
struct ProjectChange {
    QString area, subject;
    QStringList details;
    EditLocation location;
};
struct ChangeReport {
    QString source;
    QVector<ProjectChange> entries;
    QString text() const;
};
} // namespace lordly
Q_DECLARE_METATYPE(lordly::EditLocation)
