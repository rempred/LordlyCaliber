#include "app/rom_art_import.h"
#include "app/actor_pose_editor.h"
#include "engine/army_artwork.h"
#include "engine/avatar_artwork.h"
#include "engine/icon_artwork.h"
#include "engine/project.h"
#include "engine/scene_images.h"
#include "engine/sprite_object.h"
#include "engine/text.h"
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <memory>
namespace lordly {
namespace {
QImage image(const RasterImage &r) {
    return QImage(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width, r.height,
                  r.width * 4, QImage::Format_RGBA8888)
        .copy();
}
SpriteAsset resourceAsset(const Rom &rom, quint32 key) {
    if (!key || key % 2 || key >= 0x10000000)
        throw Error("Enter an aligned ROM resource key");
    const quint64 at = 0x594280ULL + key;
    const quint32 size = u32(rom.bytes(), at);
    if (size < 4 || size > 16 * 1024 * 1024 || at + 4 + size > quint64(rom.bytes().size()))
        throw Error("Resource is outside the ROM envelope");
    auto bytes = rom.bytes().mid(at + 4, size);
    if (u16(bytes, 0) != 0x5554) {
        const auto decoded = u32(bytes, 0);
        if (!decoded || decoded > 16 * 1024 * 1024)
            throw Error("Unsupported sprite resource");
        qsizetype used = 0;
        bytes = decodeLzss(bytes.mid(4), decoded, &used);
        if (used != size - 4)
            throw Error("Sprite resource size mismatch");
    }
    const auto object = SpriteObject::decode(bytes);
    if (object.children().isEmpty() || object.children().size() > 1024 ||
        quint64(object.width()) * object.height() * object.children().size() >
            SpriteLibrary::pixelLimit)
        throw Error("Sprite resource exceeds the Art Library frame or pixel budget");
    SpriteAsset asset;
    for (int i = 0; i < object.children().size(); ++i) {
        auto part = SpriteAsset::fromImage(QString("Sprite %1").arg(key, 0, 16),
                                           image(object.frame(i).image));
        if (i == 0)
            asset = part;
        else {
            auto frame = part.frames.first();
            frame.name = QString("Image %1").arg(i + 1);
            asset.frames << frame;
        }
    }
    asset.kind = asset.frames.size() > 1 ? "sequence" : "frame";
    asset.provenance = {{"source", "ROM sprite resource"}, {"key", double(key)}};
    asset.validate();
    return asset;
}
} // namespace
RomArtImport::RomArtImport(const Rom &rom, std::function<void(SpriteAsset)> sink, QWidget *parent)
    : QDialog(parent) {
    setObjectName("romArtImport");
    setWindowTitle("Import from ROM");
    resize(1080, 760);
    auto layout = new QVBoxLayout(this);
    auto top = new QHBoxLayout;
    auto label = new QLabel("Source: " + rom.sourcePath());
    label->setWordWrap(true);
    top->addWidget(label, 1);
    auto choose = new QPushButton("Choose another ROM…");
    top->addWidget(choose);
    layout->addLayout(top);
    auto note = new QLabel("Import an editable copy into Art Library. Original game artwork stays "
                           "assigned until you replace it in Art and Animation.");
    note->setWordWrap(true);
    layout->addWidget(note);
    auto tabs = new QTabWidget;
    layout->addWidget(tabs, 1);
    auto actors = new ActorPoseEditor(rom);
    actors->setSpriteLibrary([this, sink](SpriteAsset asset) {
        sink(std::move(asset));
        accept();
    });
    tabs->addTab(actors, "Sprites and animations");
    auto images = new QWidget;
    auto il = new QVBoxLayout(images);
    auto filters = new QHBoxLayout;
    auto category = new QComboBox;
    category->setObjectName("romArtCategory");
    category->addItems({"Equipment icons", "Consumables", "Portraits", "Army and formation sprites",
                        "Scene images"});
    filters->addWidget(category);
    auto search = new QLineEdit;
    search->setObjectName("romArtSearch");
    search->setPlaceholderText("Find artwork…");
    search->setClearButtonEnabled(true);
    filters->addWidget(search, 1);
    il->addLayout(filters);
    auto split = new QSplitter;
    auto listing = new QListWidget;
    listing->setObjectName("romArtList");
    split->addWidget(listing);
    auto preview = new QLabel;
    preview->setObjectName("romArtPreview");
    preview->setAlignment(Qt::AlignCenter);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(preview);
    split->addWidget(scroll);
    split->setStretchFactor(1, 1);
    il->addWidget(split, 1);
    auto status = new QLabel;
    status->setWordWrap(true);
    il->addWidget(status);
    auto import = new QPushButton("Import selected artwork");
    import->setObjectName("romArtImportSelected");
    il->addWidget(import);
    tabs->addTab(images, "Icons, portraits and images");
    auto selected = std::make_shared<std::optional<SpriteAsset>>();
    auto refresh = [=] {
        listing->clear();
        selected->reset();
        import->setEnabled(false);
        try {
            auto add = [&](QString label, QVariant value) {
                auto item = new QListWidgetItem(label, listing);
                item->setData(Qt::UserRole, value);
            };
            const auto classes = ClassData::load(rom);
            if (category->currentIndex() < 2) {
                const auto table = category->currentIndex() ? Table::Consumables : Table::Items;
                const auto records = Project::fromRom(rom, QJsonObject()).records(table);
                for (const auto &r : records)
                    if (r.id)
                        add(QString("%1 · %2").arg(r.id).arg(r.name),
                            r.id | (category->currentIndex() ? 512 : 0));
            } else if (category->currentIndex() == 2) {
                for (int i = 1; i < classes.entries().size(); ++i)
                    for (int s = 0; s < 2; ++s)
                        add(QString("%1 · %2 · portrait %3")
                                .arg(i)
                                .arg(decodeText(classes.entries()[i].name))
                                .arg(s + 1),
                            i * 2 + s);
            } else if (category->currentIndex() == 3) {
                auto army = ArmyArtwork::load(rom);
                for (int group = 0; group < 6; ++group)
                    for (int model = 0; model < army.sourceCount(group); ++model)
                        for (int palette = 0; palette < 2; ++palette)
                            if (army.present(group, model))
                                add(QString("%1 · model %2 · palette %3")
                                        .arg(ArmyArtwork::specs()[group].label)
                                        .arg(model)
                                        .arg(palette),
                                    group * 128 + model * 2 + palette);
            } else
                for (const auto &s : SceneImages::catalog())
                    add(s.name.isEmpty() ? s.id : s.name, s.id);
            for (int i = 0; i < listing->count(); ++i)
                listing->item(i)->setHidden(
                    !listing->item(i)->text().contains(search->text(), Qt::CaseInsensitive));
            status->setText("Choose artwork to preview it.");
        } catch (const std::exception &e) {
            status->setText(QString::fromUtf8(e.what()));
        }
    };
    connect(category, &QComboBox::currentIndexChanged, this, refresh);
    connect(search, &QLineEdit::textChanged, this, [=] {
        for (int i = 0; i < listing->count(); ++i)
            listing->item(i)->setHidden(
                !listing->item(i)->text().contains(search->text(), Qt::CaseInsensitive));
    });
    connect(listing, &QListWidget::currentRowChanged, this, [=] {
        selected->reset();
        import->setEnabled(false);
        preview->clear();
        auto item = listing->currentItem();
        if (!item)
            return;
        try {
            const int id = item->data(Qt::UserRole).toInt();
            SpriteAsset asset;
            if (category->currentIndex() < 2)
                asset =
                    SpriteAsset::fromImage(item->text(), IconArtwork::load(rom).image(quint16(id)));
            else if (category->currentIndex() == 2)
                asset = SpriteAsset::fromImage(
                    item->text(),
                    AvatarArtwork::load(rom).image(ClassData::load(rom), id / 2, id % 2));
            else if (category->currentIndex() == 3)
                asset = SpriteAsset::fromImage(
                    item->text(), ArmyArtwork::load(rom).image(id / 128, (id % 128) / 2, id % 2));
            else {
                const auto decoded = SceneImages::decode(rom, item->data(Qt::UserRole).toString());
                asset = SpriteAsset::fromImage(item->text(), image(decoded.composite));
            }
            asset.provenance = {{"source", "ROM artwork"},
                                {"category", category->currentText()},
                                {"entry", item->data(Qt::UserRole).toString()}};
            preview->setPixmap(QPixmap::fromImage(asset.composite(0))
                                   .scaled(asset.size.scaled(480, 420, Qt::KeepAspectRatio),
                                           Qt::KeepAspectRatio, Qt::FastTransformation));
            status->setText(
                QString("%1 × %2 pixels").arg(asset.size.width()).arg(asset.size.height()));
            *selected = std::move(asset);
            import->setEnabled(true);
        } catch (const std::exception &e) {
            status->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(import, &QPushButton::clicked, this, [=, this] {
        if (*selected) {
            try {
                sink(**selected);
                accept();
            } catch (const std::exception &e) {
                status->setText(QString::fromUtf8(e.what()));
            }
        }
    });
    auto raw = new QWidget;
    auto rl = new QVBoxLayout(raw);
    auto info =
        new QLabel("Load a native sprite by its ROM resource key (hex). This also exposes sprite "
                   "resources outside the named catalogs. All child images are copied as frames; "
                   "external palette-dependent resources require their actor or pose selector.");
    info->setWordWrap(true);
    rl->addWidget(info);
    auto key = new QLineEdit;
    key->setObjectName("romArtResourceKey");
    key->setPlaceholderText("ROM resource key, e.g. 0123456");
    rl->addWidget(key);
    auto rawPreview = new QLabel;
    rawPreview->setAlignment(Qt::AlignCenter);
    rl->addWidget(rawPreview, 1);
    auto rawStatus = new QLabel;
    rawStatus->setWordWrap(true);
    rl->addWidget(rawStatus);
    auto load = new QPushButton("Preview resource");
    rl->addWidget(load);
    auto rawImport = new QPushButton("Import sprite frames");
    rawImport->setEnabled(false);
    rl->addWidget(rawImport);
    tabs->addTab(raw, "Resource key");
    auto rawAsset = std::make_shared<std::optional<SpriteAsset>>();
    connect(key, &QLineEdit::textChanged, this, [=] {
        rawAsset->reset();
        rawImport->setEnabled(false);
        rawPreview->clear();
    });
    connect(load, &QPushButton::clicked, this, [=] {
        try {
            bool ok = false;
            auto value = key->text().toUInt(&ok, 16);
            if (!ok)
                throw Error("Enter a hexadecimal resource key");
            *rawAsset = resourceAsset(rom, value);
            rawPreview->setPixmap(QPixmap::fromImage((*rawAsset)->composite(0))
                                      .scaled(480, 420, Qt::KeepAspectRatio));
            rawStatus->setText(QString("%1 frames").arg((*rawAsset)->frames.size()));
            rawImport->setEnabled(true);
        } catch (const std::exception &e) {
            rawAsset->reset();
            rawImport->setEnabled(false);
            rawStatus->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(rawImport, &QPushButton::clicked, this, [=, this] {
        try {
            if (*rawAsset) {
                sink(**rawAsset);
                accept();
            }
        } catch (const std::exception &e) {
            rawStatus->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(choose, &QPushButton::clicked, this, [this, sink] {
        const auto path = QFileDialog::getOpenFileName(this, "Choose artwork ROM", {},
                                                       "N64 ROM (*.v64 *.z64 *.n64)");
        if (path.isEmpty())
            return;
        try {
            RomArtImport dialog(Rom::open(path), sink, this);
            if (dialog.exec() == QDialog::Accepted)
                accept();
        } catch (const std::exception &e) {
            QMessageBox::warning(this, "Import from ROM", QString::fromUtf8(e.what()));
        }
    });
    auto close = new QPushButton("Cancel");
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(close);
    refresh();
}
} // namespace lordly
