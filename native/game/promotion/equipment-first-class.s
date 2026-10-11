# Initial selection in the Army equipment-purchase screen (z64 CD638..CD640).
# Keep the surrounding purchase/UI function, but assemble its list contract.
.ifndef CLASS_LIST_OFFSET
CLASS_LIST_OFFSET = 0x1C0E
.endif
.ifndef CLASS_ID_MASK
CLASS_ID_MASK = 127
.endif
lhu $a0, CLASS_LIST_OFFSET($v0)
andi $a0, $a0, CLASS_ID_MASK
