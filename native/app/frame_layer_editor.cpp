#include "app/frame_layer_editor.h"
#include "app/recovery_draft.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
class FrameLayerDialog final : public QDialog {
  public:
    FrameLayerDialog(const QByteArray &frame, const QByteArray &newLayer, int artCount,
                     std::function<QVector<ActorFrameLayer>(const QByteArray &)> render,
                     QWidget *parent)
        : QDialog(parent), artCount_(artCount), render_(std::move(render)) {
        setObjectName("sequenceLayerDialog");
        setWindowTitle("Edit frame layers");
        resize(1040, 760);
        for (int i = 0; i < u16(frame, 0); ++i)
            layers_ << frame.mid(2 + i * 16, 16);
        auto layout = new QVBoxLayout(this);
        auto note = new QLabel(
            "Edit this frame's layers. The original frame is retained. Frame-substitution poses "
            "can also change artwork used in other poses. Sound and motion commands are not "
            "played in this frame preview.");
        note->setWordWrap(true);
        layout->addWidget(note);
        table_ = new QTableWidget(0, 9);
        table_->setObjectName("frameLayers");
        table_->setHorizontalHeaderLabels(
            {"Sprite", "X", "Y", "Width", "Height", "Flip X", "Flip Y", "Scale X", "Scale Y"});
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        layout->addWidget(table_, 1);
        auto buttons = new QHBoxLayout;
        const auto add = [&](const QString &label, const char *name,
                             const std::function<void()> &action) {
            auto button = new QPushButton(label);
            button->setObjectName(name);
            buttons->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, action] { guarded(action); });
        };
        add("Add layer", "layerAdd", [this, newLayer] {
            if (layers_.size() >= 4096)
                throw Error("A frame can contain at most 4096 layers");
            layers_ << newLayer;
            refresh(layers_.size() - 1);
        });
        add("Duplicate layer", "layerDuplicate", [this] {
            const int row = selected();
            if (layers_.size() >= 4096)
                throw Error("A frame can contain at most 4096 layers");
            layers_.insert(row + 1, layers_[row]);
            refresh(row + 1);
        });
        add("Remove layer", "layerRemove", [this] {
            const int row = selected();
            layers_.removeAt(row);
            refresh(std::min(row, int(layers_.size()) - 1));
        });
        add("Move up", "layerMoveUp", [this] {
            const int row = selected();
            if (row == 0)
                return;
            layers_.swapItemsAt(row, row - 1);
            refresh(row - 1);
        });
        add("Move down", "layerMoveDown", [this] {
            const int row = selected();
            if (row + 1 == layers_.size())
                return;
            layers_.swapItemsAt(row, row + 1);
            refresh(row + 1);
        });
        buttons->addStretch();
        layout->addLayout(buttons);
        scene_ = new QGraphicsScene(this);
        view_ = new QGraphicsView(scene_);
        view_->setObjectName("layerPreview");
        view_->setMinimumHeight(260);
        view_->setBackgroundBrush(QColor("#272c35"));
        layout->addWidget(view_, 2);
        notice_ = new QLabel;
        notice_->setObjectName("layerNotice");
        notice_->setWordWrap(true);
        layout->addWidget(notice_);
        auto actions = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        actions->setObjectName("layerDialogButtons");
        connect(actions, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(actions, &QDialogButtonBox::accepted, this, [this] {
            guarded([this] {
                render_(encoded());
                accept();
            });
        });
        layout->addWidget(actions);
        refresh(layers_.isEmpty() ? -1 : 0);
    }
    QByteArray encoded() const {
        QByteArray bytes(2, 0);
        put16(bytes, 0, quint16(layers_.size()));
        for (const auto &layer : layers_)
            bytes += layer;
        return bytes;
    }

  private:
    int selected() const {
        const int row = table_->currentRow();
        if (row < 0 || row >= layers_.size())
            throw Error("Choose a frame layer");
        return row;
    }
    void guarded(const std::function<void()> &action) {
        try {
            action();
            notice_->clear();
        } catch (const std::exception &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    }
    void redraw() {
        const auto frame = render_(encoded());
        scene_->clear();
        for (const auto &layer : frame) {
            const auto &pixels = layer.pixels.image;
            const auto &m = layer.metadata;
            const QImage image(reinterpret_cast<const uchar *>(pixels.rgba.constData()),
                               pixels.width, pixels.height, pixels.width * 4,
                               QImage::Format_RGBA8888);
            auto item = scene_->addPixmap(QPixmap::fromImage(image));
            item->setTransformationMode(Qt::FastTransformation);
            item->setPos(m.origin.x() * m.scaleX / 1024., m.origin.y() * m.scaleY / 1024.);
            item->setTransform(QTransform::fromScale(m.scaleX / 1024., m.scaleY / 1024.));
        }
        QPen origin(QColor("#f0ab5a"));
        origin.setCosmetic(true);
        scene_->addLine(-4, 0, 4, 0, origin);
        scene_->addLine(0, -4, 0, 4, origin);
        auto bounds = scene_->itemsBoundingRect();
        if (bounds.isEmpty())
            bounds = QRectF(-32, -32, 64, 64);
        scene_->setSceneRect(bounds.adjusted(-12, -12, 12, 12));
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    }
    void refresh(int selectedRow) {
        table_->setRowCount(0);
        table_->setRowCount(layers_.size());
        for (int row = 0; row < layers_.size(); ++row) {
            for (int column = 0; column < 5; ++column) {
                const int offset = column * 2;
                auto box = new QSpinBox;
                box->setObjectName(QString("layerField%1_%2").arg(row).arg(column));
                box->setRange(column == 0   ? 0
                              : column <= 2 ? -32768
                                            : 1,
                              column == 0   ? artCount_ - 1
                              : column <= 2 ? 32767
                                            : 65535);
                box->setValue(column == 1 || column == 2 ? qint16(u16(layers_[row], offset))
                                                         : u16(layers_[row], offset));
                connect(box, &QSpinBox::valueChanged, this, [this, row, offset](int value) {
                    put16(layers_[row], offset, quint16(value));
                    guarded([this] { redraw(); });
                });
                table_->setCellWidget(row, column, box);
            }
            for (int column = 5; column < 7; ++column) {
                auto box = new QCheckBox;
                box->setObjectName(QString("layerFlip%1_%2").arg(row).arg(column - 5));
                const int bit = 1 << (column - 5);
                box->setChecked(u16(layers_[row], 10) & bit);
                connect(box, &QCheckBox::toggled, this, [this, row, bit](bool enabled) {
                    put16(layers_[row], 10,
                          quint16((u16(layers_[row], 10) & ~bit) | (enabled ? bit : 0)));
                    guarded([this] { redraw(); });
                });
                table_->setCellWidget(row, column, box);
            }
            for (int column = 7; column < 9; ++column) {
                const int offset = 12 + (column - 7) * 2;
                auto box = new QDoubleSpinBox;
                box->setObjectName(QString("layerScale%1_%2").arg(row).arg(column - 7));
                box->setDecimals(5);
                box->setRange(0, 65535. / 1024);
                box->setSingleStep(.125);
                box->setValue(u16(layers_[row], offset) / 1024.);
                connect(box, &QDoubleSpinBox::valueChanged, this,
                        [this, row, offset](double value) {
                            put16(layers_[row], offset, quint16(qRound(value * 1024)));
                            guarded([this] { redraw(); });
                        });
                table_->setCellWidget(row, column, box);
            }
        }
        if (selectedRow >= 0)
            table_->selectRow(selectedRow);
        guarded([this] { redraw(); });
    }
    int artCount_;
    QVector<QByteArray> layers_;
    std::function<QVector<ActorFrameLayer>(const QByteArray &)> render_;
    QTableWidget *table_;
    QGraphicsScene *scene_;
    QGraphicsView *view_;
    QLabel *notice_;
};
} // namespace
std::optional<QByteArray>
editFrameLayers(const QByteArray &frame, const QByteArray &newLayer, int artCount,
                const std::function<QVector<ActorFrameLayer>(const QByteArray &)> &render,
                QWidget *parent,const std::function<QJsonObject(const QByteArray &)> &recover) {
    if (artCount < 1 || newLayer.size() != 16 || frame.size() < 2 ||
        frame.size() != 2 + u16(frame, 0) * 16)
        throw Error("Invalid editable frame layers");
    FrameLayerDialog dialog(frame, newLayer, artCount, render, parent);
    RecoveryDraft recovery(&dialog,"Animation frame layers",[&]{
        const auto changed=dialog.encoded();return !recover||changed==frame?QJsonObject{}:recover(changed);
    });
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return dialog.encoded();
}
} // namespace lordly
