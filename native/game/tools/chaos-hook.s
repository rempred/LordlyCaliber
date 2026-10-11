# Resident task-submit call site. CF_RETAIL restores the original call.
.if CF_RETAIL
    jal CF_SUBMIT_TASK
.else
    j CF_BOOTSTRAP
.endif
    addiu $a1,$zero,0x40
