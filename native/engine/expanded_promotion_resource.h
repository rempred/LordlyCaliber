#pragma once
#include "engine/class_runtime_inventory.h"
#include "engine/promotion.h"

namespace lordly {
struct ExpandedPromotionResource {
    PromotionData data;
    ClassRuntimeInventory inventory;
    static constexpr quint32 romOffset = 0x27d0000, slotSize = 65536;
    static QByteArray encode(const Promotion &promotion, const QByteArray &compiled,
                             const ClassRuntimeInventory &inventory);
    static ExpandedPromotionResource decode(const QByteArray &slot);
};
} // namespace lordly
