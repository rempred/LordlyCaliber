#pragma once
#include "engine/icon_artwork.h"
#include "engine/item_data.h"
#include "engine/item_icons.h"
#include "engine/rom.h"
#include "engine/scene_report_runtime.h"
#include <QImage>
#include <QVector>
#include <array>
#include <memory>
#include <optional>
namespace lordly {
class SceneReportArt;
class ScenePanelFont;
// Map-menu owner pool and shared animated entities. Host identities replace RAM
// pointers; text pointers are offsets into the retained payload.
class ScenePanels {
  public:
    struct Entity {
        quint64 identity = 0, parent = 0;
        QByteArray record, payload;
        std::optional<quint16> reward;
        QByteArray rewardLabel;
    };
    explicit ScenePanels(Rom rom) : rom_(rom), icons_(std::move(rom)) {}
    void create(int slot, int preset, int width, int height, std::optional<quint16> eventReward = {});
    void createArmy(int slot, int preset);
    void replaceArmyHelp(int slot, int section);
    void closeArmy(int slot, bool immediate);
    void advanceArmy(quint16 action, quint16 direction);
    void armyMenuLabel() {
        if (report_)
            report_->menuLabel();
        else if (armyHandshake_ >= 3)
            --armyHandshake_;
    }
    void supplyReportRuntime(const QByteArray &ram, const QByteArray &script) {
        report_.emplace(rom_, ram, script);
    }
    bool usesReportRuntime() const {
        return report_.has_value();
    }
    void supplyArmyNames(QByteArray subject, QByteArray selected) {
        subjectName_ = std::move(subject);
        selectedName_ = std::move(selected);
    }
    void supplyArmySubject(int subject, const QMap<int, QByteArray> &names, QByteArray selected);
    void supplyArmyFlag(bool flag) {
        armyFlag_ = flag;
    }
    void supplyReportClock(int hours, int minutes, int seconds);
    void setReportArtwork(const QByteArray &portrait);
    static QString rewardName(const Rom &, quint16 item);
    static QString confirmationChoice(const Rom &, int index);
    void setItems(ItemData data, IconArtwork icons) {
        items_ = std::move(data);
        artwork_ = std::move(icons);
    }
    void supplyReward(quint16 item) {
        currentRewardBytes(item);
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
    // The report uses ROM artwork. Other panel types use desktop frames and text.
    QImage render() const;

  private:
    struct Owner {
        quint32 selector;
        quint64 entity;
        int status = 0;
        QByteArray data;
    };
    QByteArray resource(quint32 key) const;
    QByteArray textPayload(quint32 selector, QByteArray &entity, bool replaceNames = false) const;
    void child(Entity &parent);
    Rom rom_;
    ItemIcons icons_;
    std::optional<ItemData> items_;
    std::optional<IconArtwork> artwork_;
    static QByteArray rewardBytes(const Rom &, quint16 item);
    QByteArray currentRewardBytes(quint16 item) const;
    std::array<std::optional<Owner>, 14> owners_{};
    QVector<Entity> entities_;
    QVector<int> audio_;
    quint64 nextIdentity_ = 1;
    std::optional<quint16> reward_;
    QByteArray subjectName_, selectedName_;
    QByteArray reportClock_ = "00:00:00";
    QImage reportBackground_, reportPortrait_;
    std::shared_ptr<const SceneReportArt> reportArt_;
    std::shared_ptr<const ScenePanelFont> rewardFont_;
    bool armyFlag_ = false;
    int armyHandshake_ = 3;
    std::optional<SceneReportRuntime> report_;
    void syncReport();
};
} // namespace lordly
