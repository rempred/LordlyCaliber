#include "app/audio_cue_view.h"
#include "app/sound_output.h"
#include <QFutureWatcher>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QSlider>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QtEndian>
#include <algorithm>
namespace lordly {
namespace {
QByteArray soundKey(const AudioCue &cue) {
    if(!cue.source.isEmpty())return QByteArray::number(cue.variant)+QCryptographicHash::hash(QJsonDocument(cue.source).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);
    return cue.pcm.isEmpty() ? QByteArray::number(cue.request)
        : QByteArray("PCM") + QCryptographicHash::hash(cue.pcm, QCryptographicHash::Sha256);
}
QString groupKey(const AudioCue &cue) {
    return cue.pcm.isEmpty() && cue.cue >= 0 ? QString("cue/%1").arg(cue.cue)
                                                          : QString::fromLatin1(soundKey(cue).toHex());
}
}
AudioCueView::AudioCueView(std::function<void(int)> seek, QWidget *parent) : QWidget(parent) {
    setObjectName("audioCueView");
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    summary_ = new QLabel; summary_->setObjectName("audioCueSummary"); summary_->setWordWrap(true);
    layout->addWidget(summary_);
    table_ = new QTreeWidget; table_->setObjectName("audioCueTable");
    table_->setColumnCount(3); table_->setHeaderLabels({"Time in effect", "Sound / part", "Routing"});
    table_->setColumnHidden(2, true);
    table_->setTreePosition(1);
    table_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setMinimumHeight(120); table_->setMaximumHeight(260);
    layout->addWidget(table_, 1);
    position_ = new QLabel; position_->setObjectName("audioCuePosition");
    position_->setMinimumHeight(position_->fontMetrics().height()); layout->addWidget(position_);
    auto controls = new QHBoxLayout;
    play_ = new QPushButton("Play selected"); play_->setObjectName("audioPlay"); play_->setAutoDefault(false);
    stop_ = new QPushButton("Stop"); stop_->setObjectName("audioStop"); stop_->setAutoDefault(false);
    volume_ = new QSlider(Qt::Horizontal); volume_->setObjectName("audioVolume");
    volume_->setRange(0, 100); volume_->setValue(35); volume_->setMaximumWidth(130);
    volume_->setToolTip("Preview volume; does not change the ROM or system volume.");
    controls->addWidget(play_); controls->addWidget(stop_);
    controls->addWidget(new QLabel("Preview volume")); controls->addWidget(volume_); controls->addStretch();
    layout->addLayout(controls);
    playback_ = new QLabel; playback_->setObjectName("audioPlaybackStatus"); playback_->setWordWrap(true);
    layout->addWidget(playback_);
    auto note = new QLabel("Preview omits N64 reverb."); note->setWordWrap(true); layout->addWidget(note);
    output_ = new SoundOutput(this); output_->setObjectName("audioOutput");
    output_->finished = [this] { stop_->setEnabled(false); playback_->setText("Finished"); };
    connect(play_, &QPushButton::clicked, this, &AudioCueView::playSound);
    connect(stop_, &QPushButton::clicked, this, [this] { stopSound(); playback_->setText("Stopped"); });
    connect(table_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (!item) return;
        selectedCue_ = item->data(0, Qt::UserRole).toInt();
        selectedPart_ = item->data(0, Qt::UserRole + 1).toInt();
        stopSound(); playback_->clear(); updateSelection();
    });
    connect(table_, &QTreeWidget::itemClicked, this, [seek](QTreeWidgetItem *item) {
        if (seek) seek(item->data(0, Qt::UserRole + 2).toInt());
    });
    connect(table_, &QTreeWidget::itemDoubleClicked, this, [this] { playSound(); });
    setMessage("Preparing sound cues…");
}
void AudioCueView::setCompact(bool compact) {
    summary_->setVisible(!compact); table_->setVisible(!compact); position_->setVisible(!compact);
}
int AudioCueView::previewVolume() const { return volume_->value(); }
void AudioCueView::stopAudition() { stopSound(); playback_->clear(); }
AudioCueView::~AudioCueView() { stopSound(); clearParts(); }
void AudioCueView::setRom(const Rom &rom) { stopSound(); clearParts(); cache_.clear(); rom_ = rom; loadParts(); }
void AudioCueView::stopSound() {
    output_->stop(); stop_->setEnabled(false);
}
std::shared_ptr<const SoundPreview> AudioCueView::sound(int cue) const {
    return cue >= 0 && cue < decoded_.size() ? decoded_[cue] : nullptr;
}
void AudioCueView::setSelectedSound(int cue, int part) {
    selectedCue_ = cue; selectedPart_ = part;
    stopSound(); playback_->clear();
    if (auto root = table_->topLevelItem(cue)) {
        QSignalBlocker block(table_);
        table_->setCurrentItem(part >= 0 ? root->child(part) : root);
    }
    updateSelection();
}
void AudioCueView::playSelectedSound() { playSound(); }
void AudioCueView::updateSelection() {
    const auto decoded = sound(selectedCue_);
    play_->setEnabled(decoded && (selectedPart_ < 0 || selectedPart_ < decoded->parts.size()));
}
void AudioCueView::clearParts() {
    ++partsGeneration_; if (partsCancel_) partsCancel_->store(true);
    decoded_.clear();
}
void AudioCueView::rebuildRows() {
    QSignalBlocker block(table_); table_->clear();
    QMap<QString, int> ordinals, counts, occurrence;
    for (const auto &cue : cues_) ++counts[groupKey(cue)];
    for (int i = 0; i < cues_.size(); ++i) {
        const auto &cue = cues_[i]; const auto key = groupKey(cue);
        if (!ordinals.contains(key)) ordinals[key] = ordinals.size() + 1;
        auto title = cue.label.isEmpty() ? (effect_ ? effectSoundName(cue, usage_, ordinals[key])
                         : QString("%1 · sound %2").arg(usage_).arg(ordinals[key])) : cue.label;
        if (counts[key] > 1) title += QString(" · repeat %1/%2").arg(++occurrence[key]).arg(counts[key]);
        const auto decoded = sound(i);
        title += decoded ? QString(" — full sound (%1 parts)").arg(decoded->parts.size()) : " — loading parts…";
        auto root = new QTreeWidgetItem(table_, {QString("%1 s").arg(cue.tick / 30., 0, 'f', 2), title,
                                                  QString("Request %1").arg(cue.request)});
        root->setData(0, Qt::UserRole, i); root->setData(0, Qt::UserRole + 1, -1);
        root->setData(0, Qt::UserRole + 2, cue.tick);
        root->setData(2, Qt::UserRole, cue.request);
        root->setToolTip(0, QString("Effect tick %1 (30 ticks per second)").arg(cue.tick));
        root->setToolTip(1, "Full sound requested at this moment. Repeats reuse the same sound; parts may overlap.");
        if (decoded) for (int j = 0; j < decoded->parts.size(); ++j) {
            const auto &part = decoded->parts[j];
            auto child = new QTreeWidgetItem(root, {
                QString("%1 s").arg(cue.tick / 30. + part.tick / 60., 0, 'f', 2),
                QString("Part %1 · +%2 s · %3 s long").arg(j + 1).arg(part.tick / 60., 0, 'f', 2)
                    .arg(part.pcm.size() / 128000., 0, 'f', 2)});
            child->setData(0, Qt::UserRole, i); child->setData(0, Qt::UserRole + 1, j);
            child->setData(0, Qt::UserRole + 2, cue.tick + (part.tick + 1) / 2);
            child->setToolTip(1, "Play selected auditions only this part, starting immediately. Time shows its onset in the full effect.");
        }
        root->setExpanded(true);
    }
    if (auto root = table_->topLevelItem(selectedCue_))
        table_->setCurrentItem(selectedPart_ >= 0 ? root->child(selectedPart_) : root);
    summary_->setText(cues_.isEmpty() ? "No sound cues in this preview." :
        QString("%1 sound triggers. Repeats use the same sound; indented parts can overlap. Select a row to listen.")
            .arg(cues_.size()) + (boundary_.isEmpty() ? "" : ' ' + boundary_));
    updateSelection();
}
void AudioCueView::loadParts() {
    clearParts(); decoded_.resize(cues_.size());
    if(cache_.size()>32)cache_.clear();
    if (!rom_) { rebuildRows(); return; }
    for (int i = 0; i < cues_.size(); ++i) decoded_[i] = cache_.value(soundKey(cues_[i]));
    rebuildRows();
    if (std::all_of(decoded_.cbegin(), decoded_.cend(), [](const auto &s) { return bool(s); })) {
        if (soundsReady) soundsReady(); return;
    }
    partsCancel_ = std::make_shared<std::atomic_bool>(false);
    const int generation = partsGeneration_;
    struct Result { QHash<QByteArray, std::shared_ptr<SoundPreview>> sounds; QString error; };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
        const auto result = watcher->result(); watcher->deleteLater();
        if (generation != partsGeneration_) return;
        cache_ = result.sounds;
        for (int i = 0; i < cues_.size(); ++i) decoded_[i] = cache_.value(soundKey(cues_[i]));
        rebuildRows(); playback_->setText(result.error);
        if (soundsReady) soundsReady();
    });
    watcher->setFuture(QtConcurrent::run([rom = *rom_, cues = cues_, cached = cache_, cancel = partsCancel_] {
        Result result; result.sounds = cached;
        for (const auto &cue : cues) {
            if (cancel->load()) return Result{};
            const auto key = soundKey(cue); if (result.sounds.contains(key)) continue;
            try {
                if(!cue.source.isEmpty()) {
                    result.sounds.insert(key,std::make_shared<SoundPreview>(renderAuthoredSound(rom,AttackSound::fromJson(cue.source),cue.variant,cancel,true)));
                } else if (cue.pcm.isEmpty()) {
                    if (cue.request < 0) continue;
                    result.sounds.insert(key, std::make_shared<SoundPreview>(renderSoundPreview(rom, cue.request, cancel, true)));
                } else { AttackSound sound; sound.pcm = cue.pcm;
                    result.sounds.insert(key, std::make_shared<SoundPreview>(renderImportedSoundPreview(rom, sound, cancel, true))); }
            } catch (const std::exception &e) { result.error = QString::fromUtf8(e.what()); }
        }
        return result;
    }));
}
void AudioCueView::playSound() {
    const auto decoded = sound(selectedCue_); if (!decoded) return;
    if (auditionStarted) auditionStarted(); stopSound();
    try {
        auto pcm = selectedPart_ >= 0 && selectedPart_ < decoded->parts.size()
                     ? decoded->parts[selectedPart_].pcm : decoded->pcm;
        for (qsizetype i = 0; i < pcm.size(); i += 2)
            qToLittleEndian<qint16>(qint16(qFromLittleEndian<qint16>(pcm.constData() + i) * volume_->value() / 100), pcm.data() + i);
        const double duration = pcm.size() / 128000.;
        output_->play(std::move(pcm)); stop_->setEnabled(true);
        playback_->setText((selectedPart_ >= 0 ? QString("Playing part %1 · ").arg(selectedPart_ + 1)
                                              : QString("Playing full sound · ")) + QString("%1 s").arg(duration, 0, 'f', 2));
    } catch (const std::exception &e) { playback_->setText(QString::fromUtf8(e.what())); }
}
void AudioCueView::setMessage(const QString &text) {
    stopSound(); clearParts(); cues_.clear(); playback_->clear(); table_->clear();
    summary_->setText(text); position_->clear(); updateSelection();
}
void AudioCueView::setTick(int tick) {
    position_->setText(QString("Preview tick %1 · %2 s").arg(tick).arg(tick / 30., 0, 'f', 2));
}
void AudioCueView::setCues(const QVector<AudioCue> &cues, const QString &usage, bool effect, const QString &boundary) {
    stopSound(); playback_->clear(); cues_ = cues; usage_ = usage; effect_ = effect; boundary_ = boundary;
    selectedCue_ = 0; selectedPart_ = -1; setTick(0); loadParts();
}
} // namespace lordly
