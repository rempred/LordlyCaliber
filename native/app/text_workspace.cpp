#include "app/text_workspace.h"
#include "app/view_position.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>
#include <memory>

namespace lordly {
static QString descriptionLabel(const Project &project, const QString &kind, int id) {
    int recordId = id;
    if (kind == "classes") {
        recordId = id < 165 ? id : id >= 225 ? id - 60 : -1;
        if (id >= 200 && id <= 222)
            return QString("Class-change help %1").arg(id + 2600);
        if (recordId < 0)
            return QString(id == 224 ? "Special description %1" : "Reserved slot %1").arg(id);
    }
    QString name;
    if (kind == "actions")
        name = project.catalog()["ACTION_NAMES"].toObject()[QString::number(id)].toString();
    else {
        const auto table = kind == "classes" ? Table::Classes
                           : kind == "items" ? Table::Items : Table::Consumables;
        const auto &records = project.records(table);
        if (recordId >= 0 && recordId < records.size())
            name = QString(records[recordId].name).replace('\n', ' ');
    }
    if (name.isEmpty())
        name = project.textBanks()[kind].text(id).section('\n', 0, 0).left(75);
    if (name.isEmpty())
        name = "Description slot";
    const auto identity = kind == "classes" ? QString("Class %1").arg(recordId)
                                            : QString::number(id);
    return QString("%1 · %2").arg(identity, name);
}
class TextCommand final : public QUndoCommand {
  public:
    TextCommand(Project *p, QString kind, int record, QByteArray before, QByteArray after,
                std::function<void()> changed)
        : QUndoCommand("Edit description"), project_(p), kind_(std::move(kind)), record_(record),
          before_(std::move(before)), after_(std::move(after)), changed_(std::move(changed)),
          time_(QDateTime::currentMSecsSinceEpoch()) {}
    void undo() override {
        project_->replaceTextBytes(kind_, record_, before_);
        changed_();
    }
    void redo() override {
        project_->replaceTextBytes(kind_, record_, after_);
        changed_();
    }
    int id() const override {
        return 0x4c4301;
    }
    bool mergeWith(const QUndoCommand *command) override {
        auto next = dynamic_cast<const TextCommand *>(command);
        if (!next || next->project_ != project_ || next->kind_ != kind_ ||
            next->record_ != record_ || next->time_ - time_ > 1000)
            return false;
        after_ = next->after_;
        time_ = next->time_;
        return true;
    }

