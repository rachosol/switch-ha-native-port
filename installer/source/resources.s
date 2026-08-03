.section .rodata.switch_ha_native_nsp, "a", %progbits
.balign 16
.global switch_ha_native_nsp_start
.global switch_ha_native_nsp_end
switch_ha_native_nsp_start:
.incbin "../romfs/switch-ha-native.nsp"
switch_ha_native_nsp_end:
.balign 16

.section .rodata.switch_ha_titles_txt, "a", %progbits
.balign 16
.global switch_ha_titles_txt_start
.global switch_ha_titles_txt_end
switch_ha_titles_txt_start:
.incbin "../romfs/titles.txt"
switch_ha_titles_txt_end:
.balign 16
