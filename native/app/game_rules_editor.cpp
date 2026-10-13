#include "app/game_rules_editor.h"
#include "app/content_refresh.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cmath>
#include <algorithm>
namespace lordly {
namespace {
class RuleCommand final : public GameRuleUndoCommand {
  public:
    RuleCommand(Project *p, const QString &key, int value, const QString &label)
      : GameRuleUndoCommand("Change " + label), p_(p), before_(p->tools()->json()), after_(before_) {
        auto rules=after_["gameRules"].toObject(); rules[key]=value; after_["gameRules"]=rules;
    }
    void undo() override { p_->setTools(before_); }
    void redo() override { p_->setTools(after_); }
  private:
    Project *p_; QJsonObject before_,after_;
};
}
GameRulesEditor::GameRulesEditor(Project *p,QUndoStack *undo,std::function<void()> changed,
                               const QStringList &groups,QWidget *parent):QWidget(parent),project_(p) {
    project_->loadTools();
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    const QMap<QString,QString> titles{{"deployment","Mission deployment"},{"reserve","Reserve experience — Expansion Pak required (8 MiB RAM)"},{"trainingXp","Training experience"},{"clock","Day and night length — all missions"},{"fatigue","Fatigue while moving"},
        {"recovery","Recovery while resting"},{"tactics","Battle tactic alignment"},
        {"goth","Mission completion — Goth"},{"soldierPopulation","Mission completion — Soldier rewards"},{"soldierMinimum","Soldier minimum"},
        {"elements","Character elemental affinity"},{"weapons","Weapon bonus to spells"},{"resistance","Resistance effects"}};
    for(const auto &group:groups) {
        auto box=new QGroupBox(titles.value(group,group));box->setObjectName("ruleGroup_"+group);auto form=new QFormLayout(box);
        auto help=new QLabel(group=="tactics"
            ? "Each credited kill combines the selected tactic's amount with the character and enemy alignment/level terms. The maximum below limits the total gain or loss. Training keeps its existing exclusion."
            : group=="deployment" ? "Choose a limit from 1 to 20 newly deployed player squads. Every squad in a Legion uses a slot, so a Legion must fit as a whole. Already deployed squads stay on the map if you lower the limit."
            : group=="reserve" ? "Once per completed mission, unassigned reserve characters gain the XP below. Assigned squad members, fallen or petrified characters, Soldiers and level-99 characters are excluded. 0 keeps retail behavior."
            : group=="trainingXp" ? "Adds a configurable bonus to the usual defeated-enemy XP after winning training. Fallen, petrified, Soldier and level-99 members keep their normal exclusions. Losses give no XP."
            : group=="goth" ? "Base mission Goth + captured population × distance from 50 Chaos Frame ÷ divisor. Smaller divisors give larger bonuses. Wallet limit stays 9,999,999."
            : group=="soldierPopulation" ? "Each captured town contributes population × morale ÷ 10,000, scaled here, with the minimum below. Objective towns use morale 50. To disable these rewards, set both contribution and minimum to 0."
            : group=="soldierMinimum" ? "The game adds town contributions, wraps totals at 256, then fills the Soldier reserve up to 200. Keep large population and minimum settings in mind when tuning this formula."
            : group=="elements" ? "Applies to elemental weapon hits and spells. Compares the attack's element with the character's own element: matching adds points; opposed subtracts them; other pairings add 0. Opposed pairs: Wind / Earth and Flame / Water. Virtue/Bane keep their alignment-based rules."
            : group=="weapons" ? "Applies when casting a spell with an elemental weapon equipped. Works with swords, spears and other weapons as well as staves. Compares the spell's element with the weapon's element: matching adds points to the magic attack factor; opposed subtracts them. Normal weapon hits get no bonus from this setting."
            : group=="recovery" ? "Rest recovery controls how often both fatigue and HP recover: 200% gives roughly twice as many cycles; 50% gives half as many. Each cycle removes up to 2 fatigue and restores the HP amount below. Night and Healing Censer bonuses remain. Living, non-petrified characters heal up to maximum HP; Soldier groups keep their native recovery stages."
            : group=="resistance" ? "Controls how much physical and elemental resistance reduces incoming damage. Higher values make resistance more protective; lower values make attacks hurt more. 100% keeps retail strength; 0% removes this reduction. The character's listed resistance does not change."
            : group=="clock" ? "100% keeps retail timing. 200% lasts twice as long; 50% lasts half as long. These settings affect every mission."
            : "100% keeps retail timing. 200% gives roughly twice as many cycles; 50% gives half as many. Existing eligibility and maximums remain.");
        help->setWordWrap(true);form->addRow(help);
        for(const auto &rule:GameRules::definitions()) if(rule.group==group ||
            (group=="tactics" && rule.group=="alignmentCap") ||
            (group=="recovery" && rule.group=="restHealing")) {
            auto row=new QWidget;auto horizontal=new QHBoxLayout(row);horizontal->setContentsMargins(0,0,0,0);
            auto spin=new QSpinBox;spin->setObjectName("rule_"+rule.key);spin->setRange(rule.minimum,rule.maximum);
            spin->setSingleStep(rule.step);spin->setKeyboardTracking(false);spin->setToolTip(rule.help);
            if(rule.percent) spin->setSuffix(" %");
            else if(rule.key=="restHealing") spin->setSuffix(" HP");
            else if(rule.group=="elements" || rule.group=="weapons" || rule.group=="alignmentCap") spin->setSuffix(" points");
            horizontal->addWidget(spin,1);fields_[rule.key]=spin;
            if(rule.key=="deploymentLimit") {
                auto fix=new QPushButton("Use 20");fix->setObjectName("ruleFix_deploymentLimit");
                horizontal->addWidget(fix);
                connect(fix,&QPushButton::clicked,this,[this,undo,rule]{
                    undo->push(new RuleCommand(project_,rule.key,20,rule.label));
                });
            }
            auto reset=new QPushButton("Retail");reset->setObjectName("ruleReset_"+rule.key);reset->setToolTip("Reset this setting to retail");
            horizontal->addWidget(reset);form->addRow(rule.label,row);
            const auto unavailable=project_->tools()->gameRules().unavailable(rule.group);
            if(!unavailable.isEmpty()) {spin->setEnabled(false);reset->setEnabled(false);row->setToolTip(unavailable);}
            connect(reset,&QPushButton::clicked,spin,[spin,rule]{spin->setValue(rule.retail);});
            connect(spin,&QSpinBox::valueChanged,this,[this,undo,spin,rule](int value){
                const int rounded=rule.minimum+int(std::round(double(value-rule.minimum)/rule.step))*rule.step;
                if(value!=rounded) {const QSignalBlocker block(spin);spin->setValue(rounded);}
                if(rounded==project_->tools()->gameRules().value(rule.key)) return;
                auto check=project_->tools()->gameRules();auto settings=check.json();settings[rule.key]=rounded;
                try {check.apply(settings);} catch(const Error &e) {QMessageBox::warning(this,"Game rules",e.what());refresh();return;}
                undo->push(new RuleCommand(project_,rule.key,rounded,rule.label));
            });
        }
        if(group=="tactics" || group=="elements" || group=="weapons" || group=="recovery" || group=="resistance") {
            auto example=new QLabel;example->setObjectName("ruleExample_"+group);
            example->setWordWrap(true);example->setTextInteractionFlags(Qt::TextSelectableByMouse);
            form->addRow(example);
        }
        if(group=="deployment") {
            auto notice=new QLabel;notice->setObjectName("expandedArmyLimit");notice->setWordWrap(true);
            form->addRow(notice);
        }
        const auto problem=project_->tools()->gameRules().unavailable(group);
        if(!problem.isEmpty()) {auto notice=new QLabel(problem);notice->setWordWrap(true);form->addRow(notice);box->setEnabled(false);}
        layout->addWidget(box);
    }
    connect(undo,&QUndoStack::indexChanged,this,[this,changed]{refresh();changed();});
    refresh();
}
void GameRulesEditor::refresh() {
    const auto &rules=project_->tools()->gameRules();
    for(auto it=fields_.begin();it!=fields_.end();++it) {
        const QSignalBlocker block(it.value());
        if(it.key()=="deploymentLimit") {
            const bool unsupported=rules.value(it.key())>20;
            // Show the real imported value, but only allow correction rather
            // than new edits above the supported limit.
            it.value()->setMaximum(std::max(20,rules.value(it.key())));
            it.value()->setEnabled(!unsupported && rules.unavailable("deployment").isEmpty());
            if(auto fix=findChild<QPushButton *>("ruleFix_deploymentLimit")) {
                fix->setVisible(unsupported);fix->setEnabled(rules.unavailable("deployment").isEmpty());
            }
        }
        it.value()->setValue(rules.value(it.key()));
    }
    if(auto notice=findChild<QLabel *>("expandedArmyLimit")) {
        notice->setText("All 30 organized player squads, the 99-member roster and reserves remain saved."
            +(rules.value("deploymentLimit")>20
                ?QString(" This ROM or project has a limit of %1, above the supported maximum. Export is blocked. Choose Use 20 or Retail to correct it.").arg(rules.value("deploymentLimit"))
                :QString()));
    }
    auto example=[this](const QString &group,const QString &text) {
        if(auto label=findChild<QLabel *>("ruleExample_"+group)) label->setText(text);
    };
    const int cap=rules.value("alignmentCap");
    example("tactics",cap ? QString("Current limit: −%1 to +%1 points per kill. A calculated +8 becomes +%2; −8 becomes −%2. Alignment always stays within 0–100.").arg(cap).arg(std::min(8,cap))
                          : "Kill-based alignment changes are disabled. Items and other alignment effects keep their existing rules.");
    const int affinity=rules.value("elementAffinity"), weapon=rules.value("weaponAffinity");
    example("elements",QString("Flame sword example: a Flame character's normal Flame sword hit gets +%1 points to its attack factor (100% → %2%). A Water character using that sword gets −%1 (100% → %3%).\n\n"
        "The incoming attack is also compared with the defender's element to adjust defense and resistance: +%1 matching resistance points or −%1 opposed points, within 0–100. These modify the calculation, not final damage directly.")
        .arg(affinity).arg(100+affinity).arg(100-affinity));
    example("weapons",QString("At %1 points: a matching weapon changes a neutral 100% magic attack factor to %2%; an opposed weapon changes it to %3%. Opposed pairs: Wind / Earth, Flame / Water, Virtue / Bane. Non-elemental and unrelated pairings add 0.\n\n"
        "Together: a Flame character holding a Flame sword or staff casts a Flame spell: +%4 character affinity +%1 weapon bonus = +%5 points (100% → %6% before other modifiers). A normal sword hit uses only Character elemental affinity. Final damage also depends on the target's defense and resistance.")
        .arg(weapon).arg(100+weapon).arg(100-weapon).arg(affinity).arg(affinity+weapon).arg(100+affinity+weapon));
    const int resistance=rules.value("resistanceStrength");
    example("resistance",QString("Example: a hit worth 100 damage before resistance, against 40 resistance. At 50% strength it deals 80 damage; at retail 100%, 60 damage; at 200%, 20 damage.\n\n"
        "Current setting (%1%): about %2 damage in this example. Other modifiers and random variation are held fixed. Damage stays at least 1; healing and fixed-damage attacks keep their own rules.")
        .arg(resistance).arg(std::max(1,100-40*resistance/100)));
    const int hp=rules.value("restHealing"), rate=rules.value("recoveryRate");
    example("recovery",hp ? QString("Current settings: %1 HP per eligible member per cycle at %2% cycle speed — about %3× retail HP healing over the same resting time, until the native HP limits are reached.")
        .arg(hp).arg(rate).arg(QString::number(double(hp)*rate/100.,'g',4))
        : "HP healing is disabled. Fatigue still recovers at the selected cycle rate.");
    if(!rules.unavailable("alignmentCap").isEmpty())
        example("tactics","The final cap is unavailable for this ROM: "+rules.unavailable("alignmentCap"));
    if(!rules.unavailable("restHealing").isEmpty())
        example("recovery","The healing amount is unavailable for this ROM: "+rules.unavailable("restHealing"));
}
}
