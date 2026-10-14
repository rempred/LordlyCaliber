#pragma once
#include "engine/rom.h"
#include <QCryptographicHash>
#include <QMap>
#include <QSet>
namespace lordly {
// Original bytes remain an opaque prefix. Only blocks we generated and signed
// are eligible for relocation or collection. The footer is not game geometry.
class FieldModelStorage {
    struct Block { int at, size, kind; };
    QMap<int, Block> blocks_;
    int base_;
    QVector<int> references(const Block &b) const {
        QVector<int> result;
        if (b.kind == 1) {
            for (int at = b.at; at + 8 <= b.at + b.size; at += 8) {
                const auto op = u8(bytes, at);
                if ((op == 1 || op == 0xda || op == 0xde || op == 0xfd) && u8(bytes, at + 4) == 2)
                    result << at + 4;
            }
        } else if (b.kind == 2) {
            constexpr int widths[] = {0,1,1,1,1,1,1,1,0,0,3,3,1,1,0,1,1,1,2,2,3,1,2,2,3,2};
            for (int at = b.at; at < b.at + b.size;) {
                const auto op = u32(bytes, at); at += 4;
                if (!op) break;
                if (op > 25 || at + widths[op] * 4 > b.at + b.size) throw Error("Invalid owned map program");
                if (op == 1 || op == 18 || op == 19 || op == 20 || op == 22 || op == 23 || op == 24)
                    result << at;
                at += widths[op] * 4;
            }
        }
        return result;
    }
    int owner(int at) const {
        if (at < base_) return -1;
        auto i = blocks_.upperBound(at);
        if (i == blocks_.cbegin()) throw Error("Map reference has no owned allocation");
        --i;
        if (at >= i->at + i->size) throw Error("Map reference exceeds owned allocation");
        return i.key();
    }
  public:
    QByteArray bytes;
    int geometry;
    FieldModelStorage(const QByteArray &original, int start) : base_(int(original.size())), bytes(original), geometry(start) {
        if (original.size() < 48 || original.mid(original.size() - 48, 8) != "LCMODEL1") return;
        const int footer = int(original.size()) - 48;
        const quint32 base = u32(original, footer + 8), count = u32(original, footer + 12);
        if (base > quint32(footer) || count > 65536 || quint64(count) * 16 > quint32(footer) - base ||
            QCryptographicHash::hash(original.first(original.size() - 32), QCryptographicHash::Sha256) != original.last(32))
            throw Error("Edited map ownership footer is damaged");
        const int table = footer - int(count * 16);
        int end = int(base);
        for (quint32 n = 0; n < count; ++n) {
            const int at = table + int(n * 16);
            const quint32 pos = u32(original, at), size = u32(original, at + 4), kind = u32(original, at + 8);
            if (pos < quint32(end) || !size || quint64(pos) + size > quint32(table) || kind > 2 || u32(original, at + 12))
                throw Error("Invalid edited map allocation table");
            blocks_[int(pos)] = {int(pos), int(size), int(kind)}; end = int(pos + size);
        }
        base_ = int(base); bytes.truncate(table);
    }
    quint32 add(const QByteArray &data, int alignment = 8, int kind = 0) {
        while (bytes.size() % alignment) bytes += char(0);
        const int at = int(bytes.size());
        if (data.isEmpty() || at + data.size() > 2 * 1024 * 1024) throw Error("Map edit exceeds working detail capacity");
        blocks_[at] = {at, int(data.size()), kind}; bytes += data;
        return 0x02000000u + quint32(at - geometry);
    }
    quint32 list(const QByteArray &data) { return add(data, 8, 1); }
    // Collapse previously generated load/call/restore wrappers before wrapping
    // the moved building again. Absolute transforms make the inner load decisive.
    void unwrap(quint32 &matrix, quint32 &list) const {
        QSet<quint32> seen;
        while (!seen.contains(list)) {
            seen.insert(list);
            const int at = geometry + int(list & 0xffffff);
            const auto b = blocks_.constFind(at);
            if (b == blocks_.cend() || b->kind != 1 || b->size != 32 ||
                u32(bytes, at) != 0xda380003 || u32(bytes, at + 8) != 0xde000000 ||
                u32(bytes, at + 16) != 0xda380003 || u32(bytes, at + 24) != 0xdf000000) break;
            matrix = u32(bytes, at + 4); list = u32(bytes, at + 12);
        }
    }
    QByteArray finish(quint32 root) {
        QSet<int> live;
        QVector<int> pending{geometry + int(root & 0xffffff)};
        while (!pending.isEmpty()) {
            const int block = owner(pending.takeLast());
            if (block < 0 || live.contains(block)) continue;
            live.insert(block);
            for (int ref : references(blocks_[block])) {
                const auto pointer = u32(bytes, ref);
                if (pointer >> 24 == 2) pending << geometry + int(pointer & 0xffffff);
            }
        }
        QByteArray compact = bytes.first(base_);
        QMap<int, int> moved;
        QMap<int, Block> kept;
        for (const auto &b : blocks_) if (live.contains(b.at)) {
            while (compact.size() % 16) compact += char(0);
            const int at = int(compact.size()); moved[b.at] = at;
            kept[at] = {at, b.size, b.kind}; compact += bytes.mid(b.at, b.size);
        }
        auto relocate = [&](quint32 pointer) {
            if (pointer >> 24 != 2) return pointer;
            const int old = geometry + int(pointer & 0xffffff), block = owner(old);
            return block < 0 ? pointer : quint32(0x02000000 + moved.value(block) + old - block - geometry);
        };
        for (const auto &b : blocks_) if (live.contains(b.at))
            for (int ref : references(b)) put32(compact, moved[b.at] + ref - b.at, relocate(u32(bytes, ref)));
        put32(compact, 0, (relocate(root) & 0xffffff) + geometry);
        for (const auto &b : kept) {
            const int at = int(compact.size()); compact += QByteArray(16, 0);
            put32(compact, at, b.at); put32(compact, at + 4, b.size); put32(compact, at + 8, b.kind);
        }
        const int footer = int(compact.size()); compact += QByteArray("LCMODEL1", 8) + QByteArray(8, 0);
        put32(compact, footer + 8, base_); put32(compact, footer + 12, quint32(kept.size()));
        compact += QCryptographicHash::hash(compact, QCryptographicHash::Sha256);
        if (compact.size() > 512 * 1024) throw Error("This map has reached its detail capacity (512 KiB); simplify or erase some strokes");
        return compact;
    }
};
}
