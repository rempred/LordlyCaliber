#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QVector>
#include <array>
namespace lordly {
// Class-indexed sections in the native compound animation resource. Opaque
// fields are copied with their class without assigning unverified meanings.
class AnimationRoutes {
  public:
    using Row = std::array<quint16, 4>;
    struct Entry {
        Row handles{};
        std::array<quint8, 2> speeds{}, attributes{};
        quint8 kind = 0;
        QByteArray selectors; // Empty: unavailable; otherwise 4-byte records + FF.
        bool operator==(const Entry &) const = default;
    };
    static constexpr quint32 resourceBase = 0x594280, retailKey = 0x315736;
    static constexpr int descriptorOffset = 0x27c4c00, descriptorSize = 96;
    static const std::array<QPair<int, int>, 8> &sites();
    static const std::array<QPair<int, int>, 2> &copySites();
    static AnimationRoutes load(const Rom &);
    static quint32 resourceKey(const Rom &);
    static QByteArray resourceBytes(const Rom &);
    static QByteArray canonicalOwner(QByteArray, qsizetype start);
    static void validateRelocations(const Rom &);
    Entry entry(int classId) const;
    Row row(int classId) const {
        return entry(classId).handles;
    }
    void setEntry(int classId, const Entry &);
    void setRow(int classId, const Row &);
    void cloneClass(int source, int target) {
        setEntry(target, entry(source));
    }
    void restore(int classId);
    int rowCount() const;
    bool modified() const {
        return !edits_.isEmpty() || !pending_.isEmpty();
    }
    void requireAssignment(int id);
    const QSet<int> &pending() const {
        return pending_;
    }
    const QMap<int, Entry> &edits() const {
        return edits_;
    }
    QJsonObject json() const;
    void apply(const QJsonObject &);
    QByteArray decodedResource() const;
    // Unchanged sections use the original header's physical extent.
    QByteArray section(int index) const;
    QByteArray rebuild(const Rom &, const QByteArray &compiled,
                       const QByteArray &compiledCopyLoads) const;

  private:
    static QVector<Entry> parseEntries(const QByteArray &);
    static QByteArray pack(const QByteArray &prefix, const QVector<Entry> &);
    QByteArray source_, prefix_, descriptor_, root_;
    QVector<Entry> rows_;
    QMap<int, Entry> edits_;
    QSet<int> pending_;
    QMap<int, quint32> ownerWords_;
    quint32 allocationOffset_ = 0, allocationSize_ = 0;
};
} // namespace lordly
