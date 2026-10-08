#include "app/animation_sequence_editor.h"
#include "app/sprite_editor.h"
#include "engine/combat_artwork.h"
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
namespace lordly {
void AnimationSequenceEditor::editSprite() {
    const int id = selectedClass(), flags = routes_->currentIndex(),
              state = poses_->currentData().toInt(), row = records_->currentRow();
    const auto original = project_->animationSequences()->find(id, flags);
    if (!original) throw Error("Enable frame and sprite editing first");
    auto snapshot = *original;
    auto bank = snapshot.poses();
    const auto records = bank.effective(state).records;
    if (row < 0 || row >= records.size() || (records[row].opcode != 1 && records[row].opcode != 21))
        throw Error("Choose a frame instruction to paint");
    const auto record = records[row];
    const int token = u8(record.operands, 0) + (record.opcode == 21 ? 256 * u8(record.operands, 1) : 0);
    auto frame = snapshot.frame(token);
    if (!u16(frame, 0)) throw Error("This frame has no sprite layers");
    const auto before = project_->animationSequences()->json();
    QDialog dialog(this); dialog.setWindowTitle("Paint animation sprite");
    dialog.setObjectName("sequenceSpriteDialog"); dialog.resize(840, 680);
    auto layout = new QVBoxLayout(&dialog);
    auto note = new QLabel("Paint a sprite layer in this frame. Other frames, classes and child variants retain their pixels. "
                           "Colors are mapped to the sprite's native palette and opacity precision.");
    note->setWordWrap(true); layout->addWidget(note);
    auto choices = new QHBoxLayout; layout->addLayout(choices);
    auto layer = new QComboBox; layer->setObjectName("sequenceSpriteLayer");
    for (int i = 0; i < u16(frame, 0); ++i)
        layer->addItem(QString("Layer %1 · sprite %2").arg(i + 1).arg(u16(frame, 2 + i * 16)), i);
    choices->addWidget(layer, 1);
    auto child = new QSpinBox; child->setObjectName("sequenceSpriteChild"); child->setPrefix("Variant "); choices->addWidget(child);
    auto tool = new QComboBox; tool->addItems({"Pencil", "Eraser", "Fill", "Pick color"}); choices->addWidget(tool);
    auto colorButton = new QPushButton("Ink…"); colorButton->setObjectName("sequenceSpriteInk"); choices->addWidget(colorButton);
    auto zoom = new QSpinBox; zoom->setRange(1, 24); zoom->setValue(8); zoom->setPrefix("Zoom "); choices->addWidget(zoom);
    auto canvas = new SpriteCanvas; canvas->setObjectName("sequenceSpriteCanvas"); canvas->setZoom(8);
    auto scroll = new QScrollArea; scroll->setWidget(canvas); scroll->setAlignment(Qt::AlignCenter); layout->addWidget(scroll, 1);
    auto status = new QLabel; status->setWordWrap(true); layout->addWidget(status);
    auto commands = new QHBoxLayout; layout->addLayout(commands);
    auto undo = new QPushButton("Undo stroke"), redo = new QPushButton("Redo stroke");
    commands->addWidget(undo); commands->addWidget(redo); commands->addStretch();
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->setObjectName("sequenceSpriteButtons"); buttons->button(QDialogButtonBox::Ok)->setText("Apply to this frame");
    commands->addWidget(buttons);
    QMap<int, QByteArray> drafts;
    QVector<QMap<int, QByteArray>> history{drafts}; int cursor = 0;
    const auto source = [&] {
        const int art = u16(frame, 2 + layer->currentIndex() * 16);
        return SpriteObject::decode(drafts.value(art, snapshot.sprite(art)));
    };
    const auto palette = [&] {
        const int art = u16(frame, 2 + layer->currentIndex() * 16);
        const auto &config = snapshot.controls()[2];
        return snapshot.controls()[3].mid(u8(config, u32(config, 0x1c) + art) * 512, 512);
    };
    const auto refresh = [&] {
        try {
            const auto sprite = source();
            { const QSignalBlocker block(child); child->setRange(0, int(sprite.children().size()) - 1); }
            const auto pixels = sprite.frame(child->value(), palette()).image;
            const auto image = QImage(reinterpret_cast<const uchar *>(pixels.rgba.constData()), pixels.width,
                                      pixels.height, pixels.width * 4, QImage::Format_RGBA8888).copy();
            auto asset = SpriteAsset::fromImage("Animation frame sprite", image);
            asset.id = QString("private-%1-%2").arg(layer->currentIndex()).arg(child->value());
            canvas->setAsset(&asset, 0, 0); canvas->setEnabled(true);
            status->setText(QString("%1 × %2 · %3 variants").arg(pixels.width).arg(pixels.height).arg(sprite.children().size()));
        } catch (const std::exception &e) {
            canvas->setAsset(nullptr, 0, 0); canvas->setEnabled(false); status->setText(QString::fromUtf8(e.what()));
        }
        undo->setEnabled(cursor > 0); redo->setEnabled(cursor + 1 < history.size());
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!drafts.isEmpty());
    };
    connect(layer, &QComboBox::currentIndexChanged, &dialog, refresh);
    connect(child, &QSpinBox::valueChanged, &dialog, refresh);
    connect(zoom, &QSpinBox::valueChanged, canvas, &SpriteCanvas::setZoom);
    connect(tool, &QComboBox::currentIndexChanged, canvas, [canvas](int i) { canvas->setTool(SpriteCanvas::Tool(i)); });
    QColor ink = Qt::black;
    const auto setInk = [&](QColor value) { ink = value; canvas->setColor(ink); colorButton->setStyleSheet("background: " + ink.name() + ";"); };
    connect(colorButton, &QPushButton::clicked, &dialog, [&] {
        const auto value = QColorDialog::getColor(ink, &dialog, "Sprite ink", QColorDialog::ShowAlphaChannel);
        if (value.isValid()) setInk(value);
    });
    connect(canvas, &SpriteCanvas::colorPicked, &dialog, setInk);
    connect(canvas, &SpriteCanvas::committed, &dialog, [&](const QImage &image, QPoint) {
        try {
            const int art = u16(frame, 2 + layer->currentIndex() * 16);
            const auto sprite = source();
            const auto pixels = CombatArtwork::mapSpriteImage(sprite, child->value(), palette(), image);
            const auto next = sprite.rebuild({{child->value(), pixels}});
            if (next == sprite.source()) return;
            if (next == snapshot.sprite(art)) drafts.remove(art); else drafts[art] = next;
            history.resize(cursor + 1); history << drafts; ++cursor; refresh();
        } catch (const std::exception &e) { refresh(); status->setText(QString::fromUtf8(e.what())); }
    });
    connect(undo, &QPushButton::clicked, &dialog, [&] { drafts = history[--cursor]; refresh(); });
    connect(redo, &QPushButton::clicked, &dialog, [&] { drafts = history[++cursor]; refresh(); });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    refresh();
    if (dialog.exec() != QDialog::Accepted || drafts.isEmpty()) return;
    if (project_->animationSequences()->json() != before)
        throw Error("Animation data changed while sprite painting was open");
    QMap<int, int> replacements;
    const auto config = snapshot.controls()[2];
    const auto palettes = snapshot.controls()[3];
    for (auto it = drafts.cbegin(); it != drafts.cend(); ++it)
        replacements[it.key()] = snapshot.appendSprite(it.value(), u8(config, 0x20 + it.key()),
            palettes.mid(u8(config, u32(config, 0x1c) + it.key()) * 512, 512));
    for (int i = 0; i < u16(frame, 0); ++i) {
        const int at = 2 + i * 16, art = u16(frame, at);
        if (replacements.contains(art)) put16(frame, at, quint16(replacements[art]));
    }
    bank.setFrame(state, row, snapshot.appendFrame(frame), u8(record.operands, record.operands.size() - 1));
    snapshot.setPoses(bank);
    auto next = *project_->animationSequences(); next.setDescriptor(id, flags, snapshot);
    apply(std::move(next), "Paint private animation frame sprite");
}
} // namespace lordly
