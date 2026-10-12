#include "engine/archive.h"
#include "engine/project.h"
#include "engine/squad_members.h"
#include <QJsonArray>
#include "engine/resource.h"
#include "engine/tool_artwork.h"
#include "rebuild/compiler.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class ToolSettingsTests : public QObject {
    Q_OBJECT
  private slots:
    void alignmentCapAndRestHealing() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"), tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if(path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
            const auto rom=Rom::open(path);
            QTemporaryDir work, output(QFileInfo(path).absolutePath()+"/Test ROMs/native-alignment-rest-XXXXXX");
            QVERIFY(output.isValid());output.setAutoRemove(false);
            for(const auto values:{qMakePair(0,0),qMakePair(2,5),qMakePair(5,1),qMakePair(10,999),qMakePair(100,5)}) {
                auto project=Project::fromRom(rom);project.loadTools();const auto defaults=project.tools()->json();
                auto settings=defaults, rules=settings["gameRules"].toObject();
                rules["alignmentCap"]=values.first;rules["restHealing"]=values.second;
                settings["gameRules"]=rules;project.setTools(settings);
                project.saveFolder(work.filePath("project"));auto restored=Project::fromRom(rom);
                restored.applyManifest(Project::readProject(work.filePath("project")));
                restored.loadTools();
                QCOMPARE(restored.tools()->json(),settings);
                for(const auto key:{"alignmentCap","restHealing"}) {
                    auto invalid=settings, bad=rules;bad[key]=QString(key)=="alignmentCap"?101:1000;invalid["gameRules"]=bad;
                    QVERIFY_THROWS_EXCEPTION(Error,project.setTools(invalid));QCOMPARE(project.tools()->json(),settings);
                }
                const auto built=rebuild(project,tools,work.path());
                const auto folder=output.filePath(QString::number(values.first));QVERIFY(QDir().mkpath(folder));
                const auto candidate=folder+"/"+QFileInfo(path).fileName();exportRom(project,built,candidate);
                auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadTools();QCOMPARE(reopened.tools()->json(),settings);
                QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);
                auto unchanged=built.image;
                for(const auto &owner:project.tools()->gameRules().builds()) unchanged.replace(owner.offset,owner.size,rom.bytes().mid(owner.offset,owner.size));
                Rom::updateChecksum(unchanged);QCOMPARE(unchanged,rom.bytes());
                QProcess execution;execution.start("node",{QFINDTESTDATA("alignment-rest-mips.cjs"),candidate,path,QString::number(values.first),QString::number(values.second)});
                QVERIFY(execution.waitForFinished(60000));QVERIFY2(execution.exitCode()==0,execution.readAllStandardError().constData());
                qInfo().noquote()<<execution.readAllStandardOutput();
                // Old project files preserve new settings already loaded from an edited ROM.
                auto legacy=settings, oldRules=rules;oldRules.remove("alignmentCap");oldRules.remove("restHealing");legacy["gameRules"]=oldRules;
                reopened.setTools(legacy);QCOMPARE(reopened.tools()->json(),settings);
                reopened.setTools(defaults);QCOMPARE(rebuild(reopened,tools,work.path()).image,rom.bytes());
            }
            for(const auto part:{qMakePair(QString("alignmentCap"),0x23336e),qMakePair(QString("restHealing"),0x133bee)}) {
                auto foreign=rom.bytes();foreign[part.second]^=1;
                auto unknown=Project::fromRom(Rom::fromBytes(foreign));unknown.loadTools();
                QVERIFY(!unknown.tools()->gameRules().unavailable(part.first).isEmpty());
                QCOMPARE(rebuild(unknown,tools,work.path()).image,foreign);
                auto settings=unknown.tools()->json(), rules=settings["gameRules"].toObject();rules[part.first]=2;settings["gameRules"]=rules;
                QVERIFY_THROWS_EXCEPTION(Error,unknown.setTools(settings));
            }
            qInfo().noquote()<<"Alignment/rest candidates:"<<output.path();
        } catch(const std::exception &e) {QFAIL(e.what());}
    }
    void deploymentLimitsExportAndExecution() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if(path.isEmpty()||tools.isEmpty())QSKIP("Private ROM and compiler required");
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadTools();const auto baseline=project.tools()->json();
            QTemporaryDir work,output(QFileInfo(path).absolutePath()+"/Test ROMs/native-deployment-XXXXXX");QVERIFY(output.isValid());output.setAutoRemove(false);
            for(int limit:{1,10,30}) {
                auto settings=baseline;auto values=settings["gameRules"].toObject();values["deploymentLimit"]=limit;settings["gameRules"]=values;project.setTools(settings);
                const auto built=rebuild(project,tools,work.path());const auto folder=output.filePath(QString::number(limit));QDir().mkpath(folder);
                const auto candidate=QDir(folder).filePath(QFileInfo(path).fileName());exportRom(project,built,candidate);
                auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadTools();QCOMPARE(reopened.tools()->json(),settings);
                auto preserved=built.image;for(const auto &owner:project.tools()->gameRules().builds())preserved.replace(owner.offset,owner.size,rom.bytes().mid(owner.offset,owner.size));
                Rom::updateChecksum(preserved);QCOMPARE(preserved,rom.bytes());
                QProcess execution;execution.start("node",{QFINDTESTDATA("deployment-mips.cjs"),candidate,QString::number(limit)});
                QVERIFY(execution.waitForFinished(60000));QVERIFY2(execution.exitCode()==0,execution.readAllStandardError().constData());qInfo().noquote()<<execution.readAllStandardOutput();
                reopened.setTools(baseline);QCOMPARE(rebuild(reopened,tools,work.path()).image,rom.bytes());
            }
            qInfo().noquote()<<"Deployment candidates:"<<output.path();
        } catch(const Error &e){QFAIL(e.what());}
    }
    void reserveExperienceExportAndExecution() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if(path.isEmpty()||tools.isEmpty())QSKIP("Private ROM and compiler required");
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadTools();const auto baseline=project.tools()->json();
            QTemporaryDir work,output(QFileInfo(path).absolutePath()+"/Test ROMs/native-reserve-xp-XXXXXX");QVERIFY(output.isValid());output.setAutoRemove(false);
            for(int amount:{25,100}) {
                auto settings=baseline;auto rules=settings["gameRules"].toObject();rules["reserveXp"]=amount;settings["gameRules"]=rules;project.setTools(settings);
                const auto built=rebuild(project,tools,work.path());const auto folder=output.filePath(QString::number(amount));QDir().mkpath(folder);
                const auto candidate=QDir(folder).filePath(QFileInfo(path).fileName());exportRom(project,built,candidate);
                auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadTools();QCOMPARE(reopened.tools()->json(),settings);
                auto preserved=built.image;for(const auto &owner:project.tools()->gameRules().builds())preserved.replace(owner.offset,owner.size,rom.bytes().mid(owner.offset,owner.size));
                Rom::updateChecksum(preserved);QCOMPARE(preserved,rom.bytes());
                QProcess execution;execution.start("node",{QFINDTESTDATA("reserve-xp-mips.cjs"),candidate,QString::number(amount)});
                QVERIFY(execution.waitForFinished(60000));QVERIFY2(execution.exitCode()==0,execution.readAllStandardError().constData());qInfo().noquote()<<execution.readAllStandardOutput();
                auto damaged=built.image;damaged[0x27c6060]=char(u8(damaged,0x27c6060)^1);
                auto unknown=Project::fromRom(Rom::fromBytes(damaged));unknown.loadTools();
                auto unsupported=unknown.tools()->json();auto invalidRules=unsupported["gameRules"].toObject();
                invalidRules["gothLowDivisor"]=12;unsupported["gameRules"]=invalidRules;
                QVERIFY_THROWS_EXCEPTION(Error,unknown.setTools(unsupported));
                // Reserve on/off composes with independent Goth formula edits.
                auto other=settings;auto changed=other["gameRules"].toObject();changed["gothLowDivisor"]=10;other["gameRules"]=changed;reopened.setTools(other);
                const auto combined=rebuild(reopened,tools,work.path());auto next=Project::fromRom(Rom::fromBytes(combined.image));next.loadTools();QCOMPARE(next.tools()->json(),other);
                next.setTools(baseline);QCOMPARE(rebuild(next,tools,work.path()).image,rom.bytes());
            }
            // Foreign cartridge data in the module allocation must never be overwritten.
            auto foreign=rom.bytes();foreign[0x27c6000]=0;auto protectedProject=Project::fromRom(Rom::fromBytes(foreign));protectedProject.loadTools();
            auto setting=protectedProject.tools()->json();auto values=setting["gameRules"].toObject();values["reserveXp"]=25;setting["gameRules"]=values;
            QVERIFY_THROWS_EXCEPTION(Error,protectedProject.setTools(setting));
            qInfo().noquote()<<"Reserve XP candidates:"<<output.path();
        } catch(const Error &e){QFAIL(e.what());}
    }
    void trainingExportAndExecution() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if(path.isEmpty()||tools.isEmpty())QSKIP("Private ROM and compiler required");
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadTraining();project.loadScenarios();
            const auto original=project.training()->json();auto value=original;auto rows=value["opponents"].toArray();
            int selected=-1;for(int i=0;i<rows.size();++i)if(rows[i].toObject()["node"]==7&&rows[i].toObject()["slot"]==0)selected=i;
            QVERIFY(selected>=0);auto row=rows[selected].toObject();row["price"]=0;
            QVector<CustomMember> members{{2,-2,8,{20,30}},{3,-1,0,{21,31}},{5,0,2,{22,32}},{6,1,4,{23,33}},{7,2,6,{24,34}}};
            const auto squad=editSquadMembers(project.training()->entries()[selected].squad,members);QVERIFY(independentSquad(squad));
            row["squad"]=QString::fromLatin1(squad.toHex());rows[selected]=row;value["opponents"]=rows;project.setTraining(value);
            QCOMPARE(Training::fee(20,15),600);QCOMPARE(Training::fee(21,15),615);QCOMPARE(Training::fee(40,15),900);
            QCOMPARE(Training::fee(41,15),945);QCOMPARE(Training::fee(99,255),60435);QCOMPARE(Training::fee(99,0),0);
            QTemporaryDir work;project.saveFolder(work.filePath("project"));auto saved=Project::fromRom(rom);
            saved.applyManifest(Project::readProject(work.filePath("project")));QCOMPARE(saved.training()->json(),value);
            // Simultaneous scenario edit must survive appending private training squads.
            auto scenario=project.scenarios()->scenario(1);project.loadScenarios();auto scenarios=*project.scenarios();
            scenarios.setStartTime(1,22,30,project.classData());project.setScenarios(scenarios.json());
            project.loadTools();auto settings=project.tools()->json();auto rules=settings["gameRules"].toObject();rules["trainingBonus"]=20;settings["gameRules"]=rules;project.setTools(settings);
            const auto built=rebuild(project,tools,work.path());
            QTemporaryDir output(QFileInfo(path).absolutePath()+"/Test ROMs/native-training-XXXXXX");QVERIFY(output.isValid());output.setAutoRemove(false);
            const auto candidate=output.filePath(QFileInfo(path).fileName());exportRom(project,built,candidate);
            auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadTraining();reopened.loadScenarios();QCOMPARE(reopened.training()->json(),value);
            reopened.loadTools();QCOMPARE(reopened.tools()->gameRules().value("trainingBonus"),20);
            QCOMPARE(reopened.scenarios()->scenario(1).layout.startHour(),22);QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);
            const auto oldEnemies=ScenarioResources::load(rom).enemyData(),newEnemies=ScenarioResources::load(reopened.rom()).enemyData();
            QCOMPARE(newEnemies.first(oldEnemies.size()),oldEnemies);
            for(int node=0;node<48;++node){const int at=Training::tableOffset+node*24;QCOMPARE(built.image.mid(at+20,4),rom.bytes().mid(at+20,4));
                for(int slot=0;slot<5;++slot)QCOMPARE(u8(built.image,at+slot*4+3),u8(rom.bytes(),at+slot*4+3));}
            QProcess execution;execution.start("node",{QFINDTESTDATA("training-mips.cjs"),candidate,path});
            QVERIFY(execution.waitForFinished(60000));QVERIFY2(execution.exitCode()==0,execution.readAllStandardError().constData());qInfo().noquote()<<execution.readAllStandardOutput();
            qInfo().noquote()<<"Training candidate:"<<candidate;
        } catch(const Error &e){QFAIL(e.what());}
    }
    void completionRulesExportAndExecution() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if(path.isEmpty()||tools.isEmpty())QSKIP("Private ROM and compiler required");
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadTools();
            const auto baseline=project.tools()->json();QTemporaryDir work;
            QTemporaryDir output(QFileInfo(path).absolutePath()+"/Test ROMs/native-completion-rules-XXXXXX");
            QVERIFY(output.isValid());output.setAutoRemove(false);
            for(int populationScale:{200,0}) {
                auto settings=baseline;auto rules=settings["gameRules"].toObject();
                rules["gothLowDivisor"]=10;rules["gothHighDivisor"]=100;
                rules["soldierPopulationScale"]=populationScale;rules["soldierMinimum"]=populationScale?3:0;
                settings["gameRules"]=rules;project.setTools(settings);
                const auto built=rebuild(project,tools,work.path());
                const auto folder=output.filePath(QString::number(populationScale));QDir().mkpath(folder);
                const auto candidate=QDir(folder).filePath(QFileInfo(path).fileName());exportRom(project,built,candidate);
                auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadTools();QCOMPARE(reopened.tools()->json(),settings);
                auto untouched=built.image;
                for(const auto &owner:project.tools()->gameRules().builds())
                    untouched.replace(owner.offset,owner.size,rom.bytes().mid(owner.offset,owner.size));
                Rom::updateChecksum(untouched);QCOMPARE(untouched,rom.bytes());
                QProcess execution;execution.start("node",{QFINDTESTDATA("completion-rules-mips.cjs"),candidate,path,QString::number(populationScale)});
                QVERIFY(execution.waitForFinished(60000));QVERIFY2(execution.exitCode()==0,execution.readAllStandardError().constData());
                qInfo().noquote()<<execution.readAllStandardOutput();
                reopened.setTools(baseline);QCOMPARE(rebuild(reopened,tools,work.path()).image,rom.bytes());
            }
            qInfo().noquote()<<"Completion candidates:"<<output.path();
        }catch(const Error &e){QFAIL(e.what());}
    }
    void elementRulesExportAndExecution() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if(path.isEmpty()||tools.isEmpty())QSKIP("Private ROM and compiler required");
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadTools();
            const auto baseline=project.tools()->json();QTemporaryDir work;
            QTemporaryDir output(QFileInfo(path).absolutePath()+"/Test ROMs/native-element-rules-XXXXXX");
            QVERIFY(output.isValid());output.setAutoRemove(false);
            for(int strength:{0,175,400}) {
                auto settings=project.tools()->json();auto rules=settings["gameRules"].toObject();
                rules["elementAffinity"]=9;rules["weaponAffinity"]=3;rules["resistanceStrength"]=strength;
                settings["gameRules"]=rules;project.setTools(settings);
                const auto built=rebuild(project,tools,work.path());
                const auto folder=output.filePath(QString::number(strength));QDir().mkpath(folder);
                const auto candidate=QDir(folder).filePath(QFileInfo(path).fileName());exportRom(project,built,candidate);
                auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadTools();QCOMPARE(reopened.tools()->json(),settings);
                auto untouched=built.image;
                for(const auto &owner:project.tools()->gameRules().builds())
                    untouched.replace(owner.offset,owner.size,rom.bytes().mid(owner.offset,owner.size));
                Rom::updateChecksum(untouched);QCOMPARE(untouched,rom.bytes());
                QProcess execution;execution.start("node",{QFINDTESTDATA("element-rules-mips.cjs"),candidate,QString::number(strength)});
                QVERIFY(execution.waitForFinished(60000));QVERIFY2(execution.exitCode()==0,execution.readAllStandardError().constData());
                qInfo().noquote()<<execution.readAllStandardOutput();
                reopened.setTools(baseline);QCOMPARE(rebuild(reopened,tools,work.path()).image,rom.bytes());
            }
            // Earlier gameRules objects need not contain later element settings.
            auto earlier=baseline;auto prior=baseline["gameRules"].toObject();
            prior.remove("elementAffinity");prior.remove("weaponAffinity");prior.remove("resistanceStrength");
            earlier["gameRules"]=prior;project.setTools(earlier);
            QCOMPARE(project.tools()->gameRules().value("resistanceStrength"),400);
            qInfo().noquote()<<"Element candidates:"<<output.path();
        }catch(const Error &e){QFAIL(e.what());}
    }
    void gameRulesExportAndExecution() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"), tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if(path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
            const auto rom=Rom::open(path); auto project=Project::fromRom(rom); project.loadTools();
            for(const auto &rule:GameRules::definitions()) {
                QVERIFY2(project.tools()->gameRules().unavailable(rule.group).isEmpty(),qPrintable(project.tools()->gameRules().unavailable(rule.group)));
                QCOMPARE(project.tools()->gameRules().value(rule.key),rule.retail);
            }
            const auto retail=project.tools()->json();
            auto settings=retail; auto rules=settings["gameRules"].toObject();
            rules["dayLength"]=150;rules["nightLength"]=50;rules["fatigueRate"]=200;rules["recoveryRate"]=50;
            rules["tacticAuto"]=-5;rules["tacticStrong"]=2;rules["tacticLeader"]=5;rules["tacticWeak"]=-2;
            settings["gameRules"]=rules;project.setTools(settings);
            auto invalid=settings;auto bad=rules;bad["dayLength"]=112;invalid["gameRules"]=bad;
            QVERIFY_THROWS_EXCEPTION(Error,project.setTools(invalid));QCOMPARE(project.tools()->json(),settings);
            QTemporaryDir work; project.saveFolder(work.filePath("project"));
            auto restored=Project::fromRom(rom);restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.tools()->json(),settings);
            const auto built=rebuild(project,tools,work.path());
            QTemporaryDir output(QFileInfo(path).absolutePath()+"/Test ROMs/native-game-rules-XXXXXX");
            QVERIFY(output.isValid());output.setAutoRemove(false);
            const auto candidate=output.filePath(QFileInfo(path).fileName());exportRom(project,built,candidate);
            auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadTools();
            QCOMPARE(reopened.tools()->json(),settings);QVERIFY(!reopened.tools()->modified());
            QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);
            auto untouched=built.image;
            for(const auto &owner:project.tools()->gameRules().builds())
                untouched.replace(owner.offset,owner.size,rom.bytes().mid(owner.offset,owner.size));
            Rom::updateChecksum(untouched);QCOMPARE(untouched,rom.bytes());
            QProcess execute;
            execute.start("node",{QFINDTESTDATA("game-rules-mips.cjs"),candidate,path});
            QVERIFY(execute.waitForFinished(60000));
            QVERIFY2(execute.exitCode()==0,execute.readAllStandardError().constData());
            qInfo().noquote()<<execute.readAllStandardOutput();
            reopened.setTools(retail);
            QCOMPARE(rebuild(reopened,tools,work.path()).image,rom.bytes());
            auto foreign=rom.bytes();foreign[0x133944]^=1;
            auto unknown=Project::fromRom(Rom::fromBytes(foreign));unknown.loadTools();
            QVERIFY(!unknown.tools()->gameRules().unavailable("fatigue").isEmpty());
            QCOMPARE(rebuild(unknown,tools,work.path()).image,foreign);
            QVERIFY_THROWS_EXCEPTION(Error,unknown.setTools(settings));
            qInfo().noquote()<<"Game rules candidate:"<<candidate;
        } catch(const Error &e) {QFAIL(e.what());}
    }
    void highAttackProjectsAndExports() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-high-attack-XXXXXX");
            QVERIFY(output.isValid());
            auto save = [&](const Project &project, const BuildResult &build, const QString &name) {
                const auto folder = output.filePath(name);
                QDir().mkpath(folder);
                const auto candidate = QDir(folder).filePath(QFileInfo(path).fileName());
                exportRom(project, build, candidate);
                return Rom::open(candidate);
            };
            auto project = Project::fromRom(rom);
            project.loadTools();
            QVERIFY(project.tools()->highAttack().available());
            QVERIFY(!project.tools()->highAttack().enabled());
            QCOMPARE(rebuild(project, tools, work.path()).image, rom.bytes());
            auto settings = project.tools()->json();
            settings["highAttackStreamSplit"] = true;
            project.setTools(settings);
            project.saveFolder(work.filePath("project"));
            project.saveArchive(work.filePath("project.lcproject"));
            for (const auto &location :
                 {work.filePath("project"), work.filePath("project.lcproject")}) {
                auto restored = Project::fromRom(rom);
                restored.applyManifest(Project::readProject(location));
                QCOMPARE(restored.tools()->json(), settings);
            }
            // Old native schemas must not reset a newer feature.
            for (int version : {1, 2, 3}) {
                auto old = settings;
                old["version"] = version;
                old.remove("highAttackStreamSplit");
                if (version < 3) {
                    old.remove("characterCardLuck");
                    old.remove("squadMenuAlignment");
                }
                if (version < 2)
                    old.remove("chaosFrameCounter");
                project.setTools(old);
                QCOMPARE(project.tools()->json(), settings);
            }
            auto invalid = settings;
            invalid["highAttackStreamSplit"] = 1;
            invalid["experienceScalePercent"] = 75;
            QVERIFY_THROWS_EXCEPTION(Error, project.setTools(invalid));
            QCOMPARE(project.tools()->json(), settings);
            const QJsonObject patch{
                {"format", "ob64-patch"},
                {"version", 36},
                {"patches",
                 QJsonObject{{"tools", QJsonObject{{"high-attack-streamsplit", true}}}}}};
            auto imported = Project::fromRom(rom);
            imported.importLegacy(patch);
            QVERIFY(imported.tools()->highAttack().enabled());
            QVERIFY(imported.pendingLegacySections().isEmpty());
            auto off = settings;
            off["highAttackStreamSplit"] = false;
            imported.setTools(off);
            auto reset = Project::fromRom(rom);
            reset.applyManifest(imported.manifest());
            QVERIFY(!reset.tools()->highAttack().enabled());
            auto owner = patch;
            owner["patches"] = QJsonObject{};
            owner["rebuild_owners"] = QJsonObject{{"schema", 1}, {"high_attack_streamsplit", true}};
            imported.importLegacy(owner);
            QVERIFY(imported.tools()->highAttack().enabled());
            QVERIFY(imported.pendingLegacySections().isEmpty());
            auto conflict = patch;
            conflict["rebuild_owners"] = QJsonObject{{"high_attack_streamsplit", false}};
            const auto before = imported.manifest();
            QVERIFY_THROWS_EXCEPTION(Error, imported.importLegacy(conflict));
            QCOMPARE(imported.manifest(), before);
            const auto built = rebuild(project, tools, work.path());
            auto reopened = Project::fromRom(save(project, built, "enabled"));
            reopened.loadTools();
            QCOMPARE(reopened.tools()->json(), settings);
            QCOMPARE(reopened.tools()->highAttack().installation(),
                     HighAttack::Installation::Native);
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            QJsonArray execution;
            auto untouched = built.image;
            const auto sites = HighAttack::definition()["sites"].toArray();
            for (const auto &value : sites) {
                const auto site = value.toObject();
                const int at = site["rom"].toInt(), size = site["size"].toInt();
                execution.append(QJsonObject{
                    {"name", site["name"]},
                    {"ram", site["ram"]},
                    {"dataHex", QString::fromLatin1(built.image.mid(at, size).toHex())}});
                untouched.replace(at, size, rom.bytes().mid(at, size));
                // Each foreign owner is independently recognized and preserved.
                auto foreign = rom.bytes();
                foreign[at] ^= 1;
                auto held = Project::fromRom(Rom::fromBytes(foreign));
                held.loadTools();
                QVERIFY(!held.tools()->highAttack().available());
                QVERIFY_THROWS_EXCEPTION(Error, held.setTools(settings));
                QCOMPARE(rebuild(held, tools, work.path()).image, foreign);
            }
            Rom::updateChecksum(untouched);
            QCOMPARE(untouched, rom.bytes());
            atomicWrite(work.filePath("execution.json"), QJsonDocument(execution).toJson());
            QProcess runner;
            runner.start("node", {QFINDTESTDATA("high-attack-execution.cjs"),
                                  work.filePath("execution.json")});
            QVERIFY(runner.waitForFinished(60000));
            QVERIFY2(runner.exitCode() == 0, runner.readAllStandardError().constData());
            qInfo().noquote() << runner.readAllStandardOutput();
            reopened.setTools(off);
            QCOMPARE(save(reopened, rebuild(reopened, tools, work.path()), "disabled").bytes(),
                     rom.bytes());
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("high-attack-browser-reference.cjs"),
                                     work.filePath("legacy.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("legacy.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            auto legacy = rom.bytes();
            for (const auto &value : QJsonDocument::fromJson(file.readAll()).array()) {
                const auto row = value.toObject();
                const auto bytes = QByteArray::fromHex(row["patched"].toString().toLatin1());
                legacy.replace(row["offset"].toInt(), bytes.size(), bytes);
            }
            auto migrated = Project::fromRom(Rom::fromBytes(legacy));
            migrated.loadTools();
            QCOMPARE(migrated.tools()->highAttack().installation(),
                     HighAttack::Installation::Legacy);
            QCOMPARE(save(migrated, rebuild(migrated, tools, work.path()), "migrated").bytes(),
                     built.image);
            migrated.setTools(off);
            QCOMPARE(
                save(migrated, rebuild(migrated, tools, work.path()), "legacy-disabled").bytes(),
                rom.bytes());
            // Existing unrelated Rev1 continuation reservation is not owned by Rev0.
            auto slot2 = rom.bytes();
            slot2[0x306cc] = 42;
            auto shared = Project::fromRom(Rom::fromBytes(slot2));
            shared.loadTools();
            shared.setTools(settings);
            const auto sharedBuild = rebuild(shared, tools, work.path());
            QCOMPARE(quint8(sharedBuild.image[0x306cc]), quint8(42));
            auto combined = Project::fromRom(rom);
            combined.loadTools();
            settings["characterCardLuck"] = settings["squadMenuAlignment"] =
                settings["chaosFrameCounter"] = true;
            settings["experienceScalePercent"] = 75;
            combined.setTools(settings);
            combined.appendClass(1, "Stream Knight");
            const auto joined = rebuild(combined, tools, work.path());
            auto combinedOpen = Project::fromRom(save(combined, joined, "combined"));
            combinedOpen.loadTools();
            QCOMPARE(combinedOpen.tools()->json(), settings);
            QCOMPARE(combinedOpen.records(Table::Classes).size(), 166);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "High Attack candidates:" << output.path();
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void displayProjectsAndExports() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("display-browser-reference.cjs"),
                                     work.filePath("reference.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("reference.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto cases = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
            QCOMPARE(cases.size(), 2);
            const QStringList keys{"characterCardLuck", "squadMenuAlignment"};
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-displays-XXXXXX");
            QVERIFY(output.isValid());
            auto save = [&](const Project &project, const BuildResult &build, const QString &name) {
                const auto folder = output.filePath(name);
                QDir().mkpath(folder);
                const auto candidate = QDir(folder).filePath(QFileInfo(path).fileName());
                exportRom(project, build, candidate);
                return Rom::open(candidate);
            };
            for (int mask : {1, 2, 3}) {
                auto project = Project::fromRom(rom);
                project.loadTools();
                auto settings = project.tools()->json();
                for (int i = 0; i < 2; ++i)
                    settings[keys[i]] = bool(mask & (1 << i));
                project.setTools(settings);
                project.saveFolder(work.filePath("project"));
                project.saveArchive(work.filePath("project.lcproject"));
                for (const auto &location :
                     {work.filePath("project"), work.filePath("project.lcproject")}) {
                    auto restored = Project::fromRom(rom);
                    restored.applyManifest(Project::readProject(location));
                    QCOMPARE(restored.tools()->json(), settings);
                }
                auto expected = rom.bytes();
                for (int i = 0; i < 2; ++i)
                    if (mask & (1 << i))
                        for (const auto &v : cases[i].toObject()["components"].toArray()) {
                            const auto row = v.toObject();
                            const auto bytes =
                                QByteArray::fromHex(row["dataHex"].toString().toLatin1());
                            expected.replace(row["romStart"].toInt(), bytes.size(), bytes);
                        }
                Rom::updateChecksum(expected);
                const auto built = rebuild(project, tools, work.path());
                QCOMPARE(built.image, expected);
                auto reopened = Project::fromRom(save(project, built, QString::number(mask)));
                reopened.loadTools();
                QCOMPARE(reopened.tools()->json(), settings);
                QCOMPARE(rebuild(reopened, tools, work.path()).image, expected);
                settings["characterCardLuck"] = false;
                settings["squadMenuAlignment"] = false;
                reopened.setTools(settings);
                const auto disabled = rebuild(reopened, tools, work.path());
                QCOMPARE(disabled.image, rom.bytes());
                QCOMPARE(save(reopened, disabled, QString("%1-disabled").arg(mask)).bytes(),
                         rom.bytes());
            }
            for (int i = 0; i < 2; ++i) {
                const auto kind = i ? ToolLabel::Alignment : ToolLabel::Luck;
                const auto definition = DisplayTool::definition(kind);
                const auto id = definition["id"].toString();
                const QString ownerKey = i ? "squad_menu_alignment" : "character_card_luck";
                const QJsonObject patch{
                    {"format", "ob64-patch"},
                    {"version", 36},
                    {"patches", QJsonObject{{"tools", QJsonObject{{id, true}}}}}};
                auto imported = Project::fromRom(rom);
                imported.importLegacy(patch);
                QVERIFY(imported.tools()->displayTool(kind).enabled());
                QVERIFY(imported.pendingLegacySections().isEmpty());
                auto reset = imported.tools()->json();
                reset[keys[i]] = false;
                imported.setTools(reset);
                auto reopened = Project::fromRom(rom);
                reopened.applyManifest(imported.manifest());
                QVERIFY(!reopened.tools()->displayTool(kind).enabled());
                auto owner = patch;
                owner["patches"] = QJsonObject{};
                owner["rebuild_owners"] = QJsonObject{{"schema", 1}, {ownerKey, true}};
                imported.importLegacy(owner);
                QVERIFY(imported.tools()->displayTool(kind).enabled());
                QVERIFY(imported.pendingLegacySections().isEmpty());
                auto bad = imported.tools()->json();
                bad[keys[i]] = 1;
                const auto before = imported.manifest();
                QVERIFY_THROWS_EXCEPTION(Error, imported.setTools(bad));
                auto conflict = patch;
                conflict["rebuild_owners"] = QJsonObject{{ownerKey, false}};
                QVERIFY_THROWS_EXCEPTION(Error, imported.importLegacy(conflict));
                QCOMPARE(imported.manifest(), before);
                auto legacy = rom.bytes();
                for (const auto &v : cases[i].toObject()["legacy"].toArray()) {
                    const auto row = v.toObject();
                    const auto bytes = QByteArray::fromHex(row["patched"].toString().toLatin1());
                    legacy.replace(row["offset"].toInt(), bytes.size(), bytes);
                }
                Rom::updateChecksum(legacy);
                auto project = Project::fromRom(Rom::fromBytes(legacy));
                project.loadTools();
                QCOMPARE(project.tools()->displayTool(kind).installation(),
                         DisplayTool::Installation::Legacy);
                const auto migrated = rebuild(project, tools, work.path());
                QVERIFY(
                    DisplayTool::load(save(project, migrated, id + "-migrated"), kind).enabled());
                for (const auto &v : definition["sites"].toArray()) {
                    const auto site = v.toObject();
                    const int at = site["rom"].toInt();
                    auto foreign = rom.bytes();
                    foreign[at] ^= 1;
                    auto held = Project::fromRom(Rom::fromBytes(foreign));
                    held.loadTools();
                    QVERIFY(!held.tools()->displayTool(kind).available());
                    QCOMPARE(rebuild(held, tools, work.path()).image, foreign);
                    if (site["name"] != "source-module")
                        continue;
                    auto custom = migrated.image;
                    custom[at + 0x520] ^= 1;
                    auto customized = Project::fromRom(Rom::fromBytes(custom));
                    customized.loadTools();
                    QCOMPARE(rebuild(customized, tools, work.path()).image, custom);
                    auto occupied = legacy;
                    occupied[at] = 42;
                    auto removable = Project::fromRom(Rom::fromBytes(occupied));
                    removable.loadTools();
                    QVERIFY_THROWS_EXCEPTION(Error, rebuild(removable, tools, work.path()));
                    auto off = removable.tools()->json();
                    off[keys[i]] = false;
                    removable.setTools(off);
                    auto expected = rom.bytes();
                    expected[at] = 42;
                    QCOMPARE(rebuild(removable, tools, work.path()).image, expected);
                }
            }
            auto combined = Project::fromRom(rom);
            combined.loadTools();
            auto settings = combined.tools()->json();
            settings["characterCardLuck"] = settings["squadMenuAlignment"] =
                settings["chaosFrameCounter"] = true;
            settings["experienceScalePercent"] = 75;
            combined.setTools(settings);
            combined.appendClass(1, "Display Knight");
            const auto built = rebuild(combined, tools, work.path());
            auto reopened = Project::fromRom(save(combined, built, "combined"));
            reopened.loadTools();
            QCOMPARE(reopened.tools()->json(), settings);
            QCOMPARE(reopened.records(Table::Classes).size(), 166);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Luck/Alignment export candidates:" << output.path();
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void displayArtworkFromCurrentRom() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("display-browser-reference.cjs"),
                                     work.filePath("reference.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("reference.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto cases = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
            QCOMPARE(cases.size(), 2);
            const auto archive = LhaMember::find(rom, "suujifont2.k64");
            const auto font = archive.decode(rom);
            auto relocate = [&](const QByteArray &value) {
                auto bytes = rom.bytes();
                bytes.replace(archive.offset, archive.headerSize + archive.storedSize,
                              QByteArray(archive.headerSize + archive.storedSize, 0));
                bytes += archive.rebuild(rom, value);
                while (bytes.size() % 4)
                    bytes.append(char(0));
                return Rom::fromBytes(bytes);
            };
            auto edited = font;
            bool altered = false;
            for (int x = 0; x < 80 && !altered; ++x) {
                const int shift = (x & 1) ? 0 : 4;
                int count = 0;
                for (int y = 0; y < 9; ++y)
                    count += ((u8(font, 8 + y * 40 + x / 2) >> shift) & 15) == 1;
                if (count < 2)
                    continue;
                for (int y = 0; y < 8 && !altered; ++y) {
                    const int at = 8 + y * 40 + x / 2;
                    if (((u8(font, at) >> shift) & 15) == 1) {
                        put8(edited, at, u8(edited, at) & ~(15 << shift));
                        altered = true;
                    }
                }
            }
            QVERIFY(altered);
            for (int i = 0; i < 2; ++i) {
                const auto label = i ? ToolLabel::Alignment : ToolLabel::Luck;
                const auto expected =
                    QByteArray::fromHex(cases[i].toObject()["texture"].toString().toLatin1());
                QCOMPARE(expected.size(), 512);
                QCOMPARE(displayToolTexture(rom, label), expected);
                QCOMPARE(displayToolTexture(relocate(font), label), expected);
                QVERIFY(displayToolTexture(relocate(edited), label) != expected);
            }
            auto malformed = font;
            put16(malformed, 4, 79);
            QVERIFY_THROWS_EXCEPTION(Error,
                                     displayToolTexture(relocate(malformed), ToolLabel::Luck));
            malformed = font;
            malformed.replace(8, 360, QByteArray(360, 0));
            QVERIFY_THROWS_EXCEPTION(Error,
                                     displayToolTexture(relocate(malformed), ToolLabel::Luck));
            auto duplicate = rom.bytes();
            duplicate += archive.rebuild(rom, font);
            while (duplicate.size() % 4)
                duplicate.append(char(0));
            QVERIFY_THROWS_EXCEPTION(
                Error, displayToolTexture(Rom::fromBytes(duplicate), ToolLabel::Luck));
            auto badCrc = rom.bytes();
            QCOMPARE(archive.level, 2);
            badCrc[archive.offset + 21] ^= 1;
            QVERIFY_THROWS_EXCEPTION(
                Error, displayToolTexture(Rom::fromBytes(badCrc), ToolLabel::Alignment));
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void chaosProjectPersistenceAndImport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        project.loadTools();
        QVERIFY(project.tools()->chaosCounter().available());
        QVERIFY(!project.tools()->chaosCounter().enabled());
        auto settings = project.tools()->json();
        settings["chaosFrameCounter"] = true;
        project.setTools(settings);
        QCOMPARE(project.changedRecords(), 1);
        QTemporaryDir directory;
        project.saveFolder(directory.path());
        project.saveArchive(directory.filePath("counter.lcproject"));
        for (const auto &location : {directory.path(), directory.filePath("counter.lcproject")}) {
            auto reopened = Project::fromRom(rom);
            reopened.applyManifest(Project::readProject(location));
            QVERIFY(reopened.tools()->chaosCounter().enabled());
        }
        const QJsonObject patch{
            {"format", "ob64-patch"},
            {"version", 36},
            {"patches", QJsonObject{{"tools", QJsonObject{{"cf-army-counter", true}}}}}};
        project.importLegacy(patch);
        QVERIFY(project.tools()->chaosCounter().enabled());
        QVERIFY(project.pendingLegacySections().isEmpty());
        settings = project.tools()->json();
        settings["chaosFrameCounter"] = false;
        project.setTools(settings);
        auto reset = Project::fromRom(rom);
        reset.applyManifest(project.manifest());
        QVERIFY(!reset.tools()->chaosCounter().enabled());
        auto owner = patch;
        owner["patches"] = QJsonObject{};
        owner["rebuild_owners"] = QJsonObject{{"schema", 1}, {"chaos_frame_counter", true}};
        project.importLegacy(owner);
        QVERIFY(project.tools()->chaosCounter().enabled());
        QVERIFY(project.pendingLegacySections().isEmpty());
        auto conflict = patch;
        conflict["rebuild_owners"] = QJsonObject{{"chaos_frame_counter", false}};
        const auto before = project.manifest();
        QVERIFY_THROWS_EXCEPTION(Error, project.importLegacy(conflict));
        QCOMPARE(project.manifest(), before);
        for (const QJsonValue invalid : {QJsonValue(1), QJsonValue("true"), QJsonValue()}) {
            auto bad = project.tools()->json();
            bad["experienceScalePercent"] = 75;
            bad["chaosFrameCounter"] = invalid;
            QVERIFY_THROWS_EXCEPTION(Error, project.setTools(bad));
            QCOMPARE(project.manifest(), before);
        }
        // Older native experience-only snapshots leave the counter intact.
        project.setTools({{"version", 1}, {"experienceScalePercent", 137}});
        QVERIFY(project.tools()->chaosCounter().enabled());
        QCOMPARE(project.tools()->experiencePercent(), 137);
    }
    void chaosSourceExportAndReopen() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("chaos-browser-reference.cjs"),
                                     work.filePath("reference.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("reference.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto data = QJsonDocument::fromJson(file.readAll()).object();
            auto expected = rom.bytes();
            for (const auto &v : data["components"].toArray()) {
                const auto row = v.toObject();
                const auto bytes = QByteArray::fromHex(row["dataHex"].toString().toLatin1());
                expected.replace(row["romStart"].toInt(), bytes.size(), bytes);
            }
            Rom::updateChecksum(expected);
            auto project = Project::fromRom(rom);
            project.loadTools();
            auto settings = project.tools()->json();
            settings["chaosFrameCounter"] = true;
            project.setTools(settings);
            const auto built = rebuild(project, tools, work.path());
            QCOMPARE(built.image, expected);
            QVERIFY(built.report.join(" ").contains("Expansion Pak"));
            QTemporaryDir output(QFileInfo(path).absolutePath() + "/Test ROMs/native-chaos-XXXXXX");
            QVERIFY(output.isValid());
            auto save = [&](const BuildResult &build, const QString &name) {
                const auto folder = output.filePath(name);
                QDir().mkpath(folder);
                const auto candidate = QDir(folder).filePath(QFileInfo(path).fileName());
                exportRom(project, build, candidate);
                return Rom::open(candidate);
            };
            auto enabled = save(built, "enabled");
            project = Project::fromRom(enabled);
            project.loadTools();
            QVERIFY(project.tools()->chaosCounter().enabled());
            QVERIFY(!project.tools()->modified());
            QCOMPARE(rebuild(project, tools, work.path()).image, expected);
            settings = project.tools()->json();
            settings["chaosFrameCounter"] = false;
            project.setTools(settings);
            const auto disabled = rebuild(project, tools, work.path());
            QCOMPARE(disabled.image, rom.bytes());
            QVERIFY(!ChaosCounter::load(save(disabled, "disabled")).enabled());
            auto legacy = rom.bytes();
            for (const auto &v : data["legacy"].toArray()) {
                const auto row = v.toObject();
                const auto bytes = QByteArray::fromHex(row["patched"].toString().toLatin1());
                legacy.replace(row["offset"].toInt(), bytes.size(), bytes);
            }
            Rom::updateChecksum(legacy);
            project = Project::fromRom(Rom::fromBytes(legacy));
            project.loadTools();
            QCOMPARE(project.tools()->chaosCounter().installation(),
                     ChaosCounter::Installation::Legacy);
            const auto migrated = rebuild(project, tools, work.path());
            QCOMPARE(migrated.image, expected);
            QVERIFY(ChaosCounter::load(save(migrated, "migrated")).enabled());
            auto customLabel = expected;
            customLabel[0x27c1700] ^= 1;
            auto customized = Project::fromRom(Rom::fromBytes(customLabel));
            customized.loadTools();
            QVERIFY(customized.tools()->chaosCounter().available());
            QCOMPARE(rebuild(customized, tools, work.path()).image, customLabel);
            auto occupied = legacy;
            occupied[0x27c1000] = 42;
            auto removable = Project::fromRom(Rom::fromBytes(occupied));
            removable.loadTools();
            QVERIFY(removable.tools()->chaosCounter().enabled());
            QVERIFY_THROWS_EXCEPTION(Error, rebuild(removable, tools, work.path()));
            auto remove = removable.tools()->json();
            remove["chaosFrameCounter"] = false;
            removable.setTools(remove);
            auto restoredOccupied = rom.bytes();
            restoredOccupied[0x27c1000] = 42;
            QCOMPARE(rebuild(removable, tools, work.path()).image, restoredOccupied);
            for (const int at : {0x23f7c, 0x3078c, 0x34e78, 0x27c1000}) {
                auto foreign = rom.bytes();
                foreign[at] ^= 1;
                auto held = Project::fromRom(Rom::fromBytes(foreign));
                held.loadTools();
                QVERIFY(!held.tools()->chaosCounter().available());
                QVERIFY(held.tools()->available());
                auto invalid = held.tools()->json();
                invalid["chaosFrameCounter"] = true;
                QVERIFY_THROWS_EXCEPTION(Error, held.setTools(invalid));
                QCOMPARE(rebuild(held, tools, work.path()).image, foreign);
            }
            // Class additions and experience edits coexist with the counter's owners.
            project = Project::fromRom(rom);
            project.loadTools();
            settings = project.tools()->json();
            settings["chaosFrameCounter"] = true;
            settings["experienceScalePercent"] = 137;
            project.setTools(settings);
            project.appendClass(1, "Counter Knight");
            const auto combined = rebuild(project, tools, work.path());
            auto combinedProject = Project::fromRom(save(combined, "combined"));
            combinedProject.loadTools();
            QCOMPARE(combinedProject.records(Table::Classes).size(), 166);
            QVERIFY(combinedProject.tools()->chaosCounter().enabled());
            QCOMPARE(combinedProject.tools()->experiencePercent(), 137);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Chaos Frame export/reopen candidates:" << output.path();
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void chaosArtworkFromCurrentRom() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("chaos-browser-reference.cjs"),
                                     work.filePath("reference.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("reference.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto expected = QByteArray::fromHex(
                QJsonDocument::fromJson(file.readAll()).object()["title"].toString().toLatin1());
            QCOMPARE(expected.size(), 384);
            QCOMPARE(chaosFrameTitle(rom), expected);
            const auto resource = Resource::load(rom, 0x235c9be, u32(rom.bytes(), 0x235c9be) + 4);
            auto bytes = rom.bytes();
            const int offset = bytes.size(), key = offset - 0x594280;
            bytes += resource.originalSlot;
            while (bytes.size() % 4)
                bytes.append(char(0));
            put16(bytes, 0xaed4e, quint16(key >> 16));
            put16(bytes, 0xaed56, quint16(key));
            QCOMPARE(chaosFrameTitle(Rom::fromBytes(bytes)), expected);
            auto changed = resource.decoded;
            // Change one interior cream texel of C to outline. The glyph retains
            // its column span but its body must reach the generated title.
            bool altered = false;
            for (int y = 17; y < 21 && !altered; ++y)
                for (int x = 2; x < 6 && !altered; ++x) {
                    const int at = 0x40f8 + y * 32 + x / 2, shift = (x & 1) ? 0 : 4;
                    if (((u8(changed, at) >> shift) & 15) == 12) {
                        put8(changed, at, u8(changed, at) | (1 << shift));
                        altered = true;
                    }
                }
            QVERIFY(altered);
            const auto compressed = encodeLzss(changed);
            QByteArray relocated(8, 0);
            put32(relocated, 0, compressed.size() + 4);
            put32(relocated, 4, changed.size());
            relocated += compressed;
            bytes.resize(offset);
            bytes += relocated;
            while (bytes.size() % 4)
                bytes.append(char(0));
            QVERIFY(chaosFrameTitle(Rom::fromBytes(bytes)) != expected);
            put32(bytes, offset + 4, 100);
            QVERIFY_THROWS_EXCEPTION(Error, chaosFrameTitle(Rom::fromBytes(bytes)));
            bytes = rom.bytes();
            put32(bytes, 0xaed4c, 0);
            QVERIFY_THROWS_EXCEPTION(Error, chaosFrameTitle(Rom::fromBytes(bytes)));
            changed.fill(0);
            const auto blank = encodeLzss(changed);
            bytes = rom.bytes();
            QByteArray empty(8, 0);
            put32(empty, 0, blank.size() + 4);
            put32(empty, 4, changed.size());
            empty += blank;
            bytes.replace(0x235c9be, empty.size(), empty);
            QVERIFY_THROWS_EXCEPTION(Error, chaosFrameTitle(Rom::fromBytes(bytes)));
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void projectPersistenceAndImport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        project.loadTools();
        QVERIFY(project.tools()->available());
        QCOMPARE(project.tools()->experiencePercent(), 100);
        project.setTools({{"version", 1}, {"experienceScalePercent", 137}});
        QCOMPARE(project.changedRecords(), 1);
        QTemporaryDir folder;
        project.saveFolder(folder.path());
        project.saveArchive(folder.filePath("tools.lcproject"));
        for (const auto &location : {folder.path(), folder.filePath("tools.lcproject")}) {
            auto reopened = Project::fromRom(rom);
            reopened.applyManifest(Project::readProject(location));
            QCOMPARE(reopened.tools()->experiencePercent(), 137);
        }
        const auto saved = project.manifest();
        QVERIFY_THROWS_EXCEPTION(Error, project.setTools({{"version", 1},
                                                          {"experienceScalePercent", 137},
                                                          {"unknownFeature", true}}));
        QCOMPARE(project.manifest(), saved);
        for (const QJsonValue value :
             {QJsonValue(-1), QJsonValue(301), QJsonValue(1.5), QJsonValue("75")}) {
            QVERIFY_THROWS_EXCEPTION(
                Error, project.setTools({{"version", 1}, {"experienceScalePercent", value}}));
            QCOMPARE(project.manifest(), saved);
        }
        const QJsonObject patch{
            {"format", "ob64-patch"},
            {"version", 36},
            {"patches",
             QJsonObject{{"tools", QJsonObject{{"experience-size-scale",
                                                QJsonObject{{"schema", 1}, {"percent", 75}}}}}}}};
        auto imported = Project::fromRom(rom);
        imported.importLegacy(patch);
        QCOMPARE(imported.tools()->experiencePercent(), 75);
        QVERIFY(imported.pendingLegacySections().isEmpty());
        imported.setTools({{"version", 1}, {"experienceScalePercent", 100}});
        auto restored = Project::fromRom(rom);
        restored.applyManifest(imported.manifest());
        QCOMPARE(restored.tools()->experiencePercent(), 100); // Native reset supersedes old import.
        auto owner = patch;
        owner["patches"] = QJsonObject{};
        owner["rebuild_owners"] = QJsonObject{{"schema", 1}, {"experience_scale_percent", 300}};
        imported.importLegacy(owner);
        QCOMPARE(imported.tools()->experiencePercent(), 300);
        QVERIFY(imported.pendingLegacySections().isEmpty());
        auto other = patch;
        auto values = other["patches"].toObject();
        auto settings = values["tools"].toObject();
        settings["unknown-future-tool"] = true;
        values["tools"] = settings;
        other["patches"] = values;
        imported.importLegacy(other);
        QCOMPARE(imported.pendingLegacySections(), QStringList{"tools.unknown-future-tool"});
        auto conflict = patch;
        conflict["rebuild_owners"] = QJsonObject{{"experience_scale_percent", 137}};
        const auto before = imported.manifest();
        QVERIFY_THROWS_EXCEPTION(Error, imported.importLegacy(conflict));
        QCOMPARE(imported.manifest(), before);
        auto bytes = rom.bytes();
        bytes[0x2192f8] ^= 1;
        const auto foreign = ToolSettings::load(Rom::fromBytes(bytes));
        QVERIFY(!foreign.available());
        QVERIFY(!foreign.modified());
    }
    void sourceExportAndReopen() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("experience-browser-reference.cjs"), path,
                                     work.filePath("reference.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("reference.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto cases = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
            QCOMPARE(cases.size(), 5);
            QTemporaryDir output(QFileInfo(path).absolutePath() + "/Test ROMs/native-tools-XXXXXX");
            QVERIFY(output.isValid());
            auto project = Project::fromRom(rom);
            for (const auto &v : cases) {
                const auto row = v.toObject();
                const int percent = row["percent"].toInt();
                project.setTools({{"version", 1}, {"experienceScalePercent", percent}});
                const auto built = rebuild(project, tools, work.path());
                auto expected = rom.bytes();
                for (const auto &entry : row["writes"].toArray()) {
                    const auto write = entry.toObject();
                    const auto data = QByteArray::fromHex(write["bytes"].toString().toLatin1());
                    expected.replace(write["offset"].toInt(), data.size(), data);
                }
                QCOMPARE(built.image, expected);
                const auto directory = output.filePath(QString::number(percent));
                QVERIFY(QDir().mkpath(directory));
                const auto candidate = QDir(directory).filePath(QFileInfo(path).fileName());
                exportRom(project, built, candidate);
                project = Project::fromRom(Rom::open(candidate));
                project.loadTools();
                QCOMPARE(project.tools()->experiencePercent(), percent);
                QVERIFY(!project.tools()->modified());
            }
            QCOMPARE(project.rom().bytes(),
                     rom.bytes()); // 100% compiled restoration is byte-exact.
            auto foreign = rom.bytes();
            foreign[0x2192f8] ^= 1;
            auto unchanged = Project::fromRom(Rom::fromBytes(foreign));
            unchanged.loadTools();
            QCOMPARE(rebuild(unchanged, tools, work.path()).image, foreign);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Experience export/reopen candidates:" << output.path();
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
};
QTEST_GUILESS_MAIN(ToolSettingsTests)
#include "tool_settings_tests.moc"
