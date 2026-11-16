#include "rebuild/export_plan.h"
#include "engine/rom.h"
#include <QMap>
#include <QSet>
#include <algorithm>
#include <cstring>

namespace lordly {
namespace {
bool intersects(ExportRange a, ExportRange b) {
    return a.begin < b.end && b.begin < a.end;
}
void validateRange(ExportRange range) {
    if (range.begin < 0 || range.end <= range.begin || range.end > 0x4000000)
        throw Error("Invalid export ownership range");
}
bool covered(ExportRange wanted, ExportPlan::Ranges ranges) {
    std::sort(ranges.begin(), ranges.end(), [](auto a, auto b) { return a.begin < b.begin; });
    for (auto range : ranges) {
        if (range.begin > wanted.begin) break;
        if (range.end > wanted.begin) wanted.begin = range.end;
        if (wanted.begin >= wanted.end) return true;
    }
    return false;
}
ExportPlan::Ranges merged(ExportPlan::Ranges ranges) {
    std::sort(ranges.begin(), ranges.end(), [](auto a, auto b) { return a.begin < b.begin; });
    ExportPlan::Ranges result;
    for (auto range : ranges) {
        if (!result.isEmpty() && result.last().end >= range.begin)
            result.last().end = std::max(result.last().end, range.end);
        else result.append(range);
    }
    return result;
}
ExportPlan::Ranges subtract(const ExportPlan::Ranges &source, const ExportPlan::Ranges &removed) {
    ExportPlan::Ranges result;
    qsizetype i = 0;
    for (auto range : source) {
        while (i < removed.size() && removed[i].end <= range.begin) ++i;
        for (qsizetype j = i; j < removed.size() && removed[j].begin < range.end; ++j) {
            if (removed[j].begin > range.begin) result.append({range.begin, removed[j].begin});
            range.begin = std::max(range.begin, removed[j].end);
            if (range.begin >= range.end) break;
        }
        if (range.begin < range.end) result.append(range);
    }
    return result;
}
}
void ExportPlan::add(QString name, QStringList prerequisites, Action prepare, Ranges owners) {
    for (auto owner : owners) validateRange(owner);
    steps_.append({std::move(name), std::move(prerequisites), std::move(prepare), std::move(owners)});
}
void ExportPlan::compose(const QString &later, const QString &earlier, Ranges ranges) {
    for (auto range : ranges) validateRange(range);
    compose(later, earlier, [ranges = std::move(ranges)](const QByteArray &) { return ranges; });
}
void ExportPlan::compose(const QString &later, const QString &earlier, RangeProvider ranges) {
    compositions_.append({later, earlier, std::move(ranges)});
}
void ExportPlan::derived(Ranges ranges) {
    for (auto range : ranges) validateRange(range);
    derived_ = merged(std::move(ranges));
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
    Ranges ranges;
    const qsizetype common = std::min(before.size(), after.size());
    qsizetype start = -1;
    for (qsizetype at = 0; at < common;) {
        // Most ROM pages remain identical. Skip those without per-byte work.
        constexpr qsizetype page = 4096;
        if (start < 0 && at + page <= common &&
            std::memcmp(before.constData() + at, after.constData() + at, page) == 0) {
            at += page;
            continue;
        }
        if (before[at] != after[at]) {
            if (start < 0) start = at;
        } else if (start >= 0) {
            ranges.append({start, at});
            start = -1;
        }
        ++at;
    }
    if (start >= 0) ranges.append({start, common});
    if (before.size() != after.size()) ranges.append({common, std::max(before.size(), after.size())});
    return ranges;
}
void ExportPlan::run() {
    validate(); // Fail bad plans before any builder or compiler runs.
    const auto original = image_;
    QVector<Claim> claims;
    changedSteps_ = 0;
    try {
        for (const auto &step : steps_) {
            const auto before = image_;
            QMap<QString, Ranges> permissions;
            for (const auto &rule : compositions_) if (rule.later == step.name) {
                const auto ranges = rule.ranges(before);
                for (auto range : ranges) validateRange(range);
                permissions[rule.earlier] += ranges;
            }
            step.prepare();
            if (image_.size() > 0x4000000)
                throw Error(step.name + " exceeds the 64 MB ROM address space");
            auto writes = changedRanges(before, image_);
            for (auto field : derived_) {
                Ranges retained;
                for (auto write : writes) {
                    if (!intersects(field, write)) retained.append(write);
                    else {
                        if (write.begin < field.begin) retained.append({write.begin, field.begin});
                        if (write.end > field.end) retained.append({field.end, write.end});
                    }
                }
                writes = std::move(retained);
            }
            if (writes.isEmpty()) continue;
            auto owned = writes;
            for (auto owner : step.owners)
                if (std::any_of(writes.begin(), writes.end(), [&](auto write) { return intersects(owner, write); }))
                    owned.append(owner);
            owned = merged(std::move(owned));
            for (const auto &prior : claims) {
                qsizetype a = 0, b = 0;
                while (a < owned.size() && b < prior.ranges.size()) {
                    const auto range = owned[a], previous = prior.ranges[b];
                    if (intersects(range, previous)) {
                        const ExportRange overlap{std::max(range.begin, previous.begin), std::min(range.end, previous.end)};
                        if (!covered(overlap, permissions.value(prior.owner)))
                            throw Error(QString("Export conflict: %1 overwrites %2 at z64 0x%3..0x%4")
                                .arg(step.name, prior.owner).arg(overlap.begin, 0, 16).arg(overlap.end, 0, 16));
                    }
                    if (range.end <= previous.end) ++a;
                    else ++b;
                }
            }
            // A verified composer becomes the current owner of those spans.
            // Do not make later writers negotiate with superseded producers.
            for (auto &prior : claims) prior.ranges = subtract(prior.ranges, owned);
            claims.append({step.name, owned});
            ++changedSteps_;
        }
    } catch (...) {
        image_ = original;
        changedSteps_ = 0;
        throw;
    }
}
}
