#include "app/main_window.h"
#include "app/pending_edits.h"
#include "app/class_setup.h"
#include "app/content_refresh.h"
#include "app/animation_editor.h"
#include "app/animation_override_editor.h"
#include "app/animation_sequence_editor.h"
#include "app/army_editor.h"
#include "app/art_editor.h"
#include "app/change_report_dialog.h"
#include "app/project_review_dialog.h"
#include "app/combat_editor.h"
#include "app/combat_animation_editor.h"
#include "app/consumable_editor.h"
#include "app/cutscene_editor.h"
#include "app/encounter_editor.h"
#include "app/portrait_editor.h"
#include "app/promotion_editor.h"
#include "app/save_editor.h"
#include "app/scenario_editor.h"
#include "app/shop_editor.h"
#include "app/sprite_editor.h"
#include "app/text_workspace.h"
#include "app/tools_editor.h"
#include "app/testing_editor.h"
#include "app/value_editor.h"
#include "rebuild/compiler.h"
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
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
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolBar>
#include <QUndoGroup>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent>
#include <optional>
#include "app/category_icon_editor.h"

namespace lordly {
bool MainWindow::eventFilter(QObject *object, QEvent *event) {
    auto widget = qobject_cast<QWidget *>(object);
    if (widget && isAncestorOf(widget)) {
        auto combo = qobject_cast<QComboBox *>(widget);
        auto spin = qobject_cast<QAbstractSpinBox *>(widget);
        if ((combo || spin) && event->type() == QEvent::Polish)
            widget->setFocusPolicy(Qt::StrongFocus);
        if ((combo || spin) && event->type() == QEvent::Wheel && !widget->hasFocus() &&
            !(combo && combo->view()->isVisible())) {
            event->ignore();
            return true;
        }
    }
    return QMainWindow::eventFilter(object, event);
}
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), undo_(new QUndoStack(this)), workspaces_(new QTabWidget(this)),
      summary_(new QLabel(this)) {
    qApp->installEventFilter(this);
    undo_->setUndoLimit(128);
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        ++projectRevision_;
        if (auto testing = workspaces_->findChild<TestingEditor *>())
            testing->setProjectRevision(projectRevision_, !pendingEdits(workspaces_).isEmpty());
    });
    setWindowTitle("Lordly Caliber · Native Editor");
    resize(1440, 900);
    setMinimumSize(900, 600);
    workspaces_->tabBar()->setObjectName("workspaceNavigation");
    setCentralWidget(workspaces_);
    undoGroup_ = new QUndoGroup(this);
    undoGroup_->addStack(undo_);
    undoGroup_->setActiveStack(undo_);
    connect(workspaces_, &QTabWidget::currentChanged, this, [this] {
        const auto save = qobject_cast<SaveEditor *>(workspaces_->currentWidget());
        undoGroup_->setActiveStack(
            save ? (save->hasPendingUnitEdits() ? nullptr : save->undoStack()) : undo_);
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
    auto review = file->addAction("Review project &changes…", this, [this] {
        if (project_) {
            ChangeReportDialog dialog(*project_, this);
            connect(&dialog, &ChangeReportDialog::editRequested, this, &MainWindow::navigateTo);
            dialog.exec();
        }
    });
    review->setObjectName("reviewProjectChanges");
    auto build = file->addAction("Build and &export ROM…", this, &MainWindow::buildRom);
    file->addSeparator();
    file->addAction("E&xit", this, &QWidget::close);
    auto edit = menuBar()->addMenu("&Edit");
    auto undoAction = undoGroup_->createUndoAction(this, "&Undo");
    undoAction->setToolTip("Undo the last edit. Project history retains up to 128 operations.");
    undoAction->setShortcut(QKeySequence::Undo);
    edit->addAction(undoAction);
    auto redoAction = undoGroup_->createRedoAction(this, "&Redo");
    redoAction->setShortcut(QKeySequence::Redo);
    edit->addAction(redoAction);
    auto toolbar = new QToolBar("Project", menuBar());
    toolbar->setObjectName("projectActions");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction(open);
    toolbar->addAction(save);
    toolbar->addAction(build);
    toolbar->addAction(review);
    auto uses = toolbar->addAction("Find uses", this, &MainWindow::showUses);
    uses->setObjectName("whereUsed");
    uses->setToolTip("Where is this used? Select a class, item, shop or sprite first.");
    auto pending = toolbar->addAction("Pending changes", this, [this] {
        resolvePendingEdits(workspaces_, "continuing", [this](const EditLocation &where) { navigateTo(where); });
    });
    pending->setObjectName("pendingProjectChanges");
    menuBar()->setCornerWidget(toolbar, Qt::TopRightCorner);
    statusBar()->addPermanentWidget(summary_);
    auto welcome =
        new QLabel("<h1>Lordly Caliber</h1><p>Open your Ogre Battle 64 ROM to begin.</p><p>Your "
                   "source ROM stays unchanged. Save your work as a project.</p>");
    welcome->setAlignment(Qt::AlignCenter);
    workspaces_->addTab(welcome, "Workspace");
    connect(undo_, &QUndoStack::cleanChanged, this, [this] { updateStatus(); });
    updateStatus();
}
bool MainWindow::confirmDiscard() {
    if (auto focus = focusWidget()) focus->clearFocus();
    if (!resolvePendingEdits(workspaces_, "closing or replacing this project",
                             [this](const EditLocation &where) { navigateTo(where); })) return false;
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
    if (auto back = findChild<QAction *>("returnToClassSetup")) back->setVisible(false);
    if (auto back = findChild<QPushButton *>("returnToClassSetupButton")) back->hide();
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
    if (project_->rom().revision() == 0) {
        const int tab =
            workspaces_->addTab(new CutsceneEditor(project_->rom(), nullptr, project_.get(), undo_,
                                                   [this] { updateStatus(); }),
                                "Cutscenes");
        auto badge = new QLabel("W.I.P");
        badge->setObjectName("cutsceneWipBadge");
        badge->setStyleSheet("QLabel { background: transparent; color: #ff9990; border: none; "
                             "padding: 0 12px 0 0; font-size: 9pt; font-weight: bold; }");
        badge->ensurePolished();
        badge->adjustSize();
        badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        workspaces_->tabBar()->setTabButton(tab, QTabBar::RightSide, badge);
        workspaces_->setTabToolTip(tab, "Cutscenes — work in progress");
    }
    if (project_->rom().revision() == 0) {
        auto artwork = new QTabWidget;
        artwork->setObjectName("artWorkspace");
        auto icons = new QTabWidget;
        icons->setObjectName("uiAndIcons");
        icons->addTab(new ArtEditor(project_.get(), undo_, [this] { updateStatus(); }),
                      "Item icons");
        icons->addTab(new CategoryIconEditor(project_.get(), undo_, [this] { updateStatus(); }),
                      "Class category icons");
        artwork->addTab(icons, "UI and Icons");
        artwork->addTab(new PortraitEditor(project_.get(), undo_, [this] { updateStatus(); }),
                        "Portraits");
        artwork->addTab(new ArmyEditor(project_.get(), undo_, [this] { updateStatus(); }),
                        "Army sprites");
        artwork->addTab(new CombatAnimationEditor(project_.get(), undo_, [this] { updateStatus(); }),
                        "Combat animations");
        auto advanced = new QStackedWidget;
        advanced->addWidget(new CombatEditor(project_.get(), undo_, [this] { updateStatus(); }));
        advanced->addWidget(new AnimationEditor(project_.get(), undo_, [this] { updateStatus(); }));
        advanced->addWidget(new AnimationOverrideEditor(project_.get(), undo_, [this] { updateStatus(); }));
        advanced->addWidget(new AnimationSequenceEditor(project_.get(), undo_, [this] { updateStatus(); }));
        artwork->addTab(advanced, "Advanced animations");
        auto advancedTool = new QComboBox;
        advancedTool->setObjectName("advancedAnimationTool");
        advancedTool->setAccessibleName("Advanced animation tool");
        advancedTool->setToolTip("Choose an advanced animation editor");
        advancedTool->addItems({"Shared combat artwork", "Class animation settings", "Attack overrides", "Private sequences"});
        artwork->setCornerWidget(advancedTool, Qt::TopRightCorner);
        advancedTool->hide();
        connect(advancedTool, &QComboBox::currentIndexChanged, advanced, &QStackedWidget::setCurrentIndex);
        connect(artwork, &QTabWidget::currentChanged, advancedTool, [artwork, advanced, advancedTool](int index) {
            advancedTool->setVisible(artwork->widget(index) == advanced);
        });
        workspaces_->addTab(artwork, "Art and Animation");
    }
    workspaces_->addTab(new ToolsEditor(project_.get(), undo_, [this] { updateStatus(); }),
                        "Tools");
    if (project_->rom().revision() == 0) {
        auto save = new SaveEditor(project_.get());
        undoGroup_->addStack(save->undoStack());
        connect(save, &SaveEditor::changed, this, &MainWindow::updateStatus);
        workspaces_->addTab(save, "Save Editor");
        if (auto cutscenes = workspaces_->findChild<CutsceneEditor *>())
            cutscenes->setSaveProvider([save]() -> const SaveDocument * {
                return save->document() ? &*save->document() : nullptr;
            });
        auto testing = new TestingEditor(project_->rom().sourcePath());
        workspaces_->addTab(testing, "Testing");
        testing->setProjectRevision(projectRevision_, false);
        connect(testing, &TestingEditor::exportRequested, this, &MainWindow::buildRom);
        for (auto widget : workspaces_->findChildren<QWidget *>())
            if (auto scenario = dynamic_cast<ScenarioEditor *>(widget))
                scenario->testScenario = [this, testing](int key) {
                    workspaces_->setCurrentWidget(testing); testing->selectScenario(key);
                };
        connect(testing, &TestingEditor::editDefaultArmy, this,
                [this, save, testing](const QByteArray &bytes) {
                    if (save->editDefaultArmy(bytes, [this, testing](const QByteArray &army) {
                            testing->setDefaultArmy(army);
                            workspaces_->setCurrentWidget(testing);
                        }))
                        workspaces_->setCurrentWidget(save);
                });
    }
    workspaces_->addTab(new SpriteEditor(project_.get(), undo_, [this] { updateStatus(); }),
                        "Art Library");
    for (auto drafts : workspaces_->findChildren<PendingEdits *>())
        connect(drafts, &PendingEdits::changed, this, &MainWindow::updateStatus);
    updateStatus();
}
void MainWindow::addRecordWorkspace(Table table, const QString &title) {
    auto page = new QWidget;
    auto layout = new QVBoxLayout(page);
    auto search = new QLineEdit;
    search->setPlaceholderText("Search names, IDs, and values…");
    search->setObjectName(tableKey(table) + "Search");
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
    if (table == Table::Consumables) {
        grid->resizeColumnsToContents();
        grid->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    }
    split->addWidget(grid);
    auto inspector = new QWidget;
    inspector->setMinimumWidth(340);
    auto inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    QComboBox *sections = nullptr;
    auto inspectorPages = new QStackedWidget;
    if (table == Table::Consumables) {
        inspectorLayout->addWidget(new QLabel("Effects"));
    } else {
        sections = new QComboBox;
        sections->setObjectName(tableKey(table) + "InspectorSection");
        inspectorLayout->addWidget(sections);
        connect(sections, &QComboBox::currentIndexChanged, inspectorPages,
                &QStackedWidget::setCurrentIndex);
    }
    inspectorLayout->addWidget(inspectorPages, 1);
    auto addInspectorPage = [sections, inspectorPages](QWidget *page, const QString &name) {
        inspectorPages->addWidget(page);
        if (sections)
            sections->addItem(name);
    };
    split->addWidget(inspector);
    split->setSizes({900, 390});
    auto selected = std::make_shared<int>(-1);
    ClassSetup *setup = nullptr;
    if (table == Table::Classes && project_->hasClassData()) {
        setup = new ClassSetup(project_.get(), [this](const EditLocation &location) {
            auto back = findChild<QAction *>("returnToClassSetup");
            if (!back) {
                back = new QAction("Back to class setup", this);
                back->setObjectName("returnToClassSetup");
                auto button = new QPushButton;
                button->setObjectName("returnToClassSetupButton");
                statusBar()->addWidget(button);
                connect(button, &QPushButton::clicked, back, &QAction::trigger);
                connect(back, &QAction::triggered, this, [this, back] {
                    navigateTo({"classes", back->data().toInt(), -1, "section:Overview"});
                });
            }
            back->setData(location.id);
            back->setText("Back to " + project_->record(Table::Classes, location.id).name.simplified());
            back->setVisible(true);
            auto button = findChild<QPushButton *>("returnToClassSetupButton");
            button->setText(back->text() + " setup"); button->show();
            navigateTo(location);
        });
        auto scroll = new QScrollArea;
        scroll->setWidgetResizable(true); scroll->setWidget(setup);
        addInspectorPage(scroll, "Overview");
        watchContentChanges(undo_, setup, [setup, selected] { setup->selectClass(*selected); });
    }
    auto description = new DescriptionPanel(project_.get(), undo_, [this] { updateStatus(); });
    description->setObjectName(tableKey(table) + "DescriptionPanel");
    inspectorLayout->addWidget(description);
    auto actionDescriptions = std::make_shared<QMap<int, QPushButton *>>();
    auto actionId = [this, table, selected](int field) {
        if (*selected < 0 || *selected >= project_->records(table).size())
            return -1;
        int id = project_->value(table, *selected, field);
        if (table == Table::Classes && id == 255) {
            const int owner = u8(project_->record(table, *selected).bytes, 69);
            if (owner > 0 && owner < project_->records(table).size())
                id = project_->value(table, owner, field);
        }
        return id;
    };
    QPushButton *addClass = nullptr, *newClass = nullptr, *renameClass = nullptr, *addItem = nullptr, *newItem = nullptr;
    if (table == Table::Classes && project_->hasClassData()) {
        addClass = new QPushButton("Copy class…");
        addClass->setObjectName("addClass");
        newClass = new QPushButton("New class…");
        newClass->setObjectName("newClass");
        auto actions = new QHBoxLayout;
        actions->addWidget(newClass);
        actions->addWidget(addClass);
        renameClass = new QPushButton("Rename class…");
        renameClass->setObjectName("renameClass");
        renameClass->setToolTip("Change the selected class's in-game name.");
        actions->addWidget(renameClass);
        actions->addStretch();
        layout->insertLayout(1, actions);
        connect(renameClass, &QPushButton::clicked, page, [this, selected, model, proxy, grid, search] {
            const int id = *selected;
            if (id < 0 || id >= model->rowCount()) return;
            QDialog dialog(this);
            dialog.setObjectName("renameClassDialog");
            dialog.setWindowTitle("Rename class");
            dialog.resize(480, 270);
            auto body = new QVBoxLayout(&dialog);
            auto selectedClass = new QLabel(QString("Class %1 · %2")
                .arg(QString::number(id, 16).toUpper().rightJustified(2, '0'),
                     project_->record(Table::Classes, id).name.simplified()));
            selectedClass->setTextFormat(Qt::PlainText);
            selectedClass->setWordWrap(true);
            body->addWidget(selectedClass);
            auto label = new QLabel("In-game class name (up to two lines)");
            auto name = new QPlainTextEdit(decodeText(project_->classNameBytes(id)));
            name->setObjectName("renameClassName");
            name->setTabChangesFocus(true);
            label->setBuddy(name);
            body->addWidget(label);
            body->addWidget(name);
            auto error = new QLabel;
            error->setObjectName("renameClassError");
            error->setWordWrap(true);
            body->addWidget(error);
            auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
            body->addWidget(buttons);
            auto validate = [name, error, buttons] {
                try {
                    encodeClassName(name->toPlainText());
                    error->clear();
                    buttons->button(QDialogButtonBox::Save)->setEnabled(true);
                } catch (const Error &e) {
                    error->setText(QString::fromUtf8(e.what()));
                    buttons->button(QDialogButtonBox::Save)->setEnabled(false);
                }
            };
            connect(name, &QPlainTextEdit::textChanged, &dialog, validate);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
                const auto index = model->index(id, 1);
                if (name->toPlainText() == index.data(Qt::EditRole).toString() ||
                    model->setData(index, name->toPlainText(), Qt::EditRole)) dialog.accept();
            });
            validate();
            name->selectAll();
            name->setFocus();
            if (dialog.exec() != QDialog::Accepted) return;
            auto index = proxy->mapFromSource(model->index(id, 1));
            if (!index.isValid()) {
                search->clear();
                index = proxy->mapFromSource(model->index(id, 1));
            }
            grid->setCurrentIndex(index);
            grid->scrollTo(index);
        });
        connect(newClass, &QPushButton::clicked, page, [this, model, proxy, grid, search] {
            bool accepted = false;
            const auto name =
                QInputDialog::getText(this, "New class",
                                      "New class name\nStarts with 1 HP and 1 in each base stat, "
                                      "no equipment, attacks or family."
                                      "\nCharacter compatibility starts as Male; this does not assign the Male family."
                                      "\nKnight artwork and animations are assigned initially; "
                                      "review them in the class Overview.",
                                      QLineEdit::Normal, "New class", &accepted);
            if (!accepted)
                return;
            const auto id = model->createClass(name);
            if (id >= 0) {
                search->clear();
                grid->setCurrentIndex(proxy->mapFromSource(model->index(id, 1)));
            }
        });
        connect(
            addClass, &QPushButton::clicked, page, [this, selected, model, proxy, grid, search] {
                if (*selected < 1)
                    return;
                bool accepted = false;
                const auto name = QInputDialog::getText(
                    this, "Copy class", "New class name", QLineEdit::Normal,
                    decodeText(project_->classNameBytes(*selected)).replace('\n', ' ') +
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
    if (table == Table::Items && project_->hasItemData()) {
        addItem = new QPushButton("Copy item…");
        addItem->setObjectName("addItem");
        addItem->setToolTip("Create a new item ID using the selected item's stats, description and "
                            "ROM artwork as a starting point.");
        auto actions = new QHBoxLayout;
        newItem = new QPushButton("New item…");
        newItem->setObjectName("newItem");
        newItem->setToolTip("Create a blank accessory with zero stats and no artwork or effects. "
                            "Set its equipment type and other fields after creation.");
        actions->addWidget(newItem);
        actions->addWidget(addItem);
        connect(newItem, &QPushButton::clicked, page, [this, model, proxy, grid, search] {
            bool accepted = false;
            const auto name = QInputDialog::getText(this, "New item", "Item name",
                                                    QLineEdit::Normal, "New item", &accepted);
            if (!accepted)
                return;
            const int id = model->createItem(name);
            if (id >= 0) {
                search->clear();
                grid->setCurrentIndex(proxy->mapFromSource(model->index(id, 1)));
            }
        });
        actions->addStretch();
        layout->insertLayout(1, actions);
        connect(addItem, &QPushButton::clicked, page, [this, selected, model, proxy, grid, search] {
            if (*selected < 1)
                return;
            bool accepted = false;
            const auto name = QInputDialog::getText(
                this, "Copy item", "New item name", QLineEdit::Normal,
                project_->record(Table::Items, *selected).name.simplified() + " copy", &accepted);
            if (!accepted)
                return;
            const int id = model->appendItem(*selected, name);
            if (id >= 0) {
                search->clear();
                grid->setCurrentIndex(proxy->mapFromSource(model->index(id, 1)));
            }
        });
    }
    auto editors = std::make_shared<QVector<ValueEditor *>>();
    QMap<QString, QFormLayout *> groups;
    for (int n = 0; n < fields(table).size(); ++n) {
        if (table == Table::Consumables) {
            editors->push_back(nullptr);
            continue;
        }
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
        if (f.offset < 4 && ((table == Table::Classes && project_->hasClassData()) ||
                             (table == Table::Items && project_->hasItemData()))) {
            if (!f.offset)
                groups[f.group]->addRow("Name address", new QLabel("Automatic — allocated during ROM export"));
            grid->setColumnHidden(n + 2, true);
            editors->push_back(nullptr);
            continue;
        }
        auto editor = new ValueEditor(f, project_->catalog(), [model, selected, n](int value) {
            if (*selected >= 0)
                model->setData(model->index(*selected, n + 2), value, Qt::EditRole);
        });
        editor->setEnabled(false);
        if (f.key == "classCopyMatch")
            editor->setToolTip(
                "<p><b>Default class and identity anchor</b><br>"
                "Sets the current class used when a character is created.</p>"
                "<p>A nonzero value also keeps the character's original class identity during manual class "
                "changes. Only their current class changes. With <b>0 - Same as this class</b>, new characters "
                "start in this class, but a later manual class change replaces both class IDs.</p>"
                "<p><b>Asnabel example (IDs in hex)</b><br>"
                "Starts as Berserker: identity <b>76</b>, current class <b>76</b>.<br>"
                "Changes to Knight: identity <b>76</b>, current class <b>05</b>.<br>"
                "Returns to Berserker: his existing class-menu mapping restores current class <b>76</b>.</p>"
                "<p>When current class matches this setting, supported class and appearance lookups use "
                "this class's own record; otherwise they use the current class. Ordinary level-up growth "
                "comes directly from the current class.</p>"
                "<p>Changing this default does not rewrite existing characters' stored class IDs.</p>");
        editors->push_back(editor);
        groups[f.group]->addRow(f.label, editor);
        if (f.choices == "ACTION_NAMES") {
            auto edit = new QPushButton("Description…");
            edit->setObjectName(f.key + "Description");
            editor->layout()->addWidget(edit);
            actionDescriptions->insert(n, edit);
            connect(edit, &QPushButton::clicked, page, [this, page, actionId, n] {
                editDescription(page, project_.get(), undo_, "actions", actionId(n),
                                [this] { updateStatus(); });
            });
        }
    }
    PromotionEditor *promotion = nullptr;
    ConsumableEditor *effects = nullptr;
    if (table == Table::Consumables) {
        if (project_->consumableEffects()) {
            effects = new ConsumableEditor(project_.get(), undo_, [this] { updateStatus(); });
            addInspectorPage(effects, "Effects");
        } else {
            auto unavailable = new QLabel("Effect editing is unavailable for this ROM.");
            unavailable->setWordWrap(true);
            unavailable->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            addInspectorPage(unavailable, "Effects");
        }
    }
    if (table == Table::Classes) {
        promotion = new PromotionEditor(project_.get(), undo_, [this] { updateStatus(); });
        connect(promotion, &PromotionEditor::editFamilyIconRequested, this,
                [this](int family, int icon, bool create) {
                    auto artwork = workspaces_->findChild<QTabWidget *>("artWorkspace");
                    auto icons = workspaces_->findChild<QTabWidget *>("uiAndIcons");
                    auto editor = workspaces_->findChild<CategoryIconEditor *>();
                    if (!artwork || !icons || !editor)
                        return;
                    workspaces_->setCurrentWidget(artwork);
                    artwork->setCurrentWidget(icons);
                    icons->setCurrentWidget(editor);
                    editor->selectFamily(family);
                    if (icon)
                        editor->selectCategory(icon);
                    if (create)
                        editor->createIcon(family);
                });
        auto scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setWidget(promotion);
        addInspectorPage(scroll, "Promotion");
    }
    auto refresh = [this, table, selected, editors, promotion, effects, model, addClass, newClass,
                    addItem, newItem, renameClass, description, actionDescriptions, actionId, setup] {
        if (newItem)
            newItem->setEnabled(model->rowCount() < ItemData::maximumCount);
        if (*selected >= model->rowCount())
            *selected = -1;
        if (setup) setup->selectClass(*selected);
        description->selectDescription(tableKey(table), *selected < 0 ? -1 :
            table == Table::Classes ? Project::classDescriptionIndex(*selected) : *selected);
        if (addItem)
            addItem->setEnabled(*selected >= 1 && model->rowCount() < ItemData::maximumCount);
        if (renameClass)
            renameClass->setEnabled(*selected >= 0 &&
                                   (model->flags(model->index(*selected, 1)) & Qt::ItemIsEditable));
        if (newClass)
            newClass->setEnabled(model->rowCount() < 256);
        if (addClass)
            addClass->setEnabled(*selected >= 1 && model->rowCount() < 256);
        if (promotion)
            promotion->selectClass(*selected);
        if (effects)
            effects->selectConsumable(*selected);
        for (int n = 0; n < editors->size(); ++n) {
            auto editor = (*editors)[n];
            if (!editor) continue;
            editor->setEnabled(*selected >= 0 &&
                               (model->flags(model->index(*selected, n + 2)) & Qt::ItemIsEditable));
            editor->setChoices(project_->catalog());
            if (*selected >= 0)
                editor->setValue(project_->value(table, *selected, n));
        }
        for (auto it = actionDescriptions->cbegin(); it != actionDescriptions->cend(); ++it) {
            const int id = actionId(it.key());
            const bool valid = project_->textBanks().contains("actions") && id >= 0 &&
                id < project_->textBanks()["actions"].entries.size();
            it.value()->setEnabled(valid);
            it.value()->setToolTip(valid
                ? QString("Edit action %1 · %2. Shared by every class using this action.")
                    .arg(id).arg(project_->catalog()["ACTION_NAMES"].toObject()[QString::number(id)].toString())
                : "This action has no description record.");
        }
    };
    connect(grid->selectionModel(), &QItemSelectionModel::currentRowChanged, page,
            [proxy, selected, refresh](const QModelIndex &current) {
                *selected = current.isValid() ? proxy->mapToSource(current).row() : -1;
                refresh();
            });
    connect(search, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterFixedString);
    auto position = std::make_shared<QPoint>();
    auto selectedBeforeReset = std::make_shared<int>(-1);
    connect(model, &RecordModel::aboutToReplace, page, [=] {
        *selectedBeforeReset = *selected;
        *position = {grid->horizontalScrollBar()->value(), grid->verticalScrollBar()->value()};
    });
    connect(model, &RecordModel::replaced, page, [=] {
        if (*selectedBeforeReset >= 0 && model->rowCount())
            grid->setCurrentIndex(proxy->mapFromSource(
                model->index(std::min(*selectedBeforeReset, model->rowCount() - 1), 1)));
        grid->doItemsLayout();
        grid->horizontalScrollBar()->setValue(position->x());
        grid->verticalScrollBar()->setValue(position->y());
    });
    connect(model, &RecordModel::edited, page, [this, refresh] {
        refresh();
        updateStatus();
    });
    if (table != Table::Consumables) {
        const auto showColumns = [=](const QString &group) {
            for (int i = 0; i < fields(table).size(); ++i)
                grid->setColumnHidden(i + 2, !group.isEmpty() && fields(table)[i].group != group);
            const auto section = sections->findText(group);
            if (section >= 0)
                sections->setCurrentIndex(section);
        };
        if (table == Table::Items) {
            showColumns("Stats");
        } else {
            auto columns = new QComboBox;
            columns->addItem("All fields", "");
            for (auto group = groups.begin(); group != groups.end(); ++group)
                columns->addItem(group.key() + " columns", group.key());
            layout->insertWidget(1, columns);
            connect(columns, &QComboBox::currentIndexChanged, page,
                    [=] { showColumns(columns->currentData().toString()); });
            columns->setCurrentIndex(columns->findData("Stats"));
        }
    }
    workspaces_->addTab(page, title);
    if (setup) sections->setCurrentIndex(sections->findText("Overview"));
    grid->selectRow(1);
}
void MainWindow::openProject() {
    if (loading_)
        return;
    if (!project_) {
        report("Open the source ROM before opening a project.");
        return;
    }
    auto path = QFileDialog::getOpenFileName(this, "Open project manifest or sharing archive", {},
                                             "Lordly projects (*.json *.lcproject);;All files (*)");
    if (path.isEmpty())
        return;
    try {
        openProjectFile(path);
    } catch (const std::exception &e) {
        report(QString::fromUtf8(e.what()));
    }
}
bool MainWindow::openProjectFile(const QString &path) {
    if (loading_ || !project_ || !confirmDiscard()) return false;
    const auto object = Project::readProject(path);
    // Manifests identify the actual input ROM, including installed runtimes.
    auto next = Project::fromRom(project_->runtimeRom(), catalog_);
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
    return true;
}
void MainWindow::saveProject(bool archive) {
    if (loading_)
        return;
    if (!project_) {
        report("Open a ROM first.");
        return;
    }
    if (auto focus = focusWidget()) focus->clearFocus();
    if (!resolvePendingEdits(workspaces_, "saving the project",
                             [this](const EditLocation &where) { navigateTo(where); })) return;
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
    if (auto focus = focusWidget()) focus->clearFocus();
    if (!resolvePendingEdits(workspaces_, "exporting the ROM",
                             [this](const EditLocation &where) { navigateTo(where); })) return;
    if (invalidEdits()) {
        report("Correct invalid text input before building.");
        return;
    }
    ProjectReviewDialog review(*project_, {}, this);
    connect(&review, &ProjectReviewDialog::editRequested, this, &MainWindow::navigateTo);
    if (review.exec() != QDialog::Accepted) return;
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
        QByteArray digest;
    };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, path, revision = projectRevision_] {
        auto result = watcher->result();
        watcher->deleteLater();
        loading_ = false;
        workspaces_->setEnabled(true);
        if (!result.error.isEmpty())
            report(result.error);
        else {
            auto testing = workspaces_->findChild<TestingEditor *>();
            if (testing) testing->useExport(path, revision, result.digest);
            QMessageBox message(QMessageBox::Information, "ROM exported", path + "\n\n" + result.report.join('\n'), QMessageBox::Ok, this);
            auto test = testing ? message.addButton("Test this ROM", QMessageBox::ActionRole) : nullptr;
            message.exec();
            if (test && message.clickedButton() == test) workspaces_->setCurrentWidget(testing);
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
            return Result{{}, result.report, sha256(result.image)};
        } catch (const std::exception &e) {
            return Result{QString::fromUtf8(e.what()), {}};
        }
    }));
}
void MainWindow::updateStatus() {
    const int draftCount = pendingEdits(workspaces_).size();
    if (auto testing = workspaces_->findChild<TestingEditor *>())
        testing->setProjectRevision(projectRevision_, draftCount > 0 || invalidEdits());
    auto pending = findChild<QAction *>("pendingProjectChanges");
    pending->setVisible(draftCount > 0);
    pending->setText(QString("Pending changes (%1)").arg(draftCount));
    findChild<QAction *>("reviewProjectChanges")->setEnabled(bool(project_));
    findChild<QAction *>("whereUsed")->setEnabled(bool(project_));
    summary_->setText(project_ ? QString("  US Rev %1  ·  %2 changed records")
                                     .arg(project_->rom().revision())
                                     .arg(project_->changedRecords())
                               : "  No ROM loaded");
    const auto save = workspaces_->findChild<SaveEditor *>();
    if (save && workspaces_->currentWidget() == save)
        undoGroup_->setActiveStack(save->hasPendingUnitEdits() ? nullptr : save->undoStack());
    setWindowModified(draftCount || !undo_->isClean() || invalidEdits() || (save && save->modified()));
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
