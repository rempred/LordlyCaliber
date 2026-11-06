#pragma once
#include "engine/rom.h"
#include <QJsonArray>
#include <QJsonObject>
namespace lordly {
class ConsumableEffects {
  public:
    static constexpr quint32 romOffset = 0x41098, ramAddress = 0x8016b198, codeSize = 1380;
    static ConsumableEffects load(const Rom &rom);
    static QJsonObject catalog();
    static QJsonObject runtimeOwner();
    static QJsonArray definitions();
    static QMap<QString, quint32> serviceAddresses();
    const QJsonObject &models() const {
        return models_;
    }
    QJsonObject json() const;
    bool modified() const {
        return models_ != original_ || trees_ != originalTrees_ || visuals_ != originalVisuals_;
    }
    void setModel(const QString &key, const QJsonObject &values);
    void apply(const QJsonObject &object);
    void importLegacy(const QJsonObject &object, int version);
    QByteArray assemblyDefinitions() const;
    QByteArray expectedCode(const QByteArray &source) const;
    bool needsCBuild() const;
    QByteArray runtimeImage(const QByteArray &code) const;
    QJsonArray tree(int item) const;
    void setTree(int item, const QJsonArray &tree);
    void restoreTree(int item);
    bool hasTrees() const { return !trees_.isEmpty(); }
    const QJsonObject &trees() const { return trees_; }
    const QJsonObject &visuals() const { return visuals_; }
    bool hasVisuals() const { return !visuals_.isEmpty(); }
    void setVisual(int item, const QJsonObject &);
    void restoreVisual(int item) { visuals_.remove(QString::number(item)); }

  private:
    static QJsonObject validate(const QJsonObject &definition, const QJsonObject &values);
    QJsonObject models_, original_;
    QJsonObject trees_, originalTrees_;
    QJsonObject visuals_, originalVisuals_;
    bool cRuntime_ = false;
};
} // namespace lordly
