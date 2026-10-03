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
// Internal build component. This alone does not install the remaining class
// consumers or save support, and cannot be used as a public ROM export.
PromotionRuntimeBuild buildPromotionRuntime(const Rom &source, const ClassDataLayout &classes,
                                            const Promotion &promotion, const Compiler &compiler,
                                            const QString &directory);
} // namespace lordly
