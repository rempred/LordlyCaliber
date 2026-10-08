#pragma once
#include "engine/artwork_edits.h"
#include "engine/rom.h"
#include "engine/sprite_object.h"
#include <QImage>
#include <QJsonObject>
namespace lordly {
struct CombatArtBinding {
    QString id;
    quint32 descriptorKey = 0, resourceKey = 0, memberOffset = 0;
    int art = 0, policy = 0, paletteBank = 0;
    QVector<int> handles;
    QByteArray palette;
};
// Edits belong to a descriptor member. Shared physical resources are detached
// through that member on export, using the game's existing resource loader.
class CombatArtwork {
  public:
    static constexpr quint32 resourceBase = 0x594280;
    static constexpr int descriptorOffset = 0x27c4b00, descriptorSize = 96;
    static CombatArtwork load(const Rom &);
    const QMap<QString, CombatArtBinding> &bindings() const {
        return bindings_;
    }
    const CombatArtBinding &binding(const QString &) const;
    SpriteObject original(const QString &) const;
    SpriteObject current(const QString &) const;
    SpritePixels pixels(const QString &, int child) const;
    QByteArray palette(const QString &, int child) const;
    QImage image(const QString &, int child) const;
    SpritePixels mapImage(const QString &, int child, const QImage &,
                          bool orderedDither = false) const;
    static SpritePixels mapSpriteImage(const SpriteObject &, int child, const QByteArray &palette,
                                       const QImage &, bool orderedDither = false);
    void setPixels(const QString &, int child, const SpritePixels &);
    void restore(const QString &, int child);
    bool modified() const {
        return !edits_.isEmpty();
    }
    bool modified(const QString &id, int child) const {
        return edits_.value(id).contains(child);
    }
    QJsonObject json() const;
    void apply(const QJsonObject &);
    void importLegacy(const QJsonObject &);
    QMap<QString, QByteArray> decodedResources() const;
    ActorArtworkEdits previewEdits() const;
    QByteArray rebuild(const Rom &, const QMap<QString, QByteArray> &) const;
    static QString identity(quint32 descriptor, int art);

  private:
    QByteArray source_, descriptor_;
    QMap<QString, CombatArtBinding> bindings_;
    QMap<QString, QMap<int, SpritePixels>> edits_;
    mutable QMap<quint32, SpriteObject> cache_;
    quint32 allocationOffset_ = 0, allocationSize_ = 0;
    QMap<QString, QByteArray> owned_;
};
} // namespace lordly
