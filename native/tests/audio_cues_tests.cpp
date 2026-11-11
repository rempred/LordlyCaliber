#include "app/actor_pose_editor.h"
#include "app/theme.h"
#include "app/sound_output.h"
#include "app/audio_cue_view.h"
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollArea>
#include "app/attack_effect_editor.h"
#include "app/attack_audio_editor.h"
#include "engine/animation_descriptor.h"
#include "engine/audio_cues.h"
#include "engine/sound_preview.h"
#include "engine/sound_parts.h"
#include "rebuild/compiler.h"
#include "third_party/vadpcm/codec/vadpcm.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QPushButton>
#include <QSlider>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>
#include <algorithm>
using namespace lordly;
class AudioCuesTests : public QObject {
    Q_OBJECT
    Rom rom() {
        return Rom::open(qEnvironmentVariable("LORDLY_TEST_ROM"));
    }
    QJsonArray rows(const QVector<AudioCue> &cues) {
        QJsonArray out;
        for (const auto &c : cues)
            out.append(QJsonArray{c.tick, c.cue, c.request, c.context});
        return out;
    }
  private slots:
    void nativePartSources() {
        const auto original=rom();
        AttackSound source;source.request=303;source.enemyRequest=304;source.name="Gaze opening";
        const auto baseline=renderAuthoredSound(original,source);
        QCOMPARE(baseline.parts.size(),5);
        for(int i=0;i<5;++i) {
            const auto part=isolateSoundPart(original,source,i);
            QCOMPARE(part.state.size(),soundPartStateSize);
            const auto isolated=renderAuthoredSound(original,part);
            QCOMPARE(isolated.parts.size(),1);
            QCOMPARE(isolated.parts[0].sampleRomOffset,baseline.parts[i].sampleRomOffset);
            QVERIFY(!isolated.pcm.isEmpty());
            // Isolating a voice retains its native duration. Moving a 60 Hz
            // event can change sample rounding by one 32 kHz output frame.
            QVERIFY(qAbs(isolated.pcm.size()-baseline.parts[i].pcm.size())<=4);
            if(baseline.parts[i].tick%3==0)QCOMPARE(isolated.pcm,baseline.parts[i].pcm);
        }
        auto changed=source;
        setSoundPartTime(original,changed,3,15);
        QCOMPARE(changed.parts[0].toObject()["tick"].toInt(),15);
        auto edited=renderAuthoredSound(original,changed);
        QCOMPARE(edited.parts[3].tick,15);
        for(int i:{0,1,2,4})QCOMPARE(edited.parts[i].pcm,baseline.parts[i].pcm);
        QCOMPARE(AttackSound::fromJson(changed.json()),changed);
        // Import one selected part without changing the other four definitions.
        AttackSound tone;tone.name="Replacement";tone.pcm=QByteArray(3200,0);
        for(int i=0;i<1600;++i)qToLittleEndian<qint16>(i%16<8?8000:-8000,tone.pcm.data()+i*2);
        replaceSoundPart(original,changed,1,tone);
        QCOMPARE(changed.parts.size(),2);QCOMPARE(source.parts.size(),0);
        const auto replaced=renderAuthoredSound(original,changed);
        QVERIFY(replaced.parts[1].pcm!=baseline.parts[1].pcm);
        for(int i:{0,2,4})QCOMPARE(replaced.parts[i].pcm,baseline.parts[i].pcm);
        // Inspect the actual private carrier execution, not the UI's list of
        // logical parts. Unedited notes must retain their samples and PCM.
        for(int side:{0,1}) {
            const auto expected=renderAuthoredSound(original,source,side);
            AudioCue cue{0,-1,-2};cue.source=changed.json();cue.variant=side;
            const auto actual=renderSoundTimeline(original,{cue,{180,-1,303}},0,{},true);
            for(int i:{0,2,4}) {
                const auto &part=expected.parts[i];bool found=false;
                for(const auto &played:actual.parts)
                    if(played.tick==part.tick && played.sampleRomOffset==part.sampleRomOffset &&
                       u32(played.state,0x13c)==u32(part.state,0x13c) && u32(played.state,0x140)==u32(part.state,0x140)) {
                        QCOMPARE(played.pcm,part.pcm);found=true;break;
                    }
                QVERIFY2(found,qPrintable(QString("Missing untouched part %1, side %2").arg(i).arg(side)));
            }
            QVERIFY(std::any_of(actual.parts.begin(),actual.parts.end(),[&](const auto &p){return p.tick==15 && p.sampleRomOffset==expected.parts[3].sampleRomOffset;}));
            int later=0;for(const auto &p:actual.parts)if(p.tick>=360)++later;
            QCOMPARE(later,5); // A later unrelated sound still uses the retail bank.
        }
        auto delayed=source;setSoundPartTime(original,delayed,4,1200);
        AudioCue late{0,-1,-2};late.source=delayed.json();
        const auto longPreview=renderSoundTimeline(original,{late},0,{},true);
        QVERIFY(std::any_of(longPreview.parts.begin(),longPreview.parts.end(),[&](const auto &p){return p.tick==1200 && p.sampleRomOffset==baseline.parts[4].sampleRomOffset;}));
        AttackSound donor;donor.request=305;donor.name="Whole impact";
        auto whole=source;replaceSoundPart(original,whole,1,donor);
        AudioCue wholeCue{0,-1,-2};wholeCue.source=whole.json();
        const auto nested=renderSoundTimeline(original,{wholeCue},0,{},true);
        const auto donorPreview=renderSoundPreview(original,305,{},true);
        for(const auto &part:donorPreview.parts)
            QVERIFY(std::any_of(nested.parts.begin(),nested.parts.end(),[&](const auto &p){
                return p.tick==baseline.parts[1].tick+part.tick && p.sampleRomOffset==part.sampleRomOffset;}));
    }
    void partAuthoringUi() {
        auto p=Project::fromRom(rom());p.loadAttacks();const auto unchanged=p.manifest();
        AttackAudioEditor ui(&p,25,{});ui.show();
        auto rows=ui.findChild<QTreeWidget *>("attackSoundRows");
        QTRY_VERIFY_WITH_TIMEOUT(rows->topLevelItemCount()==2 && rows->topLevelItem(0)->childCount()==5,30000);
        rows->setCurrentItem(rows->topLevelItem(0)->child(1));
        QTimer::singleShot(0,[&] {
            auto dialog=QApplication::activeModalWidget();QVERIFY(dialog);
            auto choices=dialog->findChild<QTreeWidget *>("attackSoundChoice");QVERIFY(choices);
            QTRY_VERIFY_WITH_TIMEOUT(choices->topLevelItemCount()==2 && choices->topLevelItem(0)->childCount()==5 && choices->topLevelItem(1)->childCount()==3,30000);
            QVERIFY(choices->topLevelItem(0)->isExpanded());QVERIFY(choices->topLevelItem(1)->isExpanded());
            choices->setCurrentItem(choices->topLevelItem(0)->child(4));
            dialog->findChild<QSlider *>("audioVolume")->setValue(0);
            dialog->findChild<QPushButton *>("audioPlay")->click();
            auto output=dynamic_cast<SoundOutput *>(dialog->findChild<QObject *>("audioOutput"));QVERIFY(output);QVERIFY(output->frameCount()>0);
            dialog->findChild<QPushButton *>("audioStop")->click();
            dialog->grab().save(QString::fromUtf8(LORDLY_SOURCE_DIR)+"/build/audio-parts-donor.png");
            dialog->findChild<QPushButton *>("confirmCopyAttackSounds")->click();
        });
        ui.findChild<QPushButton *>("copyAttackSounds")->click();
        QCOMPARE(ui.audio().sounds[0].parts.size(),1);
        auto edit=ui.audio().sounds[0].parts[0].toObject();QCOMPARE(edit["index"].toInt(),1);
        QVERIFY(edit["source"].toObject()["name"].toString().endsWith("part 5"));
        QTRY_VERIFY_WITH_TIMEOUT(ui.findChild<QDoubleSpinBox *>("partTime_0_1"),30000);
        ui.findChild<QDoubleSpinBox *>("partTime_0_1")->setValue(.75);
        QCOMPARE(ui.audio().sounds[0].parts[0].toObject()["tick"].toInt(),45);
        // Rebuilding asynchronous previews must not temporarily retarget the
        // selected part to its full-sound header.
        QCOMPARE(ui.findChild<QPushButton *>("removeAttackSound")->text(),QString("Mute part"));
        QTRY_VERIFY_WITH_TIMEOUT(rows->topLevelItem(0)->childCount()==5,30000);
        rows->setCurrentItem(rows->topLevelItem(0)->child(3));
        QTimer::singleShot(0,[&] {
            auto dialog=QApplication::activeModalWidget();QVERIFY(dialog);
            auto choices=dialog->findChild<QTreeWidget *>("attackSoundChoice");
            QTRY_VERIFY_WITH_TIMEOUT(choices->topLevelItemCount()==2 && choices->topLevelItem(1)->childCount()==3,30000);
            choices->setCurrentItem(choices->topLevelItem(1));
            dialog->findChild<QPushButton *>("confirmCopyAttackSounds")->click();
        });
        ui.findChild<QPushButton *>("copyAttackSounds")->click();
        QCOMPARE(ui.audio().sounds[0].parts.size(),2);
        const auto whole=AttackSound::fromJson(ui.audio().sounds[0].parts[1].toObject()["source"].toObject());
        QVERIFY(whole.state.isEmpty());QCOMPARE(whole.cue,160);
        QTRY_VERIFY_WITH_TIMEOUT(rows->topLevelItem(0)->childCount()==5,30000);
        ui.grab().save(QString::fromUtf8(LORDLY_SOURCE_DIR)+"/build/audio-parts-authored.png");
        QCOMPARE(p.manifest(),unchanged); // Dialog edits stay staged until its parent applies them.
        // Moving from a selected part to Mute must not retarget the action.
        auto selectedPart=rows->topLevelItem(0)->child(4);
        QTest::mouseClick(rows->viewport(),Qt::LeftButton,Qt::NoModifier,rows->visualItemRect(selectedPart).center());
        for(int i=3;i>=0;--i)QTest::mouseMove(rows->viewport(),rows->visualItemRect(rows->topLevelItem(0)->child(i)).center());
        QTest::mouseMove(rows->viewport(),rows->visualItemRect(rows->topLevelItem(0)).center());
        QCOMPARE(rows->currentItem(),selectedPart);
        // A held-button move must not turn selection into a sweep of rows either.
        QTest::mousePress(rows->viewport(),Qt::LeftButton,Qt::NoModifier,rows->visualItemRect(selectedPart).center());
        const auto overHeader=rows->visualItemRect(rows->topLevelItem(0)).center();
        QMouseEvent drag(QEvent::MouseMove,overHeader,rows->viewport()->mapToGlobal(overHeader),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(rows->viewport(),&drag);
        QTest::mouseRelease(rows->viewport(),Qt::LeftButton,Qt::NoModifier,rows->visualItemRect(rows->topLevelItem(0)).center());
        QCOMPARE(rows->currentItem(),selectedPart);
        auto mute=ui.findChild<QPushButton *>("removeAttackSound");
        QTest::mouseMove(mute);QCOMPARE(mute->text(),QString("Mute part"));QTest::mouseClick(mute,Qt::LeftButton);
        QCOMPARE(ui.audio().sounds.size(),2);
        const auto muted=ui.audio().sounds[0].parts.last().toObject();
        QCOMPARE(muted["index"].toInt(),4);QVERIFY(muted["muted"].toBool());
    }
    void individualSoundParts() {
        const auto source = rom();
        for (int request : {303, 304, 305, 306}) {
            QProcess proof;
            proof.start("node", {QString::fromUtf8(LORDLY_SOURCE_DIR) + "/tests/sound-preview-mips.cjs",
                                  qEnvironmentVariable("LORDLY_TEST_ROM"), QString::number(request)});
            QVERIFY(proof.waitForFinished(30000));
            QCOMPARE(proof.exitCode(), 0);
            const auto reference = QJsonDocument::fromJson(proof.readAllStandardOutput()).object();
            const auto parts = reference["parts"].toArray();
            const auto separated = renderSoundPreview(source, request, {}, true);
            QCOMPARE(separated.pcm, renderSoundPreview(source, request).pcm);
            QCOMPARE(separated.parts.size(), (QMap<int,int>{{303,5},{304,5},{305,3},{306,3}}.value(request)));
            QCOMPARE(separated.parts.size(), parts.size());
            QVector<int> sum(separated.pcm.size() / 2, 0);
            for (int i = 0; i < parts.size(); ++i) {
                const auto &part = separated.parts[i];
                const auto expected = parts[i].toObject();
                QCOMPARE(part.tick, expected["tick"].toInt());
                QCOMPARE(part.voice, expected["voice"].toInt());
                QCOMPARE(part.sampleRomOffset, quint32(expected["sampleRomOffset"].toDouble()));
                QVERIFY(!part.pcm.isEmpty());
                const auto at = qint64(part.tick) * 32000 / 60 * 2;
                QVERIFY(at + part.pcm.size() / 2 <= sum.size());
                for (qsizetype n = 0; n < part.pcm.size() / 2; ++n)
                    sum[at+n] += qFromLittleEndian<qint16>(part.pcm.constData()+n*2);
                qInfo() << "request" << request << "part" << i+1 << "audio tick" << part.tick
                        << "frames" << part.pcm.size()/4;
            }
            for (qsizetype n = 0; n < sum.size(); ++n) {
                const int full = qFromLittleEndian<qint16>(separated.pcm.constData()+n*2);
                // Each isolated PCM rounds once; the full mix rounds after summing.
                QVERIFY(qAbs(qBound(-32768,sum[n],32767)-full) <= separated.parts.size());
            }
        }
    }
    void individualSoundPartsUi() {
        auto project = Project::fromRom(rom()); project.loadAttacks();
        const auto before = project.manifest();
        AttackEffectEditor editor(&project,25,2); editor.show();
        auto plays = editor.findChild<QTreeWidget *>("audioCueTable");
        QTRY_COMPARE_WITH_TIMEOUT(plays->topLevelItemCount(),6,30000);
        QTRY_COMPARE_WITH_TIMEOUT(plays->topLevelItem(0)->childCount(),5,30000);
        QTRY_COMPARE_WITH_TIMEOUT(plays->topLevelItem(1)->childCount(),3,30000);
        QVERIFY(!editor.findChild<QComboBox *>("audioSoundParts"));
        QVERIFY(plays->topLevelItem(1)->text(1).contains("repeat 1/5"));
        const int request = plays->topLevelItem(1)->data(2,Qt::UserRole).toInt();
        const auto sound = renderSoundPreview(project.rom(), request, {}, true);
        const int selected = 2;
        QVERIFY(sound.parts[selected].pcm.size()!=sound.pcm.size());
        editor.findChild<QSlider *>("audioVolume")->setValue(0);
        plays->setCurrentItem(plays->topLevelItem(1)->child(selected));
        auto output = dynamic_cast<SoundOutput *>(editor.findChild<QObject *>("audioOutput")); QVERIFY(output);
        editor.findChild<QPushButton *>("audioPlay")->click();
        QTRY_COMPARE_WITH_TIMEOUT(output->frameCount(),sound.parts[selected].pcm.size()/4,10000);
        QVERIFY(editor.findChild<QLabel *>("audioPlaybackStatus")->text().startsWith("Playing part 3"));
        editor.findChild<QPushButton *>("audioStop")->click();
        plays->setCurrentItem(plays->topLevelItem(1)); plays->itemDoubleClicked(plays->topLevelItem(1),1);
        QTRY_COMPARE_WITH_TIMEOUT(output->frameCount(),sound.pcm.size()/4,10000);
        editor.findChild<QPushButton *>("audioStop")->click();
        // All controls must remain reachable at the dialog's smallest size,
        // even with the expanded multi-part cue list and playback status.
        editor.resize(editor.minimumSizeHint().expandedTo(editor.minimumSize()));
        auto position=editor.findChild<QLabel *>("audioCuePosition");
        QTRY_VERIFY_WITH_TIMEOUT(plays->mapTo(&editor,QPoint(0,plays->height())).y()<=position->mapTo(&editor,QPoint()).y(),2000);
        for(const auto name:{"attackEffectPreview","effectPlay","audioPlay","editAttackSounds","effectButtons"}) {
            auto control=editor.findChild<QWidget *>(name);QVERIFY(control);
            QTRY_VERIFY_WITH_TIMEOUT(control->visibleRegion().contains(control->rect()),2000);
        }
        qInfo()<<"Effect dialog minimum"<<editor.size();
        editor.grab().save(QString::fromUtf8(LORDLY_SOURCE_DIR)+"/build/audio-layout-minimum.png");
        AttackAudioEditor sounds(&project,25,{}); sounds.show();
        auto groups = sounds.findChild<QTreeWidget *>("attackSoundRows");
        QTRY_COMPARE_WITH_TIMEOUT(groups->topLevelItemCount(),2,30000);
        QTRY_COMPARE_WITH_TIMEOUT(groups->topLevelItem(0)->childCount(),5,30000);
        QTRY_COMPARE_WITH_TIMEOUT(groups->topLevelItem(1)->childCount(),3,30000);
        QVERIFY(!sounds.findChild<QComboBox *>("audioSoundParts"));
        groups->setCurrentItem(groups->topLevelItem(1)->child(selected));
        sounds.findChild<QSlider *>("audioVolume")->setValue(0);
        sounds.findChild<QPushButton *>("audioPlay")->click();
        output = dynamic_cast<SoundOutput *>(sounds.findChild<QObject *>("audioOutput")); QVERIFY(output);
        QTRY_COMPARE_WITH_TIMEOUT(output->frameCount(),sound.parts[selected].pcm.size()/4,10000);
        QVERIFY(sounds.findChild<QPushButton *>("replaceAttackSound")->isEnabled());
        sounds.findChild<QPushButton *>("audioStop")->click();
        groups->setCurrentItem(groups->topLevelItem(1));
        QVERIFY(sounds.findChild<QPushButton *>("replaceAttackSound")->isEnabled());
        sounds.grab().save(QString::fromUtf8(LORDLY_SOURCE_DIR)+"/build/audio-table-editor.png");
        QVERIFY(!sounds.audio().enabled); QCOMPARE(project.manifest(),before);
    }
    void soundAuthoringEntryPoints() {
        try {
            auto p=Project::fromRom(rom());p.loadAttacks();
            const auto before=p.manifest();AttackAudioEditor ui(&p,25,{});
            ui.show();auto rows=ui.findChild<QTreeWidget *>("attackSoundRows");
            QTRY_COMPARE_WITH_TIMEOUT(rows->topLevelItemCount(),2,30000);
            const auto timing=qobject_cast<QComboBox *>(rows->itemWidget(rows->topLevelItem(0),1));
            QVERIFY(timing->width()>=timing->fontMetrics().horizontalAdvance(timing->currentText())+20);
            QVERIFY(!ui.audio().enabled); // Looking at original sounds must not author an override.
            qobject_cast<QDoubleSpinBox *>(rows->itemWidget(rows->topLevelItem(0),2))->setValue(0.43);
            QCOMPARE(ui.audio().sounds[0].delay,13);
            QCOMPARE(ui.audio().sounds[0].anchor,159);QCOMPARE(ui.audio().sounds[1].anchor,160);
            rows->setCurrentItem(rows->topLevelItem(0));
            QTimer::singleShot(0,[&] {
                auto dialog=QApplication::activeModalWidget();QVERIFY(dialog);
                auto source=dialog->findChild<QComboBox *>("attackSoundSource");QVERIFY(source);
                source->setCurrentIndex(source->findData(38));source->activated(source->currentIndex());
                auto choices=dialog->findChild<QTreeWidget *>("attackSoundChoice");
                QTRY_VERIFY_WITH_TIMEOUT(choices->topLevelItemCount()>0 && choices->topLevelItem(0)->text(0).contains("Petrify"),30000);
                QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QPushButton *>("confirmCopyAttackSounds")->isEnabled(),30000);
                dialog->findChild<QPushButton *>("confirmCopyAttackSounds")->click();
            });
            ui.findChild<QPushButton *>("replaceAttackSound")->menu()->actions().first()->trigger();
            QCOMPARE(ui.audio().sounds[0].cue,175);QCOMPARE(ui.audio().sounds[0].anchor,159);
            QCOMPARE(ui.audio().sounds[0].delay,13);QCOMPARE(ui.audio().sounds[1].anchor,160);
            const auto authored=ui.audio();AttackEffectEditor outer(&p,25,2);
            QTimer::singleShot(0,[&]{auto modal=dynamic_cast<AttackAudioEditor *>(QApplication::activeModalWidget());QVERIFY(modal);modal->setAudio(authored);modal->accept();});
            outer.findChild<QPushButton *>("editAttackSounds")->click();QCOMPARE(outer.visual().audio,authored);
            outer.findChild<QPushButton *>("effectUndo")->click();QVERIFY(!outer.visual().audio.enabled);
            outer.findChild<QPushButton *>("effectRedo")->click();QCOMPARE(outer.visual().audio,authored);
            ui.findChild<QPushButton *>("restoreAttackSounds")->click();QVERIFY(!ui.audio().enabled);
            ui.grab().save(QString::fromUtf8(LORDLY_SOURCE_DIR)+"/build/audio-simplified.png");
            QTimer::singleShot(0,[&] {
                auto dialog=QApplication::activeModalWidget();QVERIFY(dialog);
                auto source=dialog->findChild<QComboBox *>("attackSoundSource");source->setCurrentIndex(source->findData(38));source->activated(source->currentIndex());
                auto choices=dialog->findChild<QTreeWidget *>("attackSoundChoice");
                QTRY_VERIFY_WITH_TIMEOUT(choices->topLevelItemCount()>0 && choices->topLevelItem(0)->text(0).contains("Petrify"),30000);
                QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QPushButton *>("confirmCopyAttackSounds")->isEnabled(),30000);
                dialog->findChild<QPushButton *>("confirmCopyAttackSounds")->click();
            });
            ui.findChild<QPushButton *>("copyAttackSounds")->click();
            QCOMPARE(ui.audio().sounds.size(),2);QCOMPARE(ui.audio().sounds[0].cue,175);
            QCOMPARE(ui.audio().sounds[0].anchor,159);QCOMPARE(ui.audio().sounds[0].delay,0);
            QCOMPARE(p.manifest(),before);
        } catch(const std::exception &e){QFAIL(e.what());}
    }
    void synchronizedSoundTimeline() {
        const auto source=rom();
        EffectPlayer player(source);EffectPlayer::Scene scene;scene.targetCount=1;
        player.start(105,scene);while(player.advance()){}
        const auto cues=player.audioCues();QCOMPARE(cues.size(),2);
        auto mix=renderSoundTimeline(source,cues,player.tick());
        // Original game queue + libmus instruction trace: IDs and durations are
        // obtained downstream of dispatch, independently of the host renderer.
        QJsonArray scheduled;
        for(const auto &cue:cues)scheduled.append(QJsonObject{{"tick",cue.tick},{"request",cue.request}});
        QProcess proof;
        proof.start("node", {QString::fromUtf8(LORDLY_SOURCE_DIR)+"/tests/sound-preview-mips.cjs",
            qEnvironmentVariable("LORDLY_TEST_ROM"), "303", QString::fromUtf8(QJsonDocument(scheduled).toJson(QJsonDocument::Compact))});
        QVERIFY(proof.waitForFinished(30000));QCOMPARE(proof.exitCode(),0);
        const auto expected=QJsonDocument::fromJson(proof.readAllStandardOutput()).object();
        QCOMPARE(mix.voices,expected["voices"].toInt());
        QCOMPARE(mix.ticks,qMax(player.tick()*2,expected["ticks"].toInt()));
        // The same request twice in a single queue update is coalesced by the
        // game. Summing two independent renders would double the sound.
        const auto single=renderSoundTimeline(source,{{0,-1,305}},0);
        const auto duplicate=renderSoundTimeline(source,{{0,-1,305},{0,-1,305}},0);
        QCOMPARE(duplicate.pcm,single.pcm);QCOMPARE(duplicate.voices,single.voices);
        auto p=Project::fromRom(source);p.loadAttacks();AttackEffectEditor ui(&p,25,2);ui.resize(1000,700);ui.show();
        auto play=ui.findChild<QPushButton *>("effectPlay");
        QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled(),30000);
        ui.findChild<QSlider *>("audioVolume")->setValue(0);
        auto table=ui.findChild<QTreeWidget *>("audioCueTable");auto pos=ui.findChild<QLabel *>("audioCuePosition");
        // Cue discovery changes the table height asynchronously. Let Qt deliver
        // the resulting LayoutRequest before checking the displayed geometry.
        QTRY_VERIFY_WITH_TIMEOUT(!QRect(table->mapTo(&ui,QPoint()),table->size()).intersects(
                                     QRect(pos->mapTo(&ui,QPoint()),pos->size())),2000);
        for(auto label:ui.findChildren<QLabel *>())QVERIFY(!label->text().contains(QString::fromUtf8("Â")));
        play->click();QTRY_COMPARE_WITH_TIMEOUT(play->text(),QString("Pause"),30000);
        QTest::qWait(250);play->click();const auto paused=pos->text();QTest::qWait(150);QCOMPARE(pos->text(),paused);
        play->click();QTRY_VERIFY_WITH_TIMEOUT(pos->text()!=paused,3000);
        QTRY_COMPARE_WITH_TIMEOUT(play->text(),QString("Play"),15000);
        ui.findChild<QCheckBox *>("effectLoop")->setChecked(true);play->click();
        QTest::qWait(mix.pcm.size()/128+200);QCOMPARE(play->text(),QString("Pause"));
        play->click();ui.grab().save(QString::fromUtf8(LORDLY_SOURCE_DIR)+"/build/effect-sound-layout.png");
        // A short effect ending between whole milliseconds must also finish.
        ui.findChild<QCheckBox *>("effectLoop")->setChecked(false);
        QImage frame(8,8,QImage::Format_RGBA8888);frame.fill(Qt::red);
        AttackVisual shortVisual;shortVisual.frames=SpriteAsset::fromImage("Short effect",frame);
        shortVisual.frames->frames[0].ticks=4;shortVisual.audio.enabled=true;
        AttackSound shortSound;shortSound.pcm=QByteArray(640,0);shortSound.name="Short clip";
        shortVisual.audio.sounds.append(shortSound);ui.setVisual(shortVisual);
        QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled(),30000);
        int total=0;for(const auto &f:ui.renderedSequence().frames)total+=f.ticks;
        play->click();
        QTRY_COMPARE_WITH_TIMEOUT(play->text(),QString("Play"),10000);
        QCOMPARE(ui.findChild<QLabel *>("effectFramePosition")->text(),QString("Frame %1 / %1 · 30 fps").arg(total));
        ui.close();
    }
    void audioExportRoundTrip() {
        try {
            const auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            QVERIFY(!tools.isEmpty());
            auto project=Project::fromRom(rom());project.loadAttacks();
            QTemporaryDir temp;QCOMPARE(rebuild(project,tools,temp.path()).image,rom().bytes());
            auto data=*project.attacks();auto a=data.entry(25);
            a.visual.audio.enabled=true;AttackSound sound;sound.anchor=159;sound.cue=175;sound.delay=9;sound.name="Petrify opening";
            a.visual.audio.sounds.append(sound);sound.anchor=-1;sound.delay=23;sound.cue=-1;sound.name="Imported tone";
            sound.pcm=QByteArray(6400,0);for(int i=0;i<3200;i++)qToLittleEndian<qint16>(i%32<16?12000:-12000,sound.pcm.data()+i*2);
            a.visual.audio.sounds.append(sound);data.set(25,a);
            AttackSound opening;opening.name="Edited opening";opening.cue=159;opening.anchor=159;opening.request=303;
            setSoundPartTime(rom(),opening,3,120);
            replaceSoundPart(rom(),opening,1,sound);
            a.visual.audio.sounds.append(opening);data.set(25,a);
            int copy=data.duplicate(25,"Independent sound copy");auto b=data.entry(copy);b.visual.audio.sounds[0].delay=18;
            setSoundPartTime(rom(),b.visual.audio.sounds[2],3,180);data.set(copy,b);
            QCOMPARE(data.entry(25).visual.audio.sounds[2].parts[0].toObject()["tick"].toInt(),120);
            project.setAttacks(data.json());project.saveArchive(temp.filePath("audio.lcproj"));
            auto saved=Project::fromRom(rom());saved.applyManifest(Project::readProject(temp.filePath("audio.lcproj")));saved.loadAttacks();
            QCOMPARE(saved.attacks()->json(),data.json());
            auto result=rebuild(saved,tools,temp.path());
            const auto out=qEnvironmentVariable("LORDLY_AUDIO_TEST_OUTPUT");QVERIFY(!out.isEmpty());QDir().mkpath(out);
            auto file=QDir(out).filePath("Ogre Battle 64 - Person of Lordly Caliber (USA).z64");exportRom(saved,result,file);
            auto exported=Rom::open(file);auto reopened=Project::fromRom(exported);reopened.loadAttacks();
            auto checksum=result.image;Rom::updateChecksum(checksum);QCOMPARE(checksum,result.image);
            QCOMPARE(reopened.attacks()->json(),data.json());
            QCOMPARE(rebuild(reopened,tools,temp.path()).image,result.image);
            const auto placement=AttackPlacement::read(exported);QVERIFY(placement);
            auto entry=placement->romOffset+AttackData::settingsOffset+25*32;
            auto wrapper=placement->romOffset+u32(exported.bytes(),entry+24)-placement->address;
            auto audio=wrapper+u32(exported.bytes(),wrapper+16);
            auto bank=audio+u32(exported.bytes(),audio+32+16+8);
            QCOMPARE(u32(exported.bytes(),bank+0x40),bank+256);
            QCOMPARE(u32(exported.bytes(),bank+0x44),quint32(sound.pcm.size()));
            QVERIFY(bank+256+sound.pcm.size()<=placement->romOffset+placement->size);
            qInfo()<<"Audio export candidate"<<file;
        } catch(const std::exception &e){QFAIL(e.what());}
    }
    void editedSounds() {
        try {
            auto source=rom();
            for(bool enemy:{false,true}) {
                EffectPlayer::Scene scene;scene.enemyCaster=enemy;scene.targetCount=1;scene.sourceHeight=scene.targetHeight=38;
                EffectPlayer original(source);original.start(105,scene);while(original.advance()){}
                const auto impact=*std::find_if(original.audioCues().begin(),original.audioCues().end(),[](auto c){return c.cue==160;});
                AttackVisual v;v.audio.enabled=true;AttackSound replacement;replacement.anchor=160;replacement.cue=175;replacement.delay=7;v.audio.sounds.append(replacement);
                EffectPlayer edited(source);edited.startVisual(v,105,scene);while(edited.advance()){}
                qInfo()<<"baseline"<<enemy<<rows(original.audioCues())<<original.tick()<<"edited"<<rows(edited.audioCues())<<edited.tick();
                for(auto i:original.impacts())qInfo()<<"baseline impact"<<i.tick;
                for(auto i:edited.impacts())qInfo()<<"edited impact"<<i.tick;
                QCOMPARE(edited.audioCues().size(),1);
                QCOMPARE(edited.audioCues()[0].request,enemy?281:280);
                QCOMPARE(edited.audioCues()[0].tick,impact.tick+7);
                QCOMPARE(edited.impacts().size(),original.impacts().size());
                for(int i=0;i<original.impacts().size();++i)QCOMPARE(edited.impacts()[i].tick,original.impacts()[i].tick);
                v.audio.sounds[0].anchor=-1;v.audio.sounds[0].delay=19;
                edited.startVisual(v,118,scene);while(edited.advance()){}
                QCOMPARE(edited.audioCues().size(),1);QCOMPARE(edited.audioCues()[0].tick,19);
                v.audio.sounds.clear();edited.startVisual(v,105,scene);while(edited.advance()){}QVERIFY(edited.audioCues().isEmpty());
            }
        } catch(const std::exception &e){QFAIL(e.what());}
    }
    void importedSounds() {
        try {
            QByteArray wav(44+6400,0);wav.replace(0,4,"RIFF");qToLittleEndian<quint32>(wav.size()-8,wav.data()+4);
            wav.replace(8,8,"WAVEfmt ");qToLittleEndian<quint32>(16,wav.data()+16);
            for(auto p:{qMakePair(20,1),qMakePair(22,1),qMakePair(32,2),qMakePair(34,16)})qToLittleEndian<quint16>(p.second,wav.data()+p.first);
            qToLittleEndian<quint32>(32000,wav.data()+24);qToLittleEndian<quint32>(64000,wav.data()+28);
            wav.replace(36,4,"data");qToLittleEndian<quint32>(6400,wav.data()+40);
            for(int i=0;i<3200;i++)qToLittleEndian<qint16>(i%32<16?12000:-12000,wav.data()+44+i*2);
            auto sound=AttackSound::importWav(wav,"Test tone");QCOMPARE(sound.pcm,wav.mid(44));
            auto invalid=wav;qToLittleEndian<quint32>(44100,invalid.data()+24);QVERIFY_THROWS_EXCEPTION(Error,AttackSound::importWav(invalid,"bad"));
            auto preview=renderImportedSoundPreview(rom(),sound);QCOMPARE(preview.voices,1);QVERIFY(!preview.limited);QVERIFY(preview.ticks<30);
            const auto isolated=renderImportedSoundPreview(rom(),sound,{},true);
            QCOMPARE(isolated.pcm,preview.pcm);QCOMPARE(isolated.parts.size(),1);
            QVERIFY(!isolated.parts[0].pcm.isEmpty());
            QVERIFY(std::any_of(preview.pcm.begin(),preview.pcm.end(),[](char c){return c!=0;}));
            AttackVisual v;v.audio.enabled=true;sound.delay=11;v.audio.sounds.append(sound);
            auto encoded=v.build(118,0x3000000);QCOMPARE(AttackVisual::read(encoded),v);QCOMPARE(AttackVisual::fromJson(v.json()),v);
            EffectPlayer p(rom());EffectPlayer::Scene scene;scene.targetCount=1;p.startVisual(v,118,scene);while(p.advance()){}
            QCOMPARE(p.audioCues().size(),1);QCOMPARE(p.audioCues()[0].tick,11);QCOMPARE(p.audioCues()[0].pcm,sound.pcm);
            QImage frame(8,8,QImage::Format_RGBA8888);frame.fill(Qt::red);v.frames=SpriteAsset::fromImage("Short visual",frame);v.frames->frames[0].ticks=2;
            p.startVisual(v,118,scene);while(p.advance()){}
            QCOMPARE(p.audioCues().size(),1);QVERIFY(!p.impacts().isEmpty());
            QCOMPARE(p.audioCues()[0].tick,p.impacts()[0].tick+11);
            auto damaged=encoded;auto at=u32(damaged,16);put32(damaged,at+32+8,0xfffffff0);QVERIFY_THROWS_EXCEPTION(Error,AttackVisual::read(damaged));
            QTemporaryDir temp;auto path=temp.filePath("tone.wav");atomicWrite(path,wav);
            auto project=Project::fromRom(rom());project.loadAttacks();AttackAudioEditor ui(&project,55,{});
            QTRY_VERIFY_WITH_TIMEOUT(ui.findChild<QPushButton *>("importAttackSound")->isEnabled(),30000);
            ui.setAudio({true,{}});ui.importWav(path);QCOMPARE(ui.audio().sounds[0].pcm,sound.pcm);
            auto table=ui.findChild<QTreeWidget *>("attackSoundRows");QVERIFY(table);QCOMPARE(table->topLevelItemCount(),1);
            qobject_cast<QDoubleSpinBox *>(table->itemWidget(table->topLevelItem(0),2))->setValue(0.9);QCOMPARE(ui.audio().sounds[0].delay,27);
            ui.show();QTest::qWait(100);ui.grab().save(QString::fromUtf8(LORDLY_SOURCE_DIR)+"/build/audio-edit-dialog.png");
            ui.findChild<QPushButton *>("removeAttackSound")->click();QVERIFY(ui.audio().sounds.isEmpty());
            AttackAudioEditor parts(&project,25,{});parts.show();
            auto partRows=parts.findChild<QTreeWidget *>("attackSoundRows");
            QTRY_VERIFY_WITH_TIMEOUT(partRows->topLevelItemCount()==2 && partRows->topLevelItem(0)->childCount()==5,30000);
            partRows->setCurrentItem(partRows->topLevelItem(0)->child(2));
            parts.importWav(path,true);
            QCOMPARE(parts.audio().sounds.size(),2);QCOMPARE(parts.audio().sounds[0].parts.size(),1);
            const auto partEdit=parts.audio().sounds[0].parts[0].toObject();
            QCOMPARE(partEdit["index"].toInt(),2);QCOMPARE(partEdit["tick"].toInt(),-1);
            QCOMPARE(AttackSound::fromJson(partEdit["source"].toObject()).pcm,sound.pcm);
            QVERIFY(parts.audio().sounds[1].parts.isEmpty());
            AttackEffectEditor outer(&project,25,2);const auto authored=parts.audio();
            QTimer::singleShot(0,[&]{auto modal=dynamic_cast<AttackAudioEditor *>(QApplication::activeModalWidget());QVERIFY(modal);modal->setAudio(authored);modal->accept();});
            outer.findChild<QPushButton *>("editAttackSounds")->click();QCOMPARE(outer.visual().audio,authored);
            outer.findChild<QPushButton *>("effectUndo")->click();QVERIFY(!outer.visual().audio.enabled);
            outer.findChild<QPushButton *>("effectRedo")->click();QCOMPARE(outer.visual().audio,authored);
            auto format=ui.findChild<QLabel *>("attackAudioImportFormat");QVERIFY(format->text().contains("32,000"));
        } catch(const std::exception &e){QFAIL(e.what());}
    }
    void initTestCase() {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        applyEditorTheme(*qApp);
        if (qEnvironmentVariableIsEmpty("LORDLY_TEST_ROM"))
            QSKIP("Private original ROM required");
    }
    void originalRequests() {
        try {
            const auto source = rom();
            for (int effect : {105, 118})
                for (bool enemy : {false, true}) {
                    EffectPlayer player(source);
                    EffectPlayer::Scene scene;
                    scene.targetCount = 1;
                    scene.sourceHeight = scene.targetHeight = 38;
                    scene.enemyCaster = enemy;
                    player.start(effect, scene);
                    QTemporaryDir dir;
                    const auto memory = dir.filePath("initial.bin"),
                               report = dir.filePath("expected.json");
                    atomicWrite(memory, player.memory());
                    const auto initial = rows(player.audioCues());
                    while (player.advance()) {
                    }
                    QVERIFY(!player.audioCues().isEmpty());
                    const auto first = player.audioCues().first();
                    QCOMPARE(first.cue, effect == 105 ? 0x9f : 0xaf);
                    QCOMPARE(first.request,
                             effect == 105 ? (enemy ? 304 : 303) : (enemy ? 281 : 280));
                    if (effect == 105) {
                        QVERIFY(std::any_of(player.audioCues().begin(), player.audioCues().end(),
                                            [](const auto &c) { return c.cue == 0xa0; }));
                    }
                    atomicWrite(report,
                                QJsonDocument(QJsonObject{{"initial", initial},
                                                          {"ticks", player.tick()},
                                                          {"cues", rows(player.audioCues())}})
                                    .toJson());
                    QProcess proof;
                    proof.start("node", {QString::fromUtf8(LORDLY_SOURCE_DIR) +
                                             "/tests/audio-cues-mips.cjs",
                                         qEnvironmentVariable("LORDLY_TEST_ROM"), memory, report});
                    QVERIFY(proof.waitForFinished(120000));
                    QVERIFY2(
                        proof.exitCode() == 0,
                        (proof.readAllStandardOutput() + proof.readAllStandardError()).constData());
                    qInfo() << effect << enemy << player.tick() << rows(player.audioCues());
                }
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void delayedAndImported() {
        try {
            const auto source = rom();
            EffectPlayer::Scene scene;
            scene.targetCount = 1;
            EffectPlayer a(source), b(source);
            AttackVisual original, delayed;
            delayed.delay = 17;
            a.startVisual(original, 105, scene);
            b.startVisual(delayed, 105, scene);
            while (a.advance()) {
            }
            while (b.advance()) {
            }
            QCOMPARE(a.audioCues().size(), b.audioCues().size());
            for (int i = 0; i < a.audioCues().size(); ++i) {
                QCOMPARE(b.audioCues()[i].request, a.audioCues()[i].request);
                QCOMPARE(b.audioCues()[i].tick, a.audioCues()[i].tick + 17);
            }
            QImage image(8, 8, QImage::Format_RGBA8888);
            image.fill(Qt::red);
            AttackVisual imported;
            imported.frames = SpriteAsset::fromImage("Silent frames", image);
            a.startVisual(imported, 105, scene);
            while (a.advance()) {
            }
            QVERIFY(a.audioCues().isEmpty());
            scene.strikeCount = 3;
            a.startVisual({}, 118, scene);
            while (a.advance()) {
            }
            QCOMPARE(a.audioCues().size(), 3);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void classMotion() {
        try {
            const auto source = rom();
            auto art = ActorArt::loadBody(source, 5, 5, 0, 0);
            auto descriptor = AnimationDescriptor::load(source, art.descriptorKey());
            auto poses = descriptor.poses();
            const auto token = art.poseRecord(0, 0).operands[0];
            poses.setProgram(
                0, PoseBank::encodeProgram({{17, QByteArray::fromHex("0010")},
                                            {1, QByteArray(1, token) + QByteArray(1, char(4))},
                                            {19, QByteArray::fromHex("0020")},
                                            {1, QByteArray(1, token) + QByteArray(1, char(2))},
                                            {18, QByteArray::fromHex("009f")},
                                            {0, {}}}));
            descriptor.setPoses(poses);
            const auto trace = tracePoseAudio(source, art.withDescriptor(descriptor), 0, 0);
            QCOMPARE(trace.cues.size(), 3);
            QCOMPARE(trace.cues[0].tick, 0);
            QCOMPARE(trace.cues[1].tick, 2);
            QCOMPARE(trace.cues[2].tick, 3);
            QCOMPARE(trace.cues[0].request, int(u16(source.bytes(), 0x212880 + 0x10 * 2)));
            QCOMPARE(trace.cues[1].context, 1);
            QCOMPARE(trace.cues[2].request, -1);
            QVERIFY(trace.cues[2].spatial);
            poses.setProgram(0, PoseBank::encodeProgram({{18, QByteArray::fromHex("009f")},
                                                         {17, QByteArray::fromHex("0010")},
                                                         {20, QByteArray::fromHex("00af")},
                                                         {19, QByteArray::fromHex("0020")},
                                                         {0, {}}}));
            descriptor.setPoses(poses);
            const auto ordered = tracePoseAudio(source, art.withDescriptor(descriptor), 0, 0);
            QCOMPARE(ordered.cues.size(), 4);
            QCOMPARE(ordered.cues[0].cue, 0x9f);
            QCOMPARE(ordered.cues[1].cue, 0x10);
            QCOMPARE(ordered.cues[2].cue, 0xaf);
            QCOMPARE(ordered.cues[3].cue, 0x20);
            for (const auto &cue : ordered.cues)
                QCOMPARE(cue.tick, 0);
            poses.setProgram(
                0, PoseBank::encodeProgram({{17, QByteArray::fromHex("0010")},
                                            {1, QByteArray(1, token) + QByteArray(1, char(4))},
                                            {4, QByteArray(1, 0)}}));
            descriptor.setPoses(poses);
            const auto loop = tracePoseAudio(source, art.withDescriptor(descriptor), 0, 0);
            QCOMPARE(loop.cues.size(), 1);
            QVERIFY(!loop.boundary.isEmpty());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void effectEditor() {
        try {
            auto project = Project::fromRom(rom());
            project.loadAttacks();
            project.bodyAnimationData();
            const auto before = project.manifest();
            AttackEffectEditor dialog(&project, 25, 2);
            dialog.show();
            auto table = dialog.findChild<QTreeWidget *>("audioCueTable");
            QVERIFY(table);
            QTRY_VERIFY_WITH_TIMEOUT(table->topLevelItemCount() > 1, 45000);
            QVERIFY(table->topLevelItem(0)->text(1).contains("Gaze of Terror"));
            const int tick = table->topLevelItem(1)->data(0,Qt::UserRole+2).toInt();
            QTest::mouseClick(table->viewport(), Qt::LeftButton, {},
                              table->visualItemRect(table->topLevelItem(1)).center());
            QVERIFY(dialog.findChild<QLabel *>("audioCuePosition")
                        ->text()
                        .contains(QString("tick %1").arg(tick)));
            QCOMPARE(dialog.findChild<QPushButton *>("effectPlay")->text(), QString("Play"));
            QCOMPARE(project.manifest(), before);
            dialog.findChild<QPushButton *>("effectPreviewEnemy")->click();
            QTRY_VERIFY_WITH_TIMEOUT(table->topLevelItemCount() > 1, 45000);
            QVERIFY(table->topLevelItem(0)->text(2).contains("304"));
            dialog.findChild<QSlider *>("audioVolume")->setValue(0);
            QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("audioPlay")->isEnabled(),10000);
            dialog.findChild<QPushButton *>("audioPlay")->click();
            auto playback = dialog.findChild<QLabel *>("audioPlaybackStatus");
            QTRY_VERIFY_WITH_TIMEOUT(
                playback->text().startsWith("Playing") || playback->text() == "Finished", 10000);
            dialog.findChild<QPushButton *>("audioStop")->click();
            QCOMPARE(playback->text(), QString("Stopped"));
            dialog.findChild<QPushButton *>("audioPlay")->click();
            dialog.findChild<QPushButton *>("audioStop")->click();
            QTest::qWait(150);
            QCOMPARE(playback->text(), QString("Stopped"));
            QCOMPARE(project.manifest(), before);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void classEditor() {
        try {
            const auto source = rom();
            ActorPoseEditor editor(source);
            editor.selectPrivateSequence(5, 0, 0);
            editor.show();
            auto button = editor.findChild<QPushButton *>("actorSoundCues");
            QVERIFY(button && button->isVisible());
            bool opened = false;
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                opened = dialog->findChild<QTreeWidget *>("audioCueTable") &&
                         dialog->findChild<QLabel *>("audioCueSummary");
                dialog->reject();
            });
            button->click();
            QVERIFY(opened);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void soundRendering() {
        try {
            const auto source = rom();
            const auto &bytes = source.bytes();
            QJsonArray hashes;
            for (int request : {303, 304, 305, 306, 280, 281, 11}) {
                QProcess proof;
                proof.start("node",
                            {QString::fromUtf8(LORDLY_SOURCE_DIR) + "/tests/sound-preview-mips.cjs",
                             qEnvironmentVariable("LORDLY_TEST_ROM"), QString::number(request)});
                QVERIFY(proof.waitForFinished(30000));
                QVERIFY2(proof.exitCode() == 0, proof.readAllStandardError().constData());
                const auto expected =
                    QJsonDocument::fromJson(proof.readAllStandardOutput()).object();
                const auto sound = renderSoundPreview(source, request);
                QCOMPARE(sound.ticks, expected["ticks"].toInt());
                QCOMPARE(sound.voices, expected["voices"].toInt());
                QCOMPARE(sound.limited, expected["limited"].toBool());
                QCOMPARE(sound.pcm.size(), sound.ticks * 32000 / 60 * 4);
                QVERIFY(
                    std::any_of(sound.pcm.begin(), sound.pcm.end(), [](char c) { return c != 0; }));
                hashes = expected["hashes"].toArray();
            }
            QCOMPARE(hashes.size(), 133);
            for (int i = 0; i < hashes.size(); ++i) {
                const quint32 base = 0x423ff0, table = base + u32(bytes, base + 0x2c),
                              wave = base + u32(bytes, table + i * 4);
                const auto start = 0x429cd0 + u32(bytes, wave), length = u32(bytes, wave + 4),
                           book = base + u32(bytes, wave + 16);
                const int order = u32(bytes, book), count = u32(bytes, book + 4);
                QVector<vadpcm_vector> coefficients(order * count);
                vadpcm_vector state{};
                for (int k = 0; k < coefficients.size(); ++k)
                    for (int j = 0; j < 8; ++j)
                        coefficients[k].v[j] = qint16(u16(bytes, book + 8 + (k * 8 + j) * 2));
                QVector<qint16> decoded(length / 9 * 16);
                QCOMPARE(vadpcm_decode(count, order, coefficients.constData(), &state, length / 9,
                                       decoded.data(), bytes.constData() + start),
                         kVADPCMErrNone);
                QByteArray pcm(decoded.size() * 2, 0);
                for (int j = 0; j < decoded.size(); ++j)
                    qToLittleEndian(decoded[j], pcm.data() + j * 2);
                QCOMPARE(QString::fromLatin1(
                             QCryptographicHash::hash(pcm, QCryptographicHash::Sha256).toHex()),
                         hashes[i].toString());
            }
            QVERIFY_THROWS_EXCEPTION(Error, renderSoundPreview(source, -1));
            QVERIFY_THROWS_EXCEPTION(Error, renderSoundPreview(source, 740));
            auto cancel = std::make_shared<std::atomic_bool>(true);
            QVERIFY(renderSoundPreview(source, 303, cancel).pcm.isEmpty());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void soundBankCoverage() {
        const auto source = rom();
        QStringList failures;
        int loops = 0;
        for (int request = 0; request < 740; ++request) {
            try {
                const auto sound = renderSoundPreview(source, request);
                if (sound.pcm.size() != sound.ticks * 32000 / 60 * 4 || sound.ticks < 1 ||
                    sound.ticks > 900)
                    failures << QString("Request %1: invalid output length").arg(request);
                loops += sound.limited;
            } catch (const std::exception &e) {
                failures << QString("Request %1: %2").arg(request).arg(e.what());
            }
        }
        qInfo() << "740 public sound effects; bounded previews" << loops;
        QVERIFY2(failures.isEmpty(), failures.join('\n').toUtf8().constData());
    }
};
QTEST_MAIN(AudioCuesTests)
#include "audio_cues_tests.moc"
