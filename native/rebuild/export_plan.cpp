#include "rebuild/export_plan.h"
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QSet>

namespace lordly {
void ExportPlan::restore(QString name, Action prepare) {
    if(building_) throw Error("Restoration must precede new ROM writers");
    QStringList previous;
    if(!steps_.isEmpty()) previous.append(steps_.last().name);
    steps_.append({std::move(name),previous,std::move(prepare),{},true});
}
void ExportPlan::add(QString name, QStringList prerequisites, Action prepare, Ranges owners) {
    for (auto owner : owners) RomWriter::validateRange(owner);
    building_=true;
    steps_.append({std::move(name), std::move(prerequisites), std::move(prepare), std::move(owners)});
}
void ExportPlan::compose(const QString &later, const QString &earlier, Ranges ranges) {
    for (auto range : ranges) RomWriter::validateRange(range);
    compose(later, earlier, [ranges = std::move(ranges)](const QByteArray &) { return ranges; });
}
void ExportPlan::compose(const QString &later, const QString &earlier, RangeProvider ranges) {
    compositions_.append({later, earlier, std::move(ranges)});
}
void ExportPlan::derived(Ranges ranges) {
    for (auto range : ranges) RomWriter::validateRange(range);
    derived_ = std::move(ranges);
}
void ExportPlan::validate() const {
    QMap<QString, int> indices;
    for (int i = 0; i < steps_.size(); ++i) {
        const auto &step = steps_[i];
        if (step.name.isEmpty() || !step.prepare || indices.contains(step.name))
            throw Error("Duplicate or invalid export step: " + step.name);
        indices.insert(step.name, i);
    }
    QMap<QString, QSet<QString>> ancestors;
    for (int i = 0; i < steps_.size(); ++i) {
        const auto &step = steps_[i];
        for (const auto &dependency : step.prerequisites) {
            if (!indices.contains(dependency))
                throw Error(step.name + " requires missing export step " + dependency);
            if (indices[dependency] >= i)
                throw Error(step.name + " must run after " + dependency + " (reversed dependency or cycle)");
            ancestors[step.name].insert(dependency);
            ancestors[step.name].unite(ancestors[dependency]);
        }
    }
    for (const auto &rule : compositions_)
        if (!indices.contains(rule.later) || !indices.contains(rule.earlier) ||
            !ancestors[rule.later].contains(rule.earlier) || !rule.ranges)
            throw Error("Export composition requires an earlier dependency: " + rule.later + " after " + rule.earlier);
}
ExportPlan::Ranges ExportPlan::changedRanges(const QByteArray &before, const QByteArray &after) {
    return RomWriter::changedRanges(before,after);
}
QByteArray ExportPlan::run(QJsonArray *inventory) {
    validate();
    RomWriter writer(original_);
    writer.derived(derived_);
    changedSteps_=0;
    bool restored=false;
    try {
        for(const auto &step:steps_) {
            if(!step.restoration && !restored) { writer.finishRestoration(); restored=true; }
            for(const auto &rule:compositions_) if(rule.later==step.name)
                writer.allow(rule.later,rule.earlier,rule.ranges(writer.bytes()));
            const auto changes=writer.changes().size();
            writer.edit(step.name,step.prepare,step.owners);
            if (inventory) {
                auto describe = [](const Ranges &ranges) {
                    QJsonArray rows;
                    for (const auto &range : ranges)
                        rows.append(QJsonObject{{"offset", range.begin},
                                                {"size", range.end - range.begin}});
                    return rows;
                };
                inventory->append(
                    QJsonObject{{"name", step.name},
                                {"restoration", step.restoration},
                                {"requires", QJsonArray::fromStringList(step.prerequisites)},
                                {"declaredRanges", describe(step.owners)},
                                {"writtenRanges", writer.changes().size() > changes
                                                      ? describe(writer.changes().last().ranges)
                                                      : QJsonArray{}}});
            }
            if(!step.restoration && writer.changes().size()!=changes) ++changedSteps_;
        }
        image_=writer.bytes();
        return image_;
    } catch(...) {
        image_=original_;
        changedSteps_=0;
        throw;
    }
}
}
