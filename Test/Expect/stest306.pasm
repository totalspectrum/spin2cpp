pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zzz
	mov	outa, #0
	mov	outb, #0
	mov	ina, #0
_zzz_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
