#include "app/consumable_visual_dialog.h"
#include "app/effect_library_editor.h"
#include "app/rom_art_import.h"
#include "engine/animation_file_import.h"
#include "engine/consumable_tree.h"
#include "engine/consumable_visual.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
QPixmap enlarged(const QImage &image) {
    QImage canvas(160, 160, QImage::Format_RGB32);
    QPainter painter(&canvas);
    for (int y = 0; y < 160; y += 10)
        for (int x = 0; x < 160; x += 10)
            painter.fillRect(x, y, 10, 10,
                             ((x + y) / 10 & 1) ? QColor("#303641") : QColor("#242932"));
    const auto scaled = image.scaled(160, 160, Qt::KeepAspectRatio, Qt::FastTransformation);
    painter.drawImage((160 - scaled.width()) / 2, (160 - scaled.height()) / 2, scaled);
    painter.end();
    return QPixmap::fromImage(canvas);
}
class VisualDialog : public QDialog {
    const Project &project_;
    ConsumableVisual visual_, opened_;
    QVector<QImage> source_;
    QLineEdit *name_;
    QListWidget *frames_;
    QLabel *sourcePreview_, *gamePreview_, *error_;
    QCheckBox *smooth_, *loop_;
    QPushButton *play_;
    QTimer timer_;
    int tick_ = 0;
    void guard(const std::function<void()> &work) {
        try {
            work();
            error_->clear();
        } catch (const std::exception &e) {
            error_->setText(QString::fromUtf8(e.what()));
        }
    }
    void stop() {
        timer_.stop();
        play_->setText("Play");
    }
    void display() {
        const int frame = std::max(0, frames_->currentRow());
        sourcePreview_->setPixmap(enlarged(source_[frame]));
        gamePreview_->setPixmap(enlarged(visual_.frame(frame)));
    }
    void refresh(bool convert = true) {
        if (convert) {
            visual_ = ConsumableVisual::fromImages(
                name_->text().trimmed().isEmpty() ? "Item effect" : name_->text(), source_,
                smooth_->isChecked());
        }
        for (int n = 0; n < 4; ++n)
            frames_->item(n)->setIcon(QIcon(enlarged(visual_.frame(n))));
        display();
    }
    void setVisual(const ConsumableVisual &visual) {
        stop();
        visual_ = visual;
        source_.clear();
        for (int n = 0; n < 4; ++n)
            source_ << visual.frame(n);
        name_->setText(visual.name);
        refresh(false);
    }
    void import(SpriteAsset asset, bool replace) {
        asset.validate();
        stop();
        const int count = replace ? 1 : 4;
        QVector<int> indices;
        for (int n = 0; n < count; ++n)
            indices << (replace ? 0 : n * (asset.frames.size() - 1) / 3);
        if (asset.frames.size() > 1) {
            QDialog choose(this);
            choose.setWindowTitle(replace ? "Choose a source frame" : "Choose four source frames");
            auto layout = new QVBoxLayout(&choose);
            auto row = new QHBoxLayout;
            layout->addLayout(row);
            for (int n = 0; n < count; ++n) {
                auto column = new QVBoxLayout;
                row->addLayout(column);
                column->addWidget(
                    new QLabel(replace ? "Source frame" : QString("Effect frame %1").arg(n + 1)));
                auto spin = new QSpinBox;
                spin->setRange(1, asset.frames.size());
                spin->setValue(indices[n] + 1);
                spin->setKeyboardTracking(false);
                column->addWidget(spin);
                auto preview = new QLabel;
                preview->setPixmap(enlarged(
                    ConsumableVisual::fit(asset.composite(indices[n]), smooth_->isChecked())));
                column->addWidget(preview);
                connect(spin, &QSpinBox::valueChanged, &choose, [&, n, preview](int value) {
                    indices[n] = value - 1;
                    preview->setPixmap(enlarged(
                        ConsumableVisual::fit(asset.composite(value - 1), smooth_->isChecked())));
                });
            }
            auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            layout->addWidget(buttons);
            connect(buttons, &QDialogButtonBox::accepted, &choose, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &choose, &QDialog::reject);
            if (choose.exec() != QDialog::Accepted)
                return;
        }
        if (replace)
            source_[std::max(0, frames_->currentRow())] = asset.composite(indices[0]);
        else
            for (int n = 0; n < 4; ++n)
                source_[n] = asset.composite(indices[n]);
        if (!replace)
            name_->setText((asset.name + " item effect").left(80));
        refresh();
    }
    void chooseArtwork(bool replace) {
        Project copy = project_;
        QUndoStack undo;
        QDialog dialog(this);
        dialog.setObjectName("itemUseArtworkPicker");
        dialog.setWindowTitle(replace ? "Replace frame · Effect artwork"
                                      : "Import sequence · Effect artwork");
        dialog.resize(1050, 760);
        auto layout = new QVBoxLayout(&dialog);
        auto alternatives = new QHBoxLayout;
        layout->addLayout(alternatives);
        auto otherRom = new QPushButton("Other ROM sprites and images…");
        otherRom->setObjectName("itemUseOtherRomArt");
        auto library = new QPushButton("Art Library…");
        auto files = new QPushButton("Files…");
        alternatives->addStretch();
        for (auto button : {otherRom, library, files})
            alternatives->addWidget(button);
        std::optional<SpriteAsset> selected;
        auto use = [&](const SpriteAsset &asset) {
            selected = asset;
            dialog.accept();
        };
        auto status = new QLabel;
        status->setWordWrap(true);
        try {
            auto picker = new EffectLibraryEditor(&copy, &undo, [] {}, &dialog, true);
            layout->addWidget(picker, 1);
            connect(picker, &EffectLibraryEditor::pieceChosen, &dialog, use);
        } catch (const std::exception &e) {
            status->setText(QString::fromUtf8(e.what()));
        }
        layout->addWidget(status);
        auto close = new QDialogButtonBox(QDialogButtonBox::Cancel);
        layout->addWidget(close);
        connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(otherRom, &QPushButton::clicked, &dialog, [&] {
            try {
                RomArtImport source(project_.runtimeRom(), use, &dialog,
                                    "Choose artwork for the item's 32 × 32 use animation.");
                source.exec();
            } catch (const std::exception &e) {
                status->setText(QString::fromUtf8(e.what()));
            }
        });
        QStringList labels, ids;
        for (const auto &asset : project_.sprites().assets()) {
            if (!asset.frames.isEmpty()) {
                labels << QString("%1 · %2").arg(labels.size() + 1).arg(asset.name);
                ids << asset.id;
            }
        }
        library->setEnabled(!ids.isEmpty());
        connect(library, &QPushButton::clicked, &dialog, [&] {
            bool ok;
            const auto label = QInputDialog::getItem(&dialog, "Choose artwork", "Art Library",
                                                     labels, 0, false, &ok);
            if (ok)
                use(project_.sprites().asset(ids.at(labels.indexOf(label))));
        });
        connect(files, &QPushButton::clicked, &dialog, [&] {
            const auto paths = QFileDialog::getOpenFileNames(
                &dialog, "Import artwork", {},
                "Images and frame sequences (*.png *.apng *.zip *.webm)");
            if (!paths.isEmpty()) {
                try {
                    use(importAnimationFiles(paths));
                } catch (const std::exception &e) {
                    status->setText(QString::fromUtf8(e.what()));
                }
            }
        });
        if (dialog.exec() == QDialog::Accepted && selected)
            import(std::move(*selected), replace);
    }

