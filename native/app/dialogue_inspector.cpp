#include "app/dialogue_inspector.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>
namespace lordly {
DialogueInspector::DialogueInspector(const Rom &rom, QWidget *parent) : QWidget(parent), rom_(rom) {
    auto layout = new QVBoxLayout(this);
    auto note = new QLabel(
        "Dialogue source from the current ROM. Controls such as @n, @p and "
        "{T05} are shown exactly. Window layout and timed playback are being implemented.");
    note->setWordWrap(true);
    layout->addWidget(note);
    archives_ = new QComboBox;
    archives_->setObjectName("dialogueArchives");
    archives_->setAccessibleName("Dialogue archive");
    layout->addWidget(archives_);
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
    split->addWidget(source_);
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
        const auto keys = DialogueArchive::directory(rom_);
        for (int i = 0; i < keys.size(); ++i)
            if (keys[i])
                archives_->addItem(QString("Archive %1").arg(i), i);
    } catch (const std::exception &e) {
        status_->setText(QString::fromUtf8(e.what()));
        archives_->setEnabled(false);
        return;
    }
    loadArchive();
}
void DialogueInspector::loadArchive() {
    archive_.reset();
    entries_->clear();
    source_->clear();
    setProperty("archiveSelector", -1);
    setProperty("entryIndex", -1);
    if (archives_->currentIndex() < 0) {
        status_->setText("No dialogue archives.");
        return;
    }
    try {
        const int selector = archives_->currentData().toInt();
        auto archive = DialogueArchive::load(rom_, selector);
        const QSignalBlocker block(entries_);
        for (int i = 0; i < archive.entryCount(); ++i) {
            const auto text = QString::fromLatin1(archive.entry(i));
            auto item =
                new QListWidgetItem(QString("%1  ·  %2").arg(i).arg(text.left(80)), entries_);
            item->setData(Qt::UserRole, text);
            item->setToolTip(text);
        }
        archive_ = std::move(archive);
        setProperty("archiveSelector", selector);
        entries_->setCurrentRow(0);
    } catch (const std::exception &e) {
        status_->setText(QString::fromUtf8(e.what()));
        return;
    }
    showEntry();
}
void DialogueInspector::showEntry() {
    source_->clear();
    setProperty("entryIndex", -1);
    if (!archive_ || entries_->currentRow() < 0)
        return;
    const int entry = entries_->currentRow();
    const auto bytes = archive_->entry(entry);
    source_->setPlainText(QString::fromLatin1(bytes));
    setProperty("entryIndex", entry);
    status_->setText(QString("%1 · archive %2 · entry %3 of %4 · %5 source bytes")
                         .arg(archive_->name())
                         .arg(archives_->currentData().toInt())
                         .arg(entry)
                         .arg(archive_->entryCount())
                         .arg(bytes.size()));
}
void DialogueInspector::selectEntry(int selector, int entry) {
    const int index = archives_->findData(selector);
    if (index < 0) {
        const QSignalBlocker block(archives_);
        archives_->setCurrentIndex(-1);
        archive_.reset();
        entries_->clear();
        source_->clear();
        setProperty("archiveSelector", -1);
        setProperty("entryIndex", -1);
        status_->setText(QString("Archive %1 is not present in the current ROM.").arg(selector));
        return;
    }
    if (archives_->currentIndex() == index)
        loadArchive();
    else
        archives_->setCurrentIndex(index);
    if (!archive_)
        return;
    if (entry < 0 || entry >= archive_->entryCount()) {
        entries_->setCurrentRow(-1);
        status_->setText(QString("Entry %1 is outside archive %2.").arg(entry).arg(selector));
        return;
    }
    if (auto search = findChild<QLineEdit *>("dialogueSearch"))
        search->clear();
    entries_->setCurrentRow(entry);
    entries_->scrollToItem(entries_->currentItem());
}
} // namespace lordly
