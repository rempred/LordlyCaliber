# Two instruction fragments, assembled during export and installed separately.
# Consumers at z64 6E810 and 1B902C originally copy header[11] bytes. That value
# also addresses another live section, so it must remain unchanged. Read the
# complete resident size from header[15], as the combat loader already does.
# $s0 is the decoded compound resource; a0 is allocator size, a2 is copy size.
lhu $a0, 0x1e($s0)
lhu $a2, 0x1e($s0)