  private:
    Project *project_;
    QString kind_;
    int record_;
    QByteArray before_, after_;
    std::function<void()> changed_;
    qint64 time_;
};
QWidget *createTextWorkspace(Project *project, QUndoStack *undo, std::function<void()> changed) {
    auto page = new QWidget;
    page->setObjectName("descriptionsWorkspace");
    auto outer = new QVBoxLayout(page);
    auto banks = new QComboBox;
    banks->setObjectName("descriptionBank");
    for (auto it = project->textBanks().begin(); it != project->textBanks().end(); ++it)
        banks->addItem(it->spec.label, it.key());
    outer->addWidget(banks);
    auto split = new QSplitter;
    outer->addWidget(split);
    auto sidebar = new QWidget;
    auto sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    auto search = new QLineEdit;
    search->setObjectName("descriptionSearch");
    search->setPlaceholderText("Find a description…");
    search->setClearButtonEnabled(true);
    sideLayout->addWidget(search);
    auto showEmpty = new QCheckBox("Show empty entries");
    showEmpty->setObjectName("descriptionShowEmpty");
    showEmpty->setToolTip("Include records with no description text, including reserved slots. "
                         "Empty entries remain available for authoring.");
    sideLayout->addWidget(showEmpty);
    auto counts = new QLabel;
    counts->setObjectName("descriptionCounts");
    sideLayout->addWidget(counts);
    auto entries = new QListWidget;
    entries->setObjectName("descriptionEntries");
    entries->setMinimumWidth(280);
    sideLayout->addWidget(entries);
    split->addWidget(sidebar);
    auto details = new QWidget;
    auto layout = new QVBoxLayout(details);
    auto heading = new QLabel;
    heading->setObjectName("descriptionHeading");
    heading->setWordWrap(true);
    layout->addWidget(heading);
    auto editor = new QPlainTextEdit;
    editor->setObjectName("descriptionEditor");
    editor->setUndoRedoEnabled(false);
    layout->addWidget(editor);
    auto validation =
        new QLabel("Edits are saved in the project as you type. Up to four game text lines.");
    validation->setWordWrap(true);
    validation->setObjectName("descriptionValidation");
    layout->addWidget(validation);
    auto restore = new QPushButton("Restore original description");
    restore->setObjectName("descriptionRestore");
    layout->addWidget(restore, 0, Qt::AlignLeft);
    split->addWidget(details);
    split->setSizes({360, 900});
    struct Selection {
        QString bank;
        int row = -1;
        bool editing = false;
    };
    auto selected = std::make_shared<Selection>();
    auto updateEntry = [=](int row) {
        auto item = entries->item(row);
        if (!item)
            return;
        const bool empty = project->textBanks()[selected->bank].text(row).trimmed().isEmpty();
        item->setData(Qt::UserRole, empty);
        item->setText(descriptionLabel(*project, selected->bank, row) + (empty ? " — Empty" : ""));
        item->setToolTip(item->text());
    };
    auto filter = [=](bool retainSelection = false) {
        int first = -1, emptyCount = 0;
        for (int i = 0; i < entries->count(); ++i) {
            auto item = entries->item(i);
            const bool empty = item->data(Qt::UserRole).toBool();
            emptyCount += empty;
            const bool matches = item->text().contains(search->text().trimmed(), Qt::CaseInsensitive);
            const bool keep = retainSelection && i == selected->row;
            item->setHidden(!keep && (!matches || (empty && !showEmpty->isChecked())));
            if (!item->isHidden() && first < 0)
                first = i;
        }
        counts->setText(QString("%1 with text · %2 empty entries")
                            .arg(entries->count() - emptyCount).arg(emptyCount));
        if (!entries->currentItem() || entries->currentItem()->isHidden())
            entries->setCurrentRow(first);
    };
    auto refresh = [=] {
        if (selected->editing)
            return;
        QSignalBlocker block(editor);
        if (selected->row < 0) {
            editor->clear();
            editor->setPlaceholderText("Select a description.");
            return;
        }
        editor->setPlainText(project->textBanks()[selected->bank].text(selected->row));
        editor->setPlaceholderText("No description text is stored for this entry. Type here to add one.");
        updateEntry(selected->row);
        heading->setText(entries->item(selected->row)->text());
        page->setProperty("invalidEdits", false);
        entries->setEnabled(true);
        banks->setEnabled(true);
        search->setEnabled(true);
        showEmpty->setEnabled(true);
        validation->setText(editor->toPlainText().trimmed().isEmpty()
            ? "This record is empty. You can author a description here; no text will be added until you type."
            : "Edits are saved in the project as you type. Up to four game text lines.");
    };
    auto notify = [=] {
        refresh();
        changed();
    };
    QObject::connect(entries, &QListWidget::currentRowChanged, page, [=](int row) {
        selected->row = row;
        editor->setEnabled(row >= 0);
        restore->setEnabled(row >= 0);
        heading->setText(row >= 0 ? entries->item(row)->text() : "Select a description");
        refresh();
    });
    auto selectBank = [=] {
        const bool sameBank = selected->bank == banks->currentData().toString();
        const ViewPosition keepView(page, sameBank);
        const auto previous = sameBank ? selected->row : 1;
        selected->row = -1;
        selected->bank = banks->currentData().toString();
        QSignalBlocker block(entries);
        entries->clear();
        const auto &bank = project->textBanks()[selected->bank];
        for (int i = 0; i < bank.entries.size(); ++i) {
            entries->addItem(QString());
            updateEntry(i);
        }
        block.unblock();
        entries->setCurrentRow(qBound(0, previous, int(bank.entries.size()) - 1));
        if (!sameBank)
            search->clear();
        filter(sameBank);
    };
    QObject::connect(banks, &QComboBox::currentIndexChanged, page, [=] { selectBank(); });
    QObject::connect(undo, &QUndoStack::indexChanged, page, [=] {
        if (!selected->editing)
            selectBank();
    });
    QObject::connect(search, &QLineEdit::textChanged, page, [=] { filter(); });
    QObject::connect(showEmpty, &QCheckBox::toggled, page, [=] { filter(); });
    QObject::connect(editor, &QPlainTextEdit::textChanged, page, [=] {
        if (selected->row < 0)
            return;
        try {
            auto bank = project->textBanks()[selected->bank];
            auto before = bank.entries[selected->row].bytes;
            bank.setText(selected->row, editor->toPlainText());
            auto after = bank.entries[selected->row].bytes;
            page->setProperty("invalidEdits", false);
            entries->setEnabled(true);
            banks->setEnabled(true);
            search->setEnabled(true);
            showEmpty->setEnabled(true);
            validation->setText("Description updated. Hidden game references remain intact.");
            if (before != after) {
                selected->editing = true;
                undo->push(
                    new TextCommand(project, selected->bank, selected->row, before, after, notify));
                selected->editing = false;
            }
            updateEntry(selected->row);
            heading->setText(entries->item(selected->row)->text());
            filter(true); // Keep the record in view while its text is being cleared or restored.
        } catch (const std::exception &e) {
            page->setProperty("invalidEdits", true);
            entries->setEnabled(false);
            banks->setEnabled(false);
            search->setEnabled(false);
            showEmpty->setEnabled(false);
            validation->setText(QString::fromUtf8(e.what()));
        }
        changed();
    });
    QObject::connect(restore, &QPushButton::clicked, page, [=] {
        if (selected->row < 0)
            return;
        const auto &entry = project->textBanks()[selected->bank].entries[selected->row];
        undo->push(new TextCommand(project, selected->bank, selected->row, entry.bytes,
                                   entry.original, notify));
    });
    selectBank();
    return page;
}
} // namespace lordly
