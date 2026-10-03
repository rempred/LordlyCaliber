#include "rebuild/class_menu_layout.h"
#include <QFile>
#include <QRegularExpression>

namespace lordly {
ClassMenuLayout ClassMenuLayout::build(const ClassDataLayout &classes,
                                       const PromotionData &promotion) {
    const auto count = classes.count;
    auto inRam = [](quint32 address, quint64 length) {
        return address >= 0x80000000U && address < 0x80400000U && length <= 0x80400000ULL - address;
    };
    if (count < 165 || count > 256 || count != promotion.classCount || (classes.records & 3) ||
        !inRam(classes.records, count * 72) || !inRam(classes.aliases, count) ||
        !inRam(classes.discovery, 32) || classes.aliases < quint64(classes.records) + count * 72 ||
        classes.discovery < quint64(classes.aliases) + count ||
        promotion.familyOffset != count * 8 || promotion.maskOffset != count * 12 ||
        promotion.flagsOffset != count * 13 || promotion.catalogOffset != ((count * 14 + 3) & ~3) ||
        promotion.bytes.size() < promotion.catalogOffset + 8)
        throw Error("Class menu tables disagree on their count or 4 MB data layout");

    ClassMenuLayout result;
    result.count = count;
    result.capacity = count + 1;
    quint32 cursor = 0x2528;
    auto allocate = [&] {
        const auto offset = cursor;
        cursor = (cursor + result.capacity * 2 + 15) & ~15U;
        return offset;
    };
    result.list = allocate();
    result.categoryClasses = allocate();
    result.currentCounts = allocate();
    result.eligibleCounts = allocate();
    result.ownerBytes = cursor;
    // All these fields are encoded as signed 16-bit owner-relative operands.
    if (cursor > 0x7fff)
        throw Error("Class menu buffers exceed the Army owner addressing range");
    result.dataSymbols = {{"lc_class_records", classes.records},
                          {"lc_named_class_map", classes.aliases}};
    const QMap<QString, quint32> overrides = {
        {"LC_CLASS_COUNT", quint32(count)},
        {"LC_RECORD_COUNT", quint32(count)},
        {"LC_DISCOVERY_SCAN_COUNT", quint32(count)},
        {"LC_LIST_CAPACITY", quint32(result.capacity)},
        {"LC_DISABLED_FLAG", 0x8000},
        {"LC_FAMILY_OFFSET", quint32(promotion.familyOffset)},
        {"LC_DISCOVERY_ADDRESS", classes.discovery},
        {"LC_LIST_OFFSET", result.list},
        {"LC_MASK_RESOURCE_OFFSET", quint32(promotion.maskOffset)},
        {"LC_FLAGS_RESOURCE_OFFSET", quint32(promotion.flagsOffset)},
        {"LC_CATEGORY_CLASSES_OFFSET", result.categoryClasses},
        {"LC_CATEGORY_CURRENT_OFFSET", result.currentCounts},
        {"LC_CATEGORY_ELIGIBLE_OFFSET", result.eligibleCounts},
        {"LC_CATEGORY_CAPACITY", quint32(result.capacity)}};
    QFile file(":/game/promotion/class_menu_config.h");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing class menu source configuration");
    auto header = QString::fromLatin1(file.readAll());
    header.replace("\r\n", "\n");
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        const QRegularExpression pattern("^#define " + it.key() + " [^\\r\\n]+$",
                                         QRegularExpression::MultilineOption);
        if (header.count(pattern) != 1)
            throw Error("Class menu source configuration is missing or duplicates " + it.key());
        header.replace(pattern, "#define " + it.key() + " " + QString::number(it.value()) + "U");
    }
    result.cHeader = header.toLatin1();
    const QMap<QString, quint32> assembly = {{"CLASS_COUNT", quint32(count)},
                                             {"FAMILY_OFFSET", quint32(promotion.familyOffset)},
                                             {"CLASS_LIST_OFFSET", result.list},
                                             {"CLASS_ID_MASK", 255},
                                             {"WIDE_CLASSES", 1},
                                             {"CATEGORY_CLASSES_OFFSET", result.categoryClasses},
                                             {"CATEGORY_CURRENT_OFFSET", result.currentCounts},
                                             {"CATEGORY_ELIGIBLE_OFFSET", result.eligibleCounts},
                                             {"ARMY_OWNER_BYTES", result.ownerBytes}};
    for (auto it = assembly.begin(); it != assembly.end(); ++it)
        result.assemblyConstants +=
            it.key().toLatin1() + " = " + QByteArray::number(it.value()) + '\n';
    return result;
}
} // namespace lordly
