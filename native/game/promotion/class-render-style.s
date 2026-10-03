# Inline Army class drawing at ROM A5668..A56B4. Class rows use either the
# retail byte or the expanded halfword. Other renderer row types are unchanged.
.if WIDE_CLASSES
lhu $a0, 0($s0)
.else
lbu $a0, 0($s0)
.endif
lhu $v1, 0x5e8($t0)
.if WIDE_CLASSES
andi $v0, $a0, 255
.else
andi $v0, $a0, 127
.endif
bne $v0, $v1, class_not_selected
.if WIDE_CLASSES
srl $v0, $a0, 15
.else
srl $v0, $a0, 7
.endif
subu $v0, $zero, $v0
j class_style_ready
andi $v0, $v0, 5
class_not_selected:
subu $v0, $zero, $v0
andi $v0, $v0, 6
ori $v0, $v0, 4
class_style_ready:
sb $v0, 0x5e4($t0)
lui $v0, 0x8019
lw $v0, 0x6af8($v0)
jal CLASS_DRAW_STYLE
lbu $a0, 0x5e4($v0)
.if WIDE_CLASSES
lhu $a0, 0($s0)
.else
lbu $a0, 0($s0)
.endif
jal DRAW_CLASS_SPRITE
.if WIDE_CLASSES
andi $a0, $a0, 255
.else
andi $a0, $a0, 127
.endif
