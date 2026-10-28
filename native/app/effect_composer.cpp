#include "app/effect_composer.h"
#include "app/effect_library_editor.h"
#include "app/recovery_draft.h"
#include "engine/animation_file_import.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
class EffectCanvas : public QWidget {
  public:
    EffectCanvas() {
        setMinimumSize(360, 300);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    EffectComposition model;
    int selected = -1, tick = 0;
    bool end = false;
    std::function<void()> startDrag;
    std::function<void(QPoint)> moved;

  protected:
    QRectF scene() const {
        const double scale =
            std::max(.1, std::min((width() - 24) / 320.0, (height() - 24) / 240.0));
        return {(width() - 320 * scale) / 2, (height() - 240 * scale) / 2, 320 * scale,
                240 * scale};
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#1d2027"));
        const auto r = scene();
        const auto scale = r.width() / 320;
        p.translate(r.topLeft());
        p.scale(scale, scale);
        p.setClipRect(QRectF(0, 0, 320, 240));
        for (int y = 0; y < 240; y += 8)
            for (int x = 0; x < 320; x += 8)
                p.fillRect(x, y, 8, 8,
                           ((x / 8 + y / 8) & 1) ? QColor("#303641") : QColor("#242932"));
        p.setPen(QColor("#e7d8bd"));
        p.drawText(QPoint(8, 16), model.placement == 1   ? "Origin: each affected unit"
                                  : model.placement == 2 ? "Origin: caster"
                                  : model.placement == 3 ? "Origin: target squad centre"
                                                         : "Origin: screen centre");
        p.drawImage(QPointF(160 - model.size.width() / 2.0, 120 - model.size.height() / 2.0),
                    model.render(tick));
        p.setPen(QPen(QColor("#a5a5aa"), .8, Qt::DashLine));
        p.drawRect(QRectF(160 - model.size.width() / 2.0, 120 - model.size.height() / 2.0,
                          model.size.width(), model.size.height()));
        if (selected >= 0 && selected < model.pieces.size()) {
            const auto &piece = model.pieces[selected];
            const auto a = QPoint(160, 120) + piece.from, b = QPoint(160, 120) + piece.to;
            p.setPen(QPen(QColor("#ffca68"), 1));
            p.drawLine(a, b);
            p.setBrush(QColor("#ffca68"));
            p.drawEllipse(end ? b : a, 3, 3);
            p.setFont(QFont("Segoe UI", 8));
            p.drawText(a + QPoint(5, -5), "Start");
            p.drawText(b + QPoint(5, 12), "End");
        }
    }
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton || selected < 0 || selected >= model.pieces.size() ||
            !scene().contains(e->position()))
            return;
        if (startDrag)
            startDrag();
        dragging_ = true;
        drag(e->position());
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (dragging_)
            drag(e->position());
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (!dragging_ || e->button() != Qt::LeftButton)
            return;
        drag(e->position());
        dragging_ = false;
        if (moved)
            moved(end ? model.pieces[selected].to : model.pieces[selected].from);
    }

  private:
    bool dragging_ = false;
    void drag(QPointF at) {
        const auto r = scene();
        const auto point = (at - r.topLeft()) / (r.width() / 320);
        const QPoint value(std::clamp(qRound(point.x()) - 160, -512, 512),
                           std::clamp(qRound(point.y()) - 120, -512, 512));
        if (end)
            model.pieces[selected].to = value;
        else
            model.pieces[selected].from = value;
        update();
    }
};
namespace {
QLabel *note(const QString &text, QLayout *layout) {
    auto label = new QLabel(text);
    label->setWordWrap(true);
    layout->addWidget(label);
    return label;
}
QSpinBox *spin(const char *id, int low, int high, const QString &suffix = {}) {
    auto s = new QSpinBox;
    s->setObjectName(id);
    s->setRange(low, high);
    s->setKeyboardTracking(false);
    s->setSuffix(suffix);
    return s;
}
} // namespace
EffectComposer::EffectComposer(Project *project, EffectComposition model, QWidget *parent,
                               bool allowAssign)
    : QDialog(parent), project_(project), model_(std::move(model)) {
    setObjectName("effectComposer");
    setWindowTitle("Create / edit effect");
    resize(1180, 780);
    model_.validate();
    history_ << model_;
    auto root = new QVBoxLayout(this);
    auto heading = new QHBoxLayout;
    root->addLayout(heading);
    heading->addWidget(new QLabel("Effect name"));
    name_ = new QLineEdit;
    name_->setObjectName("compositionName");
    name_->setMaxLength(120);
    heading->addWidget(name_, 1);
    undo_ = new QPushButton("Undo");
    redo_ = new QPushButton("Redo");
    undo_->setObjectName("compositionUndo");
    redo_->setObjectName("compositionRedo");
    heading->addWidget(undo_);
    heading->addWidget(redo_);
    connect(undo_, &QPushButton::clicked, this, [this] { history(-1); });
    connect(redo_, &QPushButton::clicked, this, [this] { history(1); });
    connect(name_, &QLineEdit::editingFinished, this, [this] {
        guarded([this] {
            auto m = model_;
            m.name = name_->text().trimmed();
            record(m);
        });
    });
    note("Add artwork from any effect, then set when each piece appears and how it moves. "
         "Drag in the preview to place its start or end. Pieces higher in the list draw in front.",
         root);
    auto split = new QSplitter;
    root->addWidget(split, 1);
    auto left = new QWidget;
    auto listLayout = new QVBoxLayout(left);
    listLayout->setContentsMargins(0, 0, 0, 0);
    split->addWidget(left);
    auto add = new QPushButton("Add piece…");
    add->setObjectName("compositionAddPiece");
    listLayout->addWidget(add);
    auto menu = new QMenu(add);
    add->setMenu(menu);
    menu->addAction("From game effects…", this, [this] { guarded([this] { chooseEffect(); }); });
    menu->addAction("From Art Library…", this, [this] { guarded([this] { chooseLibrary(); }); });
    menu->addAction("From files…", this, [this] { guarded([this] { chooseFile(); }); });
    pieces_ = new QListWidget;
    pieces_->setObjectName("compositionPieces");
    pieces_->setWordWrap(true);
    listLayout->addWidget(pieces_, 1);
    auto actions = new QHBoxLayout;
    listLayout->addLayout(actions);
    auto copy = new QPushButton("Copy"), remove = new QPushButton("Remove");
    copy->setObjectName("compositionCopy");
    remove->setObjectName("compositionRemove");
    actions->addWidget(copy);
    actions->addWidget(remove);
    connect(copy, &QPushButton::clicked, this, [this] {
        guarded([this] {
            const int at =
                pieces_->currentItem() ? pieces_->currentItem()->data(Qt::UserRole).toInt() : -1;
            if (at < 0)
                return;
            auto m = model_;
            auto p = m.pieces[at];
            p.id = SpriteLibrary::newId();
            p.name = (p.name.left(110) + " copy");
            m.pieces << p;
            record(m);
            pieces_->setCurrentRow(0);
        });
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (!pieces_->currentItem())
            return;
        auto m = model_;
        m.pieces.removeAt(pieces_->currentItem()->data(Qt::UserRole).toInt());
        record(m);
    });
    auto order = new QHBoxLayout;
    listLayout->addLayout(order);
    for (int delta : {1, -1}) {
        auto b = new QPushButton(delta > 0 ? "Bring forward" : "Send back");
        order->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, delta] {
            if (!pieces_->currentItem())
                return;
            const int at = pieces_->currentItem()->data(Qt::UserRole).toInt();
            const int to = at + delta;
            if (to < 0 || to >= model_.pieces.size())
                return;
            auto m = model_;
            m.pieces.move(at, to);
            record(m);
            pieces_->setCurrentRow(int(model_.pieces.size()) - 1 - to);
        });
    }
    auto middle = new QWidget;
    auto preview = new QVBoxLayout(middle);
    preview->setContentsMargins(0, 0, 0, 0);
    split->addWidget(middle);
    canvas_ = new EffectCanvas;
    canvas_->setObjectName("compositionCanvas");
    preview->addWidget(canvas_, 1);
    auto positions = new QHBoxLayout;
    preview->addLayout(positions);
    auto endpoints = new QButtonGroup(this);
    for (int end = 0; end < 2; ++end) {
        auto b = new QPushButton(end ? "Place end" : "Place start");
        b->setObjectName(end ? "compositionPlaceEnd" : "compositionPlaceStart");
        b->setCheckable(true);
        b->setChecked(!end);
        endpoints->addButton(b, end);
        positions->addWidget(b);
    }
    connect(endpoints, &QButtonGroup::idClicked, this, [this](int end) {
        editEnd_ = end;
        playing_ = false;
        play_->setText("Play");
        if (pieces_->currentItem()) {
            const auto &p = model_.pieces[pieces_->currentItem()->data(Qt::UserRole).toInt()];
            time_->setValue(end ? p.start + p.duration - 1 : p.start);
        }
        updatePreview();
    });
    canvas_->startDrag = [this] {
        playing_ = false;
        play_->setText("Play");
    };
    canvas_->moved = [this](QPoint p) {
        editPiece([this, p](EffectPiece &piece) {
            if (editEnd_)
                piece.to = p;
            else
                piece.from = p;
        });
    };
    auto playback = new QHBoxLayout;
    preview->addLayout(playback);
    play_ = new QPushButton("Play");
    play_->setObjectName("compositionPlay");
    auto step = new QPushButton("Step");
    auto restart = new QPushButton("Restart");
    playback->addWidget(play_);
    playback->addWidget(step);
    playback->addWidget(restart);
    timeLabel_ = new QLabel;
    playback->addWidget(timeLabel_, 1);
    time_ = new QSlider(Qt::Horizontal);
    time_->setObjectName("compositionTime");
    preview->addWidget(time_);
    connect(time_, &QSlider::sliderPressed, this, [this] {
        playing_ = false;
        play_->setText("Play");
    });
    connect(time_, &QSlider::valueChanged, this, [this] {
        playing_ = false;
        play_->setText("Play");
        updatePreview();
    });
    connect(play_, &QPushButton::clicked, this, [this] {
        playing_ = !playing_;
        play_->setText(playing_ ? "Pause" : "Play");
        playFrom_ = time_->value();
        clock_.restart();
    });
    connect(step, &QPushButton::clicked, this, [this] {
        playing_ = false;
        play_->setText("Play");
        time_->setValue((time_->value() + 1) % model_.duration);
    });
    connect(restart, &QPushButton::clicked, this, [this] {
        QSignalBlocker blocker(time_);
        time_->setValue(0);
        playFrom_ = 0;
        clock_.restart();
        updatePreview();
    });
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (playing_) {
            QSignalBlocker blocker(time_);
            time_->setValue((playFrom_ + int(clock_.elapsed() * 30 / 1000)) % model_.duration);
            updatePreview();
        }
    });
    timer->start(33);
    auto setup = new QFormLayout;
    preview->addLayout(setup);
    width_ = spin("compositionWidth", 1, 256);
    height_ = spin("compositionHeight", 1, 240);
    length_ = spin("compositionDuration", 1, 300, " ticks");
    auto dimensions = new QHBoxLayout;
    dimensions->addWidget(width_);
    dimensions->addWidget(new QLabel("×"));
    dimensions->addWidget(height_);
    setup->addRow("Canvas", dimensions);
    setup->addRow("Effect length", length_);
    placement_ = new QComboBox;
    placement_->setObjectName("compositionPlacement");
    placement_->addItem("Each target", 1);
    placement_->addItem("Caster", 2);
    placement_->addItem("Target side · once over the squad", 3);
    placement_->addItem("Screen centre", 0);
    setup->addRow("Place effect at", placement_);
    connect(placement_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_) {
            auto m = model_;
            m.placement = placement_->currentData().toInt();
            record(m);
        }
    });
    auto changeSetup = [this] {
        if (refreshing_)
            return;
        guarded([this] {
            auto m = model_;
            m.size = {width_->value(), height_->value()};
            m.duration = length_->value();
            record(m);
        });
    };
    connect(width_, &QSpinBox::valueChanged, this, changeSetup);
    connect(height_, &QSpinBox::valueChanged, this, changeSetup);
    connect(length_, &QSpinBox::valueChanged, this, changeSetup);
    note("30 ticks = 1 second. The dashed box is your canvas; artwork outside it is clipped. "
         "Each target repeats the complete effect over every affected unit, including allies for "
         "healing. "
         "Target side plays once over that squad. Movement is relative to the chosen position; "
         "artwork retains its orientation on either side.",
         preview);
    pieceControls_ = new QWidget;
    auto form = new QFormLayout(pieceControls_);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(pieceControls_);
    scroll->setMinimumWidth(300);
    split->addWidget(scroll);
    split->setSizes({220, 560, 340});
    pieceName_ = new QLineEdit;
    pieceName_->setMaxLength(120);
    pieceName_->setObjectName("compositionPieceName");
    form->addRow("Selected piece", pieceName_);
    connect(pieceName_, &QLineEdit::editingFinished, this, [this] {
        editPiece([this](EffectPiece &p) { p.name = pieceName_->text().trimmed(); });
    });
    auto value = [this, form](const QString &label, const char *id, int lo, int hi,
                              int EffectPiece::*member, const QString &suffix = {}) {
        auto s = spin(id, lo, hi, suffix);
        form->addRow(label, s);
        connect(s, &QSpinBox::valueChanged, this, [this, member](int n) {
            editPiece([member, n](EffectPiece &p) { p.*member = n; });
        });
        return s;
    };
    start_ = value("Appears at", "compositionStart", 0, 299, &EffectPiece::start, " ticks");
    duration_ =
        value("Visible for", "compositionVisible", 1, 300, &EffectPiece::duration, " ticks");
    auto position = [this, form](const QString &label, bool end) {
        auto row = new QHBoxLayout;
        auto x = spin(end ? "compositionToX" : "compositionFromX", -512, 512);
        auto y = spin(end ? "compositionToY" : "compositionFromY", -512, 512);
        row->addWidget(new QLabel("X"));
        row->addWidget(x);
        row->addWidget(new QLabel("Y"));
        row->addWidget(y);
        form->addRow(label, row);
        connect(x, &QSpinBox::valueChanged, this, [this, end](int n) {
            editPiece([end, n](EffectPiece &p) { (end ? p.to : p.from).setX(n); });
        });
        connect(y, &QSpinBox::valueChanged, this, [this, end](int n) {
            editPiece([end, n](EffectPiece &p) { (end ? p.to : p.from).setY(n); });
        });
        if (end) {
            toX_ = x;
            toY_ = y;
        } else {
            fromX_ = x;
            fromY_ = y;
        }
    };
    position("Start position", false);
    position("End position", true);
    fromScale_ = value("Start size", "compositionFromScale", 1, 400, &EffectPiece::fromScale, "%");
    toScale_ = value("End size", "compositionToScale", 1, 400, &EffectPiece::toScale, "%");
    fromAngle_ =
        value("Start rotation", "compositionFromAngle", -1080, 1080, &EffectPiece::fromAngle, "°");
    toAngle_ = value("End rotation", "compositionToAngle", -1080, 1080, &EffectPiece::toAngle, "°");
    fromOpacity_ =
        value("Start opacity", "compositionFromOpacity", 0, 100, &EffectPiece::fromOpacity, "%");
    toOpacity_ = value("End opacity", "compositionToOpacity", 0, 100, &EffectPiece::toOpacity, "%");
    easing_ = new QComboBox;
    easing_->setObjectName("compositionEasing");
    easing_->addItems(
        {"Constant speed", "Start slowly", "Finish slowly", "Start and finish slowly"});
    form->addRow("Movement", easing_);
    connect(easing_, &QComboBox::currentIndexChanged, this,
            [this](int n) { editPiece([n](EffectPiece &p) { p.easing = n; }); });
    loop_ = new QCheckBox("Repeat artwork frames");
    form->addRow(loop_);
    connect(loop_, &QCheckBox::toggled, this,
            [this](bool b) { editPiece([b](EffectPiece &p) { p.loop = b; }); });
    auto coordinates = new QLabel(
        "X = 0, Y = 0 is the centre. Positive X moves right; positive Y moves down. "
        "Each piece has its own timeline. Size, rotation and opacity change from Start to End.");
    coordinates->setWordWrap(true);
    form->addRow(coordinates);
    connect(pieces_, &QListWidget::currentRowChanged, this, [this] {
        if (!refreshing_)
            refresh();
    });
    notice_ = note({}, root);
    notice_->setObjectName("compositionNotice");
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    root->addWidget(buttons);
    buttons->button(QDialogButtonBox::Save)->setText("Save effect");
    buttons->button(QDialogButtonBox::Save)->setObjectName("compositionSave");
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { finish(false); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if (allowAssign) {
        auto b = buttons->addButton("Save and assign…", QDialogButtonBox::ActionRole);
        connect(b, &QPushButton::clicked, this, [this] { finish(true); });
    }
    new RecoveryDraft(this, "Composed effect", [this] {
        if (model_.pieces.isEmpty())
            return QJsonObject{};
        auto library = project_->sprites();
        library.replace(asset());
        return QJsonObject{{"spriteLibrary", library.json()}};
    });
    for (auto button : findChildren<QPushButton *>()) {
        button->setAutoDefault(false);
        button->setDefault(false);
    }
    undo_->setShortcut(QKeySequence::Undo);
    redo_->setShortcut(QKeySequence::Redo);
    refresh();
}
void EffectComposer::keyPressEvent(QKeyEvent *event) {
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        !qobject_cast<QPushButton *>(focusWidget())) {
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}
void EffectComposer::guarded(const std::function<void()> &fn) {
    try {
        fn();
        notice_->clear();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        refresh();
    }
}
void EffectComposer::record(EffectComposition m) {
    if (refreshing_ || m == model_)
        return;
    m.validate();
    history_.resize(historyAt_ + 1);
    history_ << m;
    historyAt_ = int(history_.size()) - 1;
    model_ = std::move(m);
    playing_ = false;
    play_->setText("Play");
    refresh();
}
void EffectComposer::setComposition(const EffectComposition &m) {
    record(m);
}
void EffectComposer::history(int delta) {
    const int at = historyAt_ + delta;
    if (at < 0 || at >= history_.size())
        return;
    historyAt_ = at;
    model_ = history_[at];
    playing_ = false;
    play_->setText("Play");
    refresh();
}
void EffectComposer::editPiece(const std::function<void(EffectPiece &)> &fn) {
    if (refreshing_ || !pieces_->currentItem())
        return;
    guarded([&] {
        auto m = model_;
        fn(m.pieces[pieces_->currentItem()->data(Qt::UserRole).toInt()]);
        record(m);
    });
}
void EffectComposer::refresh() {
    QScopedValueRollback guard(refreshing_, true);
    const int row = pieces_->currentRow();
    name_->setText(model_.name);
    width_->setValue(model_.size.width());
    height_->setValue(model_.size.height());
    length_->setValue(model_.duration);
    placement_->setCurrentIndex(placement_->findData(model_.placement));
    time_->setMaximum(model_.duration - 1);
    pieces_->clear();
    for (int at = int(model_.pieces.size()) - 1; at >= 0; --at) {
        const auto &p = model_.pieces[at];
        auto item = new QListWidgetItem(
            QString("%1\n%2–%3 ticks").arg(p.name).arg(p.start).arg(p.start + p.duration - 1),
            pieces_);
        item->setData(Qt::UserRole, at);
    }
    if (!model_.pieces.isEmpty())
        pieces_->setCurrentRow(std::clamp(row, 0, int(model_.pieces.size()) - 1));
    pieceControls_->setEnabled(!model_.pieces.isEmpty());
    if (pieces_->currentItem()) {
        const auto &p = model_.pieces[pieces_->currentItem()->data(Qt::UserRole).toInt()];
        pieceName_->setText(p.name);
        start_->setValue(p.start);
        duration_->setValue(p.duration);
        fromX_->setValue(p.from.x());
        fromY_->setValue(p.from.y());
        toX_->setValue(p.to.x());
        toY_->setValue(p.to.y());
        fromScale_->setValue(p.fromScale);
        toScale_->setValue(p.toScale);
        fromAngle_->setValue(p.fromAngle);
        toAngle_->setValue(p.toAngle);
        fromOpacity_->setValue(p.fromOpacity);
        toOpacity_->setValue(p.toOpacity);
        easing_->setCurrentIndex(p.easing);
        loop_->setChecked(p.loop);
        loop_->setVisible(p.artwork.frames.size() > 1);
        for (auto control : {toX_, toY_, toScale_, toAngle_, toOpacity_})
            control->setEnabled(p.duration > 1);
        easing_->setEnabled(p.duration > 1);
        findChild<QPushButton *>("compositionPlaceEnd")->setEnabled(p.duration > 1);
        if (p.duration == 1 && editEnd_) {
            editEnd_ = false;
            findChild<QPushButton *>("compositionPlaceStart")->setChecked(true);
        }
    }
    undo_->setEnabled(historyAt_ > 0);
    redo_->setEnabled(historyAt_ + 1 < history_.size());
    updatePreview();
}
void EffectComposer::updatePreview() {
    canvas_->model = model_;
    canvas_->tick = time_->value();
    canvas_->end = editEnd_;
    canvas_->selected =
        pieces_->currentItem() ? pieces_->currentItem()->data(Qt::UserRole).toInt() : -1;
    canvas_->update();
    timeLabel_->setText(QString("%1 / %2 ticks · %3 s")
                            .arg(time_->value())
                            .arg(model_.duration)
                            .arg(time_->value() / 30.0, 0, 'f', 2));
}
void EffectComposer::addPiece(SpriteAsset artwork) {
    artwork.provenance.remove("effectComposition");
    artwork.id = SpriteLibrary::newId();
    auto m = model_;
    EffectPiece p;
    p.id = SpriteLibrary::newId();
    p.name = artwork.name;
    p.artwork = std::move(artwork);
    p.start = time_->value();
    int sourceTicks = 0;
    for (const auto &frame : p.artwork.frames)
        sourceTicks += std::max(1, frame.ticks);
    const bool sequence = p.artwork.kind == "sequence" || p.artwork.frames.size() > 1;
    if (sequence && m.pieces.isEmpty())
        m.duration = std::max(
            m.duration,
            std::min({300, sourceTicks, 16 * 1024 * 1024 / (m.size.width() * m.size.height())}));
    p.duration = std::min(sequence ? sourceTicks : 30, m.duration - p.start);
    p.fromScale = p.toScale =
        std::clamp(96 * 100 / std::max(p.artwork.size.width(), p.artwork.size.height()), 1, 100);
    m.pieces << p;
    record(m);
    pieces_->setCurrentRow(0);
}
void EffectComposer::chooseEffect() {
    QDialog dialog(this);
    dialog.setWindowTitle("Choose a piece from an effect");
    dialog.resize(1080, 720);
    auto layout = new QVBoxLayout(&dialog);
    QUndoStack scratch;
    auto browser = new EffectLibraryEditor(project_, &scratch, [] {}, &dialog, true);
    layout->addWidget(browser);
    connect(browser, &EffectLibraryEditor::pieceChosen, &dialog,
            [this, &dialog](const SpriteAsset &piece) {
                guarded([&] {
                    addPiece(piece);
                    dialog.accept();
                });
            });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}
