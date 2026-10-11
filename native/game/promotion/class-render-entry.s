# Inline replacement for Army ROM A4D5C..A4ED8. The enclosing renderer already
# saved ra and owns s6 (the matrix buffer). All callee-saved registers survive
# the C call; the following renderer blocks initialize their own scratch values.
jal CLASS_RENDER_ROWS
move $a0, $s6
j CLASS_RENDER_CONTINUE
nop
