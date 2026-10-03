/* Retail class roster and Army overlay layout (US Rev 0).
 * Expanded rosters require a separately validated layout and consumer format.
 */
#define LC_CLASS_COUNT 81
#define LC_RECORD_COUNT 165
/* The retail discovery walk also visits the terminal row. */
#define LC_DISCOVERY_SCAN_COUNT 82
#define LC_LIST_CAPACITY 82
#define LC_DISABLED_FLAG 128
#define LC_OWNER_POINTER 0x80196AF8U
#define LC_FAMILY_OFFSET 648
#define LC_DISCOVERY_ADDRESS 0x80196A76U
#define LC_BIT_MASK_ADDRESS 0x801EF370U
#define LC_LIST_OFFSET 0x1C0E
#define LC_CHECK_CHANGE 0x801D2358U
/* Four-byte draw records precede the count at owner+0x5C8. */
#define LC_DRAW_ROW_CAPACITY 167
#define LC_MASK_RESOURCE_OFFSET -1
#define LC_MASK_ADDRESS 0x801EF2CDU
#define LC_MASK_STRIDE 2
#define LC_FLAGS_RESOURCE_OFFSET -1
#define LC_FLAGS_ADDRESS 0x801EF2CCU
#define LC_FLAGS_STRIDE 2
#define LC_CATEGORY_CLASSES_OFFSET 0x1BEE
#define LC_CATEGORY_CURRENT_OFFSET 0x1C2E
#define LC_CATEGORY_ELIGIBLE_OFFSET 0x1C6E
#define LC_CATEGORY_CAPACITY 32
