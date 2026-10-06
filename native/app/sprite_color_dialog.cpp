#include "app/sprite_color_dialog.h"
#include "engine/sprite_colors.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFutureWatcher>
#include <QLabel>
#include <QIcon>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>
#include <atomic>
#include <memory>
namespace lordly {
namespace {
struct Result {
    QVector<QRgb> colors;
    QString error;
};
QIcon swatch(QColor color) {
    QImage image(28, 28, QImage::Format_RGBA8888);
    QPainter p(&image);
    for (int y = 0; y < 28; y += 7)
        for (int x = 0; x < 28; x += 7)
            p.fillRect(x, y, 7, 7,
                       ((x / 7 + y / 7) & 1) ? QColor(170, 170, 170) : QColor(235, 235, 235));
    p.fillRect(image.rect(), color);
    p.end();
    return QIcon(QPixmap::fromImage(image));
}
} // namespace
std::optional<QColor> chooseSpriteUsedColor(QWidget *parent, const SpriteAsset &asset, int frame,
                                            int layer, QColor current) {
    QDialog dialog(parent);
    dialog.setObjectName("spriteUsedColorsDialog");
    dialog.setWindowTitle("Used sprite colors");
    dialog.resize(600, 520);
    auto layout = new QVBoxLayout(&dialog);
    auto scope = new QComboBox;
    scope->setObjectName("spriteColorScope");
    scope->addItems({"Selected layer", "Current frame", "Entire sequence"});
    layout->addWidget(scope);
    auto explanation = new QLabel("Stored colors include invisible layers and transparent pixels. "
                                  "Choose a swatch to reuse its exact RGBA value.");
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    auto status = new QLabel;
    status->setObjectName("spriteColorCount");
    layout->addWidget(status);
    auto grid = new QListWidget;
    grid->setObjectName("spriteUsedColorGrid");
    grid->setViewMode(QListView::IconMode);
    grid->setResizeMode(QListView::Adjust);
    grid->setMovement(QListView::Static);
    grid->setIconSize({28, 28});
    grid->setGridSize({36, 36});
    grid->setUniformItemSizes(true);
    grid->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(grid, 1);
    auto navigation = new QHBoxLayout;
    layout->addLayout(navigation);
    auto previous = new QPushButton("Previous"), next = new QPushButton("Next");
    previous->setObjectName("spriteColorsPrevious");
    next->setObjectName("spriteColorsNext");
    navigation->addWidget(previous);
    navigation->addStretch();
    navigation->addWidget(next);
    auto chosen = new QLabel;
    chosen->setObjectName("spriteChosenColor");
    layout->addWidget(chosen);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Use color");
    auto use = buttons->button(QDialogButtonBox::Ok);
    use->setEnabled(false);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QFutureWatcher<Result> watcher;
    std::shared_ptr<std::atomic_bool> cancel;
    QVector<QRgb> colors;
    constexpr int pageSize = 128;
    int page = 0, runningScope = -1;
    bool busy = false;
    std::optional<QColor> selected;
    const auto paintPage = [&] {
        grid->clear();
        selected.reset();
        use->setEnabled(false);
        chosen->clear();
        previous->setEnabled(!busy && page > 0);
        next->setEnabled(!busy && (page + 1) * pageSize < colors.size());
        if (busy)
            return;
        const int first = page * pageSize,
                  last = int(std::min(qsizetype(first + pageSize), colors.size()));
        status->setText(
            QString("%1 used colors - showing %2-%3").arg(colors.size()).arg(first + 1).arg(last));
        for (int i = first; i < last; ++i) {
            const auto color = QColor::fromRgba(colors.at(i));
            auto item = new QListWidgetItem(swatch(color), QString{}, grid);
            item->setData(Qt::UserRole, colors.at(i));
            const auto description = QString("RGBA %1, %2, %3, %4")
                                         .arg(color.red())
                                         .arg(color.green())
                                         .arg(color.blue())
                                         .arg(color.alpha());
            item->setToolTip(description);
            item->setData(Qt::AccessibleTextRole, description);
            if (color == current)
                grid->setCurrentItem(item);
        }
    };
    QObject::connect(grid, &QListWidget::currentItemChanged, &dialog, [&](QListWidgetItem *item) {
        if (!item || busy) {
            selected.reset();
            use->setEnabled(false);
            return;
        }
        selected = QColor::fromRgba(item->data(Qt::UserRole).toUInt());
        current = *selected;
        chosen->setText(item->toolTip());
        use->setEnabled(true);
    });
    QObject::connect(grid, &QListWidget::itemDoubleClicked, &dialog, [&] {
        if (selected)
            dialog.accept();
    });
    QObject::connect(previous, &QPushButton::clicked, &dialog, [&] {
        --page;
        paintPage();
    });
    QObject::connect(next, &QPushButton::clicked, &dialog, [&] {
        ++page;
        paintPage();
    });
    std::function<void()> start;
    start = [&] {
        busy = true;
        colors.clear();
        page = 0;
        paintPage();
        status->setText("Collecting colors...");
        grid->setEnabled(false);
        runningScope = scope->currentIndex();
        cancel = std::make_shared<std::atomic_bool>(false);
        watcher.setFuture(QtConcurrent::run(
            [snapshot = asset, frame, layer, value = runningScope, token = cancel] {
                Result result;
                try {
                    auto values = spriteUsedColors(snapshot, SpriteColorScope(value), frame, layer,
                                                   [&] { return token->load(); });
                    if (values)
                        result.colors = std::move(*values);
                } catch (const std::exception &e) {
                    result.error = QString::fromUtf8(e.what());
                }
                return result;
            }));
    };
    QObject::connect(&watcher, &QFutureWatcher<Result>::finished, &dialog, [&] {
        if (cancel->load() || runningScope != scope->currentIndex()) {
            start();
            return;
        }
        auto result = watcher.result();
        busy = false;
        grid->setEnabled(true);
        if (!result.error.isEmpty()) {
            status->setText(result.error);
            return;
        }
        colors = std::move(result.colors);
        const auto found = std::lower_bound(colors.cbegin(), colors.cend(), current.rgba());
        if (found != colors.cend() && *found == current.rgba())
            page = int(found - colors.cbegin()) / pageSize;
        paintPage();
        dialog.setProperty("colorsReady", true);
    });
    QObject::connect(scope, &QComboBox::currentIndexChanged, &dialog, [&] {
        dialog.setProperty("colorsReady", false);
        if (cancel)
            cancel->store(true);
        if (!busy)
            start();
        else {
            busy = true;
            paintPage();
            grid->setEnabled(false);
            status->setText("Collecting colors...");
        }
    });
    start();
    const auto accepted = dialog.exec() == QDialog::Accepted;
    if (cancel)
        cancel->store(true);
    return accepted ? selected : std::nullopt;
}
} // namespace lordly
