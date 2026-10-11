#include "app/dialogue_preview.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
void DialoguePreview::setPortraitProvider(DialogueDrawing::PortraitProvider provider) {
    portraits_=std::move(provider);
    if(drawing_)drawing_->setPortraitProvider(portraits_);
}
DialoguePreview::DialoguePreview(const Rom &rom, QWidget *parent) : QWidget(parent), rom_(rom) {
    auto layout = new QVBoxLayout(this);
    auto note = new QLabel("Standalone dialogue window. The fields below supply preview names; "
                           "scene actor placement and sound playback are separate.");
    note->setWordWrap(true);
    layout->addWidget(note);
    auto controls = new QHBoxLayout;
    play_ = new QPushButton("Play");
    play_->setCheckable(true);
    play_->setObjectName("dialoguePlay");
    controls->addWidget(play_);
    auto button = [&](const QString &text, const char *name, QHBoxLayout *row, auto action) {
        auto control = new QPushButton(text);
        control->setObjectName(name);
        row->addWidget(control);
        connect(control, &QPushButton::clicked, this, action);
    };
    button("Step", "dialogueStep", controls, [this] { advance(); });
    button("Restart", "dialogueRestart", controls, [this] { restart(); });
    rate_ = new QSpinBox;
    rate_->setRange(1, 120);
    rate_->setValue(30);
    rate_->setSuffix(" passes/s");
    rate_->setAccessibleName("Preview resource passes per second");
    controls->addWidget(rate_);
    controls->addStretch();
    layout->addLayout(controls);
    auto input = new QHBoxLayout;
    button("Confirm (A)", "dialogueConfirm", input, [this] { advance(0x8000); });
    button("Cancel (B)", "dialogueCancel", input, [this] { advance(0x4000); });
    button("Up", "dialogueUp", input, [this] { advance(0, 0x800); });
    button("Down", "dialogueDown", input, [this] { advance(0, 0x400); });
    history_ = new QPushButton("History");
    history_->setCheckable(true);
    history_->setObjectName("dialogueHistory");
    input->addWidget(history_);
    connect(history_, &QPushButton::toggled, this, [this] { advance(); });
    button("Resume script", "dialogueResume", input, [this] {
        if (session_) {
            session_->resume();
            advance();
        }
    });
    input->addStretch();
    layout->addLayout(input);
    canvas_ = new QLabel;
    canvas_->setObjectName("dialogueCanvas");
    canvas_->setAlignment(Qt::AlignCenter);
    canvas_->setMinimumSize(320, 240);
    canvas_->setStyleSheet("background: #182027; border: 1px solid #44505a;");
    canvas_->setAccessibleName("Native dialogue preview");
    layout->addWidget(canvas_, 1);
    status_ = new QLabel;
    status_->setObjectName("dialoguePlaybackStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    auto names = new QHBoxLayout;
    const QString labels[]{"Speaker", "Owner", "Fixed name", "Group"};
    for (int i = 0; i < 4; ++i) {
        auto field = new QVBoxLayout;
        field->addWidget(new QLabel(labels[i]));
        names_[i] = new QLineEdit;
        names_[i]->setMaxLength(48);
        names_[i]->setAccessibleName(labels[i]);
        names_[i]->setObjectName(QString("dialogueName%1").arg(i));
        field->addWidget(names_[i]);
        names->addLayout(field);
        connect(names_[i], &QLineEdit::editingFinished, this, [this] { restart(); });
    }
    layout->addLayout(names);
    auto suffix = new QHBoxLayout;
    suffix->addWidget(new QLabel("Long-name suffix"));
    suffix_ = new QLineEdit;
    suffix_->setMaxLength(32);
    suffix_->setAccessibleName("Long-name suffix");
    suffix->addWidget(suffix_);
    layout->addLayout(suffix);
    connect(suffix_, &QLineEdit::editingFinished, this, [this] { restart(); });
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, [this] { advance(); });
    connect(play_, &QPushButton::toggled, this, [this](bool playing) {
        play_->setText(playing ? "Pause" : "Play");
        if (playing && session_)
            timer_->start(std::max(1, 1000 / rate_->value()));
        else
            timer_->stop();
    });
    connect(rate_, &QSpinBox::valueChanged, this, [this] {
        if (timer_->isActive())
            timer_->setInterval(std::max(1, 1000 / rate_->value()));
    });
    clear();
}
DialoguePreview::~DialoguePreview() {
    timer_->stop();
    for (auto field : names_)
        disconnect(field, nullptr, this, nullptr);
    disconnect(suffix_, nullptr, this, nullptr);
}
void DialoguePreview::clear() {
    timer_->stop();
    play_->setChecked(false);
    session_.reset();
    play_->setEnabled(false);
    {
        const QSignalBlocker block(history_);
        history_->setChecked(false);
    }
    archive_ = -1;
    dialogues_.reset();
    canvas_->clear();
    status_->setText("Select a dialogue entry.");
    setProperty("updateCount", 0);
    setProperty("dialogueState", -1);
}
void DialoguePreview::selectEntry(int archive, quint16 entry, std::optional<Dialogues> dialogues) {
    dialogues_ = std::move(dialogues);
    archive_ = archive;
    entry_ = entry;
    restart();
}
void DialoguePreview::fail(const std::exception &error) {
    timer_->stop();
    play_->setChecked(false);
    canvas_->clear();
    play_->setEnabled(false);
    status_->setText(QString("Preview unavailable: %1").arg(QString::fromUtf8(error.what())));
}
void DialoguePreview::restart() {
    timer_->stop();
    play_->setChecked(false);
    session_.reset();
    play_->setEnabled(false);
    {
        const QSignalBlocker block(history_);
        history_->setChecked(false);
    }
    canvas_->clear();
    setProperty("updateCount", 0);
    setProperty("dialogueState", -1);
    if (archive_ < 0)
        return;
    try {
        auto environment = DialoguePlayback::environment(rom_);
        auto text = [](const QString &value) {
            const auto bytes = value.toLatin1();
            if (QString::fromLatin1(bytes) != value)
                throw Error("Preview names require supported native single-byte characters");
            return bytes + char(0);
        };
        for (int i = 0; i < 4; ++i)
            environment.substitutions[48 + i] = text(names_[i]->text());
        environment.longNameSuffix = text(suffix_->text());
        session_.emplace(rom_, archive_, entry_, std::move(environment), dialogues_);
        play_->setEnabled(true);
        if (!drawing_) {
            drawing_.emplace(rom_);
            drawing_->setPortraitProvider(portraits_);
        }
        advance();
    } catch (const std::exception &error) {
        fail(error);
    }
}
void DialoguePreview::advance(quint16 pressed, quint16 repeat) {
    if (!session_)
        return;
    try {
        session_->advance({512, pressed, repeat, quint16(history_->isChecked() ? 16 : 0)});
        refresh();
        if (!session_->activeWindows()) {
            timer_->stop();
            play_->setChecked(false);
        }
    } catch (const std::exception &error) {
        fail(error);
    }
}
void DialoguePreview::refresh() {
    const auto image = session_->image(*drawing_);
    const QImage pixels(reinterpret_cast<const uchar *>(image.rgba.constData()), 320, 240,
                        QImage::Format_RGBA8888);
    const auto fit = QSize(320, 240).scaled(canvas_->size(), Qt::KeepAspectRatio);
    canvas_->setPixmap(
        QPixmap::fromImage(pixels).scaled(fit, Qt::KeepAspectRatio, Qt::FastTransformation));
    const int state = session_->state();
    QString phase;
    switch (state) {
    case -1:
        phase = "Closed";
        break;
    case 0:
    case 1:
        phase = "Opening";
        break;
    case 2:
        phase = "Revealing text";
        break;
    case 3:
        phase = "Waiting for confirm";
        break;
    case 4:
        phase = "Waiting for script resume";
        break;
    case 5:
        phase = "Window effect";
        break;
    case 6:
        phase = "Continuation prompt";
        break;
    case 7:
        phase = "Choose an option";
        break;
    case 8:
    case 11:
        phase = "Timed pause";
        break;
    case 9:
    case 10:
        phase = "Changing portrait";
        break;
    default:
        phase = "Closing";
        break;
    }
    status_->setText(QString("%1 · %2 updates · %3 active windows")
                         .arg(phase)
                         .arg(session_->updates())
                         .arg(session_->activeWindows()));
    setProperty("updateCount", qulonglong(session_->updates()));
    setProperty("dialogueState", state);
}
} // namespace lordly
