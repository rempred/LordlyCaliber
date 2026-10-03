#include "app/main_window.h"
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
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolBar>
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
    auto undoAction = undo_->createUndoAction(this, "&Undo");
    undoAction->setShortcut(QKeySequence::Undo);
    edit->addAction(undoAction);
    auto redoAction = undo_->createRedoAction(this, "&Redo");
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
    if (undo_->isClean())
        return true;
    return QMessageBox::question(this, "Unsaved edits", "Discard unsaved edits?",
                                 QMessageBox::Discard | QMessageBox::Cancel,
                                 QMessageBox::Cancel) == QMessageBox::Discard;
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
            return Result{Project::fromRom(Rom::open(path), catalog), {}};
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
    auto inspector = new QTabWidget;
    inspector->setMinimumWidth(340);
    split->addWidget(inspector);
    split->setSizes({900, 390});
    auto selected = std::make_shared<int>(-1);
    auto spins = std::make_shared<QVector<QSpinBox *>>();
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
            inspector->addTab(scroll, f.group);
            groups[f.group] = form;
        }
        auto spin = new QSpinBox;
        spin->setRange(f.minimum(), f.maximum());
        spin->setKeyboardTracking(false);
        spin->setEnabled(false);
        spins->push_back(spin);
        groups[f.group]->addRow(f.label, spin);
        connect(spin, &QSpinBox::valueChanged, page, [model, selected, n](int value) {
            if (*selected >= 0)
                model->setData(model->index(*selected, n + 2), value, Qt::EditRole);
        });
    }
    auto refresh = [this, table, selected, spins] {
        for (int n = 0; n < spins->size(); ++n) {
            auto spin = (*spins)[n];
            QSignalBlocker block(spin);
            spin->setEnabled(*selected >= 0);
            if (*selected >= 0)
                spin->setValue(project_->value(table, *selected, n));
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
    setWindowModified(!undo_->isClean());
    setWindowTitle(QString("Lordly Caliber[*] · %1")
                       .arg(project_ ? project_->rom().title() : "Native Editor"));
}
} // namespace lordly
