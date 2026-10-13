#include "app/game_rules_editor.h"
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
class RuleCommand final : public QUndoCommand {
  public:
    RuleCommand(Project *p, const QString &key, int value, const QString &label)
      : QUndoCommand("Change " + label), p_(p), before_(p->tools()->json()), after_(before_) {
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
            : group=="deployment" ? "Limits newly deployed player squads. Every squad in a Legion uses a slot, so a Legion must fit as a whole. Already deployed squads stay on the map if you lower the limit."
            : group=="reserve" ? "Once per completed mission, unassigned reserve characters gain the XP below. Assigned squad members, fallen or petrified characters, Soldiers and level-99 characters are excluded. 0 keeps retail behavior."
            : group=="trainingXp" ? "Adds a configurable bonus to the usual defeated-enemy XP after winning training. Fallen, petrified, Soldier and level-99 members keep their normal exclusions. Losses give no XP."
            : group=="goth" ? "Base mission Goth + captured population × distance from 50 Chaos Frame ÷ divisor. Smaller divisors give larger bonuses. Wallet limit stays 9,999,999."
            : group=="soldierPopulation" ? "Each captured town contributes population × morale ÷ 10,000, scaled here, with the minimum below. Objective towns use morale 50. To disable these rewards, set both contribution and minimum to 0."
            : group=="soldierMinimum" ? "The game adds town contributions, wraps totals at 256, then fills the Soldier reserve up to 200. Keep large population and minimum settings in mind when tuning this formula."
            : group=="elements" ? "Compares an attack's element with the character's own element. Matching elements add the amount below; opposed elements subtract it; other pairings add 0. Opposed pairs: Wind / Earth and Flame / Water. This affects attack and defense factors, plus the target's resistance. Virtue/Bane alignment effects keep their own rules."
            : group=="weapons" ? "Compares a spell's element with the caster's weapon element. Matching elements add the amount below to the magic attack factor; opposed elements subtract it. Opposed pairs: Wind / Earth, Flame / Water, Virtue / Bane. Physical and unrelated elements add 0. This setting affects spells, not the weapon's physical damage."
            : group=="recovery" ? "Rest recovery controls how often both fatigue and HP recover: 200% gives roughly twice as many cycles; 50% gives half as many. Each cycle removes up to 2 fatigue and restores the HP amount below. Night and Healing Censer bonuses remain. Living, non-petrified characters heal up to maximum HP; Soldier groups keep their native recovery stages."
            : group=="resistance" ? "100% keeps retail damage reduction. 50% halves its strength; 200% doubles it. Damage still has the game's minimum of 1. Fixed-damage attacks keep their special formulas."
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
        if(group=="tactics" || group=="elements" || group=="weapons" || group=="recovery") {
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
        if(it.key()=="deploymentLimit")
            it.value()->setMaximum(project_->expandedArmies()?std::max(20,rules.value(it.key())):30);
        it.value()->setValue(rules.value(it.key()));
    }
    if(auto notice=findChild<QLabel *>("expandedArmyLimit")) {
        notice->setVisible(project_->expandedArmies());
        notice->setText("Expanded armies allow up to 20 deployed player squads and 30 enemy/allied squads. All 30 organized player squads, the 99-member roster and reserves remain saved."
            +QString(rules.value("deploymentLimit")>20?" Lower Maximum deployed squads to 20 or less before exporting.":""));
    }
    auto example=[this](const QString &group,const QString &text) {
        if(auto label=findChild<QLabel *>("ruleExample_"+group)) label->setText(text);
    };
    const int cap=rules.value("alignmentCap");
    example("tactics",cap ? QString("Current limit: −%1 to +%1 points per kill. A calculated +8 becomes +%2; −8 becomes −%2. Alignment always stays within 0–100.").arg(cap).arg(std::min(8,cap))
                          : "Kill-based alignment changes are disabled. Items and other alignment effects keep their existing rules.");
    const int affinity=rules.value("elementAffinity"), weapon=rules.value("weaponAffinity");
    example("elements",QString("At %1 points, a matching element changes a neutral 100% factor to %2%; an opposed element changes it to %3%. The target also gets +%1 matching resistance points or −%1 opposed resistance points, within the normal 0–100 resistance range. Final damage combines these factors with stats, equipment and other modifiers.")
        .arg(affinity).arg(100+affinity).arg(100-affinity));
    example("weapons",QString("At %1 points, a matching weapon changes a neutral 100% magic attack factor to %2%; an opposed weapon changes it to %3%. Raising 2 to 10 increases this adjustment fivefold.\n\n"
        "How they work together: a Flame character holding a Flame weapon casts a Flame spell. Their innate Flame element gives the bonus controlled by Character elemental affinity. Their Flame weapon gives the additional bonus controlled by Weapon bonus to spells. Both can affect the same spell. The target's element and resistance also affect the final damage.")
        .arg(weapon).arg(100+weapon).arg(100-weapon));
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