  public:
    QJsonObject result;
    VisualDialog(QWidget *parent, const Project &project, int item)
        : QDialog(parent), project_(project) {
        setObjectName("consumableVisualDialog");
        setWindowTitle("Use animation · " +
                       project.record(Table::Consumables, item).name.simplified());
        resize(760, 570);
        opened_ =
            project.consumableEffects()->visuals().contains(QString::number(item))
                ? ConsumableVisual::fromJson(
                      project.consumableEffects()->visuals()[QString::number(item)].toObject())
                : ConsumableVisual::retail(project.runtimeRom(), item);
        auto layout = new QVBoxLayout(this);
        auto form = new QFormLayout;
        layout->addLayout(form);
        name_ = new QLineEdit;
        name_->setObjectName("itemVisualName");
        name_->setMaxLength(80);
        form->addRow("Effect name", name_);
        auto tools = new QHBoxLayout;
        layout->addLayout(tools);
        auto sequence = new QPushButton("Import sequence…"),
             frame = new QPushButton("Replace frame…"), copy = new QPushButton("Copy from item…"),
             restore = new QPushButton("Restore opened animation");
        sequence->setObjectName("itemVisualImportSequence");
        frame->setObjectName("itemVisualReplaceFrame");
        for (auto button : {sequence, frame, copy, restore})
            tools->addWidget(button);
        frames_ = new QListWidget;
        frames_->setObjectName("itemVisualFrames");
        frames_->setViewMode(QListView::IconMode);
        frames_->setMovement(QListView::Static);
        frames_->setIconSize({96, 96});
        frames_->setGridSize({150, 125});
        frames_->setFixedHeight(140);
        for (int n = 0; n < 4; ++n)
            frames_->addItem(QString("Frame %1").arg(n + 1));
        layout->addWidget(frames_);
        auto previews = new QHBoxLayout;
        layout->addLayout(previews, 1);
        for (auto pair : {std::pair{"Source", &sourcePreview_},
                          std::pair{"Game colours · 32 × 32", &gamePreview_}}) {
            auto column = new QVBoxLayout;
            previews->addLayout(column);
            column->addWidget(new QLabel(pair.first), 0, Qt::AlignCenter);
            *pair.second = new QLabel;
            (*pair.second)->setAlignment(Qt::AlignCenter);
            column->addWidget(*pair.second, 1);
        }
        auto playback = new QHBoxLayout;
        layout->addLayout(playback);
        play_ = new QPushButton("Play");
        auto step = new QPushButton("Step");
        loop_ = new QCheckBox("Loop");
        smooth_ = new QCheckBox("Smooth downsizing");
        smooth_->setChecked(true);
        smooth_->setObjectName("itemVisualSmooth");
        playback->addWidget(play_);
        playback->addWidget(step);
        playback->addWidget(loop_);
        playback->addStretch();
        playback->addWidget(smooth_);
        auto limits =
            new QLabel("Four frames share 16 colours. Artwork is fitted without stretching; native "
                       "movement, fading and result displays are retained.");
        limits->setWordWrap(true);
        layout->addWidget(limits);
        error_ = new QLabel;
        error_->setWordWrap(true);
        layout->addWidget(error_);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        connect(sequence, &QPushButton::clicked, this,
                [this] { guard([&] { chooseArtwork(false); }); });
        connect(frame, &QPushButton::clicked, this,
                [this] { guard([&] { chooseArtwork(true); }); });
        connect(frames_, &QListWidget::currentRowChanged, this, [this] {
            if (!source_.isEmpty())
                display();
        });
        connect(smooth_, &QCheckBox::toggled, this, [this] { guard([&] { refresh(); }); });
        connect(restore, &QPushButton::clicked, this, [this] { setVisual(opened_); });
        connect(copy, &QPushButton::clicked, this, [this] {
            guard([&] {
                QStringList labels;
                QVector<int> ids;
                for (const auto &record : project_.records(Table::Consumables))
                    if (ConsumableTree::supported(record.id) || record.id == 20) {
                        labels << QString("%1 · %2").arg(record.id).arg(record.name.simplified());
                        ids << record.id;
                    }
                bool ok;
                auto selected =
                    QInputDialog::getItem(this, "Copy item visual", "Item", labels, 0, false, &ok);
                if (!ok)
                    return;
                const int id = ids[labels.indexOf(selected)];
                const auto visuals = project_.consumableEffects()->visuals();
                setVisual(visuals.contains(QString::number(id))
                              ? ConsumableVisual::fromJson(visuals[QString::number(id)].toObject())
                              : ConsumableVisual::retail(project_.runtimeRom(), id));
            });
        });
        timer_.setInterval(100); // Native four-frame sparkle advances every three 30 Hz ticks.
        connect(&timer_, &QTimer::timeout, this, [this] {
            if (++tick_ >= 4) {
                if (!loop_->isChecked()) {
                    stop();
                    return;
                }
                tick_ = 0;
            }
            frames_->setCurrentRow(tick_);
        });
        connect(play_, &QPushButton::clicked, this, [this] {
            if (timer_.isActive())
                stop();
            else {
                tick_ = 0;
                frames_->setCurrentRow(0);
                play_->setText("Pause");
                timer_.start();
            }
        });
        connect(step, &QPushButton::clicked, this, [this] {
            stop();
            frames_->setCurrentRow((frames_->currentRow() + 1) % 4);
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            guard([&] {
                visual_.name = name_->text().trimmed();
                result = visual_.json();
                accept();
            });
        });
        frames_->setCurrentRow(0);
        setVisual(opened_);
    }
};
} // namespace
std::optional<QJsonObject> editConsumableVisual(QWidget *parent, const Project &project, int item) {
    VisualDialog dialog(parent, project, item);
    if (dialog.exec() == QDialog::Accepted)
        return dialog.result;
    return {};
}
} // namespace lordly
