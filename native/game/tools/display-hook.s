# Replay the native delay slot for both the original and rebuilt call.
.if TOOL_RETAIL
    jal TOOL_NATIVE_DRAW
.else
    jal TOOL_BOOTSTRAP
.endif
.if TOOL_ALIGNMENT
    sw $s4,0x18($sp)
.else
    addiu $a1,$a1,0x27a8
.endif
