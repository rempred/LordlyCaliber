#include "app/text_workspace.h"
#include "app/view_position.h"
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
    for (auto it = project->textBanks().begin(); it != project->textBanks().end(); ++it)
        banks->addItem(it->spec.label, it.key());
    outer->addWidget(banks);
    auto split = new QSplitter;
    outer->addWidget(split);
    auto sidebar = new QWidget;
    auto sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    auto search = new QLineEdit;
    search->setPlaceholderText("Find a description…");
    search->setClearButtonEnabled(true);
    sideLayout->addWidget(search);
    auto entries = new QListWidget;
    entries->setMinimumWidth(280);
    sideLayout->addWidget(entries);
    split->addWidget(sidebar);
    auto details = new QWidget;
    auto layout = new QVBoxLayout(details);
    auto heading = new QLabel;
    layout->addWidget(heading);
    auto editor = new QPlainTextEdit;
    editor->setObjectName("descriptionEditor");
    editor->setUndoRedoEnabled(false);
    layout->addWidget(editor);
    auto validation =
        new QLabel("Edits are saved in the project as you type. Up to four game text lines.");
    validation->setWordWrap(true);
    layout->addWidget(validation);
    auto restore = new QPushButton("Restore original description");
    layout->addWidget(restore, 0, Qt::AlignLeft);
    split->addWidget(details);
    split->setSizes({360, 900});
    struct Selection {
        QString bank;
        int row = -1;
        bool editing = false;
    };
    auto selected = std::make_shared<Selection>();
    auto refresh = [=] {
        if (selected->row < 0 || selected->editing)
            return;
        QSignalBlocker block(editor);
        editor->setPlainText(project->textBanks()[selected->bank].text(selected->row));
        page->setProperty("invalidEdits", false);
        entries->setEnabled(true);
        banks->setEnabled(true);
        validation->setText(
            "Edits are saved in the project as you type. Up to four game text lines.");
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
        const ViewPosition keepView(page, selected->bank == banks->currentData().toString());
        const auto previous = selected->bank == banks->currentData().toString() ? selected->row : 1;
        selected->row = -1;
        selected->bank = banks->currentData().toString();
        QSignalBlocker block(entries);
        entries->clear();
        const auto &bank = project->textBanks()[selected->bank];
        QString namesKey = selected->bank == "classes"   ? "CLASS_NAMES"
                           : selected->bank == "items"   ? "ITEM_NAMES"
                           : selected->bank == "actions" ? "ACTION_NAMES"
                                                         : "";
        auto names = project->catalog().value(namesKey).toObject();
        for (int i = 0; i < bank.entries.size(); ++i) {
            const auto classId = i < 165 ? i : i >= 225 ? i - 60 : -1;
            const auto nameId = selected->bank == "classes" ? classId : i;
            auto name = nameId >= 0 ? names.value(QString::number(nameId)).toString() : QString();
            if (name.isEmpty())
                name = bank.text(i).section('\n', 0, 0).left(75);
            const auto labelId = selected->bank == "classes" && classId >= 0
                                     ? QString("Class %1").arg(classId)
                                     : QString::number(i);
            entries->addItem(QString("%1 · %2").arg(labelId, name));
        }
        block.unblock();
        entries->setCurrentRow(qBound(0, previous, int(bank.entries.size()) - 1));
        search->clear();
    };
    QObject::connect(banks, &QComboBox::currentIndexChanged, page, [=] { selectBank(); });
    QObject::connect(undo, &QUndoStack::indexChanged, page, [=] {
        if (!selected->editing)
            selectBank();
    });
    QObject::connect(search, &QLineEdit::textChanged, page, [=](const QString &query) {
        for (int i = 0; i < entries->count(); ++i)
            entries->item(i)->setHidden(
                !entries->item(i)->text().contains(query, Qt::CaseInsensitive));
    });
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
            validation->setText("Description updated. Hidden game references remain intact.");
            if (before != after) {
                selected->editing = true;
                undo->push(
                    new TextCommand(project, selected->bank, selected->row, before, after, notify));
                selected->editing = false;
            }
        } catch (const std::exception &e) {
            page->setProperty("invalidEdits", true);
            entries->setEnabled(false);
            banks->setEnabled(false);
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
