#include "app/class_setup.h"
#include <QGroupBox>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
namespace lordly {
ClassSetup::ClassSetup(Project *project, std::function<void(const EditLocation &)> navigate, QWidget *parent)
    : QWidget(parent), project_(project) {
    setObjectName("classSetup");
    auto layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    title_ = new QLabel; title_->setWordWrap(true); layout->addWidget(title_);
    portrait_ = new QLabel; portrait_->setObjectName("classSetupPortrait"); layout->addWidget(portrait_);
    auto card = [&](const QString &title, const QString &group, QLabel *&summary) {
        auto box = new QGroupBox(title); auto body = new QVBoxLayout(box);
        body->setSizeConstraint(QLayout::SetMinimumSize);
        box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        summary = new QLabel; summary->setWordWrap(true); summary->setTextFormat(Qt::PlainText);
        summary->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum); body->addWidget(summary);
        auto edit = new QPushButton("Edit " + title.toLower());
        edit->setObjectName("classSetup" + group); body->addWidget(edit);
        connect(edit, &QPushButton::clicked, this, [this, navigate, group] {
            if (id_ >= 0) navigate({"classes", id_, -1, "section:" + group});
        });
        layout->addWidget(box);
    };
    card("Stats", "Stats", stats_);
    card("Starting equipment", "Equipment", equipment_);
    card("Combat actions", "Attacks", actions_);
    card("Family and class change", "Promotion", family_);
    family_->setObjectName("classSetupFamily");
    auto art = new QGroupBox("Artwork and animation"); auto artLayout = new QVBoxLayout(art);
    artLayout->setSizeConstraint(QLayout::SetMinimumSize);
    art->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    const QVector<QPair<QString, QString>> parts{{"Portraits", "portrait"}, {"Army sprites", "armyClass"}, {"Combat animations", "combatAnimation"}};
    for (const auto &part : parts) {
        auto button = new QPushButton(part.first); button->setObjectName("classSetup_" + part.second);
        artLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, navigate, kind=part.second] {
            if (id_ >= 0) navigate({kind, id_});
        });
    }
    layout->addWidget(art);
    checklist_ = new QLabel; checklist_->setObjectName("classSetupChecklist"); checklist_->setWordWrap(true);
    checklist_->setTextFormat(Qt::PlainText); layout->addWidget(checklist_); layout->addStretch();
    selectClass(-1);
}
void ClassSetup::selectClass(int id) {
    id_ = id;
    setEnabled(id >= 0 && id < project_->records(Table::Classes).size());
    if (id < 0 || id >= project_->records(Table::Classes).size()) { title_->setText("Select a class to review its setup."); return; }
    title_->setText(QString("<b>%1 · %2</b>").arg(id).arg(project_->record(Table::Classes, id).name.simplified().toHtmlEscaped()));
    QStringList stats, equipment, actions, needs;
    int activeActions = 0, equipped = 0;
    for (int n = 0; n < fields(Table::Classes).size(); ++n) {
        const auto &f = fields(Table::Classes)[n];
        const auto value = project_->value(Table::Classes, id, n);
        if (f.group == "Stats") stats << f.label + ": " + QString::number(value);
        if (f.key.startsWith("equipment")) {
            const auto name = project_->catalog()[f.choices].toObject()[QString::number(value)].toString(QString::number(value));
            equipment << f.label + ": " + (value ? name : "None"); equipped += value != 0;
        }
        if (f.key.startsWith("attack") && !f.key.startsWith("attackCount")) {
            int action = value;
            if (action == 255) {
                const int owner = u8(project_->record(Table::Classes, id).bytes, 69);
                if (owner > 0 && owner < project_->records(Table::Classes).size()) action = project_->value(Table::Classes, owner, n);
            }
            const int count = project_->value(Table::Classes, id, n + 1);
            const auto name = project_->catalog()[f.choices].toObject()[QString::number(action)].toString(QString("Action %1").arg(action));
            actions << QString("%1: %2 × %3%4").arg(f.label, action ? name : "None").arg(count).arg(value == 255 ? " (inherited)" : "");
            activeActions += action != 0 && count != 0;
        }
    }
    stats_->setText(stats.join(" · ")); equipment_->setText(equipment.join('\n')); actions_->setText(actions.join('\n'));
    try {
        project_->loadPromotion();
        const auto &p = *project_->promotion();
        const int source = project_->hasClassData() ? project_->classData().defaultPromotionClass(id) : id;
        if (source > 0 && source < p.classCount()) {
            const int family = p.families["assignments"].toArray()[source].toInt();
            QString name = "None";
            for (const auto &f : p.families["families"].toArray())
                if (f.toObject()["id"].toInt() == family) name = Promotion::familyName(f.toObject());
            family_->setText(source == id
                ? "Class family: " + name + "\nCharacter compatibility and promotion requirements are edited separately here."
                : QString("Default class-change family: %1\nUses %2's class-change options. Open Promotion to view the shared requirements.")
                    .arg(name, project_->record(Table::Classes, source).name.simplified()));
            if (!family) needs << "No class family assigned";
        } else {
            family_->setText("No manual class-change entry for this class's default current class.");
            needs << "No manual class-change entry";
        }
    } catch (const std::exception &e) { family_->setText(QString::fromUtf8(e.what())); }
    try {
        project_->loadAvatarArtwork();
        portrait_->setPixmap(QPixmap::fromImage(project_->avatarArtwork()->image(project_->classData(), id, 0).scaled(96,96,Qt::KeepAspectRatio,Qt::FastTransformation)));
    } catch (const std::exception &) { portrait_->setText("Portrait preview unavailable"); }
    if (!activeActions) needs << "No active combat actions";
    if (!equipped) needs << "No starting equipment";
    checklist_->setText(needs.isEmpty() ? "Family, equipment and combat actions are assigned. Review their values and artwork before testing."
                                     : "Setup to review:\n• " + needs.join("\n• ") + "\nThese choices may be intentional.");
}
}
