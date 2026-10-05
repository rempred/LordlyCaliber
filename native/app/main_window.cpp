#include "app/main_window.h"
#include "app/art_editor.h"
#include "app/portrait_editor.h"
#include "app/army_editor.h"
#include "app/combat_editor.h"
#include "app/animation_editor.h"
#include "app/consumable_editor.h"
#include "app/cutscene_editor.h"
#include "app/encounter_editor.h"
#include "app/promotion_editor.h"
#include "app/save_editor.h"
#include "app/sprite_editor.h"
#include "app/scenario_editor.h"
#include "app/shop_editor.h"
#include "app/text_workspace.h"
#include "app/tools_editor.h"
#include "app/value_editor.h"
#include "rebuild/compiler.h"
#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolBar>
#include <QUndoGroup>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <optional>

namespace lordly {
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), undo_(new QUndoStack(this)), workspaces_(new QTabWidget(this)),
      summary_(new QLabel(this)) {
    setWindowTitle("Lordly Caliber · Native Editor");
    resize(1440, 900);
    setMinimumSize(900, 600);
    setCentralWidget(workspaces_);
    undoGroup_ = new QUndoGroup(this);
    undoGroup_->addStack(undo_);
    undoGroup_->setActiveStack(undo_);
    connect(workspaces_, &QTabWidget::currentChanged, this, [this] {
        const auto save = qobject_cast<SaveEditor *>(workspaces_->currentWidget());
        undoGroup_->setActiveStack(save ? save->undoStack() : undo_);
    });
    QFile catalog(":/catalog.json");
    if (catalog.open(QIODevice::ReadOnly))
        catalog_ = QJsonDocument::fromJson(catalog.readAll()).object();
    auto file = menuBar()->addMenu("&File");
    auto open = file->addAction("Open &ROM…", this, [this] {
        if (!confirmDiscard())
            return;
        auto path = QFileDialog::getOpenFileName(this, "Open Ogre Battle 64 ROM", {},
                                                 "N64 ROM (*.v64 *.z64 *.n64);;All files (*)");
        if (!path.isEmpty())
            loadRom(path);
    });
    open->setShortcut(QKeySequence::Open);
    file->addAction("Open &project…", this, &MainWindow::openProject);
    auto save = file->addAction("Save project &folder…", this, [this] { saveProject(false); });
    save->setShortcut(QKeySequence::Save);
    file->addAction("Save sharing &archive…", this, [this] { saveProject(true); });
    auto build = file->addAction("Build and &export ROM…", this, &MainWindow::buildRom);
    file->addSeparator();
    file->addAction("E&xit", this, &QWidget::close);
    auto edit = menuBar()->addMenu("&Edit");
    auto undoAction = undoGroup_->createUndoAction(this, "&Undo");
    undoAction->setShortcut(QKeySequence::Undo);
    edit->addAction(undoAction);
    auto redoAction = undoGroup_->createRedoAction(this, "&Redo");
    redoAction->setShortcut(QKeySequence::Redo);
    edit->addAction(redoAction);
    auto toolbar = addToolBar("Project");
    toolbar->setMovable(false);
    toolbar->addAction(open);
    toolbar->addAction(save);
    toolbar->addAction(build);
    toolbar->addSeparator();
    toolbar->addWidget(summary_);
    auto welcome =
        new QLabel("<h1>Lordly Caliber</h1><p>Open your Ogre Battle 64 ROM to begin.</p><p>Your "
                   "source ROM stays unchanged. Save your work as a project.</p>");
    welcome->setAlignment(Qt::AlignCenter);
    workspaces_->addTab(welcome, "Workspace");
    connect(undo_, &QUndoStack::cleanChanged, this, [this] { updateStatus(); });
    updateStatus();
}
bool MainWindow::confirmDiscard() {
    const auto save = workspaces_->findChild<SaveEditor *>();
    if (undo_->isClean() && !invalidEdits() && (!save || !save->modified()))
        return true;
    return QMessageBox::question(this, "Unsaved edits", "Discard unsaved edits?",
                                 QMessageBox::Discard | QMessageBox::Cancel,
                                 QMessageBox::Cancel) == QMessageBox::Discard;
}
MainWindow::~MainWindow() {
    // Workspaces reference project_, whose C++ member lifetime otherwise ends
    // before QWidget deletes its children.
    disconnect(undo_, nullptr, this, nullptr);
    disconnect(workspaces_, nullptr, this, nullptr);
    undoGroup_->setActiveStack(nullptr);
    undo_->clear();
    while (workspaces_->count()) {
        auto page = workspaces_->widget(0);
        workspaces_->removeTab(0);
        delete page;
    }
}
void MainWindow::closeEvent(QCloseEvent *e) {
    if (loading_ || !confirmDiscard())
        e->ignore();
    else
        e->accept();
}
void MainWindow::report(const QString &text) {
    QMessageBox::warning(this, "Lordly Caliber", text);
}
void MainWindow::loadRom(const QString &path) {
    if (loading_)
        return;
    loading_ = true;
    statusBar()->showMessage("Reading ROM and building the project…");
    workspaces_->setEnabled(false);
    struct Result {
        std::optional<Project> project;
        QString error;
    };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher] {
        auto result = watcher->result();
        watcher->deleteLater();
        loading_ = false;
        workspaces_->setEnabled(true);
        if (result.project) {
            installProject(std::move(*result.project));
            statusBar()->showMessage("Ready");
        } else {
            report(result.error);
            statusBar()->showMessage("ROM could not be loaded");
        }
        updateStatus();
    });
    watcher->setFuture(QtConcurrent::run([path, catalog = catalog_] {
        try {
            auto project = Project::fromRom(Rom::open(path), catalog);
            project.loadTexts();
            if (project.rom().revision() == 0) {
                project.loadPromotion();
                project.loadShops();
                project.loadConsumableEffects();
                project.loadEncounters();
                project.loadScenarios();
                project.loadWorld();
            }
            return Result{std::move(project), {}};
        } catch (const std::exception &e) {
            return Result{{}, QString::fromUtf8(e.what())};
        }
    }));
}
void MainWindow::installProject(Project project) {
    undo_->clear();
    while (workspaces_->count()) {
        auto old = workspaces_->widget(0);
        workspaces_->removeTab(0);
        delete old;
    }
    project_ = std::make_unique<Project>(std::move(project));
    addRecordWorkspace(Table::Classes, "Classes");
    addRecordWorkspace(Table::Items, "Items");
    addRecordWorkspace(Table::Consumables, "Consumables");
    if (project_->shops())
        workspaces_->addTab(new ShopEditor(project_.get(), undo_, [this] { updateStatus(); }),
                            "Shops");
    if (project_->encounters())
        workspaces_->addTab(new EncounterEditor(project_.get(), undo_, [this] { updateStatus(); }),
                            "Encounters");
    if (project_->scenarios())
        workspaces_->addTab(new ScenarioEditor(project_.get(), undo_, [this] { updateStatus(); }),
                            "Scenarios");
    if (!project_->textBanks().isEmpty())
        workspaces_->addTab(createTextWorkspace(project_.get(), undo_, [this] { updateStatus(); }),
                            "Descriptions");
    if (project_->rom().revision() == 0)
        workspaces_->addTab(new CutsceneEditor(project_->rom(), nullptr, project_.get(), undo_,
                                               [this] { updateStatus(); }),
                            "Cutscenes");
    workspaces_->addTab(new ToolsEditor(project_.get(), undo_, [this] { updateStatus(); }),
                        "Tools");
    workspaces_->addTab(new SpriteEditor(project_.get(), undo_, [this] { updateStatus(); }),
                        "Sprites");
    if(project_->rom().revision()==0) {
        auto artwork=new QTabWidget;
        artwork->addTab(new ArtEditor(project_.get(),undo_,[this]{updateStatus();}),"Item icons");
        artwork->addTab(new PortraitEditor(project_.get(),undo_,[this]{updateStatus();}),"Portraits");
        artwork->addTab(new ArmyEditor(project_.get(),undo_,[this]{updateStatus();}),"Army sprites");
        artwork->addTab(new CombatEditor(project_.get(),undo_,[this]{updateStatus();}),"Combat artwork");
        artwork->addTab(new AnimationEditor(project_.get(),undo_,[this]{updateStatus();}),"Class animations");
        workspaces_->addTab(artwork,"Art");
    }
    if (project_->rom().revision() == 0) {
        auto save = new SaveEditor(project_.get());
        undoGroup_->addStack(save->undoStack());
        connect(save, &SaveEditor::changed, this, &MainWindow::updateStatus);
        workspaces_->addTab(save, "Save");
    }
    updateStatus();
}
void MainWindow::addRecordWorkspace(Table table, const QString &title) {
    auto page = new QWidget;
    auto layout = new QVBoxLayout(page);
    auto search = new QLineEdit;
    search->setPlaceholderText("Search names, IDs, and values…");
    search->setClearButtonEnabled(true);
    layout->addWidget(search);
    auto split = new QSplitter;
    layout->addWidget(split);
    auto grid = new QTableView;
    grid->setObjectName(tableKey(table) + "Table");
    grid->setAlternatingRowColors(true);
    grid->setSelectionBehavior(QAbstractItemView::SelectRows);
    grid->setSelectionMode(QAbstractItemView::SingleSelection);
    grid->setSortingEnabled(true);
    grid->verticalHeader()->hide();
    auto model = new RecordModel(project_.get(), table, undo_, page);
    connect(model, &RecordModel::editRejected, this, &MainWindow::report);
    if (table == Table::Classes)
        grid->setItemDelegateForColumn(1, new ClassNameDelegate(grid));
    auto proxy = new QSortFilterProxyModel(page);
    proxy->setSourceModel(model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1);
    proxy->setSortRole(Qt::EditRole);
    grid->setModel(proxy);
    grid->setColumnWidth(0, 55);
    grid->setColumnWidth(1, 200);
    for (int i = 2; i < model->columnCount(); ++i)
        grid->setColumnWidth(i, 130);
    split->addWidget(grid);
    auto inspector = new QWidget;
    inspector->setMinimumWidth(340);
    auto inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    auto sections = new QComboBox;
    sections->setObjectName(tableKey(table) + "InspectorSection");
    auto inspectorPages = new QStackedWidget;
    inspectorLayout->addWidget(sections);
    inspectorLayout->addWidget(inspectorPages, 1);
    connect(sections, &QComboBox::currentIndexChanged, inspectorPages,
            &QStackedWidget::setCurrentIndex);
    auto addInspectorPage = [sections, inspectorPages](QWidget *page, const QString &name) {
        inspectorPages->addWidget(page);
        sections->addItem(name);
    };
    split->addWidget(inspector);
    split->setSizes({900, 390});
    auto selected = std::make_shared<int>(-1);
    QPushButton *addClass = nullptr;
    if (table == Table::Classes && project_->hasClassData()) {
        addClass = new QPushButton("Add class from selected…");
        addClass->setObjectName("addClass");
        layout->insertWidget(1, addClass, 0, Qt::AlignLeft);
        connect(
            addClass, &QPushButton::clicked, page, [this, selected, model, proxy, grid, search] {
                if (*selected < 1)
                    return;
                bool accepted = false;
                const auto name = QInputDialog::getText(
                    this, "Add class", "New class name", QLineEdit::Normal,
                    QString(project_->record(Table::Classes, *selected).name).replace('\n', ' ') +
                        " copy",
                    &accepted);
                if (!accepted)
                    return;
                const auto id = model->appendClass(*selected, name);
                if (id >= 0) {
                    search->clear();
                    grid->setCurrentIndex(proxy->mapFromSource(model->index(id, 1)));
                }
            });
    }
    auto editors = std::make_shared<QVector<ValueEditor *>>();
    QMap<QString, QFormLayout *> groups;
    for (int n = 0; n < fields(table).size(); ++n) {
        const auto &f = fields(table)[n];
        if (!groups.contains(f.group)) {
            auto body = new QWidget;
            auto form = new QFormLayout(body);
            form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
            auto scroll = new QScrollArea;
            scroll->setWidgetResizable(true);
            scroll->setWidget(body);
            addInspectorPage(scroll, f.group);
            groups[f.group] = form;
        }
        auto editor = new ValueEditor(f, project_->catalog(), [model, selected, n](int value) {
            if (*selected >= 0)
                model->setData(model->index(*selected, n + 2), value, Qt::EditRole);
        });
        editor->setEnabled(false);
        editors->push_back(editor);
        groups[f.group]->addRow(f.label, editor);
    }
    PromotionEditor *promotion = nullptr;
    ConsumableEditor *effects = nullptr;
    if (table == Table::Consumables && project_->consumableEffects()) {
        effects = new ConsumableEditor(project_.get(), undo_, [this] { updateStatus(); });
        addInspectorPage(effects, "Effects");
    }
    if (table == Table::Classes) {
        promotion = new PromotionEditor(project_.get(), undo_, [this] { updateStatus(); });
        auto scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setWidget(promotion);
        addInspectorPage(scroll, "Promotion");
    }
    auto refresh = [this, table, selected, editors, promotion, effects, model, addClass] {
        if (*selected >= model->rowCount())
            *selected = -1;
        if (addClass)
            addClass->setEnabled(*selected >= 1 && model->rowCount() < 256);
        if (promotion)
            promotion->selectClass(*selected);
        if (effects)
            effects->selectConsumable(*selected);
        for (int n = 0; n < editors->size(); ++n) {
            auto editor = (*editors)[n];
            editor->setEnabled(*selected >= 0 &&
                               (model->flags(model->index(*selected, n + 2)) & Qt::ItemIsEditable));
            editor->setChoices(project_->catalog());
            if (*selected >= 0)
                editor->setValue(project_->value(table, *selected, n));
        }
    };
    connect(grid->selectionModel(), &QItemSelectionModel::currentRowChanged, page,
            [proxy, selected, refresh](const QModelIndex &current) {
                *selected = current.isValid() ? proxy->mapToSource(current).row() : -1;
                refresh();
            });
    connect(search, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterFixedString);
    connect(model, &RecordModel::edited, page, [this, refresh] {
        refresh();
        updateStatus();
    });
    auto columns = new QComboBox;
    columns->addItem("All fields", "");
    for (auto group = groups.begin(); group != groups.end(); ++group)
        columns->addItem(group.key() + " columns", group.key());
    layout->insertWidget(1, columns);
    connect(columns, &QComboBox::currentIndexChanged, page, [=] {
        auto group = columns->currentData().toString();
        for (int i = 0; i < fields(table).size(); ++i)
            grid->setColumnHidden(i + 2, !group.isEmpty() && fields(table)[i].group != group);
        const auto section = sections->findText(group);
        if (section >= 0)
            sections->setCurrentIndex(section);
    });
    columns->setCurrentIndex(columns->findData(table == Table::Consumables ? "Values" : "Stats"));
    workspaces_->addTab(page, title);
    grid->selectRow(1);
}
void MainWindow::openProject() {
    if (loading_)
        return;
    if (!project_) {
        report("Open the source ROM before opening a project.");
        return;
    }
    if (!confirmDiscard())
        return;
    auto path = QFileDialog::getOpenFileName(this, "Open project manifest or sharing archive", {},
                                             "Lordly projects (*.json *.lcproject);;All files (*)");
    if (path.isEmpty())
        return;
    try {
        auto object = Project::readProject(path);
        auto next = Project::fromRom(project_->rom(), catalog_);
        next.loadTexts();
        if (next.rom().revision() == 0) {
            next.loadPromotion();
            next.loadShops();
            next.loadConsumableEffects();
            next.loadEncounters();
            next.loadScenarios();
            next.loadWorld();
        }
        if (object.value("format") == "LordlyCaliberProject")
            next.applyManifest(object);
        else
            next.importLegacy(object);
        auto warnings = next.importWarnings();
        installProject(std::move(next));
        if (!warnings.isEmpty())
            report(warnings.join('\n'));
    } catch (const std::exception &e) {
        report(QString::fromUtf8(e.what()));
    }
}
void MainWindow::saveProject(bool archive) {
    if (loading_)
        return;
    if (!project_) {
        report("Open a ROM first.");
        return;
    }
    if (invalidEdits()) {
        report("Correct invalid text input before saving.");
        return;
    }
    auto path = archive ? QFileDialog::getSaveFileName(this, "Save sharing archive", {},
                                                       "Lordly project (*.lcproject)")
                        : QFileDialog::getExistingDirectory(this, "Choose project folder");
    if (path.isEmpty())
        return;
    try {
        if (archive)
            project_->saveArchive(path);
        else
            project_->saveFolder(path);
        undo_->setClean();
        statusBar()->showMessage("Project saved", 5000);
    } catch (const std::exception &e) {
        report(QString::fromUtf8(e.what()));
    }
}
void MainWindow::buildRom() {
    if (loading_ || !project_)
        return;
    if (invalidEdits()) {
        report("Correct invalid text input before building.");
        return;
    }
    auto path = QFileDialog::getSaveFileName(this, "Export rebuilt ROM",
                                             "Ogre Battle 64-edited." + project_->rom().extension(),
                                             "N64 ROM (*.v64 *.z64 *.n64)");
    if (path.isEmpty())
        return;
    auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
    if (tools.isEmpty())
        tools = QCoreApplication::applicationDirPath() + "/n64-toolchain";
    if (!QFileInfo::exists(tools + "/cc1.exe"))
        tools = QCoreApplication::applicationDirPath() + "/../../.local/n64-toolchain";
    loading_ = true;
    workspaces_->setEnabled(false);
    statusBar()->showMessage("Compiling and validating the ROM…");
    struct Result {
        QString error;
        QStringList report;
    };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher] {
        auto result = watcher->result();
        watcher->deleteLater();
        loading_ = false;
        workspaces_->setEnabled(true);
        if (!result.error.isEmpty())
            report(result.error);
        else {
            QMessageBox::information(this, "ROM exported", result.report.join('\n'));
            statusBar()->showMessage("ROM exported successfully", 5000);
        }
    });
    watcher->setFuture(QtConcurrent::run([project = *project_, path, tools] {
        try {
            QTemporaryDir work(QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                               "/lordly-build-XXXXXX");
            if (!work.isValid())
                throw Error("Cannot create build workspace");
            auto result = rebuild(project, tools, work.path());
            exportRom(project, result, path);
            return Result{{}, result.report};
        } catch (const std::exception &e) {
            return Result{QString::fromUtf8(e.what()), {}};
        }
    }));
}
void MainWindow::updateStatus() {
    summary_->setText(project_ ? QString("  US Rev %1  ·  %2 changed records")
                                     .arg(project_->rom().revision())
                                     .arg(project_->changedRecords())
                               : "  No ROM loaded");
    const auto save = workspaces_->findChild<SaveEditor *>();
    setWindowModified(!undo_->isClean() || invalidEdits() || (save && save->modified()));
    setWindowTitle(QString("Lordly Caliber[*] · %1")
                       .arg(project_ ? project_->rom().title() : "Native Editor"));
}
bool MainWindow::invalidEdits() const {
    for (auto widget : workspaces_->findChildren<QWidget *>())
        if (widget->property("invalidEdits").toBool())
            return true;
    return false;
}
} // namespace lordly
