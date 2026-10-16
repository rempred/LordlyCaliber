#include "app/testing_editor.h"
#include "engine/project64_session.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGroupBox>
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
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <utility>
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
    project64_ = settings.value("testing/project64").toString();
    armyPath_ = settings.value("testing/army").toString();
    armySlot_ = settings.value("testing/armySlot", 0).toInt();
    savePath_ = settings.value("testing/completeSave").toString();
    auto layout = new QVBoxLayout(this);
    auto heading = new QLabel("<b>Test your project</b>");
    layout->addWidget(heading);
    auto note = new QLabel(
        "Build your edits and assign a mission or complete saved game to that ROM. "
        "The ROM and assigned battery save stay on disk: open the same ROM directly in this "
        "Project64 installation next time. "
        "Seeding again replaces its saved progress; the previous battery save is backed up.");
    note->setWordWrap(true);
    layout->addWidget(note);
    controls_ = new QWidget;
    auto controls = new QVBoxLayout(controls_);
    controls->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(controls_);
    auto sourceRow = new QHBoxLayout;
    sourceRow->addWidget(new QLabel("Test source"));
    sourceMode_ = new QComboBox;
    sourceMode_->setObjectName("testingSourceMode");
    sourceMode_->addItems({"Current project — build automatically", "Existing ROM file"});
    sourceRow->addWidget(sourceMode_);
    sourceRow->addStretch();
    controls->addLayout(sourceRow);
    freshness_ = new QLabel;
    freshness_->setObjectName("testingFreshness");
    freshness_->setWordWrap(true);
    controls->addWidget(freshness_);
    auto outputRow = new QHBoxLayout;
    auto outputLocation = new QPushButton("Test ROM location…");
    outputLocation->setObjectName("testingBuildLocation");
    outputRow->addWidget(outputLocation);
    buildPathLabel_ = new QLabel;
    buildPathLabel_->setObjectName("testingBuildPath");
    buildPathLabel_->setWordWrap(true);
    buildPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    outputRow->addWidget(buildPathLabel_, 1);
    controls->addLayout(outputRow);
    connect(outputLocation, &QPushButton::clicked, this, &TestingEditor::buildLocationRequested);
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
    auto rom = pathRow("Choose existing ROM…", "seederChooseRom", romLabel_);
    rom->setVisible(false);
    romLabel_->setVisible(false);
    auto p64 = pathRow("Project64 folder…", "seederChooseProject64", p64Label_);
    p64Label_->setText(project64_.isEmpty() ? "Choose the folder containing Project64.exe"
                                            : QFileInfo(project64_).absolutePath());
    auto launchRow = new QHBoxLayout;
    launchAfterSeed_ = new QCheckBox("Also launch Project64 now (cold boot)");
    launchAfterSeed_->setObjectName("testingLaunchAfterSeed");
    launchAfterSeed_->setChecked(true);
    launchRow->addWidget(launchAfterSeed_);
    launchRow->addStretch();
    auto openFolder = new QPushButton("Open ROM folder");
    openFolder->setObjectName("testingOpenRomFolder");
    launchRow->addWidget(openFolder);
    controls->addLayout(launchRow);
    connect(openFolder, &QPushButton::clicked, this, [this] {
        const auto path = sourceMode_->currentIndex() == 0 ? buildPathLabel_->text() : romPath_;
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
    auto savedGame = new QGroupBox("Assign my complete saved game to this ROM");
    savedGame->setObjectName("testingCompleteSave");
    auto savedLayout = new QVBoxLayout(savedGame);
    auto savedHelp = new QLabel("Assign your entire save to this ROM: army, inventory and story "
                                "progress. Then load it normally in Project64, now or later. The "
                                "mission list below does not affect this assignment.");
    savedHelp->setWordWrap(true);
    savedLayout->addWidget(savedHelp);
    auto savedActions = new QHBoxLayout;
    auto chooseSave = new QPushButton("Choose complete save…");
    chooseSave->setObjectName("seederUseSave");
    savedActions->addWidget(chooseSave);
    saveLabel_ = new QLabel(savePath_.isEmpty() ? "No saved game selected"
                                                : QFileInfo(savePath_).fileName());
    saveLabel_->setObjectName("testingCompleteSaveName");
    saveLabel_->setToolTip(savePath_);
    saveLabel_->setWordWrap(true);
    savedActions->addWidget(saveLabel_, 1);
    saveLaunch_ = new QPushButton;
    saveLaunch_->setObjectName("testingLaunchSave");
    savedActions->addWidget(saveLaunch_);
    savedLayout->addLayout(savedActions);
    controls->addWidget(savedGame);
    connect(chooseSave, &QPushButton::clicked, this, &TestingEditor::useSave);
    connect(saveLaunch_, &QPushButton::clicked, this, [this] { requestLaunch(-1); });

    auto missionArmy = new QGroupBox("Test any mission with an army");
    auto armyLayout = new QVBoxLayout(missionArmy);
    auto armyHelp =
        new QLabel("Use your saved army in any mission below, or use the tester army. Seeding a "
                   "mission creates the story progress needed to reach it and assigns that save to "
                   "the ROM. Load slot 1 in game, then travel to the marked destination.");
    armyHelp->setWordWrap(true);
    armyLayout->addWidget(armyHelp);
    auto actions = new QHBoxLayout;
    armyLayout->addLayout(actions);
    controls->addWidget(missionArmy);
    auto add = [&](const QString &title, const char *id) {
        auto b = new QPushButton(title);
        b->setObjectName(id);
        actions->addWidget(b);
        return b;
    };
    connect(add("Use my saved army in missions…", "seederUseArmy"), &QPushButton::clicked, this,
            &TestingEditor::chooseArmy);
    connect(add("Use tester army", "seederDefaultArmy"), &QPushButton::clicked, this, [this] {
        armyPath_.clear();
        QSettings().remove("testing/army");
        updateArmyLabel();
    });
    connect(add("Edit tester army…", "seederEditArmy"), &QPushButton::clicked, this, [this] {
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
    armyLabel_->setObjectName("testingArmyName");
    armyLabel_->setWordWrap(true);
    armyLayout->addWidget(armyLabel_);
    search_ = new QLineEdit;
    search_->setObjectName("seederSearch");
    search_->setPlaceholderText("Find a world map or mission…");
    search_->setClearButtonEnabled(true);
    controls->addWidget(search_);
    missions_ = new QTreeWidget;
    missions_->setObjectName("seederMissions");
    missions_->setHeaderLabels({"World map", "Mission / story entry", "Assign to ROM"});
    missions_->setRootIsDecorated(false);
    missions_->setAlternatingRowColors(true);
    missions_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    missions_->header()->setStretchLastSection(false);
    missions_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    missions_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(missions_, 1);
    status_ = new QLabel("Choose a mission below or assign your complete saved game above.");
    status_->setObjectName("seederStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    detail_ = new QLabel;
    detail_->setObjectName("testingAssignmentDetails");
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
    connect(sourceMode_, &QComboBox::currentIndexChanged, this,
            [this, rom, outputLocation](int index) {
                rom->setVisible(index == 1);
                romLabel_->setVisible(index == 1);
                outputLocation->setVisible(index == 0);
                buildPathLabel_->setVisible(index == 0);
                updateFreshness();
                updateLaunchControls();
            });
    updateFreshness();
    updateLaunchControls();
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
        emit assignmentFinished(status_->text());
    }
}
void TestingEditor::loadRom(const QString &path) {
    if (busy_) {
        pendingRom_ = path;
        return;
    }
    initialized_ = true;
    busy_ = true;
    controls_->setEnabled(false);
    missions_->setEnabled(false);
    missions_->clear();
    seeder_.reset();
    status_->setText("Reading ROM story routes…");
    struct Result {
        std::shared_ptr<ScenarioSeeder> seeder;
        QString error;
        qint64 size = 0, time = 0;
        QByteArray digest;
    };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, path] {
        auto r = watcher->result();
        watcher->deleteLater();
        busy_ = false;
        controls_->setEnabled(true);
        missions_->setEnabled(true);
        if (!pendingRom_.isEmpty()) {
            const auto pending = std::exchange(pendingRom_, {});
            loadRom(pending);
            return;
        }
        if (!r.seeder) {
            pendingLaunch_ = -2;
            status_->setText(r.error);
            emit assignmentFinished(r.error);
            romLabel_->setText(path + " — unavailable");
            updateLaunchControls();
            return;
        }
        seeder_ = std::move(r.seeder);
        romPath_ = path;
        QSettings().setValue("testing/rom", path);
        romSize_ = r.size;
        romTime_ = r.time;
        loadedDigest_ = r.digest;
        romLabel_->setText(path);
        updateFreshness();
        for (int i = 0; i < seeder_->choices().size(); ++i) {
            const auto &c = seeder_->choices()[i];
            auto item = new QTreeWidgetItem(missions_, {c.location, c.mission});
            item->setData(0, Qt::UserRole, i);
            addLaunchButton(item);
        }
        updateArmyLabel();
        const int requested = requestedScenario_;
        filter();
        status_->setText("Choose a mission below or assign your complete saved game above.");
        if (requested >= 0)
            selectScenario(requested);
        updateLaunchControls();
        if (pendingLaunch_ != -2) {
            int choice = std::exchange(pendingLaunch_, -2);
            if (choice >= 0) {
                // Scenario edits can change the wiki-order sort. Keep the story
                // entry the user chose, rather than launching its old row index.
                choice = -1;
                for (int i = 0; i < seeder_->choices().size(); ++i) {
                    const auto &route = seeder_->choices()[i].route;
                    if (route.node == pendingNode_ && route.event == pendingEvent_) {
                        choice = i;
                        break;
                    }
                }
                if (choice < 0) {
                    buildFailed("That story entry is no longer available in the rebuilt ROM. "
                                "Choose a mission from the updated list; no save was assigned.");
                    return;
                }
            }
            launchPrepared(choice);
        }
    });
    watcher->setFuture(QtConcurrent::run([path] {
        try {
            QFileInfo before(path);
            auto rom = Rom::open(path);
            auto result = std::make_shared<ScenarioSeeder>(std::move(rom));
            QFileInfo after(path);
            if (before.size() != after.size() || before.lastModified() != after.lastModified())
                throw Error("ROM changed while loading; choose it again");
            return Result{result,
                          {},
                          after.size(),
                          after.lastModified().toMSecsSinceEpoch(),
                          sha256(result->rom().bytes())};
        } catch (const std::exception &e) {
            return Result{{}, QString::fromUtf8(e.what())};
        }
    }));
}
void TestingEditor::useExport(const QString &path, quint64 revision, const QByteArray &digest) {
    exportPath_ = QFileInfo(path).absoluteFilePath();
    exportRevision_ = revision;
    exportDigest_ = digest;
    loadRom(path);
}
void TestingEditor::useTestBuild(const QString &path, quint64 revision, const QByteArray &digest,
                                 int choice) {
    pendingLaunch_ = choice;
    pendingNode_ = pendingEvent_ = -1;
    if (seeder_ && choice >= 0 && choice < seeder_->choices().size()) {
        const auto &route = seeder_->choices()[choice].route;
        pendingNode_ = route.node;
        pendingEvent_ = route.event;
    }
    useExport(path, revision, digest);
}
void TestingEditor::setBuildPath(const QString &path) {
    buildPathLabel_->setText(path);
    exportPath_.clear();
    exportDigest_.clear();
    updateFreshness();
}
void TestingEditor::buildFailed(const QString &message) {
    pendingLaunch_ = -2;
    status_->setText(message);
    emit assignmentFinished(message);
}
void TestingEditor::setProjectRevision(quint64 revision, bool pending) {
    projectRevision_ = revision;
    pendingEdits_ = pending;
    updateFreshness();
}
bool TestingEditor::matchesExport() const {
    const QFileInfo file(romPath_);
    return seeder_ && !exportPath_.isEmpty() && file.absoluteFilePath() == exportPath_ &&
           (buildPathLabel_->text().isEmpty() ||
            file.absoluteFilePath() == QFileInfo(buildPathLabel_->text()).absoluteFilePath()) &&
           !exportDigest_.isEmpty() && loadedDigest_ == exportDigest_ && file.size() == romSize_ &&
           file.lastModified().toMSecsSinceEpoch() == romTime_;
}
bool TestingEditor::currentProjectReady() const {
    return !busy_ && matchesExport() && projectRevision_ == exportRevision_ && !pendingEdits_;
}
void TestingEditor::updateFreshness() {
    if (sourceMode_->currentIndex() == 1) {
        freshness_->setText(
            "Testing the selected ROM file. Current project edits are not built in this mode.");
        return;
    }
    freshness_->setText(
        matchesExport()
            ? (projectRevision_ != exportRevision_ || pendingEdits_
                   ? "Project changed since the last build. Seeding will rebuild with your current "
                     "edits."
                   : "Latest build is ready. Seeding will reuse it until you make another edit.")
            : "Seeding builds the current project at the test ROM location below. No separate "
              "export or ROM selection is needed.");
}
void TestingEditor::updateLaunchControls() {
    const bool project = sourceMode_->currentIndex() == 0;
    saveLaunch_->setText(project ? "Build and assign save" : "Assign save to ROM");
    saveLaunch_->setEnabled(!busy_ && seeder_ && !savePath_.isEmpty());
    for (int row = 0; row < missions_->topLevelItemCount(); ++row)
        if (auto button =
                qobject_cast<QPushButton *>(missions_->itemWidget(missions_->topLevelItem(row), 2)))
            button->setText(project ? "Build and seed mission" : "Seed mission");
}
void TestingEditor::updateArmyLabel() {
    armyLabel_->setText(armyPath_.isEmpty() ? (QFileInfo::exists(defaultArmyPath())
                                                   ? "Mission army: your edited tester army"
                                                   : "Mission army: built-in tester army")
                                            : QString("Mission army: %1 · save slot %2")
                                                  .arg(QFileInfo(armyPath_).fileName())
                                                  .arg(armySlot_ + 1));
    armyLabel_->setToolTip(armyPath_.isEmpty() ? defaultArmyPath() : armyPath_);
}
void TestingEditor::selectScenario(int key) {
    requestedScenario_ = key;
    if (!seeder_)
        return;
    search_->clear();
    requestedScenario_ = key;
    QTreeWidgetItem *first = nullptr;
    for (int row = 0; row < missions_->topLevelItemCount(); ++row) {
        auto item = missions_->topLevelItem(row);
        const int choice = item->data(0, Qt::UserRole).toInt();
        const bool match = choice >= 0 && choice < seeder_->choices().size() &&
                           seeder_->choices()[choice].scenarios.contains(key);
        item->setHidden(!match);
        if (match && !first)
            first = item;
    }
    missions_->setCurrentItem(first);
    if (first) {
        missions_->scrollToItem(first);
        status_->setText(QString("Showing story entries for scenario %1. Launch a mission, then "
                                 "load slot 1 and travel to the marked destination. Story choices "
                                 "may select a different mission variant.")
                             .arg(key));
    } else
        status_->setText(QString("Scenario %1 has no supported world-map entry in this ROM. Search "
                                 "for another mission, or use your own save.")
                             .arg(key));
}
void TestingEditor::filter() {
    requestedScenario_ = -1;
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
        throw Error("The ROM changed on disk. Select the file again, or use Current project to "
                    "rebuild automatically.");
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
    auto button = new QPushButton(sourceMode_->currentIndex() == 0 ? "Build and seed mission"
                                                                   : "Seed mission");
    button->setObjectName("seederColdBoot");
    button->setToolTip("Test this mission with the selected mission army. Current project mode "
                       "builds your latest edits first; unchanged builds are reused.");
    missions_->setItemWidget(item, 2, button);
    connect(button, &QPushButton::clicked, this, [this, item] {
        if (!item->isHidden())
            requestLaunch(item->data(0, Qt::UserRole).toInt());
    });
}
void TestingEditor::requestLaunch(int choice) {
    if (busy_ || !seeder_)
        return;
    guarded([&] {
        if (choice < -1 || choice >= seeder_->choices().size())
            throw Error("Choose a supported mission");
        if (!QFileInfo(project64_).isFile())
            throw Error("Choose the folder containing Project64.exe first.");
        if (choice == -1 && savePath_.isEmpty())
            throw Error("Choose your complete saved game first.");
        if (sourceMode_->currentIndex() == 0) {
            status_->setText("Preparing the current project for testing…");
            emit testProjectRequested(choice);
        } else
            launchPrepared(choice);
    });
}
void TestingEditor::launchPrepared(int choice) {
    if (busy_)
        return;
    guarded([&] {
        if (sourceMode_->currentIndex() == 0 && !currentProjectReady())
            throw Error("Project changed while preparing the test. Launch again to build the "
                        "current edits.");
        const auto dest = destination();
        if (choice < -1 || choice >= seeder_->choices().size())
            throw Error("The requested mission is no longer available.");
        const bool merge = choice >= 0;
        QByteArray raw;
        if (merge) {
            auto donor = army();
            raw = seeder_->seed(choice, donor ? &*donor : nullptr);
        } else {
            raw = readSave(savePath_);
            if (!SaveDocument::open(seeder_->rom(), raw).battery())
                throw Error("Choose a battery save");
        }
        const auto title =
            merge ? seeder_->choices()[choice].location + " · " + seeder_->choices()[choice].mission
                  : "My saved game · " + QFileInfo(savePath_).fileName();
        if (merge)
            missions_->setCurrentItem(missions_->topLevelItem(choice));
        busy_ = true;
        controls_->setEnabled(false);
        missions_->setEnabled(false);
        detail_->clear();
        status_->setText("Assigning the battery save for " + title + "…");
        const bool start = launchAfterSeed_->isChecked();
        struct Result {
            QString error, backup;
            bool installed = false;
        };
        auto watcher = new QFutureWatcher<Result>(this);
        connect(watcher, &QFutureWatcher<Result>::finished, this,
                [this, watcher, dest, title, merge, start] {
                    const auto result = watcher->result();
                    watcher->deleteLater();
                    busy_ = false;
                    controls_->setEnabled(true);
                    missions_->setEnabled(true);
                    status_->setText(
                        result.error.isEmpty()
                            ? (start ? "Started: " : "Save assigned: ") + title +
                                  (merge ? ". Load slot 1 and travel to the marked destination."
                                         : ". Load your saved game normally.")
                            : result.error);
                    if (result.installed)
                        detail_->setText("ROM: " + romPath_ + "\nAssigned battery save: " + dest +
                                         "\nOpen this ROM directly in the same Project64 "
                                         "installation next time; the editor is not required." +
                                         (result.backup.isEmpty()
                                              ? QString()
                                              : "\nPrevious save backup: " + result.backup));
                    emit assignmentFinished(status_->text());
                    if (!pendingRom_.isEmpty())
                        loadRom(std::exchange(pendingRom_, {}));
                });
        watcher->setFuture(QtConcurrent::run([seeder = seeder_, exe = project64_,
                                              romPath = romPath_, size = romSize_, time = romTime_,
                                              raw, dest, merge, start, launcher = launcher_] {
            Result result;
            try {
                const auto check = [&] {
                    const QFileInfo rom(romPath);
                    if (!rom.isFile() || rom.size() != size ||
                        rom.lastModified().toMSecsSinceEpoch() != time)
                        throw Error("The ROM changed on disk. Choose it again before seeding.");
                };
                check();
                if (launcher) {
                    result.backup = launcher(seeder->rom(), romPath, exe, raw, dest, merge, start);
                    result.installed = true;
                    return result;
                }
                closeProject64(exe);
                check();
                result.backup = ScenarioSeeder::install(seeder->rom(), raw, dest, merge);
                result.installed = true;
                check();
                if (ScenarioSeeder::project64Running())
                    throw Error(
                        "Project64 opened during installation. Close it and try Cold boot again.");
                if (start &&
                    !QProcess::startDetached(exe, {romPath}, QFileInfo(exe).absolutePath()))
                    throw Error("Save installed, but Project64 could not start. Check the selected "
                                "Project64 folder.");
            } catch (const std::exception &e) {
                result.error = QString::fromUtf8(e.what());
            }
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
        if (validSlots.isEmpty())
            throw Error("This battery save has no valid army slot.");
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
        updateArmyLabel();
        status_->setText("Saved army selected for every mission below. Your complete saved-game "
                         "option is unchanged.");
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
    updateArmyLabel();
    status_->setText("Tester army saved and selected for missions below.");
}
void TestingEditor::useSave() {
    guarded([&] {
        checkRom();
        const auto path = QFileDialog::getOpenFileName(this, "Choose your complete battery save",
                                                       savePath_, "Project64 battery save (*.sra)");
        if (path.isEmpty())
            return;
        loadSave(path);
    });
}
void TestingEditor::loadSave(const QString &path) {
    if (busy_)
        return;
    guarded([&] {
        checkRom();
        if (!SaveDocument::open(seeder_->rom(), readSave(path)).battery())
            throw Error("Choose a battery save");
        savePath_ = path;
        QSettings().setValue("testing/completeSave", path);
        saveLabel_->setText(QFileInfo(path).fileName());
        saveLabel_->setToolTip(path);
        updateLaunchControls();
        status_->setText("Complete save selected. Use its assignment button above; launching "
                         "Project64 now is optional.");
    });
}
} // namespace lordly
