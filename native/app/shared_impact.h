#pragma once
#include "app/main_window.h"
#include "engine/project_references.h"
#include <QLabel>
namespace lordly {
inline void showEditImpact(QLabel *label, const QString &heading,
                           const QVector<ProjectChange> &uses, const QString &detail = {}) {
    QStringList links;
    for (int i = 0; i < uses.size(); ++i)
        links << QString("<a href=\"%1\">%2</a>").arg(i).arg(uses[i].subject.toHtmlEscaped());
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setOpenExternalLinks(false);
    label->setText("<b>" + heading.toHtmlEscaped() + "</b><br>" +
                   (links.isEmpty() ? "No known references." : links.join(" · ")) +
                   (detail.isEmpty() ? QString() : "<br>" + detail.toHtmlEscaped()));
    QObject::disconnect(label, &QLabel::linkActivated, label, nullptr);
    QObject::connect(label, &QLabel::linkActivated, label, [label, uses](const QString &link) {
        bool valid = false; const int row = link.toInt(&valid);
        if (!valid || row < 0 || row >= uses.size()) return;
        for (auto parent = label->parentWidget(); parent; parent = parent->parentWidget())
            if (auto window = qobject_cast<MainWindow *>(parent)) { window->navigateTo(uses[row].location); break; }
    });
}
}
