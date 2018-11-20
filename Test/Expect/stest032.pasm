con
	incval = 15
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_inc
	add	arg1, #15
	mov	result1, arg1
_inc_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
	fit	496
