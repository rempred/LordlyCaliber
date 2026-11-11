#include "app/attack_audio_editor.h"
#include "app/audio_cue_view.h"
#include "engine/effect_player.h"
#include "engine/sound_parts.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>
namespace lordly {
namespace {
class SoundSelectionTree final : public QTreeWidget {
  public:
    SoundSelectionTree() {
        // A pointer passing over another sound must not look like a new target.
        setStyleSheet("QTreeView::item:hover:!selected { background: transparent; }");
    }
  protected:
    void mouseMoveEvent(QMouseEvent *event) override {
        // Sound/part selection is by click or keyboard, never by moving or
        // dragging across rows on the way to an editing action.
        event->accept();
    }
};
using Trace = QPair<QVector<AudioCue>, QString>;
Trace traceSounds(const Rom &rom, const Attack &attack) {
    Trace result;
    try {
        const int effect = attack.visual.nativeEffect >= 0 ? attack.visual.nativeEffect
                                                         : EffectPlayer::retailEffect(attack.animation);
        if (effect < 0 && !attack.visual.frames) return result;
        EffectPlayer player(rom);
        EffectPlayer::Scene scene;
        scene.targetCount = 1;
        scene.attack = attack.animation;
        player.startVisual(attack.visual, qMax(0, effect), scene);
        while (player.advance()) {}
        result.first = player.audioCues();
        QMap<int, int> ordinals;
        for (auto &cue : result.first) {
            cue.tick = qMax(0, cue.tick - attack.visual.delay);
            if (!ordinals.contains(cue.cue)) ordinals[cue.cue] = ordinals.size() + 1;
            cue.label = cue.source.isEmpty() ? effectSoundName(cue, AttackData::displayName(attack), ordinals[cue.cue])
                                              : cue.source["name"].toString();
            for (const auto &sound : attack.visual.audio.sounds)
                if (!cue.pcm.isEmpty() && sound.pcm == cue.pcm) { cue.label = sound.name; break; }
        }
    } catch (const std::exception &e) { result.second = QString::fromUtf8(e.what()); }
    return result;
}
AttackSound soundFromCue(const AudioCue &cue) {
    if(!cue.source.isEmpty())return AttackSound::fromJson(cue.source);
    AttackSound sound;
    sound.delay = qBound(0, cue.tick, 900);
    sound.cue = cue.cue;
    sound.request = qMax(0, cue.request);
    sound.pcm = cue.pcm;
    sound.name = cue.label;
    return sound;
}
}
AttackAudioEditor::AttackAudioEditor(Project *p, int attack, const AttackVisual &v, QWidget *parent)
    : QDialog(parent), project_(p), attack_(attack), visual_(v), audio_(v.audio) {
    setWindowTitle("Attack sounds - " + AttackData::displayName(p->attacks()->entry(attack)));
    setObjectName("attackAudioEditor");
    resize(900, 650);
    auto layout = new QVBoxLayout(this);
    mode_ = new QLabel;
    mode_->setObjectName("attackSoundMode");
    layout->addWidget(mode_);
    auto actions = new QHBoxLayout;
    layout->addLayout(actions);
    import_ = new QPushButton("Import WAV…"); import_->setObjectName("importAttackSound");
    copy_ = new QPushButton("Copy from sound…"); copy_->setObjectName("copyAttackSounds");
    replace_ = new QPushButton("Replace sound…"); replace_->setObjectName("replaceAttackSound");
    remove_ = new QPushButton("Remove"); remove_->setObjectName("removeAttackSound");
    restore_ = new QPushButton("Restore original"); restore_->setObjectName("restoreAttackSounds");
    for (auto button : {import_, copy_, replace_, remove_, restore_}) {
        button->setAutoDefault(false); actions->addWidget(button);
    }
    copy_->setToolTip("Copy a whole sound or one donor part into the selected sound or part.");
    replace_->setToolTip("Replace the selected sound, keeping its timing.");
    table_ = new SoundSelectionTree;
    table_->setColumnCount(3);
    table_->setObjectName("attackSoundRows");
    table_->setHeaderLabels({"Sound / part", "Start sound when", "Delay (seconds)"});
    table_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    table_->setColumnWidth(1, fontMetrics().horizontalAdvance("With original sound 1") + 48);
    table_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setMinimumHeight(140);
    layout->addWidget(table_, 1);
    auto format = new QLabel("WAV: 32,000 Hz, mono, signed 16-bit PCM; up to 2 seconds per clip "
                             "(256 KB total). Select a part to import into it. Replace keeps the selected sound or part's timing.");
    format->setWordWrap(true); format->setObjectName("attackAudioImportFormat");
    layout->addWidget(format);
    audition_ = new AudioCueView({}, this);
    audition_->setCompact(true); audition_->setRom(p->rom());
    layout->addWidget(audition_);
    status_ = new QLabel; status_->setWordWrap(true); layout->addWidget(status_);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { guard([this] { audio_.validate(); accept(); }); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(table_, &QTreeWidget::currentItemChanged, this, [this] {
        // A deliberate new selection supersedes any pending preview refresh.
        desiredRow_=desiredPart_=-1;
        select();
    });
    connect(copy_, &QPushButton::clicked, this, [this] { chooseAttack(selectedRow()>=0); });
    auto importFile = [this](bool replace) {
        const auto path = QFileDialog::getOpenFileName(this, "Import sound", {}, "PCM WAV (*.wav)");
        if (!path.isEmpty()) guard([&] { importWav(path, replace); });
    };
    connect(import_, &QPushButton::clicked, this, [this,importFile] { importFile(selectedPart()>=0); });
    auto replacements = new QMenu(replace_);
    replacements->addAction("From another attack…", this, [this] { chooseAttack(true); });
    replacements->addAction("Import WAV…", this, [importFile] { importFile(true); });
    auto restorePart=replacements->addAction("Restore original part",this,[this] {
        guard([&]{auto a=editableAudio();const int row=selectedRow(),part=selectedPart();
            if(row<0 || part<0)return;
            auto &edits=a.sounds[row].parts;
            for(int i=edits.size()-1;i>=0;--i)if(edits[i].toObject()["index"].toInt()==part)edits.removeAt(i);
            setAudio(a);});
    });restorePart->setObjectName("restoreSoundPart");
    replace_->setMenu(replacements);
    connect(remove_, &QPushButton::clicked, this, [this] {
        auto a = editableAudio(); const int row = selectedRow();
        if (row >= 0 && row < a.sounds.size()) guard([&] {
            if(selectedPart()>=0)muteSoundPart(project_->rom(),a.sounds[row],selectedPart());
            else a.sounds.removeAt(row);
            setAudio(a);
        });
    });
    connect(restore_, &QPushButton::clicked, this, [this] { setAudio({}); });
    connect(table_, &QTreeWidget::itemDoubleClicked, this, [this] { audition_->playSelectedSound(); });
    audition_->soundsReady = [this] { populateParts(); };
    refresh(); readOriginal();
}
void AttackAudioEditor::guard(const std::function<void()> &fn) {
    try { status_->clear(); fn(); }
    catch (const std::exception &e) { status_->setText(QString::fromUtf8(e.what())); }
}
AttackAudio AttackAudioEditor::editableAudio() const {
    return audio_.enabled ? audio_ : AttackAudio{true, originalSounds_};
}
void AttackAudioEditor::setAudio(const AttackAudio &a) {
    a.validate(); audio_ = a; refresh();
}
void AttackAudioEditor::importWav(const QString &path, bool replace) {
    if (!ready_) throw Error("Wait for the original sounds to load");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024)
        throw Error("Cannot read WAV (maximum file size 2 MB)");
    auto a = editableAudio();
    auto sound = AttackSound::importWav(file.readAll(), QFileInfo(path).completeBaseName());
    int row = selectedRow();const int part=selectedPart();
    if (replace) {
        if (row < 0 || row >= a.sounds.size()) throw Error("Select a sound to replace");
        if(selectedPart()>=0)replaceSoundPart(project_->rom(),a.sounds[row],selectedPart(),sound);
        else {sound.anchor = a.sounds[row].anchor; sound.delay = a.sounds[row].delay; a.sounds[row] = sound;}
    } else { row = a.sounds.size(); a.sounds.append(sound); }
    setAudio(a);
    if(!replace || part<0)table_->setCurrentItem(table_->topLevelItem(row));
}
void AttackAudioEditor::refresh() {
    QSignalBlocker block(table_);
    const int selected = selectedRow(); const auto sounds = editableAudio().sounds;
    desiredRow_=selected;desiredPart_=selectedPart();
    mode_->setText(!ready_ ? "Loading original sounds…" : sounds.isEmpty() && audio_.enabled ? "Effect sounds muted"
                      : QString("%1 · %2 sound groups").arg(audio_.enabled ? "Custom attack sounds" : "Original attack sounds")
                            .arg(sounds.size()));
    table_->clear();
    for (int i = 0; i < sounds.size(); ++i) {
        const auto &sound = sounds[i];
        auto root = new QTreeWidgetItem(table_, {sound.name.isEmpty() ? "Imported sound" : sound.name});
        root->setData(0, Qt::UserRole, i); root->setData(0, Qt::UserRole + 1, -1);
        auto when = new QComboBox; when->addItem("From effect start", -1);
        QSet<int> seen;
        for (const auto &cue : anchors_) if (cue.cue >= 0 && !seen.contains(cue.cue)) {
            seen.insert(cue.cue);
            const auto label = cue.label.section(" · ", -1);
            when->addItem(label.isEmpty() ? QString("Original sound %1").arg(seen.size())
                                         : "Original " + label, cue.cue);
        }
        if (sound.anchor >= 0 && when->findData(sound.anchor) < 0)
            when->addItem(QString("Original cue %1 (not in this effect)").arg(sound.anchor), sound.anchor);
        when->setCurrentIndex(when->findData(sound.anchor));
        when->setToolTip("Original timing follows every occurrence of this cue, including repeated impacts. From effect start uses a fixed time.");
        table_->setItemWidget(root, 1, when);
        auto delay = new QDoubleSpinBox;
        delay->setRange(0, 30); delay->setDecimals(2); delay->setSingleStep(1. / 30);
        delay->setValue(sound.delay / 30.); delay->setKeyboardTracking(false);
        delay->setToolTip(QString("%1 ticks. Timing is rounded to the game's 1/30-second ticks.").arg(sound.delay));
        table_->setItemWidget(root, 2, delay);
        connect(when, &QComboBox::activated, this, [this, i, when] {
            auto a = editableAudio(); a.sounds[i].anchor = when->currentData().toInt(); setAudio(a);
        });
        connect(delay, &QDoubleSpinBox::valueChanged, this, [this, i, delay](double seconds) {
            auto a = editableAudio(); a.sounds[i].delay = qRound(seconds * 30); audio_ = a;
            mode_->setText("Custom attack sounds"); restore_->setEnabled(true);
            delay->setToolTip(QString("%1 ticks (30 ticks = 1 second)").arg(a.sounds[i].delay));
        });
    }
    if (table_->topLevelItemCount() && desiredPart_<0)
        table_->setCurrentItem(table_->topLevelItem(qBound(0, selected, table_->topLevelItemCount() - 1)));
    QVector<AudioCue> cues;
    for (const auto &sound : sounds) {
        AudioCue cue{sound.delay, sound.cue, sound.request, 0, sound.cue >= 0};
        cue.pcm = sound.pcm; cue.label = sound.name;
        if(sound.hasParts())cue.source=sound.json();
        // The editor auditions the same player-side variant as the effect preview.
        if (sound.cue >= 0) cue.request = u16(project_->rom().bytes(), 0x2123a0 + sound.cue * 4 + 2);
        cues.append(cue);
    }
    audition_->setCues(cues, {}, true);
    import_->setEnabled(ready_); copy_->setEnabled(ready_); restore_->setEnabled(audio_.enabled); select();
}
int AttackAudioEditor::selectedRow() const {
    const auto item = table_->currentItem(); return item ? item->data(0, Qt::UserRole).toInt() : desiredRow_;
}
int AttackAudioEditor::selectedPart() const {
    const auto item = table_->currentItem(); return item ? item->data(0, Qt::UserRole + 1).toInt() : desiredPart_;
}
void AttackAudioEditor::populateParts() {
    const int row = desiredRow_>=0?desiredRow_:selectedRow(), part = desiredRow_>=0?desiredPart_:selectedPart();
    QSignalBlocker block(table_);
    const auto sounds = editableAudio().sounds;
    int partCount = 0;
    for (int i = 0; i < sounds.size(); ++i) {
        auto root = table_->topLevelItem(i); if (!root) continue;
        qDeleteAll(root->takeChildren());
        const auto decoded = audition_->sound(i);
        root->setText(0, sounds[i].name + (decoded ? QString(" — full sound (%1 parts)").arg(decoded->parts.size()) : " — loading parts…"));
        if (!decoded) continue;
        partCount += decoded->parts.size();
        for (int j = 0; j < decoded->parts.size(); ++j) {
            const auto &p = decoded->parts[j];
            QString label=QString("Part %1 · lasts %2 s").arg(j+1).arg(p.pcm.size()/128000.,0,'f',2);
            for(const auto &v:sounds[i].parts)if(v.toObject()["index"].toInt()==j) {
                const auto edit=v.toObject();
                label=QString("Part %1 · %2").arg(j+1).arg(edit["muted"].toBool()?"Muted":edit["source"].toObject()["name"].toString());
            }
            auto child = new QTreeWidgetItem(root, {label,"After full sound starts"});
            child->setData(0, Qt::UserRole, i); child->setData(0, Qt::UserRole + 1, j);
            child->setToolTip(0, "Select to replace, copy, import, mute or audition this part alone.");
            auto time=new QDoubleSpinBox;time->setRange(0,30);time->setDecimals(3);time->setSingleStep(1./60);
            time->setValue(p.tick/60.);time->setKeyboardTracking(false);time->setObjectName(QString("partTime_%1_%2").arg(i).arg(j));
            time->setToolTip("Seconds after the full sound starts, rounded to the native 1/60-second audio clock. Other parts keep their times.");
            table_->setItemWidget(child,2,time);
            connect(time,&QDoubleSpinBox::valueChanged,this,[this,i,j,time](double seconds) {
                guard([&]{table_->setCurrentItem(table_->topLevelItem(i)->child(j));
                    auto a=editableAudio();setSoundPartTime(project_->rom(),a.sounds[i],j,qRound(seconds*60));setAudio(a);});
            });
        }
        root->setExpanded(true);
    }
    if (auto root = table_->topLevelItem(row)) table_->setCurrentItem(part >= 0 ? root->child(part) : root);
    desiredRow_=desiredPart_=-1;
    mode_->setText(sounds.isEmpty() && audio_.enabled ? "Effect sounds muted" :
        QString("%1 · %2 full sounds, %3 parts. Select any row to listen.")
            .arg(audio_.enabled ? "Custom attack sounds" : "Original attack sounds").arg(sounds.size()).arg(partCount));
    select();
}
void AttackAudioEditor::select() {
    const auto sounds = editableAudio().sounds; const int row = selectedRow(), part = selectedPart();
    const bool selected = row >= 0 && row < sounds.size();
    replace_->setEnabled(ready_ && selected); remove_->setEnabled(ready_ && selected);
    replace_->setText(part>=0?"Replace part…":"Replace sound…");remove_->setText(part>=0?"Mute part":"Remove sound");
    import_->setText(part>=0?"Import part WAV…":"Import WAV…");
    if(auto action=replace_->menu()->findChild<QAction *>("restoreSoundPart"))action->setEnabled(selected && part>=0);
    audition_->setSelectedSound(row, part);
    if (selected && part >= 0)
        status_->setText("Changes apply only to this part in this attack. Other parts keep their sounds and timing.");
    else status_->clear();
}
void AttackAudioEditor::readOriginal() {
    auto attack = project_->attacks()->entry(attack_); attack.visual = visual_; attack.visual.audio = {};
    auto watcher = new QFutureWatcher<Trace>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        const auto result = watcher->result(); watcher->deleteLater(); anchors_ = result.first;
        QSet<int> seen;
        for (const auto &cue : anchors_) {
            if (cue.cue >= 0 && seen.contains(cue.cue)) continue;
            auto sound = soundFromCue(cue);
            if (cue.cue >= 0) { seen.insert(cue.cue); sound.anchor = cue.cue; sound.delay = 0; }
            originalSounds_.append(sound);
        }
        ready_ = true; status_->setText(result.second); refresh();
    });
    watcher->setFuture(QtConcurrent::run([rom = project_->rom(), attack] { return traceSounds(rom, attack); }));
}
void AttackAudioEditor::chooseAttack(bool replace) {
    const int destination = selectedRow(), destinationPart = selectedPart();
    QDialog dialog(this); dialog.setObjectName("copyAttackSoundDialog");
    dialog.setWindowTitle("Copy a sound or an individual part"); dialog.resize(860, 650);
    auto layout = new QVBoxLayout(&dialog);
    auto source = new QComboBox; source->setObjectName("attackSoundSource");
    for (int id = 1; id < project_->attacks()->entries().size(); ++id)
        source->addItem(AttackData::displayName(project_->attacks()->entry(id)), id);
    source->setCurrentIndex(source->findData(attack_));
    layout->addWidget(new QLabel("Sounds from attack")); layout->addWidget(source);
    auto choices = new SoundSelectionTree; choices->setObjectName("attackSoundChoice");
    choices->setColumnCount(3);choices->setHeaderLabels({"Full sound / part", "Part starts", "Duration"});
    choices->header()->setSectionResizeMode(0,QHeaderView::Stretch);
    choices->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    choices->header()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    choices->setSelectionMode(QAbstractItemView::SingleSelection);layout->addWidget(choices,1);
    auto preview = new AudioCueView({}, &dialog); preview->setCompact(true); preview->setRom(project_->rom());
    layout->addWidget(preview);
    auto status = new QLabel("Select a full sound or an indented part. Double-click to listen.");
    status->setWordWrap(true); layout->addWidget(status);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto use = buttons->addButton("Copy selected", QDialogButtonBox::AcceptRole);
    use->setObjectName("confirmCopyAttackSounds"); layout->addWidget(buttons);
    QVector<AttackSound> sounds; int generation = 0;
    auto select = [&] {
        auto item=choices->currentItem();
        const int row=item?item->data(0,Qt::UserRole).toInt():-1, part=item?item->data(0,Qt::UserRole+1).toInt():-1;
        preview->setSelectedSound(row,part);use->setEnabled(row>=0 && row<sounds.size() && bool(preview->sound(row)));
    };
    preview->soundsReady = [&] {
        QSignalBlocker block(choices);
        for(int i=0;i<sounds.size();++i) {
            auto root=choices->topLevelItem(i);if(!root)continue;
            const auto decoded=preview->sound(i);if(!decoded)continue;
            root->setText(0,sounds[i].name+QString(" — full sound (%1 parts)").arg(decoded->parts.size()));
            root->setText(2,QString("%1 s").arg(decoded->pcm.size()/128000.,0,'f',2));
            for(int j=0;j<decoded->parts.size();++j) {
                const auto &p=decoded->parts[j];auto child=new QTreeWidgetItem(root,{QString("Part %1").arg(j+1),
                    QString("+%1 s").arg(p.tick/60.,0,'f',3),QString("%1 s").arg(p.pcm.size()/128000.,0,'f',2)});
                child->setData(0,Qt::UserRole,i);child->setData(0,Qt::UserRole+1,j);
            }
            root->setExpanded(true);
        }
        if(choices->topLevelItemCount())choices->setCurrentItem(choices->topLevelItem(0));select();
    };
    auto scan = [&] {
        const int serial = ++generation;
        choices->clear(); sounds.clear(); use->setEnabled(false); preview->setMessage("Reading sounds…");
        status->setText("Reading sounds…");
        auto attack = project_->attacks()->entry(source->currentData().toInt());
        if (source->currentData().toInt() == attack_) { attack.visual = visual_; attack.visual.audio = audio_; }
        auto watcher = new QFutureWatcher<Trace>(&dialog);
        connect(watcher, &QFutureWatcherBase::finished, &dialog, [&, watcher, serial] {
            const auto result = watcher->result(); watcher->deleteLater();
            if (serial != generation) return;
            QVector<AudioCue> cues;
            for (const auto &cue : result.first) {
                auto sound=soundFromCue(cue);sound.name=cue.label;
                const int index=sounds.size();sounds.append(sound);cues.append(cue);
                auto root=new QTreeWidgetItem(choices,{sound.name});root->setData(0,Qt::UserRole,index);root->setData(0,Qt::UserRole+1,-1);
            }
            status->setText(!result.second.isEmpty()?result.second:sounds.isEmpty()?"This attack has no separate effect sounds.":
                "Select a full sound or any part below it. Copying keeps the destination's timing and does not change the donor.");
            preview->setCues(cues,source->currentText(),true);
        });
        watcher->setFuture(QtConcurrent::run([rom = project_->rom(), attack] { return traceSounds(rom, attack); }));
    };
    connect(source, &QComboBox::activated, &dialog, scan);
    connect(choices,&QTreeWidget::currentItemChanged,&dialog,select);
    connect(choices,&QTreeWidget::itemDoubleClicked,&dialog,[&]{preview->playSelectedSound();});
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        guard([&] {
            auto item=choices->currentItem();if(!item)return;
            const int selected=item->data(0,Qt::UserRole).toInt(), part=item->data(0,Qt::UserRole+1).toInt();
            if(selected<0 || selected>=sounds.size())return;
            auto sound=part>=0?isolateSoundPart(project_->rom(),sounds[selected],part):sounds[selected];
            auto a=editableAudio();
            if(replace && destination>=0 && destination<a.sounds.size()) {
                if(destinationPart>=0)replaceSoundPart(project_->rom(),a.sounds[destination],destinationPart,sound);
                else {sound.anchor=a.sounds[destination].anchor;sound.delay=a.sounds[destination].delay;a.sounds[destination]=sound;}
            } else {sound.anchor=-1;sound.delay=0;a.sounds.append(sound);}
            setAudio(a);dialog.accept();
        });
    });
    scan(); dialog.exec();
}
} // namespace lordly