void EffectComposer::chooseLibrary() {
    QStringList labels, ids;
    for (const auto &a : project_->sprites().assets()) {
        labels << QString("%1 · %2").arg(labels.size() + 1).arg(a.name);
        ids << a.id;
    }
    if (ids.isEmpty())
        throw Error("The Art Library is empty. Add a piece from game effects or files.");
    bool ok = false;
    const auto name =
        QInputDialog::getItem(this, "Add a library piece", "Artwork", labels, 0, false, &ok);
    if (ok)
        addPiece(project_->sprites().asset(ids[labels.indexOf(name)]));
}
void EffectComposer::chooseFile() {
    const auto paths = QFileDialog::getOpenFileNames(
        this, "Add effect artwork", {}, "Artwork and sequences (*.png *.zip *.json *.lcsprite)");
    if (!paths.isEmpty())
        addPiece(importAnimationFiles(paths));
}
void EffectComposer::finish(bool assign) {
    guarded([this, assign] {
        asset();
        assignAfter_ = assign;
        accept();
    });
}
namespace {
class EffectAssignment : public QUndoCommand {
    Project *project_;
    QJsonObject before_, after_;

  public:
    EffectAssignment(Project *p, const QJsonObject &after)
        : QUndoCommand("Assign composed effect"), project_(p), before_(p->attacks()->json()),
          after_(after) {}
    void undo() override {
        project_->setAttacks(before_);
    }
    void redo() override {
        project_->setAttacks(after_);
    }
};
} // namespace
int assignComposedEffect(Project *project, QUndoStack *undo, const SpriteAsset &asset, int attack,
                         bool duplicate) {
    project->loadAttacks();
    auto data = *project->attacks();
    if (AttackData::isTemplate(attack))
        throw Error("Choose an actual attack, rather than an elemental spell template");
    if (duplicate)
        attack = data.duplicate(attack, asset.name);
    auto row = data.entry(attack);
    row.visual = {};
    row.visual.frames = asset;
    row.visual.frames->id = SpriteLibrary::newId();
    row.visual.validate();
    data.set(attack, row);
    undo->push(new EffectAssignment(project, data.json()));
    return attack;
}
void chooseEffectAttack(Project *project, QUndoStack *undo, const SpriteAsset &asset,
                        QWidget *parent) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Assign effect to an attack");
    dialog.resize(460, 560);
    auto layout = new QVBoxLayout(&dialog);
    auto search = new QLineEdit;
    search->setPlaceholderText("Find an attack…");
    layout->addWidget(search);
    auto list = new QListWidget;
    layout->addWidget(list, 1);
    for (int id = 1; id < project->attacks()->entries().size(); ++id)
        if (!AttackData::isTemplate(id)) {
            auto item = new QListWidgetItem(QString("%1 · %2").arg(id).arg(AttackData::displayName(
                                                project->attacks()->entry(id))),
                                            list);
            item->setData(Qt::UserRole, id);
        }
    list->setCurrentRow(0);
    QObject::connect(search, &QLineEdit::textChanged, &dialog, [list](const QString &text) {
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setHidden(!list->item(i)->text().contains(text, Qt::CaseInsensitive));
    });
    auto copy = new QCheckBox("Create a new attack from the selected attack");
    copy->setChecked(true);
    layout->addWidget(copy);
    note("A new attack keeps the selected attack's damage, targeting and class animation as its "
         "starting settings. "
         "Uncheck to replace the selected attack's effect for every class using it. Effect copies "
         "stay independent.",
         layout);
    auto notice = note({}, layout);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Assign effect");
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        try {
            if (!list->currentItem() || list->currentItem()->isHidden())
                throw Error("Select a visible attack");
            assignComposedEffect(project, undo, asset,
                                 list->currentItem()->data(Qt::UserRole).toInt(),
                                 copy->isChecked());
            dialog.accept();
        } catch (const std::exception &e) {
            notice->setText(QString::fromUtf8(e.what()));
        }
    });
    dialog.exec();
}
} // namespace lordly
