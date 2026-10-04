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
    static QJsonArray definitions();
    static QMap<QString, quint32> serviceAddresses();
    const QJsonObject &models() const {
        return models_;
    }
    QJsonObject json() const {
        return {{"version", 1}, {"models", models_}};
    }
    bool modified() const {
        return models_ != original_;
    }
    void setModel(const QString &key, const QJsonObject &values);
    void apply(const QJsonObject &object);
    void importLegacy(const QJsonObject &object, int version);
    QByteArray assemblyDefinitions() const;
    QByteArray expectedCode(const QByteArray &source) const;

  private:
    static QJsonObject validate(const QJsonObject &definition, const QJsonObject &values);
    QJsonObject models_, original_;
};
} // namespace lordly
