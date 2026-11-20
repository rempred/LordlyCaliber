#include "app/consumable_assignment.h"
#include "engine/consumable_tree.h"
#include "engine/consumable_visual.h"
#include "engine/project.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <array>

namespace lordly {
namespace {
QString effectDescription(const QString &type, int amount, bool percent) {
    const auto hp = QString::number(amount) + (percent ? "% of max HP" : " HP");
    if (type == "heal")
        return "Restore " + hp + ".";
    if (type == "revive")
        return "Revive with " + hp + ".\nRemove petrification.";
    if (type == "cure")
        return "Remove petrification.";
    if (type == "fatigue")
        return QString("Reduce fatigue by %1.").arg(amount);
    if (type == "level")
        return QString("Gain %1 %2.").arg(amount).arg(amount == 1 ? "level" : "levels");
    const auto stat = ConsumableTree::actions().value(type).mid(QString("Change ").size());
    return QString("%1 %2 by %3.").arg(amount < 0 ? "Lower" : "Raise", stat).arg(qAbs(amount));
}
} // namespace

QString fixedConsumableDescription(const QJsonArray &steps) {
    if (steps.size() != 1)
        return {};
    const auto node = steps.first().toObject();
    const auto type = node["type"].toString();
    if (!ConsumableTree::actions().contains(type) || node["minimum"] != node["maximum"])
        return {};
    return effectDescription(type, node["minimum"].toInt(1), node["percent"].toBool());
}

bool assignConsumableUse(QWidget *parent, Project &project, int item) {
    project.loadTexts();
    const auto &bank = project.textBanks().constFind("consumables").value();
    const auto existingDescription = bank.text(item);
    QDialog dialog(parent);
    dialog.setWindowTitle("Assign item use · " +
                          project.record(Table::Consumables, item).name.simplified());
    auto layout = new QVBoxLayout(&dialog);
    auto form = new QFormLayout;
    layout->addLayout(form);
    QComboBox effect, animation;
    effect.setObjectName("assignmentEffect");
    animation.setObjectName("assignmentAnimation");
    const bool editable = ConsumableTree::supported(item);
    QString keep = editable ? "Keep current effects and conditions" : "Use original game behavior";
    if (editable && project.consumableEffects()) {
        auto summary = fixedConsumableDescription(project.consumableEffects()->tree(item));
        if (!summary.isEmpty())
            keep = "Keep current · " + summary.replace('\n', ' ');
    }
    effect.addItem(keep);
    const auto actions = ConsumableTree::actions();
    if (editable) {
        for (const auto &type :
             {"heal", "fatigue", "cure", "revive", "level", "maxHp", "strength", "vitality",
              "intelligence", "mentality", "agility", "dexterity", "alignment", "luck"})
            effect.addItem(actions.value(type), QString(type));
    }
    effect.setEnabled(editable);
    form->addRow("Gameplay effect", &effect);
    const auto &bytes = project.record(Table::Consumables, item).bytes;
    form->addRow("Target", new QLabel(item == 20 ? "Selected squad"
                                                 : ConsumableTree::targetSummary(u8(bytes, 8),
                                                                                 u8(bytes, 9))));
    QSpinBox amount;
    amount.setObjectName("assignmentAmount");
    form->addRow("Amount", &amount);
    QCheckBox percent("Percentage of maximum HP");
    form->addRow(&percent);
    QLabel replacement("This replaces the item's current effects and conditions. Eligibility "
                       "follows the selected effect.");
    replacement.setWordWrap(true);
    form->addRow(&replacement);

    const auto &effects = *project.consumableEffects();
    const auto visuals = effects.visuals();
    const auto &rom = project.runtimeRom();
    const auto original = ConsumableVisual::retail(rom, item);
    const auto current =
        visuals.contains(QString::number(item))
            ? ConsumableVisual::fromJson(visuals.value(QString::number(item)).toObject())
            : original;
    QVector<ConsumableVisual> choices{current, ConsumableVisual::retail(rom, 1),
                                      ConsumableVisual::retail(rom, 6)};
    const auto currentName =
        visuals.contains(QString::number(item))
            ? current.name
            : (item == 6 || item == 7 ? QString("Pink sparkle") : QString("Gold sparkle"));
    animation.addItems({"Keep current (" + currentName + ")", "Gold sparkle", "Pink sparkle"});
    for (auto it = visuals.begin(); it != visuals.end(); ++it) {
        const int id = it.key().toInt();
        if (id == item)
            continue;
        const auto source = ConsumableVisual::fromJson(it.value().toObject());
        choices << source;
        animation.addItem(source.name + " · " +
                          project.record(Table::Consumables, id).name.simplified());
    }
    form->addRow("Use animation", &animation);
    auto frames = new QHBoxLayout;
    form->addRow(QString(), frames);
    std::array<QLabel *, 4> thumbnails;
    for (auto &thumb : thumbnails) {
        thumb = new QLabel;
        frames->addWidget(thumb);
    }
    frames->addStretch();
    QCheckBox useWording("Describe the selected effect in game");
    useWording.setObjectName("assignmentDescribeEffect");
    const auto priorWording = fixedConsumableDescription(effects.tree(item));
    const bool generated = !priorWording.isEmpty() && existingDescription == priorWording;
    useWording.setChecked(generated || (!effects.trees().contains(QString::number(item)) &&
                                        bank.entries[item].bytes == bank.entries[item].original));
    form->addRow(&useWording);
    QPlainTextEdit description;
    description.setObjectName("assignmentDescription");
    description.setMaximumHeight(85);
    QString manualDescription = existingDescription;
    form->addRow("In-game description", &description);
    auto error = new QLabel;
    error->setWordWrap(true);
    layout->addWidget(error);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    auto refresh = [&] {
        const auto type = effect.currentData().toString();
        const bool numeric = !type.isEmpty() && type != "cure";
        const bool hp = type == "heal" || type == "revive";
        form->setRowVisible(&amount, numeric);
        form->setRowVisible(&percent, hp);
        const auto [low, high] = ConsumableTree::amountRange(type, hp && percent.isChecked());
        amount.setRange(low, high);
        replacement.setVisible(!type.isEmpty());
        form->setRowVisible(&useWording, !type.isEmpty());
        form->setRowVisible(&description, !type.isEmpty());
        description.setReadOnly(useWording.isChecked());
        const auto wording =
            useWording.isChecked() && !type.isEmpty()
                ? effectDescription(type, amount.value(), hp && percent.isChecked())
                : manualDescription;
        if (wording != description.toPlainText()) {
            const QSignalBlocker guard(description);
            description.setPlainText(wording);
        }
        buttons->button(QDialogButtonBox::Save)
            ->setEnabled(!type.isEmpty() || animation.currentIndex() > 0);
    };
    auto display = [&] {
        for (int i = 0; i < 4; ++i)
            thumbnails[i]->setPixmap(
                QPixmap::fromImage(choices[animation.currentIndex()].frame(i).scaled(
                    48, 48, Qt::KeepAspectRatio, Qt::FastTransformation)));
        refresh();
    };
    QObject::connect(&effect, &QComboBox::currentIndexChanged, &dialog, [&] {
        percent.setChecked(false);
        refresh();
        amount.setValue(ConsumableTree::action(effect.currentData().toString())["minimum"].toInt());
    });
    QObject::connect(&percent, &QCheckBox::toggled, &dialog, refresh);
    QObject::connect(&amount, &QSpinBox::valueChanged, &dialog, refresh);
    QObject::connect(&useWording, &QCheckBox::toggled, &dialog, refresh);
    QObject::connect(&description, &QPlainTextEdit::textChanged, &dialog, [&] {
        if (!description.isReadOnly())
            manualDescription = description.toPlainText();
    });
    QObject::connect(&animation, &QComboBox::currentIndexChanged, &dialog, display);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        try {
            if (!effect.currentData().toString().isEmpty())
                project.setText("consumables", item, description.toPlainText());
            dialog.accept();
        } catch (const std::exception &e) {
            error->setText(QString::fromUtf8(e.what()));
        }
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    display();
    if (dialog.exec() != QDialog::Accepted)
        return false;
    auto result = effects;
    const auto type = effect.currentData().toString();
    if (!type.isEmpty()) {
        auto node = ConsumableTree::action(type);
        if (type != "cure") {
            node["minimum"] = amount.value();
            node["maximum"] = amount.value();
        }
        if ((type == "heal" || type == "revive") && percent.isChecked())
            node["percent"] = true;
        result.setTree(item, {node});
    }
    if (animation.currentIndex() > 0) {
        auto chosen = choices[animation.currentIndex()];
        if (animation.currentIndex() <= 2 && chosen.pixels == original.pixels &&
            chosen.palette == original.palette)
            result.restoreVisual(item);
        else if (animation.currentIndex() <= 2) {
            chosen.name = animation.currentText();
            result.setVisual(item, chosen.json());
        } else
            result.setVisual(item, chosen.json());
    }
    project.setConsumableEffects(result.json());
    return true;
}
} // namespace lordly
