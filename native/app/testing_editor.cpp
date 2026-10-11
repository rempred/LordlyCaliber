#include "app/testing_editor.h"
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
        "Choose a ROM and Project64 folder, close Project64, then click a mission. Load slot 1 and "
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
    connect(add("Edit Default Army", "seederEditArmy"), &QPushButton::clicked, this, [this] {
        guarded([&] {
            if (!seeder_)
                throw Error("Choose a supported ROM first");
            emit editDefaultArmy(QFileInfo::exists(defaultArmyPath())
                                     ? readSave(defaultArmyPath())
                                     : seeder_->defaultArmyBytes());
        });
    });
    actions->addStretch();
    connect(add("Open ROM in Project64", "seederLaunch"), &QPushButton::clicked, this,
            &TestingEditor::launch);
    armyLabel_ = new QLabel;
    controls->addWidget(armyLabel_);
    search_ = new QLineEdit;
    search_->setObjectName("seederSearch");
    search_->setPlaceholderText("Find a world map or mission…");
    search_->setClearButtonEnabled(true);
    controls->addWidget(search_);
    missions_ = new QTreeWidget;
    missions_->setObjectName("seederMissions");
    missions_->setHeaderLabels({"World map", "Mission / story entry"});
    missions_->setRootIsDecorated(false);
    missions_->setAlternatingRowColors(true);
    missions_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    missions_->header()->setStretchLastSection(true);
    layout->addWidget(missions_, 1);
    status_ = new QLabel("Choose a mission to prepare its save.");
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
    connect(missions_, &QTreeWidget::itemClicked, this, [this] { installSelected(); });
    connect(missions_, &QTreeWidget::itemActivated, this, [this] { installSelected(); });
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
        }
        armyLabel_->setText(armyPath_.isEmpty() ? "Army: default army"
                                                : "Army: " + QFileInfo(armyPath_).fileName());
        filter();
        status_->setText("Click a mission to create and install its battery save.");
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
void TestingEditor::installSelected() {
    if (busy_ || !missions_->currentItem() || missions_->currentItem()->isHidden())
        return;
    guarded([&] {
        const auto dest = destination();
        auto donor = army();
        const int i = missions_->currentItem()->data(0, Qt::UserRole).toInt();
        auto raw = seeder_->seed(i, donor ? &*donor : nullptr);
        const auto backup = ScenarioSeeder::install(seeder_->rom(), raw, dest, true);
        status_->setText("Ready: " + seeder_->choices()[i].location +
                         ". Open the ROM, load slot 1, and travel to the marked destination.");
        detail_->setText("Saved: " + dest +
                         (backup.isEmpty() ? QString() : "\nPrevious save: " + backup));
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
        status_->setText("Army selected. Click a mission to install it with this army.");
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
    status_->setText("Default army saved. Click a mission to use it.");
}
void TestingEditor::useSave() {
    guarded([&] {
        const auto dest = destination();
        const auto path = QFileDialog::getOpenFileName(this, "Install your complete battery save",
                                                       {}, "Project64 battery save (*.sra)");
        if (path.isEmpty())
            return;
        if (QFileInfo(path).absoluteFilePath() == QFileInfo(dest).absoluteFilePath())
            throw Error("This save is already installed");
        const auto backup = ScenarioSeeder::install(seeder_->rom(), readSave(path), dest, false);
        missions_->clearSelection();
        missions_->setCurrentItem(nullptr);
        status_->setText(
            "Your full save is installed unchanged. Open the ROM and load your saved game.");
        detail_->setText("Saved: " + dest +
                         (backup.isEmpty() ? QString() : "\nPrevious save: " + backup));
    });
}
void TestingEditor::launch() {
    guarded([&] {
        destination();
        if (ScenarioSeeder::project64Running())
            throw Error("Project64 is already open");
        if (!QProcess::startDetached(project64_, {romPath_}, QFileInfo(project64_).absolutePath()))
            throw Error("Could not open Project64");
    });
}
} // namespace lordly
