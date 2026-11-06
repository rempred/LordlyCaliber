#pragma once
#include <QByteArray>
#include <QMap>
#include <QSet>
#include <QString>
#include <array>

namespace lordly::EventHelperNames {
enum class Kind { Unknown, IdentityFamily, AngelUndeadClass, LichVampireClass };

struct Helper {
    Kind kind = Kind::Unknown;
    int entry = -1, byteLength = 0;
    // Each recognized body restores its saved values and changes only R0.
    quint8 preservedRegisters = 0;
    bool known() const { return kind != Kind::Unknown; }
};

// These are complete, position-independent event programs, including every
// branch, saved register and final return. Sources: reviewed retail row10
// @1190/1286/1328; native interpreter proof in the parent repository's
// runs/all-event-semantics-20261103/late-a/helper-probes.cjs and its results.
// Identity equivalence is equality or membership in one of the five groups
// {81,82,83}, {84,85}, {86,87}, {95,96}, {99,100}. It is not a roster scan.
inline const std::array<QByteArray,3> &bodies() {
    static const std::array<QByteArray,3> value{
        QByteArray::fromHex(
            "05010502050360046305a32569516a53d120c203c71edb01011e"
            "69546a55d119c203c717db01011769566a57d112c203c710db010110"
            "695f6a60d10bc203c709db01010969636a64d104c203c702db010102"
            "6800010168010603060206010400"),
        QByteArray::fromHex(
            "0501050261046a2da60c6a2ea60a6a2fa6086a24a6066a25a604"
            "6a30a602680001016801060206010400"),
        QByteArray::fromHex(
            "0501050261046a26a6066a2ba6046a2ca602680001016801060206010400")
    };
    return value;
}

// Use current bytes, not a retail row number or a stale helper destination.
// Every word must still belong to the decoded instruction inventory. A call
// to the middle of a helper, a truncated body or any changed byte stays unknown.
inline Helper match(const QByteArray &bytes, const QSet<int> &offsets, int entry) {
    if(entry < 0 || (entry & 1))return {};
    const auto &programs=bodies();
    for(int n=0;n<int(programs.size());++n){
        const auto &body=programs[n];
        if(entry>bytes.size() || body.size()>bytes.size()-entry || bytes.mid(entry,body.size())!=body)continue;
        bool owned=true;for(int at=entry;at<entry+body.size();at+=2)if(!offsets.contains(at)){owned=false;break;}
        if(owned)return {static_cast<Kind>(n+1),entry,int(body.size()),0xfe};
    }
    return {};
}

// Whole-routine presentation may hide internal branch nodes only when each
// incoming edge to an interior word originates in this same routine. Include
// handler roots as incoming source -1. The entry itself remains public, so any
// number of external calls or jumps to it are fine. The caller must supply the
// current graph and refuse collapse when a dynamic transfer is unresolved.
// A body can still have a valid call contract even if this presentation check
// fails: do not use this as an additional requirement for match().
inline bool hasOnlyInternalEntrances(const Helper &helper, const QMap<int,QSet<int>> &incoming) {
    if(!helper.known())return false;
    const int end=helper.entry+helper.byteLength;
    for(auto it=incoming.upperBound(helper.entry);it!=incoming.cend()&&it.key()<end;++it)
        for(int from:it.value())if(from<helper.entry||from>=end)return false;
    return true;
}

inline QString name(Kind kind) {
    switch(kind){
    case Kind::IdentityFamily:return "Check character identity, including equivalent class variants";
    case Kind::AngelUndeadClass:return "Check for Angel Knight, Seraph, Zombie, Skeleton or Ghost class";
    case Kind::LichVampireClass:return "Check for Lich or Vampire class";
    default:return {};
    }
}

inline QString detail(Kind kind) {
    switch(kind){
    case Kind::IdentityFamily:return "Compares the low bytes of the candidate and requested identities. Returns true for equal identities or supported variants in the same identity family.";
    case Kind::AngelUndeadClass:return "Tests the low byte of the selected class against Angel Knight, Seraph, both Zombie classes, Skeleton and Ghost. A true result means the class is in this list.";
    case Kind::LichVampireClass:return "Tests the low byte of the selected class against Lich and both Vampire classes. A true result means the class is in this list.";
    default:return {};
    }
}

// Text for the Boolean returned in R0. Callers supply expressions derived from
// the actual incoming R4/R5 values and retain their editable source fields.
// Missing inputs do not acquire a guessed character or a retail story outcome.
inline QString predicate(Kind kind, const QString &candidate, const QString &requested={}) {
    if(candidate.isEmpty())return {};
    switch(kind){
    case Kind::IdentityFamily:
        if(requested.isEmpty())return {};
        return QString("%1 matches %2 (including equivalent class variants)").arg(candidate,requested);
    case Kind::AngelUndeadClass:
        return candidate+" is Angel Knight, Seraph, Zombie, Skeleton or Ghost";
    case Kind::LichVampireClass:return candidate+" is Lich or Vampire";
    default:return {};
    }
}
}
