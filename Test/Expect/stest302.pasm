pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	outa, imm_268435455_
_main_ret
	ret

imm_268435455_
	long	268435455
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
