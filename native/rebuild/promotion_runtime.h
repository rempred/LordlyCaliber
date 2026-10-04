#pragma once
#include "engine/expanded_promotion_resource.h"
#include "rebuild/class_menu_layout.h"
#include "rebuild/compiler.h"

namespace lordly {
struct PromotionRuntimeBuild {
    ClassMenuLayout layout;
    ClassRuntimeInventory inventory;
    QMap<quint32, QByteArray> owners;
    QByteArray container;
};
// Compiles the menu owners. The normal exporter installs this together with
// class data, description routing and save support.
PromotionRuntimeBuild buildPromotionRuntime(const Rom &source, const ClassDataLayout &classes,
                                            const Promotion &promotion, const Compiler &compiler,
                                            const QString &directory);
} // namespace lordly
