#include "app/dialogue_inspector.h"
#include "app/dialogue_preview.h"
#include <QComboBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
QString display(const QByteArray &bytes) {
    QString result;
    for (const auto byte : bytes) {
        const auto value = quint8(byte);
        if (value == 92)
            result += QString(2, QChar(92));
        else if (value >= 32 && value <= 126)
            result += QChar(value);
        else
            result += QChar(92) + QString("x%1").arg(value, 2, 16, QChar('0'));
    }
    return result;
}
QByteArray parse(const QString &text) {
    QByteArray result;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const auto value = text[i].unicode();
        if (value < 32 || value > 126)
            throw Error("Use @n for a game line break, or a byte escape for non-ASCII data.");
        if (value != 92) {
            result += char(value);
            continue;
        }
        if (++i < text.size() && text[i] == QChar(92)) {
            result += char(92);
            continue;
        }
        bool valid = false;
        const auto hex = text.mid(i + 1, 2);
        const auto byte = hex.toUInt(&valid, 16);
        if (i >= text.size() || text[i] != 'x' || hex.size() != 2 || !valid ||
            QString("%1").arg(byte, 2, 16, QChar('0')) != hex.toLower())
            throw Error(
                "Complete the byte escape as backslash-x followed by two hexadecimal digits.");
        result += char(byte);
        i += 2;
    }
    return result;
}
class DialogueCommand final : public QUndoCommand {
  public:
    DialogueCommand(Project *project, QString label, QJsonObject before, QJsonObject after,
                    QString mergeKey = {})
        : QUndoCommand(label), project_(project), before_(std::move(before)),
          after_(std::move(after)), mergeKey_(std::move(mergeKey)),
          time_(QDateTime::currentMSecsSinceEpoch()) {}
    void undo() override {
        project_->setDialogues(before_);
    }
    void redo() override {
        project_->setDialogues(after_);
    }
    int id() const override {
        return mergeKey_.isEmpty() ? -1 : 0x4c4315;
    }
    bool mergeWith(const QUndoCommand *command) override {
        const auto other = dynamic_cast<const DialogueCommand *>(command);
        if (!other || other->project_ != project_ || other->mergeKey_ != mergeKey_ ||
            other->before_ != after_ || other->time_ - time_ > 1000)
            return false;
        after_ = other->after_;
        time_ = other->time_;
        return true;
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
    QString mergeKey_;
    qint64 time_;
};
} // namespace
DialogueInspector::DialogueInspector(Project *project, QUndoStack *undo,
                                     std::function<void()> changed, QWidget *parent)
    : DialogueInspector(project->rom(), parent) {
    project_ = project;
    preview_->setPortraitProvider([project](int id,bool variant) {
        project->loadAvatarArtwork();
        return project->avatarArtwork()->words(project->classData(),id,variant?1:0);
    });
    undo_ = undo;
    changed_ = std::move(changed);
    project_->loadDialogues();
    source_->setReadOnly(false);
    source_->setUndoRedoEnabled(false);
    add_->show();
    restore_->show();
    discard_->show();
    findChild<QLabel *>("dialogueNote")
        ->setText(
            "Edits enter the project as you type. Use @n for game line breaks and retain native "
            "control sequences. Nonprintable bytes use backslash-x plus two hexadecimal digits; "
            "a literal backslash is doubled. Playback uses the edited text.");
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        if (!editing_)
            loadArchive(entries_->currentRow());
        if (changed_)
            changed_();
    });
    connect(source_, &QPlainTextEdit::textChanged, this, &DialogueInspector::editSource);
    connect(add_, &QPushButton::clicked, this, [this] {
        if (!archive_)
            return;
        try {
            auto next = *project_->dialogues();
            const auto row = next.appendEntry(table(), archives_->currentData().toInt(), "@s@c");
            publish("Add dialogue entry", next);
            entries_->setCurrentRow(row);
        } catch (const std::exception &e) {
            status_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(restore_, &QPushButton::clicked, this, [this] {
        if (!archive_ || entries_->currentRow() < 0)
            return;
        auto next = *project_->dialogues();
        next.restoreEntry(table(), archives_->currentData().toInt(), entries_->currentRow());
        publish("Restore dialogue entry", next);
        showEntry();
    });
    connect(discard_, &QPushButton::clicked, this, [this] {
        showEntry();
        if (changed_)
            changed_();
    });
    loadArchive();
}

DialogueInspector::DialogueInspector(const Rom &rom, QWidget *parent) : QWidget(parent), rom_(rom) {
    auto layout = new QVBoxLayout(this);
    auto note = new QLabel("Dialogue source from the current ROM. Controls such as @n, @p and "
                           "{T05} are shown exactly. Use Playback to preview the selected entry.");
    note->setObjectName("dialogueNote");
    note->setWordWrap(true);
    layout->addWidget(note);
    archives_ = new QComboBox;
    archives_->setObjectName("dialogueArchives");
    archives_->setAccessibleName("Dialogue archive");
    layout->addWidget(archives_);
    auto buttons = new QHBoxLayout;
    add_ = new QPushButton("Add entry");
    add_->setObjectName("dialogueAdd");
    restore_ = new QPushButton("Restore original");
    restore_->setObjectName("dialogueRestore");
    discard_ = new QPushButton("Discard invalid input");
    discard_->setObjectName("dialogueDiscard");
    for (auto button : {add_, restore_, discard_}) {
        buttons->addWidget(button);
        button->hide();
    }
    buttons->addStretch();
    layout->addLayout(buttons);
    previewTimer_ = new QTimer(this);
    previewTimer_->setSingleShot(true);
    previewTimer_->setInterval(200);
    connect(previewTimer_, &QTimer::timeout, this, &DialogueInspector::updatePreview);
    auto split = new QSplitter;
    auto catalog = new QWidget;
    auto column = new QVBoxLayout(catalog);
    column->setContentsMargins(0, 0, 0, 0);
    auto search = new QLineEdit;
    search->setObjectName("dialogueSearch");
    search->setPlaceholderText("Search entries or control sequences…");
    search->setClearButtonEnabled(true);
    entries_ = new QListWidget;
    entries_->setObjectName("dialogueEntries");
    entries_->setAccessibleName("Dialogue entries");
    column->addWidget(search);
    column->addWidget(entries_, 1);
    split->addWidget(catalog);
    source_ = new QPlainTextEdit;
    source_->setObjectName("dialogueSource");
    source_->setAccessibleName("Exact dialogue source");
    source_->setReadOnly(true);
    source_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    auto pages = new QTabWidget;
    pages->setObjectName("dialogueDetailPages");
    pages->addTab(source_, "Source");
    preview_ = new DialoguePreview(rom_, this);
    pages->addTab(preview_, "Playback");
    split->addWidget(pages);
    split->setSizes({310, 690});
    layout->addWidget(split, 1);
    status_ = new QLabel;
    status_->setObjectName("dialogueStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    connect(search, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < entries_->count(); ++i) {
            auto item = entries_->item(i);
            item->setHidden(
                !item->data(Qt::UserRole).toString().contains(text, Qt::CaseInsensitive) &&
                !QString::number(i).contains(text));
        }
    });
    connect(archives_, &QComboBox::currentIndexChanged, this, [this, search] {
        search->clear();
        loadArchive();
    });
    connect(entries_, &QListWidget::currentRowChanged, this, [this] { showEntry(); });
    try {
        const QSignalBlocker block(archives_);
        for (const auto table :
             {DialogueArchive::Table::Primary, DialogueArchive::Table::Alternate}) {
            const auto keys = DialogueArchive::directory(rom_, table);
            for (int i = 0; i < keys.size(); ++i)
                if (keys[i]) {
                    const bool alternate = table == DialogueArchive::Table::Alternate;
                    archives_->addItem(
                        QString(alternate ? "Alternate archive %1" : "Archive %1").arg(i), i);
                    archives_->setItemData(archives_->count() - 1, alternate ? 1 : 0,
                                           Qt::UserRole + 1);
                }
        }
    } catch (const std::exception &e) {
        status_->setText(QString::fromUtf8(e.what()));
        archives_->setEnabled(false);
        return;
    }
    loadArchive();
}
void DialogueInspector::loadArchive(int row) {
    const QSignalBlocker sourceBlock(source_);
    previewTimer_->stop();
    archive_.reset();
    entries_->clear();
    source_->clear();
    preview_->clear();
    setProperty("archiveSelector", -1);
    setProperty("archiveTable", -1);
    setProperty("entryIndex", -1);
    if (archives_->currentIndex() < 0) {
        status_->setText("No dialogue archives.");
        return;
    }
    try {
        const int selector = archives_->currentData().toInt();
        const bool alternate = archives_->currentData(Qt::UserRole + 1).toInt() == 1;
        auto archive = DialogueArchive::load(rom_, selector,
                                             alternate ? DialogueArchive::Table::Alternate
                                                       : DialogueArchive::Table::Primary);
        const QSignalBlocker block(entries_);
        archive_ = archive;
        for (int i = 0; i < count(); ++i) {
            const auto text = display(bytes(i));
            auto item =
                new QListWidgetItem(QString("%1  ·  %2").arg(i).arg(text.left(80)), entries_);
            item->setData(Qt::UserRole, text);
            item->setToolTip(text);
        }
        archive_ = std::move(archive);
        setProperty("archiveSelector", selector);
        setProperty("archiveTable", alternate ? 1 : 0);
        entries_->setCurrentRow(qBound(0, row, count() - 1));
    } catch (const std::exception &e) {
        status_->setText(QString::fromUtf8(e.what()));
        return;
    }
    showEntry();
}
void DialogueInspector::showEntry() {
    const QSignalBlocker sourceBlock(source_);
    previewTimer_->stop();
    validity(true);
    restore_->setEnabled(archive_ && entries_->currentRow() >= 0 &&
                         entries_->currentRow() < archive_->entryCount());
    source_->clear();
    preview_->clear();
    setProperty("entryIndex", -1);
    if (!archive_ || entries_->currentRow() < 0)
        return;
    const int entry = entries_->currentRow();
    const auto text = bytes(entry);
    source_->setPlainText(display(text));
    setProperty("entryIndex", entry);
    updatePreview();
    status_->setText(QString("%1 · %2 · entry %3 of %4 · %5 source bytes")
                         .arg(archive_->name())
                         .arg(archives_->currentText())
                         .arg(entry)
                         .arg(count())
                         .arg(text.size()));
}
void DialogueInspector::selectEntry(int selector, int entry) {
    if (property("invalidEdits").toBool())
        return;
    const QSignalBlocker sourceBlock(source_);
    const int group = entry >= 0 && entry <= 0xffff ? entry & 0xc000 : -1;
    const bool supported = group == 0 || group == 0x8000;
    const int table = group == 0x8000 ? 1 : 0;
    int index = -1;
    if (supported) {
        entry &= 0x3fff;
        for (int i = 0; i < archives_->count(); ++i)
            if (archives_->itemData(i).toInt() == selector &&
                archives_->itemData(i, Qt::UserRole + 1).toInt() == table) {
                index = i;
                break;
            }
    }
    if (index < 0) {
        const QSignalBlocker block(archives_);
        archives_->setCurrentIndex(-1);
        archive_.reset();
        entries_->clear();
        source_->clear();
        preview_->clear();
        setProperty("archiveSelector", -1);
        setProperty("archiveTable", -1);
        setProperty("entryIndex", -1);
        status_->setText(
            supported ? QString("Archive %1 is not present in the current ROM.").arg(selector)
                      : "This dialogue entry selects an unresolved archive table group.");
        return;
    }
    if (archives_->currentIndex() == index)
        loadArchive();
    else
        archives_->setCurrentIndex(index);
    if (!archive_)
        return;
    if (entry < 0 || entry >= count()) {
        entries_->setCurrentRow(-1);
        status_->setText(QString("Entry %1 is outside archive %2.").arg(entry).arg(selector));
        return;
    }
    if (auto search = findChild<QLineEdit *>("dialogueSearch"))
        search->clear();
    entries_->setCurrentRow(entry);
    entries_->scrollToItem(entries_->currentItem());
}
DialogueArchive::Table DialogueInspector::table() const {
    return archives_->currentData(Qt::UserRole + 1).toInt() ? DialogueArchive::Table::Alternate
                                                            : DialogueArchive::Table::Primary;
}
int DialogueInspector::count() const {
    return project_ ? project_->dialogues()->entryCount(table(), archives_->currentData().toInt())
                    : archive_->entryCount();
}
QByteArray DialogueInspector::bytes(int index) const {
    return project_ ? project_->dialogues()->entry(table(), archives_->currentData().toInt(), index)
                    : archive_->entry(index);
}
void DialogueInspector::validity(bool valid) {
    setProperty("invalidEdits", !valid);
    archives_->setEnabled(valid);
    entries_->setEnabled(valid);
    add_->setEnabled(valid && archive_ && count() < 16384);
    discard_->setEnabled(!valid);
}
void DialogueInspector::updatePreview() {
    if (!archive_ || entries_->currentRow() < 0 || property("invalidEdits").toBool())
        return;
    preview_->selectEntry(archives_->currentData().toInt(),
                          quint16(entries_->currentRow() |
                                  (table() == DialogueArchive::Table::Alternate ? 0x8000 : 0)),
                          project_ ? std::optional<Dialogues>(*project_->dialogues())
                                   : std::nullopt);
}
void DialogueInspector::publish(const QString &label, const Dialogues &next) {
    if (next.json() == project_->dialogues()->json())
        return;
    const auto key = editing_ ? QString("%1/%2/%3")
                                    .arg(int(table()))
                                    .arg(archives_->currentData().toInt())
                                    .arg(entries_->currentRow())
                              : QString();
    undo_->push(
        new DialogueCommand(project_, label, project_->dialogues()->json(), next.json(), key));
}
void DialogueInspector::editSource() {
    if (!archive_ || entries_->currentRow() < 0)
        return;
    try {
        const auto text = parse(source_->toPlainText());
        auto next = *project_->dialogues();
        next.setEntry(table(), archives_->currentData().toInt(), entries_->currentRow(), text);
        validity(true);
        editing_ = true;
        publish("Edit dialogue text", next);
        editing_ = false;
        auto item = entries_->currentItem();
        item->setText(
            QString("%1  ·  %2").arg(entries_->currentRow()).arg(source_->toPlainText().left(80)));
        item->setData(Qt::UserRole, source_->toPlainText());
        item->setToolTip(source_->toPlainText());
        status_->setText(QString("Entry %1 updated in project · %2 source bytes")
                             .arg(entries_->currentRow())
                             .arg(text.size()));
        preview_->clear();
        previewTimer_->start();
    } catch (const std::exception &e) {
        editing_ = false;
        validity(false);
        previewTimer_->stop();
        preview_->clear();
        status_->setText(QString("Invalid edit: %1 Save and export are blocked until corrected.")
                             .arg(QString::fromUtf8(e.what())));
    }
    if (changed_)
        changed_();
}
} // namespace lordly
