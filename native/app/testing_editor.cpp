#include "app/testing_editor.h"
#include "engine/project64_session.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QShowEvent>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>
namespace lordly {
namespace {
QByteArray readSave(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        throw Error("Cannot read " + path);
    if (f.size() > 0x10000)
        throw Error("Choose a battery save, not a savestate");
    return f.readAll();
}
} // namespace
TestingEditor::TestingEditor(const QString &romPath, QWidget *parent)
    : QWidget(parent), initialRom_(romPath) {
    setObjectName("testingEditor");
    QSettings settings;
    initialRom_ = settings.value("testing/rom", romPath).toString();
    project64_ = settings.value("testing/project64").toString();
    armyPath_ = settings.value("testing/army").toString();
    armySlot_ = settings.value("testing/armySlot", 0).toInt();
    auto layout = new QVBoxLayout(this);
    auto heading = new QLabel("<b>Scenario Seeder</b> · Create a battery save for a mission");
    layout->addWidget(heading);
    auto note = new QLabel(
        "Choose a ROM and Project64 folder, then use a mission's Cold boot button. It restarts "
        "Project64 with that save installed. Load slot 1 and "
        "travel to the marked destination. Export your edited ROM first to test its changes.");
    note->setWordWrap(true);
    layout->addWidget(note);
    controls_ = new QWidget;
    auto controls = new QVBoxLayout(controls_);
    controls->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(controls_);
    auto pathRow = [&](const QString &title, const char *id, QLabel *&label) {
        auto row = new QHBoxLayout;
        auto b = new QPushButton(title);
        b->setObjectName(id);
        row->addWidget(b);
        label = new QLabel;
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setWordWrap(true);
        row->addWidget(label, 1);
        controls->addLayout(row);
        return b;
    };
    auto rom = pathRow("Choose ROM…", "seederChooseRom", romLabel_);
    auto p64 = pathRow("Project64 folder…", "seederChooseProject64", p64Label_);
    p64Label_->setText(project64_.isEmpty() ? "Choose the folder containing Project64.exe"
                                            : QFileInfo(project64_).absolutePath());
    auto actions = new QHBoxLayout;
    controls->addLayout(actions);
    auto add = [&](QString title, const char *id) {
        auto b = new QPushButton(title);
        b->setObjectName(id);
        actions->addWidget(b);
        return b;
    };
    connect(add("Use my army", "seederUseArmy"), &QPushButton::clicked, this,
            &TestingEditor::chooseArmy);
    connect(add("Use my save game", "seederUseSave"), &QPushButton::clicked, this,
            &TestingEditor::useSave);
    connect(add("Edit Tester Army", "seederEditArmy"), &QPushButton::clicked, this, [this] {
        guarded([&] {
            if (!seeder_)
                throw Error("Choose a supported ROM first");
            emit editDefaultArmy(QFileInfo::exists(defaultArmyPath())
                                     ? readSave(defaultArmyPath())
                                     : seeder_->defaultArmyBytes());
        });
    });
    actions->addStretch();
    armyLabel_ = new QLabel;
    controls->addWidget(armyLabel_);
    search_ = new QLineEdit;
    search_->setObjectName("seederSearch");
    search_->setPlaceholderText("Find a world map or mission…");
    search_->setClearButtonEnabled(true);
    controls->addWidget(search_);
    missions_ = new QTreeWidget;
    missions_->setObjectName("seederMissions");
    missions_->setHeaderLabels({"World map", "Mission / story entry", "Launch"});
    missions_->setRootIsDecorated(false);
    missions_->setAlternatingRowColors(true);
    missions_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    missions_->header()->setStretchLastSection(false);
    missions_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    missions_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(missions_, 1);
    status_ = new QLabel("Use Cold boot beside the mission you want to test.");
    status_->setObjectName("seederStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    detail_ = new QLabel;
    detail_->setWordWrap(true);
    detail_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(detail_);
    connect(rom, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Choose the ROM to test", romPath_,
                                                       "N64 ROM (*.v64 *.z64 *.n64)");
        if (!path.isEmpty())
            loadRom(path);
    });
    connect(p64, &QPushButton::clicked, this, [this] {
        const auto dir =
            QFileDialog::getExistingDirectory(this, "Choose the folder containing Project64.exe",
                                              QFileInfo(project64_).absolutePath());
        if (dir.isEmpty())
            return;
        guarded([&] {
            const auto exe = QDir(dir).filePath("Project64.exe");
            if (!QFileInfo(exe).isFile())
                throw Error("That folder does not contain Project64.exe");
            project64_ = exe;
            QSettings().setValue("testing/project64", exe);
            p64Label_->setText(dir);
        });
    });
    connect(search_, &QLineEdit::textChanged, this, &TestingEditor::filter);
}
void TestingEditor::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (!initialized_) {
        initialized_ = true;
        loadRom(initialRom_);
    }
}
void TestingEditor::guarded(const std::function<void()> &action) {
    try {
        action();
    } catch (const std::exception &e) {
        status_->setText(QString::fromUtf8(e.what()));
    }
}
void TestingEditor::loadRom(const QString &path) {
    if (busy_)
        return;
    initialized_ = true;
    busy_ = true;
    controls_->setEnabled(false);
    missions_->setEnabled(false);
    missions_->clear();
    savePath_.clear();
    seeder_.reset();
    status_->setText("Reading ROM story routes…");
    struct Result {
        std::shared_ptr<ScenarioSeeder> seeder;
        QString error;
        qint64 size = 0, time = 0;
    };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, path] {
        auto r = watcher->result();
        watcher->deleteLater();
        busy_ = false;
        controls_->setEnabled(true);
        missions_->setEnabled(true);
        if (!r.seeder) {
            status_->setText(r.error);
            romLabel_->setText(path + " — unavailable");
            return;
        }
        seeder_ = std::move(r.seeder);
        romPath_ = path;
        QSettings().setValue("testing/rom", path);
        romSize_ = r.size;
        romTime_ = r.time;
        romLabel_->setText(path);
        for (int i = 0; i < seeder_->choices().size(); ++i) {
            const auto &c = seeder_->choices()[i];
            auto item = new QTreeWidgetItem(missions_, {c.location, c.mission});
            item->setData(0, Qt::UserRole, i);
            addLaunchButton(item);
        }
        armyLabel_->setText(armyPath_.isEmpty() ? "Army: default army"
                                                : "Army: " + QFileInfo(armyPath_).fileName());
        filter();
        status_->setText("Use Cold boot beside a mission to install its save and restart Project64.");
    });
    watcher->setFuture(QtConcurrent::run([path] {
        try {
            QFileInfo before(path);
            auto rom = Rom::open(path);
            auto result = std::make_shared<ScenarioSeeder>(std::move(rom));
            QFileInfo after(path);
            if (before.size() != after.size() || before.lastModified() != after.lastModified())
                throw Error("ROM changed while loading; choose it again");
            return Result{result, {}, after.size(), after.lastModified().toMSecsSinceEpoch()};
        } catch (const std::exception &e) {
            return Result{{}, QString::fromUtf8(e.what())};
        }
    }));
}
void TestingEditor::filter() {
    for (int i = 0; i < missions_->topLevelItemCount(); ++i) {
        auto item = missions_->topLevelItem(i);
        item->setHidden(
            !(item->text(0) + " " + item->text(1)).contains(search_->text(), Qt::CaseInsensitive));
    }
}
void TestingEditor::checkRom() const {
    if (!seeder_)
        throw Error("Choose a supported ROM first");
    QFileInfo file(romPath_);
    if (!file.isFile() || file.size() != romSize_ ||
        file.lastModified().toMSecsSinceEpoch() != romTime_)
        throw Error("The ROM changed on disk. Choose it again before seeding.");
}
QString TestingEditor::destination() const {
    checkRom();
    return ScenarioSeeder::batteryPath(seeder_->rom(), project64_);
}
QString TestingEditor::defaultArmyPath() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
           "/Testing/default-army.sra";
}
std::optional<SaveDocument> TestingEditor::army() const {
    const auto path = armyPath_.isEmpty() ? defaultArmyPath() : armyPath_;
    if (armyPath_.isEmpty() && !QFileInfo::exists(path))
        return {};
    auto doc = SaveDocument::open(seeder_->rom(), readSave(path));
    doc.selectSlot(armyPath_.isEmpty() ? 0 : armySlot_);
    return doc;
}
void TestingEditor::addLaunchButton(QTreeWidgetItem *item) {
    auto button = new QPushButton("Cold boot");
    button->setObjectName("seederColdBoot");
    button->setToolTip("Close Project64, install this row's battery save, and start the selected ROM fresh.");
    missions_->setItemWidget(item, 2, button);
    connect(button, &QPushButton::clicked, this, [this, item] { coldBoot(item); });
}
void TestingEditor::coldBoot(QTreeWidgetItem *item) {
    if (busy_ || !item || item->isHidden())
        return;
    guarded([&] {
        const auto dest = destination();
        const int i = item->data(0, Qt::UserRole).toInt();
        const bool merge = i >= 0;
        QByteArray raw;
        if (merge) {
            auto donor = army();
            raw = seeder_->seed(i, donor ? &*donor : nullptr);
        } else {
            raw = readSave(savePath_);
            if (!SaveDocument::open(seeder_->rom(), raw).battery())
                throw Error("Choose a battery save");
        }
        const auto title = item->text(0) + " · " + item->text(1);
        missions_->setCurrentItem(item);
        busy_ = true;
        controls_->setEnabled(false);
        missions_->setEnabled(false);
        detail_->clear();
        status_->setText("Closing Project64 and preparing " + title + "…");
        struct Result { QString error, backup; bool installed = false; };
        auto watcher = new QFutureWatcher<Result>(this);
        connect(watcher, &QFutureWatcher<Result>::finished, this,
                [this, watcher, dest, title, merge] {
            const auto result = watcher->result();
            watcher->deleteLater();
            busy_ = false;
            controls_->setEnabled(true);
            missions_->setEnabled(true);
            status_->setText(result.error.isEmpty()
                ? "Started: " + title + (merge ? ". Load slot 1 and travel to the marked destination."
                                              : ". Load your saved game.")
                : result.error);
            if (result.installed)
                detail_->setText("Saved: " + dest + (result.backup.isEmpty() ? QString()
                                                           : "\nPrevious save: " + result.backup));
        });
        watcher->setFuture(QtConcurrent::run([seeder = seeder_, exe = project64_, romPath = romPath_,
                                               size = romSize_, time = romTime_, raw, dest, merge] {
            Result result;
            try {
                closeProject64(exe);
                const auto check = [&] {
                    const QFileInfo rom(romPath);
                    if (!rom.isFile() || rom.size() != size || rom.lastModified().toMSecsSinceEpoch() != time)
                        throw Error("The ROM changed on disk. Choose it again before seeding.");
                };
                check();
                result.backup = ScenarioSeeder::install(seeder->rom(), raw, dest, merge);
                result.installed = true;
                check();
                if (ScenarioSeeder::project64Running())
                    throw Error("Project64 opened during installation. Close it and try Cold boot again.");
                if (!QProcess::startDetached(exe, {romPath}, QFileInfo(exe).absolutePath()))
                    throw Error("Save installed, but Project64 could not start. Check the selected Project64 folder.");
            } catch (const std::exception &e) { result.error = QString::fromUtf8(e.what()); }
            return result;
        }));
    });
}
void TestingEditor::chooseArmy() {
    guarded([&] {
        checkRom();
        const auto path = QFileDialog::getOpenFileName(this, "Use the army from a battery save", {},
                                                       "Battery saves (*.sra *.SaveRAM)");
        if (path.isEmpty())
            return;
        auto doc = SaveDocument::open(seeder_->rom(), readSave(path));
        if (!doc.battery())
            throw Error("Choose a battery save");
        QStringList labels;
        QVector<int> validSlots;
        for (const auto &s : doc.saveSlots())
            if (s.valid) {
                labels << s.name;
                validSlots << s.id;
            }
        int slot = validSlots.first();
        if (validSlots.size() > 1) {
            bool ok = false;
            const auto selected =
                QInputDialog::getItem(this, "Which army?", "Save slot", labels, 0, false, &ok);
            if (!ok)
                return;
            slot = validSlots[labels.indexOf(selected)];
        }
        doc.selectSlot(slot);
        seeder_->seed(0, &doc);
        armyPath_ = path;
        armySlot_ = slot;
        QSettings settings;
        settings.setValue("testing/army", path);
        settings.setValue("testing/armySlot", slot);
        armyLabel_->setText("Army: " + QFileInfo(path).fileName());
        status_->setText("Army selected. Use a mission's Cold boot button to test it with this army.");
    });
}
void TestingEditor::setDefaultArmy(const QByteArray &bytes) {
    checkRom();
    auto doc = SaveDocument::open(seeder_->rom(), bytes);
    doc.selectSlot(0);
    seeder_->seed(0, &doc);
    const auto path = defaultArmyPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        throw Error("Cannot create default army settings folder");
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
        throw Error("Cannot save the default army");
    armyPath_.clear();
    QSettings().remove("testing/army");
    armyLabel_->setText("Army: edited default army");
    status_->setText("Tester army saved. Use a mission's Cold boot button to use it.");
}
void TestingEditor::useSave() {
    guarded([&] {
        checkRom();
        const auto path = QFileDialog::getOpenFileName(this, "Choose your complete battery save",
                                                       {}, "Project64 battery save (*.sra)");
        if (path.isEmpty())
            return;
        loadSave(path);
    });
}
void TestingEditor::loadSave(const QString &path) {
    if (busy_) return;
    guarded([&] {
        checkRom();
        if (!SaveDocument::open(seeder_->rom(), readSave(path)).battery())
            throw Error("Choose a battery save");
        if (!savePath_.isEmpty())
            delete missions_->takeTopLevelItem(0);
        savePath_ = path;
        auto item = new QTreeWidgetItem({"My save game", QFileInfo(path).fileName()});
        item->setData(0, Qt::UserRole, -1);
        item->setToolTip(1, path);
        missions_->insertTopLevelItem(0, item);
        addLaunchButton(item);
        search_->clear();
        missions_->setCurrentItem(item);
        missions_->scrollToItem(item);
        status_->setText("Use Cold boot beside My save game to install the complete save unchanged.");
    });
}
} // namespace lordly
