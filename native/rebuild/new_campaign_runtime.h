#pragma once
#include "rebuild/compiler.h"
namespace lordly {
struct NewCampaignInput {
    QJsonObject settings;
    QMap<int,QByteArray> armies;
    QPoint location;
};
NewCampaignInput prepareNewCampaign(const Project &);
CompiledModule buildNewCampaignRuntime(const NewCampaignInput &, const Compiler &,
    const QString &directory, quint32 address,
    const std::function<quint32(quint32)> &relocate, quint32 customFlags);
void linkNewCampaignRuntime(QByteArray &, quint32 bridge, const Rom &source);
}
