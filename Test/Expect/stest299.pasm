pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	outa, #261
_main_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
