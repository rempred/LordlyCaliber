#include "app/actor_pose_editor.h"
#include "engine/class_data.h"
#include "engine/text.h"
#include <QComboBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
namespace lordly {
namespace {
class PoseView : public QGraphicsView {
  public:
    using QGraphicsView::QGraphicsView;

  protected:
    void wheelEvent(QWheelEvent *event) override {
        const double factor = event->angleDelta().y() > 0 ? 1.2 : 1 / 1.2;
        const double next = transform().m11() * factor;
        if (next >= .05 && next <= 64)
            scale(factor, factor);
        event->accept();
    }
};
} // namespace
ActorPoseEditor::ActorPoseEditor(const Rom &rom, QWidget *parent) : QWidget(parent), rom_(rom) {
    auto layout = new QVBoxLayout(this);
    auto note = new QLabel(
        "Actor pose preview. Playback speed is adjustable; scene timing and cameras are separate. "
        "Audio requests are recorded without sound. Material values are listed; their drawing "
        "effects are not applied yet. Class-body inspection uses no linked equipment actors.");
    note->setWordWrap(true);
    layout->addWidget(note);
    auto selectors = new QHBoxLayout;
    kinds_ = new QComboBox;
    kinds_->setObjectName("actorSourceKind");
    kinds_->addItems({"Cutscene actors", "Class bodies"});
    owners_ = new QComboBox;
    owners_->setObjectName("actorOwner");
    flags_ = new QComboBox;
    flags_->setObjectName("actorBodyFlags");
    flags_->addItems({"0 / 0", "0 / 1", "1 / 0", "1 / 1"});
    banks_ = new QComboBox;
    banks_->setObjectName("actorBank");
    banks_->setMinimumContentsLength(20);
    banks_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    poses_ = new QComboBox;
    poses_->setObjectName("actorPose");
    poses_->setMinimumContentsLength(28);
    variant_ = new QSpinBox;
    variant_->setObjectName("actorVariant");
    variant_->setRange(0, 255);
    selectors->addWidget(kinds_);
    selectors->addWidget(banks_);
    selectors->addWidget(new QLabel("Owner class"));
    selectors->addWidget(owners_, 1);
    selectors->addWidget(new QLabel("Body flags"));
    selectors->addWidget(flags_);
    layout->addLayout(selectors);
    auto poseSelectors = new QHBoxLayout;
    poseSelectors->addWidget(new QLabel("Pose"));
    poseSelectors->addWidget(poses_, 1);
    poseSelectors->addWidget(new QLabel("Image variant"));
    poseSelectors->addWidget(variant_);
    layout->addLayout(poseSelectors);
    auto controls = new QHBoxLayout;
    play_ = new QPushButton("Play");
    play_->setObjectName("actorPlay");
    play_->setCheckable(true);
    auto next = new QPushButton("Step"), reset = new QPushButton("Restart"),
         fit = new QPushButton("Fit"), actual = new QPushButton("Actual pixels");
    next->setObjectName("actorStep");
    reset->setObjectName("actorRestart");
    position_ = new QSpinBox;
    position_->setObjectName("actorPosition");
    position_->setRange(0, 10000);
    speed_ = new QSpinBox;
    speed_->setRange(1, 120);
    speed_->setValue(30);
    speed_->setSuffix(" steps/s");
    controls->addWidget(play_);
    controls->addWidget(next);
    controls->addWidget(reset);
    controls->addWidget(new QLabel("Step"));
    controls->addWidget(position_);
    controls->addWidget(speed_);
    controls->addStretch();
    controls->addWidget(fit);
    controls->addWidget(actual);
    layout->addLayout(controls);
    auto split = new QSplitter;
    canvas_ = new QGraphicsScene(this);
    view_ = new PoseView(canvas_);
    view_->setObjectName("actorCanvas");
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    QPixmap checker(24, 24);
    checker.fill(QColor("#252931"));
    {
        QPainter p(&checker);
        p.fillRect(0, 0, 12, 12, QColor("#2e333c"));
        p.fillRect(12, 12, 12, 12, QColor("#2e333c"));
    }
    view_->setBackgroundBrush(checker);
    split->addWidget(view_);
    auto layerPanel = new QWidget;
    auto layerLayout = new QVBoxLayout(layerPanel);
    layerLayout->addWidget(new QLabel("Frame layers"));
    layers_ = new QListWidget;
    layers_->setObjectName("actorLayers");
    layerLayout->addWidget(layers_, 1);
    selection_ = new QLabel;
    selection_->setWordWrap(true);
    layerLayout->addWidget(selection_);
    split->addWidget(layerPanel);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 1);
    layout->addWidget(split, 1);
    status_ = new QLabel;
    status_->setObjectName("actorStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &ActorPoseEditor::step);
    connect(play_, &QPushButton::toggled, this, [this](bool playing) {
        if (playing && player_) {
            timer_->start(qMax(1, qRound(1000. / speed_->value())));
            play_->setText("Pause");
        } else {
            timer_->stop();
            play_->setText("Play");
        }
    });
    connect(speed_, &QSpinBox::valueChanged, this, [this] {
        if (timer_->isActive())
            timer_->setInterval(qMax(1, qRound(1000. / speed_->value())));
    });
    connect(next, &QPushButton::clicked, this, [this] {
        stop();
        step();
    });
    connect(reset, &QPushButton::clicked, this, &ActorPoseEditor::restart);
    connect(position_, &QSpinBox::valueChanged, this, &ActorPoseEditor::seek);
    connect(banks_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::loadBank);
    connect(kinds_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::loadSources);
    connect(owners_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::loadBank);
    connect(flags_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::loadBank);
    connect(poses_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::restart);
    connect(variant_, &QSpinBox::valueChanged, this, &ActorPoseEditor::restart);
    connect(fit, &QPushButton::clicked, this,
            [this] { view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio); });
    connect(actual, &QPushButton::clicked, this, [this] { view_->resetTransform(); });
    connect(layers_, &QListWidget::currentRowChanged, this, [this](int row) {
        for (int i = 0; i < items_.size(); ++i)
            items_[i]->setSelected(i == row);
        selection_->setText(row >= 0 && layers_->item(row) ? layers_->item(row)->toolTip()
                                                           : QString{});
    });
    connect(layers_, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const int row = layers_->row(item);
        if (row >= 0 && row < items_.size())
            items_[row]->setVisible(item->checkState() == Qt::Checked);
    });
    connect(canvas_, &QGraphicsScene::selectionChanged, this, [this] {
        for (int i = 0; i < items_.size(); ++i)
            if (items_[i]->isSelected()) {
                QSignalBlocker block(layers_);
                layers_->setCurrentRow(i);
                selection_->setText(layers_->item(i)->toolTip());
                break;
            }
    });
    loadSources();
}
void ActorPoseEditor::stop() {
    play_->setChecked(false);
    timer_->stop();
}
void ActorPoseEditor::loadSources() {
    stop();
    const bool body = kinds_->currentIndex() == 1;
    owners_->setEnabled(body);
    flags_->setEnabled(body);
    try {
        QSignalBlocker banksBlock(banks_), ownerBlock(owners_);
        banks_->clear();
        owners_->clear();
        owners_->addItem("Same as source", -1);
        if (body) {
            const auto classes = ClassData::load(rom_);
            for (int i = 0; i < classes.entries().size(); ++i) {
                const auto label =
                    QString("%1 · %2").arg(i).arg(decodeText(classes.entries()[i].name));
                banks_->addItem(label, i);
                owners_->addItem(label, i);
            }
            banks_->setCurrentIndex(banks_->findData(1));
        } else {
            for (int i = 1; i <= ActorArt::bankCount(rom_); ++i)
                banks_->addItem(QString("Bank %1").arg(i), i);
        }
    } catch (const Error &e) {
        status_->setText(QString::fromUtf8(e.what()));
        return;
    }
    loadBank();
}
void ActorPoseEditor::loadBank() {
    stop();
    art_.reset();
    player_.reset();
    try {
        const int source = banks_->currentData().toInt(), owner = owners_->currentData().toInt();
        const bool body = kinds_->currentIndex() == 1;
        art_ = body ? ActorArt::loadBody(rom_, source, owner < 0 ? source : owner,
                                         flags_->currentIndex() >> 1, flags_->currentIndex() & 1)
                    : ActorArt::load(rom_, source);
        if (body) {
            QSignalBlocker variantBlock(variant_);
            variant_->setValue(art_->bodyVariant());
        }
        QSignalBlocker block(poses_);
        poses_->clear();
        for (const auto &s : body ? QVector<ActorPoseSelection>{} : art_->selections())
            poses_->addItem(QString("Animation %1 · direction %2 · state %3")
                                .arg(s.animation)
                                .arg(s.facing)
                                .arg(s.state),
                            s.state);
        for (int state = 0; state < art_->poses().size(); ++state)
            if (poses_->findData(state) < 0)
                poses_->addItem(body ? QString("State %1").arg(state)
                                     : QString("State %1 · no animation key").arg(state),
                                state);
        fit_ = true;
    } catch (const Error &e) {
        status_->setText(QString::fromUtf8(e.what()));
        items_.clear();
        canvas_->clear();
        layers_->clear();
        return;
    }
    restart();
}
void ActorPoseEditor::restart() {
    stop();
    if (!art_ || poses_->currentIndex() < 0)
        return;
    player_.emplace(rom_, *art_, poses_->currentData().toInt(), variant_->value());
    if (art_->isBody())
        player_->setLinkedObjectKnownAbsent(true);
    step_ = 0;
    {
        QSignalBlocker block(position_);
        position_->setValue(0);
    }
    draw();
}
void ActorPoseEditor::seek(int target) {
    stop();
    if (!art_)
        return;
    if (!player_ || target < step_ || player_->suspended())
        restart();
    while (step_ < target) {
        const auto result = player_->advance();
        if (result != LORDLY_POSE_OK) {
            draw();
            status_->setText(player_->error());
            return;
        }
        ++step_;
    }
    {
        QSignalBlocker block(position_);
        position_->setValue(step_);
    }
    draw();
}
void ActorPoseEditor::step() {
    if (!player_ || step_ >= position_->maximum()) {
        stop();
        return;
    }
    if (player_->advance() != LORDLY_POSE_OK) {
        stop();
        draw();
        status_->setText(player_->error());
        return;
    }
    ++step_;
    {
        QSignalBlocker block(position_);
        position_->setValue(step_);
    }
    draw();
}
void ActorPoseEditor::draw() {
    // Drop observers before deleting selected scene items (selectionChanged is synchronous).
    items_.clear();
    canvas_->clear();
    layers_->clear();
    selection_->clear();
    if (!player_)
        return;
    try {
        const auto &a = player_->actor();
        const auto frame = art_->frame(rom_, a.frame, variant_->value());
        QRectF bounds;
        int fallbacks = 0;
        for (int i = 0; i < frame.size(); ++i) {
            const auto &layer = frame[i];
            const auto &r = layer.pixels.image;
            auto pixmap =
                QPixmap::fromImage(QImage(reinterpret_cast<const uchar *>(r.rgba.constData()),
                                          r.width, r.height, r.width * 4, QImage::Format_RGBA8888));
            auto item = canvas_->addPixmap(pixmap);
            const auto &m = layer.metadata;
            item->setPos(m.origin.x() + a.position.x, m.origin.y() - a.position.y);
            item->setTransform(QTransform::fromScale(m.scaleX / 1024., m.scaleY / 1024.));
            item->setZValue(i);
            item->setFlag(QGraphicsItem::ItemIsSelectable);
            items_ << item;
            bounds |= item->mapRectToScene(QRectF(0, 0, m.width, m.height));
            auto row = new QListWidgetItem(QString("Layer %1 · art %2").arg(i).arg(m.art), layers_);
            row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
            row->setCheckState(Qt::Checked);
            const bool fallback = layer.pixels.requestedChild != layer.pixels.child;
            fallbacks += fallback;
            row->setToolTip(
                QString("Origin %1, %2 · size %3 × %4\nScale %5, %6 · flags %7\nVariant %8 → "
                        "%9%10\nMaterial %11")
                    .arg(m.origin.x())
                    .arg(m.origin.y())
                    .arg(m.width)
                    .arg(m.height)
                    .arg(m.scaleX / 1024.)
                    .arg(m.scaleY / 1024.)
                    .arg(m.flags)
                    .arg(layer.pixels.requestedChild)
                    .arg(layer.pixels.child)
                    .arg(fallback ? " (fallback)" : "")
                    .arg(i < 16 ? QString::number(a.material[i]) : QString("unmapped")));
        }
        canvas_->setSceneRect(bounds.isEmpty() ? QRectF(-32, -32, 64, 64)
                                               : bounds.adjusted(-8, -8, 8, 8));
        if (fit_) {
            view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio);
            fit_ = false;
        }
        status_->setText(
            QString("Step %1 · state %2 · record %3 · delay %4 · frame %5 · position %6, %7, "
                    "%8\n%9 layers · %10 variant fallbacks · %11 audio requests this step")
                .arg(step_)
                .arg(a.state)
                .arg(a.cursor)
                .arg(a.delay)
                .arg(a.frame)
                .arg(a.position.x)
                .arg(a.position.y)
                .arg(a.position.z)
                .arg(frame.size())
                .arg(fallbacks)
                .arg(player_->audioRequests().size()));
        setProperty("frameToken", a.frame);
        setProperty("poseCursor", a.cursor);
        setProperty("poseDelay", a.delay);
    } catch (const Error &e) {
        stop();
        status_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
