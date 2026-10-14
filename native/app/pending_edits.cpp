#include "app/pending_edits.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
namespace lordly {
QVector<PendingEdit> pendingEdits(QWidget *root) {
    QVector<PendingEdit> result;
    for (auto panel : root->findChildren<PendingEdits *>())
        if (panel->entries) result += panel->entries();
    return result;
}
bool resolvePendingEdits(QWidget *root, const QString &operation,
                         const std::function<void(const EditLocation &)> &navigate) {
    auto entries = pendingEdits(root);
    if (entries.isEmpty()) return true;
    QDialog dialog(root);
    dialog.setObjectName("pendingEditsDialog");
    dialog.setWindowTitle("Pending changes");
    dialog.resize(610, 340);
    auto layout = new QVBoxLayout(&dialog);
    auto help = new QLabel("These form changes have not been applied. Resolve them before " + operation + ".");
    help->setWordWrap(true); layout->addWidget(help);
    auto list = new QListWidget; list->setObjectName("pendingEditsList");
    for (const auto &entry : entries) list->addItem(entry.label);
    list->setCurrentRow(0); layout->addWidget(list);
    auto notice = new QLabel; notice->setWordWrap(true); layout->addWidget(notice);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto apply = buttons->addButton("Apply all", QDialogButtonBox::AcceptRole);
    apply->setObjectName("pendingApplyAll");
    auto discard = buttons->addButton("Discard all", QDialogButtonBox::DestructiveRole);
    discard->setObjectName("pendingDiscardAll");
    auto review = buttons->addButton("Review selected", QDialogButtonBox::ActionRole);
    review->setObjectName("pendingReview");
    layout->addWidget(buttons);
    int reviewRow = -1;
    QObject::connect(review, &QPushButton::clicked, &dialog, [&] {
        reviewRow = list->currentRow(); dialog.reject();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(apply, &QPushButton::clicked, &dialog, [&] {
        for (auto panel : root->findChildren<PendingEdits *>()) {
            if (!panel->entries || panel->entries().isEmpty()) continue;
            if (!panel->apply || !panel->apply()) {
                entries = pendingEdits(root); list->clear();
                for (const auto &entry : entries) list->addItem(entry.label);
                list->setCurrentRow(0);
                notice->setText("A pending change could not be applied. Review that editor to complete or correct it. Your draft is retained.");
                return;
            }
        }
        if (pendingEdits(root).isEmpty()) dialog.accept();
    });
    QObject::connect(discard, &QPushButton::clicked, &dialog, [&] {
        for (auto panel : root->findChildren<PendingEdits *>())
            if (panel->discard) panel->discard();
        dialog.accept();
    });
    const bool resolved = dialog.exec() == QDialog::Accepted;
    if (reviewRow >= 0 && reviewRow < entries.size()) navigate(entries[reviewRow].location);
    return resolved;
}
}
