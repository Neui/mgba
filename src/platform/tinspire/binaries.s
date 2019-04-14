# Just a assembler file to include binary content, since objcopy also puts
# the absolute path into the symbol, which is annoying and inconsistent.
# For now, abuse the assembler for that.
# Also, the .globals need to be before the symbol, for some reason.

.global ext_font_start
ext_font_start:
.incbin "../../../res/font.png"
.global ext_font_end
ext_font_end:
.global ext_font_size
.align 4
ext_font_size:
.int ext_font_end - ext_font_start

.global ext_icons_start
ext_icons_start:
.incbin "../../../res/icons.png"
.global ext_icons_end
ext_icons_end:
.global ext_icons_size
.align 4
ext_icons_size:
.int ext_icons_end - ext_icons_start

