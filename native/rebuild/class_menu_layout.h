#pragma once
#include "engine/class_data.h"
#include "engine/promotion.h"

namespace lordly {
// Expanded Army buffers are additional owner storage. The original screen
// union stays intact for non-class lists and unrelated controls.
struct ClassMenuLayout {
    int count = 0, capacity = 0;
    quint32 list = 0, categoryClasses = 0, currentCounts = 0, eligibleCounts = 0;
    quint32 ownerBytes = 0;
    QByteArray cHeader, assemblyConstants;
    QMap<QString, quint32> dataSymbols;
    static ClassMenuLayout build(const ClassDataLayout &classes, const PromotionData &promotion);
};
} // namespace lordly
