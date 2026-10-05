#pragma once
#include "engine/avatar_library.h"
#include "engine/class_data.h"
namespace lordly {
// Per-class selectors are detached only at export; other users of a bundle retain it.
class AvatarArtwork {
  public:
    static AvatarArtwork load(const Rom &);
    QByteArray words(const ClassData &, int classId, int selector) const;
    QImage image(const ClassData &, int classId, int selector) const;
    void setWords(const ClassData &, int classId, int selector, const QByteArray &);
    void restore(int classId, int selector);
    bool modified() const {
        return !edits_.isEmpty();
    }
    bool modified(int id, int selector) const {
        return edits_.contains(id * 2 + selector);
    }
    QJsonObject json() const;
    void apply(const ClassData &, const QJsonObject &);
    void importLegacy(const ClassData &, const QJsonObject &);
    QMap<int, QByteArray> decodedBundles() const;
    QByteArray rebuild(const Rom &, ClassData &, const QMap<int, QByteArray> &compiled) const;
    static QByteArray pack(const QByteArray &words);
    static QImage render(const QByteArray &words);
    static QByteArray mapImage(const QImage &, bool reduceColors = false,
                               QColor background = Qt::black);

  private:
    AvatarLibrary library_;
    QMap<int, QByteArray> edits_;
    QMap<int, quint32> ownerWords_;
    QByteArray descriptor_;
};
} // namespace lordly
