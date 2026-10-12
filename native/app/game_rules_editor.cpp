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
    const QMap<QString,QString> titles{{"trainingXp","Training experience"},{"clock","Day and night length — all missions"},{"fatigue","Fatigue while moving"},
        {"recovery","Recovery while resting"},{"tactics","Battle tactic alignment"},
        {"goth","Mission completion — Goth"},{"soldierPopulation","Mission completion — Soldier rewards"},{"soldierMinimum","Soldier minimum"},
        {"elements","Element balance"},{"weapons","Magic weapon synergy"},{"resistance","Resistance effects"}};
    for(const auto &group:groups) {
        auto box=new QGroupBox(titles.value(group,group));auto form=new QFormLayout(box);
        auto help=new QLabel(group=="tactics"
            ? "Each kill combines this amount with the character and enemy alignment/level terms. The final change remains capped at −5 to +5. Training keeps its existing exclusion."
            : group=="trainingXp" ? "Adds a configurable bonus to the usual defeated-enemy XP after winning training. Fallen, petrified, Soldier and level-99 members keep their normal exclusions. Losses give no XP."
            : group=="goth" ? "Base mission Goth + captured population × distance from 50 Chaos Frame ÷ divisor. Smaller divisors give larger bonuses. Wallet limit stays 9,999,999."
            : group=="soldierPopulation" ? "Each captured town contributes population × morale ÷ 10,000, scaled here, with the minimum below. Objective towns use morale 50. To disable these rewards, set both contribution and minimum to 0."
            : group=="soldierMinimum" ? "The game adds town contributions, wraps totals at 256, then fills the Soldier reserve up to 200. Keep large population and minimum settings in mind when tuning this formula."
            : group=="elements" ? "Wind opposes Earth; Flame opposes Water. This shared bonus affects damage context and target resistance. Virtue/Bane alignment terms keep their existing behavior."
            : group=="weapons" ? "Matching elemental weapons strengthen magic; opposed weapons weaken it. Virtue opposes Bane as well as the four natural-element pairs."
            : group=="resistance" ? "100% keeps retail damage reduction. 50% halves its strength; 200% doubles it. Damage still has the game's minimum of 1. Fixed-damage attacks keep their special formulas."
            : group=="clock" ? "100% keeps retail timing. 200% lasts twice as long; 50% lasts half as long. These settings affect every mission."
            : "100% keeps retail timing. 200% gives roughly twice as many cycles; 50% gives half as many. Existing eligibility and maximums remain.");
        help->setWordWrap(true);form->addRow(help);
        for(const auto &rule:GameRules::definitions()) if(rule.group==group) {
            auto row=new QWidget;auto horizontal=new QHBoxLayout(row);horizontal->setContentsMargins(0,0,0,0);
            auto spin=new QSpinBox;spin->setObjectName("rule_"+rule.key);spin->setRange(rule.minimum,rule.maximum);
            spin->setSingleStep(rule.step);spin->setKeyboardTracking(false);spin->setToolTip(rule.help);
            if(rule.percent) spin->setSuffix(" %");
            horizontal->addWidget(spin,1);fields_[rule.key]=spin;
            auto reset=new QPushButton("Retail");reset->setObjectName("ruleReset_"+rule.key);reset->setToolTip("Reset this setting to retail");
            horizontal->addWidget(reset);form->addRow(rule.label,row);
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
        const auto problem=project_->tools()->gameRules().unavailable(group);
        if(!problem.isEmpty()) {auto notice=new QLabel(problem);notice->setWordWrap(true);form->addRow(notice);box->setEnabled(false);}
        layout->addWidget(box);
    }
    connect(undo,&QUndoStack::indexChanged,this,[this,changed]{refresh();changed();});
    refresh();
}
void GameRulesEditor::refresh() {
    for(auto it=fields_.begin();it!=fields_.end();++it) {
        const QSignalBlocker block(it.value());it.value()->setValue(project_->tools()->gameRules().value(it.key()));
    }
}
}
