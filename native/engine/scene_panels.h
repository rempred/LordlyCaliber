#pragma once
#include "engine/rom.h"
#include "engine/item_icons.h"
#include <QImage>
#include <QVector>
#include <array>
#include <optional>
namespace lordly {
// Map-menu owner pool and shared animated entities. Host identities replace RAM
// pointers; text pointers are offsets into the retained payload.
class ScenePanels {
  public:
    struct Entity {
        quint64 identity = 0, parent = 0;
        QByteArray record, payload;
        std::optional<quint16> reward;
    };
    explicit ScenePanels(Rom rom) : rom_(rom), icons_(std::move(rom)) {}
    void create(int slot, int preset, int width, int height);
    static QString rewardName(const Rom &, quint16 item);
    static QString confirmationChoice(const Rom &, int index);
    void supplyReward(quint16 item) {
        rewardName(rom_, item);
        reward_ = item;
    }
    const QByteArray &ownerData(int slot) const;
    void release(int slot);
    int query(int slot) const;
    void advance(quint16 action, quint16 direction);
    const QVector<Entity> &entities() const {
        return entities_;
    }
    const QVector<int> &audioRequests() const {
        return audio_;
    }
    bool empty() const {
        return entities_.isEmpty();
    }
    // Desktop frame/glyph rasterization is approximate; layout/state is native.
    QImage render() const;

  private:
    struct Owner {
        quint32 selector;
        quint64 entity;
        int status = 0;
        QByteArray data;
    };
    QByteArray resource(quint32 key) const;
    QByteArray textPayload(quint32 selector, QByteArray &entity) const;
    void child(Entity &parent);
    Rom rom_;
    ItemIcons icons_;
    std::array<std::optional<Owner>, 14> owners_{};
    QVector<Entity> entities_;
    QVector<int> audio_;
    quint64 nextIdentity_ = 1;
    std::optional<quint16> reward_;
};
} // namespace lordly
