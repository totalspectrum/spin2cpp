pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_siz
	mov	result1, #8
_siz_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
