pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_ival
	mov	result1, #48
_ival_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
