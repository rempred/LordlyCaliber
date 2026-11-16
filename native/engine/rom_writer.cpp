#include "engine/rom_writer.h"
#include "engine/rom.h"
#include <algorithm>
#include <cstring>

namespace lordly {
namespace {
bool intersects(ExportRange a, ExportRange b) {
    return a.begin < b.end && b.begin < a.end;
}
bool covered(ExportRange wanted, RomWriter::Ranges ranges) {
    std::sort(ranges.begin(), ranges.end(), [](auto a, auto b) { return a.begin < b.begin; });
    for (auto range : ranges) {
        if (range.begin > wanted.begin) break;
        if (range.end > wanted.begin) wanted.begin = range.end;
        if (wanted.begin >= wanted.end) return true;
    }
    return false;
}
RomWriter::Ranges merged(RomWriter::Ranges ranges) {
    std::sort(ranges.begin(), ranges.end(), [](auto a, auto b) { return a.begin < b.begin; });
    RomWriter::Ranges result;
    for (auto range : ranges) {
        if (!result.isEmpty() && result.last().end >= range.begin)
            result.last().end = std::max(result.last().end, range.end);
        else result.append(range);
    }
    return result;
}
RomWriter::Ranges subtract(const RomWriter::Ranges &source, const RomWriter::Ranges &removed) {
    RomWriter::Ranges result;
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
void RomWriter::validateRange(ExportRange range) {
    if (range.begin < 0 || range.end <= range.begin || range.end > 0x4000000)
        throw Error("Invalid export ownership range");
}
void RomWriter::allow(const QString &later, const QString &earlier, Ranges ranges) {
    if (later.isEmpty() || earlier.isEmpty()) throw Error("Unnamed ROM writer");
    for (auto range : ranges) validateRange(range);
    permissions_[later][earlier] += ranges;
}
void RomWriter::derived(Ranges ranges) {
    for (auto range : ranges) validateRange(range);
    derived_ = merged(std::move(ranges));
}
RomWriter::Ranges RomWriter::changedRanges(const QByteArray &before, const QByteArray &after) {
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
void RomWriter::apply(const QString &owner, QByteArray candidate, Ranges completeOwners) {
    auto writes = changedRanges(image_, candidate);
    commit(owner, std::move(candidate), std::move(writes), std::move(completeOwners));
}
void RomWriter::write(const QString &owner, qsizetype offset, const QByteArray &bytes) {
    if (bytes.isEmpty()) return;
    validateRange({offset,offset+bytes.size()});
    if (offset+bytes.size()>image_.size()) throw Error("ROM write exceeds its allocated image: " + owner);
    auto candidate=image_; candidate.replace(offset,bytes.size(),bytes);
    // Explicit writes claim the full requested span, even if some bytes agree.
    commit(owner,std::move(candidate),{{offset,offset+bytes.size()}},{});
}
void RomWriter::resize(const QString &owner, qsizetype size, char fill) {
    if(size<0 || size>0x4000000) throw Error("ROM allocation exceeds 64 MB: " + owner);
    auto candidate=image_;
    if(size>candidate.size()) candidate.append(QByteArray(size-candidate.size(),fill));
    else candidate.resize(size);
    apply(owner,std::move(candidate));
}
void RomWriter::edit(const QString &owner, const std::function<void(QByteArray &)> &prepare, Ranges completeOwners) {
    auto candidate=image_;
    prepare(candidate);
    apply(owner,std::move(candidate),std::move(completeOwners));
}
void RomWriter::finishRestoration() {
    if(restored_) throw Error("ROM restoration phase already ended");
    restored_=true;
    claims_.clear();
}
void RomWriter::commit(const QString &owner, QByteArray candidate, Ranges writes, Ranges completeOwners) {
    if(owner.isEmpty()) throw Error("Unnamed ROM writer");
    if(candidate.size()>0x4000000) throw Error(owner+" exceeds the 64 MB ROM address space");
    for(auto range:completeOwners) validateRange(range);
    writes=subtract(merged(std::move(writes)),derived_);
    auto owned=writes;
    for(auto range:completeOwners)
        if(std::any_of(writes.begin(),writes.end(),[&](auto write){return intersects(range,write);}))
            owned.append(range);
    owned=merged(std::move(owned));
    const auto permissions=permissions_.value(owner);
    for(const auto &prior:claims_) {
        if(prior.owner==owner) continue;
        qsizetype a=0,b=0;
        while(a<owned.size() && b<prior.ranges.size()) {
            const auto range=owned[a], previous=prior.ranges[b];
            if(intersects(range,previous)) {
                const ExportRange overlap{std::max(range.begin,previous.begin),std::min(range.end,previous.end)};
                if(!covered(overlap,permissions.value(prior.owner)))
                    throw Error(QString("Export conflict: %1 overwrites %2 at z64 0x%3..0x%4")
                        .arg(owner,prior.owner).arg(overlap.begin,0,16).arg(overlap.end,0,16));
            }
            if(range.end<=previous.end) ++a; else ++b;
        }
    }
    for(auto &prior:claims_) prior.ranges=subtract(prior.ranges,owned);
    if(!owned.isEmpty()) { claims_.append({owner,owned}); changes_.append({owner,writes}); }
    image_=std::move(candidate);
}
}
