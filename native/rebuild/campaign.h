#pragma once
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildCampaignRuntime(const Project &, const QByteArray &, const Compiler &,
                                  const QString &);
}
